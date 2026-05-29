// AoCCropDatabase.cpp
// Architect of Creation - Crop Growth Database Implementation

#include "AoCCropDatabase.h"

// ─────────────────────────────────────────────────────────────────────────────
// Subsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UAoCCropDatabase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PopulateAllCrops();
	UE_LOG(LogTemp, Log, TEXT("AoCCropDatabase: Initialized with %d crops."), Crops.Num());
}

void UAoCCropDatabase::Deinitialize()
{
	Crops.Empty();
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

const FCropGrowthData* UAoCCropDatabase::GetCrop(FName CropID) const
{
	return Crops.Find(CropID);
}

bool UAoCCropDatabase::GetCropCopy(FName CropID, FCropGrowthData& OutCrop) const
{
	const FCropGrowthData* Found = Crops.Find(CropID);
	if (Found)
	{
		OutCrop = *Found;
		return true;
	}
	return false;
}

TArray<FCropGrowthData> UAoCCropDatabase::GetCropsForSkillLevel(int32 Skill) const
{
	TArray<FCropGrowthData> Result;
	for (const auto& Pair : Crops)
	{
		if (Pair.Value.MinFarmingSkill <= Skill)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FCropGrowthData> UAoCCropDatabase::GetCropsByCategory(FName Category) const
{
	TArray<FCropGrowthData> Result;
	for (const auto& Pair : Crops)
	{
		if (Pair.Value.CropRotationGroup == Category)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

float UAoCCropDatabase::CheckRotationBonus(FName CurrentCrop, FName PreviousCrop) const
{
	const FCropGrowthData* Current = Crops.Find(CurrentCrop);
	const FCropGrowthData* Previous = Crops.Find(PreviousCrop);

	if (!Current || !Previous)
	{
		return 1.0f; // Unknown crop — neutral
	}

	// Same rotation group planted back-to-back = penalty
	if (Current->CropRotationGroup == Previous->CropRotationGroup)
	{
		return 0.90f;
	}

	// Current crop's ideal predecessor matches the previous crop's group = bonus
	if (Current->PreviousRotationBonus == Previous->CropRotationGroup)
	{
		return 1.05f;
	}

	// Different groups but not the ideal predecessor = neutral
	return 1.0f;
}

int32 UAoCCropDatabase::GetCropCount() const
{
	return Crops.Num();
}

// ─────────────────────────────────────────────────────────────────────────────
// Registration helpers
// ─────────────────────────────────────────────────────────────────────────────

void UAoCCropDatabase::RegisterCrop(const FCropGrowthData& Data)
{
	Crops.Add(Data.CropID, Data);
}

void UAoCCropDatabase::AddCrop(
	const FString& InName,
	float GrowthTime,
	float Water,
	int32 YieldWild,
	int32 YieldDomestic,
	int32 SkillReq,
	bool bPaddy,
	const FString& RotGroup,
	const FString& PrevBonus)
{
	FCropGrowthData D;
	D.CropID              = FName(*InName);
	D.DisplayName         = InName;
	D.GrowthTimeBase      = GrowthTime;
	D.WaterNeed           = Water;
	D.BaseYieldWild       = YieldWild;
	D.BaseYieldDomestic   = YieldDomestic;
	D.MinFarmingSkill     = SkillReq;
	D.SeedItemID          = FName(*(InName + TEXT("_Seeds")));
	D.HarvestedItemID     = FName(*InName);
	D.GrowthStageCount    = 4;
	D.RequiresPaddy       = bPaddy;
	D.CropRotationGroup   = FName(*RotGroup);
	D.PreviousRotationBonus = FName(*PrevBonus);
	RegisterCrop(D);
}

// ─────────────────────────────────────────────────────────────────────────────
// All 25 crops
//
// Growth times (from design doc):
//   Medium  =  600s (10 min)
//   Long    = 1200s (20 min)
//   Very Long = 1800s (30 min)
//
// Rotation groups:  "Vegetable", "Grain", "Industrial"
// Ideal rotation:   Vegetable → Grain → Industrial → Vegetable (cycle)
// ─────────────────────────────────────────────────────────────────────────────

void UAoCCropDatabase::PopulateAllCrops()
{
	// ── Vegetables (11) ─────────────────────────────────────────────────────
	//                       Name          Time    Water  Wild  Dom  Skill  Paddy  RotGroup       PrevBonus

	AddCrop(TEXT("Cabbage"),     600.0f,  0.6f,   6,   12,  60, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Carrot"),      600.0f,  0.4f,   6,   12,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Onion"),       600.0f,  0.4f,   6,   12,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Peas"),        600.0f,  0.5f,  12,   22,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Potato"),      600.0f,  0.5f,   6,    8,  60, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Turnip"),      600.0f,  0.4f,   8,   14,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Garlic"),     1200.0f,  0.3f,   6,   10,  60, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Leek"),        600.0f,  0.5f,   6,   12,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Beetroot"),    600.0f,  0.5f,   8,   14,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Parsnip"),    1200.0f,  0.4f,   6,   10,  30, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Pumpkin"),    1800.0f,  0.7f,   3,    6,  60, false, TEXT("Vegetable"),  TEXT("Industrial"));

	// ── Grains (4) ──────────────────────────────────────────────────────────

	AddCrop(TEXT("Wheat"),       600.0f,  0.4f,   6,    8,  30, false, TEXT("Grain"),      TEXT("Vegetable"));
	AddCrop(TEXT("Barley"),      600.0f,  0.4f,   6,    8,  30, false, TEXT("Grain"),      TEXT("Vegetable"));
	AddCrop(TEXT("Oat"),         600.0f,  0.3f,   6,    8,  30, false, TEXT("Grain"),      TEXT("Vegetable"));
	AddCrop(TEXT("Rye"),         600.0f,  0.3f,   6,    8,  30, false, TEXT("Grain"),      TEXT("Vegetable"));

	// ── Industrial / Utility (6) ────────────────────────────────────────────

	AddCrop(TEXT("Flax"),        600.0f,  0.5f,   4,   16,  30, false, TEXT("Industrial"), TEXT("Grain"));
	AddCrop(TEXT("Grapes"),     1200.0f,  0.6f,   4,    8,  60, false, TEXT("Industrial"), TEXT("Grain"));
	AddCrop(TEXT("Hemp"),        600.0f,  0.4f,   4,   12,  30, false, TEXT("Industrial"), TEXT("Grain"));
	AddCrop(TEXT("Hops"),       1200.0f,  0.6f,   4,    8,  60, false, TEXT("Industrial"), TEXT("Grain"));
	AddCrop(TEXT("Cotton"),     1200.0f,  0.7f,   4,   10,  60, false, TEXT("Industrial"), TEXT("Grain"));
	AddCrop(TEXT("Sunflower"),  1800.0f,  0.5f,   2,    4,  90, false, TEXT("Industrial"), TEXT("Grain"));

	// ── Trees / Fruit (2) ───────────────────────────────────────────────────

	AddCrop(TEXT("Apple"),      1200.0f,  0.5f,   4,    8,  60, false, TEXT("Vegetable"),  TEXT("Industrial"));
	AddCrop(TEXT("Mulberry"),   1200.0f,  0.4f,   2,    4,   0, false, TEXT("Industrial"), TEXT("Grain"));

	// ── Special (2) ─────────────────────────────────────────────────────────

	AddCrop(TEXT("Rice"),       1800.0f,  0.9f,   6,   16,  90, true,  TEXT("Grain"),      TEXT("Vegetable"));
	AddCrop(TEXT("Lavender"),   1200.0f,  0.3f,   4,    8,  60, false, TEXT("Industrial"), TEXT("Grain"));
}
