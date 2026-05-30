// TerraForgeTypes.h
// TerraForge — Unified Geological Deformation Engine
// All shared types, enums, structs, and constants.

#pragma once

#include "CoreMinimal.h"
#include "TerraForgeTypes.generated.h"

// ============================================================================
// CONSTANTS
// ============================================================================

/** Voxels per chunk axis. 16³ = 4096 voxels per chunk. */
static constexpr int32 TF_CHUNK_SIZE = 16;

/** Meters per voxel edge. 0.5m gives fine-grained terrain control. */
static constexpr float TF_VOXEL_SIZE = 0.5f;

/** Centimeters per voxel edge (UE5 units = cm). */
static constexpr float TF_VOXEL_SIZE_CM = TF_VOXEL_SIZE * 100.0f; // 50cm

/** World-space size of one chunk (meters). 16 * 0.5 = 8m. */
static constexpr float TF_CHUNK_WORLD_SIZE = TF_CHUNK_SIZE * TF_VOXEL_SIZE;

/** Maximum number of chunks loaded in memory at once. */
static constexpr int32 TF_MAX_LOADED_CHUNKS = 200;

/** How many chunks to stream in per frame (prevents hitches). */
static constexpr int32 TF_CHUNKS_PER_FRAME = 4;

/** Distance rings for chunk LOD (in chunk units). */
static constexpr int32 TF_RING_CRITICAL = 1;    // 0-8m:  full detail
static constexpr int32 TF_RING_HIGH     = 4;    // 8-32m: full detail
static constexpr int32 TF_RING_MEDIUM   = 8;    // 32-64m: reduced
static constexpr int32 TF_RING_LOW      = 16;   // 64-128m: minimal

/** Terrain modification amount per action (meters). */
static constexpr float TF_DIG_DEPTH_PER_ACTION = 0.1f;

/** Maximum unsupported span before collapse warning (voxel units). */
static constexpr int32 TF_MAX_UNSUPPORTED_SPAN_DEFAULT = 6;

/** Stress threshold for collapse (0-255 mapped to 0.0-1.0). */
static constexpr float TF_COLLAPSE_THRESHOLD = 1.0f;

/** Days before unmodified terrain regenerates (game-time days). */
static constexpr float TF_REGEN_DAYS_UNCLAIMED = 7.0f;

/** Water table default depth below surface (meters). */
static constexpr float TF_WATER_TABLE_DEFAULT_DEPTH = 12.0f;

/** Observe mode grid dimensions. */
static constexpr int32 TF_OBSERVE_GRID_SIZE = 11;

/** QEF regularization factor for dual contouring. */
static constexpr float TF_QEF_REGULARIZATION = 0.01f;

/** Minimum density change to count as a surface crossing. */
static constexpr float TF_SURFACE_EPSILON = 0.001f;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/** Geological material — what each voxel is made of. */
UENUM(BlueprintType)
enum class EGeoMaterial : uint8
{
	Air            = 0   UMETA(DisplayName = "Air"),

	// Soil layers (surface)
	Topsoil        = 1   UMETA(DisplayName = "Topsoil"),
	ForestSoil     = 2   UMETA(DisplayName = "Forest Soil"),
	FertileSoil    = 3   UMETA(DisplayName = "Fertile Soil"),

	// Sediment layers
	Clay           = 4   UMETA(DisplayName = "Clay"),
	Sand           = 5   UMETA(DisplayName = "Sand"),
	Gravel         = 6   UMETA(DisplayName = "Gravel"),

	// Stone types (matching game data — 12 stone types)
	Sandstone      = 10  UMETA(DisplayName = "Sandstone"),
	Limestone      = 11  UMETA(DisplayName = "Limestone"),
	Slate          = 12  UMETA(DisplayName = "Slate"),
	Granite        = 13  UMETA(DisplayName = "Granite"),
	Marble         = 14  UMETA(DisplayName = "Marble"),
	Basalt         = 15  UMETA(DisplayName = "Basalt"),
	Obsidian       = 16  UMETA(DisplayName = "Obsidian"),
	Quartzite      = 17  UMETA(DisplayName = "Quartzite"),
	Jade           = 18  UMETA(DisplayName = "Jade"),
	Onyx           = 19  UMETA(DisplayName = "Onyx"),
	Runestone      = 20  UMETA(DisplayName = "Runestone"),
	Voidrock       = 21  UMETA(DisplayName = "Voidrock"),

	// Special types
	OreVein        = 30  UMETA(DisplayName = "Ore Vein"),
	Water          = 31  UMETA(DisplayName = "Water"),
	Rubble         = 32  UMETA(DisplayName = "Rubble"),
	Bedrock        = 33  UMETA(DisplayName = "Bedrock"),
	WaterTable     = 34  UMETA(DisplayName = "Water Table"),

	MAX            = 35  UMETA(Hidden)
};

/** Ore type — the 22 metals from the periodic table system. */
UENUM(BlueprintType)
enum class EGeoOreType : uint8
{
	None           = 0   UMETA(DisplayName = "None"),

	// Tier 1 — Common (shallow)
	Copper         = 1   UMETA(DisplayName = "Copper"),
	Tin            = 2   UMETA(DisplayName = "Tin"),

	// Tier 2 — Common-Moderate
	Iron           = 3   UMETA(DisplayName = "Iron"),
	Zinc           = 4   UMETA(DisplayName = "Zinc"),
	Lead           = 5   UMETA(DisplayName = "Lead"),

	// Tier 3 — Moderate-Uncommon
	Nickel         = 6   UMETA(DisplayName = "Nickel"),
	Silver         = 7   UMETA(DisplayName = "Silver"),
	Gold           = 8   UMETA(DisplayName = "Gold"),

	// Tier 4 — Uncommon
	Chromium       = 9   UMETA(DisplayName = "Chromium"),
	Cobalt         = 10  UMETA(DisplayName = "Cobalt"),
	Manganese      = 11  UMETA(DisplayName = "Manganese"),

	// Tier 5 — Rare
	Molybdenum     = 12  UMETA(DisplayName = "Molybdenum"),
	Titanium       = 13  UMETA(DisplayName = "Titanium"),
	Tungsten       = 14  UMETA(DisplayName = "Tungsten"),
	Vanadium       = 15  UMETA(DisplayName = "Vanadium"),

	// Tier 6 — Very Rare / Precious
	Platinum       = 16  UMETA(DisplayName = "Platinum"),
	Antimony       = 17  UMETA(DisplayName = "Antimony"),
	Bismuth        = 18  UMETA(DisplayName = "Bismuth"),

	// Tier 7 — Fantasy / Ultra Rare (Arcane Forge)
	Mithril        = 19  UMETA(DisplayName = "Mithril"),
	Adamantite     = 20  UMETA(DisplayName = "Adamantite"),
	Orichalcum     = 21  UMETA(DisplayName = "Orichalcum"),
	StarMetal      = 22  UMETA(DisplayName = "Star Metal"),

	MAX            = 23  UMETA(Hidden)
};

/** Gem type — rare drops while mining rock layers. */
UENUM(BlueprintType)
enum class EGeoGemType : uint8
{
	None           = 0   UMETA(DisplayName = "None"),

	// Common
	Quartz         = 1   UMETA(DisplayName = "Quartz"),

	// Uncommon
	Amethyst       = 2   UMETA(DisplayName = "Amethyst"),
	Garnet         = 3   UMETA(DisplayName = "Garnet"),

	// Rare
	Topaz          = 4   UMETA(DisplayName = "Topaz"),
	Sapphire       = 5   UMETA(DisplayName = "Sapphire"),

	// Very Rare
	Ruby           = 6   UMETA(DisplayName = "Ruby"),
	Emerald        = 7   UMETA(DisplayName = "Emerald"),

	// Ultra Rare
	Diamond        = 8   UMETA(DisplayName = "Diamond"),

	// Legendary
	BlackOpal      = 9   UMETA(DisplayName = "Black Opal"),
	StarSapphire   = 10  UMETA(DisplayName = "Star Sapphire"),

	MAX            = 11  UMETA(Hidden)
};

/** Gem rarity tier — determines drop chance and value. */
UENUM(BlueprintType)
enum class EGemRarity : uint8
{
	Common      = 0  UMETA(DisplayName = "Common"),
	Uncommon    = 1  UMETA(DisplayName = "Uncommon"),
	Rare        = 2  UMETA(DisplayName = "Rare"),
	VeryRare    = 3  UMETA(DisplayName = "Very Rare"),
	UltraRare   = 4  UMETA(DisplayName = "Ultra Rare"),
	Legendary   = 5  UMETA(DisplayName = "Legendary"),
	MAX         = 6  UMETA(Hidden)
};

/** Properties of a gem type. */
USTRUCT(BlueprintType)
struct FGemProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EGeoGemType GemType = EGeoGemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EGemRarity Rarity = EGemRarity::Common;

	/** Minimum depth in meters below surface to find this gem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinDepthM = 0.0f;

	/** Drop chance per rock voxel mined (0.0 - 1.0). Base rate before skill modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseDropChance = 0.0f;

	/** Base trade value in gold units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 BaseValue = 0;

	/** Color tint for world mesh and UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor Color = FLinearColor::White;

	/** Static mesh path for world drop representation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString WorldMeshPath;

	/** Icon texture path for inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString IconPath;
};

/** Result of a gem drop roll. */
USTRUCT(BlueprintType)
struct FGemDropResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bDropped = false;

	UPROPERTY(BlueprintReadOnly)
	EGeoGemType GemType = EGeoGemType::None;

	UPROPERTY(BlueprintReadOnly)
	float Quality = 0.0f;  // 0-100

	UPROPERTY(BlueprintReadOnly)
	int32 Value = 0;
};

/** Gem drop table — singleton data, initialized once. */
struct FGemDropTable
{
	TMap<EGeoGemType, FGemProperties> Gems;

	void Initialize()
	{
		auto Add = [this](EGeoGemType Type, const FString& Name, EGemRarity Rarity,
			float MinDepth, float DropChance, int32 Value, FLinearColor Color)
		{
			FGemProperties Props;
			Props.GemType = Type;
			Props.DisplayName = FText::FromString(Name);
			Props.Rarity = Rarity;
			Props.MinDepthM = MinDepth;
			Props.BaseDropChance = DropChance;
			Props.BaseValue = Value;
			Props.Color = Color;
			Props.WorldMeshPath = FString::Printf(TEXT("/Game/TerraForge/Gems/SM_Gem_%s.SM_Gem_%s"), *Name.Replace(TEXT(" "), TEXT("")), *Name.Replace(TEXT(" "), TEXT("")));
			Props.IconPath = FString::Printf(TEXT("/Game/TerraForge/Icons/Gems/T_Gem_%s.T_Gem_%s"), *Name.Replace(TEXT(" "), TEXT("")), *Name.Replace(TEXT(" "), TEXT("")));
			Gems.Add(Type, Props);
		};

		//                Type                       Name              Rarity                MinDepth  DropChance  Value   Color
		Add(EGeoGemType::Quartz,        TEXT("Quartz"),        EGemRarity::Common,      3.0f,    0.020f,      10,   FLinearColor(0.95f, 0.95f, 0.95f, 1.0f));
		Add(EGeoGemType::Amethyst,      TEXT("Amethyst"),      EGemRarity::Uncommon,    5.0f,    0.005f,      50,   FLinearColor(0.58f, 0.27f, 0.82f, 1.0f));
		Add(EGeoGemType::Garnet,        TEXT("Garnet"),        EGemRarity::Uncommon,    8.0f,    0.005f,      60,   FLinearColor(0.69f, 0.11f, 0.11f, 1.0f));
		Add(EGeoGemType::Topaz,         TEXT("Topaz"),         EGemRarity::Rare,       10.0f,    0.001f,     200,   FLinearColor(1.0f, 0.78f, 0.17f, 1.0f));
		Add(EGeoGemType::Sapphire,      TEXT("Sapphire"),      EGemRarity::Rare,       15.0f,    0.001f,     250,   FLinearColor(0.06f, 0.32f, 0.73f, 1.0f));
		Add(EGeoGemType::Ruby,          TEXT("Ruby"),          EGemRarity::VeryRare,   18.0f,    0.0002f,    800,   FLinearColor(0.88f, 0.07f, 0.37f, 1.0f));
		Add(EGeoGemType::Emerald,       TEXT("Emerald"),       EGemRarity::VeryRare,   20.0f,    0.0002f,    850,   FLinearColor(0.18f, 0.80f, 0.25f, 1.0f));
		Add(EGeoGemType::Diamond,       TEXT("Diamond"),       EGemRarity::UltraRare,  25.0f,    0.0001f,   2000,   FLinearColor(0.85f, 0.97f, 1.0f, 1.0f));
		Add(EGeoGemType::BlackOpal,     TEXT("Black Opal"),    EGemRarity::Legendary,  30.0f,    0.00005f,  5000,   FLinearColor(0.1f, 0.1f, 0.15f, 1.0f));
		Add(EGeoGemType::StarSapphire,  TEXT("Star Sapphire"), EGemRarity::Legendary,  35.0f,    0.00005f,  5000,   FLinearColor(0.40f, 0.55f, 0.95f, 1.0f));
	}

	/** Roll for a gem drop given depth and player mining skill. */
	FGemDropResult RollGemDrop(float DepthBelowSurface, float MiningSkillLevel) const
	{
		FGemDropResult Result;

		// Skill bonus: every 10 skill levels = +10% drop chance (max +100% at skill 100)
		float SkillMultiplier = 1.0f + FMath::Clamp(MiningSkillLevel / 100.0f, 0.0f, 1.0f);

		// Roll from rarest to most common — only one gem per voxel
		static const EGeoGemType RollOrder[] = {
			EGeoGemType::StarSapphire,
			EGeoGemType::BlackOpal,
			EGeoGemType::Diamond,
			EGeoGemType::Emerald,
			EGeoGemType::Ruby,
			EGeoGemType::Sapphire,
			EGeoGemType::Topaz,
			EGeoGemType::Garnet,
			EGeoGemType::Amethyst,
			EGeoGemType::Quartz
		};

		for (EGeoGemType GemType : RollOrder)
		{
			const FGemProperties* Props = Gems.Find(GemType);
			if (!Props) continue;

			// Must be deep enough
			if (DepthBelowSurface < Props->MinDepthM) continue;

			// Depth bonus: deeper = slightly better odds (10% per 10m past minimum)
			float DepthBonus = 1.0f + FMath::Clamp((DepthBelowSurface - Props->MinDepthM) / 100.0f, 0.0f, 0.5f);

			float FinalChance = Props->BaseDropChance * SkillMultiplier * DepthBonus;

			if (FMath::FRand() < FinalChance)
			{
				Result.bDropped = true;
				Result.GemType = GemType;
				// Quality influenced by skill and depth
				Result.Quality = FMath::Clamp(
					FMath::FRandRange(20.0f, 60.0f) + (MiningSkillLevel * 0.3f) + (DepthBelowSurface * 0.2f),
					0.0f, 100.0f);
				// Value scales with quality
				Result.Value = FMath::RoundToInt(Props->BaseValue * (0.5f + Result.Quality / 100.0f));
				return Result;
			}
		}

		return Result; // bDropped = false
	}
};

/** Biome type — determines geological layer composition. */
UENUM(BlueprintType)
enum class EGeoBiome : uint8
{
	Plains         = 0   UMETA(DisplayName = "Plains"),
	Forest         = 1   UMETA(DisplayName = "Forest"),
	Hills          = 2   UMETA(DisplayName = "Hills"),
	Mountains      = 3   UMETA(DisplayName = "Mountains"),
	Swamp          = 4   UMETA(DisplayName = "Swamp"),
	Desert         = 5   UMETA(DisplayName = "Desert"),
	Volcanic       = 6   UMETA(DisplayName = "Volcanic"),
	Tundra         = 7   UMETA(DisplayName = "Tundra"),
	River          = 8   UMETA(DisplayName = "River Valley"),
	Coast          = 9   UMETA(DisplayName = "Coastline"),
	MAX            = 10  UMETA(Hidden)
};

/** Terraforming action the player can perform. */
UENUM(BlueprintType)
enum class ETerraAction : uint8
{
	None            = 0  UMETA(DisplayName = "None"),
	Observe         = 1  UMETA(DisplayName = "Observe"),
	LowerGround     = 2  UMETA(DisplayName = "Lower Ground"),
	RaiseGround     = 3  UMETA(DisplayName = "Raise Ground"),
	Flatten         = 4  UMETA(DisplayName = "Flatten"),
	FlattenSlope    = 5  UMETA(DisplayName = "Flatten Slope"),
	DigTunnel       = 6  UMETA(DisplayName = "Dig Tunnel"),
	PlaceSupport    = 7  UMETA(DisplayName = "Place Support"),
	Prospect        = 8  UMETA(DisplayName = "Prospect"),
	MAX             = 9  UMETA(Hidden)
};

/** Tool required for a terraforming action. */
UENUM(BlueprintType)
enum class ETerraToolType : uint8
{
	None           = 0   UMETA(DisplayName = "None"),
	Shovel         = 1   UMETA(DisplayName = "Shovel"),
	Pickaxe        = 2   UMETA(DisplayName = "Pickaxe"),
	Hammer         = 3   UMETA(DisplayName = "Hammer"),
	MAX            = 4   UMETA(Hidden)
};

/** LOD level for a loaded chunk. */
UENUM()
enum class EChunkLOD : uint8
{
	Full           = 0,  // All vertices
	Half           = 1,  // Every other vertex
	Quarter        = 2,  // Every 4th vertex
	Minimal        = 3   // 8 vertices (bounding box)
};

/** Structural stability state. */
UENUM(BlueprintType)
enum class EStructuralState : uint8
{
	Stable         = 0   UMETA(DisplayName = "Stable"),
	Supported      = 1   UMETA(DisplayName = "Supported — Mine Supports Active"),
	Warning        = 2   UMETA(DisplayName = "Warning — Unstable"),
	Critical       = 3   UMETA(DisplayName = "Critical — Imminent Collapse"),
	Collapsed      = 4   UMETA(DisplayName = "Collapsed")
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * A single voxel in the geological field.
 * Packed to 12 bytes for cache efficiency.
 * Millions of these exist — keep it lean.
 */
USTRUCT()
struct FGeoVoxel
{
	GENERATED_BODY()

	/** Signed distance field value. Negative = solid, positive = air, 0 = surface. */
	float Density = 1.0f;  // Default: air

	/** What material this voxel is. */
	uint8 Material = (uint8)EGeoMaterial::Air;

	/** Ore type if Material == OreVein. */
	uint8 OreType = (uint8)EGeoOreType::None;

	/** Quality rating 0-100. */
	uint8 Quality = 0;

	/** Moisture content 0-255 (mapped to 0.0-1.0). */
	uint8 Moisture = 0;

	/** Structural stress 0-255 (mapped to 0.0-1.0). */
	uint8 StressLoad = 0;

	/** Bit flags. */
	uint8 Flags = 0;

	/** Remaining ore units (only for ore voxels). */
	uint16 RemainingUnits = 0;

	// --- Flag bit masks ---
	static constexpr uint8 FLAG_MODIFIED   = 1 << 0;
	static constexpr uint8 FLAG_SURVEYED   = 1 << 1;
	static constexpr uint8 FLAG_CLAIMED    = 1 << 2;
	static constexpr uint8 FLAG_SUPPORTED  = 1 << 3;
	static constexpr uint8 FLAG_WATERLOGGED = 1 << 4;

	// --- Convenience ---
	FORCEINLINE bool IsAir() const       { return Density > 0.0f; }
	FORCEINLINE bool IsSolid() const     { return Density <= 0.0f; }
	FORCEINLINE bool IsOre() const       { return Material == (uint8)EGeoMaterial::OreVein; }
	FORCEINLINE bool IsModified() const  { return (Flags & FLAG_MODIFIED) != 0; }
	FORCEINLINE bool IsClaimed() const   { return (Flags & FLAG_CLAIMED) != 0; }
	FORCEINLINE bool IsSupported() const { return (Flags & FLAG_SUPPORTED) != 0; }

	FORCEINLINE EGeoMaterial GetMaterial() const   { return (EGeoMaterial)Material; }
	FORCEINLINE EGeoOreType  GetOreType() const    { return (EGeoOreType)OreType; }
	FORCEINLINE float GetQualityF() const          { return (float)Quality; }
	FORCEINLINE float GetMoistureF() const         { return (float)Moisture / 255.0f; }
	FORCEINLINE float GetStressF() const           { return (float)StressLoad / 255.0f; }

	FORCEINLINE void SetMaterial(EGeoMaterial M)   { Material = (uint8)M; }
	FORCEINLINE void SetOreType(EGeoOreType O)     { OreType = (uint8)O; }
	FORCEINLINE void SetQuality(float Q)           { Quality = (uint8)FMath::Clamp(Q, 0.0f, 100.0f); }
	FORCEINLINE void SetMoisture(float M)          { Moisture = (uint8)FMath::Clamp(M * 255.0f, 0.0f, 255.0f); }
	FORCEINLINE void SetStress(float S)            { StressLoad = (uint8)FMath::Clamp(S * 255.0f, 0.0f, 255.0f); }

	/** Make this voxel solid with a given material. */
	void MakeSolid(EGeoMaterial Mat, float InQuality = 50.0f)
	{
		Density = -1.0f;
		SetMaterial(Mat);
		SetQuality(InQuality);
		Flags |= FLAG_MODIFIED;
	}

	/** Make this voxel air (dug out). */
	void MakeAir()
	{
		Density = 1.0f;
		SetMaterial(EGeoMaterial::Air);
		OreType = (uint8)EGeoOreType::None;
		Quality = 0;
		RemainingUnits = 0;
		Flags |= FLAG_MODIFIED;
	}

	/** Make this voxel an ore deposit. */
	void MakeOre(EGeoOreType Ore, float InQuality, uint16 Units)
	{
		Density = -1.0f;
		SetMaterial(EGeoMaterial::OreVein);
		SetOreType(Ore);
		SetQuality(InQuality);
		RemainingUnits = Units;
		Flags |= FLAG_MODIFIED;
	}
};

// Verify size at compile time — keep it tight
static_assert(sizeof(FGeoVoxel) <= 16, "FGeoVoxel exceeds 16 bytes — review packing");

/**
 * Chunk coordinate key for the world hashmap.
 * Integer coordinates in chunk-space (world pos / TF_CHUNK_WORLD_SIZE).
 */
USTRUCT()
struct FTerraChunkKey
{
	GENERATED_BODY()

	UPROPERTY()
	int32 X = 0;

	UPROPERTY()
	int32 Y = 0;

	UPROPERTY()
	int32 Z = 0;

	FTerraChunkKey() = default;
	FTerraChunkKey(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

	/** Convert a world position to a chunk key. */
	static FTerraChunkKey FromWorldPos(const FVector& WorldPos)
	{
		return FTerraChunkKey(
			FMath::FloorToInt(WorldPos.X / (TF_CHUNK_WORLD_SIZE * 100.0f)),  // UE uses cm
			FMath::FloorToInt(WorldPos.Y / (TF_CHUNK_WORLD_SIZE * 100.0f)),
			FMath::FloorToInt(WorldPos.Z / (TF_CHUNK_WORLD_SIZE * 100.0f))
		);
	}

	/** Get the world-space origin of this chunk (min corner, in cm). */
	FVector ToWorldPos() const
	{
		return FVector(
			X * TF_CHUNK_WORLD_SIZE * 100.0f,
			Y * TF_CHUNK_WORLD_SIZE * 100.0f,
			Z * TF_CHUNK_WORLD_SIZE * 100.0f
		);
	}

	bool operator==(const FTerraChunkKey& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	bool operator!=(const FTerraChunkKey& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FTerraChunkKey& Key)
	{
		// Combine with prime multipliers for good distribution
		return HashCombine(HashCombine(GetTypeHash(Key.X), GetTypeHash(Key.Y)), GetTypeHash(Key.Z));
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(%d,%d,%d)"), X, Y, Z);
	}
};

/**
 * Material properties — physical characteristics of each geological material.
 * Used for dig speed, sound, structural strength, tool requirements, etc.
 */
USTRUCT(BlueprintType)
struct FGeoMaterialProperties
{
	GENERATED_BODY()

	/** Display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	/** How long it takes to dig one action (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DigTime = 1.0f;

	/** Minimum tool type required to dig this material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETerraToolType RequiredTool = ETerraToolType::Shovel;

	/** Structural strength — how much stress this material can bear (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Strength = 0.5f;

	/** Maximum unsupported span before collapse (in voxel units). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxUnsupportedSpan = 6;

	/** Weight per voxel (affects overburdened mechanic). kg per 0.5m³ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float WeightPerVoxel = 50.0f;

	/** Water permeability 0-1 (0=waterproof, 1=flows freely). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Permeability = 0.5f;

	/** Tool durability cost per dig action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ToolDurabilityCost = 1.0f;

	/** Color tint for vertex coloring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BaseColor = FLinearColor(0.5f, 0.4f, 0.3f, 1.0f);

	/** Sound cue name for digging this material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName DigSoundName;
};

/**
 * Ore generation rule — how each ore type is distributed underground.
 */
USTRUCT(BlueprintType)
struct FGeoOreRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EGeoOreType OreType = EGeoOreType::None;

	/** Minimum depth below surface (meters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinDepth = 3.0f;

	/** Maximum depth below surface (meters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDepth = 30.0f;

	/** Biomes where this ore can spawn. Empty = all biomes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EGeoBiome> PreferredBiomes;

	/** Rock types this ore appears in. Empty = any rock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EGeoMaterial> HostRockTypes;

	/** Average vein size in voxels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AverageVeinSize = 20;

	/** Ore units per voxel (small node). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	uint16 UnitsPerVoxel = 5;

	/** Spawns per km² (controls rarity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SpawnsPerKmSq = 10.0f;

	/** Quality range (min, max). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D QualityRange = FVector2D(20.0f, 80.0f);

	/** Minimum prospecting skill to detect this ore. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinProspectingSkill = 0;

	/** Smelting temperature (°C) — for reference/UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SmeltingTemp = 1000.0f;
};

/**
 * Geological layer definition — what material exists at a given depth in a biome.
 */
USTRUCT(BlueprintType)
struct FGeoLayerDef
{
	GENERATED_BODY()

	/** Material for this layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EGeoMaterial Material = EGeoMaterial::Granite;

	/** Depth range (meters below surface). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinDepth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDepth = 100.0f;

	/** Quality range for this layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D QualityRange = FVector2D(30.0f, 70.0f);
};

/**
 * Biome geological profile — the full layer stack for a biome type.
 */
USTRUCT(BlueprintType)
struct FGeoBiomeProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EGeoBiome Biome = EGeoBiome::Plains;

	/** Ordered layers from surface downward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FGeoLayerDef> Layers;

	/** Water table depth (meters below surface). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float WaterTableDepth = 12.0f;

	/** Soil quality base value (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseSoilQuality = 50.0f;
};

/**
 * Mesh output data — result of dual contouring for one chunk.
 * Thread-safe container passed from worker to game thread.
 */
struct FTerraChunkMeshData
{
	TArray<FVector>        Vertices;
	TArray<int32>          Triangles;
	TArray<FVector>        Normals;
	TArray<FVector2D>      UV0;
	TArray<FColor>         VertexColors;
	TArray<FVector>        Tangents;

	/** Per-vertex material index (for material blending in shader). */
	TArray<uint8>          MaterialIndices;

	/** Is this mesh data valid and ready to apply? */
	bool bValid = false;

	/** LOD level this mesh was generated at. */
	EChunkLOD LODLevel = EChunkLOD::Full;

	void Reset()
	{
		Vertices.Reset();
		Triangles.Reset();
		Normals.Reset();
		UV0.Reset();
		VertexColors.Reset();
		Tangents.Reset();
		MaterialIndices.Reset();
		bValid = false;
	}

	int32 GetVertexCount() const { return Vertices.Num(); }
	int32 GetTriangleCount() const { return Triangles.Num() / 3; }
};

/**
 * Prospecting result — information about a detected ore deposit.
 */
USTRUCT(BlueprintType)
struct FProspectingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly)
	EGeoOreType OreType = EGeoOreType::None;

	/** Approximate direction from player to deposit. */
	UPROPERTY(BlueprintReadOnly)
	FVector Direction = FVector::ZeroVector;

	/** Approximate depth below surface (meters). */
	UPROPERTY(BlueprintReadOnly)
	float Depth = 0.0f;

	/** Estimated quality (accuracy depends on skill). */
	UPROPERTY(BlueprintReadOnly)
	float EstimatedQuality = 0.0f;

	/** Estimated remaining units (accuracy depends on skill). */
	UPROPERTY(BlueprintReadOnly)
	int32 EstimatedUnits = 0;

	/** Distance from player (meters). */
	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.0f;

	/** Detail level 0-3 based on skill (0=vague, 3=precise). */
	UPROPERTY(BlueprintReadOnly)
	int32 DetailLevel = 0;
};

/**
 * Observe mode tile data — one tile in the observe grid overlay.
 */
USTRUCT(BlueprintType)
struct FObserveTile
{
	GENERATED_BODY()

	/** World position of this tile's center. */
	UPROPERTY(BlueprintReadOnly)
	FVector WorldPosition = FVector::ZeroVector;

	/** Elevation at this point (meters). */
	UPROPERTY(BlueprintReadOnly)
	float Elevation = 0.0f;

	/** Surface material at this point. */
	UPROPERTY(BlueprintReadOnly)
	EGeoMaterial SurfaceMaterial = EGeoMaterial::Topsoil;

	/** Soil quality (if applicable). */
	UPROPERTY(BlueprintReadOnly)
	float Quality = 0.0f;

	/** Moisture level 0-1. */
	UPROPERTY(BlueprintReadOnly)
	float Moisture = 0.0f;

	/** Is this tile flat relative to neighbors? */
	UPROPERTY(BlueprintReadOnly)
	bool bIsFlat = false;

	/** Height difference from the reference tile (player's position). */
	UPROPERTY(BlueprintReadOnly)
	float HeightDelta = 0.0f;
};

// ============================================================================
// STATIC DATA TABLES
// ============================================================================

/**
 * Static helper to get default material properties.
 * Called once at subsystem init, cached in a TMap.
 */
namespace TerraForgeData
{
	/** Build the default material property table. */
	inline TMap<EGeoMaterial, FGeoMaterialProperties> BuildMaterialTable()
	{
		TMap<EGeoMaterial, FGeoMaterialProperties> Table;

		auto Add = [&](EGeoMaterial Mat, const TCHAR* Name, float Dig, ETerraToolType Tool,
		               float Str, int32 Span, float Weight, float Perm, float DurCost,
		               FLinearColor Color, FName Sound)
		{
			FGeoMaterialProperties P;
			P.DisplayName         = FText::FromString(Name);
			P.DigTime             = Dig;
			P.RequiredTool        = Tool;
			P.Strength            = Str;
			P.MaxUnsupportedSpan  = Span;
			P.WeightPerVoxel      = Weight;
			P.Permeability        = Perm;
			P.ToolDurabilityCost  = DurCost;
			P.BaseColor           = Color;
			P.DigSoundName        = Sound;
			Table.Add(Mat, P);
		};

		//               Material             Name            Dig  Tool          Str  Span  Wt    Perm  Dur   Color                              Sound
		Add(EGeoMaterial::Topsoil,      TEXT("Topsoil"),      0.8f, ETerraToolType::Shovel,  0.1f,  2,  30.0f, 0.8f, 0.5f, FLinearColor(0.45f, 0.35f, 0.20f), TEXT("Dig_Dirt"));
		Add(EGeoMaterial::ForestSoil,   TEXT("Forest Soil"),  0.9f, ETerraToolType::Shovel,  0.15f, 2,  35.0f, 0.7f, 0.5f, FLinearColor(0.30f, 0.25f, 0.15f), TEXT("Dig_Dirt"));
		Add(EGeoMaterial::FertileSoil,  TEXT("Fertile Soil"), 0.8f, ETerraToolType::Shovel,  0.12f, 2,  32.0f, 0.75f,0.5f, FLinearColor(0.25f, 0.20f, 0.10f), TEXT("Dig_Dirt"));
		Add(EGeoMaterial::Clay,         TEXT("Clay"),         1.2f, ETerraToolType::Shovel,  0.25f, 3,  55.0f, 0.2f, 0.8f, FLinearColor(0.65f, 0.40f, 0.20f), TEXT("Dig_Clay"));
		Add(EGeoMaterial::Sand,         TEXT("Sand"),         0.6f, ETerraToolType::Shovel,  0.05f, 1,  40.0f, 0.95f,0.3f, FLinearColor(0.85f, 0.78f, 0.55f), TEXT("Dig_Sand"));
		Add(EGeoMaterial::Gravel,       TEXT("Gravel"),       1.0f, ETerraToolType::Shovel,  0.2f,  2,  60.0f, 0.9f, 0.7f, FLinearColor(0.55f, 0.52f, 0.48f), TEXT("Dig_Gravel"));
		Add(EGeoMaterial::Sandstone,    TEXT("Sandstone"),    2.0f, ETerraToolType::Pickaxe, 0.4f,  5,  80.0f, 0.3f, 1.5f, FLinearColor(0.80f, 0.70f, 0.50f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Limestone,    TEXT("Limestone"),    2.2f, ETerraToolType::Pickaxe, 0.5f,  6,  85.0f, 0.25f,1.5f, FLinearColor(0.85f, 0.83f, 0.78f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Slate,        TEXT("Slate"),        2.5f, ETerraToolType::Pickaxe, 0.55f, 5,  90.0f, 0.1f, 2.0f, FLinearColor(0.35f, 0.38f, 0.42f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Granite,      TEXT("Granite"),      3.5f, ETerraToolType::Pickaxe, 0.85f, 10, 100.0f,0.05f,3.0f, FLinearColor(0.60f, 0.58f, 0.55f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Marble,       TEXT("Marble"),       3.0f, ETerraToolType::Pickaxe, 0.7f,  8,  95.0f, 0.08f,2.5f, FLinearColor(0.92f, 0.90f, 0.88f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Basalt,       TEXT("Basalt"),       4.0f, ETerraToolType::Pickaxe, 0.9f,  10, 110.0f,0.03f,3.5f, FLinearColor(0.20f, 0.20f, 0.22f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Obsidian,     TEXT("Obsidian"),     4.5f, ETerraToolType::Pickaxe, 0.6f,  4,  105.0f,0.01f,4.0f, FLinearColor(0.05f, 0.05f, 0.08f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Quartzite,    TEXT("Quartzite"),    3.8f, ETerraToolType::Pickaxe, 0.8f,  8,  98.0f, 0.05f,3.0f, FLinearColor(0.88f, 0.85f, 0.82f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Jade,         TEXT("Jade"),         4.0f, ETerraToolType::Pickaxe, 0.75f, 7,  100.0f,0.02f,3.5f, FLinearColor(0.30f, 0.65f, 0.35f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Onyx,         TEXT("Onyx"),         4.2f, ETerraToolType::Pickaxe, 0.78f, 7,  102.0f,0.02f,3.5f, FLinearColor(0.08f, 0.08f, 0.10f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Runestone,    TEXT("Runestone"),    5.0f, ETerraToolType::Pickaxe, 0.95f, 12, 120.0f,0.01f,4.5f, FLinearColor(0.40f, 0.35f, 0.55f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::Voidrock,     TEXT("Voidrock"),     6.0f, ETerraToolType::Pickaxe, 1.0f,  15, 150.0f,0.0f, 5.0f, FLinearColor(0.10f, 0.02f, 0.15f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::OreVein,      TEXT("Ore Vein"),     3.0f, ETerraToolType::Pickaxe, 0.7f,  6,  90.0f, 0.05f,2.0f, FLinearColor(0.55f, 0.45f, 0.30f), TEXT("Dig_Ore"));
		Add(EGeoMaterial::Rubble,       TEXT("Rubble"),       1.5f, ETerraToolType::Shovel,  0.1f,  1,  70.0f, 0.6f, 1.0f, FLinearColor(0.50f, 0.48f, 0.45f), TEXT("Dig_Gravel"));
		Add(EGeoMaterial::Bedrock,      TEXT("Bedrock"),      10.0f,ETerraToolType::Pickaxe, 2.0f,  20, 200.0f,0.0f, 8.0f, FLinearColor(0.15f, 0.12f, 0.10f), TEXT("Dig_Rock"));
		Add(EGeoMaterial::WaterTable,   TEXT("Water Table"),  0.0f, ETerraToolType::Shovel,  0.0f,  0,  0.0f,  0.0f, 0.0f, FLinearColor(0.10f, 0.30f, 0.60f), TEXT("Water_Splash"));

		return Table;
	}

	/** Build the default ore generation rules for all 22 metals. */
	inline TArray<FGeoOreRule> BuildOreRules()
	{
		TArray<FGeoOreRule> Rules;

		auto Add = [&](EGeoOreType Ore, float MinD, float MaxD,
		               TArray<EGeoBiome> Biomes, TArray<EGeoMaterial> Rocks,
		               int32 VeinSize, uint16 UPV, float Rarity,
		               FVector2D QRange, int32 MinSkill, float Temp)
		{
			FGeoOreRule R;
			R.OreType            = Ore;
			R.MinDepth           = MinD;
			R.MaxDepth           = MaxD;
			R.PreferredBiomes    = MoveTemp(Biomes);
			R.HostRockTypes      = MoveTemp(Rocks);
			R.AverageVeinSize    = VeinSize;
			R.UnitsPerVoxel      = UPV;
			R.SpawnsPerKmSq      = Rarity;
			R.QualityRange       = QRange;
			R.MinProspectingSkill = MinSkill;
			R.SmeltingTemp       = Temp;
			Rules.Add(R);
		};

		// Tier 1 — Common, shallow
		Add(EGeoOreType::Copper, 3.0f, 15.0f,
			{EGeoBiome::Hills, EGeoBiome::Mountains, EGeoBiome::Plains},
			{EGeoMaterial::Limestone, EGeoMaterial::Sandstone},
			25, 5, 40.0f, FVector2D(20, 80), 0, 1085.0f);

		Add(EGeoOreType::Tin, 3.0f, 12.0f,
			{EGeoBiome::Hills, EGeoBiome::River, EGeoBiome::Plains},
			{EGeoMaterial::Limestone, EGeoMaterial::Slate},
			20, 5, 35.0f, FVector2D(20, 75), 0, 232.0f);

		// Tier 2 — Common-Moderate
		Add(EGeoOreType::Iron, 5.0f, 25.0f,
			{}, // Any biome
			{},  // Any rock
			30, 6, 30.0f, FVector2D(25, 85), 5, 1538.0f);

		Add(EGeoOreType::Zinc, 5.0f, 18.0f,
			{EGeoBiome::Hills, EGeoBiome::Mountains},
			{EGeoMaterial::Limestone},
			18, 4, 20.0f, FVector2D(20, 70), 5, 420.0f);

		Add(EGeoOreType::Lead, 5.0f, 20.0f,
			{EGeoBiome::Hills, EGeoBiome::Mountains},
			{EGeoMaterial::Limestone, EGeoMaterial::Sandstone},
			22, 5, 25.0f, FVector2D(25, 75), 5, 327.0f);

		// Tier 3 — Moderate-Uncommon
		Add(EGeoOreType::Nickel, 8.0f, 25.0f,
			{EGeoBiome::Mountains, EGeoBiome::Volcanic},
			{EGeoMaterial::Granite, EGeoMaterial::Basalt},
			15, 4, 15.0f, FVector2D(30, 80), 15, 1455.0f);

		Add(EGeoOreType::Silver, 8.0f, 28.0f,
			{EGeoBiome::Mountains, EGeoBiome::Hills},
			{EGeoMaterial::Granite, EGeoMaterial::Quartzite},
			12, 3, 10.0f, FVector2D(30, 85), 20, 962.0f);

		Add(EGeoOreType::Gold, 10.0f, 35.0f,
			{EGeoBiome::Mountains, EGeoBiome::River},
			{EGeoMaterial::Quartzite, EGeoMaterial::Granite},
			8, 3, 5.0f, FVector2D(35, 90), 25, 1064.0f);

		// Tier 4 — Uncommon
		Add(EGeoOreType::Chromium, 10.0f, 30.0f,
			{EGeoBiome::Mountains, EGeoBiome::Volcanic},
			{EGeoMaterial::Granite},
			14, 4, 12.0f, FVector2D(30, 80), 30, 1907.0f);

		Add(EGeoOreType::Cobalt, 10.0f, 28.0f,
			{EGeoBiome::Mountains},
			{EGeoMaterial::Granite, EGeoMaterial::Basalt},
			12, 3, 10.0f, FVector2D(30, 80), 30, 1495.0f);

		Add(EGeoOreType::Manganese, 8.0f, 22.0f,
			{EGeoBiome::Hills, EGeoBiome::Mountains, EGeoBiome::Swamp},
			{EGeoMaterial::Limestone, EGeoMaterial::Sandstone},
			16, 4, 15.0f, FVector2D(25, 75), 25, 1246.0f);

		// Tier 5 — Rare
		Add(EGeoOreType::Molybdenum, 15.0f, 40.0f,
			{EGeoBiome::Mountains, EGeoBiome::Volcanic},
			{EGeoMaterial::Granite},
			10, 3, 6.0f, FVector2D(35, 85), 40, 2623.0f);

		Add(EGeoOreType::Titanium, 12.0f, 35.0f,
			{EGeoBiome::Mountains, EGeoBiome::Volcanic},
			{EGeoMaterial::Granite, EGeoMaterial::Basalt},
			12, 3, 7.0f, FVector2D(35, 85), 40, 1668.0f);

		Add(EGeoOreType::Tungsten, 15.0f, 40.0f,
			{EGeoBiome::Mountains},
			{EGeoMaterial::Granite},
			8, 2, 4.0f, FVector2D(40, 90), 45, 3422.0f);

		Add(EGeoOreType::Vanadium, 12.0f, 30.0f,
			{EGeoBiome::Mountains, EGeoBiome::Volcanic},
			{EGeoMaterial::Basalt},
			10, 3, 6.0f, FVector2D(35, 85), 40, 1910.0f);

		// Tier 6 — Very Rare / Precious
		Add(EGeoOreType::Platinum, 18.0f, 45.0f,
			{EGeoBiome::Mountains},
			{EGeoMaterial::Granite},
			6, 2, 2.0f, FVector2D(45, 95), 55, 1768.0f);

		Add(EGeoOreType::Antimony, 8.0f, 20.0f,
			{EGeoBiome::Mountains, EGeoBiome::Hills},
			{EGeoMaterial::Slate, EGeoMaterial::Quartzite},
			10, 3, 4.0f, FVector2D(30, 75), 35, 630.0f);

		Add(EGeoOreType::Bismuth, 10.0f, 25.0f,
			{EGeoBiome::Mountains},
			{EGeoMaterial::Granite, EGeoMaterial::Limestone},
			8, 2, 3.0f, FVector2D(30, 75), 35, 271.0f);

		// Tier 7 — Fantasy / Ultra Rare (Arcane Forge)
		Add(EGeoOreType::Mithril, 25.0f, 50.0f,
			{EGeoBiome::Volcanic, EGeoBiome::Mountains},
			{EGeoMaterial::Basalt, EGeoMaterial::Obsidian},
			3, 1, 0.5f, FVector2D(60, 100), 80, 2800.0f);

		Add(EGeoOreType::Adamantite, 30.0f, 60.0f,
			{EGeoBiome::Volcanic},
			{EGeoMaterial::Obsidian, EGeoMaterial::Basalt},
			2, 1, 0.4f, FVector2D(65, 100), 85, 3200.0f);

		Add(EGeoOreType::Orichalcum, 20.0f, 45.0f,
			{EGeoBiome::Mountains, EGeoBiome::Volcanic},
			{EGeoMaterial::Marble, EGeoMaterial::Granite},
			3, 1, 0.6f, FVector2D(55, 98), 75, 2600.0f);

		Add(EGeoOreType::StarMetal, 35.0f, 70.0f,
			{EGeoBiome::Volcanic},
			{EGeoMaterial::Obsidian, EGeoMaterial::Basalt, EGeoMaterial::Granite},
			1, 1, 0.2f, FVector2D(70, 100), 90, 3500.0f);

		return Rules;
	}

	/** Build default biome geological profiles. */
	inline TArray<FGeoBiomeProfile> BuildBiomeProfiles()
	{
		TArray<FGeoBiomeProfile> Profiles;

		auto MakeProfile = [&](EGeoBiome Biome, float WaterDepth, float SoilQ,
		                       TArray<FGeoLayerDef> InLayers)
		{
			FGeoBiomeProfile P;
			P.Biome            = Biome;
			P.WaterTableDepth  = WaterDepth;
			P.BaseSoilQuality  = SoilQ;
			P.Layers           = MoveTemp(InLayers);
			Profiles.Add(P);
		};

		auto L = [](EGeoMaterial M, float Min, float Max, float QMin = 30.0f, float QMax = 70.0f)
		{
			FGeoLayerDef D;
			D.Material     = M;
			D.MinDepth     = Min;
			D.MaxDepth     = Max;
			D.QualityRange = FVector2D(QMin, QMax);
			return D;
		};

		// Plains: thick topsoil, clay, sand, limestone
		MakeProfile(EGeoBiome::Plains, 10.0f, 55.0f, {
			L(EGeoMaterial::Topsoil,    0.0f, 1.5f, 40, 65),
			L(EGeoMaterial::Clay,       1.5f, 4.0f, 35, 60),
			L(EGeoMaterial::Sand,       4.0f, 7.0f, 30, 55),
			L(EGeoMaterial::Limestone,  7.0f, 100.0f, 40, 80)
		});

		// Forest: deep forest soil, clay, sandstone, granite
		MakeProfile(EGeoBiome::Forest, 14.0f, 75.0f, {
			L(EGeoMaterial::ForestSoil, 0.0f, 2.5f, 55, 90),
			L(EGeoMaterial::Clay,       2.5f, 5.0f, 40, 70),
			L(EGeoMaterial::Sandstone,  5.0f, 12.0f, 35, 65),
			L(EGeoMaterial::Granite,    12.0f, 100.0f, 45, 85)
		});

		// Hills: thin topsoil, gravel, slate, granite
		MakeProfile(EGeoBiome::Hills, 18.0f, 40.0f, {
			L(EGeoMaterial::Topsoil,    0.0f, 0.8f, 30, 50),
			L(EGeoMaterial::Gravel,     0.8f, 2.5f, 25, 45),
			L(EGeoMaterial::Slate,      2.5f, 8.0f, 35, 65),
			L(EGeoMaterial::Granite,    8.0f, 100.0f, 50, 90)
		});

		// Mountains: very thin soil, straight to granite
		MakeProfile(EGeoBiome::Mountains, 25.0f, 25.0f, {
			L(EGeoMaterial::Topsoil,    0.0f, 0.3f, 15, 35),
			L(EGeoMaterial::Gravel,     0.3f, 1.0f, 20, 40),
			L(EGeoMaterial::Granite,    1.0f, 100.0f, 55, 95)
		});

		// Swamp: fertile soil, clay, sand, limestone
		MakeProfile(EGeoBiome::Swamp, 3.0f, 65.0f, {
			L(EGeoMaterial::FertileSoil, 0.0f, 2.0f, 60, 85),
			L(EGeoMaterial::Clay,        2.0f, 6.0f, 45, 75),
			L(EGeoMaterial::Sand,        6.0f, 10.0f, 30, 55),
			L(EGeoMaterial::Limestone,   10.0f, 100.0f, 35, 65)
		});

		// Desert: sand all the way, some sandstone
		MakeProfile(EGeoBiome::Desert, 20.0f, 10.0f, {
			L(EGeoMaterial::Sand,       0.0f, 6.0f, 15, 35),
			L(EGeoMaterial::Sandstone,  6.0f, 15.0f, 30, 60),
			L(EGeoMaterial::Limestone,  15.0f, 100.0f, 35, 65)
		});

		// Volcanic: thin ash soil, basalt all the way
		MakeProfile(EGeoBiome::Volcanic, 30.0f, 20.0f, {
			L(EGeoMaterial::Topsoil,    0.0f, 0.5f, 10, 30),
			L(EGeoMaterial::Gravel,     0.5f, 2.0f, 20, 40),
			L(EGeoMaterial::Basalt,     2.0f, 100.0f, 60, 95)
		});

		// Tundra: frozen topsoil, gravel, granite
		MakeProfile(EGeoBiome::Tundra, 30.0f, 15.0f, {
			L(EGeoMaterial::Topsoil,    0.0f, 0.5f, 10, 25),
			L(EGeoMaterial::Gravel,     0.5f, 3.0f, 15, 35),
			L(EGeoMaterial::Slate,      3.0f, 10.0f, 30, 60),
			L(EGeoMaterial::Granite,    10.0f, 100.0f, 45, 80)
		});

		// River Valley: sand, gravel, clay, limestone
		MakeProfile(EGeoBiome::River, 5.0f, 60.0f, {
			L(EGeoMaterial::FertileSoil, 0.0f, 1.5f, 55, 80),
			L(EGeoMaterial::Sand,        1.5f, 4.0f, 35, 55),
			L(EGeoMaterial::Gravel,      4.0f, 7.0f, 25, 45),
			L(EGeoMaterial::Limestone,   7.0f, 100.0f, 40, 70)
		});

		// Coast: sand, clay, sandstone
		MakeProfile(EGeoBiome::Coast, 4.0f, 35.0f, {
			L(EGeoMaterial::Sand,       0.0f, 5.0f, 20, 45),
			L(EGeoMaterial::Clay,       5.0f, 10.0f, 30, 55),
			L(EGeoMaterial::Sandstone,  10.0f, 100.0f, 35, 65)
		});

		return Profiles;
	}
}
