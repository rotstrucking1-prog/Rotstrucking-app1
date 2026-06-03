// AoCMineSupport.cpp
// Architect of Creation - Mine Support Actor Implementation

#include "AoCMineSupport.h"
#include "AoCVoxelWorld.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

AAoCMineSupport::AAoCMineSupport()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create mesh component
	SupportMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SupportMesh"));
	RootComponent = SupportMesh;

	// Defaults
	SupportType = ESupportType::Beam;
	CoverageRadius = 0; // Beam = 0 (own tile only); set to 1 for Column in OnPlaced
	Health = 100.f;
	MaxHealth = 100.f;
	TilePosition = FIntVector::ZeroValue;
	VoxelWorld = nullptr;
	BeamMesh = nullptr;
	ColumnMesh = nullptr;
	ActiveDustEffect = nullptr;

	// Collapse system defaults
	// 24 game hours; assuming 1 real minute = 1 game hour → 1440 real seconds
	CollapseTimeThreshold = 1440.f;
	CollapseDamage = 99999.f; // Effectively instant death
	CollapseDamageRadius = CHUNK_WORLD_SIZE; // Entire chunk radius

	// VFX / Audio defaults (set via Blueprint or level)
	DustParticleEffect = nullptr;
	CreakingSound = nullptr;
	CrackingSound = nullptr;
	CollapseSound = nullptr;
}

// ─── BeginPlay & Tick ────────────────────────────────────────────────────────

void AAoCMineSupport::BeginPlay()
{
	Super::BeginPlay();

	FindVoxelWorld();
}

void AAoCMineSupport::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update instability for all tracked tiles
	for (int32 i = TrackedTiles.Num() - 1; i >= 0; --i)
	{
		FTileInstability& Tile = TrackedTiles[i];

		if (Tile.bIsSupported)
		{
			// Supported tiles slowly recover stability
			Tile.UnsupportedTime = FMath::Max(0.f, Tile.UnsupportedTime - DeltaTime * 2.f);
			Tile.Instability = FMath::Clamp(Tile.UnsupportedTime / CollapseTimeThreshold, 0.f, 1.f);
			continue;
		}

		// Unsupported: accumulate instability over time
		Tile.UnsupportedTime += DeltaTime;
		Tile.Instability = FMath::Clamp(Tile.UnsupportedTime / CollapseTimeThreshold, 0.f, 1.f);

		// Check warning level transitions
		const ECollapseWarningLevel CurrentLevel = GetWarningLevel(Tile.Instability);
		const ECollapseWarningLevel* PrevLevel = LastWarningLevels.Find(Tile.TilePos);

		if (!PrevLevel || *PrevLevel != CurrentLevel)
		{
			LastWarningLevels.Add(Tile.TilePos, CurrentLevel);

			if (CurrentLevel != ECollapseWarningLevel::None)
			{
				SpawnWarningEffects(Tile.TilePos, CurrentLevel);
				OnCollapseWarning.Broadcast(Tile.TilePos, CurrentLevel);
			}
		}

		// Check for collapse
		if (Tile.Instability >= 1.f)
		{
			TriggerCollapse(Tile.TilePos);
			TrackedTiles.RemoveAt(i);
		}
	}
}

// ─── Damage Handling ─────────────────────────────────────────────────────────

float AAoCMineSupport::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	ApplyDamage(ActualDamage);

	return ActualDamage;
}

void AAoCMineSupport::ApplyDamage(float DamageAmount)
{
	Health -= DamageAmount;
	Health = FMath::Max(Health, 0.f);

	if (Health <= 0.f)
	{
		DestroySupport();
	}
}

float AAoCMineSupport::GetHealthPercentage() const
{
	if (MaxHealth <= 0.f)
	{
		return 0.f;
	}
	return Health / MaxHealth;
}

// ─── Placement ───────────────────────────────────────────────────────────────

void AAoCMineSupport::OnPlaced()
{
	// Set coverage radius based on type
	switch (SupportType)
	{
	case ESupportType::Beam:
		CoverageRadius = 0; // Covers only its own tile
		MaxHealth = 100.f;
		break;

	case ESupportType::Column:
		CoverageRadius = 1; // Covers 3×3 tiles
		MaxHealth = 200.f;
		break;
	}

	Health = MaxHealth;

	// Calculate tile position from world position
	const FVector WorldPos = GetActorLocation();
	TilePosition = AoCVoxelUtils::WorldToLocalVoxel(WorldPos);

	// Set the correct mesh
	SetupMesh();

	// Mark all covered tiles as supported
	const TArray<FIntVector> CoveredTiles = GetSupportedTiles();
	for (const FIntVector& Tile : CoveredTiles)
	{
		MarkTileSupported(Tile);
	}

	// Fire delegate
	OnSupportPlaced.Broadcast(this);
}

void AAoCMineSupport::SetupMesh()
{
	if (!SupportMesh)
	{
		return;
	}

	switch (SupportType)
	{
	case ESupportType::Beam:
		if (BeamMesh)
		{
			SupportMesh->SetStaticMesh(BeamMesh);
		}
		break;

	case ESupportType::Column:
		if (ColumnMesh)
		{
			SupportMesh->SetStaticMesh(ColumnMesh);
		}
		break;
	}
}

TArray<FIntVector> AAoCMineSupport::GetSupportedTiles() const
{
	TArray<FIntVector> Tiles;

	// Generate all tile positions in a square of (2*CoverageRadius+1)^2
	for (int32 dx = -CoverageRadius; dx <= CoverageRadius; ++dx)
	{
		for (int32 dy = -CoverageRadius; dy <= CoverageRadius; ++dy)
		{
			Tiles.Add(FIntVector(
				TilePosition.X + dx,
				TilePosition.Y + dy,
				TilePosition.Z
			));
		}
	}

	return Tiles;
}

// ─── Support Destruction ─────────────────────────────────────────────────────

void AAoCMineSupport::DestroySupport()
{
	// Unmark all covered tiles as supported → they begin accumulating instability
	const TArray<FIntVector> CoveredTiles = GetSupportedTiles();
	for (const FIntVector& Tile : CoveredTiles)
	{
		MarkTileUnsupported(Tile);
	}

	// Fire delegate before destroying
	OnSupportDestroyed.Broadcast(this);

	// Remove visual
	if (SupportMesh)
	{
		SupportMesh->SetVisibility(false);
		SupportMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Deferred destroy to allow delegates to process
	SetLifeSpan(0.1f);
}

// ─── Collapse System ─────────────────────────────────────────────────────────

void AAoCMineSupport::RegisterTileForTracking(FIntVector TilePos)
{
	// Check if already tracked
	for (const FTileInstability& Existing : TrackedTiles)
	{
		if (Existing.TilePos == TilePos)
		{
			return; // Already tracked
		}
	}

	FTileInstability NewTile;
	NewTile.TilePos = TilePos;
	NewTile.Instability = 0.f;
	NewTile.bIsSupported = false;
	NewTile.UnsupportedTime = 0.f;

	TrackedTiles.Add(NewTile);
}

void AAoCMineSupport::MarkTileSupported(FIntVector TilePos)
{
	// Find or create tracking entry
	bool bFound = false;
	for (FTileInstability& Tile : TrackedTiles)
	{
		if (Tile.TilePos == TilePos)
		{
			Tile.bIsSupported = true;
			Tile.UnsupportedTime = 0.f;
			Tile.Instability = 0.f;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		FTileInstability NewTile;
		NewTile.TilePos = TilePos;
		NewTile.Instability = 0.f;
		NewTile.bIsSupported = true;
		NewTile.UnsupportedTime = 0.f;
		TrackedTiles.Add(NewTile);
	}

	// Clear any warning level
	LastWarningLevels.Remove(TilePos);
}

void AAoCMineSupport::MarkTileUnsupported(FIntVector TilePos)
{
	for (FTileInstability& Tile : TrackedTiles)
	{
		if (Tile.TilePos == TilePos)
		{
			// Only mark unsupported if no other support covers this tile
			const UWorld* World = GetWorld();
			if (World && CheckTileHasSupport(World, TilePos))
			{
				return; // Another support covers this tile
			}

			Tile.bIsSupported = false;
			// Don't reset UnsupportedTime — it may have been accumulating
			break;
		}
	}
}

void AAoCMineSupport::TriggerCollapse(FIntVector TilePos)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Convert tile position to world space
	const FIntVector ChunkCoord = AoCVoxelUtils::WorldToChunkCoord(
		FVector(TilePos.X * VOXEL_SIZE, TilePos.Y * VOXEL_SIZE, TilePos.Z * VOXEL_SIZE)
	);
	const FVector CollapseCenter = AoCVoxelUtils::ChunkLocalToWorld(ChunkCoord, TilePos.X, TilePos.Y, TilePos.Z);

	// Play collapse sound
	if (CollapseSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, CollapseSound, CollapseCenter, 1.f, 1.f, 0.f);
	}

	// Apply INSTANT DEATH damage to all actors in the collapse zone
	TArray<AActor*> OverlappingActors;
	TArray<FHitResult> HitResults;

	// Use a sphere overlap to find all actors in the damage zone
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(CollapseDamageRadius);
	const bool bHit = World->SweepMultiByChannel(
		HitResults,
		CollapseCenter,
		CollapseCenter + FVector(0.f, 0.f, 1.f), // Tiny sweep
		FQuat::Identity,
		ECollisionChannel::ECC_Pawn,
		CollisionShape
	);

	if (bHit)
	{
		TSet<AActor*> DamagedActors;
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && !DamagedActors.Contains(HitActor))
			{
				DamagedActors.Add(HitActor);

				// Apply lethal damage
				FDamageEvent DamageEvent;
				HitActor->TakeDamage(CollapseDamage, DamageEvent, nullptr, this);
			}
		}
	}

	// Also fill the collapsed area with rock (seal the tunnel)
	if (VoxelWorld)
	{
		const FVoxelData RockData(EVoxelMaterial::Rock, 1.f, 0);
		const FVector TileWorldPos(TilePos.X * VOXEL_SIZE, TilePos.Y * VOXEL_SIZE, TilePos.Z * VOXEL_SIZE);
		VoxelWorld->SetVoxelAt(TileWorldPos, RockData);
	}

	// Fire collapse delegate
	OnCollapse.Broadcast(TilePos);

	// Clean up warning state
	LastWarningLevels.Remove(TilePos);
}

void AAoCMineSupport::SpawnWarningEffects(FIntVector TilePos, ECollapseWarningLevel Level)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Convert tile to world position for effect placement
	const FIntVector ChunkCoord = AoCVoxelUtils::WorldToChunkCoord(
		FVector(TilePos.X * VOXEL_SIZE, TilePos.Y * VOXEL_SIZE, TilePos.Z * VOXEL_SIZE)
	);
	const FVector EffectLocation = AoCVoxelUtils::ChunkLocalToWorld(ChunkCoord, TilePos.X, TilePos.Y, TilePos.Z);

	switch (Level)
	{
	case ECollapseWarningLevel::Low:
		// Dust particles from ceiling
		if (DustParticleEffect)
		{
			ActiveDustEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, DustParticleEffect, EffectLocation + FVector(0.f, 0.f, 125.f), // Ceiling height
				FRotator::ZeroRotator, FVector(0.5f), true, true
			);
		}
		break;

	case ECollapseWarningLevel::Medium:
		// Creaking sounds begin
		if (CreakingSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, CreakingSound, EffectLocation, 0.5f, 1.f, 0.f);
		}
		break;

	case ECollapseWarningLevel::High:
		// Cracking sounds + heavier dust
		if (CrackingSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, CrackingSound, EffectLocation, 0.8f, 1.f, 0.f);
		}
		if (DustParticleEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, DustParticleEffect, EffectLocation + FVector(0.f, 0.f, 125.f),
				FRotator::ZeroRotator, FVector(1.f), true, true
			);
		}
		break;

	case ECollapseWarningLevel::Critical:
		// Loud cracking + maximum dust — imminent collapse
		if (CrackingSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, CrackingSound, EffectLocation, 1.f, 0.8f, 0.f);
		}
		if (DustParticleEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, DustParticleEffect, EffectLocation + FVector(0.f, 0.f, 125.f),
				FRotator::ZeroRotator, FVector(2.f), true, true
			);
		}
		break;

	default:
		break;
	}
}

// ─── Static Helpers ──────────────────────────────────────────────────────────

FSupportBuildRequirements AAoCMineSupport::GetBuildRequirements(ESupportType Type)
{
	FSupportBuildRequirements Req;

	switch (Type)
	{
	case ESupportType::Beam:
		// Beam: 2 boards
		Req.BoardsRequired = 2;
		Req.HardwoodBilletsRequired = 0;
		break;

	case ESupportType::Column:
		// Column: 4 boards + 2 hardwood billets
		Req.BoardsRequired = 4;
		Req.HardwoodBilletsRequired = 2;
		break;
	}

	return Req;
}

bool AAoCMineSupport::CheckTileHasSupport(const UWorld* World, FIntVector TilePos)
{
	if (!World)
	{
		return false;
	}

	// Find all mine support actors in the world
	TArray<AActor*> SupportActors;
	UGameplayStatics::GetAllActorsOfClass(World, AAoCMineSupport::StaticClass(), SupportActors);

	for (AActor* Actor : SupportActors)
	{
		AAoCMineSupport* Support = Cast<AAoCMineSupport>(Actor);
		if (!Support || Support->Health <= 0.f)
		{
			continue;
		}

		// Check if this support covers the given tile
		const TArray<FIntVector> CoveredTiles = Support->GetSupportedTiles();
		for (const FIntVector& Covered : CoveredTiles)
		{
			if (Covered == TilePos)
			{
				return true;
			}
		}
	}

	return false;
}

ECollapseWarningLevel AAoCMineSupport::GetWarningLevel(float Instability)
{
	// Warning thresholds:
	// 0.0 - 0.25: None
	// 0.25 - 0.50: Low (dust particles)
	// 0.50 - 0.75: Medium (creaking sounds)
	// 0.75 - 0.90: High (cracking + heavy dust)
	// 0.90 - 1.0:  Critical (imminent collapse)

	if (Instability >= 0.90f)
	{
		return ECollapseWarningLevel::Critical;
	}
	if (Instability >= 0.75f)
	{
		return ECollapseWarningLevel::High;
	}
	if (Instability >= 0.50f)
	{
		return ECollapseWarningLevel::Medium;
	}
	if (Instability >= 0.25f)
	{
		return ECollapseWarningLevel::Low;
	}
	return ECollapseWarningLevel::None;
}

// ─── Utility ─────────────────────────────────────────────────────────────────

void AAoCMineSupport::FindVoxelWorld()
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
