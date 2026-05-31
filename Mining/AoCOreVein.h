// AoCOreVein.h
// Architect of Creation - Ore Vein Actor
// Represents an ore vein in the world with visual rock mesh and metadata for extraction.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCOreVein.generated.h"

class AAoCVoxelWorld;
class UStaticMeshComponent;

// ─── Structs ─────────────────────────────────────────────────────────────────

/** Ore tier generation parameters for procedural vein placement. */
USTRUCT(BlueprintType)
struct AOC_API FOreGenParams
{
	GENERATED_BODY()

	/** Minimum depth in meters below surface for this tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation")
	float MinDepthM = 0.f;

	/** Maximum depth in meters below surface for this tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation")
	float MaxDepthM = 0.f;

	/** Minimum ore units in a vein of this tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation")
	int32 MinOre = 0;

	/** Maximum ore units in a vein of this tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation")
	int32 MaxOre = 0;

	/** Approximate frequency: 1 vein per NxN tile area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation")
	int32 SpawnAreaTiles = 50;

	/** Minimum random quality for veins of this tier (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 MinQuality = 20;

	/** Maximum random quality for veins of this tier (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Generation", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 MaxQuality = 60;
};

/** Result of an ore extraction attempt. */
USTRUCT(BlueprintType)
struct AOC_API FOreExtractResult
{
	GENERATED_BODY()

	/** Number of ore units extracted. */
	UPROPERTY(BlueprintReadOnly, Category = "OreVein")
	int32 AmountExtracted = 0;

	/** Quality of the extracted ore (0-100). */
	UPROPERTY(BlueprintReadOnly, Category = "OreVein")
	uint8 OutputQuality = 0;

	/** Material type of the extracted ore. */
	UPROPERTY(BlueprintReadOnly, Category = "OreVein")
	EVoxelMaterial Material = EVoxelMaterial::Air;

	/** True if the extraction was successful. */
	UPROPERTY(BlueprintReadOnly, Category = "OreVein")
	bool bSuccess = false;

	/** True if the vein is now depleted after this extraction. */
	UPROPERTY(BlueprintReadOnly, Category = "OreVein")
	bool bVeinDepleted = false;
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVeinDepletedDelegate, AAoCOreVein*, DepletedVein);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOreMinedDelegate, AAoCOreVein*, Vein, FOreExtractResult, Result);

// ─── Actor ───────────────────────────────────────────────────────────────────

UCLASS()
class AOC_API AAoCOreVein : public AActor
{
	GENERATED_BODY()

public:
	AAoCOreVein();

	virtual void BeginPlay() override;

	// ── Components ──────────────────────────────────────────────────────────

	/** Visual rock mesh representing the ore vein in the world. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OreVein|Components")
	UStaticMeshComponent* VeinMesh;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Center position of the vein in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config")
	FVector VeinCenter;

	/** Radius of the ore vein in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config", meta = (ClampMin = "0.0"))
	float VeinRadius;

	/** Material type of the ore in this vein. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config")
	EVoxelMaterial OreMaterial;

	/** Quality of the ore in this vein (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 Quality;

	/** Total ore units when the vein was created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config", meta = (ClampMin = "0"))
	int32 TotalOre;

	/** Remaining ore units available for extraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|State", meta = (ClampMin = "0"))
	int32 RemainingOre;

	/** Tier of this ore vein (1-6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config", meta = (ClampMin = "1", ClampMax = "6"))
	int32 Tier;

	/** Reference to the voxel world for updating voxel data on extraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OreVein|Config")
	AAoCVoxelWorld* VoxelWorld;

	// ── Delegates ───────────────────────────────────────────────────────────

	/** Fired when this vein has been fully depleted. */
	UPROPERTY(BlueprintAssignable, Category = "OreVein|Events")
	FOnVeinDepletedDelegate OnVeinDepleted;

	/** Fired each time ore is extracted from this vein. */
	UPROPERTY(BlueprintAssignable, Category = "OreVein|Events")
	FOnOreMinedDelegate OnOreMined;

	// ── Functions ───────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "OreVein")
	void InitializeVein(EVoxelMaterial Mat, FVector Center, float Radius, uint8 InQuality, int32 OreAmount);

	UFUNCTION(BlueprintCallable, Category = "OreVein")
	FOreExtractResult ExtractOre(float SkillLevel, float ToolQuality);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein")
	bool IsDepleted() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein")
	float GetRemainingPercentage() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein")
	FLinearColor GetOreColor() const;

	/** Update the visual scale of the rock mesh based on remaining ore %. */
	UFUNCTION(BlueprintCallable, Category = "OreVein")
	void UpdateVisualScale();

	// ── Static Helpers ──────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FString GetOreName(EVoxelMaterial Mat);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static int32 GetOreTier(EVoxelMaterial Mat);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static int32 GetOreReductionTemp(EVoxelMaterial Mat);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FLinearColor GetOreColorForMaterial(EVoxelMaterial Mat);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FOreGenParams GetGenParamsForTier(int32 InTier);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static TArray<EVoxelMaterial> GetMaterialsForTier(int32 InTier);

	/** Get the inventory item ID FName for a given ore material. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FName GetItemIDForMaterial(EVoxelMaterial Mat);

	/** Get a short display name for inventory (e.g., "Copper Ore"). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FString GetShortOreName(EVoxelMaterial Mat);

private:
	/** Original scale saved at BeginPlay, used for shrinking calculation. */
	FVector OriginalScale;
};
