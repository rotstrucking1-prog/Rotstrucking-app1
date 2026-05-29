// AoCFarmingTile.h
// Architect of Creation - Farmable Ground Tile
// Represents a single farmable ground tile with crop growth, watering, and soil quality systems.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCFarmingTile.generated.h"

class UStaticMeshComponent;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EFarmTileState : uint8
{
	Untouched		UMETA(DisplayName = "Untouched"),
	Plowed			UMETA(DisplayName = "Plowed"),
	Fertilized		UMETA(DisplayName = "Fertilized"),
	Planted			UMETA(DisplayName = "Planted"),
	Watered			UMETA(DisplayName = "Watered"),
	Growing			UMETA(DisplayName = "Growing"),
	Ready			UMETA(DisplayName = "Ready"),
	Harvested		UMETA(DisplayName = "Harvested")
};

UENUM(BlueprintType)
enum class EGrowthStage : uint8
{
	Seed			UMETA(DisplayName = "Seed"),
	Sprout			UMETA(DisplayName = "Sprout"),
	Young			UMETA(DisplayName = "Young"),
	Mature			UMETA(DisplayName = "Mature"),
	Ready			UMETA(DisplayName = "Ready"),
	Withered		UMETA(DisplayName = "Withered")
};

UENUM(BlueprintType)
enum class ESoilQuality : uint8
{
	Poor			UMETA(DisplayName = "Poor"),
	Fair			UMETA(DisplayName = "Fair"),
	Good			UMETA(DisplayName = "Good"),
	Great			UMETA(DisplayName = "Great"),
	Excellent		UMETA(DisplayName = "Excellent")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FCropData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CropID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	/** Seconds for full growth cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GrowthTimeBase = 600.f;

	/** 0-1, how much water the crop demands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WaterNeed = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseYield = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGrowthStage CurrentStage = EGrowthStage::Seed;

	FCropData()
		: CropID(NAME_None)
		, DisplayName(TEXT(""))
		, GrowthTimeBase(600.f)
		, WaterNeed(0.5f)
		, BaseYield(1)
		, CurrentStage(EGrowthStage::Seed)
	{}
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCropReady, AAoCFarmingTile*, Tile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTileStateChanged, AAoCFarmingTile*, Tile, EFarmTileState, NewState);

// ─── Actor ───────────────────────────────────────────────────────────────────

UCLASS(BlueprintType, Blueprintable)
class AOC_API AAoCFarmingTile : public AActor
{
	GENERATED_BODY()

public:
	AAoCFarmingTile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Properties ──────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming")
	EFarmTileState TileState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming")
	ESoilQuality SoilQuality;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Farming")
	float TileSize;

	UPROPERTY(BlueprintReadOnly, Category = "Farming")
	FCropData PlantedCrop;

	/** 0-1 progress toward full growth. */
	UPROPERTY(BlueprintReadOnly, Category = "Farming")
	float GrowthProgress;

	/** 0-1 water level — drains over time. */
	UPROPERTY(BlueprintReadOnly, Category = "Farming")
	float WaterLevel;

	/** 0-1 fertilizer multiplier bonus. */
	UPROPERTY(BlueprintReadOnly, Category = "Farming")
	float FertilizerBonus;

	UPROPERTY(BlueprintReadOnly, Category = "Farming")
	bool bIsWatered;

	// ── Components ──────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CropMesh;

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Farming|Events")
	FOnCropReady OnCropReady;

	UPROPERTY(BlueprintAssignable, Category = "Farming|Events")
	FOnTileStateChanged OnTileStateChanged;

	// ── Functions ───────────────────────────────────────────────────────────

	/** Plow untouched soil — Untouched → Plowed. */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	bool PlowTile();

	/** Add fertilizer bonus (0-1). */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	bool FertilizeTile(float Amount);

	/** Plant a crop by ID — requires Plowed or Fertilized state. */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	bool PlantCrop(FName CropID);

	/** Water the tile — sets water level to 1.0. */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	bool WaterTile();

	/** Harvest a ready crop — returns CropID (NAME_None on failure). */
	UFUNCTION(BlueprintCallable, Category = "Farming")
	FName HarvestCrop();

protected:
	/** Advance growth based on water, soil, and fertilizer. */
	void UpdateGrowth(float DeltaTime);

	/** Move to next growth stage when thresholds are met. */
	void AdvanceGrowthStage();

	/** If crop is Ready for too long without harvest → Withered. */
	void CheckWithering();

	/** Update tile material / color per current state. */
	void UpdateTileVisuals();

	/** Scale crop mesh to match current growth stage. */
	void UpdateCropVisuals();

	/** Returns effective soil quality with fertilizer factored in. */
	ESoilQuality GetEffectiveSoilQuality() const;

	/** Helper — soil quality → float multiplier. */
	float GetSoilQualityMultiplier() const;

	/** Initialize the local crop database (called in BeginPlay). */
	void InitCropDatabase();

private:
	FTimerHandle GrowthTimerHandle;

	/** Timer used to track how long a crop sits at Ready before withering. */
	FTimerHandle WitheringTimerHandle;

	/** Local crop template database. */
	TMap<FName, FCropData> CropDatabase;

	/** Dynamic material instance for tile color changes. */
	UPROPERTY()
	UMaterialInstanceDynamic* TileDynamicMaterial;

	/** Seconds the crop has been in Ready state without harvest. */
	float ReadyElapsedTime;

	/** Max seconds a crop can remain Ready before withering. */
	static constexpr float WitherTimeThreshold = 300.f;
};
