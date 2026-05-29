// AoCWorldGenerator.h
// Architect of Creation - World Generator
// Editor-facing actor that configures procedural underground generation.
// Place ONE of these in any level to control seed, ore rarity, soil layers, and vein sizes.
// Works with AoCVoxelWorld to generate deterministic underground terrain.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCWorldGenerator.generated.h"

class AAoCVoxelWorld;

// ─── Structs ─────────────────────────────────────────────────────────────────

/** Per-ore rarity configuration exposed in the editor Details panel. */
USTRUCT(BlueprintType)
struct AOC_API FOreRarityEntry
{
	GENERATED_BODY()

	/** The ore material this entry controls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore")
	EVoxelMaterial OreMaterial = EVoxelMaterial::Air;

	/** Display name for the editor UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore")
	FString DisplayName;

	/**
	 * Spawn frequency weight (0.0 = never, 1.0 = very common).
	 * Iron might be 0.8 while Osmium might be 0.01.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Frequency = 0.1f;

	/** Minimum depth below surface (in meters) where this ore can appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0.0"))
	float MinDepthMeters = 5.f;

	/** Maximum depth below surface (in meters) for this ore. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0.0"))
	float MaxDepthMeters = 0.f;

	/** Minimum vein radius in meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float MinVeinRadiusMeters = 1.f;

	/** Maximum vein radius in meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0.5", ClampMax = "50.0"))
	float MaxVeinRadiusMeters = 3.f;

	/** Minimum ore units per vein. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "10"))
	int32 MinOrePerVein = 50;

	/** Maximum ore units per vein. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "10"))
	int32 MaxOrePerVein = 200;

	/** Minimum quality of veins for this ore (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 MinQuality = 10;

	/** Maximum quality of veins for this ore (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 MaxQuality = 100;

	/** Progression tier (1-6). Used for sorting in editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldGen|Ore", meta = (ClampMin = "1", ClampMax = "6"))
	uint8 Tier = 1;

	FOreRarityEntry() = default;

	FOreRarityEntry(EVoxelMaterial InMat, const FString& InName, float InFreq,
		float InMinDepth, float InMaxDepth, float InMinRadius, float InMaxRadius,
		int32 InMinOre, int32 InMaxOre, uint8 InMinQ, uint8 InMaxQ, uint8 InTier)
		: OreMaterial(InMat), DisplayName(InName), Frequency(InFreq)
		, MinDepthMeters(InMinDepth), MaxDepthMeters(InMaxDepth)
		, MinVeinRadiusMeters(InMinRadius), MaxVeinRadiusMeters(InMaxRadius)
		, MinOrePerVein(InMinOre), MaxOrePerVein(InMaxOre)
		, MinQuality(InMinQ), MaxQuality(InMaxQ), Tier(InTier)
	{}
};

/** Soil layer configuration. */
USTRUCT(BlueprintType)
struct AOC_API FSoilLayerConfig
{
	GENERATED_BODY()

	/** Depth of topsoil in meters from the surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float TopsoilDepthMeters = 1.5f;

	/** Depth of forest soil layer under forested areas (meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float ForestSoilDepthMeters = 2.0f;

	/** Depth of clay deposits near water (meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float ClayDepthMeters = 1.5f;

	/** Depth of sand deposits at coastlines/rivers (meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float SandDepthMeters = 1.0f;

	/** Depth at which rock layer begins (meters below surface). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "2.0", ClampMax = "20.0"))
	float RockTransitionDepthMeters = 4.0f;

	/** Base quality of regular topsoil (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 BaseTopsoilQuality = 40;

	/** Base quality of forest soil (0-100). Higher = better farming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0", ClampMax = "100"))
	uint8 BaseForestSoilQuality = 70;

	/** Quality variation range (+/- this value from base). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil", meta = (ClampMin = "0", ClampMax = "30"))
	uint8 QualityVariation = 15;
};

// ─── World Generator Actor ──────────────────────────────────────────────────

/**
 * Place ONE AoCWorldGenerator in your level to configure procedural underground generation.
 *
 * Workflow:
 *   1. Drag into level
 *   2. Set WorldSeed (any integer — same seed = same world)
 *   3. Adjust ore rarity sliders per metal
 *   4. Adjust soil layer depths
 *   5. Click "Generate Preview" in editor to see debug overlay of vein placements
 *   6. Happy? Save the level. Done.
 *
 * At runtime, chunks generate on-demand as the player explores.
 * Only modified chunks (player dug) are saved — unmodified chunks
 * regenerate identically from the seed.
 */
UCLASS(BlueprintType, Blueprintable)
class AOC_API AAoCWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	AAoCWorldGenerator();

	virtual void BeginPlay() override;

	// ── Core Settings ───────────────────────────────────────────────────────

	/**
	 * World seed — one number drives all procedural generation.
	 * Same seed = same ore placement every time.
	 * Change seed = completely different world.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Core")
	int32 WorldSeed;

	/** Map size in chunks per axis. Total underground area = MapSize² chunks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Core", meta = (ClampMin = "4", ClampMax = "256"))
	int32 MapSizeChunks;

	/** Maximum underground depth in chunks (how deep the voxel world extends). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Core", meta = (ClampMin = "2", ClampMax = "32"))
	int32 MaxDepthChunks;

	// ── Soil Configuration ──────────────────────────────────────────────────

	/** Soil layer depths and quality settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Soil")
	FSoilLayerConfig SoilConfig;

	// ── Ore Rarity Table ────────────────────────────────────────────────────

	/**
	 * Per-ore rarity configuration.
	 * Adjust frequency sliders to control how common each ore is.
	 * Initialized with realistic defaults for all 22 metals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Ore")
	TArray<FOreRarityEntry> OreRarityTable;

	// ── Noise Configuration ─────────────────────────────────────────────────

	/** Perlin noise frequency for ore vein placement. Lower = larger clusters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Noise", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float OreNoiseFrequency;

	/** Perlin noise octaves for vein shape variation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 OreNoiseOctaves;

	/** Density noise frequency for terrain variation within rock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Noise", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float TerrainNoiseFrequency;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Initialize the VoxelWorld with this generator's settings.
	 * Called automatically in BeginPlay if VoxelWorld reference is set.
	 * Can also be called manually.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void ApplyToVoxelWorld();

	/**
	 * Generate a voxel data block for a specific chunk coordinate.
	 * Uses WorldSeed + chunk coord for deterministic generation.
	 * Called by VoxelWorld when a new chunk needs to be created.
	 *
	 * @param ChunkCoord  The chunk coordinate to generate data for.
	 * @param OutVoxels   Output array of CHUNK_VOLUME voxels.
	 * @param OutVeins    Output array of ore veins found in this chunk.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void GenerateChunkData(FIntVector ChunkCoord, TArray<FVoxelData>& OutVoxels, TArray<FVeinData>& OutVeins);

	/**
	 * Determine the soil material at a given depth below the surface.
	 * Accounts for biome type (forest, coast, etc.) via noise sampling.
	 *
	 * @param WorldPos      World-space position being queried.
	 * @param DepthMeters   Depth below the surface in meters.
	 * @param OutQuality    Output quality value for the soil.
	 * @return The EVoxelMaterial for the soil/rock at that depth.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	EVoxelMaterial GetMaterialAtDepth(FVector WorldPos, float DepthMeters, uint8& OutQuality) const;

	/**
	 * Check if a given position should contain ore based on noise sampling.
	 * If yes, returns the ore entry and quality.
	 *
	 * @param WorldPos       World-space position.
	 * @param OutOreEntry    The ore rarity entry if ore is present.
	 * @param OutQuality     Quality of the ore at this location.
	 * @return True if ore should be placed here.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	bool SampleOreAt(FVector WorldPos, FOreRarityEntry& OutOreEntry, uint8& OutQuality) const;

	/**
	 * Get the ore rarity entry for a specific material.
	 * @param Material  The ore material to look up.
	 * @return Pointer to the entry, or nullptr if not found.
	 */
	const FOreRarityEntry* GetOreEntry(EVoxelMaterial Material) const;

	/**
	 * Randomize the world seed. Useful for "New Random World" button.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldGen")
	void RandomizeSeed();

	/**
	 * Get the total number of ore types configured.
	 */
	UFUNCTION(BlueprintPure, Category = "WorldGen")
	int32 GetOreTypeCount() const;

#if WITH_EDITOR
	/**
	 * Generate debug preview of ore vein placements.
	 * Draws debug spheres in the editor viewport for 10 seconds.
	 * Editor-only — does nothing in packaged builds.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldGen|Preview")
	void GeneratePreview();

	/**
	 * Clear the debug preview overlay.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "WorldGen|Preview")
	void ClearPreview();
#endif

protected:

	/** Reference to the VoxelWorld actor in the level. Auto-found in BeginPlay if not set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldGen|Core")
	AAoCVoxelWorld* VoxelWorld;

private:

	/** Populate OreRarityTable with default values for all 22 metals. */
	void InitializeDefaultOreTable();

	/**
	 * Deterministic random stream seeded from WorldSeed + chunk coordinate.
	 * Guarantees same results for same seed + position.
	 */
	FRandomStream GetChunkRandom(FIntVector ChunkCoord) const;

	/**
	 * Simple 3D Perlin-like noise sampling using FMath::PerlinNoise3D.
	 * @param WorldPos   World position to sample.
	 * @param Frequency  Noise frequency.
	 * @param Seed       Additional seed offset.
	 * @return Noise value in range [0.0, 1.0].
	 */
	float SampleNoise3D(FVector WorldPos, float Frequency, int32 Seed) const;

	/**
	 * Multi-octave noise for more natural vein shapes.
	 * @param WorldPos   World position to sample.
	 * @param Frequency  Base frequency.
	 * @param Octaves    Number of octaves.
	 * @param Seed       Additional seed offset.
	 * @return Noise value in range [0.0, 1.0].
	 */
	float SampleNoiseMultiOctave(FVector WorldPos, float Frequency, int32 Octaves, int32 Seed) const;

	/** Check if a world position is near water (for clay/sand placement). */
	bool IsNearWater(FVector WorldPos) const;

	/** Check if a world position is in a forested area (for forest soil). */
	bool IsForested(FVector WorldPos) const;

	/** Get surface height at a world XY position (queries landscape or returns default). */
	float GetSurfaceHeight(float WorldX, float WorldY) const;
};
