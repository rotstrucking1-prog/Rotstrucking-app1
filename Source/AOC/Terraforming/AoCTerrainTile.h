// AoCTerrainTile.h
// Architect of Creation - Modifiable Terrain Tile
// A procedural mesh tile with 9 height control points (4 corners, 4 edges, 1 center).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCTerrainTile.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Height point index layout (top-down view, Y+ = South):
 *
 *   0 (NW) ── 1 (N) ── 2 (NE)
 *     │         │         │
 *   3 (W)  ── 4 (C)  ── 5 (E)
 *     │         │         │
 *   6 (SW) ── 7 (S) ── 8 (SE)
 */
UCLASS(BlueprintType, Blueprintable)
class AOC_API AAoCTerrainTile : public AActor
{
	GENERATED_BODY()

public:
	AAoCTerrainTile();

	virtual void BeginPlay() override;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Size of this tile in cm (width and depth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float TileSize;

	/** Procedural mesh that renders the terrain surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
	UProceduralMeshComponent* TerrainMesh;

	/**
	 * 9 height values — one per control point.
	 * Index order: 0=NW, 1=N, 2=NE, 3=W, 4=Center, 5=E, 6=SW, 7=S, 8=SE
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TArray<float> HeightPoints;

	/** Grid coordinate of this tile (for world management). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	FVector2D GridCoord;

	/** Dynamic material applied to the terrain surface. */
	UPROPERTY()
	UMaterialInstanceDynamic* TerrainMaterial;

	// ── Functions ───────────────────────────────────────────────────────────

	/** (Re)build the procedural mesh from current HeightPoints. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void GenerateMesh();

	/** Set the height of one control point (0-8) and regenerate the mesh. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void SetHeightAtPoint(int32 Index, float NewHeight);

	/** Get the height value at a control point index (0-8). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	float GetHeightAtPoint(int32 Index) const;

	/** Set all 9 heights at once and regenerate. Array must have 9 elements. */
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void SetAllHeights(const TArray<float>& Heights);

	/** Returns the index (0-8) of the control point closest to WorldLocation. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	int32 GetNearestPointIndex(FVector WorldLocation) const;

	/** Convert a control point index to its world-space position. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terrain")
	FVector GetPointWorldPosition(int32 Index) const;

	/** Rebuild collision after mesh changes. */
	void UpdateCollision();

	/** Build the 9 vertex positions from current HeightPoints. */
	TArray<FVector> BuildVertexPositions() const;

	/** Compute averaged normals from triangle faces. */
	static void CalculateNormals(
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		TArray<FVector>& OutNormals);

private:
	/** Local XY offsets for each of the 9 points (relative to tile centre). */
	FVector2D GetLocalOffset(int32 Index) const;
};
