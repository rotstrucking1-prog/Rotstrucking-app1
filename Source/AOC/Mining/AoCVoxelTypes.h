// AoCVoxelTypes.h
// Architect of Creation - Voxel Terrain Shared Types
// Contains enums, structs, and constants for the voxel terrain and mining systems.

#pragma once

#include "CoreMinimal.h"
#include "AoCVoxelTypes.generated.h"

// ─── Constants ───────────────────────────────────────────────────────────────

/** Number of voxels per chunk axis (32×32×32). */
static constexpr int32 CHUNK_SIZE = 32;

/** Size of a single voxel in centimeters (0.5m = 50cm). */
static constexpr float VOXEL_SIZE = 50.f;

/** Maximum render distance in chunks. */
static constexpr int32 MAX_RENDER_DISTANCE = 3;

/** Total voxels per chunk (32^3 = 32768). */
static constexpr int32 CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

/** World-space size of a chunk in centimeters (32 × 50 = 1600cm = 16m). */
static constexpr float CHUNK_WORLD_SIZE = CHUNK_SIZE * VOXEL_SIZE;

// ─── Voxel Material Enum ─────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EVoxelMaterial : uint8
{
	Air				UMETA(DisplayName = "Air"),
	Soil			UMETA(DisplayName = "Soil"),
	FertileSoil		UMETA(DisplayName = "Fertile Soil"),
	ForestSoil		UMETA(DisplayName = "Forest Soil"),
	Clay			UMETA(DisplayName = "Clay"),
	Sand			UMETA(DisplayName = "Sand"),
	Rock			UMETA(DisplayName = "Rock"),

	// Tier 1 — Common
	OreCopper		UMETA(DisplayName = "Copper Ore"),
	OreTin			UMETA(DisplayName = "Tin Ore"),

	// Tier 2 — Common
	OreIron			UMETA(DisplayName = "Iron Ore"),
	OreZinc			UMETA(DisplayName = "Zinc Ore"),
	OreLead			UMETA(DisplayName = "Lead Ore"),

	// Tier 3 — Uncommon
	OreNickel		UMETA(DisplayName = "Nickel Ore"),
	OreSilver		UMETA(DisplayName = "Silver Ore"),
	OreGold			UMETA(DisplayName = "Gold Ore"),

	// Tier 4 — Rare
	OreChromium		UMETA(DisplayName = "Chromium Ore"),
	OreCobalt		UMETA(DisplayName = "Cobalt Ore"),
	OreManganese	UMETA(DisplayName = "Manganese Ore"),
	OreMolybdenum	UMETA(DisplayName = "Molybdenum Ore"),

	// Tier 5 — Very Rare
	OreTitanium		UMETA(DisplayName = "Titanium Ore"),
	OreTungsten		UMETA(DisplayName = "Tungsten Ore"),
	OreVanadium		UMETA(DisplayName = "Vanadium Ore"),
	OrePlatinum		UMETA(DisplayName = "Platinum Ore"),
	OrePalladium	UMETA(DisplayName = "Palladium Ore"),

	// Tier 6 — Extremely Rare
	OreRhodium		UMETA(DisplayName = "Rhodium Ore"),
	OreIridium		UMETA(DisplayName = "Iridium Ore"),
	OreNiobium		UMETA(DisplayName = "Niobium Ore"),
	OreTantalum		UMETA(DisplayName = "Tantalum Ore"),
	OreOsmium		UMETA(DisplayName = "Osmium Ore"),

	MAX				UMETA(Hidden)
};

// ─── Voxel Data Struct ───────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FVoxelData
{
	GENERATED_BODY()

	/** Material type of this voxel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	EVoxelMaterial Material = EVoxelMaterial::Air;

	/** Density — 0.0 = fully empty (air), 1.0 = fully solid. Used as the isosurface field value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Density = 0.f;

	/** Quality of this voxel's material (0-100). Ore quality from vein, soil quality for farming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 Quality = 0;

	FVoxelData() = default;

	FVoxelData(EVoxelMaterial InMaterial, float InDensity, uint8 InQuality)
		: Material(InMaterial)
		, Density(InDensity)
		, Quality(InQuality)
	{
	}

	/** Returns true if this voxel is solid (density above threshold). */
	FORCEINLINE bool IsSolid() const { return Density > 0.5f; }

	/** Returns true if this voxel is air. */
	FORCEINLINE bool IsAir() const { return Material == EVoxelMaterial::Air || Density <= 0.f; }

	/** Returns true if this voxel contains any type of ore. */
	FORCEINLINE bool IsOre() const { return Material >= EVoxelMaterial::OreCopper && Material <= EVoxelMaterial::OreOsmium; }

	/** Returns true if this voxel is a soil type (non-rock, non-ore, non-air). */
	FORCEINLINE bool IsSoil() const { return Material >= EVoxelMaterial::Soil && Material <= EVoxelMaterial::Sand; }
};

// ─── Chunk Coordinate Struct ─────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FChunkCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 Y = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
	int32 Z = 0;

	FChunkCoord() = default;

	FChunkCoord(int32 InX, int32 InY, int32 InZ)
		: X(InX), Y(InY), Z(InZ)
	{
	}

	explicit FChunkCoord(const FIntVector& InVec)
		: X(InVec.X), Y(InVec.Y), Z(InVec.Z)
	{
	}

	/** Convert to FIntVector for use with TMap keys. */
	FIntVector ToIntVector() const { return FIntVector(X, Y, Z); }

	bool operator==(const FChunkCoord& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	bool operator!=(const FChunkCoord& Other) const
	{
		return !(*this == Other);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(%d, %d, %d)"), X, Y, Z);
	}
};

FORCEINLINE uint32 GetTypeHash(const FChunkCoord& Coord)
{
	uint32 Hash = HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
	return HashCombine(Hash, GetTypeHash(Coord.Z));
}

// ─── Vein Data Struct ────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FVeinData
{
	GENERATED_BODY()

	/** Center location of the vein in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Vein")
	FVector Center = FVector::ZeroVector;

	/** Radius of the ore vein in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Vein")
	float Radius = 0.f;

	/** Material type of the ore in this vein. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Vein")
	EVoxelMaterial Material = EVoxelMaterial::Air;

	/** Quality of the ore in this vein (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Vein", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 Quality = 0;

	/** Remaining ore units in this vein. Decreases as ore is mined. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Vein")
	int32 RemainingOre = 0;

	FVeinData() = default;

	FVeinData(FVector InCenter, float InRadius, EVoxelMaterial InMaterial, uint8 InQuality, int32 InRemainingOre)
		: Center(InCenter)
		, Radius(InRadius)
		, Material(InMaterial)
		, Quality(InQuality)
		, RemainingOre(InRemainingOre)
	{
	}

	/** Returns true if this vein has been fully depleted. */
	bool IsDepleted() const { return RemainingOre <= 0; }
};

// ─── Utility Functions ───────────────────────────────────────────────────────

namespace AoCVoxelUtils
{
	/** Convert a world position to the chunk coordinate containing that position. */
	FORCEINLINE FIntVector WorldToChunkCoord(const FVector& WorldPos)
	{
		return FIntVector(
			FMath::FloorToInt(WorldPos.X / CHUNK_WORLD_SIZE),
			FMath::FloorToInt(WorldPos.Y / CHUNK_WORLD_SIZE),
			FMath::FloorToInt(WorldPos.Z / CHUNK_WORLD_SIZE)
		);
	}

	/** Convert a world position to local voxel indices within a chunk. */
	FORCEINLINE FIntVector WorldToLocalVoxel(const FVector& WorldPos)
	{
		FIntVector ChunkCoord = WorldToChunkCoord(WorldPos);
		FVector ChunkOrigin(ChunkCoord.X * CHUNK_WORLD_SIZE, ChunkCoord.Y * CHUNK_WORLD_SIZE, ChunkCoord.Z * CHUNK_WORLD_SIZE);
		FVector LocalPos = WorldPos - ChunkOrigin;
		return FIntVector(
			FMath::Clamp(FMath::FloorToInt(LocalPos.X / VOXEL_SIZE), 0, CHUNK_SIZE - 1),
			FMath::Clamp(FMath::FloorToInt(LocalPos.Y / VOXEL_SIZE), 0, CHUNK_SIZE - 1),
			FMath::Clamp(FMath::FloorToInt(LocalPos.Z / VOXEL_SIZE), 0, CHUNK_SIZE - 1)
		);
	}

	/** Convert chunk coordinate + local voxel index to world position (center of voxel). */
	FORCEINLINE FVector ChunkLocalToWorld(const FIntVector& ChunkCoord, int32 X, int32 Y, int32 Z)
	{
		return FVector(
			ChunkCoord.X * CHUNK_WORLD_SIZE + (X + 0.5f) * VOXEL_SIZE,
			ChunkCoord.Y * CHUNK_WORLD_SIZE + (Y + 0.5f) * VOXEL_SIZE,
			ChunkCoord.Z * CHUNK_WORLD_SIZE + (Z + 0.5f) * VOXEL_SIZE
		);
	}

	/** Flatten 3D voxel index to 1D array index. */
	FORCEINLINE int32 VoxelIndex(int32 X, int32 Y, int32 Z)
	{
		return X + Y * CHUNK_SIZE + Z * CHUNK_SIZE * CHUNK_SIZE;
	}

	/** Check if local voxel coordinates are within bounds. */
	FORCEINLINE bool IsValidVoxel(int32 X, int32 Y, int32 Z)
	{
		return X >= 0 && X < CHUNK_SIZE && Y >= 0 && Y < CHUNK_SIZE && Z >= 0 && Z < CHUNK_SIZE;
	}
}
