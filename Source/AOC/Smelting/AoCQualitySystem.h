// AoCQualitySystem.h
// Architect of Creation - Quality System
// GameInstanceSubsystem providing quality calculations for all crafting, gathering,
// smelting, and farming activities. Quality (0-100) is the core economic driver.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AoCQualitySystem.generated.h"

// ─── Structs ─────────────────────────────────────────────────────────────────

/**
 * A single quality input for crafting calculations.
 * Each component (material, tool, workshop, etc.) has a quality value
 * and an influence percentage that determines its weight in the formula.
 */
USTRUCT(BlueprintType)
struct AOC_API FQualityInput
{
	GENERATED_BODY()

	/** Quality value of this component (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	float Quality = 0.f;

	/** Influence percentage of this component in the recipe (0.0-1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality")
	float InfluencePercent = 0.f;

	FQualityInput()
		: Quality(0.f)
		, InfluencePercent(0.f)
	{}

	FQualityInput(float InQuality, float InInfluence)
		: Quality(InQuality)
		, InfluencePercent(InInfluence)
	{}
};

// ─── Quality System Subsystem ────────────────────────────────────────────────

/**
 * Central quality calculation system for Architect of Creation.
 * 
 * Quality range: 0-100 on everything.
 * Quality is THE core economic driver — higher Q items are more durable,
 * deal more damage, provide better mitigation, and are more valuable.
 *
 * Formulas:
 *   Gathered:  Q = min(skill, source_Q)
 *   Mining:    Q = source vein quality (tool Q doesn't matter)
 *   Farming:   Q = min(skill, soil_Q, seed_Q, water_Q)
 *   Crafting:  predicted_Q = sum(component_Q * influence%)
 *              nl_quality = 3 + (predicted_Q ^ 1.35) / 4.5
 *              final_Q = floor(min(skill, min(nl_quality * shop_bonus, 100) * material_modifier))
 *   Smelting:  Q = min(smelting_skill, ore_Q, furnace_condition%)
 *
 * Material Modifiers:
 *   T1 (Copper, Tin):                                 0.6  (max Q 60)
 *   T2 (Iron, Zinc, Lead):                            0.7  (max Q 70)
 *   T3 (Nickel, Silver, Gold):                        0.8  (max Q 80)
 *   T4 (Chromium, Cobalt, Manganese, Molybdenum):     0.9  (max Q 90)
 *   T5 (Titanium, Tungsten, Vanadium, Platinum, Palladium): 1.0
 *   T6 (Iridium, Osmium, Rhodium, Niobium, Tantalum):      1.0
 */
UCLASS()
class AOC_API UAoCQualitySystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── Lifecycle ───────────────────────────────────────────────────────────

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ── Quality Calculation Functions ────────────────────────────────────────

	/**
	 * Calculate gathered item quality (herbs, wood).
	 * Q = min(Skill, SourceQ)
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Calculation")
	static uint8 CalculateGatheredQuality(float Skill, float SourceQ);

	/**
	 * Calculate mined ore quality.
	 * Q = source vein quality (tool quality does NOT matter).
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Calculation")
	static uint8 CalculateMiningQuality(float VeinQ);

	/**
	 * Calculate farmed crop quality.
	 * Q = min(Skill, SoilQ, SeedQ, WaterQ)
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Calculation")
	static uint8 CalculateFarmingQuality(float Skill, float SoilQ, float SeedQ, float WaterQ);

	/**
	 * Calculate crafted item quality using the non-linear forging formula.
	 * predicted_Q = sum(component_Q * influence%)
	 * nl_quality = 3 + (predicted_Q ^ 1.35) / 4.5
	 * final_Q = floor(min(Skill, min(nl_quality * ShopBonus, 100) * MaterialModifier))
	 *
	 * @param Skill          Player's crafting skill (0-100).
	 * @param Components     Array of quality inputs with influence percentages.
	 * @param ShopBonus      Workshop bonus multiplier (1.0 = no bonus, 1.2 = blacksmith shop).
	 * @param MaterialModifier Tier-based modifier (0.6 for T1, up to 1.0 for T5/T6).
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Calculation")
	static uint8 CalculateCraftingQuality(float Skill, const TArray<FQualityInput>& Components, float ShopBonus, float MaterialModifier);

	/**
	 * Calculate smelted output quality.
	 * Q = min(SmeltingSkill, OreQ, FurnaceCondition%)
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Calculation")
	static uint8 CalculateSmeltingQuality(float Skill, float OreQ, float FurnaceCondition);

	// ── Quality Effect Functions ─────────────────────────────────────────────

	/**
	 * Get tool durability based on quality.
	 * durability = BaseHP * (1.0 + Q / 100.0)
	 * At Q100: 2x durability of Q0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Effects")
	static int32 GetToolDurability(int32 BaseHP, uint8 Quality);

	/**
	 * Get weapon damage multiplier based on quality.
	 * multiplier = 0.5 + Q / 100.0
	 * At Q100: 1.5x damage. At Q0: 0.5x damage.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Effects")
	static float GetWeaponDamageMultiplier(uint8 Quality);

	/**
	 * Get armor mitigation based on quality.
	 * mitigation = BaseMitigation * (0.5 + Q / 100.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Effects")
	static float GetArmorMitigation(float BaseMitigation, uint8 Quality);

	/**
	 * Get building durability HP based on quality.
	 * durability = floor(50 + 1.5 * Q) * 100
	 * At Q0: 5000 HP. At Q100: 20000 HP.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Effects")
	static int32 GetBuildingDurability(uint8 Quality);

	/**
	 * Get the material quality modifier for a given tier.
	 * T1=0.6, T2=0.7, T3=0.8, T4=0.9, T5/T6=1.0
	 *
	 * @param Tier Material tier (1-6).
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Effects")
	static float GetMaterialModifier(uint8 Tier);

	// ── Quality Utility Functions ────────────────────────────────────────────

	/**
	 * Merge two stacks of items with different qualities.
	 * Uses weighted average with integer rounding (inherent quality loss).
	 * This is the core economic sink — merging always loses fractional quality.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Utility")
	static uint8 MergeQuality(uint8 Q1, int32 Count1, uint8 Q2, int32 Count2);

	/**
	 * Clamp a raw quality float to the valid uint8 range [0, 100].
	 */
	UFUNCTION(BlueprintCallable, Category = "Quality|Utility")
	static uint8 ClampQuality(float RawQuality);
};
