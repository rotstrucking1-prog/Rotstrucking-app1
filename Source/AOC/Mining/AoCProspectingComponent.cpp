// AoCProspectingComponent.cpp
// Architect of Creation - Prospecting Component Implementation

#include "AoCProspectingComponent.h"
#include "AoCVoxelWorld.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ─── FProspectResult ─────────────────────────────────────────────────────────

FString FProspectResult::GetDescription() const
{
	switch (DetailLevel)
	{
	case EProspectDetailLevel::Vague:
		if (OreMaterial == EVoxelMaterial::Air)
		{
			return TEXT("Nothing found.");
		}
		return TEXT("You found something nearby.");

	case EProspectDetailLevel::Basic:
		return FString::Printf(TEXT("You found %s nearby."), *OreName);

	case EProspectDetailLevel::Detailed:
		{
			const FString DirStr = UAoCProspectingComponent::GetDirectionString(Direction);
			return FString::Printf(
				TEXT("You found %s to the %s, about %.0f tiles away."),
				*OreName, *DirStr, DistanceTiles
			);
		}

	case EProspectDetailLevel::Precise:
		return FString::Printf(
			TEXT("Rich vein of %s at (%.0f, %.0f, %.0f), quality %d, ~%d ore remaining."),
			*OreName,
			VeinLocation.X, VeinLocation.Y, VeinLocation.Z,
			Quality,
			EstimatedRemainingOre
		);

	default:
		return TEXT("Nothing found.");
	}
}

// ─── Constructor & BeginPlay ─────────────────────────────────────────────────

UAoCProspectingComponent::UAoCProspectingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ProspectingSkill = 0.f;
	CurrentStamina = 100.f;
	MaxStamina = 100.f;
	BaseStaminaCost = 15.f;
	XPPerProspect = 8.f;
	XPPerDiscovery = 20.f;
	MaxHistorySize = 10;
	bIsProspecting = false;
	ProspectDuration = 3.f;
	VoxelWorld = nullptr;
	ProspectingXP = 0.f;
	PendingProspectLocation = FVector::ZeroVector;
}

void UAoCProspectingComponent::BeginPlay()
{
	Super::BeginPlay();

	FindVoxelWorld();
}

// ─── Core Prospecting ────────────────────────────────────────────────────────

bool UAoCProspectingComponent::StartProspect(FVector Location)
{
	if (bIsProspecting)
	{
		return false;
	}

	if (!HasEnoughStamina())
	{
		return false;
	}

	if (!VoxelWorld)
	{
		FindVoxelWorld();
		if (!VoxelWorld)
		{
			return false;
		}
	}

	// Consume stamina
	if (!ConsumeStamina(GetStaminaCost()))
	{
		return false;
	}

	bIsProspecting = true;
	PendingProspectLocation = Location;

	// Start prospect timer — takes time to complete
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			ProspectTimerHandle,
			this,
			&UAoCProspectingComponent::OnProspectTimerComplete,
			ProspectDuration,
			false
		);
	}

	return true;
}

void UAoCProspectingComponent::CancelProspect()
{
	if (!bIsProspecting)
	{
		return;
	}

	bIsProspecting = false;
	PendingProspectLocation = FVector::ZeroVector;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ProspectTimerHandle);
	}
}

void UAoCProspectingComponent::OnProspectTimerComplete()
{
	if (!bIsProspecting)
	{
		return;
	}

	// Perform the actual search
	LastProspectResults = PerformProspectSearch(PendingProspectLocation);

	// Always gain base XP for the action
	GainProspectingXP(XPPerProspect);

	// Bonus XP per discovery
	if (LastProspectResults.Num() > 0)
	{
		GainProspectingXP(XPPerDiscovery * LastProspectResults.Num());
	}
	else
	{
		// If nothing found, add a "nothing" result
		FProspectResult EmptyResult;
		EmptyResult.OreName = TEXT("Nothing");
		EmptyResult.OreMaterial = EVoxelMaterial::Air;
		EmptyResult.DetailLevel = GetDetailLevel();
		EmptyResult.ProspectLocation = PendingProspectLocation;
		LastProspectResults.Add(EmptyResult);
	}

	// Add results to history and fire delegates
	for (const FProspectResult& Result : LastProspectResults)
	{
		AddToHistory(Result);
		OnProspectResultAdded.Broadcast(Result);
	}

	// Broadcast the complete set
	OnProspectComplete.Broadcast(LastProspectResults);

	bIsProspecting = false;
	PendingProspectLocation = FVector::ZeroVector;
}

TArray<FProspectResult> UAoCProspectingComponent::PerformProspectSearch(FVector Location)
{
	TArray<FProspectResult> Results;

	if (!VoxelWorld)
	{
		return Results;
	}

	const float SearchRadius = GetSearchRadius();
	const EProspectDetailLevel Detail = GetDetailLevel();

	// Query the VoxelWorld for veins within our search sphere
	TArray<FVeinData> FoundVeins;
	const bool bFoundAny = VoxelWorld->ProspectAt(Location, SearchRadius, FoundVeins);

	if (!bFoundAny)
	{
		return Results;
	}

	// Build result entries for each found vein
	for (const FVeinData& Vein : FoundVeins)
	{
		// Skip depleted veins
		if (Vein.IsDepleted())
		{
			continue;
		}

		FProspectResult Result = BuildResult(Vein, Location, Detail);
		Results.Add(Result);
	}

	return Results;
}

FProspectResult UAoCProspectingComponent::BuildResult(const FVeinData& Vein, FVector ProspectLocation, EProspectDetailLevel Detail) const
{
	FProspectResult Result;

	// Always set these
	Result.OreMaterial = Vein.Material;
	Result.ProspectLocation = ProspectLocation;
	Result.DetailLevel = Detail;

	// Compute direction and distance
	const FVector ToVein = Vein.Center - ProspectLocation;
	const float DistCm = ToVein.Size();
	const float DistTiles = DistCm / VOXEL_SIZE;

	Result.Direction = GetCompassDirection(ProspectLocation, Vein.Center);
	Result.DistanceTiles = DistTiles;

	// Ore name — determine based on material
	// Use the EVoxelMaterial enum display name via UEnum reflection
	const UEnum* MaterialEnum = StaticEnum<EVoxelMaterial>();
	if (MaterialEnum)
	{
		Result.OreName = MaterialEnum->GetDisplayNameTextByValue(static_cast<int64>(Vein.Material)).ToString();
	}
	else
	{
		Result.OreName = TEXT("Unknown Ore");
	}

	// Filter information based on detail level
	switch (Detail)
	{
	case EProspectDetailLevel::Vague:
		// Don't reveal ore name, direction, or quality
		Result.OreName = TEXT("Something");
		Result.DistanceTiles = 0.f;
		Result.Quality = 0;
		Result.VeinLocation = FVector::ZeroVector;
		Result.EstimatedRemainingOre = 0;
		break;

	case EProspectDetailLevel::Basic:
		// Reveal ore name only, not direction or quality
		Result.DistanceTiles = 0.f;
		Result.Quality = 0;
		Result.VeinLocation = FVector::ZeroVector;
		Result.EstimatedRemainingOre = 0;
		break;

	case EProspectDetailLevel::Detailed:
		// Reveal ore name, direction, approximate distance
		// Round distance to nearest tile for some imprecision
		Result.DistanceTiles = FMath::RoundToFloat(DistTiles);
		Result.Quality = 0;
		Result.VeinLocation = FVector::ZeroVector;
		Result.EstimatedRemainingOre = 0;
		break;

	case EProspectDetailLevel::Precise:
		// Reveal everything
		Result.Quality = Vein.Quality;
		Result.VeinLocation = Vein.Center;
		Result.EstimatedRemainingOre = Vein.RemainingOre;
		break;
	}

	return Result;
}

// ─── History ─────────────────────────────────────────────────────────────────

TArray<FProspectResult> UAoCProspectingComponent::GetProspectResults() const
{
	return LastProspectResults;
}

TArray<FProspectResult> UAoCProspectingComponent::GetProspectHistory() const
{
	return ProspectHistory;
}

void UAoCProspectingComponent::ClearHistory()
{
	ProspectHistory.Empty();
}

void UAoCProspectingComponent::AddToHistory(const FProspectResult& Result)
{
	// Don't store "nothing found" results in history
	if (Result.OreMaterial == EVoxelMaterial::Air)
	{
		return;
	}

	ProspectHistory.Add(Result);

	// Trim to MaxHistorySize (remove oldest entries)
	while (ProspectHistory.Num() > MaxHistorySize)
	{
		ProspectHistory.RemoveAt(0);
	}
}

// ─── Calculations ────────────────────────────────────────────────────────────

float UAoCProspectingComponent::GetSearchRadius() const
{
	return GetSearchRadiusTiles() * VOXEL_SIZE;
}

float UAoCProspectingComponent::GetSearchRadiusTiles() const
{
	// Radius = 1 + (Skill / 10) tiles, max 11 tiles at skill 100
	return FMath::Clamp(1.f + (ProspectingSkill / 10.f), 1.f, 11.f);
}

EProspectDetailLevel UAoCProspectingComponent::GetDetailLevel() const
{
	if (ProspectingSkill >= 90.f)
	{
		return EProspectDetailLevel::Precise;
	}
	if (ProspectingSkill >= 60.f)
	{
		return EProspectDetailLevel::Detailed;
	}
	if (ProspectingSkill >= 30.f)
	{
		return EProspectDetailLevel::Basic;
	}
	return EProspectDetailLevel::Vague;
}

float UAoCProspectingComponent::GetStaminaCost() const
{
	// Stamina cost decreases with skill: skill 100 = 40% of base cost
	const float SkillReduction = (ProspectingSkill / 100.f) * 0.6f;
	return BaseStaminaCost * (1.f - SkillReduction);
}

bool UAoCProspectingComponent::HasEnoughStamina() const
{
	return CurrentStamina >= GetStaminaCost();
}

// ─── Skill XP ────────────────────────────────────────────────────────────────

void UAoCProspectingComponent::GainProspectingXP(float Amount)
{
	if (ProspectingSkill >= 100.f)
	{
		return;
	}

	ProspectingXP += Amount;
	while (ProspectingXP >= XPPerSkillPoint && ProspectingSkill < 100.f)
	{
		ProspectingXP -= XPPerSkillPoint;
		ProspectingSkill = FMath::Min(ProspectingSkill + 1.f, 100.f);
	}
}

bool UAoCProspectingComponent::ConsumeStamina(float Amount)
{
	if (CurrentStamina < Amount)
	{
		return false;
	}
	CurrentStamina -= Amount;
	return true;
}

// ─── Direction Utilities ─────────────────────────────────────────────────────

ECompassDirection UAoCProspectingComponent::GetCompassDirection(FVector From, FVector To)
{
	const FVector Delta = To - From;

	// Check vertical dominance first
	const float HorizontalDistSq = Delta.X * Delta.X + Delta.Y * Delta.Y;
	const float VerticalDistSq = Delta.Z * Delta.Z;

	// If vertical distance is more than 2× horizontal, report as above/below
	if (VerticalDistSq > HorizontalDistSq * 4.f)
	{
		return Delta.Z > 0.f ? ECompassDirection::Above : ECompassDirection::Below;
	}

	// Compute horizontal angle (UE5: X = forward/north, Y = right/east)
	const float AngleRad = FMath::Atan2(Delta.Y, Delta.X);
	float AngleDeg = FMath::RadiansToDegrees(AngleRad);
	if (AngleDeg < 0.f)
	{
		AngleDeg += 360.f;
	}

	// 8 compass directions, each spanning 45 degrees
	// N: 337.5 - 22.5, NE: 22.5 - 67.5, E: 67.5 - 112.5, etc.
	if (AngleDeg < 22.5f || AngleDeg >= 337.5f)
	{
		return ECompassDirection::North;
	}
	if (AngleDeg < 67.5f)
	{
		return ECompassDirection::NorthEast;
	}
	if (AngleDeg < 112.5f)
	{
		return ECompassDirection::East;
	}
	if (AngleDeg < 157.5f)
	{
		return ECompassDirection::SouthEast;
	}
	if (AngleDeg < 202.5f)
	{
		return ECompassDirection::South;
	}
	if (AngleDeg < 247.5f)
	{
		return ECompassDirection::SouthWest;
	}
	if (AngleDeg < 292.5f)
	{
		return ECompassDirection::West;
	}
	return ECompassDirection::NorthWest;
}

FString UAoCProspectingComponent::GetDirectionString(ECompassDirection Dir)
{
	switch (Dir)
	{
	case ECompassDirection::North:		return TEXT("north");
	case ECompassDirection::NorthEast:	return TEXT("northeast");
	case ECompassDirection::East:		return TEXT("east");
	case ECompassDirection::SouthEast:	return TEXT("southeast");
	case ECompassDirection::South:		return TEXT("south");
	case ECompassDirection::SouthWest:	return TEXT("southwest");
	case ECompassDirection::West:		return TEXT("west");
	case ECompassDirection::NorthWest:	return TEXT("northwest");
	case ECompassDirection::Above:		return TEXT("above");
	case ECompassDirection::Below:		return TEXT("below");
	default:							return TEXT("nearby");
	}
}

// ─── Utility ─────────────────────────────────────────────────────────────────

void UAoCProspectingComponent::FindVoxelWorld()
{
	if (VoxelWorld)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AAoCVoxelWorld::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		VoxelWorld = Cast<AAoCVoxelWorld>(FoundActors[0]);
	}
}
