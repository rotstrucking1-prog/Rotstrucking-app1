// TerraForgeDualContour.cpp
// TerraForge — Dual Contouring mesh generation implementation.
// Generates terrain meshes from chunk voxel data with sharp edge preservation.

#include "TerraForgeDualContour.h"
#include "TerraForgeChunk.h"

// ============================================================================
// QEF SOLVER
// ============================================================================

void FTerraForgeDualContour::FQEFData::AddPlane(const FVector& Point, const FVector& Normal)
{
	// Accumulate A^T A (symmetric, store upper triangle)
	ATA[0] += Normal.X * Normal.X;  // xx
	ATA[1] += Normal.X * Normal.Y;  // xy
	ATA[2] += Normal.X * Normal.Z;  // xz
	ATA[3] += Normal.Y * Normal.Y;  // yy
	ATA[4] += Normal.Y * Normal.Z;  // yz
	ATA[5] += Normal.Z * Normal.Z;  // zz

	// Accumulate A^T b (where b_i = dot(n_i, p_i))
	const float Dot = FVector::DotProduct(Normal, Point);
	ATb.X += Normal.X * Dot;
	ATb.Y += Normal.Y * Dot;
	ATb.Z += Normal.Z * Dot;

	// Accumulate mass point
	MassPoint += Point;
	NumCrossings++;
}

FVector FTerraForgeDualContour::FQEFData::Solve(const FVector& CellMin, const FVector& CellMax) const
{
	if (NumCrossings == 0)
	{
		return (CellMin + CellMax) * 0.5f;
	}

	const FVector AvgPoint = MassPoint / (float)NumCrossings;

	// Build the 3x3 symmetric matrix (A^T A + regularization)
	const float Reg = TF_QEF_REGULARIZATION;
	const float M00 = ATA[0] + Reg;
	const float M01 = ATA[1];
	const float M02 = ATA[2];
	const float M11 = ATA[3] + Reg;
	const float M12 = ATA[4];
	const float M22 = ATA[5] + Reg;

	// Right-hand side adjusted for regularization toward mass point
	const float B0 = ATb.X + Reg * AvgPoint.X;
	const float B1 = ATb.Y + Reg * AvgPoint.Y;
	const float B2 = ATb.Z + Reg * AvgPoint.Z;

	// Solve using Cramer's rule on the 3x3 system
	// | M00 M01 M02 | | x |   | B0 |
	// | M01 M11 M12 | | y | = | B1 |
	// | M02 M12 M22 | | z |   | B2 |

	const float Det =
		M00 * (M11 * M22 - M12 * M12) -
		M01 * (M01 * M22 - M12 * M02) +
		M02 * (M01 * M12 - M11 * M02);

	if (FMath::Abs(Det) < 1e-10f)
	{
		// Singular — fall back to mass point
		return FMath::Clamp(AvgPoint,  CellMin, CellMax);
	}

	const float InvDet = 1.0f / Det;

	FVector Result;
	Result.X = InvDet * (
		B0 * (M11 * M22 - M12 * M12) -
		M01 * (B1 * M22 - M12 * B2) +
		M02 * (B1 * M12 - M11 * B2));
	Result.Y = InvDet * (
		M00 * (B1 * M22 - M12 * B2) -
		B0 * (M01 * M22 - M12 * M02) +
		M02 * (M01 * B2 - B1 * M02));
	Result.Z = InvDet * (
		M00 * (M11 * B2 - B1 * M12) -
		M01 * (M01 * B2 - B1 * M02) +
		B0 * (M01 * M12 - M11 * M02));

	// Clamp to cell bounds to prevent vertex from escaping
	Result.X = FMath::Clamp(Result.X, CellMin.X, CellMax.X);
	Result.Y = FMath::Clamp(Result.Y, CellMin.Y, CellMax.Y);
	Result.Z = FMath::Clamp(Result.Z, CellMin.Z, CellMax.Z);

	return Result;
}

// ============================================================================
// EDGE CROSSING DETECTION
// ============================================================================

bool FTerraForgeDualContour::FindEdgeCrossing(
	const FTerraForgeChunk& Chunk,
	int32 X0, int32 Y0, int32 Z0,
	int32 X1, int32 Y1, int32 Z1,
	FEdgeCrossing& OutCrossing)
{
	const FGeoVoxel& V0 = Chunk.GetVoxelOrNeighbor(X0, Y0, Z0);
	const FGeoVoxel& V1 = Chunk.GetVoxelOrNeighbor(X1, Y1, Z1);

	const float D0 = V0.Density;
	const float D1 = V1.Density;

	// Check for sign change (surface crossing)
	if ((D0 > 0.0f) == (D1 > 0.0f))
	{
		return false; // No crossing — both same side
	}

	// Linear interpolation to find crossing position
	const float T = D0 / (D0 - D1);

	// World-space position in cm
	const FVector ChunkOrigin = Chunk.GetWorldOrigin();
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;

	const FVector P0 = ChunkOrigin + FVector(
		(X0 + 0.5f) * VoxelCm,
		(Y0 + 0.5f) * VoxelCm,
		(Z0 + 0.5f) * VoxelCm);
	const FVector P1 = ChunkOrigin + FVector(
		(X1 + 0.5f) * VoxelCm,
		(Y1 + 0.5f) * VoxelCm,
		(Z1 + 0.5f) * VoxelCm);

	OutCrossing.Position = FMath::Lerp(P0, P1, T);

	// Normal from density gradient at the crossing point
	const float CrossX = FMath::Lerp((float)X0, (float)X1, T);
	const float CrossY = FMath::Lerp((float)Y0, (float)Y1, T);
	const float CrossZ = FMath::Lerp((float)Z0, (float)Z1, T);
	OutCrossing.Normal = Chunk.SampleGradient(CrossX, CrossY, CrossZ);

	// Material from the solid side
	OutCrossing.Material = (D0 <= 0.0f) ? V0.Material : V1.Material;

	return true;
}

// ============================================================================
// CELL SOLVING
// ============================================================================

FTerraForgeDualContour::FDCVertex FTerraForgeDualContour::SolveCell(
	const FTerraForgeChunk& Chunk,
	int32 CellX, int32 CellY, int32 CellZ,
	int32 Step)
{
	FDCVertex Result;
	Result.bValid = false;

	FQEFData QEF;
	uint8 DominantMaterial = (uint8)EGeoMaterial::Granite;
	int32 MaterialCounts[(int32)EGeoMaterial::MAX] = {};

	// Check all 12 edges of this cell
	// Each edge connects two adjacent corner voxels
	const int32 X = CellX;
	const int32 Y = CellY;
	const int32 Z = CellZ;
	const int32 S = Step;

	// Edge definitions: pairs of corner indices
	// Corners of cell: (X,Y,Z), (X+S,Y,Z), (X,Y+S,Z), (X+S,Y+S,Z),
	//                  (X,Y,Z+S), (X+S,Y,Z+S), (X,Y+S,Z+S), (X+S,Y+S,Z+S)
	struct FEdge { int32 X0,Y0,Z0, X1,Y1,Z1; };
	const FEdge Edges[12] = {
		// X-aligned edges (4)
		{X, Y,   Z,   X+S, Y,   Z  },
		{X, Y+S, Z,   X+S, Y+S, Z  },
		{X, Y,   Z+S, X+S, Y,   Z+S},
		{X, Y+S, Z+S, X+S, Y+S, Z+S},
		// Y-aligned edges (4)
		{X,   Y, Z,   X,   Y+S, Z  },
		{X+S, Y, Z,   X+S, Y+S, Z  },
		{X,   Y, Z+S, X,   Y+S, Z+S},
		{X+S, Y, Z+S, X+S, Y+S, Z+S},
		// Z-aligned edges (4)
		{X,   Y,   Z, X,   Y,   Z+S},
		{X+S, Y,   Z, X+S, Y,   Z+S},
		{X,   Y+S, Z, X,   Y+S, Z+S},
		{X+S, Y+S, Z, X+S, Y+S, Z+S}
	};

	for (int32 E = 0; E < 12; ++E)
	{
		FEdgeCrossing Crossing;
		if (FindEdgeCrossing(Chunk,
			Edges[E].X0, Edges[E].Y0, Edges[E].Z0,
			Edges[E].X1, Edges[E].Y1, Edges[E].Z1,
			Crossing))
		{
			QEF.AddPlane(Crossing.Position, Crossing.Normal);
			if (Crossing.Material < (uint8)EGeoMaterial::MAX)
			{
				MaterialCounts[Crossing.Material]++;
			}
		}
	}

	if (QEF.NumCrossings == 0)
	{
		return Result; // No surface in this cell
	}

	// Determine cell bounds in world space (cm)
	const FVector ChunkOrigin = Chunk.GetWorldOrigin();
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;
	const FVector CellMin = ChunkOrigin + FVector(
		(X + 0.5f) * VoxelCm,
		(Y + 0.5f) * VoxelCm,
		(Z + 0.5f) * VoxelCm);
	const FVector CellMax = ChunkOrigin + FVector(
		(X + S + 0.5f) * VoxelCm,
		(Y + S + 0.5f) * VoxelCm,
		(Z + S + 0.5f) * VoxelCm);

	// Solve QEF for optimal vertex position
	Result.Position = QEF.Solve(CellMin, CellMax);
	Result.Normal = (QEF.ATb).GetSafeNormal();
	if (Result.Normal.IsNearlyZero())
	{
		Result.Normal = FVector::UpVector;
	}
	Result.bValid = true;

	// Find dominant material
	int32 MaxCount = 0;
	for (int32 I = 1; I < (int32)EGeoMaterial::MAX; ++I)
	{
		if (MaterialCounts[I] > MaxCount)
		{
			MaxCount = MaterialCounts[I];
			DominantMaterial = (uint8)I;
		}
	}
	Result.Material = DominantMaterial;

	return Result;
}

// ============================================================================
// QUAD EMISSION
// ============================================================================

void FTerraForgeDualContour::EmitQuad(
	const FDCVertex& V0, const FDCVertex& V1,
	const FDCVertex& V2, const FDCVertex& V3,
	bool bFlip,
	FTerraChunkMeshData& OutMesh)
{
	if (!V0.bValid || !V1.bValid || !V2.bValid || !V3.bValid) return;

	const int32 BaseIdx = OutMesh.Vertices.Num();

	// Add 4 vertices
	OutMesh.Vertices.Add(V0.Position);
	OutMesh.Vertices.Add(V1.Position);
	OutMesh.Vertices.Add(V2.Position);
	OutMesh.Vertices.Add(V3.Position);

	OutMesh.Normals.Add(V0.Normal);
	OutMesh.Normals.Add(V1.Normal);
	OutMesh.Normals.Add(V2.Normal);
	OutMesh.Normals.Add(V3.Normal);

	OutMesh.UV0.Add(CalculateUV(V0.Position, V0.Normal));
	OutMesh.UV0.Add(CalculateUV(V1.Position, V1.Normal));
	OutMesh.UV0.Add(CalculateUV(V2.Position, V2.Normal));
	OutMesh.UV0.Add(CalculateUV(V3.Position, V3.Normal));

	OutMesh.VertexColors.Add(MaterialToVertexColor(V0.Material));
	OutMesh.VertexColors.Add(MaterialToVertexColor(V1.Material));
	OutMesh.VertexColors.Add(MaterialToVertexColor(V2.Material));
	OutMesh.VertexColors.Add(MaterialToVertexColor(V3.Material));

	OutMesh.MaterialIndices.Add(V0.Material);
	OutMesh.MaterialIndices.Add(V1.Material);
	OutMesh.MaterialIndices.Add(V2.Material);
	OutMesh.MaterialIndices.Add(V3.Material);

	// 2 triangles per quad
	if (bFlip)
	{
		// Triangle 1: 0-2-1
		OutMesh.Triangles.Add(BaseIdx + 0);
		OutMesh.Triangles.Add(BaseIdx + 2);
		OutMesh.Triangles.Add(BaseIdx + 1);
		// Triangle 2: 0-3-2
		OutMesh.Triangles.Add(BaseIdx + 0);
		OutMesh.Triangles.Add(BaseIdx + 3);
		OutMesh.Triangles.Add(BaseIdx + 2);
	}
	else
	{
		// Triangle 1: 0-1-2
		OutMesh.Triangles.Add(BaseIdx + 0);
		OutMesh.Triangles.Add(BaseIdx + 1);
		OutMesh.Triangles.Add(BaseIdx + 2);
		// Triangle 2: 0-2-3
		OutMesh.Triangles.Add(BaseIdx + 0);
		OutMesh.Triangles.Add(BaseIdx + 2);
		OutMesh.Triangles.Add(BaseIdx + 3);
	}
}

// ============================================================================
// MAIN MESH GENERATION
// ============================================================================

void FTerraForgeDualContour::GenerateMesh(
	const FTerraForgeChunk& Chunk,
	EChunkLOD LOD,
	FTerraChunkMeshData& OutMesh)
{
	OutMesh.Reset();
	OutMesh.LODLevel = LOD;

	const int32 Step = GetLODStep(LOD);
	const int32 CellCount = (TF_CHUNK_SIZE - 1) / Step;

	if (CellCount <= 0)
	{
		OutMesh.bValid = true;
		return;
	}

	// ── Step 1: Solve all cells ─────────────────────────────────────────────
	// Allocate vertex grid (one per cell)
	const int32 TotalCells = CellCount * CellCount * CellCount;
	TArray<FDCVertex> CellVertices;
	CellVertices.SetNum(TotalCells);

	auto CellIndex = [CellCount](int32 CX, int32 CY, int32 CZ) -> int32
	{
		return CX + CY * CellCount + CZ * CellCount * CellCount;
	};

	for (int32 CZ = 0; CZ < CellCount; ++CZ)
	{
		for (int32 CY = 0; CY < CellCount; ++CY)
		{
			for (int32 CX = 0; CX < CellCount; ++CX)
			{
				CellVertices[CellIndex(CX, CY, CZ)] = SolveCell(
					Chunk,
					CX * Step, CY * Step, CZ * Step,
					Step);
			}
		}
	}

	// ── Step 2: Connect vertices along sign-change edges ────────────────────
	// For each internal edge shared by 4 cells, if there's a sign change,
	// create a quad from the 4 cell vertices.

	// X-aligned edges: shared by cells (cx, cy, cz), (cx, cy-1, cz), (cx, cy, cz-1), (cx, cy-1, cz-1)
	for (int32 CZ = 0; CZ < CellCount; ++CZ)
	{
		for (int32 CY = 0; CY < CellCount; ++CY)
		{
			for (int32 CX = 0; CX < CellCount; ++CX)
			{
				const int32 VX = CX * Step;
				const int32 VY = CY * Step;
				const int32 VZ = CZ * Step;

				// Check X-edge: between (VX, VY+Step, VZ+Step) and (VX+Step, VY+Step, VZ+Step)
				if (CY + 1 < CellCount && CZ + 1 < CellCount)
				{
					const FGeoVoxel& VA = Chunk.GetVoxelOrNeighbor(VX, VY + Step, VZ + Step);
					const FGeoVoxel& VB = Chunk.GetVoxelOrNeighbor(VX + Step, VY + Step, VZ + Step);
					if ((VA.Density > 0.0f) != (VB.Density > 0.0f))
					{
						EmitQuad(
							CellVertices[CellIndex(CX, CY, CZ)],
							CellVertices[CellIndex(CX, CY + 1, CZ)],
							CellVertices[CellIndex(CX, CY + 1, CZ + 1)],
							CellVertices[CellIndex(CX, CY, CZ + 1)],
							VA.Density > 0.0f,
							OutMesh);
					}
				}

				// Check Y-edge: between (VX+Step, VY, VZ+Step) and (VX+Step, VY+Step, VZ+Step)
				if (CX + 1 < CellCount && CZ + 1 < CellCount)
				{
					const FGeoVoxel& VA = Chunk.GetVoxelOrNeighbor(VX + Step, VY, VZ + Step);
					const FGeoVoxel& VB = Chunk.GetVoxelOrNeighbor(VX + Step, VY + Step, VZ + Step);
					if ((VA.Density > 0.0f) != (VB.Density > 0.0f))
					{
						EmitQuad(
							CellVertices[CellIndex(CX, CY, CZ)],
							CellVertices[CellIndex(CX, CY, CZ + 1)],
							CellVertices[CellIndex(CX + 1, CY, CZ + 1)],
							CellVertices[CellIndex(CX + 1, CY, CZ)],
							VA.Density > 0.0f,
							OutMesh);
					}
				}

				// Check Z-edge: between (VX+Step, VY+Step, VZ) and (VX+Step, VY+Step, VZ+Step)
				if (CX + 1 < CellCount && CY + 1 < CellCount)
				{
					const FGeoVoxel& VA = Chunk.GetVoxelOrNeighbor(VX + Step, VY + Step, VZ);
					const FGeoVoxel& VB = Chunk.GetVoxelOrNeighbor(VX + Step, VY + Step, VZ + Step);
					if ((VA.Density > 0.0f) != (VB.Density > 0.0f))
					{
						EmitQuad(
							CellVertices[CellIndex(CX, CY, CZ)],
							CellVertices[CellIndex(CX + 1, CY, CZ)],
							CellVertices[CellIndex(CX + 1, CY + 1, CZ)],
							CellVertices[CellIndex(CX, CY + 1, CZ)],
							VA.Density > 0.0f,
							OutMesh);
					}
				}
			}
		}
	}

	// ── Step 3: Smooth normals ──────────────────────────────────────────────
	// Average normals at shared vertices for smooth shading on natural surfaces.
	// (Skip if we want sharp edges everywhere — controlled per material later.)
	// For now, use face normals computed from triangle winding.
	for (int32 I = 0; I + 2 < OutMesh.Triangles.Num(); I += 3)
	{
		const int32 I0 = OutMesh.Triangles[I];
		const int32 I1 = OutMesh.Triangles[I + 1];
		const int32 I2 = OutMesh.Triangles[I + 2];

		if (I0 >= OutMesh.Vertices.Num() || I1 >= OutMesh.Vertices.Num() ||
		    I2 >= OutMesh.Vertices.Num())
			continue;

		const FVector& P0 = OutMesh.Vertices[I0];
		const FVector& P1 = OutMesh.Vertices[I1];
		const FVector& P2 = OutMesh.Vertices[I2];

		const FVector FaceNormal = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();

		// Blend face normal with QEF normal (50/50 for natural look)
		if (!FaceNormal.IsNearlyZero())
		{
			OutMesh.Normals[I0] = (OutMesh.Normals[I0] + FaceNormal).GetSafeNormal();
			OutMesh.Normals[I1] = (OutMesh.Normals[I1] + FaceNormal).GetSafeNormal();
			OutMesh.Normals[I2] = (OutMesh.Normals[I2] + FaceNormal).GetSafeNormal();
		}
	}

	OutMesh.bValid = (OutMesh.Vertices.Num() > 0);
}

void FTerraForgeDualContour::GenerateCollisionMesh(
	const FTerraForgeChunk& Chunk,
	FTerraChunkMeshData& OutMesh)
{
	// Collision mesh uses reduced LOD for performance
	GenerateMesh(Chunk, EChunkLOD::Half, OutMesh);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

FVector2D FTerraForgeDualContour::CalculateUV(const FVector& Position, const FVector& Normal)
{
	// Triplanar UV projection based on dominant normal axis
	const FVector AbsN = Normal.GetAbs();
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;

	if (AbsN.Z >= AbsN.X && AbsN.Z >= AbsN.Y)
	{
		// Top/bottom face — project XY
		return FVector2D(Position.X / (VoxelCm * 4.0f), Position.Y / (VoxelCm * 4.0f));
	}
	else if (AbsN.X >= AbsN.Y)
	{
		// Left/right face — project YZ
		return FVector2D(Position.Y / (VoxelCm * 4.0f), Position.Z / (VoxelCm * 4.0f));
	}
	else
	{
		// Front/back face — project XZ
		return FVector2D(Position.X / (VoxelCm * 4.0f), Position.Z / (VoxelCm * 4.0f));
	}
}

FColor FTerraForgeDualContour::MaterialToVertexColor(uint8 Material)
{
	// Vertex color encodes material blend weights for the shader.
	// R = soil/organic weight, G = clay/sediment weight,
	// B = rock weight, A = ore/special weight.

	const EGeoMaterial Mat = (EGeoMaterial)Material;
	switch (Mat)
	{
	// Soil
	case EGeoMaterial::Topsoil:
	case EGeoMaterial::ForestSoil:
	case EGeoMaterial::FertileSoil:
		return FColor(255, 0, 0, 0);    // Pure R — soil

	// Sediment
	case EGeoMaterial::Clay:
	case EGeoMaterial::Sand:
	case EGeoMaterial::Gravel:
		return FColor(0, 255, 0, 0);    // Pure G — sediment

	// Rock
	case EGeoMaterial::Sandstone:
	case EGeoMaterial::Limestone:
	case EGeoMaterial::Slate:
	case EGeoMaterial::Granite:
	case EGeoMaterial::Marble:
	case EGeoMaterial::Basalt:
	case EGeoMaterial::Obsidian:
	case EGeoMaterial::Quartzite:
	case EGeoMaterial::Jade:
	case EGeoMaterial::Onyx:
	case EGeoMaterial::Runestone:
	case EGeoMaterial::Voidrock:
		return FColor(0, 0, 255, 0);    // Pure B — rock

	// Ore
	case EGeoMaterial::OreVein:
		return FColor(0, 0, 0, 255);    // Pure A — ore

	// Special
	case EGeoMaterial::Rubble:
		return FColor(128, 0, 128, 0);  // Mix R+B — rubble

	default:
		return FColor(0, 0, 0, 0);      // Air/water — invisible
	}
}

int32 FTerraForgeDualContour::GetLODStep(EChunkLOD LOD)
{
	switch (LOD)
	{
	case EChunkLOD::Full:    return 1;
	case EChunkLOD::Half:    return 2;
	case EChunkLOD::Quarter: return 4;
	case EChunkLOD::Minimal: return 8;
	default:                 return 1;
	}
}
