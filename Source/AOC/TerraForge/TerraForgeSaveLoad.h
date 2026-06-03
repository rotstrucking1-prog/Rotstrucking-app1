// TerraForgeSaveLoad.h
// TerraForge — Persistence system.
// Saves/loads modified chunks, ore veins, mine supports, mega nodes.
// Atomic writes (temp → rename) prevent corruption.
// Delta encoding for efficient storage.

#pragma once

#include "CoreMinimal.h"
#include "TerraForgeTypes.h"
#include "TerraForgeSaveLoad.generated.h"

class UTerraForgeSubsystem;
class FTerraForgeChunk;

/**
 * Save file header for version tracking.
 */
USTRUCT()
struct FTerraForgeSaveHeader
{
	GENERATED_BODY()

	/** Magic number for file validation. */
	UPROPERTY()
	uint32 Magic = 0x54464F52; // "TFOR"

	/** Format version. */
	UPROPERTY()
	int32 Version = 1;

	/** World seed. */
	UPROPERTY()
	int32 WorldSeed = 0;

	/** Number of saved chunks. */
	UPROPERTY()
	int32 ChunkCount = 0;

	/** Number of saved ore veins. */
	UPROPERTY()
	int32 VeinCount = 0;

	/** Number of saved mine supports. */
	UPROPERTY()
	int32 SupportCount = 0;

	/** Timestamp. */
	UPROPERTY()
	FDateTime SaveTime;
};

/**
 * Compressed chunk save data.
 * Uses delta encoding: only stores differences from default generation.
 */
USTRUCT()
struct FChunkSaveData
{
	GENERATED_BODY()

	/** Chunk coordinate. */
	UPROPERTY()
	FIntVector ChunkCoord = FIntVector::ZeroValue;

	/** Compressed voxel data (RLE delta encoded). */
	UPROPERTY()
	TArray<uint8> CompressedData;

	/** Timestamp of last modification. */
	UPROPERTY()
	FDateTime LastModified;

	/** Is this on claimed land? (prevents nature reclaim). */
	UPROPERTY()
	bool bOnClaimedLand = false;

	/** Uncompressed size (for allocation). */
	UPROPERTY()
	int32 UncompressedSize = 0;
};

/**
 * TerraForge persistence manager.
 *
 * Features:
 * - Atomic saves (write to .tmp, rename to .sav)
 * - Delta encoding (store only changes from default)
 * - RLE compression (~70% smaller)
 * - Incremental saves (only dirty chunks)
 * - Auto-save on interval
 * - Nature reclaim: unclaimed chunks older than 7 days are deleted
 */
UCLASS()
class AOC_API UTerraForgeSaveLoad : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize with save directory path. */
	void Initialize(const FString& InSaveDir, UTerraForgeSubsystem* InSubsystem);

	/** Save all modified data. Returns true on success. */
	bool SaveAll();

	/** Save only dirty (recently modified) chunks. */
	bool SaveDirtyChunks();

	/** Load all saved data. Returns true on success. */
	bool LoadAll();

	/** Load a specific chunk's data. Returns true if found. */
	bool LoadChunk(const FIntVector& ChunkCoord, FTerraForgeChunk* OutChunk);

	/** Save a specific chunk. */
	bool SaveChunk(const FIntVector& ChunkCoord, const FTerraForgeChunk* Chunk);

	/** Delete a chunk's save data (for nature reclaim). */
	void DeleteChunkData(const FIntVector& ChunkCoord);

	/** Process nature reclaim — delete old unclaimed chunks. */
	void ProcessNatureReclaim();

	/** Get save file size on disk (bytes). */
	int64 GetSaveFileSize() const;

	/** Get total number of saved chunks. */
	int32 GetSavedChunkCount() const { return SavedChunks.Num(); }

	// ── Configuration ───────────────────────────────────────────────────────

	/** Auto-save interval (seconds). 0 = disabled. */
	UPROPERTY(EditAnywhere, Category = "TerraForge|Save")
	float AutoSaveInterval = 120.0f; // 2 minutes

	/** Nature reclaim time (seconds). 7 days default. */
	UPROPERTY(EditAnywhere, Category = "TerraForge|Save")
	float NatureReclaimTime = 604800.0f; // 7 days

	/** Tick save system (call from subsystem). */
	void Tick(float DeltaTime);

private:
	/** Save directory path. */
	FString SaveDir;

	/** Reference to subsystem. */
	UPROPERTY()
	TObjectPtr<UTerraForgeSubsystem> Subsystem;

	/** All saved chunk data. */
	TMap<FIntVector, FChunkSaveData> SavedChunks;

	/** Dirty chunks needing save. */
	TSet<FIntVector> DirtyChunks;

	/** Auto-save timer. */
	float AutoSaveTimer = 0.0f;

	// ── Compression ─────────────────────────────────────────────────────────

	/** RLE compress voxel data. */
	static TArray<uint8> CompressRLE(const TArray<uint8>& RawData);

	/** RLE decompress voxel data. */
	static TArray<uint8> DecompressRLE(const TArray<uint8>& CompressedData, int32 UncompressedSize);

	/** Delta encode chunk data against default generation. */
	TArray<uint8> DeltaEncode(const FTerraForgeChunk* Chunk) const;

	/** Delta decode chunk data to restore from default. */
	void DeltaDecode(const TArray<uint8>& DeltaData, FTerraForgeChunk* OutChunk) const;

	// ── File I/O ────────────────────────────────────────────────────────────

	/** Get the file path for a chunk. */
	FString GetChunkFilePath(const FIntVector& ChunkCoord) const;

	/** Get the main save file path. */
	FString GetMainSaveFilePath() const;

	/** Atomic write: write to .tmp then rename. */
	bool AtomicWriteFile(const FString& FilePath, const TArray<uint8>& Data);

	/** Validate save file integrity. */
	bool ValidateSaveFile(const TArray<uint8>& Data) const;
};
