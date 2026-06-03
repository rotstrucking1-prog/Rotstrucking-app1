// AoCVoxelWorld.cpp
// Architect of Creation - Voxel World Manager Implementation

#include "AoCVoxelWorld.h"
#include "AoCVoxelChunk.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AAoCVoxelWorld::AAoCVoxelWorld()
{
	PrimaryActorTick.bCanEverTick = true;

	RenderDistance = MAX_RENDER_DISTANCE;
	MaxChunkSpawnsPerFrame = 2;
	MaxPoolSize = 50;
	ChunkUpdateInterval = 0.5f;
	WorldSeed = 42;

	ChunkClass = AAoCVoxelChunk::StaticClass();
	WorldMaterial = nullptr;

	LastPlayerChunkCoord = FIntVector(INT32_MAX, INT32_MAX, INT32_MAX);
	ChunkUpdateTimer = 0.f;
}

void AAoCVoxelWorld::BeginPlay()
{
	Super::BeginPlay();

	// Initial chunk load around origin
	UpdateChunks(FVector::ZeroVector);
}

void AAoCVoxelWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Throttled chunk update
	ChunkUpdateTimer += DeltaTime;
	if (ChunkUpdateTimer < ChunkUpdateInterval)
	{
		return;
	}
	ChunkUpdateTimer = 0.f;

	// Find the player location
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		UpdateChunks(PlayerPawn->GetActorLocation());
	}

	// Process pending chunk loads (throttled per frame)
	int32 SpawnedThisFrame = 0;
	while (PendingChunkLoads.Num() > 0 && SpawnedThisFrame < MaxChunkSpawnsPerFrame)
	{
		const FIntVector Coord = PendingChunkLoads.Pop(EAllowShrinking::No);

		// Skip if already loaded (may have been loaded by ForceLoadChunk)
		if (LoadedChunks.Contains(Coord))
		{
			continue;
		}

		AAoCVoxelChunk* Chunk = AcquireChunk();
		if (Chunk)
		{
			// Assign material
			Chunk->ChunkMaterial = WorldMaterial;

			// Generate ore veins for this chunk region if not already done
			GenerateOreVeins(Coord);

			// Initialize the chunk (fills with terrain data)
			Chunk->InitializeChunk(Coord);

			// Apply any ore veins that overlap this chunk
			ApplyVeinsToChunk(Chunk);

			// Set LOD based on player distance
			if (LastPlayerChunkCoord.X != INT32_MAX)
			{
				Chunk->SetLODLevel(CalculateLOD(Coord, LastPlayerChunkCoord));
			}

			LoadedChunks.Add(Coord, Chunk);
			SpawnedThisFrame++;
		}
	}
}

// ─── Chunk Management ────────────────────────────────────────────────────────

void AAoCVoxelWorld::UpdateChunks(FVector PlayerLocation)
{
	const FIntVector PlayerChunkCoord = AoCVoxelUtils::WorldToChunkCoord(PlayerLocation);

	// Only recalculate if player moved to a different chunk
	if (PlayerChunkCoord == LastPlayerChunkCoord)
	{
		// Still update LODs for existing chunks
		return;
	}
	LastPlayerChunkCoord = PlayerChunkCoord;

	// Determine which chunks should be loaded
	TSet<FIntVector> DesiredChunks;
	for (int32 Z = PlayerChunkCoord.Z - RenderDistance; Z <= PlayerChunkCoord.Z + RenderDistance; ++Z)
	{
		for (int32 Y = PlayerChunkCoord.Y - RenderDistance; Y <= PlayerChunkCoord.Y + RenderDistance; ++Y)
		{
			for (int32 X = PlayerChunkCoord.X - RenderDistance; X <= PlayerChunkCoord.X + RenderDistance; ++X)
			{
				const FIntVector Coord(X, Y, Z);
				const int32 Dist = FMath::Max3(
					FMath::Abs(X - PlayerChunkCoord.X),
					FMath::Abs(Y - PlayerChunkCoord.Y),
					FMath::Abs(Z - PlayerChunkCoord.Z)
				);

				if (Dist <= RenderDistance)
				{
					DesiredChunks.Add(Coord);
				}
			}
		}
	}

	// Unload chunks that are no longer in range
	TArray<FIntVector> ChunksToUnload;
	for (auto& Pair : LoadedChunks)
	{
		if (!DesiredChunks.Contains(Pair.Key))
		{
			ChunksToUnload.Add(Pair.Key);
		}
	}
	for (const FIntVector& Coord : ChunksToUnload)
	{
		AAoCVoxelChunk* Chunk = LoadedChunks.FindAndRemoveChecked(Coord);
		ReleaseChunk(Chunk);
	}

	// Update LOD for existing chunks
	for (auto& Pair : LoadedChunks)
	{
		const int32 NewLOD = CalculateLOD(Pair.Key, PlayerChunkCoord);
		Pair.Value->SetLODLevel(NewLOD);
	}

	// Queue new chunks for loading (sorted by distance — closest first)
	PendingChunkLoads.Reset();
	for (const FIntVector& Coord : DesiredChunks)
	{
		if (!LoadedChunks.Contains(Coord))
		{
			PendingChunkLoads.Add(Coord);
		}
	}

	// Sort pending loads by distance to player (closest first)
	PendingChunkLoads.Sort([PlayerChunkCoord](const FIntVector& A, const FIntVector& B)
	{
		const int32 DistA = FMath::Abs(A.X - PlayerChunkCoord.X) + FMath::Abs(A.Y - PlayerChunkCoord.Y) + FMath::Abs(A.Z - PlayerChunkCoord.Z);
		const int32 DistB = FMath::Abs(B.X - PlayerChunkCoord.X) + FMath::Abs(B.Y - PlayerChunkCoord.Y) + FMath::Abs(B.Z - PlayerChunkCoord.Z);
		return DistA < DistB;
	});
}

AAoCVoxelChunk* AAoCVoxelWorld::GetChunkAt(FIntVector ChunkCoord) const
{
	const AAoCVoxelChunk* const* Found = LoadedChunks.Find(ChunkCoord);
	return Found ? const_cast<AAoCVoxelChunk*>(*Found) : nullptr;
}

AAoCVoxelChunk* AAoCVoxelWorld::ForceLoadChunk(FIntVector ChunkCoord)
{
	// Return existing chunk if already loaded
	if (AAoCVoxelChunk* Existing = GetChunkAt(ChunkCoord))
	{
		return Existing;
	}

	AAoCVoxelChunk* Chunk = AcquireChunk();
	if (Chunk)
	{
		Chunk->ChunkMaterial = WorldMaterial;
		GenerateOreVeins(ChunkCoord);
		Chunk->InitializeChunk(ChunkCoord);
		ApplyVeinsToChunk(Chunk);

		if (LastPlayerChunkCoord.X != INT32_MAX)
		{
			Chunk->SetLODLevel(CalculateLOD(ChunkCoord, LastPlayerChunkCoord));
		}

		LoadedChunks.Add(ChunkCoord, Chunk);
	}
	return Chunk;
}

int32 AAoCVoxelWorld::GetLoadedChunkCount() const
{
	return LoadedChunks.Num();
}

int32 AAoCVoxelWorld::GetPooledChunkCount() const
{
	return ChunkPool.Num();
}

// ─── Voxel Access ────────────────────────────────────────────────────────────

FVoxelData AAoCVoxelWorld::GetVoxelAt(FVector WorldPos) const
{
	const FIntVector ChunkCoord = AoCVoxelUtils::WorldToChunkCoord(WorldPos);
	const AAoCVoxelChunk* Chunk = GetChunkAt(ChunkCoord);
	if (!Chunk)
	{
		return FVoxelData();
	}

	const FIntVector LocalVoxel = AoCVoxelUtils::WorldToLocalVoxel(WorldPos);
	return Chunk->GetVoxel(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z);
}

void AAoCVoxelWorld::SetVoxelAt(FVector WorldPos, FVoxelData Data)
{
	const FIntVector ChunkCoord = AoCVoxelUtils::WorldToChunkCoord(WorldPos);
	AAoCVoxelChunk* Chunk = GetChunkAt(ChunkCoord);
	if (!Chunk)
	{
		return;
	}

	const FIntVector LocalVoxel = AoCVoxelUtils::WorldToLocalVoxel(WorldPos);
	Chunk->SetVoxel(LocalVoxel.X, LocalVoxel.Y, LocalVoxel.Z, Data);
}

void AAoCVoxelWorld::CarveAt(FVector WorldPos, float Radius)
{
	if (Radius <= 0.f)
	{
		return;
	}

	// Carving may affect multiple chunks — find all chunks overlapped by the sphere
	const float RadiusInChunks = (Radius / CHUNK_WORLD_SIZE) + 1.f;
	const FIntVector CenterChunk = AoCVoxelUtils::WorldToChunkCoord(WorldPos);
	const int32 SearchRadius = FMath::CeilToInt(RadiusInChunks);

	for (int32 Z = CenterChunk.Z - SearchRadius; Z <= CenterChunk.Z + SearchRadius; ++Z)
	{
		for (int32 Y = CenterChunk.Y - SearchRadius; Y <= CenterChunk.Y + SearchRadius; ++Y)
		{
			for (int32 X = CenterChunk.X - SearchRadius; X <= CenterChunk.X + SearchRadius; ++X)
			{
				AAoCVoxelChunk* Chunk = GetChunkAt(FIntVector(X, Y, Z));
				if (Chunk)
				{
					Chunk->CarveVoxel(WorldPos, Radius);
				}
			}
		}
	}
}

// ─── Ore Vein Generation ─────────────────────────────────────────────────────

void AAoCVoxelWorld::GenerateOreVeins(FIntVector ChunkCoord)
{
	// Only generate veins once per chunk region
	if (VeinGeneratedChunks.Contains(ChunkCoord))
	{
		return;
	}
	VeinGeneratedChunks.Add(ChunkCoord);

	FRandomStream ChunkRandom = GetChunkRandom(ChunkCoord);

	// Ore vein generation parameters from design doc
	// Each tier has different depth ranges, sizes, and rarities

	struct FOreGenParams
	{
		EVoxelMaterial Material;
		float MinDepthCm;     // Minimum depth below surface in cm
		float MaxDepthCm;     // Maximum depth below surface in cm
		int32 MinOre;         // Minimum remaining ore
		int32 MaxOre;         // Maximum remaining ore
		float MinRadius;      // Vein radius in cm
		float MaxRadius;      // Vein radius in cm
		float SpawnChance;    // Chance per chunk (0-1)
		uint8 MinQuality;     // Min ore quality
		uint8 MaxQuality;     // Max ore quality
	};

	// T1 ores: ~1 vein per 50×50, large, common
	// T2 ores: ~1 vein per 75×75, large, common
	// T3 ores: ~1 vein per 150×150, medium, uncommon
	// T4 ores: ~1 vein per 300×300, medium, rare
	// T5 ores: ~1 vein per 500×500, small, very rare
	// T6 ores: ~1 vein per 1000×1000, tiny, extremely rare
	//
	// Converting to per-chunk probability (chunk=16m, so 50m ≈ 3 chunks):
	// T1: 1/(3×3) ≈ 0.11 → 0.12
	// T2: 1/(5×5) ≈ 0.04 → 0.05
	// T3: 1/(9×9) ≈ 0.012 → 0.015
	// T4: 1/(19×19) ≈ 0.003
	// T5: 1/(31×31) ≈ 0.001
	// T6: 1/(63×63) ≈ 0.00025

	static const FOreGenParams OreParams[] =
	{
		// Tier 1 — Common
		{ EVoxelMaterial::OreCopper,    300.f,  1200.f,  200, 500,  200.f, 400.f,  0.12f,   20, 60 },
		{ EVoxelMaterial::OreTin,       300.f,  1200.f,  200, 500,  200.f, 400.f,  0.12f,   20, 60 },

		// Tier 2 — Common
		{ EVoxelMaterial::OreIron,      600.f,  1800.f,  150, 400,  180.f, 350.f,  0.05f,   30, 70 },
		{ EVoxelMaterial::OreZinc,      600.f,  1800.f,  150, 400,  180.f, 350.f,  0.05f,   30, 70 },
		{ EVoxelMaterial::OreLead,      600.f,  1800.f,  150, 400,  180.f, 350.f,  0.05f,   30, 70 },

		// Tier 3 — Uncommon
		{ EVoxelMaterial::OreNickel,   1000.f,  2600.f,   80, 200,  120.f, 250.f,  0.015f,  40, 80 },
		{ EVoxelMaterial::OreSilver,   1000.f,  2600.f,   80, 200,  120.f, 250.f,  0.015f,  40, 80 },
		{ EVoxelMaterial::OreGold,     1000.f,  2600.f,   80, 200,  100.f, 200.f,  0.015f,  40, 80 },

		// Tier 4 — Rare
		{ EVoxelMaterial::OreChromium,  1500.f,  3200.f,   50, 150,  100.f, 200.f,  0.003f,  50, 90 },
		{ EVoxelMaterial::OreCobalt,    1500.f,  3200.f,   50, 150,  100.f, 200.f,  0.003f,  50, 90 },
		{ EVoxelMaterial::OreManganese, 1500.f,  3200.f,   50, 150,  100.f, 200.f,  0.003f,  50, 90 },
		{ EVoxelMaterial::OreMolybdenum,1500.f,  3200.f,   50, 150,  100.f, 200.f,  0.003f,  50, 90 },

		// Tier 5 — Very Rare
		{ EVoxelMaterial::OreTitanium,  2200.f,  4500.f,   30, 100,   80.f, 150.f,  0.001f,  60, 100 },
		{ EVoxelMaterial::OreTungsten,  2200.f,  4500.f,   30, 100,   80.f, 150.f,  0.001f,  60, 100 },
		{ EVoxelMaterial::OreVanadium,  2200.f,  4500.f,   30, 100,   80.f, 150.f,  0.001f,  60, 100 },
		{ EVoxelMaterial::OrePlatinum,  2200.f,  4500.f,   30, 100,   80.f, 150.f,  0.001f,  60, 100 },
		{ EVoxelMaterial::OrePalladium, 2200.f,  4500.f,   30, 100,   80.f, 150.f,  0.001f,  60, 100 },

		// Tier 6 — Extremely Rare
		{ EVoxelMaterial::OreRhodium,   3200.f,  6500.f,   10,  50,   60.f, 120.f,  0.00025f, 70, 100 },
		{ EVoxelMaterial::OreIridium,   3200.f,  6500.f,   10,  50,   60.f, 120.f,  0.00025f, 70, 100 },
		{ EVoxelMaterial::OreNiobium,   3200.f,  6500.f,   10,  50,   60.f, 120.f,  0.00025f, 70, 100 },
		{ EVoxelMaterial::OreTantalum,  3200.f,  6500.f,   10,  50,   60.f, 120.f,  0.00025f, 70, 100 },
		{ EVoxelMaterial::OreOsmium,    3200.f,  6500.f,   10,  50,   60.f, 120.f,  0.00025f, 70, 100 },
	};

	const int32 NumOreTypes = UE_ARRAY_COUNT(OreParams);

	for (int32 OreIdx = 0; OreIdx < NumOreTypes; ++OreIdx)
	{
		const FOreGenParams& Params = OreParams[OreIdx];

		// Roll spawn chance
		if (ChunkRandom.FRand() > Params.SpawnChance)
		{
			continue;
		}

		// Generate vein center within the chunk
		const float VeinWorldX = (ChunkCoord.X + ChunkRandom.FRand()) * CHUNK_WORLD_SIZE;
		const float VeinWorldY = (ChunkCoord.Y + ChunkRandom.FRand()) * CHUNK_WORLD_SIZE;

		// Depth below surface (negative Z direction)
		const float VeinDepth = ChunkRandom.FRandRange(Params.MinDepthCm, Params.MaxDepthCm);
		const float VeinWorldZ = -VeinDepth;

		const float VeinRadius = ChunkRandom.FRandRange(Params.MinRadius, Params.MaxRadius);
		const int32 VeinOreAmount = ChunkRandom.RandRange(Params.MinOre, Params.MaxOre);
		const uint8 VeinQuality = static_cast<uint8>(ChunkRandom.RandRange(Params.MinQuality, Params.MaxQuality));

		FVeinData NewVein;
		NewVein.Center = FVector(VeinWorldX, VeinWorldY, VeinWorldZ);
		NewVein.Radius = VeinRadius;
		NewVein.Material = Params.Material;
		NewVein.Quality = VeinQuality;
		NewVein.RemainingOre = VeinOreAmount;

		OreVeins.Add(NewVein);
	}
}

bool AAoCVoxelWorld::ProspectAt(FVector WorldPos, float SearchRadius, TArray<FVeinData>& OutVeins) const
{
	OutVeins.Reset();
	const float SearchRadiusSq = SearchRadius * SearchRadius;

	for (const FVeinData& Vein : OreVeins)
	{
		if (Vein.IsDepleted())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(WorldPos, Vein.Center);
		if (DistSq <= SearchRadiusSq)
		{
			OutVeins.Add(Vein);
		}
	}

	return OutVeins.Num() > 0;
}

// ─── Chunk Pool ──────────────────────────────────────────────────────────────

AAoCVoxelChunk* AAoCVoxelWorld::AcquireChunk()
{
	// Try to recycle from pool
	if (ChunkPool.Num() > 0)
	{
		AAoCVoxelChunk* Chunk = ChunkPool.Pop(EAllowShrinking::No);
		if (Chunk && IsValid(Chunk))
		{
			Chunk->SetActorHiddenInGame(false);
			Chunk->SetActorEnableCollision(true);
			Chunk->SetActorTickEnabled(true);
			return Chunk;
		}
	}

	// Spawn a new chunk actor
	UWorld* World = GetWorld();
	if (!World || !ChunkClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAoCVoxelChunk* NewChunk = World->SpawnActor<AAoCVoxelChunk>(ChunkClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	return NewChunk;
}

void AAoCVoxelWorld::ReleaseChunk(AAoCVoxelChunk* Chunk)
{
	if (!Chunk || !IsValid(Chunk))
	{
		return;
	}

	Chunk->ResetChunk();
	Chunk->SetActorHiddenInGame(true);
	Chunk->SetActorEnableCollision(false);
	Chunk->SetActorTickEnabled(false);

	if (ChunkPool.Num() < MaxPoolSize)
	{
		ChunkPool.Add(Chunk);
	}
	else
	{
		// Pool is full — destroy the chunk
		Chunk->Destroy();
	}
}

// ─── LOD ─────────────────────────────────────────────────────────────────────

int32 AAoCVoxelWorld::CalculateLOD(FIntVector ChunkCoord, FIntVector PlayerChunkCoord) const
{
	const int32 Dist = FMath::Max3(
		FMath::Abs(ChunkCoord.X - PlayerChunkCoord.X),
		FMath::Abs(ChunkCoord.Y - PlayerChunkCoord.Y),
		FMath::Abs(ChunkCoord.Z - PlayerChunkCoord.Z)
	);

	if (Dist <= 1)
	{
		return 0; // Full detail within 1 chunk
	}
	else if (Dist <= 2)
	{
		return 1; // Half detail at 2 chunks
	}
	else
	{
		return 2; // Quarter detail at 3+ chunks
	}
}

// ─── Vein Application ────────────────────────────────────────────────────────

void AAoCVoxelWorld::ApplyVeinsToChunk(AAoCVoxelChunk* Chunk)
{
	if (!Chunk || !Chunk->bIsInitialized)
	{
		return;
	}

	const FVector ChunkOrigin = Chunk->GetChunkWorldOrigin();
	const FVector ChunkEnd = ChunkOrigin + FVector(CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE);

	for (const FVeinData& Vein : OreVeins)
	{
		if (Vein.IsDepleted())
		{
			continue;
		}

		// Quick AABB check: does this vein's bounding sphere overlap this chunk?
		const FVector ClosestPoint(
			FMath::Clamp(Vein.Center.X, ChunkOrigin.X, ChunkEnd.X),
			FMath::Clamp(Vein.Center.Y, ChunkOrigin.Y, ChunkEnd.Y),
			FMath::Clamp(Vein.Center.Z, ChunkOrigin.Z, ChunkEnd.Z)
		);

		if (FVector::DistSquared(Vein.Center, ClosestPoint) > Vein.Radius * Vein.Radius)
		{
			continue;
		}

		// Apply vein to overlapping voxels
		TArray<FVoxelData>& Grid = Chunk->GetVoxelGrid();
		const float VeinRadiusSq = Vein.Radius * Vein.Radius;

		for (int32 Z = 0; Z < CHUNK_SIZE; ++Z)
		{
			for (int32 Y = 0; Y < CHUNK_SIZE; ++Y)
			{
				for (int32 X = 0; X < CHUNK_SIZE; ++X)
				{
					const FVector VoxelWorld = AoCVoxelUtils::ChunkLocalToWorld(Chunk->ChunkCoord, X, Y, Z);
					const float DistSq = FVector::DistSquared(VoxelWorld, Vein.Center);

					if (DistSq < VeinRadiusSq)
					{
						const int32 Idx = AoCVoxelUtils::VoxelIndex(X, Y, Z);

						// Only replace solid rock/soil with ore (don't replace air)
						if (Grid[Idx].Density > 0.f && Grid[Idx].Material == EVoxelMaterial::Rock)
						{
							// Ore density falls off at edges of vein for natural look
							const float Dist = FMath::Sqrt(DistSq);
							const float CoreRadius = Vein.Radius * 0.6f;
							float OreDensity = 1.f;

							if (Dist > CoreRadius)
							{
								// Edge of vein: mix ore with rock
								OreDensity = 1.f - (Dist - CoreRadius) / (Vein.Radius - CoreRadius);
							}

							// Only place ore if density is significant
							if (OreDensity > 0.3f)
							{
								Grid[Idx].Material = Vein.Material;
								Grid[Idx].Quality = Vein.Quality;
								// Keep existing density (terrain shape)
							}
						}
					}
				}
			}
		}
	}

	// Mark chunk dirty to regenerate mesh with ore colors
	Chunk->RegenerateMesh();
}

// ─── Deterministic Random ────────────────────────────────────────────────────

FRandomStream AAoCVoxelWorld::GetChunkRandom(FIntVector ChunkCoord) const
{
	// Combine world seed with chunk coordinate for deterministic per-chunk randomness
	const int32 Hash = HashCombine(
		HashCombine(
			HashCombine(GetTypeHash(WorldSeed), GetTypeHash(ChunkCoord.X)),
			GetTypeHash(ChunkCoord.Y)
		),
		GetTypeHash(ChunkCoord.Z)
	);

	return FRandomStream(Hash);
}
