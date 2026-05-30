// TerraForgeSubsystem.cpp
// TerraForge — World subsystem implementation.
// Manages chunks, streaming, landscape integration, and mesh generation.

#include "TerraForgeSubsystem.h"
#include "TerraForgeStructural.h"
#include "TerraForgeOreGen.h"
#include "TerraForgeSaveLoad.h"
#include "TerraForgeDualContour.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "Components/ActorComponent.h"
#include "NavigationSystem.h"
#include "Async/Async.h"
#include "ProceduralMeshComponent.h"

// ============================================================================
// LIFECYCLE
// ============================================================================

void UTerraForgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Subsystem initializing..."));

	InitializeDataTables();
	InitializePool();

	// Create sub-systems
	StructuralSystem = NewObject<UTerraForgeStructural>(this);
	if (StructuralSystem)
	{
		StructuralSystem->Initialize(this);
	}

	OreGenSystem = NewObject<UTerraForgeOreGen>(this);
	if (OreGenSystem)
	{
		OreGenSystem->Initialize(this, FMath::Rand());
	}

	SaveSystem = NewObject<UTerraForgeSaveLoad>(this);
	if (SaveSystem)
	{
		const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TerraForge"));
		SaveSystem->Initialize(SaveDir, this);
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Subsystem initialized. %d material types, %d ore rules, %d biome profiles."),
		MaterialTable.Num(), OreRules.Num(), BiomeProfiles.Num());
}

void UTerraForgeSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("TerraForge: Subsystem deinitializing. %d chunks active."), ChunkMap.Num());

	// Clean up chunks
	ChunkMap.Empty();
	ChunkMeshMap.Empty();
	LoadedChunkKeys.Empty();
	DirtyChunkQueue.Empty();

	// Pool components are owned by the pool actor — destroyed with it
	MeshPool.Empty();
	FreeMeshPool.Empty();

	Super::Deinitialize();
}

void UTerraForgeSubsystem::InitializeDataTables()
{
	MaterialTable = TerraForgeData::BuildMaterialTable();
	OreRules      = TerraForgeData::BuildOreRules();
	BiomeProfiles = TerraForgeData::BuildBiomeProfiles();
}

void UTerraForgeSubsystem::InitializePool()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Create the pool actor to hold all ProceduralMeshComponents
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MeshPoolActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (MeshPoolActor)
	{
		MeshPoolActor->SetActorHiddenInGame(false);
#if WITH_EDITOR
		MeshPoolActor->SetActorLabel(TEXT("TerraForge Mesh Pool"));
#endif

		// Pre-allocate mesh components
		const int32 InitialPoolSize = 64;
		MeshPool.Reserve(InitialPoolSize);
		FreeMeshPool.Reserve(InitialPoolSize);

		for (int32 I = 0; I < InitialPoolSize; ++I)
		{
			UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(MeshPoolActor);
			Comp->RegisterComponent();
			Comp->SetVisibility(false);
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Comp->SetCollisionResponseToAllChannels(ECR_Block);
			Comp->bUseComplexAsSimpleCollision = true;
			MeshPool.Add(Comp);
			FreeMeshPool.Add(Comp);
		}

		UE_LOG(LogTemp, Log, TEXT("TerraForge: Mesh pool initialized with %d components."), InitialPoolSize);
	}
}

// ============================================================================
// PER-FRAME UPDATE
// ============================================================================

void UTerraForgeSubsystem::TickSubsystem(float DeltaTime)
{
	// Get player position for streaming
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FVector PlayerPos = Pawn->GetActorLocation();

	// Core per-frame systems
	UpdateStreaming(PlayerPos);
	ProcessDirtyChunks();
	UpdateRegeneration(DeltaTime);
}

// ============================================================================
// CHUNK ACCESS
// ============================================================================

FTerraForgeChunk* UTerraForgeSubsystem::GetOrCreateChunk(const FVector& WorldPos)
{
	const FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);

	TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
	if (Found)
	{
		return Found->Get();
	}

	// Create new chunk
	if (ChunkMap.Num() >= TF_MAX_LOADED_CHUNKS)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: Max chunk limit (%d) reached! Cannot create chunk at %s."),
			TF_MAX_LOADED_CHUNKS, *Key.ToString());
		return nullptr;
	}

	TUniquePtr<FTerraForgeChunk> NewChunk = MakeUnique<FTerraForgeChunk>(Key);
	FTerraForgeChunk* ChunkPtr = NewChunk.Get();

	// Initialize from landscape
	InitializeChunkFromLandscape(*ChunkPtr);

	// Place ore veins based on geological rules
	PlaceOreVeinsInChunk(*ChunkPtr);

	ChunkMap.Add(Key, MoveTemp(NewChunk));

	// Update neighbor links
	UpdateNeighborLinks(Key);

	// Add to dirty queue for mesh generation
	DirtyChunkQueue.AddUnique(Key);
	LoadedChunkKeys.Add(Key);

	UE_LOG(LogTemp, Verbose, TEXT("TerraForge: Created chunk %s (total: %d)"), *Key.ToString(), ChunkMap.Num());

	return ChunkPtr;
}

FTerraForgeChunk* UTerraForgeSubsystem::GetChunk(const FTerraChunkKey& Key) const
{
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
	return Found ? Found->Get() : nullptr;
}

bool UTerraForgeSubsystem::HasChunk(const FTerraChunkKey& Key) const
{
	return ChunkMap.Contains(Key);
}

// ============================================================================
// TERRAFORMING ACTIONS
// ============================================================================

EGeoMaterial UTerraForgeSubsystem::LowerTerrain(const FVector& WorldPos, float Amount)
{
	UE_LOG(LogTemp, Warning, TEXT("TerraForge: LowerTerrain at (%.1f, %.1f, %.1f) amount=%.2f"),
		WorldPos.X, WorldPos.Y, WorldPos.Z, Amount);

	FTerraForgeChunk* Chunk = GetOrCreateChunk(WorldPos);
	if (!Chunk)
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: LowerTerrain — failed to get/create chunk!"));
		return EGeoMaterial::Air;
	}

	FIntVector Local = Chunk->WorldToLocal(WorldPos);
	UE_LOG(LogTemp, Warning, TEXT("TerraForge: LowerTerrain — local coords (%d,%d,%d), chunk key (%d,%d,%d)"),
		Local.X, Local.Y, Local.Z, Chunk->GetKey().X, Chunk->GetKey().Y, Chunk->GetKey().Z);

	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: LowerTerrain — local coords out of bounds!"));
		return EGeoMaterial::Air;
	}

	Chunk->Lock();

	// Find the topmost solid voxel in this column near the target Z
	// Start from the target Z and scan downward
	int32 TargetZ = Local.Z;
	bool bFoundSolid = false;
	for (int32 Z = FMath::Min(TargetZ + 2, TF_CHUNK_SIZE - 1); Z >= 0; --Z)
	{
		const FGeoVoxel& V = Chunk->GetVoxel(Local.X, Local.Y, Z);
		if (V.IsSolid())
		{
			TargetZ = Z;
			bFoundSolid = true;
			break;
		}
	}

	if (!bFoundSolid)
	{
		Chunk->Unlock();
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: LowerTerrain — no solid voxel found in column!"));
		return EGeoMaterial::Air;
	}

	// Get the material before we remove it
	FGeoVoxel& Voxel = Chunk->GetVoxelMutable(Local.X, Local.Y, TargetZ);
	const EGeoMaterial DugMaterial = Voxel.GetMaterial();
	const float OldDensity = Voxel.Density;

	// Modify the density to create the surface transition
	const float DigVoxels = Amount / TF_VOXEL_SIZE;
	Voxel.Density += DigVoxels;

	// If density is now positive (above surface), make it air
	if (Voxel.Density > 0.0f)
	{
		Voxel.MakeAir();
	}

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: LowerTerrain — voxel (%d,%d,%d) density %.3f -> %.3f, material=%d, made air=%s"),
		Local.X, Local.Y, TargetZ, OldDensity, Voxel.Density,
		(int32)DugMaterial, Voxel.GetMaterial() == EGeoMaterial::Air ? TEXT("YES") : TEXT("NO"));

	Chunk->MarkDirty();
	Chunk->Unlock();

	// Add to dirty queue
	DirtyChunkQueue.AddUnique(Chunk->GetKey());

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: LowerTerrain — chunk marked dirty, queue size=%d"), DirtyChunkQueue.Num());

	return DugMaterial;
}

bool UTerraForgeSubsystem::RaiseTerrain(const FVector& WorldPos, EGeoMaterial Material, float Amount)
{
	FTerraForgeChunk* Chunk = GetOrCreateChunk(WorldPos);
	if (!Chunk) return false;

	FIntVector Local = Chunk->WorldToLocal(WorldPos);
	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
		return false;

	Chunk->Lock();

	// Find the first air voxel above ground at this column
	int32 TargetZ = -1;
	for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
	{
		const FGeoVoxel& V = Chunk->GetVoxel(Local.X, Local.Y, Z);
		if (V.IsAir())
		{
			// Check if there's solid below
			if (Z == 0 || Chunk->GetVoxel(Local.X, Local.Y, Z - 1).IsSolid())
			{
				TargetZ = Z;
				break;
			}
		}
	}

	if (TargetZ < 0)
	{
		Chunk->Unlock();
		return false;
	}

	// Make the air voxel solid with the given material
	FGeoVoxel& Voxel = Chunk->GetVoxelMutable(Local.X, Local.Y, TargetZ);
	const float RaiseVoxels = Amount / TF_VOXEL_SIZE;
	Voxel.Density -= RaiseVoxels;

	if (Voxel.Density <= 0.0f)
	{
		Voxel.MakeSolid(Material, 50.0f);
	}

	Chunk->MarkDirty();
	Chunk->Unlock();

	DirtyChunkQueue.AddUnique(Chunk->GetKey());
	return true;
}

bool UTerraForgeSubsystem::FlattenTerrain(const FVector& TargetPos, float ReferenceHeight)
{
	FTerraForgeChunk* Chunk = GetOrCreateChunk(TargetPos);
	if (!Chunk) return false;

	FIntVector Local = Chunk->WorldToLocal(TargetPos);
	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
		return false;

	Chunk->Lock();

	// Convert reference height to local Z voxel
	const FVector ChunkOrigin = Chunk->GetWorldOrigin();
	const float RefZ = (ReferenceHeight - ChunkOrigin.Z) / (TF_VOXEL_SIZE * 100.0f);

	// Set all voxels in this column: solid below ref height, air above
	for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
	{
		FGeoVoxel& V = Chunk->GetVoxelMutable(Local.X, Local.Y, Z);
		const float VoxelZ = (float)Z;

		if (VoxelZ < RefZ - 1.0f)
		{
			// Well below reference — ensure solid
			if (V.IsAir())
			{
				V.MakeSolid(EGeoMaterial::Topsoil, 50.0f);
			}
		}
		else if (VoxelZ > RefZ + 1.0f)
		{
			// Well above reference — ensure air
			if (V.IsSolid())
			{
				V.MakeAir();
			}
		}
		else
		{
			// Transition zone — set density based on distance from reference
			V.Density = (VoxelZ - RefZ) * 0.5f;
			if (V.Density <= 0.0f && V.GetMaterial() == EGeoMaterial::Air)
			{
				V.SetMaterial(EGeoMaterial::Topsoil);
			}
			V.Flags |= FGeoVoxel::FLAG_MODIFIED;
		}
	}

	Chunk->MarkDirty();
	Chunk->Unlock();

	DirtyChunkQueue.AddUnique(Chunk->GetKey());
	return true;
}

bool UTerraForgeSubsystem::DigTunnel(const FVector& WorldPos, const FVector& Direction, float Radius)
{
	// Carve a spherical volume at the dig position
	const float RadiusCm = Radius * 100.0f;
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;
	const int32 VoxelRadius = FMath::CeilToInt(Radius / TF_VOXEL_SIZE);

	TSet<FTerraChunkKey> AffectedChunks;

	// Iterate over a cube of voxels centered at the dig point
	for (int32 DX = -VoxelRadius; DX <= VoxelRadius; ++DX)
	{
		for (int32 DY = -VoxelRadius; DY <= VoxelRadius; ++DY)
		{
			for (int32 DZ = -VoxelRadius; DZ <= VoxelRadius; ++DZ)
			{
				const FVector Offset(DX * VoxelCm, DY * VoxelCm, DZ * VoxelCm);
				const float Dist = Offset.Size();

				if (Dist > RadiusCm) continue; // Outside sphere

				const FVector VoxelWorldPos = WorldPos + Offset;
				FTerraForgeChunk* Chunk = nullptr;
				FGeoVoxel* Voxel = GetVoxelAtWorldPos(VoxelWorldPos, &Chunk);

				if (Voxel && Chunk)
				{
					if (Voxel->IsSolid())
					{
						// Smooth falloff at edges
						const float EdgeDist = (RadiusCm - Dist) / VoxelCm;
						Voxel->Density = FMath::Max(Voxel->Density, EdgeDist);
						if (Voxel->Density > 0.0f)
						{
							Voxel->MakeAir();
						}
						Voxel->Flags |= FGeoVoxel::FLAG_MODIFIED;
					}

					AffectedChunks.Add(Chunk->GetKey());
				}
			}
		}
	}

	// Mark all affected chunks dirty
	for (const FTerraChunkKey& Key : AffectedChunks)
	{
		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (Chunk)
		{
			Chunk->MarkDirty();
			DirtyChunkQueue.AddUnique(Key);
		}
	}

	return AffectedChunks.Num() > 0;
}

int32 UTerraForgeSubsystem::MineOre(const FVector& WorldPos, int32 SkillLevel)
{
	FTerraForgeChunk* Chunk = nullptr;
	FGeoVoxel* Voxel = GetVoxelAtWorldPos(WorldPos, &Chunk);

	if (!Voxel || !Chunk) return 0;
	if (!Voxel->IsOre()) return 0;
	if (Voxel->RemainingUnits == 0) return 0;

	// Extraction amount based on skill
	const int32 BaseExtract = FMath::Max(1, SkillLevel / 10);
	const int32 Extracted = FMath::Min((int32)Voxel->RemainingUnits, BaseExtract);

	Voxel->RemainingUnits -= Extracted;
	Voxel->Flags |= FGeoVoxel::FLAG_MODIFIED;

	// If depleted, convert to rock
	if (Voxel->RemainingUnits == 0)
	{
		EGeoMaterial ReplaceMat = EGeoMaterial::Granite; // Default replacement
		Voxel->Density = -1.0f;
		Voxel->SetMaterial(ReplaceMat);
		Voxel->SetOreType(EGeoOreType::None);
		Chunk->MarkDirty();
		DirtyChunkQueue.AddUnique(Chunk->GetKey());
	}

	return Extracted;
}

// ============================================================================
// OBSERVE MODE
// ============================================================================

TArray<FObserveTile> UTerraForgeSubsystem::GetObserveGrid(const FVector& CenterPos) const
{
	TArray<FObserveTile> Grid;
	Grid.Reserve(TF_OBSERVE_GRID_SIZE * TF_OBSERVE_GRID_SIZE);

	const float TileSize = TF_VOXEL_SIZE * 100.0f * 2.0f; // 1m tiles (2 voxels wide)
	const int32 Half = TF_OBSERVE_GRID_SIZE / 2;
	const float CenterElevation = GetElevationAt(CenterPos);

	for (int32 Y = -Half; Y <= Half; ++Y)
	{
		for (int32 X = -Half; X <= Half; ++X)
		{
			FObserveTile Tile;
			Tile.WorldPosition = CenterPos + FVector(X * TileSize, Y * TileSize, 0.0f);
			Tile.Elevation = GetElevationAt(Tile.WorldPosition) / 100.0f; // Convert cm to meters
			Tile.SurfaceMaterial = GetSurfaceMaterialAt(Tile.WorldPosition);
			Tile.HeightDelta = Tile.Elevation - (CenterElevation / 100.0f);

			// Flat if height delta is within ±0.1m
			Tile.bIsFlat = FMath::Abs(Tile.HeightDelta) < 0.1f;

			// Get quality from voxel if available
			FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(Tile.WorldPosition);
			const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
			if (Found)
			{
				FIntVector Local = (*Found)->WorldToLocal(Tile.WorldPosition);
				if (FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
				{
					const FGeoVoxel& V = (*Found)->GetVoxel(Local.X, Local.Y, Local.Z);
					Tile.Quality = V.GetQualityF();
					Tile.Moisture = V.GetMoistureF();
				}
			}

			Grid.Add(Tile);
		}
	}

	return Grid;
}

float UTerraForgeSubsystem::GetElevationAt(const FVector& WorldPos) const
{
	// First check if we have a chunk with data here
	FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);

	if (Found)
	{
		// Scan column for topmost solid voxel
		FIntVector Local = (*Found)->WorldToLocal(WorldPos);
		Local.X = FMath::Clamp(Local.X, 0, TF_CHUNK_SIZE - 1);
		Local.Y = FMath::Clamp(Local.Y, 0, TF_CHUNK_SIZE - 1);

		for (int32 Z = TF_CHUNK_SIZE - 1; Z >= 0; --Z)
		{
			if ((*Found)->GetVoxel(Local.X, Local.Y, Z).IsSolid())
			{
				return (*Found)->LocalToWorld(Local.X, Local.Y, Z).Z;
			}
		}
	}

	// Fall back to landscape height
	return SampleLandscapeHeight(WorldPos.X, WorldPos.Y);
}

EGeoMaterial UTerraForgeSubsystem::GetSurfaceMaterialAt(const FVector& WorldPos) const
{
	FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);

	if (Found)
	{
		FIntVector Local = (*Found)->WorldToLocal(WorldPos);
		Local.X = FMath::Clamp(Local.X, 0, TF_CHUNK_SIZE - 1);
		Local.Y = FMath::Clamp(Local.Y, 0, TF_CHUNK_SIZE - 1);

		for (int32 Z = TF_CHUNK_SIZE - 1; Z >= 0; --Z)
		{
			const FGeoVoxel& V = (*Found)->GetVoxel(Local.X, Local.Y, Z);
			if (V.IsSolid())
			{
				return V.GetMaterial();
			}
		}
	}

	// Default to topsoil if no chunk exists
	return EGeoMaterial::Topsoil;
}

// ============================================================================
// PROSPECTING
// ============================================================================

TArray<FProspectingResult> UTerraForgeSubsystem::Prospect(const FVector& WorldPos, float Radius, int32 SkillLevel) const
{
	TArray<FProspectingResult> Results;

	const float RadiusCm = Radius * 100.0f;
	const FTerraChunkKey CenterKey = FTerraChunkKey::FromWorldPos(WorldPos);
	const int32 ChunkRadius = FMath::CeilToInt(Radius / TF_CHUNK_WORLD_SIZE) + 1;

	// Scan nearby chunks for ore deposits
	for (int32 DX = -ChunkRadius; DX <= ChunkRadius; ++DX)
	{
		for (int32 DY = -ChunkRadius; DY <= ChunkRadius; ++DY)
		{
			for (int32 DZ = -ChunkRadius; DZ <= ChunkRadius; ++DZ)
			{
				FTerraChunkKey Key(CenterKey.X + DX, CenterKey.Y + DY, CenterKey.Z + DZ);
				const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
				if (!Found) continue;

				const FTerraForgeChunk& Chunk = *Found->Get();

				// Scan for ore voxels
				for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
				{
					for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
					{
						for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
						{
							const FGeoVoxel& V = Chunk.GetVoxel(X, Y, Z);
							if (!V.IsOre()) continue;
							if (V.RemainingUnits == 0) continue;

							const FVector OreWorldPos = Chunk.LocalToWorld(X, Y, Z);
							const float Dist = FVector::Dist(WorldPos, OreWorldPos);
							if (Dist > RadiusCm) continue;

							// Check skill requirement
							const EGeoOreType OreType = V.GetOreType();
							int32 MinSkillRequired = 0;
							for (const FGeoOreRule& Rule : OreRules)
							{
								if (Rule.OreType == OreType)
								{
									MinSkillRequired = Rule.MinProspectingSkill;
									break;
								}
							}

							if (SkillLevel < MinSkillRequired) continue;

							// Build result with detail level based on skill
							FProspectingResult PR;
							PR.bFound = true;
							PR.OreType = OreType;
							PR.Distance = Dist / 100.0f; // cm to meters

							// Detail level affects accuracy
							if (SkillLevel >= 90)
							{
								PR.DetailLevel = 3;
								PR.Direction = (OreWorldPos - WorldPos).GetSafeNormal();
								PR.Depth = (WorldPos.Z - OreWorldPos.Z) / 100.0f;
								PR.EstimatedQuality = V.GetQualityF();
								PR.EstimatedUnits = V.RemainingUnits;
							}
							else if (SkillLevel >= 60)
							{
								PR.DetailLevel = 2;
								PR.Direction = (OreWorldPos - WorldPos).GetSafeNormal();
								// Add some noise
								PR.Direction += FVector(FMath::RandRange(-0.2f, 0.2f),
								                        FMath::RandRange(-0.2f, 0.2f), 0.0f);
								PR.Direction.Normalize();
								PR.Depth = (WorldPos.Z - OreWorldPos.Z) / 100.0f + FMath::RandRange(-2.0f, 2.0f);
								PR.EstimatedQuality = V.GetQualityF() + FMath::RandRange(-10.0f, 10.0f);
								PR.EstimatedUnits = V.RemainingUnits + FMath::RandRange(-20, 20);
							}
							else if (SkillLevel >= 30)
							{
								PR.DetailLevel = 1;
								PR.Direction = (OreWorldPos - WorldPos).GetSafeNormal();
								PR.Direction += FVector(FMath::RandRange(-0.5f, 0.5f),
								                        FMath::RandRange(-0.5f, 0.5f), 0.0f);
								PR.Direction.Normalize();
								PR.Depth = (WorldPos.Z - OreWorldPos.Z) / 100.0f + FMath::RandRange(-5.0f, 5.0f);
							}
							else
							{
								PR.DetailLevel = 0;
								// Just "something metallic nearby"
							}

							Results.Add(PR);
						}
					}
				}
			}
		}
	}

	// Sort by distance
	Results.Sort([](const FProspectingResult& A, const FProspectingResult& B)
	{
		return A.Distance < B.Distance;
	});

	return Results;
}

// ============================================================================
// STRUCTURAL INTEGRITY
// ============================================================================

EStructuralState UTerraForgeSubsystem::GetStructuralState(const FVector& WorldPos) const
{
	FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
	if (!Found) return EStructuralState::Stable;

	const FTerraForgeChunk& Chunk = *Found->Get();
	FIntVector Local = Chunk.WorldToLocal(WorldPos);
	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
		return EStructuralState::Stable;

	const float Stress = CalculateStress(Chunk, Local.X, Local.Y, Local.Z);

	if (Stress >= TF_COLLAPSE_THRESHOLD)  return EStructuralState::Collapsed;
	if (Stress >= 0.95f)                   return EStructuralState::Critical;
	if (Stress >= 0.6f)                    return EStructuralState::Warning;
	return EStructuralState::Stable;
}

bool UTerraForgeSubsystem::CanMineAt(const FVector& WorldPos) const
{
	const EStructuralState State = GetStructuralState(WorldPos);
	return State != EStructuralState::Collapsed;
}

bool UTerraForgeSubsystem::PlaceSupport(const FVector& WorldPos)
{
	FTerraForgeChunk* Chunk = nullptr;
	FGeoVoxel* Voxel = GetVoxelAtWorldPos(WorldPos, &Chunk);
	if (!Voxel || !Chunk) return false;

	// Support can only be placed in air adjacent to solid
	if (!Voxel->IsAir()) return false;

	// Mark surrounding voxels as supported
	FIntVector Local = Chunk->WorldToLocal(WorldPos);
	const int32 SupportRadius = 4; // 4 voxels = 2 meters support radius

	for (int32 DX = -SupportRadius; DX <= SupportRadius; ++DX)
	{
		for (int32 DY = -SupportRadius; DY <= SupportRadius; ++DY)
		{
			for (int32 DZ = -SupportRadius; DZ <= SupportRadius; ++DZ)
			{
				const int32 NX = Local.X + DX;
				const int32 NY = Local.Y + DY;
				const int32 NZ = Local.Z + DZ;

				if (FTerraForgeChunk::IsInBounds(NX, NY, NZ))
				{
					FGeoVoxel& NV = Chunk->GetVoxelMutable(NX, NY, NZ);
					NV.Flags |= FGeoVoxel::FLAG_SUPPORTED;
				}
			}
		}
	}

	Chunk->MarkDirty();
	DirtyChunkQueue.AddUnique(Chunk->GetKey());
	return true;
}

float UTerraForgeSubsystem::CalculateStress(const FTerraForgeChunk& Chunk, int32 X, int32 Y, int32 Z) const
{
	// Look upward: count consecutive air voxels above this point
	// Then check horizontal span of air in the ceiling
	const FGeoVoxel& V = Chunk.GetVoxel(X, Y, Z);
	if (V.IsSolid()) return 0.0f; // Solid material has no stress on itself

	// Check if there's a ceiling above
	bool bHasCeiling = false;
	int32 CeilingZ = Z;
	for (int32 CheckZ = Z + 1; CheckZ < TF_CHUNK_SIZE; ++CheckZ)
	{
		if (Chunk.GetVoxel(X, Y, CheckZ).IsSolid())
		{
			bHasCeiling = true;
			CeilingZ = CheckZ;
			break;
		}
	}

	if (!bHasCeiling) return 0.0f; // No ceiling = open sky = no collapse

	// Measure unsupported span (how wide the air gap is at ceiling level)
	int32 Span = 0;
	for (int32 Dir = -1; Dir <= 1; Dir += 2)
	{
		for (int32 D = 1; D <= 20; ++D) // Max scan 20 voxels
		{
			const int32 CX = X + D * Dir;
			if (!FTerraForgeChunk::IsInBounds(CX, Y, CeilingZ)) break;

			if (Chunk.GetVoxel(CX, Y, CeilingZ).IsSolid())
			{
				// Also check if there's a support below
				bool bHasWall = false;
				for (int32 WZ = CeilingZ - 1; WZ >= Z; --WZ)
				{
					if (Chunk.GetVoxel(CX, Y, WZ).IsSolid() ||
					    (Chunk.GetVoxel(CX, Y, WZ).Flags & FGeoVoxel::FLAG_SUPPORTED))
					{
						bHasWall = true;
						break;
					}
				}
				if (bHasWall) break;
			}
			Span++;
		}
	}

	// Get ceiling material strength
	const FGeoVoxel& CeilingVoxel = Chunk.GetVoxel(X, Y, CeilingZ);
	const EGeoMaterial CeilingMat = CeilingVoxel.GetMaterial();
	const FGeoMaterialProperties* Props = MaterialTable.Find(CeilingMat);
	const int32 MaxSpan = Props ? Props->MaxUnsupportedSpan : TF_MAX_UNSUPPORTED_SPAN_DEFAULT;

	// Check if this voxel is supported
	if (V.IsSupported())
	{
		return FMath::Max(0.0f, (float)Span / (float)(MaxSpan * 3) - 0.1f);
	}

	if (MaxSpan <= 0) return 1.0f;
	return FMath::Clamp((float)Span / (float)MaxSpan, 0.0f, 1.0f);
}

// ============================================================================
// LANDSCAPE INTEGRATION
// ============================================================================

ALandscapeProxy* UTerraForgeSubsystem::FindLandscape() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscapeProxy::StaticClass(), Actors);
	return Actors.Num() > 0 ? Cast<ALandscapeProxy>(Actors[0]) : nullptr;
}

float UTerraForgeSubsystem::SampleLandscapeHeight(float WorldX, float WorldY) const
{
	if (!CachedLandscape)
	{
		// Try to find it (const_cast because we're caching for perf)
		const_cast<UTerraForgeSubsystem*>(this)->CachedLandscape = FindLandscape();
	}

	if (CachedLandscape)
	{
		// Use line trace to get precise landscape height
		UWorld* World = GetWorld();
		if (World)
		{
			FHitResult Hit;
			FVector Start(WorldX, WorldY, 100000.0f);
			FVector End(WorldX, WorldY, -100000.0f);
			FCollisionQueryParams Params;
			Params.bTraceComplex = true;

			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
			{
				return Hit.Location.Z;
			}
		}
	}

	return 0.0f; // No landscape found
}

EGeoBiome UTerraForgeSubsystem::GetBiomeAt(const FVector& WorldPos) const
{
	// Simple biome determination based on elevation and position
	// TODO: Replace with proper biome map (texture or data-driven)
	const float Elevation = WorldPos.Z / 100.0f; // cm to meters

	if (Elevation > 80.0f)  return EGeoBiome::Mountains;
	if (Elevation > 50.0f)  return EGeoBiome::Hills;
	if (Elevation < 5.0f)   return EGeoBiome::Coast;
	if (Elevation < 15.0f)  return EGeoBiome::River;

	// Use position hash for some variety
	const int32 RegionX = FMath::FloorToInt(WorldPos.X / 10000.0f); // 100m regions
	const int32 RegionY = FMath::FloorToInt(WorldPos.Y / 10000.0f);
	const uint32 Hash = HashCombine(GetTypeHash(RegionX), GetTypeHash(RegionY));

	switch (Hash % 5)
	{
	case 0: return EGeoBiome::Plains;
	case 1: return EGeoBiome::Forest;
	case 2: return EGeoBiome::Swamp;
	case 3: return EGeoBiome::Desert;
	case 4: return EGeoBiome::Tundra;
	default: return EGeoBiome::Plains;
	}
}

const FGeoBiomeProfile* UTerraForgeSubsystem::GetBiomeProfile(EGeoBiome Biome) const
{
	for (const FGeoBiomeProfile& Profile : BiomeProfiles)
	{
		if (Profile.Biome == Biome) return &Profile;
	}
	return nullptr;
}

// ============================================================================
// INTERNAL OPERATIONS
// ============================================================================

void UTerraForgeSubsystem::InitializeChunkFromLandscape(FTerraForgeChunk& Chunk)
{
	const FVector ChunkOrigin = Chunk.GetWorldOrigin();
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;

	// Determine biome for this chunk
	const EGeoBiome Biome = GetBiomeAt(ChunkOrigin);
	const FGeoBiomeProfile* Profile = GetBiomeProfile(Biome);

	// Default layers if no profile
	TArray<FGeoLayerDef> DefaultLayers;
	if (!Profile)
	{
		FGeoLayerDef Layer;
		Layer.Material = EGeoMaterial::Topsoil;
		Layer.MinDepth = 0.0f;
		Layer.MaxDepth = 2.0f;
		DefaultLayers.Add(Layer);

		Layer.Material = EGeoMaterial::Granite;
		Layer.MinDepth = 2.0f;
		Layer.MaxDepth = 100.0f;
		DefaultLayers.Add(Layer);
	}

	const TArray<FGeoLayerDef>& Layers = Profile ? Profile->Layers : DefaultLayers;
	const float SoilQ = Profile ? Profile->BaseSoilQuality : 50.0f;

	// Sample landscape heights for each XY position
	float SurfaceHeights[TF_CHUNK_SIZE][TF_CHUNK_SIZE];
	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
	{
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
		{
			const float WorldX = ChunkOrigin.X + (X + 0.5f) * VoxelCm;
			const float WorldY = ChunkOrigin.Y + (Y + 0.5f) * VoxelCm;
			const float HeightCm = SampleLandscapeHeight(WorldX, WorldY);

			// Convert to meters for the chunk system
			SurfaceHeights[X][Y] = HeightCm / 100.0f;
		}
	}

	Chunk.InitializeFromLandscape(SurfaceHeights, Layers, SoilQ);
}

void UTerraForgeSubsystem::PlaceOreVeinsInChunk(FTerraForgeChunk& Chunk)
{
	const FVector ChunkOrigin = Chunk.GetWorldOrigin();
	const EGeoBiome Biome = GetBiomeAt(ChunkOrigin);

	for (const FGeoOreRule& Rule : OreRules)
	{
		// Check if this ore can spawn in this biome
		if (Rule.PreferredBiomes.Num() > 0 && !Rule.PreferredBiomes.Contains(Biome))
		{
			continue;
		}

		// Probability of a vein in this chunk
		const float ChunkAreaKmSq = (TF_CHUNK_WORLD_SIZE * TF_CHUNK_WORLD_SIZE) / 1000000.0f;
		const float Probability = Rule.SpawnsPerKmSq * ChunkAreaKmSq;

		// Use chunk key as seed for deterministic placement
		const FTerraChunkKey& Key = Chunk.GetKey();
		const uint32 Seed = HashCombine(
			HashCombine(GetTypeHash(Key.X), GetTypeHash(Key.Y)),
			HashCombine(GetTypeHash(Key.Z), GetTypeHash((int32)Rule.OreType)));

		FRandomStream RNG(Seed);

		if (RNG.FRand() > Probability) continue;

		// Place the vein at a random position within the chunk
		const int32 CenterX = RNG.RandRange(2, TF_CHUNK_SIZE - 3);
		const int32 CenterY = RNG.RandRange(2, TF_CHUNK_SIZE - 3);

		// Z position based on depth rules
		const float SurfaceH = Chunk.OriginalSurfaceHeights[CenterX][CenterY];
		const float ChunkBaseZ = Key.Z * TF_CHUNK_WORLD_SIZE;
		const float MinVeinZ = SurfaceH - Rule.MaxDepth;
		const float MaxVeinZ = SurfaceH - Rule.MinDepth;

		// Convert to local Z
		const int32 MinZ = FMath::Clamp(
			FMath::FloorToInt((MinVeinZ - ChunkBaseZ) / TF_VOXEL_SIZE), 0, TF_CHUNK_SIZE - 1);
		const int32 MaxZ = FMath::Clamp(
			FMath::FloorToInt((MaxVeinZ - ChunkBaseZ) / TF_VOXEL_SIZE), 0, TF_CHUNK_SIZE - 1);

		if (MinZ >= MaxZ) continue;

		const int32 CenterZ = RNG.RandRange(MinZ, MaxZ);

		// Check host rock requirement
		if (Rule.HostRockTypes.Num() > 0)
		{
			const FGeoVoxel& HostVoxel = Chunk.GetVoxel(CenterX, CenterY, CenterZ);
			if (!Rule.HostRockTypes.Contains(HostVoxel.GetMaterial()))
			{
				continue; // Wrong rock type
			}
		}

		// Place the vein
		const int32 Radius = FMath::Max(1, FMath::FloorToInt(FMath::Pow((float)Rule.AverageVeinSize, 1.0f/3.0f)));
		const float Quality = RNG.FRandRange(Rule.QualityRange.X, Rule.QualityRange.Y);

		Chunk.PlaceOreVein(
			FIntVector(CenterX, CenterY, CenterZ),
			Rule.OreType,
			Radius,
			Quality,
			Rule.UnitsPerVoxel);

		UE_LOG(LogTemp, Verbose, TEXT("TerraForge: Placed %s vein at chunk %s local (%d,%d,%d) Q:%.0f R:%d"),
			*UEnum::GetValueAsString(Rule.OreType),
			*Chunk.GetKey().ToString(),
			CenterX, CenterY, CenterZ,
			Quality, Radius);
	}
}

void UTerraForgeSubsystem::UpdateStreaming(const FVector& PlayerPos)
{
	const FTerraChunkKey PlayerChunk = FTerraChunkKey::FromWorldPos(PlayerPos);
	const int32 LoadRadius = TF_RING_LOW; // Load chunks within this radius

	// Determine which chunks should be loaded
	TSet<FTerraChunkKey> DesiredChunks;
	for (int32 DX = -LoadRadius; DX <= LoadRadius; ++DX)
	{
		for (int32 DY = -LoadRadius; DY <= LoadRadius; ++DY)
		{
			for (int32 DZ = -2; DZ <= 2; ++DZ) // Vertical range limited
			{
				FTerraChunkKey Key(PlayerChunk.X + DX, PlayerChunk.Y + DY, PlayerChunk.Z + DZ);
				// Only desire chunks that actually have modifications
				if (ChunkMap.Contains(Key))
				{
					DesiredChunks.Add(Key);
				}
			}
		}
	}

	// Unload chunks that are no longer desired
	TArray<FTerraChunkKey> ToUnload;
	for (const FTerraChunkKey& Key : LoadedChunkKeys)
	{
		if (!DesiredChunks.Contains(Key))
		{
			ToUnload.Add(Key);
		}
	}

	for (const FTerraChunkKey& Key : ToUnload)
	{
		// Release mesh component
		UProceduralMeshComponent** MeshComp = ChunkMeshMap.Find(Key);
		if (MeshComp && *MeshComp)
		{
			ReleaseMeshComponent(*MeshComp);
			ChunkMeshMap.Remove(Key);
		}

		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (Chunk) Chunk->SetHasRenderedMesh(false);

		LoadedChunkKeys.Remove(Key);
	}

	// Load desired chunks that aren't loaded yet (limited per frame)
	int32 LoadedThisFrame = 0;
	for (const FTerraChunkKey& Key : DesiredChunks)
	{
		if (!LoadedChunkKeys.Contains(Key) && LoadedThisFrame < TF_CHUNKS_PER_FRAME)
		{
			FTerraForgeChunk* Chunk = GetChunk(Key);
			if (Chunk && Chunk->HasModifications())
			{
				LoadedChunkKeys.Add(Key);
				DirtyChunkQueue.AddUnique(Key);
				LoadedThisFrame++;
			}
		}
	}
}

void UTerraForgeSubsystem::ProcessDirtyChunks(int32 MaxPerFrame)
{
	if (DirtyChunkQueue.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: ProcessDirtyChunks — %d chunks in queue"), DirtyChunkQueue.Num());
	}

	int32 Processed = 0;
	while (DirtyChunkQueue.Num() > 0 && Processed < MaxPerFrame)
	{
		const FTerraChunkKey Key = DirtyChunkQueue[0];
		DirtyChunkQueue.RemoveAt(0);

		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (!Chunk || !Chunk->IsDirty())
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: ProcessDirtyChunks — skipping chunk (%d,%d,%d): null=%s dirty=%s"),
				Key.X, Key.Y, Key.Z, Chunk ? TEXT("no") : TEXT("YES"), Chunk && Chunk->IsDirty() ? TEXT("yes") : TEXT("NO"));
			continue;
		}
		if (Chunk->IsMeshGenerating())
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: ProcessDirtyChunks — chunk (%d,%d,%d) already generating mesh, skipping"),
				Key.X, Key.Y, Key.Z);
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("TerraForge: ProcessDirtyChunks — regenerating mesh for chunk (%d,%d,%d)"),
			Key.X, Key.Y, Key.Z);
		RegenerateMesh(Key);
		Processed++;
	}
}

void UTerraForgeSubsystem::RegenerateMesh(const FTerraChunkKey& Key)
{
	FTerraForgeChunk* Chunk = GetChunk(Key);
	if (!Chunk) return;

	// Determine LOD based on... for now just full
	// TODO: Calculate LOD from player distance
	const EChunkLOD LOD = EChunkLOD::Full;

	// Create snapshot for thread-safe mesh generation
	Chunk->SetMeshGenerating(true);
	TSharedPtr<FTerraForgeChunk> Snapshot = Chunk->CreateSnapshot();

	// Generate mesh (currently synchronous — TODO: move to async task)
	FTerraChunkMeshData MeshData;
	FTerraForgeDualContour::GenerateMesh(*Snapshot, LOD, MeshData);

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: RegenerateMesh — chunk (%d,%d,%d): valid=%s verts=%d tris=%d"),
		Key.X, Key.Y, Key.Z, MeshData.bValid ? TEXT("YES") : TEXT("NO"),
		MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);

	// Apply mesh to ProceduralMeshComponent
	if (MeshData.bValid && MeshData.Vertices.Num() > 0)
	{
		UProceduralMeshComponent* MeshComp = nullptr;

		// Check if we already have a mesh component for this chunk
		UProceduralMeshComponent** ExistingComp = ChunkMeshMap.Find(Key);
		if (ExistingComp && *ExistingComp)
		{
			MeshComp = *ExistingComp;
		}
		else
		{
			MeshComp = AcquireMeshComponent();
			if (MeshComp)
			{
				ChunkMeshMap.Add(Key, MeshComp);
			}
		}

		if (MeshComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: RegenerateMesh — generating mesh for chunk (%d,%d,%d): %d verts, %d tris"),
				Key.X, Key.Y, Key.Z, MeshData.Vertices.Num(), MeshData.Triangles.Num() / 3);

			// Convert tangent vectors to FProcMeshTangent
			TArray<FProcMeshTangent> ProcTangents;
			ProcTangents.SetNum(MeshData.Vertices.Num());
			for (int32 I = 0; I < ProcTangents.Num(); ++I)
			{
				// Compute tangent from normal
				const FVector& N = MeshData.Normals[I];
				FVector T = FVector::CrossProduct(N, FVector::UpVector);
				if (T.IsNearlyZero()) T = FVector::CrossProduct(N, FVector::RightVector);
				T.Normalize();
				ProcTangents[I] = FProcMeshTangent(T, false);
			}

			MeshComp->ClearAllMeshSections();
			MeshComp->CreateMeshSection(
				0,
				MeshData.Vertices,
				MeshData.Triangles,
				MeshData.Normals,
				MeshData.UV0,
				MeshData.VertexColors,
				ProcTangents,
				true /* collision */);

			MeshComp->SetVisibility(true);
			MeshComp->SetHiddenInGame(false);
			MeshComp->SetCastShadow(true);
			Chunk->SetHasRenderedMesh(true);

			// Hide the landscape at this chunk's location so our mesh is visible
			HideLandscapeForChunk(Key);

			UE_LOG(LogTemp, Warning, TEXT("TerraForge: RegenerateMesh — mesh applied successfully, landscape hidden"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: RegenerateMesh — no MeshComp available (pool actor=%s)"),
				MeshPoolActor ? TEXT("exists") : TEXT("NULL"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: RegenerateMesh — no valid mesh generated (valid=%s verts=%d)"),
			MeshData.bValid ? TEXT("YES") : TEXT("NO"), MeshData.Vertices.Num());
	}

	Chunk->SetMeshGenerating(false);
	Chunk->ClearDirty();
}

UProceduralMeshComponent* UTerraForgeSubsystem::AcquireMeshComponent()
{
	if (FreeMeshPool.Num() > 0)
	{
		UProceduralMeshComponent* Comp = FreeMeshPool.Pop();
		return Comp;
	}

	// Pool exhausted — create a new component
	if (MeshPoolActor)
	{
		UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(MeshPoolActor);
		Comp->RegisterComponent();
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->bUseComplexAsSimpleCollision = true;
		MeshPool.Add(Comp);
		return Comp;
	}

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: Cannot acquire mesh component — no pool actor!"));
	return nullptr;
}

void UTerraForgeSubsystem::ReleaseMeshComponent(UProceduralMeshComponent* Comp)
{
	if (!Comp) return;

	Comp->ClearAllMeshSections();
	Comp->SetVisibility(false);
	FreeMeshPool.Add(Comp);
}

void UTerraForgeSubsystem::UpdateNeighborLinks(const FTerraChunkKey& Key)
{
	FTerraForgeChunk* Center = GetChunk(Key);
	if (!Center) return;

	// +X, -X, +Y, -Y, +Z, -Z
	const FIntVector Offsets[6] = {
		{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
	};
	const int32 OppositeIdx[6] = {1, 0, 3, 2, 5, 4};

	for (int32 I = 0; I < 6; ++I)
	{
		FTerraChunkKey NeighborKey(Key.X + Offsets[I].X, Key.Y + Offsets[I].Y, Key.Z + Offsets[I].Z);
		FTerraForgeChunk* Neighbor = GetChunk(NeighborKey);
		Center->Neighbors[I] = Neighbor;
		if (Neighbor)
		{
			Neighbor->Neighbors[OppositeIdx[I]] = Center;
		}
	}
}

void UTerraForgeSubsystem::UpdateRegeneration(float DeltaTime)
{
	// Only process a few chunks per frame to limit overhead
	static int32 RegenCursor = 0;

	TArray<FTerraChunkKey> AllKeys;
	ChunkMap.GenerateKeyArray(AllKeys);

	if (AllKeys.Num() == 0) return;

	const int32 MaxPerFrame = 5;
	for (int32 I = 0; I < MaxPerFrame && AllKeys.Num() > 0; ++I)
	{
		RegenCursor = RegenCursor % AllKeys.Num();
		const FTerraChunkKey& Key = AllKeys[RegenCursor];

		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (Chunk && Chunk->HasModifications() && !Chunk->bIsClaimed)
		{
			Chunk->TimeSinceLastInteraction += DeltaTime;

			// Check if enough time has passed for regeneration
			const float RegenTimeSeconds = TF_REGEN_DAYS_UNCLAIMED * 86400.0f; // days to seconds
			if (Chunk->TimeSinceLastInteraction >= RegenTimeSeconds)
			{
				// Regenerate: restore to original landscape state
				if (Chunk->bHasOriginalData)
				{
					const EGeoBiome Biome = GetBiomeAt(Chunk->GetWorldOrigin());
					const FGeoBiomeProfile* Profile = GetBiomeProfile(Biome);
					TArray<FGeoLayerDef> Layers;
					float SoilQ = 50.0f;
					if (Profile)
					{
						Layers = Profile->Layers;
						SoilQ = Profile->BaseSoilQuality;
					}

					Chunk->InitializeFromLandscape(Chunk->OriginalSurfaceHeights, Layers, SoilQ);

					// Mark for mesh rebuild or remove entirely
					UProceduralMeshComponent** MeshComp = ChunkMeshMap.Find(Key);
					if (MeshComp && *MeshComp)
					{
						ReleaseMeshComponent(*MeshComp);
						ChunkMeshMap.Remove(Key);
					}

					// Remove the chunk entirely to free memory
					LoadedChunkKeys.Remove(Key);
					ChunkMap.Remove(Key);

					UE_LOG(LogTemp, Log, TEXT("TerraForge: Chunk %s regenerated and removed."), *Key.ToString());
				}
			}
		}

		RegenCursor++;
	}
}

FGeoVoxel* UTerraForgeSubsystem::GetVoxelAtWorldPos(const FVector& WorldPos, FTerraForgeChunk** OutChunk)
{
	FTerraForgeChunk* Chunk = GetOrCreateChunk(WorldPos);
	if (!Chunk) return nullptr;

	if (OutChunk) *OutChunk = Chunk;

	FIntVector Local = Chunk->WorldToLocal(WorldPos);
	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
		return nullptr;

	return &Chunk->GetVoxelMutable(Local.X, Local.Y, Local.Z);
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

void UTerraForgeSubsystem::SaveAllChunks(const FString& SaveSlot)
{
	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("TerraForge") / SaveSlot;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*SaveDir);

	int32 SavedCount = 0;
	for (auto& Pair : ChunkMap)
	{
		const FTerraChunkKey& Key = Pair.Key;
		const FTerraForgeChunk& Chunk = *Pair.Value;

		if (!Chunk.HasModifications()) continue;

		const FString FileName = FString::Printf(TEXT("chunk_%d_%d_%d.tfb"), Key.X, Key.Y, Key.Z);
		const FString FilePath = SaveDir / FileName;

		TArray<uint8> Data;
		FMemoryWriter Writer(Data);

		// Header
		const int32 MagicNumber = 0x54464247; // "TFBG" - TerraForge Binary Geology
		const int32 Version = 1;
		Writer << const_cast<int32&>(MagicNumber);
		Writer << const_cast<int32&>(Version);

		// Chunk key
		Writer << const_cast<int32&>(Key.X);
		Writer << const_cast<int32&>(Key.Y);
		Writer << const_cast<int32&>(Key.Z);

		// Voxel data (only modified voxels — delta encoding)
		int32 ModifiedCount = 0;
		TArray<uint8> VoxelPayload;
		FMemoryWriter VoxelWriter(VoxelPayload);

		for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
		{
			for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
			{
				for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
				{
					const FGeoVoxel& V = Chunk.GetVoxel(X, Y, Z);
					if (V.IsModified())
					{
						ModifiedCount++;
						// Encode position as single uint16 (X*256 + Y*16 + Z)
						uint16 Pos = (uint16)(X * TF_CHUNK_SIZE * TF_CHUNK_SIZE + Y * TF_CHUNK_SIZE + Z);
						VoxelWriter << Pos;
						// Encode voxel data
						float D = V.Density;
						VoxelWriter << D;
						uint8 Mat = V.Material;
						VoxelWriter << Mat;
						uint8 Ore = V.OreType;
						VoxelWriter << Ore;
						uint8 Q = V.Quality;
						VoxelWriter << Q;
						uint8 Moist = V.Moisture;
						VoxelWriter << Moist;
						uint8 Stress = V.StressLoad;
						VoxelWriter << Stress;
						uint8 Fl = V.Flags;
						VoxelWriter << Fl;
						uint16 Units = V.RemainingUnits;
						VoxelWriter << Units;
					}
				}
			}
		}

		Writer << ModifiedCount;
		Writer.Serialize(VoxelPayload.GetData(), VoxelPayload.Num());

		FFileHelper::SaveArrayToFile(Data, *FilePath);
		SavedCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Saved %d chunks to '%s'"), SavedCount, *SaveSlot);
}

void UTerraForgeSubsystem::LoadChunks(const FString& SaveSlot)
{
	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("TerraForge") / SaveSlot;

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SaveDir / TEXT("*.tfb")), true, false);

	int32 LoadedCount = 0;
	for (const FString& FileName : Files)
	{
		const FString FilePath = SaveDir / FileName;

		TArray<uint8> Data;
		if (!FFileHelper::LoadFileToArray(Data, *FilePath)) continue;

		FMemoryReader Reader(Data);

		int32 Magic, Version;
		Reader << Magic;
		Reader << Version;

		if (Magic != 0x54464247 || Version != 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: Invalid save file '%s'"), *FileName);
			continue;
		}

		int32 X, Y, Z;
		Reader << X;
		Reader << Y;
		Reader << Z;

		FTerraChunkKey Key(X, Y, Z);

		// Create or get chunk
		TUniquePtr<FTerraForgeChunk> Chunk = MakeUnique<FTerraForgeChunk>(Key);
		InitializeChunkFromLandscape(*Chunk);

		// Load modified voxels
		int32 ModifiedCount;
		Reader << ModifiedCount;

		for (int32 I = 0; I < ModifiedCount; ++I)
		{
			uint16 Pos;
			Reader << Pos;

			const int32 VX = Pos / (TF_CHUNK_SIZE * TF_CHUNK_SIZE);
			const int32 VY = (Pos / TF_CHUNK_SIZE) % TF_CHUNK_SIZE;
			const int32 VZ = Pos % TF_CHUNK_SIZE;

			if (!FTerraForgeChunk::IsInBounds(VX, VY, VZ)) continue;

			FGeoVoxel& V = Chunk->GetVoxelMutable(VX, VY, VZ);
			float D; Reader << D; V.Density = D;
			uint8 Mat; Reader << Mat; V.Material = Mat;
			uint8 Ore; Reader << Ore; V.OreType = Ore;
			uint8 Q; Reader << Q; V.Quality = Q;
			uint8 Moist; Reader << Moist; V.Moisture = Moist;
			uint8 Stress; Reader << Stress; V.StressLoad = Stress;
			uint8 Fl; Reader << Fl; V.Flags = Fl;
			uint16 Units; Reader << Units; V.RemainingUnits = Units;
		}

		ChunkMap.Add(Key, MoveTemp(Chunk));
		LoadedCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Loaded %d chunks from '%s'"), LoadedCount, *SaveSlot);
}

void UTerraForgeSubsystem::SetVoxelMaterial(const FVector& WorldPos, EGeoMaterial Material)
{
	// Convert world position to chunk + local coordinates
	const FIntVector VoxelCoord(
		FMath::FloorToInt(WorldPos.X / TF_VOXEL_SIZE_CM),
		FMath::FloorToInt(WorldPos.Y / TF_VOXEL_SIZE_CM),
		FMath::FloorToInt(WorldPos.Z / TF_VOXEL_SIZE_CM)
	);

	FTerraChunkKey Key;
	Key.X = FMath::FloorToInt((float)VoxelCoord.X / TF_CHUNK_SIZE);
	Key.Y = FMath::FloorToInt((float)VoxelCoord.Y / TF_CHUNK_SIZE);
	Key.Z = FMath::FloorToInt((float)VoxelCoord.Z / TF_CHUNK_SIZE);

	auto* ChunkPtr = ChunkMap.Find(Key);
	if (!ChunkPtr) return;

	FTerraForgeChunk* Chunk = ChunkPtr->Get();
	if (!Chunk) return;

	const int32 LX = ((VoxelCoord.X % TF_CHUNK_SIZE) + TF_CHUNK_SIZE) % TF_CHUNK_SIZE;
	const int32 LY = ((VoxelCoord.Y % TF_CHUNK_SIZE) + TF_CHUNK_SIZE) % TF_CHUNK_SIZE;
	const int32 LZ = ((VoxelCoord.Z % TF_CHUNK_SIZE) + TF_CHUNK_SIZE) % TF_CHUNK_SIZE;

	FGeoVoxel& V = Chunk->GetVoxelMutable(LX, LY, LZ);
	V.Material = (uint8)Material;
	V.Density = (Material == EGeoMaterial::Air) ? 1.0f : -1.0f;
	Chunk->MarkDirty();

	// Queue mesh regen
	DirtyChunkQueue.AddUnique(Key);
}

void UTerraForgeSubsystem::RegenerateMeshesInRadius(const FVector& Center, float Radius)
{
	const float VoxelSize = TF_VOXEL_SIZE_CM;
	const int32 ChunkRadius = FMath::CeilToInt(Radius / (TF_CHUNK_SIZE * VoxelSize)) + 1;

	const FIntVector CenterChunk(
		FMath::FloorToInt(Center.X / (TF_CHUNK_SIZE * VoxelSize)),
		FMath::FloorToInt(Center.Y / (TF_CHUNK_SIZE * VoxelSize)),
		FMath::FloorToInt(Center.Z / (TF_CHUNK_SIZE * VoxelSize))
	);

	for (int32 X = -ChunkRadius; X <= ChunkRadius; ++X)
	{
		for (int32 Y = -ChunkRadius; Y <= ChunkRadius; ++Y)
		{
			for (int32 Z = -ChunkRadius; Z <= ChunkRadius; ++Z)
			{
				FTerraChunkKey Key;
				Key.X = CenterChunk.X + X;
				Key.Y = CenterChunk.Y + Y;
				Key.Z = CenterChunk.Z + Z;

				if (ChunkMap.Contains(Key))
				{
					DirtyChunkQueue.AddUnique(Key);
				}
			}
		}
	}
}

FTerraForgeChunk* UTerraForgeSubsystem::GetChunkAt(const FIntVector& ChunkCoord) const
{
	FTerraChunkKey Key;
	Key.X = ChunkCoord.X;
	Key.Y = ChunkCoord.Y;
	Key.Z = ChunkCoord.Z;

	const auto* Found = ChunkMap.Find(Key);
	if (Found)
	{
		return Found->Get();
	}
	return nullptr;
}

// ============================================================================
// LANDSCAPE HIDING
// ============================================================================

void UTerraForgeSubsystem::HideLandscapeForChunk(const FTerraChunkKey& Key)
{
	if (!CachedLandscape)
	{
		CachedLandscape = FindLandscape();
	}
	if (!CachedLandscape)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: HideLandscapeForChunk — no landscape found!"));
		return;
	}

	// Get chunk world bounds (2D, XY plane)
	const FVector ChunkOrigin = Key.ToWorldPos(); // in cm
	const float ChunkSizeCm = TF_CHUNK_WORLD_SIZE * 100.0f;
	const FVector ChunkEnd = ChunkOrigin + FVector(ChunkSizeCm, ChunkSizeCm, ChunkSizeCm);

	// Get landscape transform
	const FVector LandscapePos = CachedLandscape->GetActorLocation();
	const FVector LandscapeScale = CachedLandscape->GetActorScale3D();

	// Iterate through landscape components and hide those overlapping with this chunk
	TArray<ULandscapeComponent*> Comps;
	CachedLandscape->GetComponents<ULandscapeComponent>(Comps);

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: HideLandscapeForChunk — checking %d landscape components, chunk origin=(%.0f,%.0f)"),
		Comps.Num(), ChunkOrigin.X, ChunkOrigin.Y);

	for (ULandscapeComponent* LC : Comps)
	{
		if (!LC || HiddenLandscapeComponents.Contains(LC))
		{
			continue;
		}

		// Get component's world bounds
		FBoxSphereBounds CompBounds = LC->CalcBounds(CachedLandscape->GetActorTransform());
		FBox CompBox = CompBounds.GetBox();

		// Check 2D overlap (XY plane)
		bool bOverlaps =
			CompBox.Max.X >= ChunkOrigin.X && CompBox.Min.X <= ChunkEnd.X &&
			CompBox.Max.Y >= ChunkOrigin.Y && CompBox.Min.Y <= ChunkEnd.Y;

		if (bOverlaps)
		{
			LC->SetVisibility(false, true);
			LC->SetHiddenInGame(true);
			HiddenLandscapeComponents.Add(LC);

			UE_LOG(LogTemp, Warning,
				TEXT("TerraForge: Hidden landscape component '%s' at bounds (%.0f,%.0f)-(%.0f,%.0f)"),
				*LC->GetName(),
				CompBox.Min.X, CompBox.Min.Y,
				CompBox.Max.X, CompBox.Max.Y);
		}
	}
}
