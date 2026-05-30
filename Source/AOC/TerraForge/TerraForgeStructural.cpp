// TerraForgeStructural.cpp
// TerraForge — Structural integrity simulation implementation.
// Stress propagation + collapse + landslide mechanics.

#include "TerraForgeStructural.h"
#include "TerraForgeSubsystem.h"
#include "TerraForgeChunk.h"

// ============================================================================
// INITIALIZATION
// ============================================================================

void UTerraForgeStructural::Initialize(UTerraForgeSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogTemp, Log, TEXT("TerraForge Structural: Initialized."));
}

// ============================================================================
// TICK
// ============================================================================

void UTerraForgeStructural::Tick(float DeltaTime)
{
	UpdateTimer += DeltaTime;
	if (UpdateTimer < UpdateInterval) return;
	UpdateTimer = 0.0f;

	// Degrade supports under load
	DegradeSupports(UpdateInterval);

	// Process dirty regions (max 2 per tick for performance)
	int32 Processed = 0;
	while (DirtyRegions.Num() > 0 && Processed < 2)
	{
		const auto Region = DirtyRegions[0];
		DirtyRegions.RemoveAt(0);

		RecalculateRegion(Region.Key, Region.Value);
		Processed++;
	}
}

// ============================================================================
// SUPPORT MANAGEMENT
// ============================================================================

int32 UTerraForgeStructural::PlaceSupport(const FVector& Position, int32 Tier)
{
	FMineSupportData Support;
	Support.Position = Position;
	Support.SupportID = NextSupportID++;
	Support.Tier = Tier;
	Support.TimePlaced = 0.0f;
	Support.StructuralHP = 100.0f;

	// Tier affects radius and max load
	switch (Tier)
	{
	case 1:
		Support.SupportRadius = 300.0f;  // 3m
		Support.MaxLoad = 400.0f;
		break;
	case 2:
		Support.SupportRadius = 500.0f;  // 5m
		Support.MaxLoad = 800.0f;
		break;
	case 3:
		Support.SupportRadius = 700.0f;  // 7m
		Support.MaxLoad = 1500.0f;
		break;
	default:
		Support.SupportRadius = 300.0f;
		Support.MaxLoad = 400.0f;
		break;
	}

	Supports.Add(Support);

	// Mark region as dirty for recalculation
	MarkDirty(Position, Support.SupportRadius);

	UE_LOG(LogTemp, Log, TEXT("TerraForge Structural: Placed Tier %d support #%d at (%.0f, %.0f, %.0f)"),
		Tier, Support.SupportID, Position.X, Position.Y, Position.Z);

	return Support.SupportID;
}

void UTerraForgeStructural::RemoveSupport(int32 SupportID)
{
	for (int32 I = 0; I < Supports.Num(); ++I)
	{
		if (Supports[I].SupportID == SupportID)
		{
			const FVector Pos = Supports[I].Position;
			const float Radius = Supports[I].SupportRadius;

			Supports.RemoveAt(I);

			// Recalculate — removing a support might cause collapse!
			MarkDirty(Pos, Radius);

			UE_LOG(LogTemp, Log, TEXT("TerraForge Structural: Removed support #%d"), SupportID);
			return;
		}
	}
}

TArray<FMineSupportData> UTerraForgeStructural::GetSupportsInRadius(
	const FVector& Center, float Radius) const
{
	TArray<FMineSupportData> Result;
	const float RadiusSq = Radius * Radius;

	for (const auto& Support : Supports)
	{
		if (FVector::DistSquared(Support.Position, Center) <= RadiusSq)
		{
			Result.Add(Support);
		}
	}
	return Result;
}

const FMineSupportData* UTerraForgeStructural::GetSupport(int32 SupportID) const
{
	for (const auto& Support : Supports)
	{
		if (Support.SupportID == SupportID)
		{
			return &Support;
		}
	}
	return nullptr;
}

// ============================================================================
// STRUCTURAL QUERIES
// ============================================================================

EStructuralState UTerraForgeStructural::GetStructuralState(const FVector& Position) const
{
	const float Stress = GetStressAt(Position);

	if (Stress < 0.3f) return EStructuralState::Stable;
	if (Stress < 0.6f) return EStructuralState::Supported;
	if (Stress < 0.85f) return EStructuralState::Warning;
	return EStructuralState::Critical;
}

float UTerraForgeStructural::GetStressAt(const FVector& Position) const
{
	if (!Subsystem) return 0.0f;

	// Not underground = no structural stress
	if (!IsUnderground(Position)) return 0.0f;

	// Convert to voxel coordinates
	const float VoxelSize = 50.0f; // 0.5m
	const FIntVector VoxelCoord(
		FMath::FloorToInt(Position.X / VoxelSize),
		FMath::FloorToInt(Position.Y / VoxelSize),
		FMath::FloorToInt(Position.Z / VoxelSize)
	);

	// Check cache first
	const FIntVector ChunkKey(
		FMath::FloorToInt(VoxelCoord.X / 16.0f),
		FMath::FloorToInt(VoxelCoord.Y / 16.0f),
		FMath::FloorToInt(VoxelCoord.Z / 16.0f)
	);

	const TArray<float>* CachedStress = StressCache.Find(ChunkKey);
	if (CachedStress)
	{
		const int32 LocalX = ((VoxelCoord.X % 16) + 16) % 16;
		const int32 LocalY = ((VoxelCoord.Y % 16) + 16) % 16;
		const int32 LocalZ = ((VoxelCoord.Z % 16) + 16) % 16;
		const int32 Index = LocalX + LocalY * 16 + LocalZ * 256;

		if (Index >= 0 && Index < CachedStress->Num())
		{
			return (*CachedStress)[Index];
		}
	}

	// Calculate on the fly (slower, but correct)
	// Count solid voxels above this position
	int32 SolidAbove = 0;
	const int32 MaxScanHeight = 20; // Scan up to 10m above

	for (int32 Z = 1; Z <= MaxScanHeight; ++Z)
	{
		const FVector ScanPos = Position + FVector(0, 0, Z * VoxelSize);
		const EGeoMaterial Mat = Subsystem->GetSurfaceMaterialAt(ScanPos);
		if (Mat != EGeoMaterial::Air)
		{
			SolidAbove++;
		}
		else
		{
			break; // Hit air, stop counting
		}
	}

	if (SolidAbove == 0) return 0.0f;

	// Check support contribution
	const float SupportFactor = GetSupportContribution(Position);

	// Calculate base stress (more weight above = more stress)
	const float WeightStress = FMath::Min(1.0f, SolidAbove / (float)MaxUnsupportedSpan);

	// Support reduces stress
	const float FinalStress = WeightStress * (1.0f - SupportFactor);

	return FMath::Clamp(FinalStress, 0.0f, 1.0f);
}

bool UTerraForgeStructural::IsUnderground(const FVector& Position) const
{
	if (!Subsystem) return false;

	// Check if there are solid voxels above
	const float VoxelSize = 50.0f;
	for (int32 Z = 1; Z <= 10; ++Z)
	{
		const FVector ScanPos = Position + FVector(0, 0, Z * VoxelSize);
		const EGeoMaterial Mat = Subsystem->GetSurfaceMaterialAt(ScanPos);
		if (Mat != EGeoMaterial::Air)
		{
			return true;
		}
	}
	return false;
}

float UTerraForgeStructural::GetNearestSupportDistance(const FVector& Position) const
{
	float MinDist = MAX_FLT;
	for (const auto& Support : Supports)
	{
		if (!Support.IsValid()) continue;
		const float Dist = FVector::Dist(Position, Support.Position);
		if (Dist < MinDist) MinDist = Dist;
	}
	return MinDist;
}

// ============================================================================
// DIRTY REGION MANAGEMENT
// ============================================================================

void UTerraForgeStructural::MarkDirty(const FVector& Center, float Radius)
{
	// Merge with existing dirty regions if overlapping
	for (auto& Region : DirtyRegions)
	{
		if (FVector::Dist(Region.Key, Center) < Region.Value + Radius)
		{
			// Expand existing region to cover both
			const FVector NewCenter = (Region.Key + Center) * 0.5f;
			const float NewRadius = FVector::Dist(Region.Key, Center) * 0.5f +
				FMath::Max(Region.Value, Radius);
			Region.Key = NewCenter;
			Region.Value = NewRadius;
			return;
		}
	}

	// New dirty region
	DirtyRegions.Add(TPair<FVector, float>(Center, Radius));
}

// ============================================================================
// STRUCTURAL CALCULATION
// ============================================================================

void UTerraForgeStructural::RecalculateRegion(const FVector& Center, float Radius)
{
	if (!Subsystem) return;

	const float VoxelSize = 50.0f;
	const int32 VoxelRadius = FMath::CeilToInt(Radius / VoxelSize);

	// Central voxel coordinate
	const FIntVector CenterVoxel(
		FMath::FloorToInt(Center.X / VoxelSize),
		FMath::FloorToInt(Center.Y / VoxelSize),
		FMath::FloorToInt(Center.Z / VoxelSize)
	);

	bool bAnyCollapse = false;
	FVector CollapseCenter = FVector::ZeroVector;
	int32 CollapseCount = 0;
	EGeoMaterial CollapseMaterial = EGeoMaterial::Granite;

	// Scan each column in the region
	for (int32 X = -VoxelRadius; X <= VoxelRadius; ++X)
	{
		for (int32 Y = -VoxelRadius; Y <= VoxelRadius; ++Y)
		{
			// Check if this column is within radius
			const FVector ColumnWorld = FVector(
				(CenterVoxel.X + X) * VoxelSize + VoxelSize * 0.5f,
				(CenterVoxel.Y + Y) * VoxelSize + VoxelSize * 0.5f,
				Center.Z
			);

			if (FVector::DistSquared2D(Center, ColumnWorld) > Radius * Radius)
				continue;

			// Calculate stress for this column
			const float ColumnStress = CalculateColumnStress(
				FIntVector(CenterVoxel.X + X, CenterVoxel.Y + Y, 0),
				CenterVoxel.Z - VoxelRadius,
				CenterVoxel.Z + VoxelRadius
			);

			// If stress exceeds threshold, trigger collapse
			if (ColumnStress >= 1.0f)
			{
				bAnyCollapse = true;
				CollapseCenter += ColumnWorld;
				CollapseCount++;

				// Get the material for effects
				const EGeoMaterial Mat = Subsystem->GetSurfaceMaterialAt(ColumnWorld);
				if (Mat != EGeoMaterial::Air) CollapseMaterial = Mat;
			}
		}
	}

	// Execute collapse if needed
	if (bAnyCollapse && CollapseCount > 0)
	{
		CollapseCenter /= CollapseCount;
		const float CollapseRadius = FMath::Max(150.0f, CollapseCount * VoxelSize);
		ExecuteCollapse(CollapseCenter, CollapseRadius, CollapseMaterial);
	}

	// Check for surface landslides
	CheckLandslides(Center, Radius);
}

float UTerraForgeStructural::CalculateColumnStress(
	const FIntVector& ColumnXY, int32 MinZ, int32 MaxZ) const
{
	if (!Subsystem) return 0.0f;

	const float VoxelSize = 50.0f;
	float TotalWeight = 0.0f;
	float TotalSupport = 0.0f;
	int32 AirGapSize = 0;
	bool bInAirGap = false;

	// Scan from top to bottom
	for (int32 Z = MaxZ; Z >= MinZ; --Z)
	{
		const FVector WorldPos(
			ColumnXY.X * VoxelSize + VoxelSize * 0.5f,
			ColumnXY.Y * VoxelSize + VoxelSize * 0.5f,
			Z * VoxelSize + VoxelSize * 0.5f
		);

		const EGeoMaterial Mat = Subsystem->GetSurfaceMaterialAt(WorldPos);

		if (Mat == EGeoMaterial::Air)
		{
			if (!bInAirGap && TotalWeight > 0.0f)
			{
				bInAirGap = true;
			}
			if (bInAirGap)
			{
				AirGapSize++;
			}
		}
		else
		{
			if (bInAirGap)
			{
				// We had an air gap — the material above needs support
				TotalWeight += GetMaterialWeight(Mat);
			}
			else
			{
				TotalWeight += GetMaterialWeight(Mat);
			}
			bInAirGap = false;

			// Check for support at this position
			TotalSupport += GetSupportContribution(WorldPos);

			// Bedrock is infinitely strong
			if (Mat == EGeoMaterial::Bedrock)
			{
				TotalSupport = TotalWeight + 1.0f;
				break;
			}
		}
	}

	if (TotalWeight <= 0.0f) return 0.0f;

	// Span penalty: wider unsupported gaps = exponentially more stress
	const float SpanPenalty = AirGapSize > 0 ?
		FMath::Pow(AirGapSize / (float)MaxUnsupportedSpan, 2.0f) : 0.0f;

	// Final stress
	const float BaseStress = (TotalWeight - TotalSupport) / FMath::Max(1.0f, TotalWeight);
	return FMath::Clamp(BaseStress + SpanPenalty, 0.0f, 1.5f);
}

float UTerraForgeStructural::GetSupportContribution(const FVector& VoxelWorldPos) const
{
	float TotalContribution = 0.0f;

	for (const auto& Support : Supports)
	{
		if (!Support.IsValid()) continue;

		const float Dist = FVector::Dist(VoxelWorldPos, Support.Position);
		if (Dist > Support.SupportRadius) continue;

		// Linear falloff from center
		const float Falloff = 1.0f - (Dist / Support.SupportRadius);

		// Scale by support HP
		const float HPFactor = Support.StructuralHP / 100.0f;

		TotalContribution += Falloff * HPFactor * (Support.Tier * 0.5f);
	}

	return FMath::Clamp(TotalContribution, 0.0f, 1.0f);
}

// ============================================================================
// COLLAPSE EXECUTION
// ============================================================================

void UTerraForgeStructural::ExecuteCollapse(const FVector& Center, float Radius, EGeoMaterial Material)
{
	if (!Subsystem) return;

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: COLLAPSE at (%.0f, %.0f, %.0f) radius %.0f!"),
		Center.X, Center.Y, Center.Z, Radius);

	const float VoxelSize = 50.0f;
	const int32 VoxelRadius = FMath::CeilToInt(Radius / VoxelSize);
	int32 CollapsedCount = 0;

	// Fill the air gap with falling material
	const FIntVector CenterVoxel(
		FMath::FloorToInt(Center.X / VoxelSize),
		FMath::FloorToInt(Center.Y / VoxelSize),
		FMath::FloorToInt(Center.Z / VoxelSize)
	);

	for (int32 X = -VoxelRadius; X <= VoxelRadius; ++X)
	{
		for (int32 Y = -VoxelRadius; Y <= VoxelRadius; ++Y)
		{
			// Check if within radius
			const float DistSq = X * X + Y * Y;
			if (DistSq > VoxelRadius * VoxelRadius) continue;

			// Fill the air gap in this column with rubble
			for (int32 Z = -2; Z <= VoxelRadius; ++Z)
			{
				const FVector VoxelPos(
					(CenterVoxel.X + X) * VoxelSize + VoxelSize * 0.5f,
					(CenterVoxel.Y + Y) * VoxelSize + VoxelSize * 0.5f,
					(CenterVoxel.Z + Z) * VoxelSize + VoxelSize * 0.5f
				);

				const EGeoMaterial Mat = Subsystem->GetSurfaceMaterialAt(VoxelPos);
				if (Mat == EGeoMaterial::Air)
				{
					// Fill with collapsed rubble
					Subsystem->SetVoxelMaterial(VoxelPos, Material);
					CollapsedCount++;
				}
			}
		}
	}

	// Destroy any supports in the collapse zone
	for (int32 I = Supports.Num() - 1; I >= 0; --I)
	{
		if (FVector::Dist(Supports[I].Position, Center) < Radius)
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: Support #%d destroyed in collapse!"), Supports[I].SupportID);
			Supports.RemoveAt(I);
		}
	}

	// Broadcast collapse event
	FCollapseEvent Event;
	Event.Center = Center;
	Event.Radius = Radius;
	Event.CollapsedVoxels = CollapsedCount;
	Event.Material = Material;
	OnCollapse.Broadcast(Event);

	// Force mesh regeneration in the affected area
	Subsystem->RegenerateMeshesInRadius(Center, Radius);

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: Collapse filled %d voxels with rubble."), CollapsedCount);
}

// ============================================================================
// LANDSLIDE MECHANICS
// ============================================================================

void UTerraForgeStructural::CheckLandslides(const FVector& Center, float Radius)
{
	if (!Subsystem) return;

	const float VoxelSize = 50.0f;
	const int32 ScanRadius = FMath::CeilToInt(Radius / VoxelSize);

	const FIntVector CenterVoxel(
		FMath::FloorToInt(Center.X / VoxelSize),
		FMath::FloorToInt(Center.Y / VoxelSize),
		0 // Surface-level scan
	);

	// Check each position against its neighbors for height differences
	for (int32 X = -ScanRadius; X <= ScanRadius; ++X)
	{
		for (int32 Y = -ScanRadius; Y <= ScanRadius; ++Y)
		{
			const FVector BasePos(
				(CenterVoxel.X + X) * VoxelSize + VoxelSize * 0.5f,
				(CenterVoxel.Y + Y) * VoxelSize + VoxelSize * 0.5f,
				Center.Z
			);

			// Get the surface height at this position (trace down from sky)
			// Use landscape height as reference
			const float ThisHeight = BasePos.Z; // Simplified — subsystem would provide real height

			// Check 4 cardinal neighbors
			static const FIntVector Neighbors[] = {
				FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
				FIntVector(0, 1, 0), FIntVector(0, -1, 0)
			};

			for (const auto& Offset : Neighbors)
			{
				const FVector NeighborPos(
					(CenterVoxel.X + X + Offset.X) * VoxelSize + VoxelSize * 0.5f,
					(CenterVoxel.Y + Y + Offset.Y) * VoxelSize + VoxelSize * 0.5f,
					Center.Z
				);

				const float NeighborHeight = NeighborPos.Z; // Simplified

				const float HeightDiff = ThisHeight - NeighborHeight;

				if (HeightDiff > LandslideThreshold)
				{
					// Landslide! Material cascades downhill
					const EGeoMaterial SlideMat = Subsystem->GetSurfaceMaterialAt(BasePos);

					// Only soft materials slide (not rock)
					if (SlideMat == EGeoMaterial::Topsoil ||
						SlideMat == EGeoMaterial::ForestSoil ||
						SlideMat == EGeoMaterial::Clay ||
						SlideMat == EGeoMaterial::Sand)
					{
						// Remove from high point
						Subsystem->SetVoxelMaterial(BasePos, EGeoMaterial::Air);

						// Add to low point
						Subsystem->SetVoxelMaterial(NeighborPos, SlideMat);

						// Broadcast landslide event
						FLandslideEvent Event;
						Event.StartPosition = BasePos;
						Event.EndPosition = NeighborPos;
						Event.Volume = 1;
						Event.Material = SlideMat;
						OnLandslide.Broadcast(Event);
					}
				}
			}
		}
	}
}

// ============================================================================
// SUPPORT DEGRADATION
// ============================================================================

void UTerraForgeStructural::DegradeSupports(float DeltaTime)
{
	for (int32 I = Supports.Num() - 1; I >= 0; --I)
	{
		FMineSupportData& Support = Supports[I];
		Support.TimePlaced += DeltaTime;

		// Calculate load on this support
		float Load = 0.0f;

		if (Subsystem)
		{
			const float VoxelSize = 50.0f;
			const int32 ScanRadius = FMath::CeilToInt(Support.SupportRadius / VoxelSize);

			// Count solid voxels above within support radius
			const FIntVector CenterVoxel(
				FMath::FloorToInt(Support.Position.X / VoxelSize),
				FMath::FloorToInt(Support.Position.Y / VoxelSize),
				FMath::FloorToInt(Support.Position.Z / VoxelSize)
			);

			for (int32 X = -ScanRadius; X <= ScanRadius; ++X)
			{
				for (int32 Y = -ScanRadius; Y <= ScanRadius; ++Y)
				{
					const float DistSq = X * X + Y * Y;
					if (DistSq > ScanRadius * ScanRadius) continue;

					// Check voxels above
					for (int32 Z = 1; Z <= 10; ++Z)
					{
						const FVector ScanPos(
							(CenterVoxel.X + X) * VoxelSize + VoxelSize * 0.5f,
							(CenterVoxel.Y + Y) * VoxelSize + VoxelSize * 0.5f,
							(CenterVoxel.Z + Z) * VoxelSize + VoxelSize * 0.5f
						);

						const EGeoMaterial Mat = Subsystem->GetSurfaceMaterialAt(ScanPos);
						if (Mat != EGeoMaterial::Air)
						{
							Load += GetMaterialWeight(Mat);
						}
					}
				}
			}
		}

		Support.CurrentLoad = Load;

		// Degrade under load
		if (Load > 0.0f)
		{
			const float LoadRatio = FMath::Clamp(Load / Support.MaxLoad, 0.0f, 2.0f);
			Support.StructuralHP -= SupportDegradeRate * LoadRatio * DeltaTime;
		}

		// Warning at 30% HP
		if (Support.StructuralHP <= 30.0f && Support.StructuralHP > 0.0f)
		{
			OnSupportWarning.Broadcast(Support);
		}

		// Support destroyed
		if (Support.StructuralHP <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: Support #%d COLLAPSED under load!"), Support.SupportID);
			const FVector Pos = Support.Position;
			const float Radius = Support.SupportRadius;
			Supports.RemoveAt(I);

			// The collapse of a support might cause a chain collapse
			MarkDirty(Pos, Radius);
		}
	}
}

// ============================================================================
// MATERIAL WEIGHT
// ============================================================================

float UTerraForgeStructural::GetMaterialWeight(EGeoMaterial Material) const
{
	// Weight per voxel in arbitrary stress units
	switch (Material)
	{
	case EGeoMaterial::Air:          return 0.0f;
	case EGeoMaterial::Topsoil:      return 1.0f;
	case EGeoMaterial::ForestSoil:   return 1.0f;
	case EGeoMaterial::Clay:         return 1.5f;
	case EGeoMaterial::Sand:         return 1.3f;
	case EGeoMaterial::Sandstone:    return 2.0f;
	case EGeoMaterial::Limestone:    return 2.5f;
	case EGeoMaterial::Slate:        return 2.5f;
	case EGeoMaterial::Granite:      return 3.0f;
	case EGeoMaterial::Basalt:       return 3.5f;
	case EGeoMaterial::Marble:       return 3.0f;
	case EGeoMaterial::Obsidian:     return 3.0f;
	case EGeoMaterial::Quartzite:    return 2.8f;
	case EGeoMaterial::OreVein:      return 4.0f; // Ore is heavy
	case EGeoMaterial::Bedrock:      return 999.0f; // Immovable
	case EGeoMaterial::WaterTable:   return 0.5f;
	default:                          return 1.0f;
	}
}
