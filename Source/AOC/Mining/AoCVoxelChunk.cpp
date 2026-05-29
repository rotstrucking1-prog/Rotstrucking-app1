// AoCVoxelChunk.cpp
// Architect of Creation - Voxel Chunk Actor Implementation

#include "AoCVoxelChunk.h"
#include "AoCMarchingCubes.h"
#include "ProceduralMeshComponent.h"
#include "Async/Async.h"

AAoCVoxelChunk::AAoCVoxelChunk()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f; // Half-second tick for dirty checks

	ChunkMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ChunkMesh"));
	RootComponent = ChunkMesh;

	// Default settings
	ChunkMesh->bUseAsyncCooking = true;
	ChunkMesh->SetCastShadow(true);

	ChunkCoord = FIntVector::ZeroValue;
	LODLevel = 0;
	bIsDirty = false;
	bIsGenerating = false;
	bIsInitialized = false;
	IsoLevel = 0.5f;
	ChunkMaterial = nullptr;
}

void AAoCVoxelChunk::BeginPlay()
{
	Super::BeginPlay();
}

void AAoCVoxelChunk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Auto-regenerate if dirty and not already in progress
	if (bIsDirty && !bIsGenerating && bIsInitialized)
	{
		GenerateMesh();
	}
}

// ─── Initialization ──────────────────────────────────────────────────────────

void AAoCVoxelChunk::InitializeChunk(FIntVector Coord)
{
	ChunkCoord = Coord;

	// Position the actor at the chunk's world origin
	SetActorLocation(GetChunkWorldOrigin());

	// Allocate and fill voxel grid
	VoxelGrid.SetNumZeroed(CHUNK_VOLUME);

	// Procedural terrain generation
	// Surface is at Z=0 in world space. Chunks above surface are mostly air,
	// chunks below are solid with layered materials.
	const float ChunkWorldZ = Coord.Z * CHUNK_WORLD_SIZE;

	for (int32 Z = 0; Z < CHUNK_SIZE; ++Z)
	{
		for (int32 Y = 0; Y < CHUNK_SIZE; ++Y)
		{
			for (int32 X = 0; X < CHUNK_SIZE; ++X)
			{
				const float WorldX = Coord.X * CHUNK_WORLD_SIZE + X * VOXEL_SIZE;
				const float WorldY = Coord.Y * CHUNK_WORLD_SIZE + Y * VOXEL_SIZE;
				const float WorldZ = ChunkWorldZ + Z * VOXEL_SIZE;

				// Surface height with gentle noise (using sine for determinism)
				const float NoiseScale1 = 0.005f;
				const float NoiseScale2 = 0.015f;
				const float SurfaceHeight =
					FMath::Sin(WorldX * NoiseScale1) * 200.f +
					FMath::Cos(WorldY * NoiseScale1) * 200.f +
					FMath::Sin(WorldX * NoiseScale2 + WorldY * NoiseScale2) * 80.f;

				const float DepthBelowSurface = SurfaceHeight - WorldZ;
				FVoxelData Voxel;

				if (DepthBelowSurface <= -VOXEL_SIZE)
				{
					// Above surface — air
					Voxel.Material = EVoxelMaterial::Air;
					Voxel.Density = 0.f;
					Voxel.Quality = 0;
				}
				else if (DepthBelowSurface < VOXEL_SIZE)
				{
					// Surface transition — partial density for smooth surface
					Voxel.Material = EVoxelMaterial::FertileSoil;
					Voxel.Density = FMath::Clamp((DepthBelowSurface + VOXEL_SIZE) / (2.f * VOXEL_SIZE), 0.f, 1.f);
					Voxel.Quality = FMath::RandRange(40, 70);
				}
				else if (DepthBelowSurface < 100.f) // 0-1m: topsoil
				{
					Voxel.Material = EVoxelMaterial::FertileSoil;
					Voxel.Density = 1.f;
					Voxel.Quality = FMath::RandRange(40, 70);
				}
				else if (DepthBelowSurface < 200.f) // 1-2m: soil
				{
					Voxel.Material = EVoxelMaterial::Soil;
					Voxel.Density = 1.f;
					Voxel.Quality = FMath::RandRange(30, 60);
				}
				else if (DepthBelowSurface < 300.f) // 2-3m: clay/sand mix
				{
					// Alternate clay and sand based on position
					const float ClayNoise = FMath::Sin(WorldX * 0.02f + WorldY * 0.03f);
					Voxel.Material = (ClayNoise > 0.f) ? EVoxelMaterial::Clay : EVoxelMaterial::Sand;
					Voxel.Density = 1.f;
					Voxel.Quality = FMath::RandRange(30, 60);
				}
				else
				{
					// 3m+: bedrock (rock)
					Voxel.Material = EVoxelMaterial::Rock;
					Voxel.Density = 1.f;
					Voxel.Quality = FMath::RandRange(20, 50);
				}

				const int32 Idx = AoCVoxelUtils::VoxelIndex(X, Y, Z);
				VoxelGrid[Idx] = Voxel;
			}
		}
	}

	bIsInitialized = true;
	bIsDirty = true;
}

void AAoCVoxelChunk::ResetChunk()
{
	VoxelGrid.Reset();
	bIsInitialized = false;
	bIsDirty = false;
	bIsGenerating = false;
	LODLevel = 0;
	ChunkCoord = FIntVector::ZeroValue;

	// Clear the mesh
	if (ChunkMesh)
	{
		ChunkMesh->ClearAllMeshSections();
	}
}

// ─── Mesh Generation ─────────────────────────────────────────────────────────

void AAoCVoxelChunk::GenerateMesh()
{
	if (bIsGenerating || !bIsInitialized)
	{
		return;
	}

	bIsGenerating = true;
	bIsDirty = false;

	// Copy data for async processing
	TArray<FVoxelData> VoxelCopy = VoxelGrid;
	const int32 CurrentChunkSize = CHUNK_SIZE;
	const float CurrentVoxelSize = VOXEL_SIZE;
	const float CurrentIsoLevel = IsoLevel;
	const int32 CurrentLODStep = GetLODStep();

	// Weak pointer to self for async callback safety
	TWeakObjectPtr<AAoCVoxelChunk> WeakThis(this);

	// Compute mesh on background thread
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, VoxelCopy = MoveTemp(VoxelCopy), CurrentChunkSize, CurrentVoxelSize, CurrentIsoLevel, CurrentLODStep]() mutable
		{
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FColor> Colors;

			FAoCMarchingCubes::GenerateMeshLOD(
				VoxelCopy,
				CurrentChunkSize,
				CurrentVoxelSize,
				CurrentIsoLevel,
				CurrentLODStep,
				Vertices,
				Triangles,
				Normals,
				UVs,
				Colors
			);

			// Dispatch to game thread for mesh creation
			AsyncTask(ENamedThreads::GameThread,
				[WeakThis,
				 Vertices = MoveTemp(Vertices),
				 Triangles = MoveTemp(Triangles),
				 Normals = MoveTemp(Normals),
				 UVs = MoveTemp(UVs),
				 Colors = MoveTemp(Colors)]() mutable
				{
					if (AAoCVoxelChunk* Chunk = WeakThis.Get())
					{
						Chunk->ApplyMeshData(
							MoveTemp(Vertices),
							MoveTemp(Triangles),
							MoveTemp(Normals),
							MoveTemp(UVs),
							MoveTemp(Colors)
						);
					}
				}
			);
		}
	);
}

void AAoCVoxelChunk::ApplyMeshData(
	TArray<FVector>&& Vertices,
	TArray<int32>&& Triangles,
	TArray<FVector>&& Normals,
	TArray<FVector2D>&& UVs,
	TArray<FColor>&& Colors)
{
	if (!ChunkMesh)
	{
		bIsGenerating = false;
		return;
	}

	// Clear existing mesh
	ChunkMesh->ClearAllMeshSections();

	if (Vertices.Num() > 0 && Triangles.Num() > 0)
	{
		// Build tangent array (empty — UE5 can auto-compute from normals)
		TArray<FProcMeshTangent> Tangents;

		// Only enable collision for LOD 0 (full-detail chunks)
		const bool bCreateCollision = (LODLevel == 0);

		ChunkMesh->CreateMeshSection(
			0,              // Section index
			Vertices,
			Triangles,
			Normals,
			UVs,
			Colors,
			Tangents,
			bCreateCollision
		);

		// Apply material if set
		if (ChunkMaterial)
		{
			ChunkMesh->SetMaterial(0, ChunkMaterial);
		}

		// Collision settings — only enable for walkable/interactable chunks
		ChunkMesh->SetCollisionEnabled(bCreateCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCreateCollision)
		{
			ChunkMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
		}
	}

	bIsGenerating = false;

	// If dirty again while generating, re-trigger
	if (bIsDirty)
	{
		GenerateMesh();
	}
}

void AAoCVoxelChunk::RegenerateMesh()
{
	bIsDirty = true;
	if (!bIsGenerating)
	{
		GenerateMesh();
	}
}

// ─── Voxel Accessors ─────────────────────────────────────────────────────────

FVoxelData AAoCVoxelChunk::GetVoxel(int32 X, int32 Y, int32 Z) const
{
	if (!AoCVoxelUtils::IsValidVoxel(X, Y, Z) || !bIsInitialized)
	{
		return FVoxelData();
	}

	const int32 Idx = AoCVoxelUtils::VoxelIndex(X, Y, Z);
	if (Idx >= 0 && Idx < VoxelGrid.Num())
	{
		return VoxelGrid[Idx];
	}
	return FVoxelData();
}

void AAoCVoxelChunk::SetVoxel(int32 X, int32 Y, int32 Z, FVoxelData Data)
{
	if (!AoCVoxelUtils::IsValidVoxel(X, Y, Z) || !bIsInitialized)
	{
		return;
	}

	const int32 Idx = AoCVoxelUtils::VoxelIndex(X, Y, Z);
	if (Idx >= 0 && Idx < VoxelGrid.Num())
	{
		VoxelGrid[Idx] = Data;
		bIsDirty = true;
	}
}

void AAoCVoxelChunk::CarveVoxel(FVector WorldPos, float Radius)
{
	if (!bIsInitialized || Radius <= 0.f)
	{
		return;
	}

	const FVector ChunkOrigin = GetChunkWorldOrigin();
	const FVector LocalPos = WorldPos - ChunkOrigin;

	// Calculate bounding box of the carve sphere in voxel coordinates
	const int32 MinX = FMath::Max(0, FMath::FloorToInt((LocalPos.X - Radius) / VOXEL_SIZE));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt((LocalPos.Y - Radius) / VOXEL_SIZE));
	const int32 MinZ = FMath::Max(0, FMath::FloorToInt((LocalPos.Z - Radius) / VOXEL_SIZE));
	const int32 MaxX = FMath::Min(CHUNK_SIZE - 1, FMath::CeilToInt((LocalPos.X + Radius) / VOXEL_SIZE));
	const int32 MaxY = FMath::Min(CHUNK_SIZE - 1, FMath::CeilToInt((LocalPos.Y + Radius) / VOXEL_SIZE));
	const int32 MaxZ = FMath::Min(CHUNK_SIZE - 1, FMath::CeilToInt((LocalPos.Z + Radius) / VOXEL_SIZE));

	bool bModified = false;
	const float RadiusSq = Radius * Radius;

	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				// Voxel center in local space
				const FVector VoxelCenter(
					(X + 0.5f) * VOXEL_SIZE,
					(Y + 0.5f) * VOXEL_SIZE,
					(Z + 0.5f) * VOXEL_SIZE
				);

				const float DistSq = FVector::DistSquared(VoxelCenter, LocalPos);
				if (DistSq < RadiusSq)
				{
					const int32 Idx = AoCVoxelUtils::VoxelIndex(X, Y, Z);
					if (Idx >= 0 && Idx < VoxelGrid.Num())
					{
						// Smooth carving: reduce density based on distance from center
						const float Dist = FMath::Sqrt(DistSq);
						const float FalloffStart = Radius * 0.7f;
						float NewDensity = 0.f;

						if (Dist > FalloffStart)
						{
							// Partial carve at edges for smooth surface
							const float T = (Dist - FalloffStart) / (Radius - FalloffStart);
							NewDensity = VoxelGrid[Idx].Density * T;
						}

						if (NewDensity < VoxelGrid[Idx].Density)
						{
							VoxelGrid[Idx].Density = NewDensity;
							if (NewDensity <= 0.f)
							{
								VoxelGrid[Idx].Material = EVoxelMaterial::Air;
							}
							bModified = true;
						}
					}
				}
			}
		}
	}

	if (bModified)
	{
		bIsDirty = true;
	}
}

// ─── LOD ─────────────────────────────────────────────────────────────────────

void AAoCVoxelChunk::SetLODLevel(int32 NewLOD)
{
	NewLOD = FMath::Clamp(NewLOD, 0, 2);
	if (NewLOD != LODLevel)
	{
		LODLevel = NewLOD;
		bIsDirty = true;
	}
}

int32 AAoCVoxelChunk::GetLODStep() const
{
	switch (LODLevel)
	{
	case 0: return 1;  // Full resolution
	case 1: return 2;  // Half resolution
	case 2: return 4;  // Quarter resolution
	default: return 1;
	}
}

// ─── Utility ─────────────────────────────────────────────────────────────────

FVector AAoCVoxelChunk::GetChunkWorldOrigin() const
{
	return FVector(
		ChunkCoord.X * CHUNK_WORLD_SIZE,
		ChunkCoord.Y * CHUNK_WORLD_SIZE,
		ChunkCoord.Z * CHUNK_WORLD_SIZE
	);
}

bool AAoCVoxelChunk::ContainsWorldPosition(FVector WorldPos) const
{
	const FVector Origin = GetChunkWorldOrigin();
	return WorldPos.X >= Origin.X && WorldPos.X < Origin.X + CHUNK_WORLD_SIZE
		&& WorldPos.Y >= Origin.Y && WorldPos.Y < Origin.Y + CHUNK_WORLD_SIZE
		&& WorldPos.Z >= Origin.Z && WorldPos.Z < Origin.Z + CHUNK_WORLD_SIZE;
}
