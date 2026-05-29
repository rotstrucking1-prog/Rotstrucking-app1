// AoCOreVein.h
// Architect of Creation - Ore Vein Actor
// Represents an ore vein in the world with metadata for extraction, quality, and generation.
// Not a visible mesh — metadata actor that tracks vein state and provides ore extraction logic.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCOreVein.generated.h"

class AAoCVoxelWorld;

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

	/**
	 * Initialize this vein with the given parameters.
	 * Sets up all vein properties and calculates tier.
	 * @param Mat Material type of the ore
	 * @param Center World-space center of the vein
	 * @param Radius Vein radius in centimeters
	 * @param InQuality Ore quality (0-100)
	 * @param OreAmount Total ore units in the vein
	 */
	UFUNCTION(BlueprintCallable, Category = "OreVein")
	void InitializeVein(EVoxelMaterial Mat, FVector Center, float Radius, uint8 InQuality, int32 OreAmount);

	/**
	 * Extract ore from this vein.
	 * Output quality = (VeinQuality + SkillBonus + ToolQuality) / 3.
	 * Decrements RemainingOre.
	 * @param SkillLevel Mining skill of the extractor (0-100)
	 * @param ToolQuality Quality of the mining tool (0-100)
	 * @return Extraction result with amount and quality
	 */
	UFUNCTION(BlueprintCallable, Category = "OreVein")
	FOreExtractResult ExtractOre(float SkillLevel, float ToolQuality);

	/**
	 * Check if this vein has been fully depleted.
	 * @return True if no ore remains
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein")
	bool IsDepleted() const;

	/**
	 * Get the percentage of ore remaining in this vein.
	 * @return Percentage (0.0 to 1.0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein")
	float GetRemainingPercentage() const;

	/**
	 * Get the display color for this ore type (based on real-world ore appearance).
	 * @return Linear color representing the ore's visual appearance
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein")
	FLinearColor GetOreColor() const;

	// ── Static Helpers ──────────────────────────────────────────────────────

	/**
	 * Get the display name for a given ore material.
	 * @param Mat The voxel material enum value
	 * @return Human-readable ore name (e.g. "Copper Ore (Malachite)")
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FString GetOreName(EVoxelMaterial Mat);

	/**
	 * Get the tier (1-6) for a given ore material.
	 * @param Mat The voxel material enum value
	 * @return Tier number (1-6), or 0 if not an ore
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static int32 GetOreTier(EVoxelMaterial Mat);

	/**
	 * Get the ore reduction (smelting) temperature in degrees Celsius.
	 * @param Mat The voxel material enum value
	 * @return Temperature in °C required to smelt/reduce this ore
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static int32 GetOreReductionTemp(EVoxelMaterial Mat);

	/**
	 * Get the ore color for a given material type.
	 * @param Mat The voxel material enum value
	 * @return Linear color representing the ore
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FLinearColor GetOreColorForMaterial(EVoxelMaterial Mat);

	/**
	 * Get generation parameters for a given tier.
	 * @param InTier Tier number (1-6)
	 * @return Generation parameters struct
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static FOreGenParams GetGenParamsForTier(int32 InTier);

	/**
	 * Get all ore materials that belong to a given tier.
	 * @param InTier Tier number (1-6)
	 * @return Array of EVoxelMaterial values in that tier
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OreVein|Static")
	static TArray<EVoxelMaterial> GetMaterialsForTier(int32 InTier);
};
