// AoCTerraformingComponent.cpp
// Architect of Creation - Terraforming Component Implementation

#include "AoCTerraformingComponent.h"
#include "AoCTerrainTile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

UAoCTerraformingComponent::UAoCTerraformingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	TerraformSkill = 0.f;
	TileSize = 200.f;
	HeightStep = 50.f;
	MaxSlopeAngle = 45.f;
	bIsObserving = false;
	bIsTerraforming = false;
	CurrentAction = ETerraformAction::Observe;
	PendingTargetLocation = FVector::ZeroVector;
	TerraformXP = 0.f;
}

// ─── BeginPlay ───────────────────────────────────────────────────────────────

void UAoCTerraformingComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ─── Public Interface ────────────────────────────────────────────────────────

bool UAoCTerraformingComponent::StartTerraform(ETerraformAction Action, FVector TargetLocation)
{
	if (bIsTerraforming)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAoCTerraformingComponent: Already terraforming."));
		return false;
	}

	if (!HasShovelEquipped() && Action != ETerraformAction::Observe)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAoCTerraformingComponent: No shovel equipped."));
		return false;
	}

	// Observe mode is special — no timer, just toggle camera
	if (Action == ETerraformAction::Observe)
	{
		EnterObserveMode();
		return true;
	}

	// Check stamina (placeholder: owner must have at least StaminaCost available)
	// In a real system this would query the character's stamina component
	const float Cost = GetStaminaCost();
	// TODO: if (OwnerStamina < Cost) return false;

	CurrentAction = Action;
	PendingTargetLocation = TargetLocation;
	bIsTerraforming = true;

	const float ActionDuration = GetActionTime();

	UE_LOG(LogTemp, Log, TEXT("Terraforming: %d at %s — %.1fs"),
		static_cast<int32>(Action), *TargetLocation.ToString(), ActionDuration);

	// Start timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TerraformTimer,
			this,
			&UAoCTerraformingComponent::OnTerraformActionComplete,
			ActionDuration,
			false);
	}

	return true;
}

void UAoCTerraformingComponent::CancelTerraform()
{
	if (!bIsTerraforming)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TerraformTimer);
	}

	bIsTerraforming = false;
	UE_LOG(LogTemp, Log, TEXT("Terraforming cancelled."));
}

void UAoCTerraformingComponent::EnterObserveMode()
{
	bIsObserving = true;
	UE_LOG(LogTemp, Log, TEXT("Entered Observe mode (top-down camera)."));
	// TODO: Switch camera to top-down view on owning character
}

void UAoCTerraformingComponent::ExitObserveMode()
{
	bIsObserving = false;
	UE_LOG(LogTemp, Log, TEXT("Exited Observe mode."));
	// TODO: Restore normal camera
}

// ─── Timer Callback ──────────────────────────────────────────────────────────

void UAoCTerraformingComponent::OnTerraformActionComplete()
{
	bIsTerraforming = false;

	AAoCTerrainTile* Tile = FindOrCreateTileAt(PendingTargetLocation);
	if (!Tile)
	{
		UE_LOG(LogTemp, Error, TEXT("Terraforming: Could not find or create tile."));
		return;
	}

	switch (CurrentAction)
	{
	case ETerraformAction::LowerGround:
		ApplyLowerGround(Tile, PendingTargetLocation);
		break;

	case ETerraformAction::RaiseGround:
		ApplyRaiseGround(Tile, PendingTargetLocation);
		break;

	case ETerraformAction::Flatten:
		ApplyFlatten(Tile, PendingTargetLocation);
		break;

	case ETerraformAction::FlattenSlopeUp:
		// Same as flatten but biased upward — use average of upper half
		ApplyFlatten(Tile, PendingTargetLocation);
		break;

	case ETerraformAction::FlattenSlopeDown:
		// Same as flatten but biased downward — use average of lower half
		ApplyFlatten(Tile, PendingTargetLocation);
		break;

	default:
		break;
	}

	// Gain XP
	GainTerraformXP(10.f);

	// Deduct stamina (placeholder)
	// const float Cost = GetStaminaCost();
	// OwnerCharacter->DeductStamina(Cost);

	// Broadcast delegate
	OnTerraformComplete.Broadcast(CurrentAction, PendingTargetLocation);

	UE_LOG(LogTemp, Log, TEXT("Terraform action %d complete at %s."),
		static_cast<int32>(CurrentAction), *PendingTargetLocation.ToString());
}

// ─── Terrain Modification ────────────────────────────────────────────────────

void UAoCTerraformingComponent::ApplyLowerGround(AAoCTerrainTile* Tile, FVector HitPoint)
{
	if (!Tile)
	{
		return;
	}

	const int32 Idx = Tile->GetNearestPointIndex(HitPoint);
	const float Current = Tile->GetHeightAtPoint(Idx);
	Tile->SetHeightAtPoint(Idx, Current - HeightStep);
}

void UAoCTerraformingComponent::ApplyRaiseGround(AAoCTerrainTile* Tile, FVector HitPoint)
{
	if (!Tile)
	{
		return;
	}

	const int32 Idx = Tile->GetNearestPointIndex(HitPoint);
	const float Current = Tile->GetHeightAtPoint(Idx);
	Tile->SetHeightAtPoint(Idx, Current + HeightStep);
}

void UAoCTerraformingComponent::ApplyFlatten(AAoCTerrainTile* Tile, FVector HitPoint)
{
	if (!Tile)
	{
		return;
	}

	// Compute the average height of all 9 points
	float Sum = 0.f;
	for (int32 i = 0; i < 9; ++i)
	{
		Sum += Tile->GetHeightAtPoint(i);
	}
	const float Average = Sum / 9.f;

	// Set all points to the average
	TArray<float> NewHeights;
	NewHeights.SetNum(9);
	for (int32 i = 0; i < 9; ++i)
	{
		NewHeights[i] = Average;
	}
	Tile->SetAllHeights(NewHeights);
}

// ─── Tile Lookup / Spawn ─────────────────────────────────────────────────────

AAoCTerrainTile* UAoCTerraformingComponent::FindOrCreateTileAt(FVector Location)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Snap location to grid
	const float HalfTile = TileSize * 0.5f;
	const float SnapX = FMath::RoundToFloat(Location.X / TileSize) * TileSize;
	const float SnapY = FMath::RoundToFloat(Location.Y / TileSize) * TileSize;
	const FVector SnappedLocation(SnapX, SnapY, 0.f);

	// Search existing terrain tiles within half a tile distance
	const float SearchRadius = HalfTile + 1.f;

	for (TActorIterator<AAoCTerrainTile> It(World); It; ++It)
	{
		AAoCTerrainTile* Existing = *It;
		if (Existing && FVector::DistSquared2D(Existing->GetActorLocation(), SnappedLocation) < SearchRadius * SearchRadius)
		{
			return Existing;
		}
	}

	// Spawn a new tile
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAoCTerrainTile* NewTile = World->SpawnActor<AAoCTerrainTile>(
		AAoCTerrainTile::StaticClass(),
		SnappedLocation,
		FRotator::ZeroRotator,
		SpawnParams);

	if (NewTile)
	{
		NewTile->GridCoord = FVector2D(SnapX / TileSize, SnapY / TileSize);
		UE_LOG(LogTemp, Log, TEXT("Spawned terrain tile at %s (grid %s)"),
			*SnappedLocation.ToString(), *NewTile->GridCoord.ToString());
	}

	return NewTile;
}

// ─── Skill & Stamina ─────────────────────────────────────────────────────────

float UAoCTerraformingComponent::GetActionTime() const
{
	// 30 seconds at skill 0, 5 seconds at skill 100
	return FMath::Lerp(30.f, 5.f, FMath::Clamp(TerraformSkill / 100.f, 0.f, 1.f));
}

float UAoCTerraformingComponent::GetStaminaCost() const
{
	// 50 stamina at skill 0, 15 at skill 100
	return FMath::Lerp(50.f, 15.f, FMath::Clamp(TerraformSkill / 100.f, 0.f, 1.f));
}

void UAoCTerraformingComponent::GainTerraformXP(float Amount)
{
	TerraformXP += Amount;
	while (TerraformXP >= XPPerSkillPoint && TerraformSkill < 100.f)
	{
		TerraformXP -= XPPerSkillPoint;
		TerraformSkill = FMath::Min(TerraformSkill + 1.f, 100.f);
		UE_LOG(LogTemp, Log, TEXT("Terraform skill increased to %.0f"), TerraformSkill);
	}
}

bool UAoCTerraformingComponent::HasShovelEquipped() const
{
	// Placeholder — always returns true
	return true;
}
