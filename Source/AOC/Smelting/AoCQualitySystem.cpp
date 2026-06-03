// AoCQualitySystem.cpp
// Architect of Creation - Quality System Implementation

#include "AoCQualitySystem.h"

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void UAoCQualitySystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("AoCQualitySystem initialized."));
}

void UAoCQualitySystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("AoCQualitySystem deinitialized."));
	Super::Deinitialize();
}

bool UAoCQualitySystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

// ─── Quality Calculation Functions ───────────────────────────────────────────

uint8 UAoCQualitySystem::CalculateGatheredQuality(float Skill, float SourceQ)
{
	// Q = min(skill, source_Q)
	const float Result = FMath::Min(Skill, SourceQ);
	return ClampQuality(Result);
}

uint8 UAoCQualitySystem::CalculateMiningQuality(float VeinQ)
{
	// Mining Q = source vein quality (tool Q does NOT matter)
	return ClampQuality(VeinQ);
}

uint8 UAoCQualitySystem::CalculateFarmingQuality(float Skill, float SoilQ, float SeedQ, float WaterQ)
{
	// Q = min(skill, soil_Q, seed_Q, water_Q)
	const float MinSeedWater = FMath::Min(SeedQ, WaterQ);
	const float Result = FMath::Min3(Skill, SoilQ, MinSeedWater);
	return ClampQuality(Result);
}

uint8 UAoCQualitySystem::CalculateCraftingQuality(float Skill, const TArray<FQualityInput>& Components, float ShopBonus, float MaterialModifier)
{
	// Step 1: predicted_Q = sum(component_Q * influence%)
	float PredictedQ = 0.f;
	for (const FQualityInput& Input : Components)
	{
		PredictedQ += Input.Quality * Input.InfluencePercent;
	}

	// Step 2: Non-linear quality curve
	// nl_quality = 3 + (predicted_Q ^ 1.35) / 4.5
	const float NLQuality = 3.f + FMath::Pow(FMath::Max(PredictedQ, 0.f), 1.35f) / 4.5f;

	// Step 3: Apply shop bonus and material modifier
	// final_Q = floor(min(skill, min(nl_quality * shop_bonus, 100) * material_modifier))
	const float BonusedQ = FMath::Min(NLQuality * ShopBonus, 100.f);
	const float ModifiedQ = BonusedQ * MaterialModifier;
	const float FinalQ = FMath::Floor(FMath::Min(Skill, ModifiedQ));

	return ClampQuality(FinalQ);
}

uint8 UAoCQualitySystem::CalculateSmeltingQuality(float Skill, float OreQ, float FurnaceCondition)
{
	// Q = min(smelting_skill, ore_Q, furnace_condition%)
	const float Result = FMath::Min3(Skill, OreQ, FurnaceCondition);
	return ClampQuality(Result);
}

// ─── Quality Effect Functions ────────────────────────────────────────────────

int32 UAoCQualitySystem::GetToolDurability(int32 BaseHP, uint8 Quality)
{
	// durability = BaseHP * (1.0 + Q/100.0)
	// At Q0 = 1.0x BaseHP, at Q100 = 2.0x BaseHP
	const float Multiplier = 1.0f + static_cast<float>(Quality) / 100.0f;
	return FMath::FloorToInt(static_cast<float>(BaseHP) * Multiplier);
}

float UAoCQualitySystem::GetWeaponDamageMultiplier(uint8 Quality)
{
	// multiplier = 0.5 + Q/100.0
	// At Q0 = 0.5x, at Q100 = 1.5x
	return 0.5f + static_cast<float>(Quality) / 100.0f;
}

float UAoCQualitySystem::GetArmorMitigation(float BaseMitigation, uint8 Quality)
{
	// mitigation = BaseMitigation * (0.5 + Q/100.0)
	const float Multiplier = 0.5f + static_cast<float>(Quality) / 100.0f;
	return BaseMitigation * Multiplier;
}

int32 UAoCQualitySystem::GetBuildingDurability(uint8 Quality)
{
	// durability = floor(50 + 1.5 * Q) * 100
	// At Q0 = 5000 HP, at Q100 = 20000 HP
	const float Base = 50.f + 1.5f * static_cast<float>(Quality);
	return FMath::FloorToInt(Base) * 100;
}

float UAoCQualitySystem::GetMaterialModifier(uint8 Tier)
{
	// Material tier modifiers determine maximum achievable quality.
	// T1 (Copper, Tin):                                        0.6 → max Q 60
	// T2 (Iron, Zinc, Lead):                                   0.7 → max Q 70
	// T3 (Nickel, Silver, Gold):                                0.8 → max Q 80
	// T4 (Chromium, Cobalt, Manganese, Molybdenum):             0.9 → max Q 90
	// T5 (Titanium, Tungsten, Vanadium, Platinum, Palladium):   1.0 → max Q 100
	// T6 (Iridium, Osmium, Rhodium, Niobium, Tantalum):         1.0 → max Q 100
	switch (Tier)
	{
		case 1:  return 0.6f;
		case 2:  return 0.7f;
		case 3:  return 0.8f;
		case 4:  return 0.9f;
		case 5:  return 1.0f;
		case 6:  return 1.0f;
		default: return 0.5f;
	}
}

// ─── Quality Utility Functions ───────────────────────────────────────────────

uint8 UAoCQualitySystem::MergeQuality(uint8 Q1, int32 Count1, uint8 Q2, int32 Count2)
{
	// Weighted average with integer division → inherent quality loss.
	// This is a core economic sink: merging stacks always rounds down.
	const int32 TotalCount = Count1 + Count2;
	if (TotalCount <= 0)
	{
		return 0;
	}

	const int32 WeightedSum = static_cast<int32>(Q1) * Count1 + static_cast<int32>(Q2) * Count2;
	const int32 Result = WeightedSum / TotalCount; // Integer division = quality loss

	return static_cast<uint8>(FMath::Clamp(Result, 0, 100));
}

uint8 UAoCQualitySystem::ClampQuality(float RawQuality)
{
	return static_cast<uint8>(FMath::Clamp(FMath::FloorToInt(RawQuality), 0, 100));
}
