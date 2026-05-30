// TerraForgeOreGen.cpp
// TerraForge — Geological ore vein generation implementation.
// All 22 metals with real-world metallurgy values.

#include "TerraForgeOreGen.h"
#include "TerraForgeSubsystem.h"
#include "Math/UnrealMathUtility.h"

// ============================================================================
// INITIALIZATION
// ============================================================================

void UTerraForgeOreGen::Initialize(UTerraForgeSubsystem* InSubsystem, int32 InWorldSeed)
{
	Subsystem = InSubsystem;
	WorldSeed = InWorldSeed;
	InitializeOreRules();

	UE_LOG(LogTemp, Log, TEXT("TerraForge OreGen: Initialized with seed %d, %d ore rules."),
		WorldSeed, OreRules.Num());
}

void UTerraForgeOreGen::InitializeOreRules()
{
	OreRules.Empty();
	OreRules.Reserve(22);

	// Helper lambda for adding rules
	auto AddRule = [this](
		EGeoOreType Type, const FString& Name,
		float MinD, float MaxD,
		TArray<EGeoMaterial> Hosts,
		float Rarity,
		FVector2D Quality,
		FVector2D SmallYield, FVector2D MegaYield,
		FVector2D VeinSize,
		float SmeltTemp, int32 FurnaceTier)
	{
		FOreGenRule Rule;
		Rule.OreType = Type;
		Rule.Name = Name;
		Rule.MinDepth = MinD;
		Rule.MaxDepth = MaxD;
		Rule.HostRocks = MoveTemp(Hosts);
		Rule.Rarity = Rarity;
		Rule.QualityRange = Quality;
		Rule.SmallNodeYield = SmallYield;
		Rule.MegaNodeYield = MegaYield;
		Rule.VeinSizeRange = VeinSize;
		Rule.SmeltingTemp = SmeltTemp;
		Rule.RequiredFurnaceTier = FurnaceTier;
		OreRules.Add(Rule);
	};

	// ══════════════════════════════════════════════════════════════════════
	// ALL 22 METALS — Real metallurgy values
	// ══════════════════════════════════════════════════════════════════════

	// ── Common Tier (Campfire Crucible / Bloomery) ──────────────────────

	AddRule(EGeoOreType::Copper, TEXT("Copper"),
		3.0f, 12.0f,
		{EGeoMaterial::Limestone, EGeoMaterial::Sandstone, EGeoMaterial::Slate},
		0.70f,
		FVector2D(20, 80),
		FVector2D(40, 70), FVector2D(500, 800),
		FVector2D(1.5f, 3.0f),
		1085.0f, 1); // Melts at 1085°C

	AddRule(EGeoOreType::Tin, TEXT("Tin"),
		3.0f, 12.0f,
		{EGeoMaterial::Limestone, EGeoMaterial::Slate, EGeoMaterial::Granite},
		0.60f,
		FVector2D(20, 75),
		FVector2D(35, 60), FVector2D(400, 700),
		FVector2D(1.0f, 2.5f),
		232.0f, 1); // Melts at 232°C — very low

	AddRule(EGeoOreType::Iron, TEXT("Iron"),
		5.0f, 18.0f,
		{EGeoMaterial::Limestone, EGeoMaterial::Granite, EGeoMaterial::Sandstone, EGeoMaterial::Basalt},
		0.65f,
		FVector2D(25, 85),
		FVector2D(40, 70), FVector2D(500, 1000),
		FVector2D(2.0f, 4.0f),
		1538.0f, 2); // Melts at 1538°C — needs bloomery

	AddRule(EGeoOreType::Lead, TEXT("Lead"),
		5.0f, 18.0f,
		{EGeoMaterial::Limestone, EGeoMaterial::Sandstone},
		0.50f,
		FVector2D(20, 70),
		FVector2D(35, 60), FVector2D(400, 700),
		FVector2D(1.0f, 2.5f),
		327.0f, 1); // Melts at 327°C — very low

	AddRule(EGeoOreType::Zinc, TEXT("Zinc"),
		5.0f, 15.0f,
		{EGeoMaterial::Limestone, EGeoMaterial::Sandstone},
		0.45f,
		FVector2D(25, 75),
		FVector2D(30, 55), FVector2D(350, 650),
		FVector2D(1.0f, 2.0f),
		420.0f, 1); // Melts at 420°C

	// ── Moderate Tier (Bloomery / Blast Furnace) ────────────────────────

	AddRule(EGeoOreType::Nickel, TEXT("Nickel"),
		8.0f, 20.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Basalt},
		0.35f,
		FVector2D(30, 80),
		FVector2D(30, 55), FVector2D(350, 600),
		FVector2D(1.0f, 2.5f),
		1455.0f, 2); // Melts at 1455°C

	AddRule(EGeoOreType::Manganese, TEXT("Manganese"),
		8.0f, 22.0f,
		{EGeoMaterial::Limestone, EGeoMaterial::Sandstone, EGeoMaterial::Slate},
		0.40f,
		FVector2D(25, 75),
		FVector2D(30, 55), FVector2D(350, 650),
		FVector2D(1.0f, 2.0f),
		1246.0f, 2); // Melts at 1246°C

	AddRule(EGeoOreType::Antimony, TEXT("Antimony"),
		8.0f, 20.0f,
		{EGeoMaterial::Slate, EGeoMaterial::Quartzite},
		0.30f,
		FVector2D(30, 75),
		FVector2D(25, 50), FVector2D(300, 550),
		FVector2D(0.8f, 2.0f),
		630.0f, 1); // Melts at 630°C

	// ── Uncommon Tier (Blast Furnace) ───────────────────────────────────

	AddRule(EGeoOreType::Silver, TEXT("Silver"),
		8.0f, 25.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Quartzite},
		0.25f,
		FVector2D(35, 85),
		FVector2D(20, 45), FVector2D(250, 500),
		FVector2D(0.8f, 2.0f),
		962.0f, 2); // Melts at 962°C

	AddRule(EGeoOreType::Cobalt, TEXT("Cobalt"),
		10.0f, 25.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Basalt},
		0.25f,
		FVector2D(35, 80),
		FVector2D(20, 45), FVector2D(250, 500),
		FVector2D(0.8f, 1.8f),
		1495.0f, 2); // Melts at 1495°C

	AddRule(EGeoOreType::Bismuth, TEXT("Bismuth"),
		10.0f, 25.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Limestone},
		0.25f,
		FVector2D(30, 75),
		FVector2D(20, 40), FVector2D(250, 450),
		FVector2D(0.7f, 1.5f),
		271.0f, 1); // Melts at 271°C — very low

	AddRule(EGeoOreType::Chromium, TEXT("Chromium"),
		12.0f, 30.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Basalt},
		0.22f,
		FVector2D(35, 82),
		FVector2D(20, 40), FVector2D(250, 500),
		FVector2D(0.8f, 1.8f),
		1907.0f, 3); // Melts at 1907°C — needs blast furnace

	// ── Rare Tier (Blast Furnace / Crucible) ────────────────────────────

	AddRule(EGeoOreType::Gold, TEXT("Gold"),
		10.0f, 30.0f,
		{EGeoMaterial::Quartzite, EGeoMaterial::Granite},
		0.15f,
		FVector2D(40, 90),
		FVector2D(15, 35), FVector2D(200, 400),
		FVector2D(0.5f, 1.5f),
		1064.0f, 2); // Melts at 1064°C

	AddRule(EGeoOreType::Titanium, TEXT("Titanium"),
		12.0f, 30.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Basalt},
		0.15f,
		FVector2D(40, 85),
		FVector2D(15, 35), FVector2D(200, 400),
		FVector2D(0.7f, 1.5f),
		1668.0f, 3); // Melts at 1668°C

	AddRule(EGeoOreType::Tungsten, TEXT("Tungsten"),
		15.0f, 35.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Quartzite},
		0.12f,
		FVector2D(45, 90),
		FVector2D(10, 30), FVector2D(150, 350),
		FVector2D(0.5f, 1.2f),
		3422.0f, 5); // Melts at 3422°C — needs arcane forge!

	AddRule(EGeoOreType::Molybdenum, TEXT("Molybdenum"),
		15.0f, 35.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Quartzite},
		0.12f,
		FVector2D(40, 85),
		FVector2D(10, 30), FVector2D(150, 350),
		FVector2D(0.5f, 1.2f),
		2623.0f, 4); // Melts at 2623°C — needs crucible

	AddRule(EGeoOreType::Vanadium, TEXT("Vanadium"),
		12.0f, 28.0f,
		{EGeoMaterial::Basalt, EGeoMaterial::Granite},
		0.15f,
		FVector2D(35, 82),
		FVector2D(15, 30), FVector2D(180, 380),
		FVector2D(0.6f, 1.3f),
		1910.0f, 3); // Melts at 1910°C

	// ── Very Rare Tier (Crucible Furnace) ───────────────────────────────

	AddRule(EGeoOreType::Platinum, TEXT("Platinum"),
		15.0f, 35.0f,
		{EGeoMaterial::Granite, EGeoMaterial::Basalt},
		0.08f,
		FVector2D(50, 95),
		FVector2D(8, 25), FVector2D(120, 300),
		FVector2D(0.4f, 1.0f),
		1768.0f, 3); // Melts at 1768°C

	// ── Ultra Rare / Fantasy Tier (Arcane Forge) ────────────────────────

	AddRule(EGeoOreType::Mithril, TEXT("Mithril"),
		25.0f, 50.0f,
		{EGeoMaterial::Basalt, EGeoMaterial::Obsidian},
		0.04f,
		FVector2D(60, 100),
		FVector2D(5, 20), FVector2D(100, 250),
		FVector2D(0.3f, 0.8f),
		2800.0f, 5); // Fantasy — arcane forge only

	AddRule(EGeoOreType::Adamantite, TEXT("Adamantite"),
		30.0f, 60.0f,
		{EGeoMaterial::Obsidian, EGeoMaterial::Basalt},
		0.03f,
		FVector2D(65, 100),
		FVector2D(3, 15), FVector2D(80, 200),
		FVector2D(0.3f, 0.7f),
		3200.0f, 5); // Fantasy — arcane forge only

	AddRule(EGeoOreType::Orichalcum, TEXT("Orichalcum"),
		20.0f, 45.0f,
		{EGeoMaterial::Marble, EGeoMaterial::Granite},
		0.05f,
		FVector2D(55, 98),
		FVector2D(5, 18), FVector2D(100, 250),
		FVector2D(0.3f, 0.8f),
		2600.0f, 4); // Fantasy — crucible minimum

	AddRule(EGeoOreType::StarMetal, TEXT("Star Metal"),
		35.0f, 70.0f,
		{EGeoMaterial::Obsidian, EGeoMaterial::Basalt, EGeoMaterial::Granite},
		0.02f,
		FVector2D(70, 100),
		FVector2D(2, 10), FVector2D(50, 150),
		FVector2D(0.2f, 0.5f),
		3500.0f, 5); // Legendary — arcane forge only
}

// ============================================================================
// VEIN GENERATION
// ============================================================================

TArray<FOreVeinInstance> UTerraForgeOreGen::GenerateVeinsForChunk(const FIntVector& ChunkCoord)
{
	TArray<FOreVeinInstance> NewVeins;

	// Skip already generated chunks
	if (GeneratedChunks.Contains(ChunkCoord))
	{
		return NewVeins;
	}
	GeneratedChunks.Add(ChunkCoord);

	const float ChunkWorldSize = 16.0f * 50.0f; // 16 voxels * 50cm = 8m

	// Chunk world position
	const FVector ChunkWorldMin(
		ChunkCoord.X * ChunkWorldSize,
		ChunkCoord.Y * ChunkWorldSize,
		ChunkCoord.Z * ChunkWorldSize
	);

	// Unique seed for this chunk
	const int32 ChunkSeed = WorldSeed ^
		(ChunkCoord.X * 73856093) ^
		(ChunkCoord.Y * 19349663) ^
		(ChunkCoord.Z * 83492791);

	FRandomStream ChunkRNG(ChunkSeed);

	// Try to generate each ore type
	for (const auto& Rule : OreRules)
	{
		// Rarity check — lower rarity = fewer chances per chunk
		const float SpawnChance = Rule.Rarity * 0.3f; // ~20% chance for common ores per chunk
		if (ChunkRNG.FRand() > SpawnChance) continue;

		// Check depth constraint
		// Approximate surface height (simplified — real implementation queries landscape)
		const float SurfaceHeight = 0.0f; // Meters — relative to world origin
		const float ChunkDepthMeters = (SurfaceHeight - ChunkWorldMin.Z / 100.0f);

		if (ChunkDepthMeters < Rule.MinDepth || ChunkDepthMeters > Rule.MaxDepth)
			continue;

		// Check host rock
		const FVector SamplePos = ChunkWorldMin + FVector(
			ChunkRNG.FRandRange(0.0f, ChunkWorldSize),
			ChunkRNG.FRandRange(0.0f, ChunkWorldSize),
			ChunkRNG.FRandRange(0.0f, ChunkWorldSize)
		);

		const EGeoMaterial HostRock = GetHostRockAtDepth(
			ChunkDepthMeters, FVector2D(SamplePos.X, SamplePos.Y));

		if (!IsValidHostRock(Rule, HostRock)) continue;

		// Use 3D noise for natural clustering
		const float NoiseVal = OreNoise3D(
			ChunkCoord.X * 0.1f,
			ChunkCoord.Y * 0.1f,
			ChunkCoord.Z * 0.15f,
			(int32)Rule.OreType
		);

		// Noise threshold — ores cluster, not evenly distributed
		if (NoiseVal < 0.6f) continue;

		// Generate the vein!
		FOreVeinInstance Vein;
		Vein.VeinID = NextVeinID++;
		Vein.OreType = Rule.OreType;
		Vein.Center = SamplePos;
		Vein.Seed = ChunkSeed ^ (int32)Rule.OreType;
		Vein.bIsMegaNode = false;
		Vein.bDiscovered = false;

		// Random quality within range (deeper = higher quality)
		const float DepthFactor = FMath::Clamp(
			(ChunkDepthMeters - Rule.MinDepth) / (Rule.MaxDepth - Rule.MinDepth),
			0.0f, 1.0f);
		Vein.Quality = FMath::Lerp(Rule.QualityRange.X, Rule.QualityRange.Y,
			DepthFactor * 0.7f + ChunkRNG.FRand() * 0.3f);

		// Random yield
		Vein.MaxOre = ChunkRNG.RandRange(
			(int32)Rule.SmallNodeYield.X,
			(int32)Rule.SmallNodeYield.Y);
		Vein.RemainingOre = Vein.MaxOre;

		// Random vein radius
		Vein.VeinRadius = ChunkRNG.FRandRange(
			Rule.VeinSizeRange.X * 100.0f,
			Rule.VeinSizeRange.Y * 100.0f); // Convert to cm

		NewVeins.Add(Vein);
		AllVeins.Add(Vein);

		UE_LOG(LogTemp, Verbose, TEXT("TerraForge OreGen: Spawned %s vein (Q%.0f, %d units) at depth %.1fm"),
			*Rule.Name, Vein.Quality, Vein.MaxOre, ChunkDepthMeters);
	}

	return NewVeins;
}

// ============================================================================
// QUERIES
// ============================================================================

TArray<FOreVeinInstance> UTerraForgeOreGen::GetVeinsInRadius(
	const FVector& Center, float Radius) const
{
	TArray<FOreVeinInstance> Result;
	const float RadiusSq = Radius * Radius;

	for (const auto& Vein : AllVeins)
	{
		if (Vein.IsDepleted()) continue;

		if (FVector::DistSquared(Vein.Center, Center) <= RadiusSq)
		{
			Result.Add(Vein);
		}
	}

	// Also check mega nodes
	for (const auto& Mega : MegaNodes)
	{
		if (Mega.IsDepleted()) continue;

		if (FVector::DistSquared(Mega.Center, Center) <= RadiusSq)
		{
			Result.Add(Mega);
		}
	}

	return Result;
}

const FOreVeinInstance* UTerraForgeOreGen::GetVein(int32 VeinID) const
{
	for (const auto& Vein : AllVeins)
	{
		if (Vein.VeinID == VeinID) return &Vein;
	}
	for (const auto& Mega : MegaNodes)
	{
		if (Mega.VeinID == VeinID) return &Mega;
	}
	return nullptr;
}

int32 UTerraForgeOreGen::ExtractOre(int32 VeinID, int32 SkillLevel)
{
	// Find in regular veins
	for (auto& Vein : AllVeins)
	{
		if (Vein.VeinID == VeinID && !Vein.IsDepleted())
		{
			// Skill affects yield (1 base + skill bonus)
			const int32 BaseYield = 1;
			const int32 BonusYield = SkillLevel / 25; // +1 per 25 skill
			const int32 Yield = FMath::Min(BaseYield + BonusYield, Vein.RemainingOre);

			Vein.RemainingOre -= Yield;
			return Yield;
		}
	}

	// Find in mega nodes
	for (auto& Mega : MegaNodes)
	{
		if (Mega.VeinID == VeinID && !Mega.IsDepleted())
		{
			const int32 BaseYield = 2; // Mega nodes yield more
			const int32 BonusYield = SkillLevel / 20;
			const int32 Yield = FMath::Min(BaseYield + BonusYield, Mega.RemainingOre);

			Mega.RemainingOre -= Yield;
			return Yield;
		}
	}

	return 0;
}

void UTerraForgeOreGen::DiscoverVein(int32 VeinID)
{
	for (auto& Vein : AllVeins)
	{
		if (Vein.VeinID == VeinID)
		{
			Vein.bDiscovered = true;
			UE_LOG(LogTemp, Log, TEXT("TerraForge: Discovered %s vein #%d (Q%.0f)!"),
				*UEnum::GetValueAsString(Vein.OreType), VeinID, Vein.Quality);
			return;
		}
	}
}

// ============================================================================
// MEGA NODE SYSTEM
// ============================================================================

FOreVeinInstance UTerraForgeOreGen::SpawnMegaNode()
{
	// Pick a random ore type (weighted by rarity — rarer ores less likely)
	FRandomStream RNG(FMath::Rand());

	float TotalWeight = 0.0f;
	for (const auto& Rule : OreRules)
	{
		TotalWeight += Rule.Rarity;
	}

	float Roll = RNG.FRandRange(0.0f, TotalWeight);
	const FOreGenRule* ChosenRule = &OreRules[0];
	for (const auto& Rule : OreRules)
	{
		Roll -= Rule.Rarity;
		if (Roll <= 0.0f)
		{
			ChosenRule = &Rule;
			break;
		}
	}

	// Random world position for mega node
	// Surface visible — these are world events
	const float WorldRadius = 50000.0f; // 500m radius from origin
	const FVector Position(
		RNG.FRandRange(-WorldRadius, WorldRadius),
		RNG.FRandRange(-WorldRadius, WorldRadius),
		0.0f // Surface level — adjusted by landscape height at runtime
	);

	FOreVeinInstance Mega;
	Mega.VeinID = NextVeinID++;
	Mega.OreType = ChosenRule->OreType;
	Mega.Center = Position;
	Mega.bIsMegaNode = true;
	Mega.bDiscovered = true; // Mega nodes are always visible
	Mega.Seed = RNG.RandRange(0, INT32_MAX);

	// Mega yield
	Mega.MaxOre = RNG.RandRange(
		(int32)ChosenRule->MegaNodeYield.X,
		(int32)ChosenRule->MegaNodeYield.Y);
	Mega.RemainingOre = Mega.MaxOre;

	// Higher quality than small nodes
	Mega.Quality = RNG.FRandRange(
		ChosenRule->QualityRange.Y * 0.8f,
		FMath::Min(ChosenRule->QualityRange.Y * 1.2f, 100.0f));

	// Larger vein radius
	Mega.VeinRadius = RNG.FRandRange(
		ChosenRule->VeinSizeRange.Y * 150.0f,
		ChosenRule->VeinSizeRange.Y * 300.0f);

	MegaNodes.Add(Mega);

	UE_LOG(LogTemp, Log, TEXT("TerraForge: MEGA NODE spawned! %s (Q%.0f, %d units) at (%.0f, %.0f)"),
		*ChosenRule->Name, Mega.Quality, Mega.MaxOre, Position.X, Position.Y);

	return Mega;
}

void UTerraForgeOreGen::RemoveMegaNode(int32 VeinID)
{
	for (int32 I = MegaNodes.Num() - 1; I >= 0; --I)
	{
		if (MegaNodes[I].VeinID == VeinID)
		{
			UE_LOG(LogTemp, Log, TEXT("TerraForge: Mega node #%d removed."), VeinID);
			MegaNodes.RemoveAt(I);
			return;
		}
	}
}

TArray<FOreVeinInstance> UTerraForgeOreGen::GetActiveMegaNodes() const
{
	TArray<FOreVeinInstance> Active;
	for (const auto& Mega : MegaNodes)
	{
		if (!Mega.IsDepleted())
		{
			Active.Add(Mega);
		}
	}
	return Active;
}

// ============================================================================
// NOISE & GEOLOGICAL MODEL
// ============================================================================

float UTerraForgeOreGen::OreNoise3D(float X, float Y, float Z, int32 OctaveSeed) const
{
	// Simple hash-based noise (deterministic, no external dependencies)
	auto Hash = [](int32 A, int32 B, int32 C, int32 Seed) -> float
	{
		int32 H = Seed;
		H ^= A * 73856093;
		H ^= B * 19349663;
		H ^= C * 83492791;
		H = (H ^ (H >> 13)) * 1274126177;
		H = H ^ (H >> 16);
		return (H & 0x7FFFFFFF) / (float)0x7FFFFFFF;
	};

	const int32 IX = FMath::FloorToInt(X);
	const int32 IY = FMath::FloorToInt(Y);
	const int32 IZ = FMath::FloorToInt(Z);

	const float FX = X - IX;
	const float FY = Y - IY;
	const float FZ = Z - IZ;

	// Smoothstep
	const float SX = FX * FX * (3.0f - 2.0f * FX);
	const float SY = FY * FY * (3.0f - 2.0f * FY);
	const float SZ = FZ * FZ * (3.0f - 2.0f * FZ);

	// Trilinear interpolation of 8 corner hashes
	const float C000 = Hash(IX,     IY,     IZ,     OctaveSeed);
	const float C100 = Hash(IX + 1, IY,     IZ,     OctaveSeed);
	const float C010 = Hash(IX,     IY + 1, IZ,     OctaveSeed);
	const float C110 = Hash(IX + 1, IY + 1, IZ,     OctaveSeed);
	const float C001 = Hash(IX,     IY,     IZ + 1, OctaveSeed);
	const float C101 = Hash(IX + 1, IY,     IZ + 1, OctaveSeed);
	const float C011 = Hash(IX,     IY + 1, IZ + 1, OctaveSeed);
	const float C111 = Hash(IX + 1, IY + 1, IZ + 1, OctaveSeed);

	const float X00 = FMath::Lerp(C000, C100, SX);
	const float X10 = FMath::Lerp(C010, C110, SX);
	const float X01 = FMath::Lerp(C001, C101, SX);
	const float X11 = FMath::Lerp(C011, C111, SX);

	const float Y0 = FMath::Lerp(X00, X10, SY);
	const float Y1 = FMath::Lerp(X01, X11, SY);

	return FMath::Lerp(Y0, Y1, SZ);
}

EGeoMaterial UTerraForgeOreGen::GetHostRockAtDepth(
	float DepthMeters, const FVector2D& WorldXY) const
{
	// Simplified geological model:
	// 0-2m: Topsoil / Forest Soil
	// 2-5m: Clay / Sand (varies by region)
	// 5-15m: Sandstone / Limestone
	// 15-30m: Granite / Slate
	// 30-50m: Granite / Basalt
	// 50m+: Basalt / Obsidian (deep volcanic)

	// Add noise for variation
	const float RegionNoise = OreNoise3D(
		WorldXY.X * 0.001f, WorldXY.Y * 0.001f, DepthMeters * 0.05f, WorldSeed + 999);

	if (DepthMeters < 2.0f)
		return RegionNoise > 0.5f ? EGeoMaterial::Topsoil : EGeoMaterial::ForestSoil;
	if (DepthMeters < 5.0f)
		return RegionNoise > 0.5f ? EGeoMaterial::Clay : EGeoMaterial::Sand;
	if (DepthMeters < 15.0f)
	{
		if (RegionNoise < 0.33f) return EGeoMaterial::Sandstone;
		if (RegionNoise < 0.66f) return EGeoMaterial::Limestone;
		return EGeoMaterial::Slate;
	}
	if (DepthMeters < 30.0f)
	{
		if (RegionNoise < 0.4f) return EGeoMaterial::Granite;
		if (RegionNoise < 0.7f) return EGeoMaterial::Slate;
		return EGeoMaterial::Quartzite;
	}
	if (DepthMeters < 50.0f)
	{
		return RegionNoise > 0.5f ? EGeoMaterial::Granite : EGeoMaterial::Basalt;
	}

	// Deep volcanic
	if (RegionNoise > 0.7f) return EGeoMaterial::Obsidian;
	return EGeoMaterial::Basalt;
}

bool UTerraForgeOreGen::IsValidHostRock(const FOreGenRule& Rule, EGeoMaterial Rock) const
{
	return Rule.HostRocks.Contains(Rock);
}

TArray<FIntVector> UTerraForgeOreGen::GenerateVeinShape(int32 Seed, float RadiusMeters) const
{
	TArray<FIntVector> Shape;
	FRandomStream RNG(Seed);

	const int32 VoxelRadius = FMath::CeilToInt(RadiusMeters * 2.0f); // 0.5m voxels

	// Irregular ellipsoid shape with noise perturbation
	const float StretchX = RNG.FRandRange(0.5f, 1.5f);
	const float StretchY = RNG.FRandRange(0.5f, 1.5f);
	const float StretchZ = RNG.FRandRange(0.3f, 0.8f); // Veins are flatter vertically

	for (int32 X = -VoxelRadius; X <= VoxelRadius; ++X)
	{
		for (int32 Y = -VoxelRadius; Y <= VoxelRadius; ++Y)
		{
			for (int32 Z = -VoxelRadius; Z <= VoxelRadius; ++Z)
			{
				const float NormX = X / (float)VoxelRadius * StretchX;
				const float NormY = Y / (float)VoxelRadius * StretchY;
				const float NormZ = Z / (float)VoxelRadius * StretchZ;

				const float Dist = FMath::Sqrt(NormX * NormX + NormY * NormY + NormZ * NormZ);

				// Add noise for irregular edges
				const float Noise = OreNoise3D(X * 0.3f, Y * 0.3f, Z * 0.3f, Seed);
				const float Threshold = 1.0f + (Noise - 0.5f) * 0.4f;

				if (Dist < Threshold)
				{
					Shape.Add(FIntVector(X, Y, Z));
				}
			}
		}
	}

	return Shape;
}
