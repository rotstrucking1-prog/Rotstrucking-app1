// AoCVoxelWorld.h
// Architect of Creation - Voxel World Manager
// Manages chunk loading/unloading, ore vein placement, and world-level voxel operations.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCVoxelWorld.generated.h"

class AAoCVoxelChunk;

UCLASS()
class AOC_API AAoCVoxelWorld : public AActor
{
	GENERATED_BODY()

public:
	AAoCVoxelWorld();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Maximum render distance in chunks from the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World")
	int32 RenderDistance;

	/** Class to spawn for chunks (defaults to AAoCVoxelChunk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World")
	TSubclassOf<AAoCVoxelChunk> ChunkClass;

	/** Material applied to all chunk procedural meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World")
	UMaterialInterface* WorldMaterial;

	/** Maximum chunks to spawn per frame (throttle to avoid hitches). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World|Performance", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxChunkSpawnsPerFrame;

	/** Maximum chunk pool size for recycling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World|Performance", meta = (ClampMin = "0", ClampMax = "200"))
	int32 MaxPoolSize;

	/** Interval (seconds) between chunk update passes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World|Performance", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ChunkUpdateInterval;

	/** World seed for deterministic terrain generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|World")
	int32 WorldSeed;

	/** All active ore veins in the world (persisted for save/load). */
	UPROPERTY(BlueprintReadOnly, Category = "Voxel|World")
	TArray<FVeinData> OreVeins;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Update which chunks are loaded based on the player's current position.
	 * Loads new chunks within render distance, unloads distant ones.
	 * @param PlayerLocation Current player world position
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	void UpdateChunks(FVector PlayerLocation);

	/**
	 * Get the voxel data at a specific world position.
	 * @param WorldPos World-space position to query
	 * @return Voxel data at that location (Air if chunk not loaded)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|World")
	FVoxelData GetVoxelAt(FVector WorldPos) const;

	/**
	 * Set voxel data at a specific world position.
	 * @param WorldPos World-space position to modify
	 * @param Data New voxel data
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	void SetVoxelAt(FVector WorldPos, FVoxelData Data);

	/**
	 * Carve a sphere of air at a world position across all affected chunks.
	 * @param WorldPos Center of the carve sphere
	 * @param Radius Radius in centimeters
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	void CarveAt(FVector WorldPos, float Radius);

	/**
	 * Get the chunk at a specific chunk coordinate, or nullptr if not loaded.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|World")
	AAoCVoxelChunk* GetChunkAt(FIntVector ChunkCoord) const;

	/**
	 * Force-load a chunk at the given coordinate (creates if not loaded).
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	AAoCVoxelChunk* ForceLoadChunk(FIntVector ChunkCoord);

	/**
	 * Generate ore veins for a chunk region.
	 * Called automatically when a chunk is initialized.
	 * @param ChunkCoord The chunk coordinate to generate veins for
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	void GenerateOreVeins(FIntVector ChunkCoord);

	/**
	 * Prospect for ore veins near a world position.
	 * @param WorldPos Center of search
	 * @param SearchRadius Radius in centimeters
	 * @param OutVeins Found veins within radius
	 * @return True if any veins were found
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	bool ProspectAt(FVector WorldPos, float SearchRadius, TArray<FVeinData>& OutVeins) const;

	/**
	 * Get the number of currently loaded chunks.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|World")
	int32 GetLoadedChunkCount() const;

	/**
	 * Get the number of chunks in the pool (recycled, not in use).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voxel|World")
	int32 GetPooledChunkCount() const;

private:
	/** Currently loaded/active chunks, keyed by chunk coordinate. */
	UPROPERTY(VisibleInstanceOnly, Category = "Voxel|World|Internal")
	TMap<FIntVector, AAoCVoxelChunk*> LoadedChunks;

	/** Pool of deactivated chunks available for reuse. */
	UPROPERTY(VisibleInstanceOnly, Category = "Voxel|World|Internal")
	TArray<AAoCVoxelChunk*> ChunkPool;

	/** Tracks the last known player chunk coordinate to avoid redundant updates. */
	FIntVector LastPlayerChunkCoord;

	/** Timer for throttled chunk updates. */
	float ChunkUpdateTimer;

	/** Set of chunk coordinates that already had ore veins generated. */
	TSet<FIntVector> VeinGeneratedChunks;

	/** Pending chunk coordinates that need to be loaded. */
	TArray<FIntVector> PendingChunkLoads;

	// ── Internal Methods ────────────────────────────────────────────────────

	/** Spawn or retrieve a chunk from the pool. */
	AAoCVoxelChunk* AcquireChunk();

	/** Return a chunk to the pool for recycling. */
	void ReleaseChunk(AAoCVoxelChunk* Chunk);

	/** Calculate LOD level based on chunk distance from the player chunk. */
	int32 CalculateLOD(FIntVector ChunkCoord, FIntVector PlayerChunkCoord) const;

	/** Apply ore vein data to a specific chunk's voxel grid. */
	void ApplyVeinsToChunk(AAoCVoxelChunk* Chunk);

	/** Deterministic random stream seeded per chunk coordinate. */
	FRandomStream GetChunkRandom(FIntVector ChunkCoord) const;
};
