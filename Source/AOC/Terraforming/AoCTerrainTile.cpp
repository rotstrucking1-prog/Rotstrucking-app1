// AoCTerrainTile.cpp
// Architect of Creation - Modifiable Terrain Tile Implementation
// Procedural mesh with 9 height control points forming 8 triangles.

#include "AoCTerrainTile.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

AAoCTerrainTile::AAoCTerrainTile()
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;

	TileSize = 200.f;
	GridCoord = FVector2D::ZeroVector;
	TerrainMaterial = nullptr;

	// Initialize 9 height points to zero
	HeightPoints.SetNum(9);
	for (auto& H : HeightPoints)
	{
		H = 0.f;
	}
}

// ─── BeginPlay ───────────────────────────────────────────────────────────────

void AAoCTerrainTile::BeginPlay()
{
	Super::BeginPlay();
	GenerateMesh();
}

// ─── Local Offset Helper ─────────────────────────────────────────────────────

FVector2D AAoCTerrainTile::GetLocalOffset(int32 Index) const
{
	// Index layout:
	//   0 (NW) ── 1 (N) ── 2 (NE)
	//   3 (W)  ── 4 (C) ── 5 (E)
	//   6 (SW) ── 7 (S) ── 8 (SE)
	const float Half = TileSize * 0.5f;

	switch (Index)
	{
	case 0: return FVector2D(-Half, -Half);  // NW
	case 1: return FVector2D(0.f,   -Half);  // N
	case 2: return FVector2D(Half,  -Half);  // NE
	case 3: return FVector2D(-Half,  0.f);   // W
	case 4: return FVector2D(0.f,    0.f);   // Center
	case 5: return FVector2D(Half,   0.f);   // E
	case 6: return FVector2D(-Half,  Half);  // SW
	case 7: return FVector2D(0.f,    Half);  // S
	case 8: return FVector2D(Half,   Half);  // SE
	default: return FVector2D::ZeroVector;
	}
}

// ─── Vertex Building ─────────────────────────────────────────────────────────

TArray<FVector> AAoCTerrainTile::BuildVertexPositions() const
{
	TArray<FVector> Vertices;
	Vertices.SetNum(9);

	for (int32 i = 0; i < 9; ++i)
	{
		const FVector2D Offset = GetLocalOffset(i);
		Vertices[i] = FVector(Offset.X, Offset.Y, HeightPoints[i]);
	}

	return Vertices;
}

// ─── Normal Calculation ──────────────────────────────────────────────────────

void AAoCTerrainTile::CalculateNormals(
	const TArray<FVector>& Vertices,
	const TArray<int32>& Triangles,
	TArray<FVector>& OutNormals)
{
	OutNormals.SetNumZeroed(Vertices.Num());

	const int32 NumTris = Triangles.Num() / 3;
	for (int32 T = 0; T < NumTris; ++T)
	{
		const int32 I0 = Triangles[T * 3 + 0];
		const int32 I1 = Triangles[T * 3 + 1];
		const int32 I2 = Triangles[T * 3 + 2];

		const FVector Edge1 = Vertices[I1] - Vertices[I0];
		const FVector Edge2 = Vertices[I2] - Vertices[I0];
		const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

		OutNormals[I0] += FaceNormal;
		OutNormals[I1] += FaceNormal;
		OutNormals[I2] += FaceNormal;
	}

	// Normalize accumulated per-vertex normals
	for (FVector& N : OutNormals)
	{
		N = N.GetSafeNormal();
		if (N.IsNearlyZero())
		{
			N = FVector::UpVector;
		}
	}
}

// ─── Mesh Generation ─────────────────────────────────────────────────────────

void AAoCTerrainTile::GenerateMesh()
{
	if (!TerrainMesh)
	{
		return;
	}

	// --- Vertices ---
	const TArray<FVector> Vertices = BuildVertexPositions();

	// --- Triangles (8 tris, CCW winding from above) ---
	TArray<int32> Triangles;
	Triangles.Reserve(24); // 8 triangles * 3 indices

	// Top-left quad:  0-1-4, 0-4-3
	Triangles.Append({ 0, 1, 4 });
	Triangles.Append({ 0, 4, 3 });
	// Top-right quad: 1-2-5, 1-5-4
	Triangles.Append({ 1, 2, 5 });
	Triangles.Append({ 1, 5, 4 });
	// Bottom-left quad: 3-4-7, 3-7-6
	Triangles.Append({ 3, 4, 7 });
	Triangles.Append({ 3, 7, 6 });
	// Bottom-right quad: 4-5-8, 4-8-7
	Triangles.Append({ 4, 5, 8 });
	Triangles.Append({ 4, 8, 7 });

	// --- Normals ---
	TArray<FVector> Normals;
	CalculateNormals(Vertices, Triangles, Normals);

	// --- UVs (map XY position to 0-1 range) ---
	TArray<FVector2D> UVs;
	UVs.SetNum(9);
	const float InvSize = 1.f / TileSize;
	for (int32 i = 0; i < 9; ++i)
	{
		const FVector2D Offset = GetLocalOffset(i);
		UVs[i] = FVector2D(
			(Offset.X / TileSize) + 0.5f,
			(Offset.Y / TileSize) + 0.5f);
	}

	// --- Vertex Colors (green-brown earth tone) ---
	TArray<FLinearColor> VertexColors;
	VertexColors.SetNum(9);
	for (auto& C : VertexColors)
	{
		C = FLinearColor(0.3f, 0.25f, 0.1f, 1.f);
	}

	// --- Tangents ---
	TArray<FProcMeshTangent> Tangents;
	Tangents.SetNum(9);
	for (auto& T : Tangents)
	{
		T = FProcMeshTangent(FVector(1.f, 0.f, 0.f), false);
	}

	// --- Create / Update mesh section ---
	TerrainMesh->CreateMeshSection_LinearColor(
		0,               // SectionIndex
		Vertices,
		Triangles,
		Normals,
		UVs,
		VertexColors,
		Tangents,
		true             // bCreateCollision
	);
}

// ─── Height Accessors ────────────────────────────────────────────────────────

void AAoCTerrainTile::SetHeightAtPoint(int32 Index, float NewHeight)
{
	if (Index < 0 || Index > 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("AAoCTerrainTile::SetHeightAtPoint — Index %d out of range (0-8)."), Index);
		return;
	}

	HeightPoints[Index] = NewHeight;
	GenerateMesh();
}

float AAoCTerrainTile::GetHeightAtPoint(int32 Index) const
{
	if (Index < 0 || Index > 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("AAoCTerrainTile::GetHeightAtPoint — Index %d out of range."), Index);
		return 0.f;
	}
	return HeightPoints[Index];
}

void AAoCTerrainTile::SetAllHeights(const TArray<float>& Heights)
{
	if (Heights.Num() != 9)
	{
		UE_LOG(LogTemp, Warning, TEXT("AAoCTerrainTile::SetAllHeights — Expected 9 values, got %d."), Heights.Num());
		return;
	}

	HeightPoints = Heights;
	GenerateMesh();
}

// ─── Spatial Queries ─────────────────────────────────────────────────────────

int32 AAoCTerrainTile::GetNearestPointIndex(FVector WorldLocation) const
{
	int32 NearestIdx = 0;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (int32 i = 0; i < 9; ++i)
	{
		const FVector PointWorld = GetPointWorldPosition(i);
		const float DistSq = FVector::DistSquared(WorldLocation, PointWorld);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestIdx = i;
		}
	}

	return NearestIdx;
}

FVector AAoCTerrainTile::GetPointWorldPosition(int32 Index) const
{
	if (Index < 0 || Index > 8)
	{
		return GetActorLocation();
	}

	const FVector2D Offset = GetLocalOffset(Index);
	const FVector ActorLoc = GetActorLocation();
	return FVector(
		ActorLoc.X + Offset.X,
		ActorLoc.Y + Offset.Y,
		ActorLoc.Z + HeightPoints[Index]);
}

// ─── Collision ───────────────────────────────────────────────────────────────

void AAoCTerrainTile::UpdateCollision()
{
	// Regenerating the mesh via CreateMeshSection_LinearColor with bCreateCollision=true
	// already rebuilds collision. This is called explicitly if needed outside of GenerateMesh.
	GenerateMesh();
}
