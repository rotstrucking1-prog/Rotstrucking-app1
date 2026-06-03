// TerraForgeSubsystem.h
// TerraForge — World subsystem managing terrain modification,
// chunk management, and landscape integration.
//
// ARCHITECTURE (v2 — Heightmap-based surface terraforming):
// Surface: Direct landscape heightmap texture modification (same as UE5 sculpt tools).
// Underground: Voxel chunk system + ProceduralMesh (kept for tunnels/ore).
// No ProceduralMesh replacement of surface terrain. No landscape hiding.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TerraForgeTypes.h"
#include "TerraForgeChunk.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Texture2D.h"
#include "TerraForgeSubsystem.generated.h"

class ALandscapeProxy;
class ULandscapeComponent;
class UProceduralMeshComponent;
class UTerraForgeGeology;
class UTerraForgeStructural;
class UTerraForgeOreGen;
class UTerraForgeSaveLoad;

/**
 * Central TerraForge subsystem — one per world.
 *
 * Responsibilities:
 * - Sparse chunk hashmap (only modified areas exist)
 * - Chunk streaming (load/unload based on player proximity)
 * - Landscape height sampling (initialize chunks from UE5 landscape)
 * - Async mesh generation (worker threads → game thread swap)
 * - ProceduralMeshComponent pooling (zero runtime allocation)
 * - Geological layer generation (material placement)
 * - Ore vein placement (procedural, rule-based)
 * - Terrain regeneration (nature reclaims unclaimed land)
 */
UCLASS()
class AOC_API UTerraForgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	friend class UTerraForgeStructural;
	friend class UTerraForgeSaveLoad;
	friend class UTerraForgeOreGen;

public:
	// ── Lifecycle ───────────────────────────────────────────────────────────

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	/** Called every frame by the game. */
	void TickSubsystem(float DeltaTime);

	// ── Chunk Access ────────────────────────────────────────────────────────

	/** Get or create a chunk at the given world position.
	 *  If the chunk doesn't exist, initializes it from the landscape. */
	FTerraForgeChunk* GetOrCreateChunk(const FVector& WorldPos);

	/** Get a chunk if it exists (returns null if no modifications at this location). */
	FTerraForgeChunk* GetChunk(const FTerraChunkKey& Key) const;

	/** Check if a chunk exists at this position. */
	bool HasChunk(const FTerraChunkKey& Key) const;

	/** Get the total number of active chunks. */
	int32 GetChunkCount() const { return ChunkMap.Num(); }

	// ── Terraforming Actions ────────────────────────────────────────────────

	/** Lower terrain at a world position. Returns material dug out. */
	EGeoMaterial LowerTerrain(const FVector& WorldPos, float Amount = TF_DIG_DEPTH_PER_ACTION);

	/** Raise terrain at a world position using material from inventory. */
	bool RaiseTerrain(const FVector& WorldPos, EGeoMaterial Material, float Amount = TF_DIG_DEPTH_PER_ACTION);

	/** Flatten terrain at a target position to match a reference height. */
	bool FlattenTerrain(const FVector& TargetPos, float ReferenceHeight);

	/** Dig a tunnel opening at a world position in a given direction. */
	bool DigTunnel(const FVector& WorldPos, const FVector& Direction, float Radius = 1.0f);

	/** Mine ore from a voxel. Returns units extracted. */
	int32 MineOre(const FVector& WorldPos, int32 SkillLevel);

	// ── Edit Session Management ────────────────────────────────────────────

	/** Begin an edit session — pre-locks all landscape heightmap textures
	 *  within radius. Subsequent LowerTerrain/RaiseTerrain/FlattenTerrain calls
	 *  modify pre-locked data without per-call lock/unlock/UpdateResource.
	 *  Call EndEditSession() to commit changes to GPU and update collision.
	 *  This eliminates the BulkData double-lock crash caused by UpdateResource()
	 *  internally re-locking textures via InitRHI → LockMip.
	 *  @param Center  World position center of the edit region.
	 *  @param Radius  Radius in world units to pre-lock (default 1000 cm = 10m).
	 *  @return True if session started (or was already active). */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	bool BeginEditSession(const FVector& Center, float Radius = 1000.0f);

	/** End the current edit session — unlocks heightmap textures, pushes
	 *  changes to GPU, and updates landscape collision.
	 *  Other players see terrain changes only after this call. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	void EndEditSession();

	/** Is an edit session currently active? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	bool IsEditSessionActive() const { return bEditSessionActive; }

	// ── Observe Mode ────────────────────────────────────────────────────────

	/** Get observe-mode grid data around a world position.
	 *  Returns an 11×11 grid of tile info. */
	TArray<FObserveTile> GetObserveGrid(const FVector& CenterPos) const;

	/** Get elevation at a world position (from chunk or landscape). */
	float GetElevationAt(const FVector& WorldPos) const;

	/** Get surface material at a world position. */
	EGeoMaterial GetSurfaceMaterialAt(const FVector& WorldPos) const;

	// ── Prospecting ─────────────────────────────────────────────────────────

	/** Prospect for ore around a world position.
	 *  @param Radius    Search radius in meters.
	 *  @param SkillLevel Player's prospecting skill (0-100).
	 *  @return Array of detected ore deposits (detail depends on skill). */
	TArray<FProspectingResult> Prospect(const FVector& WorldPos, float Radius, int32 SkillLevel) const;

	// ── Structural Integrity ────────────────────────────────────────────────

	/** Get structural state at a position. */
	EStructuralState GetStructuralState(const FVector& WorldPos) const;

	/** Check if a position can support mining (enough structural integrity). */
	bool CanMineAt(const FVector& WorldPos) const;

	/** Place a mine support at a position. Returns true if successful. */
	bool PlaceSupport(const FVector& WorldPos);

	// ── Landscape Integration ───────────────────────────────────────────────

	/** Find the landscape actor in the world. */
	ALandscapeProxy* FindLandscape() const;

	/** Sample landscape height at a world XY position (returns Z in cm). */
	float SampleLandscapeHeight(float WorldX, float WorldY) const;

	/** Get the biome at a world position. */
	EGeoBiome GetBiomeAt(const FVector& WorldPos) const;

	// ── Save/Load ───────────────────────────────────────────────────────────

	/** Save all modified chunks to disk. */
	void SaveAllChunks(const FString& SaveSlot);

	/** Load chunks from disk. */
	void LoadChunks(const FString& SaveSlot);

	// ── Configuration ───────────────────────────────────────────────────────

	/** Sub-system: Structural integrity and collapse. */
	UPROPERTY()
	TObjectPtr<UTerraForgeStructural> StructuralSystem;

	/** Sub-system: Ore vein generation. */
	UPROPERTY()
	TObjectPtr<UTerraForgeOreGen> OreGenSystem;

	/** Sub-system: Save/load persistence. */
	UPROPERTY()
	TObjectPtr<UTerraForgeSaveLoad> SaveSystem;

	/** Material property table (loaded at init). */
	UPROPERTY()
	TMap<EGeoMaterial, FGeoMaterialProperties> MaterialTable;

	/** Ore generation rules. */
	TArray<FGeoOreRule> OreRules;

	/** Biome geological profiles. */
	TArray<FGeoBiomeProfile> BiomeProfiles;

	// ── Mesh Pool ───────────────────────────────────────────────────────────

	/** The actor that holds all pooled ProceduralMeshComponents. */
	UPROPERTY()
	TObjectPtr<AActor> MeshPoolActor;

private:
	// ── Chunk Management ────────────────────────────────────────────────────

	/** Sparse hashmap of all active chunks. */
	TMap<FTerraChunkKey, TUniquePtr<FTerraForgeChunk>> ChunkMap;

	/** Queue of chunks needing mesh regeneration. */
	TArray<FTerraChunkKey> DirtyChunkQueue;

	/** ProceduralMeshComponent pool. */
	TArray<UProceduralMeshComponent*> MeshPool;
	TArray<UProceduralMeshComponent*> FreeMeshPool;

	/** Map from chunk key to its assigned ProceduralMeshComponent. */
	TMap<FTerraChunkKey, UProceduralMeshComponent*> ChunkMeshMap;

	/** Currently loaded chunk keys (for streaming). */
	TSet<FTerraChunkKey> LoadedChunkKeys;

	/** Cached landscape reference. */
	UPROPERTY()
	TObjectPtr<ALandscapeProxy> CachedLandscape;

	// ── Internal Operations ─────────────────────────────────────────────────

	/** Initialize the component pool and data tables. */
	void InitializePool();
	void InitializeDataTables();

	/** Update chunk streaming based on player position. */
	void UpdateStreaming(const FVector& PlayerPos);

	/** Process dirty chunk queue (limited per frame). */
	void ProcessDirtyChunks(int32 MaxPerFrame = TF_CHUNKS_PER_FRAME);

	/** Generate mesh for a chunk and apply to ProceduralMeshComponent. */
	void RegenerateMesh(const FTerraChunkKey& Key);

	/** Initialize a new chunk from landscape data. */
	void InitializeChunkFromLandscape(FTerraForgeChunk& Chunk);

	/** Place ore veins in a chunk based on geological rules. */
	void PlaceOreVeinsInChunk(FTerraForgeChunk& Chunk);

	/** Assign a ProceduralMeshComponent from the pool to a chunk. */
	UProceduralMeshComponent* AcquireMeshComponent();

	/** Return a mesh component to the pool. */
	void ReleaseMeshComponent(UProceduralMeshComponent* Comp);

	/** Update neighbor pointers for a chunk and its neighbors. */
	void UpdateNeighborLinks(const FTerraChunkKey& Key);

	/** Update terrain regeneration timers. */
	void UpdateRegeneration(float DeltaTime);

	/** Modify a voxel at a world position and mark the chunk dirty.
	 *  Handles cross-chunk boundaries. */
	FGeoVoxel* GetVoxelAtWorldPos(const FVector& WorldPos, FTerraForgeChunk** OutChunk = nullptr);

	/** Get the biome profile for a given biome type. */
	const FGeoBiomeProfile* GetBiomeProfile(EGeoBiome Biome) const;

	/** Calculate structural stress at a position. */
	float CalculateStress(const FTerraForgeChunk& Chunk, int32 X, int32 Y, int32 Z) const;

	/** Set voxel material at a world position. */
	void SetVoxelMaterial(const FVector& WorldPos, EGeoMaterial Material);

	/** Regenerate meshes within a radius (after collapse/modification). */
	void RegenerateMeshesInRadius(const FVector& Center, float Radius);

	/** Get a loaded chunk by coordinate. Returns nullptr if not loaded. */
	FTerraForgeChunk* GetChunkAt(const FIntVector& ChunkCoord) const;

	// ── Heightmap Surface Terraforming (v2) ─────────────────────────────────

	/** Cache landscape transform data for fast coordinate conversion. */
	void CacheLandscapeTransform();

	/** Find the landscape component that contains a world XY position.
	 *  Uses cached O(1) grid lookup. */
	ULandscapeComponent* FindComponentAtWorldPos(const FVector& WorldPos);

	/** Core heightmap modification: apply brush at world position.
	 *  @param WorldPos        Center of the brush in world space.
	 *  @param DeltaCm         Height change in centimeters (negative=lower, positive=raise).
	 *  @param BrushRadiusPixels Brush radius in heightmap pixels (~1 pixel = 1 meter).
	 *  @param FalloffSigma    Gaussian sigma for edge falloff (in pixels).
	 *  @return True if any heightmap pixels were modified. */
	bool ModifyLandscapeHeight(const FVector& WorldPos, float DeltaCm,
		float BrushRadiusPixels = 1.5f, float FalloffSigma = 0.5f);

	/** Flatten landscape heights in a brush area to a target height.
	 *  @param WorldPos      Center of the brush.
	 *  @param TargetHeightCm Target world Z height in cm.
	 *  @param BrushRadiusPixels Brush radius in heightmap pixels.
	 *  @return True if any heights were modified. */
	bool FlattenLandscapeHeight(const FVector& WorldPos, float TargetHeightCm,
		float BrushRadiusPixels = 1.5f);

	/** Convert world position to landscape heightmap coordinates. */
	FIntPoint WorldToHeightmapCoord(const FVector& WorldPos) const;

	/** Get geological material at a depth below surface (uses biome profile). */
	EGeoMaterial GetGeologicalMaterialAtDepth(const FVector& WorldPos, float DepthMeters) const;

	/** Track a terrain modification for persistence (delta system). */
	void RecordTerrainDelta(const FIntPoint& HeightmapCoord, int16 HeightDelta);

	// ── Cached Landscape Data ───────────────────────────────────────────────

	/** Landscape world-space origin (cached). */
	FVector LandscapeOrigin = FVector::ZeroVector;

	/** Landscape actor scale (cached). */
	FVector LandscapeScale = FVector(100.0f, 100.0f, 100.0f);

	/** Whether landscape transform has been cached. */
	bool bLandscapeTransformCached = false;

	/** Cached component size (quads per side). All components share this value. */
	int32 CachedComponentSizeQuads = 127;

	/** O(1) component lookup: grid index → component pointer. */
	TMap<FIntPoint, ULandscapeComponent*> ComponentMap;

	/** Terrain modification deltas for persistence (heightmap coord → cumulative delta). */
	TMap<FIntPoint, int32> TerrainDeltas;

	/** Original landscape heights (sampled at first dig) for geological material lookup. */
	TMap<FIntPoint, float> OriginalSurfaceHeights;

	// ── Heightmap CPU Cache + Edit Session ──────────────────────────────────

	/** CPU-side copy of a heightmap texture's pixel data.
	 *  Enables runtime-compatible modification without editor-only APIs.
	 *  Persistent across sessions — once cached, stays in memory. */
	struct FHeightmapCache
	{
		TArray<FColor> Pixels;
		int32 SizeX = 0;
		int32 SizeY = 0;
	};

	/** Whether an edit session is currently active. */
	bool bEditSessionActive = false;

	/** CPU-side heightmap data cache per texture.
	 *  Populated on first access via CacheHeightmapTexture().
	 *  Modified during terraforming, pushed to GPU at commit time. */
	TMap<UTexture2D*, FHeightmapCache> HeightmapCPUCache;

	/** Textures modified during current edit session (need GPU push at EndEditSession). */
	TSet<UTexture2D*> SessionDirtyTextures;

	/** All landscape components touched during this edit session (for collision update). */
	TSet<ULandscapeComponent*> SessionAffectedComponents;

	/** Read a heightmap texture into the CPU cache.
	 *  Tries PlatformData BulkData (runtime) then Source (editor fallback).
	 *  @return True if data was successfully read and cached. */
	bool CacheHeightmapTexture(UTexture2D* Tex);

	/** Push modified CPU cache data back to the GPU heightmap texture.
	 *  Tries PlatformData BulkData write (runtime) then Source write (editor fallback). */
	void PushHeightmapToGPU(UTexture2D* Tex);

	/** Update collision for landscape components after height modification. */
	void UpdateCollisionForComponents(const TSet<ULandscapeComponent*>& Components);
};
