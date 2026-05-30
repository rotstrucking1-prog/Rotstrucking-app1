// TerraForgeChunk.h
// TerraForge — Individual terrain chunk containing a 16³ voxel grid.
// Thread-safe with snapshot support for async mesh generation.

#pragma once

#include "CoreMinimal.h"
#include "TerraForgeTypes.h"
#include "HAL/CriticalSection.h"

/**
 * A single chunk of terrain in the TerraForge system.
 * Contains a 16×16×16 grid of FGeoVoxel (4096 voxels).
 * Supports thread-safe read/write and dirty tracking.
 */
class FTerraForgeChunk
{
public:
	FTerraForgeChunk();
	explicit FTerraForgeChunk(const FTerraChunkKey& InKey);
	~FTerraForgeChunk();

	// Non-copyable, moveable
	FTerraForgeChunk(const FTerraForgeChunk&) = delete;
	FTerraForgeChunk& operator=(const FTerraForgeChunk&) = delete;
	FTerraForgeChunk(FTerraForgeChunk&&) = default;
	FTerraForgeChunk& operator=(FTerraForgeChunk&&) = default;

	// ── Identification ──────────────────────────────────────────────────────

	/** Get this chunk's key (integer coordinates in chunk-space). */
	const FTerraChunkKey& GetKey() const { return Key; }

	/** Get the world-space origin (min corner) of this chunk in cm. */
	FVector GetWorldOrigin() const { return Key.ToWorldPos(); }

	// ── Voxel Access (bounds-checked) ───────────────────────────────────────

	/** Get a voxel. Returns air voxel if out of bounds. */
	const FGeoVoxel& GetVoxel(int32 X, int32 Y, int32 Z) const;

	/** Get a mutable voxel reference. Marks chunk dirty. Asserts on OOB. */
	FGeoVoxel& GetVoxelMutable(int32 X, int32 Y, int32 Z);

	/** Set a voxel at the given local coordinate. Marks chunk dirty. */
	void SetVoxel(int32 X, int32 Y, int32 Z, const FGeoVoxel& Voxel);

	/** Check if local coordinates are within chunk bounds. */
	static FORCEINLINE bool IsInBounds(int32 X, int32 Y, int32 Z)
	{
		return X >= 0 && X < TF_CHUNK_SIZE &&
		       Y >= 0 && Y < TF_CHUNK_SIZE &&
		       Z >= 0 && Z < TF_CHUNK_SIZE;
	}

	/** Convert a world position (cm) to local voxel coordinates. */
	FIntVector WorldToLocal(const FVector& WorldPos) const;

	/** Convert local voxel coordinates to world position (cm, voxel center). */
	FVector LocalToWorld(int32 X, int32 Y, int32 Z) const;

	// ── Density Field ───────────────────────────────────────────────────────

	/** Get the density at a local position (trilinear interpolation). */
	float SampleDensity(float X, float Y, float Z) const;

	/** Get the density gradient (normal) at a local position. */
	FVector SampleGradient(float X, float Y, float Z) const;

	// ── Bulk Operations ─────────────────────────────────────────────────────

	/** Fill all voxels with a single material and density. */
	void Fill(EGeoMaterial Material, float Density, float Quality = 50.0f);

	/** Fill a column of voxels (all Z for given X,Y). */
	void FillColumn(int32 X, int32 Y, EGeoMaterial Material, float Density, float Quality = 50.0f);

	/** Initialize this chunk from landscape height data + geological layers.
	 *  SurfaceHeights: 16x16 array of surface heights relative to chunk base (meters).
	 *  Layers: sorted geological layers for this biome. */
	void InitializeFromLandscape(
		const float SurfaceHeights[TF_CHUNK_SIZE][TF_CHUNK_SIZE],
		const TArray<FGeoLayerDef>& Layers,
		float BaseSoilQuality);

	/** Place an ore vein centered at a local position with given radius. */
	void PlaceOreVein(const FIntVector& Center, EGeoOreType Ore,
	                  int32 Radius, float Quality, uint16 UnitsPerVoxel);

	// ── Dirty State ─────────────────────────────────────────────────────────

	/** Is this chunk dirty (needs mesh rebuild)? */
	bool IsDirty() const { return bDirty; }

	/** Mark this chunk as dirty. */
	void MarkDirty() { bDirty = true; }

	/** Clear the dirty flag (called after mesh generation). */
	void ClearDirty() { bDirty = false; }

	/** Has any voxel in this chunk ever been modified? */
	bool HasModifications() const { return bHasModifications; }

	// ── Mesh State ──────────────────────────────────────────────────────────

	/** Is a mesh currently being generated for this chunk? */
	bool IsMeshGenerating() const { return bMeshGenerating; }
	void SetMeshGenerating(bool bGen) { bMeshGenerating = bGen; }

	/** Does this chunk have a valid rendered mesh? */
	bool HasRenderedMesh() const { return bHasRenderedMesh; }
	void SetHasRenderedMesh(bool bHas) { bHasRenderedMesh = bHas; }

	// ── Snapshot (for async mesh gen) ───────────────────────────────────────

	/** Create a thread-safe copy of the voxel data for async processing.
	 *  Returns a heap-allocated copy the caller must delete. */
	TSharedPtr<FTerraForgeChunk> CreateSnapshot() const;

	// ── Thread Safety ───────────────────────────────────────────────────────

	/** Lock for read/write operations. */
	void Lock() const   { DataLock.Lock(); }
	void Unlock() const { DataLock.Unlock(); }

	// ── Statistics ──────────────────────────────────────────────────────────

	/** Count non-air voxels. */
	int32 CountSolidVoxels() const;

	/** Count ore voxels. */
	int32 CountOreVoxels() const;

	/** Get the dominant surface material (most common non-air material). */
	EGeoMaterial GetDominantMaterial() const;

	// ── Regeneration ────────────────────────────────────────────────────────

	/** Time since last player interaction (seconds). */
	float TimeSinceLastInteraction = 0.0f;

	/** Is this chunk on claimed land? (claimed chunks don't regenerate) */
	bool bIsClaimed = false;

	/** Original landscape height data (for regeneration). */
	float OriginalSurfaceHeights[TF_CHUNK_SIZE][TF_CHUNK_SIZE];
	bool bHasOriginalData = false;

	// ── Neighbor References ─────────────────────────────────────────────────

	/** Pointers to adjacent chunks (may be null). Set by subsystem.
	 *  Index: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z */
	FTerraForgeChunk* Neighbors[6] = { nullptr };

	/** Get a voxel from this chunk or a neighbor (for cross-chunk operations). */
	const FGeoVoxel& GetVoxelOrNeighbor(int32 X, int32 Y, int32 Z) const;

private:
	/** The chunk's position in chunk-space. */
	FTerraChunkKey Key;

	/** 3D voxel grid — the core data. 16³ = 4096 voxels. */
	FGeoVoxel Voxels[TF_CHUNK_SIZE][TF_CHUNK_SIZE][TF_CHUNK_SIZE];

	/** Thread safety lock. */
	mutable FCriticalSection DataLock;

	/** Dirty flag — true when voxels changed but mesh not rebuilt. */
	bool bDirty = true;

	/** True if any voxel has ever been modified from initial state. */
	bool bHasModifications = false;

	/** True if async mesh generation is in progress. */
	bool bMeshGenerating = false;

	/** True if this chunk has a valid ProceduralMesh in the scene. */
	bool bHasRenderedMesh = false;

	/** Static air voxel returned for out-of-bounds reads. */
	static const FGeoVoxel AirVoxel;

	/** Flat index into the 3D array. */
	static FORCEINLINE int32 FlatIndex(int32 X, int32 Y, int32 Z)
	{
		return X + Y * TF_CHUNK_SIZE + Z * TF_CHUNK_SIZE * TF_CHUNK_SIZE;
	}
};
