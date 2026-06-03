// TerraForgeChunk.cpp
// TerraForge — Chunk implementation with voxel operations,
// landscape initialization, and thread-safe access.

#include "TerraForgeChunk.h"

// ============================================================================
// STATIC DATA
// ============================================================================

const FGeoVoxel FTerraForgeChunk::AirVoxel = FGeoVoxel();

// ============================================================================
// CONSTRUCTION
// ============================================================================

FTerraForgeChunk::FTerraForgeChunk()
	: Key(0, 0, 0)
{
	FMemory::Memset(OriginalSurfaceHeights, 0, sizeof(OriginalSurfaceHeights));
}

FTerraForgeChunk::FTerraForgeChunk(const FTerraChunkKey& InKey)
	: Key(InKey)
{
	FMemory::Memset(OriginalSurfaceHeights, 0, sizeof(OriginalSurfaceHeights));
}

FTerraForgeChunk::~FTerraForgeChunk() = default;

// ============================================================================
// VOXEL ACCESS
// ============================================================================

const FGeoVoxel& FTerraForgeChunk::GetVoxel(int32 X, int32 Y, int32 Z) const
{
	if (!IsInBounds(X, Y, Z))
	{
		return AirVoxel;
	}
	return Voxels[X][Y][Z];
}

FGeoVoxel& FTerraForgeChunk::GetVoxelMutable(int32 X, int32 Y, int32 Z)
{
	checkf(IsInBounds(X, Y, Z),
		TEXT("TerraForge: Voxel OOB (%d,%d,%d) in chunk %s"), X, Y, Z, *Key.ToString());

	bDirty = true;
	bHasModifications = true;
	TimeSinceLastInteraction = 0.0f;
	return Voxels[X][Y][Z];
}

void FTerraForgeChunk::SetVoxel(int32 X, int32 Y, int32 Z, const FGeoVoxel& Voxel)
{
	if (!IsInBounds(X, Y, Z))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TerraForge: SetVoxel OOB (%d,%d,%d) in chunk %s"), X, Y, Z, *Key.ToString());
		return;
	}

	Voxels[X][Y][Z] = Voxel;
	bDirty = true;
	bHasModifications = true;
	TimeSinceLastInteraction = 0.0f;
}

FIntVector FTerraForgeChunk::WorldToLocal(const FVector& WorldPos) const
{
	const FVector Origin = GetWorldOrigin();
	const FVector Local = (WorldPos - Origin) / (TF_VOXEL_SIZE * 100.0f); // cm to voxel
	return FIntVector(
		FMath::FloorToInt(Local.X),
		FMath::FloorToInt(Local.Y),
		FMath::FloorToInt(Local.Z)
	);
}

FVector FTerraForgeChunk::LocalToWorld(int32 X, int32 Y, int32 Z) const
{
	const FVector Origin = GetWorldOrigin();
	return Origin + FVector(
		(X + 0.5f) * TF_VOXEL_SIZE * 100.0f,
		(Y + 0.5f) * TF_VOXEL_SIZE * 100.0f,
		(Z + 0.5f) * TF_VOXEL_SIZE * 100.0f
	);
}

const FGeoVoxel& FTerraForgeChunk::GetVoxelOrNeighbor(int32 X, int32 Y, int32 Z) const
{
	// If in bounds, return directly
	if (IsInBounds(X, Y, Z))
	{
		return Voxels[X][Y][Z];
	}

	// Determine which neighbor to check
	int32 NeighborIdx = -1;
	int32 LocalX = X, LocalY = Y, LocalZ = Z;

	if (X >= TF_CHUNK_SIZE) { NeighborIdx = 0; LocalX = X - TF_CHUNK_SIZE; }
	else if (X < 0)          { NeighborIdx = 1; LocalX = X + TF_CHUNK_SIZE; }
	else if (Y >= TF_CHUNK_SIZE) { NeighborIdx = 2; LocalY = Y - TF_CHUNK_SIZE; }
	else if (Y < 0)          { NeighborIdx = 3; LocalY = Y + TF_CHUNK_SIZE; }
	else if (Z >= TF_CHUNK_SIZE) { NeighborIdx = 4; LocalZ = Z - TF_CHUNK_SIZE; }
	else if (Z < 0)          { NeighborIdx = 5; LocalZ = Z + TF_CHUNK_SIZE; }

	if (NeighborIdx >= 0 && Neighbors[NeighborIdx] != nullptr)
	{
		return Neighbors[NeighborIdx]->GetVoxel(LocalX, LocalY, LocalZ);
	}

	return AirVoxel;
}

// ============================================================================
// DENSITY SAMPLING
// ============================================================================

float FTerraForgeChunk::SampleDensity(float X, float Y, float Z) const
{
	// Trilinear interpolation of the density field
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const int32 Z0 = FMath::FloorToInt(Z);

	const float FracX = X - X0;
	const float FracY = Y - Y0;
	const float FracZ = Z - Z0;

	// Sample 8 corners
	const float D000 = GetVoxelOrNeighbor(X0,   Y0,   Z0  ).Density;
	const float D100 = GetVoxelOrNeighbor(X0+1, Y0,   Z0  ).Density;
	const float D010 = GetVoxelOrNeighbor(X0,   Y0+1, Z0  ).Density;
	const float D110 = GetVoxelOrNeighbor(X0+1, Y0+1, Z0  ).Density;
	const float D001 = GetVoxelOrNeighbor(X0,   Y0,   Z0+1).Density;
	const float D101 = GetVoxelOrNeighbor(X0+1, Y0,   Z0+1).Density;
	const float D011 = GetVoxelOrNeighbor(X0,   Y0+1, Z0+1).Density;
	const float D111 = GetVoxelOrNeighbor(X0+1, Y0+1, Z0+1).Density;

	// Trilinear interpolation
	const float C00 = FMath::Lerp(D000, D100, FracX);
	const float C10 = FMath::Lerp(D010, D110, FracX);
	const float C01 = FMath::Lerp(D001, D101, FracX);
	const float C11 = FMath::Lerp(D011, D111, FracX);

	const float C0 = FMath::Lerp(C00, C10, FracY);
	const float C1 = FMath::Lerp(C01, C11, FracY);

	return FMath::Lerp(C0, C1, FracZ);
}

FVector FTerraForgeChunk::SampleGradient(float X, float Y, float Z) const
{
	// Central differences for gradient estimation
	const float H = 0.5f; // half-voxel step

	const float Dx = SampleDensity(X + H, Y, Z) - SampleDensity(X - H, Y, Z);
	const float Dy = SampleDensity(X, Y + H, Z) - SampleDensity(X, Y - H, Z);
	const float Dz = SampleDensity(X, Y, Z + H) - SampleDensity(X, Y, Z - H);

	FVector Grad(Dx, Dy, Dz);
	if (!Grad.IsNearlyZero())
	{
		Grad.Normalize();
	}
	return Grad;
}

// ============================================================================
// BULK OPERATIONS
// ============================================================================

void FTerraForgeChunk::Fill(EGeoMaterial Material, float Density, float Quality)
{
	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
	{
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
		{
			for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
			{
				FGeoVoxel& V = Voxels[X][Y][Z];
				V.Density = Density;
				V.SetMaterial(Material);
				V.SetQuality(Quality);
			}
		}
	}
	bDirty = true;
}

void FTerraForgeChunk::FillColumn(int32 X, int32 Y, EGeoMaterial Material, float Density, float Quality)
{
	if (X < 0 || X >= TF_CHUNK_SIZE || Y < 0 || Y >= TF_CHUNK_SIZE) return;

	for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
	{
		FGeoVoxel& V = Voxels[X][Y][Z];
		V.Density = Density;
		V.SetMaterial(Material);
		V.SetQuality(Quality);
	}
	bDirty = true;
}

void FTerraForgeChunk::InitializeFromLandscape(
	const float SurfaceHeights[TF_CHUNK_SIZE][TF_CHUNK_SIZE],
	const TArray<FGeoLayerDef>& Layers,
	float BaseSoilQuality)
{
	const float ChunkBaseZ = Key.Z * TF_CHUNK_WORLD_SIZE; // meters

	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
	{
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
		{
			const float SurfaceH = SurfaceHeights[X][Y]; // meters, world-space

			// Store original data for regeneration
			OriginalSurfaceHeights[X][Y] = SurfaceH;

			for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
			{
				const float WorldZ = ChunkBaseZ + Z * TF_VOXEL_SIZE; // meters
				const float DepthBelowSurface = SurfaceH - WorldZ;

				FGeoVoxel& V = Voxels[X][Y][Z];

				if (DepthBelowSurface < -TF_VOXEL_SIZE)
				{
					// Well above surface — air
					V.Density = 1.0f;
					V.SetMaterial(EGeoMaterial::Air);
				}
				else if (DepthBelowSurface < 0.0f)
				{
					// Just above surface — transition zone
					// SDF: positive = air, but near surface
					V.Density = -DepthBelowSurface / TF_VOXEL_SIZE;
					V.SetMaterial(EGeoMaterial::Air);
				}
				else if (DepthBelowSurface < TF_VOXEL_SIZE)
				{
					// Just below surface — transition zone
					V.Density = -DepthBelowSurface / TF_VOXEL_SIZE;

					// Find the material for this depth
					EGeoMaterial Mat = EGeoMaterial::Topsoil;
					float LayerQ = BaseSoilQuality;
					for (const FGeoLayerDef& Layer : Layers)
					{
						if (DepthBelowSurface >= Layer.MinDepth && DepthBelowSurface < Layer.MaxDepth)
						{
							Mat = Layer.Material;
							LayerQ = FMath::RandRange(Layer.QualityRange.X, Layer.QualityRange.Y);
							break;
						}
					}
					V.SetMaterial(Mat);
					V.SetQuality(LayerQ);
				}
				else
				{
					// Below surface — solid
					V.Density = -1.0f;

					// Determine material from geological layers
					EGeoMaterial Mat = EGeoMaterial::Granite; // fallback
					float LayerQ = 50.0f;
					for (const FGeoLayerDef& Layer : Layers)
					{
						if (DepthBelowSurface >= Layer.MinDepth && DepthBelowSurface < Layer.MaxDepth)
						{
							Mat = Layer.Material;
							LayerQ = FMath::RandRange(Layer.QualityRange.X, Layer.QualityRange.Y);
							break;
						}
					}
					V.SetMaterial(Mat);
					V.SetQuality(LayerQ);
				}
			}
		}
	}

	bHasOriginalData = true;
	bDirty = true;
}

void FTerraForgeChunk::PlaceOreVein(
	const FIntVector& Center, EGeoOreType Ore,
	int32 Radius, float Quality, uint16 UnitsPerVoxel)
{
	const float RadiusSq = (float)(Radius * Radius);

	for (int32 X = Center.X - Radius; X <= Center.X + Radius; ++X)
	{
		for (int32 Y = Center.Y - Radius; Y <= Center.Y + Radius; ++Y)
		{
			for (int32 Z = Center.Z - Radius; Z <= Center.Z + Radius; ++Z)
			{
				if (!IsInBounds(X, Y, Z)) continue;

				// Ellipsoidal shape with noise
				const float Dx = (float)(X - Center.X);
				const float Dy = (float)(Y - Center.Y);
				const float Dz = (float)(Z - Center.Z);
				const float DistSq = Dx*Dx + Dy*Dy + Dz*Dz;

				if (DistSq > RadiusSq) continue;

				FGeoVoxel& V = Voxels[X][Y][Z];

				// Only replace solid non-air, non-ore voxels
				if (V.IsSolid() && !V.IsOre())
				{
					// Quality varies with distance from center (higher at core)
					const float DistFrac = FMath::Sqrt(DistSq) / (float)Radius;
					const float VeinQ = Quality * (1.0f - 0.3f * DistFrac); // ±30%

					V.MakeOre(Ore, VeinQ, UnitsPerVoxel);
				}
			}
		}
	}

	bDirty = true;
	bHasModifications = true;
}

// ============================================================================
// SNAPSHOT
// ============================================================================

TSharedPtr<FTerraForgeChunk> FTerraForgeChunk::CreateSnapshot() const
{
	FScopeLock ScopeLock(&DataLock);

	TSharedPtr<FTerraForgeChunk> Snap = MakeShared<FTerraForgeChunk>(Key);
	FMemory::Memcpy(Snap->Voxels, Voxels, sizeof(Voxels));
	Snap->bDirty = false;
	Snap->bHasModifications = bHasModifications;

	// Copy neighbor pointers (for cross-chunk mesh gen)
	FMemory::Memcpy(Snap->Neighbors, Neighbors, sizeof(Neighbors));

	return Snap;
}

// ============================================================================
// STATISTICS
// ============================================================================

int32 FTerraForgeChunk::CountSolidVoxels() const
{
	int32 Count = 0;
	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
			for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
				if (Voxels[X][Y][Z].IsSolid()) ++Count;
	return Count;
}

int32 FTerraForgeChunk::CountOreVoxels() const
{
	int32 Count = 0;
	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
			for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
				if (Voxels[X][Y][Z].IsOre()) ++Count;
	return Count;
}

EGeoMaterial FTerraForgeChunk::GetDominantMaterial() const
{
	int32 Counts[(int32)EGeoMaterial::MAX] = {};
	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
			for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
			{
				const uint8 Mat = Voxels[X][Y][Z].Material;
				if (Mat != (uint8)EGeoMaterial::Air && Mat < (uint8)EGeoMaterial::MAX)
					Counts[Mat]++;
			}

	int32 MaxCount = 0;
	EGeoMaterial Dominant = EGeoMaterial::Granite;
	for (int32 I = 0; I < (int32)EGeoMaterial::MAX; ++I)
	{
		if (Counts[I] > MaxCount)
		{
			MaxCount = Counts[I];
			Dominant = (EGeoMaterial)I;
		}
	}
	return Dominant;
}
