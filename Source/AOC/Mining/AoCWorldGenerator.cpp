// AoCWorldGenerator.cpp
// Architect of Creation - World Generator Implementation

#include "AoCWorldGenerator.h"
#include "AoCVoxelWorld.h"
#include "AoCVoxelChunk.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeHeightfieldCollisionComponent.h"

AAoCWorldGenerator::AAoCWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	WorldSeed = 12345;
	MapSizeChunks = 64;
	MaxDepthChunks = 8;

	OreNoiseFrequency = 0.02f;
	OreNoiseOctaves = 3;
	TerrainNoiseFrequency = 0.01f;

	VoxelWorld = nullptr;

	InitializeDefaultOreTable();
}

void AAoCWorldGenerator::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find VoxelWorld if not manually set
	if (!VoxelWorld)
	{
		VoxelWorld = Cast<AAoCVoxelWorld>(UGameplayStatics::GetActorOfClass(GetWorld(), AAoCVoxelWorld::StaticClass()));
	}

	if (VoxelWorld)
	{
		ApplyToVoxelWorld();
	}
}

// ─── Core Functions ──────────────────────────────────────────────────────────

void AAoCWorldGenerator::ApplyToVoxelWorld()
{
	if (!VoxelWorld) return;

	VoxelWorld->WorldSeed = WorldSeed;
	VoxelWorld->RenderDistance = MAX_RENDER_DISTANCE;
}

void AAoCWorldGenerator::GenerateChunkData(FIntVector ChunkCoord, TArray<FVoxelData>& OutVoxels, TArray<FVeinData>& OutVeins)
{
	OutVoxels.SetNum(CHUNK_VOLUME);
	OutVeins.Empty();

	FRandomStream ChunkRand = GetChunkRandom(ChunkCoord);

	for (int32 Z = 0; Z < CHUNK_SIZE; ++Z)
	{
		for (int32 Y = 0; Y < CHUNK_SIZE; ++Y)
		{
			for (int32 X = 0; X < CHUNK_SIZE; ++X)
			{
				const int32 Index = AoCVoxelUtils::VoxelIndex(X, Y, Z);
				const FVector WorldPos = AoCVoxelUtils::ChunkLocalToWorld(ChunkCoord, X, Y, Z);
				const float SurfaceH = GetSurfaceHeight(WorldPos.X, WorldPos.Y);
				const float DepthCm = SurfaceH - WorldPos.Z;

				// Above surface = air
				if (DepthCm <= 0.f)
				{
					OutVoxels[Index] = FVoxelData(EVoxelMaterial::Air, 0.f, 0);
					continue;
				}

				const float DepthMeters = DepthCm / 100.f;

				// Check for ore first (ore overrides soil/rock)
				FOreRarityEntry OreEntry;
				uint8 OreQuality = 0;
				if (DepthMeters >= SoilConfig.RockTransitionDepthMeters && SampleOreAt(WorldPos, OreEntry, OreQuality))
				{
					// Add slight density variation for smooth surfaces
					const float DensityNoise = SampleNoise3D(WorldPos, TerrainNoiseFrequency * 2.f, WorldSeed + 999);
					const float Density = FMath::Clamp(0.7f + DensityNoise * 0.3f, 0.5f, 1.f);
					OutVoxels[Index] = FVoxelData(OreEntry.OreMaterial, Density, OreQuality);
					continue;
				}

				// Determine soil/rock material by depth
				uint8 SoilQuality = 0;
				const EVoxelMaterial Mat = GetMaterialAtDepth(WorldPos, DepthMeters, SoilQuality);

				// Density: solid underground with slight noise for organic surface transitions
				float Density = 1.f;
				if (DepthMeters < SoilConfig.RockTransitionDepthMeters)
				{
					// Softer transition near surface
					const float SurfaceNoise = SampleNoise3D(WorldPos, TerrainNoiseFrequency, WorldSeed + 500);
					Density = FMath::Clamp(0.6f + DepthMeters / SoilConfig.RockTransitionDepthMeters * 0.4f + SurfaceNoise * 0.1f, 0.f, 1.f);
				}

				OutVoxels[Index] = FVoxelData(Mat, Density, SoilQuality);
			}
		}
	}

	// Generate ore veins for this chunk (separate pass for larger coherent veins)
	for (const FOreRarityEntry& Entry : OreRarityTable)
	{
		if (Entry.Frequency <= 0.f) continue;

		// Deterministic vein count per chunk based on frequency
		const float VeinChance = Entry.Frequency * 0.5f; // Scaled so freq 1.0 ≈ 0.5 veins per chunk
		const float Roll = ChunkRand.FRand();

		if (Roll < VeinChance)
		{
			// Place a vein in this chunk
			const float VX = ChunkRand.FRandRange(0.f, CHUNK_WORLD_SIZE);
			const float VY = ChunkRand.FRandRange(0.f, CHUNK_WORLD_SIZE);
			const float VZ = ChunkRand.FRandRange(0.f, CHUNK_WORLD_SIZE);

			const FVector VeinWorldPos = FVector(
				ChunkCoord.X * CHUNK_WORLD_SIZE + VX,
				ChunkCoord.Y * CHUNK_WORLD_SIZE + VY,
				ChunkCoord.Z * CHUNK_WORLD_SIZE + VZ
			);

			// Check depth requirement
			const float VeinSurfaceH = GetSurfaceHeight(VeinWorldPos.X, VeinWorldPos.Y);
			const float VeinDepthM = (VeinSurfaceH - VeinWorldPos.Z) / 100.f;

			if (VeinDepthM < Entry.MinDepthMeters) continue;
			if (Entry.MaxDepthMeters > 0.f && VeinDepthM > Entry.MaxDepthMeters) continue;

			const float Radius = ChunkRand.FRandRange(Entry.MinVeinRadiusMeters, Entry.MaxVeinRadiusMeters) * 100.f; // Convert to cm
			const int32 OreAmount = ChunkRand.RandRange(Entry.MinOrePerVein, Entry.MaxOrePerVein);
			const uint8 Quality = static_cast<uint8>(ChunkRand.RandRange(Entry.MinQuality, Entry.MaxQuality));

			FVeinData Vein(VeinWorldPos, Radius, Entry.OreMaterial, Quality, OreAmount);
			OutVeins.Add(Vein);
		}
	}
}

EVoxelMaterial AAoCWorldGenerator::GetMaterialAtDepth(FVector WorldPos, float DepthMeters, uint8& OutQuality) const
{
	// Surface noise for biome variation
	const float BiomeNoise = SampleNoise3D(FVector(WorldPos.X, WorldPos.Y, 0.f), 0.005f, WorldSeed + 100);

	// Check for forest soil (under forested areas, below topsoil)
	if (IsForested(WorldPos) && DepthMeters > SoilConfig.TopsoilDepthMeters
		&& DepthMeters <= SoilConfig.TopsoilDepthMeters + SoilConfig.ForestSoilDepthMeters)
	{
		const float QualityNoise = SampleNoise3D(WorldPos, 0.01f, WorldSeed + 200);
		const int32 BaseQ = SoilConfig.BaseForestSoilQuality;
		const int32 Variation = SoilConfig.QualityVariation;
		OutQuality = static_cast<uint8>(FMath::Clamp(BaseQ + FMath::RoundToInt((QualityNoise - 0.5f) * 2.f * Variation), 0, 100));
		return EVoxelMaterial::ForestSoil;
	}

	// Clay near water
	if (IsNearWater(WorldPos) && DepthMeters > SoilConfig.TopsoilDepthMeters
		&& DepthMeters <= SoilConfig.TopsoilDepthMeters + SoilConfig.ClayDepthMeters)
	{
		OutQuality = static_cast<uint8>(FMath::Clamp(50 + FMath::RoundToInt((BiomeNoise - 0.5f) * 30.f), 0, 100));
		return EVoxelMaterial::Clay;
	}

	// Sand at coastlines (biome noise driven)
	if (BiomeNoise < 0.2f && DepthMeters <= SoilConfig.SandDepthMeters)
	{
		OutQuality = static_cast<uint8>(FMath::Clamp(40 + FMath::RoundToInt(BiomeNoise * 60.f), 0, 100));
		return EVoxelMaterial::Sand;
	}

	// Regular topsoil
	if (DepthMeters <= SoilConfig.TopsoilDepthMeters)
	{
		const float QualityNoise = SampleNoise3D(WorldPos, 0.01f, WorldSeed + 300);
		const int32 BaseQ = SoilConfig.BaseTopsoilQuality;
		const int32 Variation = SoilConfig.QualityVariation;
		OutQuality = static_cast<uint8>(FMath::Clamp(BaseQ + FMath::RoundToInt((QualityNoise - 0.5f) * 2.f * Variation), 0, 100));
		return EVoxelMaterial::Soil;
	}

	// Fertile soil (transition layer between topsoil and rock)
	if (DepthMeters <= SoilConfig.RockTransitionDepthMeters)
	{
		OutQuality = static_cast<uint8>(FMath::Clamp(30 + FMath::RoundToInt(BiomeNoise * 40.f), 0, 100));
		return EVoxelMaterial::FertileSoil;
	}

	// Deep rock
	OutQuality = 0;
	return EVoxelMaterial::Rock;
}

bool AAoCWorldGenerator::SampleOreAt(FVector WorldPos, FOreRarityEntry& OutOreEntry, uint8& OutQuality) const
{
	for (int32 i = 0; i < OreRarityTable.Num(); ++i)
	{
		const FOreRarityEntry& Entry = OreRarityTable[i];
		if (Entry.Frequency <= 0.f) continue;

		// Each ore type gets its own noise channel (seeded differently)
		const int32 OreSeed = WorldSeed + static_cast<int32>(Entry.OreMaterial) * 7919; // Prime offset per ore
		const float NoiseVal = SampleNoiseMultiOctave(WorldPos, OreNoiseFrequency, OreNoiseOctaves, OreSeed);

		// Threshold: higher frequency = lower threshold = more common
		const float Threshold = 1.f - (Entry.Frequency * 0.15f); // Freq 1.0 → threshold 0.85

		if (NoiseVal > Threshold)
		{
			OutOreEntry = Entry;

			// Quality varies within the vein based on position noise
			const float QNoise = SampleNoise3D(WorldPos, 0.05f, OreSeed + 1000);
			const int32 QRange = Entry.MaxQuality - Entry.MinQuality;
			OutQuality = static_cast<uint8>(FMath::Clamp(
				Entry.MinQuality + FMath::RoundToInt(QNoise * QRange),
				0, 100));

			return true;
		}
	}

	return false;
}

const FOreRarityEntry* AAoCWorldGenerator::GetOreEntry(EVoxelMaterial Material) const
{
	for (const FOreRarityEntry& Entry : OreRarityTable)
	{
		if (Entry.OreMaterial == Material)
		{
			return &Entry;
		}
	}
	return nullptr;
}

void AAoCWorldGenerator::RandomizeSeed()
{
	WorldSeed = FMath::RandRange(1, 999999);
}

int32 AAoCWorldGenerator::GetOreTypeCount() const
{
	return OreRarityTable.Num();
}

// ─── Editor Preview ──────────────────────────────────────────────────────────

#if WITH_EDITOR
void AAoCWorldGenerator::GeneratePreview()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Generate veins for a 4×4×4 chunk area around the generator's position
	const FIntVector CenterChunk = AoCVoxelUtils::WorldToChunkCoord(GetActorLocation());

	for (int32 DX = -2; DX <= 2; ++DX)
	{
		for (int32 DY = -2; DY <= 2; ++DY)
		{
			for (int32 DZ = -2; DZ <= 0; ++DZ)
			{
				const FIntVector ChunkCoord(CenterChunk.X + DX, CenterChunk.Y + DY, CenterChunk.Z + DZ);

				TArray<FVoxelData> Voxels;
				TArray<FVeinData> Veins;
				GenerateChunkData(ChunkCoord, Voxels, Veins);

				// Draw debug spheres for each vein
				for (const FVeinData& Vein : Veins)
				{
					FColor DebugColor;
					switch (static_cast<uint8>(Vein.Material) % 6)
					{
					case 0: DebugColor = FColor::Red; break;
					case 1: DebugColor = FColor::Green; break;
					case 2: DebugColor = FColor::Blue; break;
					case 3: DebugColor = FColor::Yellow; break;
					case 4: DebugColor = FColor::Cyan; break;
					default: DebugColor = FColor::Magenta; break;
					}

					DrawDebugSphere(World, Vein.Center, Vein.Radius, 16, DebugColor, false, 10.f, 0, 2.f);

					// Draw label with ore name
					const UEnum* MatEnum = StaticEnum<EVoxelMaterial>();
					const FString MatName = MatEnum ? MatEnum->GetDisplayNameTextByValue(static_cast<int64>(Vein.Material)).ToString() : TEXT("Unknown");
					DrawDebugString(World, Vein.Center + FVector(0, 0, Vein.Radius + 50.f),
						FString::Printf(TEXT("%s Q:%d Ore:%d"), *MatName, Vein.Quality, Vein.RemainingOre),
						nullptr, DebugColor, 10.f, true);
				}
			}
		}
	}
}

void AAoCWorldGenerator::ClearPreview()
{
	FlushPersistentDebugLines(GetWorld());
}
#endif

// ─── Private Helpers ─────────────────────────────────────────────────────────

void AAoCWorldGenerator::InitializeDefaultOreTable()
{
	OreRarityTable.Empty();

	// Tier 1 — Common
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreCopper, TEXT("Copper"), 0.80f, 3.f, 0.f, 1.5f, 4.f, 80, 300, 20, 80, 1));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreTin, TEXT("Tin"), 0.70f, 3.f, 0.f, 1.5f, 3.5f, 60, 250, 20, 75, 1));

	// Tier 2 — Common
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreIron, TEXT("Iron"), 0.85f, 5.f, 0.f, 2.f, 5.f, 100, 400, 15, 85, 2));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreZinc, TEXT("Zinc"), 0.50f, 4.f, 0.f, 1.f, 3.f, 50, 200, 20, 70, 2));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreLead, TEXT("Lead"), 0.55f, 3.f, 0.f, 1.5f, 3.5f, 60, 250, 25, 75, 2));

	// Tier 3 — Uncommon
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreNickel, TEXT("Nickel"), 0.30f, 8.f, 0.f, 1.f, 2.5f, 40, 150, 15, 70, 3));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreSilver, TEXT("Silver"), 0.20f, 10.f, 0.f, 0.8f, 2.f, 30, 100, 25, 80, 3));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreGold, TEXT("Gold"), 0.15f, 12.f, 0.f, 0.6f, 1.5f, 20, 80, 30, 90, 3));

	// Tier 4 — Rare
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreChromium, TEXT("Chromium"), 0.12f, 15.f, 0.f, 0.8f, 2.f, 25, 100, 20, 75, 4));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreCobalt, TEXT("Cobalt"), 0.10f, 15.f, 0.f, 0.7f, 1.8f, 20, 80, 20, 75, 4));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreManganese, TEXT("Manganese"), 0.18f, 10.f, 0.f, 1.f, 2.5f, 30, 120, 15, 70, 4));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreMolybdenum, TEXT("Molybdenum"), 0.08f, 20.f, 0.f, 0.6f, 1.5f, 15, 60, 25, 80, 4));

	// Tier 5 — Very Rare
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreTitanium, TEXT("Titanium"), 0.06f, 25.f, 0.f, 0.5f, 1.2f, 10, 50, 30, 85, 5));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreTungsten, TEXT("Tungsten"), 0.04f, 30.f, 0.f, 0.5f, 1.0f, 10, 40, 35, 90, 5));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreVanadium, TEXT("Vanadium"), 0.05f, 25.f, 0.f, 0.5f, 1.2f, 10, 45, 30, 85, 5));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OrePlatinum, TEXT("Platinum"), 0.03f, 30.f, 0.f, 0.4f, 1.0f, 8, 35, 40, 95, 5));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OrePalladium, TEXT("Palladium"), 0.04f, 28.f, 0.f, 0.4f, 1.0f, 10, 40, 35, 90, 5));

	// Tier 6 — Extremely Rare
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreRhodium, TEXT("Rhodium"), 0.02f, 35.f, 0.f, 0.3f, 0.8f, 5, 25, 40, 95, 6));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreIridium, TEXT("Iridium"), 0.015f, 40.f, 0.f, 0.3f, 0.7f, 5, 20, 45, 100, 6));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreNiobium, TEXT("Niobium"), 0.02f, 35.f, 0.f, 0.3f, 0.8f, 5, 25, 40, 95, 6));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreTantalum, TEXT("Tantalum"), 0.015f, 40.f, 0.f, 0.3f, 0.7f, 5, 20, 45, 100, 6));
	OreRarityTable.Add(FOreRarityEntry(EVoxelMaterial::OreOsmium, TEXT("Osmium"), 0.01f, 45.f, 0.f, 0.3f, 0.6f, 3, 15, 50, 100, 6));
}

FRandomStream AAoCWorldGenerator::GetChunkRandom(FIntVector ChunkCoord) const
{
	const int32 Seed = WorldSeed ^ (ChunkCoord.X * 73856093) ^ (ChunkCoord.Y * 19349663) ^ (ChunkCoord.Z * 83492791);
	return FRandomStream(Seed);
}

float AAoCWorldGenerator::SampleNoise3D(FVector WorldPos, float Frequency, int32 Seed) const
{
	// FMath::PerlinNoise3D returns [-1, 1], remap to [0, 1]
	const FVector NoisePos = FVector(
		WorldPos.X * Frequency + Seed * 0.1f,
		WorldPos.Y * Frequency + Seed * 0.07f,
		WorldPos.Z * Frequency + Seed * 0.13f
	);
	return (FMath::PerlinNoise3D(NoisePos) + 1.f) * 0.5f;
}

float AAoCWorldGenerator::SampleNoiseMultiOctave(FVector WorldPos, float Frequency, int32 Octaves, int32 Seed) const
{
	float Total = 0.f;
	float Amplitude = 1.f;
	float MaxValue = 0.f;
	float Freq = Frequency;

	for (int32 i = 0; i < Octaves; ++i)
	{
		Total += SampleNoise3D(WorldPos, Freq, Seed + i * 31) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= 0.5f;
		Freq *= 2.f;
	}

	return (MaxValue > 0.f) ? (Total / MaxValue) : 0.f;
}

bool AAoCWorldGenerator::IsNearWater(FVector WorldPos) const
{
	// Use noise to simulate water proximity (rivers, lakes)
	// In a full implementation, this would query actual water body actors
	const float WaterNoise = SampleNoise3D(FVector(WorldPos.X, WorldPos.Y, 0.f), 0.003f, WorldSeed + 400);
	return WaterNoise < 0.25f;
}

bool AAoCWorldGenerator::IsForested(FVector WorldPos) const
{
	// Use noise to simulate forest density
	// In a full implementation, this would query foliage density or biome data
	const float ForestNoise = SampleNoise3D(FVector(WorldPos.X, WorldPos.Y, 0.f), 0.004f, WorldSeed + 600);
	return ForestNoise > 0.55f;
}

float AAoCWorldGenerator::GetSurfaceHeight(float WorldX, float WorldY) const
{
	// Try to find landscape actor and get height
	UWorld* World = GetWorld();
	if (World)
	{
		// Trace downward from high up to find landscape surface
		FHitResult Hit;
		const FVector TraceStart(WorldX, WorldY, 50000.f);
		const FVector TraceEnd(WorldX, WorldY, -50000.f);

		FCollisionQueryParams Params;
		Params.bTraceComplex = false;

		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			return Hit.ImpactPoint.Z;
		}
	}

	// Default surface height if no landscape found (sea level)
	return 0.f;
}
