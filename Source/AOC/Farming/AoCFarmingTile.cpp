// AoCFarmingTile.cpp
// Architect of Creation - Farmable Ground Tile Implementation

#include "AoCFarmingTile.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

AAoCFarmingTile::AAoCFarmingTile()
{
	PrimaryActorTick.bCanEverTick = true;

	// Tile ground mesh — engine cube scaled flat (200×200×10 cm)
	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	RootComponent = TileMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		TileMesh->SetStaticMesh(CubeMesh.Object);
	}
	TileMesh->SetWorldScale3D(FVector(2.f, 2.f, 0.1f));

	// Crop visual mesh — starts hidden, scaled up as crop grows
	CropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CropMesh"));
	CropMesh->SetupAttachment(TileMesh);
	CropMesh->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	CropMesh->SetVisibility(false);

	// Defaults
	TileState = EFarmTileState::Untouched;
	SoilQuality = ESoilQuality::Fair;
	TileSize = 200.f;
	GrowthProgress = 0.f;
	WaterLevel = 0.f;
	FertilizerBonus = 0.f;
	bIsWatered = false;
	ReadyElapsedTime = 0.f;
	TileDynamicMaterial = nullptr;
}

// ─── BeginPlay ───────────────────────────────────────────────────────────────

void AAoCFarmingTile::BeginPlay()
{
	Super::BeginPlay();

	InitCropDatabase();
	UpdateTileVisuals();
}

// ─── Tick ────────────────────────────────────────────────────────────────────

void AAoCFarmingTile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Drain water slowly
	if (WaterLevel > 0.f)
	{
		// Drain roughly 0.01 per second → full tank lasts ~100s
		WaterLevel = FMath::Max(0.f, WaterLevel - 0.01f * DeltaTime);
		bIsWatered = WaterLevel > 0.1f;
	}
	else
	{
		bIsWatered = false;
	}

	// Advance growth when a crop is planted / growing
	if (TileState == EFarmTileState::Planted ||
		TileState == EFarmTileState::Watered ||
		TileState == EFarmTileState::Growing)
	{
		UpdateGrowth(DeltaTime);
	}

	// Track withering
	if (TileState == EFarmTileState::Ready)
	{
		CheckWithering();
		ReadyElapsedTime += DeltaTime;
	}
}

// ─── Crop Database Initialization ────────────────────────────────────────────

void AAoCFarmingTile::InitCropDatabase()
{
	auto AddCrop = [this](const FName& ID, const FString& Name, float GrowthTime, float Water, int32 Yield)
	{
		FCropData Data;
		Data.CropID = ID;
		Data.DisplayName = Name;
		Data.GrowthTimeBase = GrowthTime;
		Data.WaterNeed = Water;
		Data.BaseYield = Yield;
		Data.CurrentStage = EGrowthStage::Seed;
		CropDatabase.Add(ID, Data);
	};

	//                     ID              Display       Time   Water  Yield
	AddCrop(FName("Wheat"),    TEXT("Wheat"),     600.f, 0.5f, 4);
	AddCrop(FName("Barley"),   TEXT("Barley"),    500.f, 0.4f, 4);
	AddCrop(FName("Rye"),      TEXT("Rye"),       550.f, 0.4f, 3);
	AddCrop(FName("Corn"),     TEXT("Corn"),      700.f, 0.6f, 5);
	AddCrop(FName("Rice"),     TEXT("Rice"),      800.f, 0.9f, 5);
	AddCrop(FName("Flax"),     TEXT("Flax"),      400.f, 0.3f, 3);
	AddCrop(FName("Cotton"),   TEXT("Cotton"),    450.f, 0.4f, 3);
	AddCrop(FName("Cabbage"),  TEXT("Cabbage"),   500.f, 0.5f, 2);
	AddCrop(FName("Carrot"),   TEXT("Carrot"),    350.f, 0.4f, 6);
	AddCrop(FName("Potato"),   TEXT("Potato"),    600.f, 0.5f, 8);
	AddCrop(FName("Onion"),    TEXT("Onion"),     400.f, 0.3f, 5);
	AddCrop(FName("Garlic"),   TEXT("Garlic"),    350.f, 0.3f, 6);
	AddCrop(FName("Pumpkin"),  TEXT("Pumpkin"),   900.f, 0.7f, 2);
	AddCrop(FName("Herbs"),    TEXT("Herbs"),     300.f, 0.3f, 4);
	AddCrop(FName("Hops"),     TEXT("Hops"),      700.f, 0.6f, 3);
}

// ─── Tile Actions ────────────────────────────────────────────────────────────

bool AAoCFarmingTile::PlowTile()
{
	if (TileState != EFarmTileState::Untouched)
	{
		return false;
	}

	TileState = EFarmTileState::Plowed;
	UpdateTileVisuals();
	OnTileStateChanged.Broadcast(this, TileState);
	return true;
}

bool AAoCFarmingTile::FertilizeTile(float Amount)
{
	if (TileState == EFarmTileState::Untouched || TileState == EFarmTileState::Harvested)
	{
		return false;
	}

	FertilizerBonus = FMath::Clamp(FertilizerBonus + Amount, 0.f, 1.f);

	if (TileState == EFarmTileState::Plowed)
	{
		TileState = EFarmTileState::Fertilized;
		OnTileStateChanged.Broadcast(this, TileState);
	}

	UpdateTileVisuals();
	return true;
}

bool AAoCFarmingTile::PlantCrop(FName CropID)
{
	if (TileState != EFarmTileState::Plowed && TileState != EFarmTileState::Fertilized)
	{
		return false;
	}

	const FCropData* Template = CropDatabase.Find(CropID);
	if (!Template)
	{
		UE_LOG(LogTemp, Warning, TEXT("AAoCFarmingTile::PlantCrop — Unknown CropID: %s"), *CropID.ToString());
		return false;
	}

	PlantedCrop = *Template;
	PlantedCrop.CurrentStage = EGrowthStage::Seed;
	GrowthProgress = 0.f;
	ReadyElapsedTime = 0.f;

	TileState = EFarmTileState::Planted;
	UpdateTileVisuals();
	UpdateCropVisuals();
	CropMesh->SetVisibility(true);

	OnTileStateChanged.Broadcast(this, TileState);
	return true;
}

bool AAoCFarmingTile::WaterTile()
{
	if (TileState == EFarmTileState::Untouched)
	{
		return false;
	}

	WaterLevel = 1.f;
	bIsWatered = true;

	if (TileState == EFarmTileState::Planted)
	{
		TileState = EFarmTileState::Watered;
		OnTileStateChanged.Broadcast(this, TileState);
	}

	UpdateTileVisuals();
	return true;
}

FName AAoCFarmingTile::HarvestCrop()
{
	if (TileState != EFarmTileState::Ready)
	{
		return NAME_None;
	}

	if (PlantedCrop.CurrentStage == EGrowthStage::Withered)
	{
		// Withered — nothing to harvest
		TileState = EFarmTileState::Harvested;
		CropMesh->SetVisibility(false);
		OnTileStateChanged.Broadcast(this, TileState);
		return NAME_None;
	}

	// Calculate yield: BaseYield * SoilMultiplier * (1 + Fertilizer) ± 20%
	const float SoilMult = GetSoilQualityMultiplier();
	const float RawYield = PlantedCrop.BaseYield * SoilMult * (1.f + FertilizerBonus);
	const float Variance = FMath::FRandRange(-0.2f, 0.2f);
	const int32 FinalYield = FMath::Max(1, FMath::RoundToInt(RawYield * (1.f + Variance)));

	const FName HarvestedCropID = PlantedCrop.CropID;

	UE_LOG(LogTemp, Log, TEXT("Harvested %s x%d"), *HarvestedCropID.ToString(), FinalYield);

	// Reset tile
	TileState = EFarmTileState::Harvested;
	CropMesh->SetVisibility(false);
	GrowthProgress = 0.f;
	FertilizerBonus = 0.f;
	PlantedCrop = FCropData();

	UpdateTileVisuals();
	OnTileStateChanged.Broadcast(this, TileState);

	return HarvestedCropID;
}

// ─── Growth Logic ────────────────────────────────────────────────────────────

void AAoCFarmingTile::UpdateGrowth(float DeltaTime)
{
	if (PlantedCrop.CropID == NAME_None)
	{
		return;
	}

	const float SoilMult = GetSoilQualityMultiplier();
	const float WaterMult = (WaterLevel > 0.3f) ? 1.f : 0.25f;
	const float GrowthModifier = SoilMult * WaterMult * (1.f + FertilizerBonus);
	const float EffectiveGrowthTime = PlantedCrop.GrowthTimeBase / FMath::Max(GrowthModifier, 0.01f);

	GrowthProgress = FMath::Clamp(GrowthProgress + DeltaTime / EffectiveGrowthTime, 0.f, 1.f);

	// Advance growth stage based on progress thresholds
	AdvanceGrowthStage();

	// Transition tile state to Growing once growth begins
	if (GrowthProgress > 0.05f &&
		(TileState == EFarmTileState::Planted || TileState == EFarmTileState::Watered))
	{
		TileState = EFarmTileState::Growing;
		UpdateTileVisuals();
		OnTileStateChanged.Broadcast(this, TileState);
	}

	// Crop fully grown
	if (GrowthProgress >= 1.f && TileState != EFarmTileState::Ready)
	{
		TileState = EFarmTileState::Ready;
		ReadyElapsedTime = 0.f;
		UpdateTileVisuals();
		OnCropReady.Broadcast(this);
		OnTileStateChanged.Broadcast(this, TileState);
	}

	UpdateCropVisuals();
}

void AAoCFarmingTile::AdvanceGrowthStage()
{
	EGrowthStage NewStage = PlantedCrop.CurrentStage;

	if (GrowthProgress >= 1.0f)
	{
		NewStage = EGrowthStage::Ready;
	}
	else if (GrowthProgress >= 0.75f)
	{
		NewStage = EGrowthStage::Mature;
	}
	else if (GrowthProgress >= 0.5f)
	{
		NewStage = EGrowthStage::Young;
	}
	else if (GrowthProgress >= 0.25f)
	{
		NewStage = EGrowthStage::Sprout;
	}
	else if (GrowthProgress >= 0.1f)
	{
		NewStage = EGrowthStage::Sprout; // early sprout
	}
	else
	{
		NewStage = EGrowthStage::Seed;
	}

	PlantedCrop.CurrentStage = NewStage;
}

void AAoCFarmingTile::CheckWithering()
{
	if (PlantedCrop.CurrentStage == EGrowthStage::Ready && ReadyElapsedTime >= WitherTimeThreshold)
	{
		PlantedCrop.CurrentStage = EGrowthStage::Withered;
		UpdateCropVisuals();
		UE_LOG(LogTemp, Warning, TEXT("Crop %s withered on tile at %s"),
			*PlantedCrop.CropID.ToString(), *GetActorLocation().ToString());
	}
}

// ─── Visuals ─────────────────────────────────────────────────────────────────

void AAoCFarmingTile::UpdateTileVisuals()
{
	if (!TileMesh)
	{
		return;
	}

	// Create dynamic material on first call
	if (!TileDynamicMaterial)
	{
		UMaterialInterface* BaseMat = TileMesh->GetMaterial(0);
		if (BaseMat)
		{
			TileDynamicMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			TileMesh->SetMaterial(0, TileDynamicMaterial);
		}
	}

	if (!TileDynamicMaterial)
	{
		return;
	}

	FLinearColor TileColor;
	switch (TileState)
	{
	case EFarmTileState::Untouched:
		TileColor = FLinearColor(0.2f, 0.5f, 0.1f, 1.f);  // green
		break;
	case EFarmTileState::Plowed:
		TileColor = FLinearColor(0.4f, 0.25f, 0.1f, 1.f);  // brown
		break;
	case EFarmTileState::Fertilized:
		TileColor = FLinearColor(0.35f, 0.2f, 0.05f, 1.f);  // rich brown
		break;
	case EFarmTileState::Planted:
		TileColor = FLinearColor(0.3f, 0.2f, 0.08f, 1.f);  // dark brown
		break;
	case EFarmTileState::Watered:
		TileColor = FLinearColor(0.25f, 0.18f, 0.05f, 1.f);  // wet dark brown
		break;
	case EFarmTileState::Growing:
		TileColor = FLinearColor(0.3f, 0.55f, 0.15f, 1.f);  // light green
		break;
	case EFarmTileState::Ready:
		TileColor = FLinearColor(0.85f, 0.7f, 0.1f, 1.f);  // golden
		break;
	case EFarmTileState::Harvested:
		TileColor = FLinearColor(0.5f, 0.35f, 0.15f, 1.f);  // pale brown
		break;
	default:
		TileColor = FLinearColor::White;
		break;
	}

	TileDynamicMaterial->SetVectorParameterValue(FName("BaseColor"), TileColor);
}

void AAoCFarmingTile::UpdateCropVisuals()
{
	if (!CropMesh)
	{
		return;
	}

	float ScaleFactor = 0.1f; // Seed default

	switch (PlantedCrop.CurrentStage)
	{
	case EGrowthStage::Seed:
		ScaleFactor = 0.1f;
		break;
	case EGrowthStage::Sprout:
		ScaleFactor = 0.25f;
		break;
	case EGrowthStage::Young:
		ScaleFactor = 0.5f;
		break;
	case EGrowthStage::Mature:
		ScaleFactor = 0.75f;
		break;
	case EGrowthStage::Ready:
		ScaleFactor = 1.0f;
		break;
	case EGrowthStage::Withered:
		ScaleFactor = 0.6f;  // shrinks slightly when withered
		break;
	}

	CropMesh->SetWorldScale3D(FVector(ScaleFactor, ScaleFactor, ScaleFactor));
}

// ─── Soil Helpers ────────────────────────────────────────────────────────────

ESoilQuality AAoCFarmingTile::GetEffectiveSoilQuality() const
{
	// Fertilizer can bump soil quality by one level
	int32 Base = static_cast<int32>(SoilQuality);
	if (FertilizerBonus > 0.5f)
	{
		Base = FMath::Min(Base + 1, static_cast<int32>(ESoilQuality::Excellent));
	}
	return static_cast<ESoilQuality>(Base);
}

float AAoCFarmingTile::GetSoilQualityMultiplier() const
{
	switch (GetEffectiveSoilQuality())
	{
	case ESoilQuality::Poor:      return 0.5f;
	case ESoilQuality::Fair:      return 0.75f;
	case ESoilQuality::Good:      return 1.0f;
	case ESoilQuality::Great:     return 1.25f;
	case ESoilQuality::Excellent: return 1.5f;
	default:                      return 1.0f;
	}
}
