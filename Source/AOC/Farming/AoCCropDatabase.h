// AoCCropDatabase.h
// Architect of Creation - Crop Growth Database Subsystem
// Centralized crop growth parameters, skill requirements, and rotation data.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AoCCropDatabase.generated.h"

// ─── Structs ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FCropGrowthData
{
	GENERATED_BODY()

	/** Unique crop identifier matching ItemDatabase CropID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop")
	FName CropID;

	/** Human-readable name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop")
	FString DisplayName;

	/** Base seconds for a full growth cycle (seed → harvestable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Growth")
	float GrowthTimeBase = 600.0f;

	/** Water demand factor (0-1). Higher = more watering needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Growth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaterNeed = 0.5f;

	/** Yield when harvested from a wild / undomesticated source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Yield")
	int32 BaseYieldWild = 4;

	/** Yield when harvested from a fully cultivated farm tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Yield")
	int32 BaseYieldDomestic = 8;

	/** Minimum Farming skill level required to sow this crop (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Requirements", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MinFarmingSkill = 30;

	/** ItemDatabase ID for the seed item needed to plant this crop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Items")
	FName SeedItemID;

	/** ItemDatabase ID for the harvested produce item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Items")
	FName HarvestedItemID;

	/** Number of visual growth stages (always 4: Sprout, Growing, Mature, Harvestable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Growth")
	int32 GrowthStageCount = 4;

	/** True if this crop requires a waterlogged paddy tile (Rice only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Requirements")
	bool RequiresPaddy = false;

	/** Rotation group for this crop: "Vegetable", "Grain", or "Industrial". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Rotation")
	FName CropRotationGroup;

	/** Which rotation group should ideally precede this crop for a +5% quality bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crop|Rotation")
	FName PreviousRotationBonus;

	FCropGrowthData()
		: CropID(NAME_None)
		, DisplayName(TEXT(""))
		, GrowthTimeBase(600.0f)
		, WaterNeed(0.5f)
		, BaseYieldWild(4)
		, BaseYieldDomestic(8)
		, MinFarmingSkill(30)
		, SeedItemID(NAME_None)
		, HarvestedItemID(NAME_None)
		, GrowthStageCount(4)
		, RequiresPaddy(false)
		, CropRotationGroup(NAME_None)
		, PreviousRotationBonus(NAME_None)
	{}
};

// ─── Subsystem ───────────────────────────────────────────────────────────────

UCLASS()
class AOC_API UAoCCropDatabase : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Called automatically when the GameInstance creates subsystems. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Called when the GameInstance is torn down. */
	virtual void Deinitialize() override;

	// ── Queries ─────────────────────────────────────────────────────────────

	/** Get crop data by CropID. Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category = "CropDatabase")
	const FCropGrowthData* GetCrop(FName CropID) const;

	/** Return all crops that can be unlocked at a given Farming skill level. */
	UFUNCTION(BlueprintCallable, Category = "CropDatabase")
	TArray<FCropGrowthData> GetCropsForSkillLevel(int32 Skill) const;

	/** Return all crops belonging to a rotation group (e.g. "Vegetable", "Grain", "Industrial"). */
	UFUNCTION(BlueprintCallable, Category = "CropDatabase")
	TArray<FCropGrowthData> GetCropsByCategory(FName Category) const;

	/**
	 * Check whether planting CurrentCrop after PreviousCrop gives a rotation bonus.
	 * @return  1.05 if ideal rotation, 0.90 if bad rotation (same group back-to-back), 1.0 if neutral.
	 */
	UFUNCTION(BlueprintCallable, Category = "CropDatabase")
	float CheckRotationBonus(FName CurrentCrop, FName PreviousCrop) const;

	/** Total number of registered crops. */
	UFUNCTION(BlueprintPure, Category = "CropDatabase")
	int32 GetCropCount() const;

protected:

	/** Master registry: CropID → growth data. */
	UPROPERTY()
	TMap<FName, FCropGrowthData> Crops;

private:

	/** Register a single crop entry. */
	void RegisterCrop(const FCropGrowthData& Data);

	/** Helper to build and register a crop in one call. */
	void AddCrop(
		const FString& InName,
		float GrowthTime,
		float Water,
		int32 YieldWild,
		int32 YieldDomestic,
		int32 SkillReq,
		bool bPaddy,
		const FString& RotGroup,
		const FString& PrevBonus
	);

	/** Populate all 25 crops. */
	void PopulateAllCrops();
};
