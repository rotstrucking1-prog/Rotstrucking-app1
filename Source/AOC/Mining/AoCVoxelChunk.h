// AoCVoxelChunk.h
// Architect of Creation - Voxel Chunk Actor
// Represents a single chunk of the voxel terrain grid (32×32×32 voxels).
// Uses ProceduralMeshComponent for rendering and collision.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCVoxelChunk.generated.h"

class UProceduralMeshComponent;

UCLASS()
class AOC_API AAoCVoxelChunk : public AActor
{
	GENERATED_BODY()

public:
	AAoCVoxelChunk();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Components ──────────────────────────────────────────────────────────

	/** Procedural mesh component for rendering the marching cubes surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Mesh")
	UProceduralMeshComponent* ChunkMesh;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Coordinate of this chunk in chunk-space (integer grid). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Chunk")
	FIntVector ChunkCoord;

	/** Current LOD level: 0 = full detail, 1 = half, 2 = quarter resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Chunk", meta = (ClampMin = "0", ClampMax = "2"))
	int32 LODLevel;

	/** True if the voxel data has been modified since last mesh rebuild. */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Chunk")
	bool bIsDirty;

	/** True while an async mesh generation task is running. */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Chunk")
	bool bIsGenerating;

	/** True if this chunk has been initialized with voxel data. */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|Chunk")
	bool bIsInitialized;

	/** Density isolevel threshold for surface extraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Chunk", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IsoLevel;

	/** Material to apply to the procedural mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Chunk")
	UMaterialInterface* ChunkMaterial;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Initialize this chunk with procedural terrain data.
	 * Fills the voxel grid based on chunk coordinate.
	 * @param Coord The chunk coordinate in chunk-space
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void InitializeChunk(FIntVector Coord);

	/**
	 * Reset this chunk for reuse from pool. Clears voxel data and mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void ResetChunk();

	/**
	 * Generate the mesh from current voxel data.
	 * Spawns async task for computation, applies mesh on game thread.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void GenerateMesh();

	/**
	 * Mark dirty and regenerate mesh if not already generating.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void RegenerateMesh();

	/**
	 * Get voxel data at local coordinates.
	 * @param X, Y, Z Local voxel coordinates (0 to CHUNK_SIZE-1)
	 * @return Voxel data at the specified position
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|Chunk")
	FVoxelData GetVoxel(int32 X, int32 Y, int32 Z) const;

	/**
	 * Set voxel data at local coordinates and mark chunk dirty.
	 * @param X, Y, Z Local voxel coordinates (0 to CHUNK_SIZE-1)
	 * @param Data New voxel data to set
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void SetVoxel(int32 X, int32 Y, int32 Z, FVoxelData Data);

	/**
	 * Carve a spherical hole in the chunk around a world position.
	 * Sets all voxels within radius to Air with 0 density.
	 * @param WorldPos Center of the carve sphere in world space
	 * @param Radius Radius in centimeters
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void CarveVoxel(FVector WorldPos, float Radius);

	/**
	 * Set LOD level and regenerate mesh if changed.
	 * @param NewLOD New LOD level (0=full, 1=half, 2=quarter)
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Chunk")
	void SetLODLevel(int32 NewLOD);

	/**
	 * Get the world-space origin of this chunk.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|Chunk")
	FVector GetChunkWorldOrigin() const;

	/**
	 * Check if a world position falls within this chunk's bounds.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|Chunk")
	bool ContainsWorldPosition(FVector WorldPos) const;

	/** Direct access to the voxel grid array (for bulk operations). */
	TArray<FVoxelData>& GetVoxelGrid() { return VoxelGrid; }
	const TArray<FVoxelData>& GetVoxelGrid() const { return VoxelGrid; }

private:
	/** Flat array of voxel data (CHUNK_SIZE^3 elements). */
	TArray<FVoxelData> VoxelGrid;

	/** Apply the computed mesh data on the game thread. */
	void ApplyMeshData(
		TArray<FVector>&& Vertices,
		TArray<int32>&& Triangles,
		TArray<FVector>&& Normals,
		TArray<FVector2D>&& UVs,
		TArray<FColor>&& Colors
	);

	/** Convert LODLevel to step size for marching cubes. */
	int32 GetLODStep() const;
};
