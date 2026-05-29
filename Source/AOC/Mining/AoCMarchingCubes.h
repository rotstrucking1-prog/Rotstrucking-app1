// AoCMarchingCubes.h
// Architect of Creation - Marching Cubes Mesh Generation
// Static utility class implementing Paul Bourke's marching cubes algorithm
// for converting volumetric voxel data into triangle meshes.

#pragma once

#include "CoreMinimal.h"
#include "AoCVoxelTypes.h"

/**
 * Static utility class for marching cubes isosurface extraction.
 * Uses Paul Bourke's lookup tables for the 256 cube configurations.
 * This is NOT a UObject — purely a C++ utility class.
 */
class AOC_API FAoCMarchingCubes
{
public:
	FAoCMarchingCubes() = delete;

	/**
	 * Generate a triangle mesh from a 3D voxel grid using the marching cubes algorithm.
	 *
	 * @param VoxelGrid		Flat array of voxel data (ChunkSize^3 elements, indexed [X + Y*Size + Z*Size*Size])
	 * @param ChunkSize		Number of voxels per axis (typically CHUNK_SIZE = 32)
	 * @param VoxelSize		World-space size of each voxel in cm (typically 50.0)
	 * @param IsoLevel		Density threshold for surface extraction (typically 0.5)
	 * @param OutVertices	Output vertex positions in local chunk space
	 * @param OutTriangles	Output triangle indices
	 * @param OutNormals	Output vertex normals (computed via central differences)
	 * @param OutUVs		Output UV coordinates
	 * @param OutColors		Output vertex colors (mapped from EVoxelMaterial)
	 */
	static void GenerateMesh(
		const TArray<FVoxelData>& VoxelGrid,
		int32 ChunkSize,
		float VoxelSize,
		float IsoLevel,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FColor>& OutColors
	);

	/**
	 * Generate a mesh at reduced resolution for LOD.
	 * Step = 2 means half resolution, step = 4 means quarter.
	 */
	static void GenerateMeshLOD(
		const TArray<FVoxelData>& VoxelGrid,
		int32 ChunkSize,
		float VoxelSize,
		float IsoLevel,
		int32 LODStep,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FColor>& OutColors
	);

	/** Map a voxel material to a display color for vertex coloring. */
	static FColor GetMaterialColor(EVoxelMaterial Material);

private:
	/**
	 * Linearly interpolate vertex position along an edge between two voxel corners.
	 *
	 * @param P1	Position of corner 1
	 * @param P2	Position of corner 2
	 * @param V1	Density at corner 1
	 * @param V2	Density at corner 2
	 * @param IsoLevel	Target density threshold
	 * @return Interpolated position on the edge where density crosses the isolevel
	 */
	static FVector VertexInterp(const FVector& P1, const FVector& P2, float V1, float V2, float IsoLevel);

	/**
	 * Compute a normal at a voxel grid position using central differences.
	 *
	 * @param VoxelGrid	Voxel data array
	 * @param X, Y, Z	Voxel coordinates
	 * @param ChunkSize	Grid dimension
	 * @return Normalized gradient vector (points away from solid)
	 */
	static FVector ComputeNormal(const TArray<FVoxelData>& VoxelGrid, int32 X, int32 Y, int32 Z, int32 ChunkSize);

	/** Sample density from the voxel grid, clamping to edges. */
	static float SampleDensity(const TArray<FVoxelData>& VoxelGrid, int32 X, int32 Y, int32 Z, int32 ChunkSize);

	/** Sample material from the voxel grid, clamping to edges. */
	static EVoxelMaterial SampleMaterial(const TArray<FVoxelData>& VoxelGrid, int32 X, int32 Y, int32 Z, int32 ChunkSize);

	// ── Paul Bourke Lookup Tables ───────────────────────────────────────────

	/** Edge table: maps cube vertex configuration (8 bits) to intersected edges (12-bit mask). */
	static const int32 EdgeTable[256];

	/** Triangle table: maps cube config to triangle vertex sequences (up to 5 triangles, -1 terminated). */
	static const int32 TriTable[256][16];
};
