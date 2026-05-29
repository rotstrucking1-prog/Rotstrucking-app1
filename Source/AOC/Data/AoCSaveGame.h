// AoCSaveGame.h
// Architect of Creation - Save Game System
// Serializes persistent world state: voxel modifications, veins, supports, farms, terrain.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AoCSaveGame.generated.h"

// ─── Helper Structs ──────────────────────────────────────────────────────────

/** Wrapper for TArray<uint8> so it can be used as a TMap value (UHT limitation). */
USTRUCT(BlueprintType)
struct AOC_API FChunkVoxelData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Voxel")
	TArray<uint8> DensityData;

	FChunkVoxelData() = default;
};

USTRUCT(BlueprintType)
struct AOC_API FVeinSaveData
{
	GENERATED_BODY()

	/** World location of the vein centre. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Vein")
	FVector Location = FVector::ZeroVector;

	/** Material / metal ID (e.g. "Copper"). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Vein")
	FName Material = NAME_None;

	/** Ore units remaining in this vein. 0 = depleted. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Vein")
	int32 RemainingOre = 0;

	FVeinSaveData() = default;

	FVeinSaveData(const FVector& InLoc, FName InMat, int32 InOre)
		: Location(InLoc)
		, Material(InMat)
		, RemainingOre(InOre)
	{}
};

USTRUCT(BlueprintType)
struct AOC_API FSupportSaveData
{
	GENERATED_BODY()

	/** World location of the support structure. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Support")
	FVector Location = FVector::ZeroVector;

	/** "Beam" or "Column". */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Support")
	FName SupportType = NAME_None;

	/** Current structural HP (0 = destroyed). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Support")
	float Health = 100.0f;

	FSupportSaveData() = default;

	FSupportSaveData(const FVector& InLoc, FName InType, float InHP)
		: Location(InLoc)
		, SupportType(InType)
		, Health(InHP)
	{}
};

USTRUCT(BlueprintType)
struct AOC_API FFarmTileSaveData
{
	GENERATED_BODY()

	/** World location of the farming tile. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Farm")
	FVector Location = FVector::ZeroVector;

	/** Tile state index (maps to EFarmTileState). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Farm")
	uint8 TileState = 0;

	/** Planted crop ID (NAME_None if empty). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Farm")
	FName CropID = NAME_None;

	/** 0-1 growth progress toward harvest. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Farm")
	float GrowthProgress = 0.0f;

	/** Soil quality value (0-100). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Farm")
	float SoilQuality = 0.0f;

	/** Current water level (0-1). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Farm")
	float WaterLevel = 0.0f;

	FFarmTileSaveData() = default;
};

USTRUCT(BlueprintType)
struct AOC_API FTerrainModification
{
	GENERATED_BODY()

	/** Grid coordinate of the modified terrain tile. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Terrain")
	FIntVector TileCoord = FIntVector::ZeroValue;

	/**
	 * Height deltas for 9 control points in a 3×3 grid per tile.
	 * Order: NW, N, NE, W, Centre, E, SW, S, SE.
	 */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Save|Terrain")
	TArray<float> HeightDeltas;

	FTerrainModification()
	{
		HeightDeltas.SetNum(9);
		for (int32 i = 0; i < 9; ++i)
		{
			HeightDeltas[i] = 0.0f;
		}
	}
};

// ─── Save Game ───────────────────────────────────────────────────────────────

UCLASS()
class AOC_API UAoCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UAoCSaveGame();

	// ── Saved data ──────────────────────────────────────────────────────────

	/** Voxel density modifications keyed by chunk coordinate. Only stores chunks the player has changed. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Voxel")
	TMap<FIntVector, FChunkVoxelData> ModifiedChunks;

	/** Ore veins that have been partially or fully depleted. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Mining")
	TArray<FVeinSaveData> DepletedVeins;

	/** Mine support structures placed by the player. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Mining")
	TArray<FSupportSaveData> PlacedSupports;

	/** All farming tile states. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Farming")
	TArray<FFarmTileSaveData> FarmTiles;

	/** Surface terraforming modifications. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Terrain")
	TArray<FTerrainModification> TerrainMods;

	/** User-facing slot name (e.g. "Slot_01"). */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Meta")
	FString SaveSlotName;

	/** Schema version — incremented when save format changes, used for migration. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Meta")
	int32 SaveVersion;

	/** UTC timestamp of last save. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Save|Meta")
	FDateTime SaveTimestamp;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Iterate relevant actors in the world and serialize their state into this object.
	 * Call before SaveToSlot().
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveWorld(UWorld* World);

	/**
	 * Deserialize saved data and apply it to the world (spawn actors, set states).
	 * Call after LoadFromSlot().
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void LoadWorld(UWorld* World);

	/**
	 * Persist this save object to disk via UGameplayStatics.
	 * @param SlotName  Disk slot name (e.g. "Slot_01").
	 * @return True on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveToSlot(const FString& SlotName);

	/**
	 * Load a save game from disk.
	 * @param SlotName  Disk slot to read.
	 * @return Loaded save object, or nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	static UAoCSaveGame* LoadFromSlot(const FString& SlotName);

	/**
	 * Estimate the serialized byte size of this save (for UI display).
	 */
	UFUNCTION(BlueprintPure, Category = "Save")
	int64 GetSaveSize() const;

private:

	/** Current save format version. Bump when struct layout changes. */
	static constexpr int32 CurrentSaveVersion = 1;

	/** Helper — gather all farming tiles in the world. */
	void SerializeFarmTiles(UWorld* World);

	/** Helper — gather depleted resource nodes (veins). */
	void SerializeVeins(UWorld* World);

	/** Helper — gather placed mine supports. */
	void SerializeSupports(UWorld* World);

	/** Helper — gather terrain modifications. */
	void SerializeTerrainMods(UWorld* World);

	/** Helper — apply farming tile data to existing actors or spawn new ones. */
	void DeserializeFarmTiles(UWorld* World);

	/** Helper — restore depleted vein states. */
	void DeserializeVeins(UWorld* World);

	/** Helper — spawn support actors from saved data. */
	void DeserializeSupports(UWorld* World);

	/** Helper — apply terrain height deltas. */
	void DeserializeTerrainMods(UWorld* World);
};
