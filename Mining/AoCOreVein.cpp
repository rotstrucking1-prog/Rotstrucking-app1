// AoCOreVein.cpp
// Architect of Creation - Ore Vein Actor Implementation

#include "AoCOreVein.h"
#include "AoCVoxelWorld.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

// ─── Constructor & BeginPlay ─────────────────────────────────────────────────

AAoCOreVein::AAoCOreVein()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create visual rock mesh as root component
	VeinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VeinMesh"));
	RootComponent = VeinMesh;
	VeinMesh->SetMobility(EComponentMobility::Movable);

	// Try to load a default rock mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RockMeshFinder(
		TEXT("/Game/RuralAustralia/Rocks/Rock_M_03.Rock_M_03"));
	if (RockMeshFinder.Succeeded())
	{
		VeinMesh->SetStaticMesh(RockMeshFinder.Object);
	}

	// Tag this actor so the interaction system can identify it
	Tags.Add(FName("OreVein"));

	VeinCenter = FVector::ZeroVector;
	VeinRadius = 200.f;
	OreMaterial = EVoxelMaterial::OreCopper;
	Quality = 50;
	TotalOre = 300;
	RemainingOre = 300;
	Tier = 1;
	VoxelWorld = nullptr;
	OriginalScale = FVector::OneVector;
}

void AAoCOreVein::BeginPlay()
{
	Super::BeginPlay();

	// Save original scale for shrinking calculation
	OriginalScale = GetActorScale3D();

	// Make sure tag is set
	if (!Tags.Contains(FName("OreVein")))
	{
		Tags.Add(FName("OreVein"));
	}

	// Auto-find VoxelWorld if not set (optional — mining works without it)
	if (!VoxelWorld)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(World, AAoCVoxelWorld::StaticClass(), FoundActors);
			if (FoundActors.Num() > 0)
			{
				VoxelWorld = Cast<AAoCVoxelWorld>(FoundActors[0]);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AoC OreVein: %s — %s — %d ore — Quality %d — Scale(%.1f, %.1f, %.1f)"),
		*GetActorNameOrLabel(),
		*GetOreName(OreMaterial),
		RemainingOre,
		Quality,
		OriginalScale.X, OriginalScale.Y, OriginalScale.Z);
}

// ─── Core Functions ──────────────────────────────────────────────────────────

void AAoCOreVein::InitializeVein(EVoxelMaterial Mat, FVector Center, float Radius, uint8 InQuality, int32 OreAmount)
{
	OreMaterial = Mat;
	VeinCenter = Center;
	VeinRadius = Radius;
	Quality = InQuality;
	TotalOre = OreAmount;
	RemainingOre = OreAmount;
	Tier = GetOreTier(Mat);

	// Position the actor at the vein center
	SetActorLocation(Center);

	// Apply ore-specific color tint to the rock mesh
	if (VeinMesh)
	{
		UMaterialInstanceDynamic* DynMat = VeinMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			FLinearColor OreCol = GetOreColorForMaterial(Mat);
			DynMat->SetVectorParameterValue(FName("BaseColor"), OreCol);
			DynMat->SetVectorParameterValue(FName("Color"), OreCol);
		}
	}
}

FOreExtractResult AAoCOreVein::ExtractOre(float SkillLevel, float ToolQuality)
{
	FOreExtractResult Result;
	Result.Material = OreMaterial;
	Result.bSuccess = false;
	Result.bVeinDepleted = false;

	// Cannot extract from a depleted vein
	if (IsDepleted())
	{
		Result.bVeinDepleted = true;
		return Result;
	}

	// Base extraction: 1 ore per action
	// Skill provides a chance for bonus yield (up to +1 at high skill)
	const int32 BaseExtract = 1;
	const int32 BonusExtract = (FMath::FRand() < (SkillLevel / 200.f)) ? 1 : 0;
	int32 TotalExtract = BaseExtract + BonusExtract;

	// Clamp to remaining ore
	TotalExtract = FMath::Min(TotalExtract, RemainingOre);

	// Calculate output quality:
	// Output Q = (VeinQuality + SkillBonus + ToolQuality) / 3
	const float SkillBonus = SkillLevel;
	const float RawQuality = (static_cast<float>(Quality) + SkillBonus + ToolQuality) / 3.f;
	const uint8 OutputQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(RawQuality), 0, 100));

	// Decrement remaining ore
	RemainingOre -= TotalExtract;

	// Build result
	Result.AmountExtracted = TotalExtract;
	Result.OutputQuality = OutputQ;
	Result.bSuccess = true;
	Result.bVeinDepleted = IsDepleted();

	UE_LOG(LogTemp, Log, TEXT("AoC OreVein: Extracted %d %s (Q%d) — %d remaining"),
		TotalExtract, *GetOreName(OreMaterial), OutputQ, RemainingOre);

	// Update the visual rock scale — rock shrinks as ore is extracted
	UpdateVisualScale();

	// Fire delegate
	OnOreMined.Broadcast(this, Result);

	// Check depletion
	if (IsDepleted())
	{
		OnVeinDepleted.Broadcast(this);
		UE_LOG(LogTemp, Log, TEXT("AoC OreVein: %s DEPLETED"), *GetOreName(OreMaterial));
	}

	return Result;
}

bool AAoCOreVein::IsDepleted() const
{
	return RemainingOre <= 0;
}

float AAoCOreVein::GetRemainingPercentage() const
{
	if (TotalOre <= 0)
	{
		return 0.f;
	}
	return static_cast<float>(RemainingOre) / static_cast<float>(TotalOre);
}

FLinearColor AAoCOreVein::GetOreColor() const
{
	return GetOreColorForMaterial(OreMaterial);
}

// ─── Visual Scale Update ─────────────────────────────────────────────────────

void AAoCOreVein::UpdateVisualScale()
{
	if (TotalOre <= 0) return;

	const float Pct = GetRemainingPercentage();

	if (Pct <= 0.0f)
	{
		// Depleted — hide the rock and disable collision
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		// Destroy after a short delay
		SetLifeSpan(5.0f);

		UE_LOG(LogTemp, Log, TEXT("AoC OreVein: Rock depleted and hidden"));
		return;
	}

	// Scale between 30% and 100% of original size based on remaining ore
	// This gives a satisfying visual shrink as ore is extracted
	const float MinScale = 0.3f;
	const float ScaleFactor = MinScale + (1.0f - MinScale) * Pct;
	SetActorScale3D(OriginalScale * ScaleFactor);
}

// ─── Static Helpers: Item ID for Inventory ──────────────────────────────────

FName AAoCOreVein::GetItemIDForMaterial(EVoxelMaterial Mat)
{
	switch (Mat)
	{
	case EVoxelMaterial::OreCopper:     return FName("ore_copper");
	case EVoxelMaterial::OreTin:        return FName("ore_tin");
	case EVoxelMaterial::OreIron:       return FName("ore_iron");
	case EVoxelMaterial::OreZinc:       return FName("ore_zinc");
	case EVoxelMaterial::OreLead:       return FName("ore_lead");
	case EVoxelMaterial::OreNickel:     return FName("ore_nickel");
	case EVoxelMaterial::OreSilver:     return FName("ore_silver");
	case EVoxelMaterial::OreGold:       return FName("ore_gold");
	case EVoxelMaterial::OreChromium:   return FName("ore_chromium");
	case EVoxelMaterial::OreCobalt:     return FName("ore_cobalt");
	case EVoxelMaterial::OreManganese:  return FName("ore_manganese");
	case EVoxelMaterial::OreMolybdenum: return FName("ore_molybdenum");
	case EVoxelMaterial::OreTitanium:   return FName("ore_titanium");
	case EVoxelMaterial::OreTungsten:   return FName("ore_tungsten");
	case EVoxelMaterial::OreVanadium:   return FName("ore_vanadium");
	case EVoxelMaterial::OrePlatinum:   return FName("ore_platinum");
	case EVoxelMaterial::OrePalladium:  return FName("ore_palladium");
	case EVoxelMaterial::OreIridium:    return FName("ore_iridium");
	case EVoxelMaterial::OreOsmium:     return FName("ore_osmium");
	case EVoxelMaterial::OreRhodium:    return FName("ore_rhodium");
	case EVoxelMaterial::OreNiobium:    return FName("ore_niobium");
	case EVoxelMaterial::OreTantalum:   return FName("ore_tantalum");
	default:                            return FName("ore_unknown");
	}
}

FString AAoCOreVein::GetShortOreName(EVoxelMaterial Mat)
{
	switch (Mat)
	{
	case EVoxelMaterial::OreCopper:     return TEXT("Copper Ore");
	case EVoxelMaterial::OreTin:        return TEXT("Tin Ore");
	case EVoxelMaterial::OreIron:       return TEXT("Iron Ore");
	case EVoxelMaterial::OreZinc:       return TEXT("Zinc Ore");
	case EVoxelMaterial::OreLead:       return TEXT("Lead Ore");
	case EVoxelMaterial::OreNickel:     return TEXT("Nickel Ore");
	case EVoxelMaterial::OreSilver:     return TEXT("Silver Ore");
	case EVoxelMaterial::OreGold:       return TEXT("Gold Ore");
	case EVoxelMaterial::OreChromium:   return TEXT("Chromium Ore");
	case EVoxelMaterial::OreCobalt:     return TEXT("Cobalt Ore");
	case EVoxelMaterial::OreManganese:  return TEXT("Manganese Ore");
	case EVoxelMaterial::OreMolybdenum: return TEXT("Molybdenum Ore");
	case EVoxelMaterial::OreTitanium:   return TEXT("Titanium Ore");
	case EVoxelMaterial::OreTungsten:   return TEXT("Tungsten Ore");
	case EVoxelMaterial::OreVanadium:   return TEXT("Vanadium Ore");
	case EVoxelMaterial::OrePlatinum:   return TEXT("Platinum Ore");
	case EVoxelMaterial::OrePalladium:  return TEXT("Palladium Ore");
	case EVoxelMaterial::OreIridium:    return TEXT("Iridium Ore");
	case EVoxelMaterial::OreOsmium:     return TEXT("Osmium Ore");
	case EVoxelMaterial::OreRhodium:    return TEXT("Rhodium Ore");
	case EVoxelMaterial::OreNiobium:    return TEXT("Niobium Ore");
	case EVoxelMaterial::OreTantalum:   return TEXT("Tantalum Ore");
	default:                            return TEXT("Unknown Ore");
	}
}

// ─── Static Helpers: Ore Name ────────────────────────────────────────────────

FString AAoCOreVein::GetOreName(EVoxelMaterial Mat)
{
	switch (Mat)
	{
	// Tier 1
	case EVoxelMaterial::OreCopper:		return TEXT("Copper Ore (Malachite)");
	case EVoxelMaterial::OreTin:		return TEXT("Tin Ore (Cassiterite)");

	// Tier 2
	case EVoxelMaterial::OreIron:		return TEXT("Iron Ore (Hematite)");
	case EVoxelMaterial::OreZinc:		return TEXT("Zinc Ore (Sphalerite)");
	case EVoxelMaterial::OreLead:		return TEXT("Lead Ore (Galena)");

	// Tier 3
	case EVoxelMaterial::OreNickel:		return TEXT("Nickel Ore (Pentlandite)");
	case EVoxelMaterial::OreSilver:		return TEXT("Silver Ore (Argentite)");
	case EVoxelMaterial::OreGold:		return TEXT("Gold Ore (Native Gold)");

	// Tier 4
	case EVoxelMaterial::OreChromium:	return TEXT("Chromium Ore (Chromite)");
	case EVoxelMaterial::OreCobalt:		return TEXT("Cobalt Ore (Cobaltite)");
	case EVoxelMaterial::OreManganese:	return TEXT("Manganese Ore (Pyrolusite)");
	case EVoxelMaterial::OreMolybdenum:	return TEXT("Molybdenum Ore (Molybdenite)");

	// Tier 5
	case EVoxelMaterial::OreTitanium:	return TEXT("Titanium Ore (Ilmenite)");
	case EVoxelMaterial::OreTungsten:	return TEXT("Tungsten Ore (Wolframite)");
	case EVoxelMaterial::OreVanadium:	return TEXT("Vanadium Ore (Vanadinite)");
	case EVoxelMaterial::OrePlatinum:	return TEXT("Platinum Ore (Native Platinum)");
	case EVoxelMaterial::OrePalladium:	return TEXT("Palladium Ore (Braggite)");

	// Tier 6
	case EVoxelMaterial::OreIridium:	return TEXT("Iridium Ore (Iridosmine)");
	case EVoxelMaterial::OreOsmium:		return TEXT("Osmium Ore (Osmiridium)");
	case EVoxelMaterial::OreRhodium:	return TEXT("Rhodium Ore (Rhodium Concentrate)");
	case EVoxelMaterial::OreNiobium:	return TEXT("Niobium Ore (Columbite)");
	case EVoxelMaterial::OreTantalum:	return TEXT("Tantalum Ore (Tantalite)");

	default: return TEXT("Unknown");
	}
}

// ─── Static Helpers: Ore Tier ────────────────────────────────────────────────

int32 AAoCOreVein::GetOreTier(EVoxelMaterial Mat)
{
	switch (Mat)
	{
	// Tier 1 — Common
	case EVoxelMaterial::OreCopper:
	case EVoxelMaterial::OreTin:
		return 1;

	// Tier 2 — Common
	case EVoxelMaterial::OreIron:
	case EVoxelMaterial::OreZinc:
	case EVoxelMaterial::OreLead:
		return 2;

	// Tier 3 — Uncommon
	case EVoxelMaterial::OreNickel:
	case EVoxelMaterial::OreSilver:
	case EVoxelMaterial::OreGold:
		return 3;

	// Tier 4 — Rare
	case EVoxelMaterial::OreChromium:
	case EVoxelMaterial::OreCobalt:
	case EVoxelMaterial::OreManganese:
	case EVoxelMaterial::OreMolybdenum:
		return 4;

	// Tier 5 — Very Rare
	case EVoxelMaterial::OreTitanium:
	case EVoxelMaterial::OreTungsten:
	case EVoxelMaterial::OreVanadium:
	case EVoxelMaterial::OrePlatinum:
	case EVoxelMaterial::OrePalladium:
		return 5;

	// Tier 6 — Extremely Rare
	case EVoxelMaterial::OreIridium:
	case EVoxelMaterial::OreOsmium:
	case EVoxelMaterial::OreRhodium:
	case EVoxelMaterial::OreNiobium:
	case EVoxelMaterial::OreTantalum:
		return 6;

	default:
		return 0;
	}
}

// ─── Static Helpers: Ore Reduction Temperature ──────────────────────────────

int32 AAoCOreVein::GetOreReductionTemp(EVoxelMaterial Mat)
{
	switch (Mat)
	{
	// Tier 1
	case EVoxelMaterial::OreCopper:		return 1100;
	case EVoxelMaterial::OreTin:		return 1100;

	// Tier 2
	case EVoxelMaterial::OreIron:		return 1200;
	case EVoxelMaterial::OreZinc:		return 950;
	case EVoxelMaterial::OreLead:		return 800;

	// Tier 3
	case EVoxelMaterial::OreNickel:		return 1450;
	case EVoxelMaterial::OreSilver:		return 1000;
	case EVoxelMaterial::OreGold:		return 1100;

	// Tier 4
	case EVoxelMaterial::OreChromium:	return 1850;
	case EVoxelMaterial::OreCobalt:		return 1400;
	case EVoxelMaterial::OreManganese:	return 1400;
	case EVoxelMaterial::OreMolybdenum:	return 2500;

	// Tier 5
	case EVoxelMaterial::OreTitanium:	return 1800;
	case EVoxelMaterial::OreTungsten:	return 3300;
	case EVoxelMaterial::OreVanadium:	return 1750;
	case EVoxelMaterial::OrePlatinum:	return 1800;
	case EVoxelMaterial::OrePalladium:	return 1600;

	// Tier 6
	case EVoxelMaterial::OreIridium:	return 2400;
	case EVoxelMaterial::OreOsmium:		return 3000;
	case EVoxelMaterial::OreRhodium:	return 2000;
	case EVoxelMaterial::OreNiobium:	return 2400;
	case EVoxelMaterial::OreTantalum:	return 2900;

	default: return 0;
	}
}

// ─── Static Helpers: Ore Color ───────────────────────────────────────────────

FLinearColor AAoCOreVein::GetOreColorForMaterial(EVoxelMaterial Mat)
{
	switch (Mat)
	{
	// Tier 1
	case EVoxelMaterial::OreCopper:		return FLinearColor(0.18f, 0.55f, 0.34f, 1.f);
	case EVoxelMaterial::OreTin:		return FLinearColor(0.15f, 0.12f, 0.10f, 1.f);

	// Tier 2
	case EVoxelMaterial::OreIron:		return FLinearColor(0.40f, 0.10f, 0.08f, 1.f);
	case EVoxelMaterial::OreZinc:		return FLinearColor(0.55f, 0.45f, 0.20f, 1.f);
	case EVoxelMaterial::OreLead:		return FLinearColor(0.60f, 0.60f, 0.65f, 1.f);

	// Tier 3
	case EVoxelMaterial::OreNickel:		return FLinearColor(0.65f, 0.55f, 0.25f, 1.f);
	case EVoxelMaterial::OreSilver:		return FLinearColor(0.75f, 0.75f, 0.78f, 1.f);
	case EVoxelMaterial::OreGold:		return FLinearColor(0.85f, 0.75f, 0.15f, 1.f);

	// Tier 4
	case EVoxelMaterial::OreChromium:	return FLinearColor(0.12f, 0.10f, 0.08f, 1.f);
	case EVoxelMaterial::OreCobalt:		return FLinearColor(0.70f, 0.68f, 0.72f, 1.f);
	case EVoxelMaterial::OreManganese:	return FLinearColor(0.30f, 0.30f, 0.32f, 1.f);
	case EVoxelMaterial::OreMolybdenum:	return FLinearColor(0.45f, 0.45f, 0.48f, 1.f);

	// Tier 5
	case EVoxelMaterial::OreTitanium:	return FLinearColor(0.10f, 0.08f, 0.06f, 1.f);
	case EVoxelMaterial::OreTungsten:	return FLinearColor(0.22f, 0.18f, 0.10f, 1.f);
	case EVoxelMaterial::OreVanadium:	return FLinearColor(0.80f, 0.20f, 0.05f, 1.f);
	case EVoxelMaterial::OrePlatinum:	return FLinearColor(0.78f, 0.78f, 0.80f, 1.f);
	case EVoxelMaterial::OrePalladium:	return FLinearColor(0.55f, 0.55f, 0.57f, 1.f);

	// Tier 6
	case EVoxelMaterial::OreIridium:	return FLinearColor(0.72f, 0.72f, 0.80f, 1.f);
	case EVoxelMaterial::OreOsmium:		return FLinearColor(0.60f, 0.65f, 0.80f, 1.f);
	case EVoxelMaterial::OreRhodium:	return FLinearColor(0.80f, 0.80f, 0.82f, 1.f);
	case EVoxelMaterial::OreNiobium:	return FLinearColor(0.15f, 0.12f, 0.10f, 1.f);
	case EVoxelMaterial::OreTantalum:	return FLinearColor(0.18f, 0.10f, 0.08f, 1.f);

	default: return FLinearColor(0.5f, 0.5f, 0.5f, 1.f);
	}
}

// ─── Static Helpers: Generation Parameters ──────────────────────────────────

FOreGenParams AAoCOreVein::GetGenParamsForTier(int32 InTier)
{
	FOreGenParams Params;

	switch (InTier)
	{
	case 1:
		Params.MinDepthM = 3.f;
		Params.MaxDepthM = 12.f;
		Params.MinOre = 200;
		Params.MaxOre = 500;
		Params.SpawnAreaTiles = 50;
		Params.MinQuality = 20;
		Params.MaxQuality = 60;
		break;

	case 2:
		Params.MinDepthM = 6.f;
		Params.MaxDepthM = 18.f;
		Params.MinOre = 150;
		Params.MaxOre = 400;
		Params.SpawnAreaTiles = 75;
		Params.MinQuality = 30;
		Params.MaxQuality = 70;
		break;

	case 3:
		Params.MinDepthM = 10.f;
		Params.MaxDepthM = 26.f;
		Params.MinOre = 80;
		Params.MaxOre = 200;
		Params.SpawnAreaTiles = 150;
		Params.MinQuality = 40;
		Params.MaxQuality = 80;
		break;

	case 4:
		Params.MinDepthM = 15.f;
		Params.MaxDepthM = 32.f;
		Params.MinOre = 50;
		Params.MaxOre = 150;
		Params.SpawnAreaTiles = 300;
		Params.MinQuality = 50;
		Params.MaxQuality = 90;
		break;

	case 5:
		Params.MinDepthM = 22.f;
		Params.MaxDepthM = 45.f;
		Params.MinOre = 30;
		Params.MaxOre = 100;
		Params.SpawnAreaTiles = 500;
		Params.MinQuality = 60;
		Params.MaxQuality = 100;
		break;

	case 6:
		Params.MinDepthM = 32.f;
		Params.MaxDepthM = 65.f;
		Params.MinOre = 10;
		Params.MaxOre = 50;
		Params.SpawnAreaTiles = 1000;
		Params.MinQuality = 70;
		Params.MaxQuality = 100;
		break;

	default:
		break;
	}

	return Params;
}

TArray<EVoxelMaterial> AAoCOreVein::GetMaterialsForTier(int32 InTier)
{
	TArray<EVoxelMaterial> Materials;

	switch (InTier)
	{
	case 1:
		Materials.Add(EVoxelMaterial::OreCopper);
		Materials.Add(EVoxelMaterial::OreTin);
		break;

	case 2:
		Materials.Add(EVoxelMaterial::OreIron);
		Materials.Add(EVoxelMaterial::OreZinc);
		Materials.Add(EVoxelMaterial::OreLead);
		break;

	case 3:
		Materials.Add(EVoxelMaterial::OreNickel);
		Materials.Add(EVoxelMaterial::OreSilver);
		Materials.Add(EVoxelMaterial::OreGold);
		break;

	case 4:
		Materials.Add(EVoxelMaterial::OreChromium);
		Materials.Add(EVoxelMaterial::OreCobalt);
		Materials.Add(EVoxelMaterial::OreManganese);
		Materials.Add(EVoxelMaterial::OreMolybdenum);
		break;

	case 5:
		Materials.Add(EVoxelMaterial::OreTitanium);
		Materials.Add(EVoxelMaterial::OreTungsten);
		Materials.Add(EVoxelMaterial::OreVanadium);
		Materials.Add(EVoxelMaterial::OrePlatinum);
		Materials.Add(EVoxelMaterial::OrePalladium);
		break;

	case 6:
		Materials.Add(EVoxelMaterial::OreIridium);
		Materials.Add(EVoxelMaterial::OreOsmium);
		Materials.Add(EVoxelMaterial::OreRhodium);
		Materials.Add(EVoxelMaterial::OreNiobium);
		Materials.Add(EVoxelMaterial::OreTantalum);
		break;

	default:
		break;
	}

	return Materials;
}
