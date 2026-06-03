// AoCFurnaceActor.h
// Architect of Creation - Furnace Actor
// 5-tier furnace system for smelting ores into lumps, bars, and ingots.
// Implements real metallurgy temperatures, bellows interaction, fuel management,
// condition degradation, slag buildup, and visual heat emissive effects.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCFurnaceActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EFurnaceTier : uint8
{
	CampfireCrucible	UMETA(DisplayName = "Campfire Crucible"),
	Bloomery			UMETA(DisplayName = "Bloomery"),
	BlastFurnace		UMETA(DisplayName = "Blast Furnace"),
	CrucibleFurnace		UMETA(DisplayName = "Crucible Furnace"),
	ArcaneForge			UMETA(DisplayName = "Arcane Forge")
};

UENUM(BlueprintType)
enum class EFurnaceState : uint8
{
	Idle				UMETA(DisplayName = "Idle"),
	Heating				UMETA(DisplayName = "Heating"),
	Ready				UMETA(DisplayName = "Ready"),
	Smelting			UMETA(DisplayName = "Smelting"),
	Cooling				UMETA(DisplayName = "Cooling"),
	NeedsFuel			UMETA(DisplayName = "Needs Fuel")
};

UENUM(BlueprintType)
enum class ESmeltOutput : uint8
{
	Lump				UMETA(DisplayName = "Lump"),
	Bar					UMETA(DisplayName = "Bar"),
	Ingot				UMETA(DisplayName = "Ingot")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

/** Data for a finished smelting output, returned by ExtractOutput. */
USTRUCT(BlueprintType)
struct AOC_API FSmeltingResult
{
	GENERATED_BODY()

	/** The material type of the smelted output. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|Output")
	EVoxelMaterial Material;

	/** Output form: Lump, Bar, or Ingot. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|Output")
	ESmeltOutput Form = ESmeltOutput::Lump;

	/** Quality of the smelted output (0-100). */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|Output")
	uint8 Quality = 0;

	/** Temperature of the output when extracted (cools over 60s). */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|Output")
	float Temperature = 20.f;

	/** Whether this struct contains valid output data. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|Output")
	bool bIsValid = false;

	FSmeltingResult()
		: Material(static_cast<EVoxelMaterial>(0))
		, Form(ESmeltOutput::Lump)
		, Quality(0)
		, Temperature(20.f)
		, bIsValid(false)
	{}
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFurnaceIgnited, AAoCFurnaceActor*, Furnace);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFurnaceTempChanged, AAoCFurnaceActor*, Furnace, float, NewTemperature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFurnaceSmeltingComplete, AAoCFurnaceActor*, Furnace, FSmeltingResult, Output);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFurnaceFuelDepleted, AAoCFurnaceActor*, Furnace);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFurnaceConditionLow, AAoCFurnaceActor*, Furnace, float, CurrentCondition);

// ─── Actor ───────────────────────────────────────────────────────────────────

/**
 * 5-tier furnace actor for smelting ores into metal products.
 *
 * Furnace Tiers:
 *   Campfire Crucible:  800°C max, 20 ore capacity,  speed 1.0x  — Lead/Zinc only, lumps only
 *   Bloomery:          1500°C max, 400 ore capacity, speed 1.0x  — requires bellows for high temps
 *   Blast Furnace:     2000°C max, 800 ore capacity, speed 3.0x  — built-in forced air
 *   Crucible Furnace:  2500°C max, 400 ore capacity, speed 2.0x  — precise high-temp control
 *   Arcane Forge:      3500°C max, 200 ore capacity, speed 5.0x  — magic-sustained extreme temps
 *
 * Smelting temperatures (real metallurgy reduction temps):
 *   Lead 800°C, Zinc 950°C, Copper 1100°C, Tin 1100°C, Silver 1000°C, Gold 1100°C,
 *   Iron 1200°C, Manganese 1400°C, Cobalt 1400°C, Nickel 1450°C, Vanadium 1750°C,
 *   Chromium 1850°C, Palladium 1600°C, Titanium 1800°C, Platinum 1800°C, Rhodium 2000°C,
 *   Iridium 2400°C, Niobium 2400°C, Molybdenum 2500°C, Tantalum 2900°C,
 *   Osmium 3000°C, Tungsten 3300°C
 */
UCLASS(BlueprintType, Blueprintable)
class AOC_API AAoCFurnaceActor : public AActor
{
	GENERATED_BODY()

public:
	AAoCFurnaceActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Components ──────────────────────────────────────────────────────────

	/** Main furnace body mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furnace|Components")
	UStaticMeshComponent* FurnaceMesh;

	/** Visible ore pile inside the furnace (scales with OreLoaded). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furnace|Components")
	UStaticMeshComponent* OrePileMesh;

	/** Emissive glow mesh inside the furnace (intensity driven by temperature). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furnace|Components")
	UStaticMeshComponent* GlowMesh;

	// ── Configuration ───────────────────────────────────────────────────────

	/** Which tier of furnace this is. Determines max temp, capacity, and speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furnace|Config")
	EFurnaceTier FurnaceTier;

	// ── State ───────────────────────────────────────────────────────────────

	/** Current operational state of the furnace. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	EFurnaceState FurnaceState;

	/** Current internal temperature in degrees Celsius. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	float CurrentTemp;

	/** Maximum achievable temperature for this furnace tier. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	float MaxTemp;

	/** Maximum temperature achievable by fuel alone (without bellows). */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	float SustainedMaxTemp;

	/** Current fuel level (0-100). Consumed while furnace is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	float FuelLevel;

	/** Rate at which fuel is consumed (units per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furnace|Config")
	float FuelDecayRate;

	/** Furnace structural condition (0-100). Degrades with use, affects output quality. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	float Condition;

	/** Maximum condition value (default 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furnace|Config")
	float MaxCondition;

	/** Maximum ore capacity based on furnace tier. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	int32 Capacity;

	/** Amount of ore currently loaded in the furnace. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	int32 OreLoaded;

	/** Material type of the loaded ore. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	EVoxelMaterial OreMaterial;

	/** Quality of the loaded ore (0-100). */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	uint8 OreQuality;

	/** Whether bellows are currently being pumped (Bloomery only). */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	bool bBellowsActive;

	/** Slag buildup level (0-100). Reduces efficiency if not cleaned. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|State")
	float SlagLevel;

	// ── Smelting Progress ───────────────────────────────────────────────────

	/** Current smelting progress (0.0 to 1.0). */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|Smelting")
	float SmeltingProgress;

	/** Total duration for the current smelting operation in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|Smelting")
	float SmeltingDuration;

	/** Output type being smelted. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|Smelting")
	ESmeltOutput CurrentOutputType;

	/** Speed multiplier for this furnace tier (1.0x to 5.0x). */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|Smelting")
	float SpeedMultiplier;

	/** Whether finished output is ready for extraction. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|Smelting")
	bool bHasOutput;

	/** The completed smelting result waiting for extraction. */
	UPROPERTY(BlueprintReadOnly, Category = "Furnace|Smelting")
	FSmeltingResult PendingOutput;

	// ── Temperature Config ──────────────────────────────────────────────────

	/** Rate at which temperature decays without fuel (°C per minute). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furnace|Config")
	float TempDecayRate;

	/** Rate at which furnace heats up with fuel (°C per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furnace|Config")
	float HeatingRate;

	// ── Delegates ───────────────────────────────────────────────────────────

	/** Fired when the furnace is ignited. */
	UPROPERTY(BlueprintAssignable, Category = "Furnace|Events")
	FOnFurnaceIgnited OnIgnited;

	/** Fired when temperature changes by more than 10°C. */
	UPROPERTY(BlueprintAssignable, Category = "Furnace|Events")
	FOnFurnaceTempChanged OnTempChanged;

	/** Fired when smelting completes. */
	UPROPERTY(BlueprintAssignable, Category = "Furnace|Events")
	FOnFurnaceSmeltingComplete OnSmeltingComplete;

	/** Fired when fuel is depleted during operation. */
	UPROPERTY(BlueprintAssignable, Category = "Furnace|Events")
	FOnFurnaceFuelDepleted OnFuelDepleted;

	/** Fired when condition drops below 20%. */
	UPROPERTY(BlueprintAssignable, Category = "Furnace|Events")
	FOnFurnaceConditionLow OnConditionLow;

	// ── Public Functions ────────────────────────────────────────────────────

	/**
	 * Ignite the furnace — begins heating. Requires fuel > 0.
	 * @return True if ignited successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	bool Ignite();

	/**
	 * Add fuel (charcoal/billets) to the furnace.
	 * @param FuelAmount   Units of fuel to add (clamped to 0-100 total).
	 * @param FuelQuality  Quality of fuel (higher Q = +50°C max temp bonus).
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	void AddFuel(int32 FuelAmount, float FuelQuality);

	/**
	 * Pump bellows — Bloomery only. Raises temperature by 100°C (capped at MaxTemp).
	 * @return True if bellows pumped successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	bool PumpBellows();

	/**
	 * Load raw ore into the furnace.
	 * @param Material  Type of ore to load.
	 * @param Amount    Number of ore pieces to load.
	 * @param Quality   Quality of the ore (0-100).
	 * @return True if ore was loaded successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	bool LoadOre(EVoxelMaterial Material, int32 Amount, uint8 Quality);

	/**
	 * Check if the furnace can smelt the given material at current temperature.
	 * @param Material  Ore material to check.
	 * @return True if CurrentTemp >= ReductionTemp for the material.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	bool CanSmelt(EVoxelMaterial Material) const;

	/**
	 * Begin smelting the loaded ore into the specified output form.
	 * Requires sufficient ore, correct temperature, and no pending output.
	 * @param OutputType  Lump (1 ore), Bar (4 ore), or Ingot (20 ore).
	 * @return True if smelting started successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	bool StartSmelting(ESmeltOutput OutputType);

	/**
	 * Extract the finished smelting output. Metal will be hot and cools over 60s.
	 * @return FSmeltingResult struct with material, form, quality, and temperature.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	FSmeltingResult ExtractOutput();

	/**
	 * Repair furnace condition using materials.
	 * Repair amount = 20 * (MaterialQuality / 100).
	 * @param MaterialQuality  Quality of repair materials (0-100).
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	void Repair(float MaterialQuality);

	/**
	 * Remove slag buildup from the furnace. Slag reduces smelting efficiency.
	 * Resets SlagLevel to 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Actions")
	void CleanSlag();

	// ── Static Helpers ──────────────────────────────────────────────────────

	/**
	 * Get the ore reduction temperature for a given material.
	 * Real metallurgy reduction temperatures — NOT melting points.
	 * e.g., Iron reduces at 1200°C (bloom), not 1538°C (melting point).
	 */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetReductionTemperature(EVoxelMaterial Material);

	/** Get maximum temperature for a furnace tier. */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetMaxTempForTier(EFurnaceTier Tier);

	/** Get the fuel-sustained max temperature (without bellows). */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetSustainedMaxTempForTier(EFurnaceTier Tier);

	/** Get ore capacity for a furnace tier. */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static int32 GetCapacityForTier(EFurnaceTier Tier);

	/** Get smelting speed multiplier for a furnace tier. */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetSpeedMultiplierForTier(EFurnaceTier Tier);

	/** Get temperature decay rate (°C per minute) for a furnace tier. */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetTempDecayRateForTier(EFurnaceTier Tier);

	/** Get heating rate (°C per second) for a furnace tier. */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetHeatingRateForTier(EFurnaceTier Tier);

	/** Get the required ore count for a given output form. */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static int32 GetOreRequiredForOutput(ESmeltOutput OutputType);

	/** Get base smelting time in seconds for a given output form (before speed multiplier). */
	UFUNCTION(BlueprintCallable, Category = "Furnace|Data")
	static float GetBaseSmeltTime(ESmeltOutput OutputType);

protected:
	// ── Internal Tick Systems ───────────────────────────────────────────────

	/** Manage temperature heating and decay. */
	void UpdateTemperature(float DeltaTime);

	/** Consume fuel over time while furnace is active. */
	void UpdateFuelConsumption(float DeltaTime);

	/** Advance smelting progress and check for completion. */
	void UpdateSmeltingProgress(float DeltaTime);

	/** Apply overheating quality penalty when temp is 50°C+ above reduction temp. */
	void UpdateOverheatPenalty(float DeltaTime);

	/** Update emissive material based on current temperature. */
	void UpdateVisuals();

	/** Initialize furnace properties based on tier. */
	void InitializeFromTier();

	/** Transition the furnace to a new state. */
	void SetFurnaceState(EFurnaceState NewState);

private:
	/** Dynamic material instance for temperature-driven emissive glow. */
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMaterialInstance;

	/** Dynamic material instance for ore pile color changes. */
	UPROPERTY()
	UMaterialInstanceDynamic* OrePileMaterialInstance;

	/** Accumulated overheating quality loss (fractional, floored when applied). */
	float OverheatQualityLoss;

	/** Snapshot of furnace condition at smelting start (used for quality calc). */
	float SmeltConditionSnapshot;

	/** Previous temperature value for delegate change detection. */
	float PreviousTempForDelegate;

	/** Timer tracking bellows boost decay (Bloomery only). */
	float BellowsBoostTimer;

	/** Fuel quality bonus to max temperature. */
	float FuelQualityTempBonus;

	/** Ambient temperature constant. */
	static constexpr float AmbientTemp = 20.f;

	/** Temperature change threshold for firing OnTempChanged delegate. */
	static constexpr float TempChangeDelegateThreshold = 10.f;

	/** Seconds for extracted hot metal to cool to ambient. */
	static constexpr float OutputCooldownDuration = 60.f;

	/** Condition threshold below which OnConditionLow fires. */
	static constexpr float ConditionLowThreshold = 20.f;

	/** Overheating threshold: penalty applied when temp exceeds reduction temp by this much. */
	static constexpr float OverheatThreshold = 50.f;

	/** Quality loss per minute when overheating. */
	static constexpr float OverheatQualityLossPerMinute = 5.f;

	/** Condition degradation per Lump smelted. */
	static constexpr float ConditionLossPerLump = 1.f;

	/** Condition degradation per Bar smelted. */
	static constexpr float ConditionLossPerBar = 2.f;

	/** Condition degradation per Ingot smelted. */
	static constexpr float ConditionLossPerIngot = 5.f;

	/** Slag buildup per Lump smelted. */
	static constexpr float SlagPerLump = 3.f;

	/** Slag buildup per Bar smelted. */
	static constexpr float SlagPerBar = 8.f;

	/** Slag buildup per Ingot smelted. */
	static constexpr float SlagPerIngot = 15.f;

	/** Bellows boost duration in seconds. */
	static constexpr float BellowsBoostDuration = 30.f;

	/** Temperature added per bellows pump (°C). */
	static constexpr float BellowsTempBoost = 100.f;
};
