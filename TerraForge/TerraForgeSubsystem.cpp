// TerraForgeSubsystem.cpp
// TerraForge — World subsystem implementation.
//
// ARCHITECTURE v2:
// Surface terraforming → direct landscape heightmap texture modification.
// Underground tunnels  → voxel chunks + ProceduralMesh (unchanged).
// No ProceduralMesh replacement for surface. No landscape hiding/destroying.

#include "TerraForgeSubsystem.h"
#include "TerraForgeStructural.h"
#include "TerraForgeOreGen.h"
#include "TerraForgeSaveLoad.h"
#include "TerraForgeDualContour.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "Components/ActorComponent.h"
#include "NavigationSystem.h"
#include "Async/Async.h"
#include "ProceduralMeshComponent.h"
#include "RenderingThread.h"  // FlushRenderingCommands()

// Heightmap constants — use TF_ prefix to avoid collision with UE5 macros
// (LANDSCAPE_ZSCALE is already #defined in LandscapeDataAccess.h)
static constexpr float TF_ZSCALE = 1.0f / 128.0f;
static constexpr uint16 TF_HEIGHT_MID = 32768;

// ============================================================================
// LIFECYCLE
// ============================================================================

void UTerraForgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Subsystem initializing (v2 — heightmap surface)..."));

	InitializeDataTables();
	InitializePool();

	// Create sub-systems
	StructuralSystem = NewObject<UTerraForgeStructural>(this);
	if (StructuralSystem)
	{
		StructuralSystem->Initialize(this);
	}

	OreGenSystem = NewObject<UTerraForgeOreGen>(this);
	if (OreGenSystem)
	{
		OreGenSystem->Initialize(this, FMath::Rand());
	}

	SaveSystem = NewObject<UTerraForgeSaveLoad>(this);
	if (SaveSystem)
	{
		const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TerraForge"));
		SaveSystem->Initialize(SaveDir, this);
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Subsystem initialized. %d material types, %d ore rules, %d biome profiles."),
		MaterialTable.Num(), OreRules.Num(), BiomeProfiles.Num());
}

void UTerraForgeSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("TerraForge: Subsystem deinitializing. %d chunks, %d terrain deltas."),
		ChunkMap.Num(), TerrainDeltas.Num());

	// Clean up chunks
	ChunkMap.Empty();
	ChunkMeshMap.Empty();
	LoadedChunkKeys.Empty();
	DirtyChunkQueue.Empty();

	// Clean up heightmap caches
	ComponentMap.Empty();
	TerrainDeltas.Empty();
	OriginalSurfaceHeights.Empty();
	bLandscapeTransformCached = false;

	// Pool components are owned by the pool actor — destroyed with it
	MeshPool.Empty();
	FreeMeshPool.Empty();

	Super::Deinitialize();
}

void UTerraForgeSubsystem::InitializeDataTables()
{
	MaterialTable = TerraForgeData::BuildMaterialTable();
	OreRules      = TerraForgeData::BuildOreRules();
	BiomeProfiles = TerraForgeData::BuildBiomeProfiles();
}

void UTerraForgeSubsystem::InitializePool()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Create the pool actor to hold all ProceduralMeshComponents (for underground)
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MeshPoolActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (MeshPoolActor)
	{
		MeshPoolActor->SetActorHiddenInGame(false);
#if WITH_EDITOR
		MeshPoolActor->SetActorLabel(TEXT("TerraForge Mesh Pool"));
#endif

		// Pre-allocate mesh components (for underground tunnel rendering)
		const int32 InitialPoolSize = 64;
		MeshPool.Reserve(InitialPoolSize);
		FreeMeshPool.Reserve(InitialPoolSize);

		for (int32 I = 0; I < InitialPoolSize; ++I)
		{
			UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(MeshPoolActor);
			Comp->RegisterComponent();
			Comp->SetVisibility(false);
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Comp->SetCollisionResponseToAllChannels(ECR_Block);
			Comp->bUseComplexAsSimpleCollision = true;
			MeshPool.Add(Comp);
			FreeMeshPool.Add(Comp);
		}

		UE_LOG(LogTemp, Log, TEXT("TerraForge: Mesh pool initialized with %d components."), InitialPoolSize);
	}
}

// ============================================================================
// PER-FRAME UPDATE
// ============================================================================

void UTerraForgeSubsystem::TickSubsystem(float DeltaTime)
{
	// Get player position for streaming
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const FVector PlayerPos = Pawn->GetActorLocation();

	// Core per-frame systems
	UpdateStreaming(PlayerPos);
	ProcessDirtyChunks();
	UpdateRegeneration(DeltaTime);
}

// ============================================================================
// CHUNK ACCESS (unchanged — used for underground/ore tracking)
// ============================================================================

FTerraForgeChunk* UTerraForgeSubsystem::GetOrCreateChunk(const FVector& WorldPos)
{
	const FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);

	TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
	if (Found)
	{
		return Found->Get();
	}

	// Create new chunk
	if (ChunkMap.Num() >= TF_MAX_LOADED_CHUNKS)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: Max chunk limit (%d) reached! Cannot create chunk at %s."),
			TF_MAX_LOADED_CHUNKS, *Key.ToString());
		return nullptr;
	}

	TUniquePtr<FTerraForgeChunk> NewChunk = MakeUnique<FTerraForgeChunk>(Key);
	FTerraForgeChunk* ChunkPtr = NewChunk.Get();

	// Initialize from landscape
	InitializeChunkFromLandscape(*ChunkPtr);

	// Place ore veins based on geological rules
	PlaceOreVeinsInChunk(*ChunkPtr);

	ChunkMap.Add(Key, MoveTemp(NewChunk));

	// Update neighbor links
	UpdateNeighborLinks(Key);

	// Add to dirty queue for mesh generation
	DirtyChunkQueue.AddUnique(Key);
	LoadedChunkKeys.Add(Key);

	UE_LOG(LogTemp, Verbose, TEXT("TerraForge: Created chunk %s (total: %d)"), *Key.ToString(), ChunkMap.Num());

	return ChunkPtr;
}

FTerraForgeChunk* UTerraForgeSubsystem::GetChunk(const FTerraChunkKey& Key) const
{
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
	return Found ? Found->Get() : nullptr;
}

bool UTerraForgeSubsystem::HasChunk(const FTerraChunkKey& Key) const
{
	return ChunkMap.Contains(Key);
}

// ============================================================================
// SURFACE TERRAFORMING ACTIONS (v2 — Heightmap-based)
// ============================================================================

EGeoMaterial UTerraForgeSubsystem::LowerTerrain(const FVector& WorldPos, float Amount)
{
	UE_LOG(LogTemp, Log, TEXT("TerraForge: LowerTerrain at (%.1f, %.1f, %.1f) amount=%.3f"),
		WorldPos.X, WorldPos.Y, WorldPos.Z, Amount);

	// Convert Amount (meters, default 0.1) to centimeters for heightmap delta
	// Negative delta = lower the terrain
	const float DeltaCm = -Amount * 100.0f; // e.g., -10 cm per dig

	// Shovel brush: 1.5 pixel radius (~1.5 meter), sigma 0.5 for Gaussian falloff
	const float BrushRadius = 1.5f;
	const float FalloffSigma = 0.5f;

	// Determine geological material at this depth before modifying
	// Cache original surface height if not cached
	FIntPoint HeightmapCoord = WorldToHeightmapCoord(WorldPos);

	float OriginalSurfaceZ = WorldPos.Z; // Fallback
	if (const float* CachedOriginal = OriginalSurfaceHeights.Find(HeightmapCoord))
	{
		OriginalSurfaceZ = *CachedOriginal;
	}
	else
	{
		// First dig at this location — record original surface height
		OriginalSurfaceZ = SampleLandscapeHeight(WorldPos.X, WorldPos.Y);
		OriginalSurfaceHeights.Add(HeightmapCoord, OriginalSurfaceZ);
	}

	// How deep below original surface are we now?
	const float CurrentDepthCm = OriginalSurfaceZ - WorldPos.Z;
	const float DepthMeters = CurrentDepthCm / 100.0f;

	// Determine what material the player is digging through
	EGeoMaterial DugMaterial = GetGeologicalMaterialAtDepth(WorldPos, DepthMeters);

	// Apply the heightmap modification
	bool bSuccess = ModifyLandscapeHeight(WorldPos, DeltaCm, BrushRadius, FalloffSigma);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: LowerTerrain SUCCESS — depth=%.2fm, material=%d"),
			DepthMeters, (int32)DugMaterial);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: LowerTerrain FAILED — could not modify heightmap"));
		return EGeoMaterial::Air;
	}

	return DugMaterial;
}

bool UTerraForgeSubsystem::RaiseTerrain(const FVector& WorldPos, EGeoMaterial Material, float Amount)
{
	UE_LOG(LogTemp, Log, TEXT("TerraForge: RaiseTerrain at (%.1f, %.1f, %.1f) amount=%.3f material=%d"),
		WorldPos.X, WorldPos.Y, WorldPos.Z, Amount, (int32)Material);

	// Positive delta = raise the terrain
	const float DeltaCm = Amount * 100.0f; // e.g., +10 cm per dump

	// Same brush as shovel lowering
	const float BrushRadius = 1.5f;
	const float FalloffSigma = 0.5f;

	bool bSuccess = ModifyLandscapeHeight(WorldPos, DeltaCm, BrushRadius, FalloffSigma);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: RaiseTerrain SUCCESS"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: RaiseTerrain FAILED"));
	}

	return bSuccess;
}

bool UTerraForgeSubsystem::FlattenTerrain(const FVector& TargetPos, float ReferenceHeight)
{
	UE_LOG(LogTemp, Log, TEXT("TerraForge: FlattenTerrain at (%.1f, %.1f, %.1f) refHeight=%.1f"),
		TargetPos.X, TargetPos.Y, TargetPos.Z, ReferenceHeight);

	// Use a slightly larger brush for flatten (2 pixel radius = ~2m)
	const float BrushRadius = 2.0f;

	bool bSuccess = FlattenLandscapeHeight(TargetPos, ReferenceHeight, BrushRadius);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: FlattenTerrain SUCCESS"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: FlattenTerrain FAILED"));
	}

	return bSuccess;
}

// ============================================================================
// HEIGHTMAP MODIFICATION ENGINE (v2 core)
// ============================================================================

void UTerraForgeSubsystem::CacheLandscapeTransform()
{
	if (!CachedLandscape)
	{
		CachedLandscape = FindLandscape();
	}
	if (!CachedLandscape)
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: CacheLandscapeTransform — no landscape found!"));
		return;
	}

	LandscapeOrigin = CachedLandscape->GetActorLocation();
	LandscapeScale = CachedLandscape->GetActorScale3D();

	// Build O(1) component lookup map
	TArray<ULandscapeComponent*> Comps;
	CachedLandscape->GetComponents<ULandscapeComponent>(Comps);

	ComponentMap.Empty();
	if (Comps.Num() > 0)
	{
		CachedComponentSizeQuads = Comps[0]->ComponentSizeQuads;

		for (ULandscapeComponent* LC : Comps)
		{
			if (!LC || !IsValid(LC)) continue;

			FIntPoint Base = LC->GetSectionBase();
			// Grid key = which component cell this belongs to
			FIntPoint CompKey(Base.X / CachedComponentSizeQuads, Base.Y / CachedComponentSizeQuads);
			ComponentMap.Add(CompKey, LC);
		}
	}

	bLandscapeTransformCached = true;

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Cached landscape transform. Origin=(%.0f,%.0f,%.0f) Scale=(%.0f,%.0f,%.0f) Components=%d QuadsPerComp=%d"),
		LandscapeOrigin.X, LandscapeOrigin.Y, LandscapeOrigin.Z,
		LandscapeScale.X, LandscapeScale.Y, LandscapeScale.Z,
		ComponentMap.Num(), CachedComponentSizeQuads);
}

ULandscapeComponent* UTerraForgeSubsystem::FindComponentAtWorldPos(const FVector& WorldPos)
{
	if (!bLandscapeTransformCached) CacheLandscapeTransform();
	if (!CachedLandscape || ComponentMap.Num() == 0) return nullptr;

	// Convert world pos to heightmap coordinate
	int32 HmapX = FMath::FloorToInt((WorldPos.X - LandscapeOrigin.X) / LandscapeScale.X);
	int32 HmapY = FMath::FloorToInt((WorldPos.Y - LandscapeOrigin.Y) / LandscapeScale.Y);

	// Which component cell does this heightmap coordinate fall in?
	int32 CompIdxX = HmapX / CachedComponentSizeQuads;
	int32 CompIdxY = HmapY / CachedComponentSizeQuads;

	FIntPoint CompKey(CompIdxX, CompIdxY);
	ULandscapeComponent** Found = ComponentMap.Find(CompKey);

	if (Found && *Found && IsValid(*Found))
	{
		return *Found;
	}

	// Fallback: try adjacent cells (rounding at boundaries)
	for (int32 dx = -1; dx <= 1; dx++)
	{
		for (int32 dy = -1; dy <= 1; dy++)
		{
			if (dx == 0 && dy == 0) continue;
			FIntPoint AdjKey(CompIdxX + dx, CompIdxY + dy);
			ULandscapeComponent** AdjFound = ComponentMap.Find(AdjKey);
			if (AdjFound && *AdjFound && IsValid(*AdjFound))
			{
				// Verify this component actually contains the position
				FIntPoint Base = (*AdjFound)->GetSectionBase();
				int32 Quads = (*AdjFound)->ComponentSizeQuads;
				if (HmapX >= Base.X && HmapX <= Base.X + Quads &&
					HmapY >= Base.Y && HmapY <= Base.Y + Quads)
				{
					return *AdjFound;
				}
			}
		}
	}

	return nullptr;
}

FIntPoint UTerraForgeSubsystem::WorldToHeightmapCoord(const FVector& WorldPos) const
{
	if (!bLandscapeTransformCached)
	{
		const_cast<UTerraForgeSubsystem*>(this)->CacheLandscapeTransform();
	}

	int32 HmapX = FMath::RoundToInt((WorldPos.X - LandscapeOrigin.X) / LandscapeScale.X);
	int32 HmapY = FMath::RoundToInt((WorldPos.Y - LandscapeOrigin.Y) / LandscapeScale.Y);
	return FIntPoint(HmapX, HmapY);
}

bool UTerraForgeSubsystem::ModifyLandscapeHeight(const FVector& WorldPos, float DeltaCm,
	float BrushRadiusPixels, float FalloffSigma)
{
	// Ensure landscape data is cached
	if (!bLandscapeTransformCached) CacheLandscapeTransform();
	if (!CachedLandscape || ComponentMap.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: ModifyLandscapeHeight — no landscape cached!"));
		return false;
	}

	// Convert world position to heightmap grid coordinates
	float HmapXf = (WorldPos.X - LandscapeOrigin.X) / LandscapeScale.X;
	float HmapYf = (WorldPos.Y - LandscapeOrigin.Y) / LandscapeScale.Y;
	int32 CenterX = FMath::RoundToInt(HmapXf);
	int32 CenterY = FMath::RoundToInt(HmapYf);
	int32 BrushExtent = FMath::CeilToInt(BrushRadiusPixels);

	// Convert cm delta to uint16 heightmap units
	// Each uint16 step = LandscapeScale.Z * TF_ZSCALE cm = Scale.Z / 128 cm
	// So delta in uint16 = DeltaCm / (Scale.Z / 128) = DeltaCm * 128 / Scale.Z
	float DeltaUnits = DeltaCm * 128.0f / LandscapeScale.Z;

	UE_LOG(LogTemp, Log, TEXT("TerraForge: ModifyLandscapeHeight — center(%d,%d) delta=%.1fcm (%.1f units) brush=%.1f sigma=%.1f"),
		CenterX, CenterY, DeltaCm, DeltaUnits, BrushRadiusPixels, FalloffSigma);

	// ──────────────────────────────────────────────────────────────
	// PHASE 1: Collect all pixel modifications, grouped by UNIQUE TEXTURE.
	// CRITICAL: Multiple landscape components share the same heightmap texture.
	// Locking per-component causes double-lock assertion crash.
	// We lock each unique texture exactly ONCE.
	// ──────────────────────────────────────────────────────────────

	struct FPixelMod
	{
		int32 TexPixelX;
		int32 TexPixelY;
		float Weight;
		FIntPoint GlobalCoord;
	};

	struct FTextureBatch
	{
		UTexture2D* Texture = nullptr;
		int32 TexW = 0;
		int32 TexH = 0;
		TArray<FPixelMod> Pixels;
		TArray<ULandscapeComponent*> AffectedComponents;
	};

	TMap<UTexture2D*, FTextureBatch> TextureBatches;

	for (int32 dy = -BrushExtent; dy <= BrushExtent; dy++)
	{
		for (int32 dx = -BrushExtent; dx <= BrushExtent; dx++)
		{
			float Dist = FMath::Sqrt((float)(dx * dx + dy * dy));
			if (Dist > BrushRadiusPixels) continue;

			// Gaussian weight: center=1.0, edges→0
			float Weight = FMath::Exp(-Dist * Dist / (2.0f * FalloffSigma * FalloffSigma));

			int32 PixelX = CenterX + dx;
			int32 PixelY = CenterY + dy;

			// Find which component owns this pixel
			FVector PixelWorldPos(
				LandscapeOrigin.X + PixelX * LandscapeScale.X,
				LandscapeOrigin.Y + PixelY * LandscapeScale.Y,
				0.0f);

			ULandscapeComponent* Comp = FindComponentAtWorldPos(PixelWorldPos);
			if (!Comp) continue;

			UTexture2D* HeightmapTex = Comp->GetHeightmap(false);
			if (!HeightmapTex) continue;

			FTexturePlatformData* PD = HeightmapTex->GetPlatformData();
			if (!PD || PD->Mips.Num() == 0) continue;

			// Convert to texture pixel coordinates via this component's ScaleBias
			FIntPoint SectionBase = Comp->GetSectionBase();
			int32 CompQuads = Comp->ComponentSizeQuads;
			FVector4 ScaleBias = Comp->HeightmapScaleBias;

			int32 LocalX = PixelX - SectionBase.X;
			int32 LocalY = PixelY - SectionBase.Y;
			if (LocalX < 0 || LocalX > CompQuads || LocalY < 0 || LocalY > CompQuads)
				continue;

			float LocalUV_X = (float)LocalX / (float)CompQuads;
			float LocalUV_Y = (float)LocalY / (float)CompQuads;
			float TexUV_X = LocalUV_X * ScaleBias.X + ScaleBias.Z;
			float TexUV_Y = LocalUV_Y * ScaleBias.Y + ScaleBias.W;

			int32 TexW = PD->Mips[0].SizeX;
			int32 TexH = PD->Mips[0].SizeY;

			FPixelMod Mod;
			Mod.TexPixelX = FMath::Clamp(FMath::RoundToInt(TexUV_X * (float)TexW), 0, TexW - 1);
			Mod.TexPixelY = FMath::Clamp(FMath::RoundToInt(TexUV_Y * (float)TexH), 0, TexH - 1);
			Mod.Weight = Weight;
			Mod.GlobalCoord = FIntPoint(PixelX, PixelY);

			FTextureBatch& Batch = TextureBatches.FindOrAdd(HeightmapTex);
			Batch.Texture = HeightmapTex;
			Batch.TexW = TexW;
			Batch.TexH = TexH;
			Batch.Pixels.Add(Mod);
			Batch.AffectedComponents.AddUnique(Comp);
		}
	}

	if (TextureBatches.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: ModifyLandscapeHeight — no components found for brush area!"));
		return false;
	}

	// ──────────────────────────────────────────────────────────────
	// PHASE 2: Modify pixels.
	// SESSION MODE: textures are pre-locked by BeginEditSession() — just
	//   modify pixels using cached pointers. No lock/unlock/UpdateResource.
	//   GPU update + collision happen later in EndEditSession().
	// SINGLE-CALL MODE: full lock → modify → unlock → UpdateResource cycle.
	//   Works for one-off calls (no subsequent Lock() to conflict with).
	// ──────────────────────────────────────────────────────────────

	bool bAnyModified = false;

	if (bEditSessionActive)
	{
		// ── SESSION MODE ──────────────────────────────────────────
		for (auto& BatchPair : TextureBatches)
		{
			FTextureBatch& Batch = BatchPair.Value;
			UTexture2D* Tex = Batch.Texture;

			FLockedTextureInfo* Info = SessionLockedTextures.Find(Tex);
			if (!Info || !Info->MipData)
			{
				UE_LOG(LogTemp, Warning, TEXT("TerraForge: Session mode — texture not pre-locked, skipping"));
				continue;
			}

			for (const FPixelMod& Mod : Batch.Pixels)
			{
				FColor& Pixel = Info->MipData[Mod.TexPixelY * Info->SizeX + Mod.TexPixelX];
				uint16 CurrentHeight = ((uint16)Pixel.R << 8) | (uint16)Pixel.G;

				int32 NewHeight = (int32)CurrentHeight + FMath::RoundToInt(DeltaUnits * Mod.Weight);
				NewHeight = FMath::Clamp(NewHeight, 0, 65535);

				Pixel.R = (uint8)((NewHeight >> 8) & 0xFF);
				Pixel.G = (uint8)(NewHeight & 0xFF);

				RecordTerrainDelta(Mod.GlobalCoord, (int16)(NewHeight - (int32)CurrentHeight));
				bAnyModified = true;
			}

			// Track components for collision update at EndEditSession()
			for (ULandscapeComponent* Comp : Batch.AffectedComponents)
			{
				SessionAffectedComponents.Add(Comp);
			}
		}
		// No unlock, no UpdateResource, no FlushRenderingCommands.
		// EndEditSession() handles all of that.
	}
	else
	{
		// ── SINGLE-CALL MODE ──────────────────────────────────────
		for (auto& BatchPair : TextureBatches)
		{
			FTextureBatch& Batch = BatchPair.Value;
			UTexture2D* Tex = Batch.Texture;

			FTexturePlatformData* PlatformData = Tex->GetPlatformData();
			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			int32 TexW = Batch.TexW;

			void* RawData = Mip.BulkData.Lock(LOCK_READ_WRITE);
			if (!RawData)
			{
				UE_LOG(LogTemp, Error, TEXT("TerraForge: Failed to lock heightmap texture!"));
				continue;
			}

			FColor* TexData = static_cast<FColor*>(RawData);

			for (const FPixelMod& Mod : Batch.Pixels)
			{
				FColor& Pixel = TexData[Mod.TexPixelY * TexW + Mod.TexPixelX];
				uint16 CurrentHeight = ((uint16)Pixel.R << 8) | (uint16)Pixel.G;

				int32 NewHeight = (int32)CurrentHeight + FMath::RoundToInt(DeltaUnits * Mod.Weight);
				NewHeight = FMath::Clamp(NewHeight, 0, 65535);

				Pixel.R = (uint8)((NewHeight >> 8) & 0xFF);
				Pixel.G = (uint8)(NewHeight & 0xFF);

				RecordTerrainDelta(Mod.GlobalCoord, (int16)(NewHeight - (int32)CurrentHeight));
				bAnyModified = true;
			}

			Mip.BulkData.Unlock();
			Tex->UpdateResource();

			UE_LOG(LogTemp, Log, TEXT("TerraForge: Modified %d pixels across %d components in texture '%s'"),
				Batch.Pixels.Num(), Batch.AffectedComponents.Num(), *Tex->GetName());
		}

		FlushRenderingCommands();

		for (auto& BatchPair : TextureBatches)
		{
			for (ULandscapeComponent* Comp : BatchPair.Value.AffectedComponents)
			{
				Comp->UpdateCollisionData(false);
				Comp->UpdateCachedBounds(false);
			}
		}
	}

	return bAnyModified;
}

bool UTerraForgeSubsystem::FlattenLandscapeHeight(const FVector& WorldPos, float TargetHeightCm,
	float BrushRadiusPixels)
{
	// Ensure landscape data is cached
	if (!bLandscapeTransformCached) CacheLandscapeTransform();
	if (!CachedLandscape || ComponentMap.Num() == 0) return false;

	// Convert world position to heightmap grid coordinates
	int32 CenterX = FMath::RoundToInt((WorldPos.X - LandscapeOrigin.X) / LandscapeScale.X);
	int32 CenterY = FMath::RoundToInt((WorldPos.Y - LandscapeOrigin.Y) / LandscapeScale.Y);
	int32 BrushExtent = FMath::CeilToInt(BrushRadiusPixels);

	// Convert target world Z to uint16 heightmap value
	uint16 TargetUint16 = (uint16)FMath::Clamp(
		(int32)(TF_HEIGHT_MID +
			FMath::RoundToInt((TargetHeightCm - LandscapeOrigin.Z) * 128.0f / LandscapeScale.Z)),
		0, 65535);

	UE_LOG(LogTemp, Log, TEXT("TerraForge: FlattenLandscapeHeight — target Z=%.1f cm → uint16=%d"),
		TargetHeightCm, TargetUint16);

	// ──────────────────────────────────────────────────────────────
	// PHASE 1: Collect pixels grouped by UNIQUE TEXTURE (same pattern as ModifyLandscapeHeight)
	// ──────────────────────────────────────────────────────────────

	struct FFlattenPixel
	{
		int32 TexPixelX;
		int32 TexPixelY;
		FIntPoint GlobalCoord;
	};

	struct FFlattenBatch
	{
		UTexture2D* Texture = nullptr;
		int32 TexW = 0;
		int32 TexH = 0;
		TArray<FFlattenPixel> Pixels;
		TArray<ULandscapeComponent*> AffectedComponents;
	};

	TMap<UTexture2D*, FFlattenBatch> TextureBatches;

	for (int32 dy = -BrushExtent; dy <= BrushExtent; dy++)
	{
		for (int32 dx = -BrushExtent; dx <= BrushExtent; dx++)
		{
			float Dist = FMath::Sqrt((float)(dx * dx + dy * dy));
			if (Dist > BrushRadiusPixels) continue;

			int32 PixelX = CenterX + dx;
			int32 PixelY = CenterY + dy;

			FVector PixelWorldPos(
				LandscapeOrigin.X + PixelX * LandscapeScale.X,
				LandscapeOrigin.Y + PixelY * LandscapeScale.Y,
				0.0f);

			ULandscapeComponent* Comp = FindComponentAtWorldPos(PixelWorldPos);
			if (!Comp) continue;

			UTexture2D* HeightmapTex = Comp->GetHeightmap(false);
			if (!HeightmapTex) continue;

			FTexturePlatformData* PD = HeightmapTex->GetPlatformData();
			if (!PD || PD->Mips.Num() == 0) continue;

			FIntPoint SectionBase = Comp->GetSectionBase();
			int32 CompQuads = Comp->ComponentSizeQuads;
			FVector4 ScaleBias = Comp->HeightmapScaleBias;

			int32 LocalX = PixelX - SectionBase.X;
			int32 LocalY = PixelY - SectionBase.Y;
			if (LocalX < 0 || LocalX > CompQuads || LocalY < 0 || LocalY > CompQuads)
				continue;

			float LocalUV_X = (float)LocalX / (float)CompQuads;
			float LocalUV_Y = (float)LocalY / (float)CompQuads;
			float TexUV_X = LocalUV_X * ScaleBias.X + ScaleBias.Z;
			float TexUV_Y = LocalUV_Y * ScaleBias.Y + ScaleBias.W;

			int32 TexW = PD->Mips[0].SizeX;
			int32 TexH = PD->Mips[0].SizeY;

			FFlattenPixel FP;
			FP.TexPixelX = FMath::Clamp(FMath::RoundToInt(TexUV_X * (float)TexW), 0, TexW - 1);
			FP.TexPixelY = FMath::Clamp(FMath::RoundToInt(TexUV_Y * (float)TexH), 0, TexH - 1);
			FP.GlobalCoord = FIntPoint(PixelX, PixelY);

			FFlattenBatch& Batch = TextureBatches.FindOrAdd(HeightmapTex);
			Batch.Texture = HeightmapTex;
			Batch.TexW = TexW;
			Batch.TexH = TexH;
			Batch.Pixels.Add(FP);
			Batch.AffectedComponents.AddUnique(Comp);
		}
	}

	if (TextureBatches.Num() == 0) return false;

	// ──────────────────────────────────────────────────────────────
	// PHASE 2: Flatten pixels — session or single-call mode (same pattern
	// as ModifyLandscapeHeight).
	// ──────────────────────────────────────────────────────────────

	bool bAnyModified = false;

	if (bEditSessionActive)
	{
		// ── SESSION MODE ──────────────────────────────────────────
		for (auto& BatchPair : TextureBatches)
		{
			FFlattenBatch& Batch = BatchPair.Value;
			UTexture2D* Tex = Batch.Texture;

			FLockedTextureInfo* Info = SessionLockedTextures.Find(Tex);
			if (!Info || !Info->MipData) continue;

			for (const FFlattenPixel& FP : Batch.Pixels)
			{
				FColor& Pixel = Info->MipData[FP.TexPixelY * Info->SizeX + FP.TexPixelX];
				uint16 CurrentHeight = ((uint16)Pixel.R << 8) | (uint16)Pixel.G;

				int32 NewHeight = CurrentHeight + (int32)(TargetUint16 - CurrentHeight) / 2;
				NewHeight = FMath::Clamp(NewHeight, 0, 65535);

				Pixel.R = (uint8)((NewHeight >> 8) & 0xFF);
				Pixel.G = (uint8)(NewHeight & 0xFF);

				RecordTerrainDelta(FP.GlobalCoord, (int16)(NewHeight - (int32)CurrentHeight));
				bAnyModified = true;
			}

			for (ULandscapeComponent* Comp : Batch.AffectedComponents)
			{
				SessionAffectedComponents.Add(Comp);
			}
		}
	}
	else
	{
		// ── SINGLE-CALL MODE ──────────────────────────────────────
		for (auto& BatchPair : TextureBatches)
		{
			FFlattenBatch& Batch = BatchPair.Value;
			UTexture2D* Tex = Batch.Texture;

			FTexturePlatformData* PlatformData = Tex->GetPlatformData();
			FTexture2DMipMap& Mip = PlatformData->Mips[0];

			void* RawData = Mip.BulkData.Lock(LOCK_READ_WRITE);
			if (!RawData) continue;

			FColor* TexData = static_cast<FColor*>(RawData);

			for (const FFlattenPixel& FP : Batch.Pixels)
			{
				FColor& Pixel = TexData[FP.TexPixelY * Batch.TexW + FP.TexPixelX];
				uint16 CurrentHeight = ((uint16)Pixel.R << 8) | (uint16)Pixel.G;

				int32 NewHeight = CurrentHeight + (int32)(TargetUint16 - CurrentHeight) / 2;
				NewHeight = FMath::Clamp(NewHeight, 0, 65535);

				Pixel.R = (uint8)((NewHeight >> 8) & 0xFF);
				Pixel.G = (uint8)(NewHeight & 0xFF);

				RecordTerrainDelta(FP.GlobalCoord, (int16)(NewHeight - (int32)CurrentHeight));
				bAnyModified = true;
			}

			Mip.BulkData.Unlock();
			Tex->UpdateResource();
		}

		FlushRenderingCommands();

		for (auto& BatchPair : TextureBatches)
		{
			for (ULandscapeComponent* Comp : BatchPair.Value.AffectedComponents)
			{
				Comp->UpdateCollisionData(false);
				Comp->UpdateCachedBounds(false);
			}
		}
	}

	return bAnyModified;
}

EGeoMaterial UTerraForgeSubsystem::GetGeologicalMaterialAtDepth(const FVector& WorldPos, float DepthMeters) const
{
	// Determine what geological layer exists at this depth below original surface
	const EGeoBiome Biome = GetBiomeAt(WorldPos);
	const FGeoBiomeProfile* Profile = GetBiomeProfile(Biome);

	if (Profile)
	{
		for (const FGeoLayerDef& Layer : Profile->Layers)
		{
			if (DepthMeters >= Layer.MinDepth && DepthMeters < Layer.MaxDepth)
			{
				return Layer.Material;
			}
		}
	}

	// Default geological layers if no biome profile
	if (DepthMeters < 0.5f)  return EGeoMaterial::Topsoil;
	if (DepthMeters < 2.0f)  return EGeoMaterial::Topsoil;
	if (DepthMeters < 4.0f)  return EGeoMaterial::Clay;
	if (DepthMeters < 8.0f)  return EGeoMaterial::Sandstone;
	if (DepthMeters < 15.0f) return EGeoMaterial::Limestone;
	return EGeoMaterial::Granite;
}

void UTerraForgeSubsystem::RecordTerrainDelta(const FIntPoint& HeightmapCoord, int16 HeightDelta)
{
	int32& CumulativeDelta = TerrainDeltas.FindOrAdd(HeightmapCoord);
	CumulativeDelta += (int32)HeightDelta;
}

// ============================================================================
// EDIT SESSION MANAGEMENT
// ============================================================================
//
// Session-based texture locking eliminates the BulkData double-lock crash.
//
// Problem: UTexture2D::UpdateResource() internally re-locks BulkData via
//   InitRHI → LockMip → BulkData.Lock(). Even with FlushRenderingCommands(),
//   the internal lock state isn't fully released before the next explicit Lock()
//   call, causing "Assertion failed: IsUnlocked()" in BulkData.cpp.
//
// Solution: Lock all heightmap textures ONCE at session start. All terrain
//   modifications during the session modify the pre-locked pixel data directly.
//   Unlock + UpdateResource happens ONCE at session end. No intermediate
//   Lock/Unlock cycles = no double-lock crash.
//
// Flow: Player presses G → BeginEditSession (lock textures)
//       Player digs/raises/flattens multiple times → pixels modified in-place
//       Player presses G → EndEditSession (unlock, GPU push, collision update)
// ============================================================================

bool UTerraForgeSubsystem::BeginEditSession(const FVector& Center, float Radius)
{
	if (bEditSessionActive)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Edit session already active."));
		return true;
	}

	// Ensure landscape data is cached
	if (!bLandscapeTransformCached) CacheLandscapeTransform();
	if (!CachedLandscape || ComponentMap.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: BeginEditSession — no landscape found!"));
		return false;
	}

	// Determine component grid range to scan
	const int32 PixelRadius = FMath::CeilToInt(Radius / LandscapeScale.X) + CachedComponentSizeQuads;
	const int32 CenterPX = FMath::RoundToInt((Center.X - LandscapeOrigin.X) / LandscapeScale.X);
	const int32 CenterPY = FMath::RoundToInt((Center.Y - LandscapeOrigin.Y) / LandscapeScale.Y);
	const int32 CompStep = FMath::Max(CachedComponentSizeQuads, 1);

	const int32 MinGX = (CenterPX - PixelRadius) / CompStep;
	const int32 MaxGX = (CenterPX + PixelRadius) / CompStep;
	const int32 MinGY = (CenterPY - PixelRadius) / CompStep;
	const int32 MaxGY = (CenterPY + PixelRadius) / CompStep;

	// Collect unique textures from all components in range
	TSet<UTexture2D*> UniqueTextures;

	for (int32 gy = MinGY; gy <= MaxGY; gy++)
	{
		for (int32 gx = MinGX; gx <= MaxGX; gx++)
		{
			FIntPoint GridKey(gx * CompStep, gy * CompStep);
			ULandscapeComponent** FoundComp = ComponentMap.Find(GridKey);
			if (!FoundComp || !(*FoundComp)) continue;

			ULandscapeComponent* Comp = *FoundComp;
			UTexture2D* Tex = Comp->GetHeightmap(false);
			if (!Tex) continue;

			SessionAffectedComponents.Add(Comp);
			UniqueTextures.Add(Tex);
		}
	}

	if (UniqueTextures.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: BeginEditSession — no textures found in radius %.0f at (%.0f, %.0f)"),
			Radius, Center.X, Center.Y);
		SessionAffectedComponents.Empty();
		return false;
	}

	// Lock each unique texture exactly ONCE
	for (UTexture2D* Tex : UniqueTextures)
	{
		FTexturePlatformData* PD = Tex->GetPlatformData();
		if (!PD || PD->Mips.Num() == 0) continue;

		FTexture2DMipMap& Mip = PD->Mips[0];
		void* RawData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (!RawData)
		{
			UE_LOG(LogTemp, Error, TEXT("TerraForge: Failed to lock texture '%s'"), *Tex->GetName());
			continue;
		}

		FLockedTextureInfo Info;
		Info.MipData = static_cast<FColor*>(RawData);
		Info.SizeX = Mip.SizeX;
		Info.SizeY = Mip.SizeY;
		SessionLockedTextures.Add(Tex, Info);
	}

	bEditSessionActive = true;

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Edit session STARTED — %d unique textures locked, %d components in range"),
		SessionLockedTextures.Num(), SessionAffectedComponents.Num());

	return true;
}

void UTerraForgeSubsystem::EndEditSession()
{
	if (!bEditSessionActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: EndEditSession called with no active session."));
		return;
	}

	// Unlock all textures and push updated data to GPU
	for (auto& Pair : SessionLockedTextures)
	{
		UTexture2D* Tex = Pair.Key;
		FTexturePlatformData* PD = Tex->GetPlatformData();
		if (PD && PD->Mips.Num() > 0)
		{
			PD->Mips[0].BulkData.Unlock();
			Tex->UpdateResource();
		}
	}

	// Wait for all GPU texture uploads to complete before touching collision
	FlushRenderingCommands();

	// Update collision and bounds for every component that was modified
	int32 CollisionUpdates = 0;
	for (ULandscapeComponent* Comp : SessionAffectedComponents)
	{
		if (Comp)
		{
			Comp->UpdateCollisionData(false);
			Comp->UpdateCachedBounds(false);
			CollisionUpdates++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Edit session ENDED — %d textures unlocked, %d collision updates"),
		SessionLockedTextures.Num(), CollisionUpdates);

	// Clean up session state
	SessionLockedTextures.Empty();
	SessionAffectedComponents.Empty();
	bEditSessionActive = false;
}

// ============================================================================
// UNDERGROUND TUNNEL ACTIONS (voxel-based — unchanged)
// ============================================================================

bool UTerraForgeSubsystem::DigTunnel(const FVector& WorldPos, const FVector& Direction, float Radius)
{
	// Carve a spherical volume at the dig position
	const float RadiusCm = Radius * 100.0f;
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;
	const int32 VoxelRadius = FMath::CeilToInt(Radius / TF_VOXEL_SIZE);

	TSet<FTerraChunkKey> AffectedChunks;

	// Iterate over a cube of voxels centered at the dig point
	for (int32 DX = -VoxelRadius; DX <= VoxelRadius; ++DX)
	{
		for (int32 DY = -VoxelRadius; DY <= VoxelRadius; ++DY)
		{
			for (int32 DZ = -VoxelRadius; DZ <= VoxelRadius; ++DZ)
			{
				const FVector Offset(DX * VoxelCm, DY * VoxelCm, DZ * VoxelCm);
				const float Dist = Offset.Size();

				if (Dist > RadiusCm) continue; // Outside sphere

				const FVector VoxelWorldPos = WorldPos + Offset;
				FTerraForgeChunk* Chunk = nullptr;
				FGeoVoxel* Voxel = GetVoxelAtWorldPos(VoxelWorldPos, &Chunk);

				if (Voxel && Chunk)
				{
					if (Voxel->IsSolid())
					{
						// Smooth falloff at edges
						const float EdgeDist = (RadiusCm - Dist) / VoxelCm;
						Voxel->Density = FMath::Max(Voxel->Density, EdgeDist);
						if (Voxel->Density > 0.0f)
						{
							Voxel->MakeAir();
						}
						Voxel->Flags |= FGeoVoxel::FLAG_MODIFIED;
					}

					AffectedChunks.Add(Chunk->GetKey());
				}
			}
		}
	}

	// Mark all affected chunks dirty
	for (const FTerraChunkKey& Key : AffectedChunks)
	{
		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (Chunk)
		{
			Chunk->MarkDirty();
			DirtyChunkQueue.AddUnique(Key);
		}
	}

	return AffectedChunks.Num() > 0;
}

int32 UTerraForgeSubsystem::MineOre(const FVector& WorldPos, int32 SkillLevel)
{
	FTerraForgeChunk* Chunk = nullptr;
	FGeoVoxel* Voxel = GetVoxelAtWorldPos(WorldPos, &Chunk);

	if (!Voxel || !Chunk) return 0;
	if (!Voxel->IsOre()) return 0;
	if (Voxel->RemainingUnits == 0) return 0;

	// Extraction amount based on skill
	const int32 BaseExtract = FMath::Max(1, SkillLevel / 10);
	const int32 Extracted = FMath::Min((int32)Voxel->RemainingUnits, BaseExtract);

	Voxel->RemainingUnits -= Extracted;
	Voxel->Flags |= FGeoVoxel::FLAG_MODIFIED;

	// If depleted, convert to rock
	if (Voxel->RemainingUnits == 0)
	{
		EGeoMaterial ReplaceMat = EGeoMaterial::Granite; // Default replacement
		Voxel->Density = -1.0f;
		Voxel->SetMaterial(ReplaceMat);
		Voxel->SetOreType(EGeoOreType::None);
		Chunk->MarkDirty();
		DirtyChunkQueue.AddUnique(Chunk->GetKey());
	}

	return Extracted;
}

// ============================================================================
// OBSERVE MODE
// ============================================================================

TArray<FObserveTile> UTerraForgeSubsystem::GetObserveGrid(const FVector& CenterPos) const
{
	TArray<FObserveTile> Grid;
	Grid.Reserve(TF_OBSERVE_GRID_SIZE * TF_OBSERVE_GRID_SIZE);

	const float TileSize = TF_VOXEL_SIZE * 100.0f * 2.0f; // 1m tiles (2 voxels wide)
	const int32 Half = TF_OBSERVE_GRID_SIZE / 2;
	const float CenterElevation = GetElevationAt(CenterPos);

	for (int32 Y = -Half; Y <= Half; ++Y)
	{
		for (int32 X = -Half; X <= Half; ++X)
		{
			FObserveTile Tile;
			Tile.WorldPosition = CenterPos + FVector(X * TileSize, Y * TileSize, 0.0f);
			Tile.Elevation = GetElevationAt(Tile.WorldPosition) / 100.0f; // Convert cm to meters
			Tile.SurfaceMaterial = GetSurfaceMaterialAt(Tile.WorldPosition);
			Tile.HeightDelta = Tile.Elevation - (CenterElevation / 100.0f);

			// Flat if height delta is within +/-0.1m
			Tile.bIsFlat = FMath::Abs(Tile.HeightDelta) < 0.1f;

			// Get quality from voxel if available
			FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(Tile.WorldPosition);
			const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
			if (Found)
			{
				FIntVector Local = (*Found)->WorldToLocal(Tile.WorldPosition);
				if (FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
				{
					const FGeoVoxel& V = (*Found)->GetVoxel(Local.X, Local.Y, Local.Z);
					Tile.Quality = V.GetQualityF();
					Tile.Moisture = V.GetMoistureF();
				}
			}

			Grid.Add(Tile);
		}
	}

	return Grid;
}

float UTerraForgeSubsystem::GetElevationAt(const FVector& WorldPos) const
{
	// First check if we have a chunk with data here
	FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);

	if (Found)
	{
		// Scan column for topmost solid voxel
		FIntVector Local = (*Found)->WorldToLocal(WorldPos);
		Local.X = FMath::Clamp(Local.X, 0, TF_CHUNK_SIZE - 1);
		Local.Y = FMath::Clamp(Local.Y, 0, TF_CHUNK_SIZE - 1);

		for (int32 Z = TF_CHUNK_SIZE - 1; Z >= 0; --Z)
		{
			if ((*Found)->GetVoxel(Local.X, Local.Y, Z).IsSolid())
			{
				return (*Found)->LocalToWorld(Local.X, Local.Y, Z).Z;
			}
		}
	}

	// Fall back to landscape height
	return SampleLandscapeHeight(WorldPos.X, WorldPos.Y);
}

EGeoMaterial UTerraForgeSubsystem::GetSurfaceMaterialAt(const FVector& WorldPos) const
{
	FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);

	if (Found)
	{
		FIntVector Local = (*Found)->WorldToLocal(WorldPos);
		Local.X = FMath::Clamp(Local.X, 0, TF_CHUNK_SIZE - 1);
		Local.Y = FMath::Clamp(Local.Y, 0, TF_CHUNK_SIZE - 1);

		for (int32 Z = TF_CHUNK_SIZE - 1; Z >= 0; --Z)
		{
			const FGeoVoxel& V = (*Found)->GetVoxel(Local.X, Local.Y, Z);
			if (V.IsSolid())
			{
				return V.GetMaterial();
			}
		}
	}

	// Default to topsoil if no chunk exists
	return EGeoMaterial::Topsoil;
}

// ============================================================================
// PROSPECTING
// ============================================================================

TArray<FProspectingResult> UTerraForgeSubsystem::Prospect(const FVector& WorldPos, float Radius, int32 SkillLevel) const
{
	TArray<FProspectingResult> Results;

	const float RadiusCm = Radius * 100.0f;
	const FTerraChunkKey CenterKey = FTerraChunkKey::FromWorldPos(WorldPos);
	const int32 ChunkRadius = FMath::CeilToInt(Radius / TF_CHUNK_WORLD_SIZE) + 1;

	// Scan nearby chunks for ore deposits
	for (int32 DX = -ChunkRadius; DX <= ChunkRadius; ++DX)
	{
		for (int32 DY = -ChunkRadius; DY <= ChunkRadius; ++DY)
		{
			for (int32 DZ = -ChunkRadius; DZ <= ChunkRadius; ++DZ)
			{
				FTerraChunkKey Key(CenterKey.X + DX, CenterKey.Y + DY, CenterKey.Z + DZ);
				const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
				if (!Found) continue;

				const FTerraForgeChunk& Chunk = *Found->Get();

				// Scan for ore voxels
				for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
				{
					for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
					{
						for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
						{
							const FGeoVoxel& V = Chunk.GetVoxel(X, Y, Z);
							if (!V.IsOre()) continue;
							if (V.RemainingUnits == 0) continue;

							const FVector OreWorldPos = Chunk.LocalToWorld(X, Y, Z);
							const float Dist = FVector::Dist(WorldPos, OreWorldPos);
							if (Dist > RadiusCm) continue;

							// Check skill requirement
							const EGeoOreType OreType = V.GetOreType();
							int32 MinSkillRequired = 0;
							for (const FGeoOreRule& Rule : OreRules)
							{
								if (Rule.OreType == OreType)
								{
									MinSkillRequired = Rule.MinProspectingSkill;
									break;
								}
							}

							if (SkillLevel < MinSkillRequired) continue;

							// Build result with detail level based on skill
							FProspectingResult PR;
							PR.bFound = true;
							PR.OreType = OreType;
							PR.Distance = Dist / 100.0f; // cm to meters

							// Detail level affects accuracy
							if (SkillLevel >= 90)
							{
								PR.DetailLevel = 3;
								PR.Direction = (OreWorldPos - WorldPos).GetSafeNormal();
								PR.Depth = (WorldPos.Z - OreWorldPos.Z) / 100.0f;
								PR.EstimatedQuality = V.GetQualityF();
								PR.EstimatedUnits = V.RemainingUnits;
							}
							else if (SkillLevel >= 60)
							{
								PR.DetailLevel = 2;
								PR.Direction = (OreWorldPos - WorldPos).GetSafeNormal();
								PR.Direction += FVector(FMath::RandRange(-0.2f, 0.2f),
								                        FMath::RandRange(-0.2f, 0.2f), 0.0f);
								PR.Direction.Normalize();
								PR.Depth = (WorldPos.Z - OreWorldPos.Z) / 100.0f + FMath::RandRange(-2.0f, 2.0f);
								PR.EstimatedQuality = V.GetQualityF() + FMath::RandRange(-10.0f, 10.0f);
								PR.EstimatedUnits = V.RemainingUnits + FMath::RandRange(-20, 20);
							}
							else if (SkillLevel >= 30)
							{
								PR.DetailLevel = 1;
								PR.Direction = (OreWorldPos - WorldPos).GetSafeNormal();
								PR.Direction += FVector(FMath::RandRange(-0.5f, 0.5f),
								                        FMath::RandRange(-0.5f, 0.5f), 0.0f);
								PR.Direction.Normalize();
								PR.Depth = (WorldPos.Z - OreWorldPos.Z) / 100.0f + FMath::RandRange(-5.0f, 5.0f);
							}
							else
							{
								PR.DetailLevel = 0;
								// Just "something metallic nearby"
							}

							Results.Add(PR);
						}
					}
				}
			}
		}
	}

	// Sort by distance
	Results.Sort([](const FProspectingResult& A, const FProspectingResult& B)
	{
		return A.Distance < B.Distance;
	});

	return Results;
}

// ============================================================================
// STRUCTURAL INTEGRITY
// ============================================================================

EStructuralState UTerraForgeSubsystem::GetStructuralState(const FVector& WorldPos) const
{
	FTerraChunkKey Key = FTerraChunkKey::FromWorldPos(WorldPos);
	const TUniquePtr<FTerraForgeChunk>* Found = ChunkMap.Find(Key);
	if (!Found) return EStructuralState::Stable;

	const FTerraForgeChunk& Chunk = *Found->Get();
	FIntVector Local = Chunk.WorldToLocal(WorldPos);
	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
		return EStructuralState::Stable;

	const float Stress = CalculateStress(Chunk, Local.X, Local.Y, Local.Z);

	if (Stress >= TF_COLLAPSE_THRESHOLD)  return EStructuralState::Collapsed;
	if (Stress >= 0.95f)                   return EStructuralState::Critical;
	if (Stress >= 0.6f)                    return EStructuralState::Warning;
	return EStructuralState::Stable;
}

bool UTerraForgeSubsystem::CanMineAt(const FVector& WorldPos) const
{
	const EStructuralState State = GetStructuralState(WorldPos);
	return State != EStructuralState::Collapsed;
}

bool UTerraForgeSubsystem::PlaceSupport(const FVector& WorldPos)
{
	FTerraForgeChunk* Chunk = nullptr;
	FGeoVoxel* Voxel = GetVoxelAtWorldPos(WorldPos, &Chunk);
	if (!Voxel || !Chunk) return false;

	// Support can only be placed in air adjacent to solid
	if (!Voxel->IsAir()) return false;

	// Mark surrounding voxels as supported
	FIntVector Local = Chunk->WorldToLocal(WorldPos);
	const int32 SupportRadius = 4; // 4 voxels = 2 meters support radius

	for (int32 DX = -SupportRadius; DX <= SupportRadius; ++DX)
	{
		for (int32 DY = -SupportRadius; DY <= SupportRadius; ++DY)
		{
			for (int32 DZ = -SupportRadius; DZ <= SupportRadius; ++DZ)
			{
				const int32 NX = Local.X + DX;
				const int32 NY = Local.Y + DY;
				const int32 NZ = Local.Z + DZ;

				if (FTerraForgeChunk::IsInBounds(NX, NY, NZ))
				{
					FGeoVoxel& NV = Chunk->GetVoxelMutable(NX, NY, NZ);
					NV.Flags |= FGeoVoxel::FLAG_SUPPORTED;
				}
			}
		}
	}

	Chunk->MarkDirty();
	DirtyChunkQueue.AddUnique(Chunk->GetKey());
	return true;
}

float UTerraForgeSubsystem::CalculateStress(const FTerraForgeChunk& Chunk, int32 X, int32 Y, int32 Z) const
{
	// Look upward: count consecutive air voxels above this point
	// Then check horizontal span of air in the ceiling
	const FGeoVoxel& V = Chunk.GetVoxel(X, Y, Z);
	if (V.IsSolid()) return 0.0f; // Solid material has no stress on itself

	// Check if there's a ceiling above
	bool bHasCeiling = false;
	int32 CeilingZ = Z;
	for (int32 CheckZ = Z + 1; CheckZ < TF_CHUNK_SIZE; ++CheckZ)
	{
		if (Chunk.GetVoxel(X, Y, CheckZ).IsSolid())
		{
			bHasCeiling = true;
			CeilingZ = CheckZ;
			break;
		}
	}

	if (!bHasCeiling) return 0.0f; // No ceiling = open sky = no collapse

	// Measure unsupported span (how wide the air gap is at ceiling level)
	int32 Span = 0;
	for (int32 Dir = -1; Dir <= 1; Dir += 2)
	{
		for (int32 D = 1; D <= 20; ++D) // Max scan 20 voxels
		{
			const int32 CX = X + D * Dir;
			if (!FTerraForgeChunk::IsInBounds(CX, Y, CeilingZ)) break;

			if (Chunk.GetVoxel(CX, Y, CeilingZ).IsSolid())
			{
				// Also check if there's a support below
				bool bHasWall = false;
				for (int32 WZ = CeilingZ - 1; WZ >= Z; --WZ)
				{
					if (Chunk.GetVoxel(CX, Y, WZ).IsSolid() ||
					    (Chunk.GetVoxel(CX, Y, WZ).Flags & FGeoVoxel::FLAG_SUPPORTED))
					{
						bHasWall = true;
						break;
					}
				}
				if (bHasWall) break;
			}
			Span++;
		}
	}

	// Get ceiling material strength
	const FGeoVoxel& CeilingVoxel = Chunk.GetVoxel(X, Y, CeilingZ);
	const EGeoMaterial CeilingMat = CeilingVoxel.GetMaterial();
	const FGeoMaterialProperties* Props = MaterialTable.Find(CeilingMat);
	const int32 MaxSpan = Props ? Props->MaxUnsupportedSpan : TF_MAX_UNSUPPORTED_SPAN_DEFAULT;

	// Check if this voxel is supported
	if (V.IsSupported())
	{
		return FMath::Max(0.0f, (float)Span / (float)(MaxSpan * 3) - 0.1f);
	}

	if (MaxSpan <= 0) return 1.0f;
	return FMath::Clamp((float)Span / (float)MaxSpan, 0.0f, 1.0f);
}

// ============================================================================
// LANDSCAPE INTEGRATION
// ============================================================================

ALandscapeProxy* UTerraForgeSubsystem::FindLandscape() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscapeProxy::StaticClass(), Actors);
	return Actors.Num() > 0 ? Cast<ALandscapeProxy>(Actors[0]) : nullptr;
}

float UTerraForgeSubsystem::SampleLandscapeHeight(float WorldX, float WorldY) const
{
	if (!CachedLandscape)
	{
		// Try to find it (const_cast because we're caching for perf)
		const_cast<UTerraForgeSubsystem*>(this)->CachedLandscape = FindLandscape();
	}

	if (CachedLandscape)
	{
		// Use line trace to get precise landscape height
		UWorld* World = GetWorld();
		if (World)
		{
			FHitResult Hit;
			FVector Start(WorldX, WorldY, 100000.0f);
			FVector End(WorldX, WorldY, -100000.0f);
			FCollisionQueryParams Params;
			Params.bTraceComplex = true;

			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
			{
				return Hit.Location.Z;
			}
		}
	}

	return 0.0f; // No landscape found
}

EGeoBiome UTerraForgeSubsystem::GetBiomeAt(const FVector& WorldPos) const
{
	// Simple biome determination based on elevation and position
	// TODO: Replace with proper biome map (texture or data-driven)
	const float Elevation = WorldPos.Z / 100.0f; // cm to meters

	if (Elevation > 80.0f)  return EGeoBiome::Mountains;
	if (Elevation > 50.0f)  return EGeoBiome::Hills;
	if (Elevation < 5.0f)   return EGeoBiome::Coast;
	if (Elevation < 15.0f)  return EGeoBiome::River;

	// Use position hash for some variety
	const int32 RegionX = FMath::FloorToInt(WorldPos.X / 10000.0f); // 100m regions
	const int32 RegionY = FMath::FloorToInt(WorldPos.Y / 10000.0f);
	const uint32 Hash = HashCombine(GetTypeHash(RegionX), GetTypeHash(RegionY));

	switch (Hash % 5)
	{
	case 0: return EGeoBiome::Plains;
	case 1: return EGeoBiome::Forest;
	case 2: return EGeoBiome::Swamp;
	case 3: return EGeoBiome::Desert;
	case 4: return EGeoBiome::Tundra;
	default: return EGeoBiome::Plains;
	}
}

const FGeoBiomeProfile* UTerraForgeSubsystem::GetBiomeProfile(EGeoBiome Biome) const
{
	for (const FGeoBiomeProfile& Profile : BiomeProfiles)
	{
		if (Profile.Biome == Biome) return &Profile;
	}
	return nullptr;
}

// ============================================================================
// INTERNAL OPERATIONS
// ============================================================================

void UTerraForgeSubsystem::InitializeChunkFromLandscape(FTerraForgeChunk& Chunk)
{
	const FVector ChunkOrigin = Chunk.GetWorldOrigin();
	const float VoxelCm = TF_VOXEL_SIZE * 100.0f;

	// Determine biome for this chunk
	const EGeoBiome Biome = GetBiomeAt(ChunkOrigin);
	const FGeoBiomeProfile* Profile = GetBiomeProfile(Biome);

	// Default layers if no profile
	TArray<FGeoLayerDef> DefaultLayers;
	if (!Profile)
	{
		FGeoLayerDef Layer;
		Layer.Material = EGeoMaterial::Topsoil;
		Layer.MinDepth = 0.0f;
		Layer.MaxDepth = 2.0f;
		DefaultLayers.Add(Layer);

		Layer.Material = EGeoMaterial::Granite;
		Layer.MinDepth = 2.0f;
		Layer.MaxDepth = 100.0f;
		DefaultLayers.Add(Layer);
	}

	const TArray<FGeoLayerDef>& Layers = Profile ? Profile->Layers : DefaultLayers;
	const float SoilQ = Profile ? Profile->BaseSoilQuality : 50.0f;

	// Sample landscape heights for each XY position
	float SurfaceHeights[TF_CHUNK_SIZE][TF_CHUNK_SIZE];
	for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
	{
		for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
		{
			const float WorldX = ChunkOrigin.X + (X + 0.5f) * VoxelCm;
			const float WorldY = ChunkOrigin.Y + (Y + 0.5f) * VoxelCm;
			const float HeightCm = SampleLandscapeHeight(WorldX, WorldY);

			// Convert to meters for the chunk system
			SurfaceHeights[X][Y] = HeightCm / 100.0f;
		}
	}

	Chunk.InitializeFromLandscape(SurfaceHeights, Layers, SoilQ);
}

void UTerraForgeSubsystem::PlaceOreVeinsInChunk(FTerraForgeChunk& Chunk)
{
	const FVector ChunkOrigin = Chunk.GetWorldOrigin();
	const EGeoBiome Biome = GetBiomeAt(ChunkOrigin);

	for (const FGeoOreRule& Rule : OreRules)
	{
		// Check if this ore can spawn in this biome
		if (Rule.PreferredBiomes.Num() > 0 && !Rule.PreferredBiomes.Contains(Biome))
		{
			continue;
		}

		// Probability of a vein in this chunk
		const float ChunkAreaKmSq = (TF_CHUNK_WORLD_SIZE * TF_CHUNK_WORLD_SIZE) / 1000000.0f;
		const float Probability = Rule.SpawnsPerKmSq * ChunkAreaKmSq;

		// Use chunk key as seed for deterministic placement
		const FTerraChunkKey& Key = Chunk.GetKey();
		const uint32 Seed = HashCombine(
			HashCombine(GetTypeHash(Key.X), GetTypeHash(Key.Y)),
			HashCombine(GetTypeHash(Key.Z), GetTypeHash((int32)Rule.OreType)));

		FRandomStream RNG(Seed);

		if (RNG.FRand() > Probability) continue;

		// Place the vein at a random position within the chunk
		const int32 CenterX = RNG.RandRange(2, TF_CHUNK_SIZE - 3);
		const int32 CenterY = RNG.RandRange(2, TF_CHUNK_SIZE - 3);

		// Z position based on depth rules
		const float SurfaceH = Chunk.OriginalSurfaceHeights[CenterX][CenterY];
		const float ChunkBaseZ = Key.Z * TF_CHUNK_WORLD_SIZE;
		const float MinVeinZ = SurfaceH - Rule.MaxDepth;
		const float MaxVeinZ = SurfaceH - Rule.MinDepth;

		// Convert to local Z
		const int32 MinZ = FMath::Clamp(
			FMath::FloorToInt((MinVeinZ - ChunkBaseZ) / TF_VOXEL_SIZE), 0, TF_CHUNK_SIZE - 1);
		const int32 MaxZ = FMath::Clamp(
			FMath::FloorToInt((MaxVeinZ - ChunkBaseZ) / TF_VOXEL_SIZE), 0, TF_CHUNK_SIZE - 1);

		if (MinZ >= MaxZ) continue;

		const int32 CenterZ = RNG.RandRange(MinZ, MaxZ);

		// Check host rock requirement
		if (Rule.HostRockTypes.Num() > 0)
		{
			const FGeoVoxel& HostVoxel = Chunk.GetVoxel(CenterX, CenterY, CenterZ);
			if (!Rule.HostRockTypes.Contains(HostVoxel.GetMaterial()))
			{
				continue; // Wrong rock type
			}
		}

		// Place the vein
		const int32 Radius = FMath::Max(1, FMath::FloorToInt(FMath::Pow((float)Rule.AverageVeinSize, 1.0f/3.0f)));
		const float Quality = RNG.FRandRange(Rule.QualityRange.X, Rule.QualityRange.Y);

		Chunk.PlaceOreVein(
			FIntVector(CenterX, CenterY, CenterZ),
			Rule.OreType,
			Radius,
			Quality,
			Rule.UnitsPerVoxel);

		UE_LOG(LogTemp, Verbose, TEXT("TerraForge: Placed %s vein at chunk %s local (%d,%d,%d) Q:%.0f R:%d"),
			*UEnum::GetValueAsString(Rule.OreType),
			*Chunk.GetKey().ToString(),
			CenterX, CenterY, CenterZ,
			Quality, Radius);
	}
}

void UTerraForgeSubsystem::UpdateStreaming(const FVector& PlayerPos)
{
	const FTerraChunkKey PlayerChunk = FTerraChunkKey::FromWorldPos(PlayerPos);
	const int32 LoadRadius = TF_RING_LOW;

	// Determine which chunks should be loaded
	TSet<FTerraChunkKey> DesiredChunks;
	for (int32 DX = -LoadRadius; DX <= LoadRadius; ++DX)
	{
		for (int32 DY = -LoadRadius; DY <= LoadRadius; ++DY)
		{
			for (int32 DZ = -2; DZ <= 2; ++DZ)
			{
				FTerraChunkKey Key(PlayerChunk.X + DX, PlayerChunk.Y + DY, PlayerChunk.Z + DZ);
				if (ChunkMap.Contains(Key))
				{
					DesiredChunks.Add(Key);
				}
			}
		}
	}

	// Unload chunks that are no longer desired
	TArray<FTerraChunkKey> ToUnload;
	for (const FTerraChunkKey& Key : LoadedChunkKeys)
	{
		if (!DesiredChunks.Contains(Key))
		{
			ToUnload.Add(Key);
		}
	}

	for (const FTerraChunkKey& Key : ToUnload)
	{
		UProceduralMeshComponent** MeshComp = ChunkMeshMap.Find(Key);
		if (MeshComp && *MeshComp)
		{
			ReleaseMeshComponent(*MeshComp);
			ChunkMeshMap.Remove(Key);
		}

		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (Chunk) Chunk->SetHasRenderedMesh(false);

		LoadedChunkKeys.Remove(Key);
	}

	// Load desired chunks that aren't loaded yet (limited per frame)
	int32 LoadedThisFrame = 0;
	for (const FTerraChunkKey& Key : DesiredChunks)
	{
		if (!LoadedChunkKeys.Contains(Key) && LoadedThisFrame < TF_CHUNKS_PER_FRAME)
		{
			FTerraForgeChunk* Chunk = GetChunk(Key);
			if (Chunk && Chunk->HasModifications())
			{
				LoadedChunkKeys.Add(Key);
				DirtyChunkQueue.AddUnique(Key);
				LoadedThisFrame++;
			}
		}
	}
}

void UTerraForgeSubsystem::ProcessDirtyChunks(int32 MaxPerFrame)
{
	// Underground chunks still use ProceduralMesh rendering
	int32 Processed = 0;
	while (DirtyChunkQueue.Num() > 0 && Processed < MaxPerFrame)
	{
		const FTerraChunkKey Key = DirtyChunkQueue[0];
		DirtyChunkQueue.RemoveAt(0);

		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (!Chunk || !Chunk->IsDirty()) continue;
		if (Chunk->IsMeshGenerating()) continue;

		// Only regenerate ProceduralMesh for underground chunks
		// Surface terrain changes are handled by heightmap modification (no mesh needed)
		RegenerateMesh(Key);
		Processed++;
	}
}

void UTerraForgeSubsystem::RegenerateMesh(const FTerraChunkKey& Key)
{
	FTerraForgeChunk* Chunk = GetChunk(Key);
	if (!Chunk) return;

	const EChunkLOD LOD = EChunkLOD::Full;

	// Create snapshot for thread-safe mesh generation
	Chunk->SetMeshGenerating(true);
	TSharedPtr<FTerraForgeChunk> Snapshot = Chunk->CreateSnapshot();

	// Generate mesh
	FTerraChunkMeshData MeshData;
	FTerraForgeDualContour::GenerateMesh(*Snapshot, LOD, MeshData);

	// Apply mesh to ProceduralMeshComponent (underground visualization)
	if (MeshData.bValid && MeshData.Vertices.Num() > 0)
	{
		UProceduralMeshComponent* MeshComp = nullptr;

		UProceduralMeshComponent** ExistingComp = ChunkMeshMap.Find(Key);
		if (ExistingComp && *ExistingComp)
		{
			MeshComp = *ExistingComp;
		}
		else
		{
			MeshComp = AcquireMeshComponent();
			if (MeshComp)
			{
				ChunkMeshMap.Add(Key, MeshComp);
			}
		}

		if (MeshComp)
		{
			// Convert tangent vectors to FProcMeshTangent
			TArray<FProcMeshTangent> ProcTangents;
			ProcTangents.SetNum(MeshData.Vertices.Num());
			for (int32 I = 0; I < ProcTangents.Num(); ++I)
			{
				const FVector& N = MeshData.Normals[I];
				FVector T = FVector::CrossProduct(N, FVector::UpVector);
				if (T.IsNearlyZero()) T = FVector::CrossProduct(N, FVector::RightVector);
				T.Normalize();
				ProcTangents[I] = FProcMeshTangent(T, false);
			}

			MeshComp->ClearAllMeshSections();
			MeshComp->CreateMeshSection(
				0,
				MeshData.Vertices,
				MeshData.Triangles,
				MeshData.Normals,
				MeshData.UV0,
				MeshData.VertexColors,
				ProcTangents,
				true);

			MeshComp->SetVisibility(true);
			MeshComp->SetHiddenInGame(false);
			MeshComp->SetCastShadow(true);

			UMaterial* DefaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
			if (DefaultMat)
			{
				MeshComp->SetMaterial(0, DefaultMat);
			}

			Chunk->SetHasRenderedMesh(true);

			// NOTE: No landscape hiding for surface. Underground chunks will use
			// landscape hole material when tunnel system is implemented.
		}
	}

	Chunk->SetMeshGenerating(false);
	Chunk->ClearDirty();
}

UProceduralMeshComponent* UTerraForgeSubsystem::AcquireMeshComponent()
{
	if (FreeMeshPool.Num() > 0)
	{
		UProceduralMeshComponent* Comp = FreeMeshPool.Pop();
		return Comp;
	}

	// Pool exhausted — create a new component
	if (MeshPoolActor)
	{
		UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(MeshPoolActor);
		Comp->RegisterComponent();
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->bUseComplexAsSimpleCollision = true;
		MeshPool.Add(Comp);
		return Comp;
	}

	UE_LOG(LogTemp, Warning, TEXT("TerraForge: Cannot acquire mesh component — no pool actor!"));
	return nullptr;
}

void UTerraForgeSubsystem::ReleaseMeshComponent(UProceduralMeshComponent* Comp)
{
	if (!Comp) return;

	Comp->ClearAllMeshSections();
	Comp->SetVisibility(false);
	FreeMeshPool.Add(Comp);
}

void UTerraForgeSubsystem::UpdateNeighborLinks(const FTerraChunkKey& Key)
{
	FTerraForgeChunk* Center = GetChunk(Key);
	if (!Center) return;

	// +X, -X, +Y, -Y, +Z, -Z
	const FIntVector Offsets[6] = {
		{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
	};
	const int32 OppositeIdx[6] = {1, 0, 3, 2, 5, 4};

	for (int32 I = 0; I < 6; ++I)
	{
		FTerraChunkKey NeighborKey(Key.X + Offsets[I].X, Key.Y + Offsets[I].Y, Key.Z + Offsets[I].Z);
		FTerraForgeChunk* Neighbor = GetChunk(NeighborKey);
		Center->Neighbors[I] = Neighbor;
		if (Neighbor)
		{
			Neighbor->Neighbors[OppositeIdx[I]] = Center;
		}
	}
}

void UTerraForgeSubsystem::UpdateRegeneration(float DeltaTime)
{
	// Only process a few chunks per frame to limit overhead
	static int32 RegenCursor = 0;

	TArray<FTerraChunkKey> AllKeys;
	ChunkMap.GenerateKeyArray(AllKeys);

	if (AllKeys.Num() == 0) return;

	const int32 MaxPerFrame = 5;
	for (int32 I = 0; I < MaxPerFrame && AllKeys.Num() > 0; ++I)
	{
		RegenCursor = RegenCursor % AllKeys.Num();
		const FTerraChunkKey& Key = AllKeys[RegenCursor];

		FTerraForgeChunk* Chunk = GetChunk(Key);
		if (Chunk && Chunk->HasModifications() && !Chunk->bIsClaimed)
		{
			Chunk->TimeSinceLastInteraction += DeltaTime;

			const float RegenTimeSeconds = TF_REGEN_DAYS_UNCLAIMED * 86400.0f;
			if (Chunk->TimeSinceLastInteraction >= RegenTimeSeconds)
			{
				if (Chunk->bHasOriginalData)
				{
					const EGeoBiome Biome = GetBiomeAt(Chunk->GetWorldOrigin());
					const FGeoBiomeProfile* Profile = GetBiomeProfile(Biome);
					TArray<FGeoLayerDef> Layers;
					float SoilQ = 50.0f;
					if (Profile)
					{
						Layers = Profile->Layers;
						SoilQ = Profile->BaseSoilQuality;
					}

					Chunk->InitializeFromLandscape(Chunk->OriginalSurfaceHeights, Layers, SoilQ);

					UProceduralMeshComponent** MeshComp = ChunkMeshMap.Find(Key);
					if (MeshComp && *MeshComp)
					{
						ReleaseMeshComponent(*MeshComp);
						ChunkMeshMap.Remove(Key);
					}

					LoadedChunkKeys.Remove(Key);
					ChunkMap.Remove(Key);

					UE_LOG(LogTemp, Log, TEXT("TerraForge: Chunk %s regenerated and removed."), *Key.ToString());
				}
			}
		}

		RegenCursor++;
	}
}

FGeoVoxel* UTerraForgeSubsystem::GetVoxelAtWorldPos(const FVector& WorldPos, FTerraForgeChunk** OutChunk)
{
	FTerraForgeChunk* Chunk = GetOrCreateChunk(WorldPos);
	if (!Chunk) return nullptr;

	if (OutChunk) *OutChunk = Chunk;

	FIntVector Local = Chunk->WorldToLocal(WorldPos);
	if (!FTerraForgeChunk::IsInBounds(Local.X, Local.Y, Local.Z))
		return nullptr;

	return &Chunk->GetVoxelMutable(Local.X, Local.Y, Local.Z);
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

void UTerraForgeSubsystem::SaveAllChunks(const FString& SaveSlot)
{
	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("TerraForge") / SaveSlot;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*SaveDir);

	int32 SavedCount = 0;
	for (auto& ChunkPair : ChunkMap)
	{
		const FTerraChunkKey& Key = ChunkPair.Key;
		const FTerraForgeChunk& Chunk = *ChunkPair.Value;

		if (!Chunk.HasModifications()) continue;

		const FString FileName = FString::Printf(TEXT("chunk_%d_%d_%d.tfb"), Key.X, Key.Y, Key.Z);
		const FString FilePath = SaveDir / FileName;

		TArray<uint8> Data;
		FMemoryWriter Writer(Data);

		// Header
		const int32 MagicNumber = 0x54464247; // "TFBG"
		const int32 Version = 1;
		Writer << const_cast<int32&>(MagicNumber);
		Writer << const_cast<int32&>(Version);

		// Chunk key
		Writer << const_cast<int32&>(Key.X);
		Writer << const_cast<int32&>(Key.Y);
		Writer << const_cast<int32&>(Key.Z);

		// Voxel data (only modified voxels — delta encoding)
		int32 ModifiedCount = 0;
		TArray<uint8> VoxelPayload;
		FMemoryWriter VoxelWriter(VoxelPayload);

		for (int32 X = 0; X < TF_CHUNK_SIZE; ++X)
		{
			for (int32 Y = 0; Y < TF_CHUNK_SIZE; ++Y)
			{
				for (int32 Z = 0; Z < TF_CHUNK_SIZE; ++Z)
				{
					const FGeoVoxel& V = Chunk.GetVoxel(X, Y, Z);
					if (V.IsModified())
					{
						ModifiedCount++;
						uint16 Pos = (uint16)(X * TF_CHUNK_SIZE * TF_CHUNK_SIZE + Y * TF_CHUNK_SIZE + Z);
						VoxelWriter << Pos;
						float D = V.Density;
						VoxelWriter << D;
						uint8 Mat = V.Material;
						VoxelWriter << Mat;
						uint8 Ore = V.OreType;
						VoxelWriter << Ore;
						uint8 Q = V.Quality;
						VoxelWriter << Q;
						uint8 Moist = V.Moisture;
						VoxelWriter << Moist;
						uint8 Stress = V.StressLoad;
						VoxelWriter << Stress;
						uint8 Fl = V.Flags;
						VoxelWriter << Fl;
						uint16 Units = V.RemainingUnits;
						VoxelWriter << Units;
					}
				}
			}
		}

		Writer << ModifiedCount;
		Writer.Serialize(VoxelPayload.GetData(), VoxelPayload.Num());

		FFileHelper::SaveArrayToFile(Data, *FilePath);
		SavedCount++;
	}

	// Also save heightmap deltas
	if (TerrainDeltas.Num() > 0)
	{
		const FString DeltaFile = SaveDir / TEXT("terrain_deltas.tfd");
		TArray<uint8> DeltaData;
		FMemoryWriter DeltaWriter(DeltaData);

		int32 DeltaMagic = 0x54464844; // "TFHD" — TerraForge Height Deltas
		int32 DeltaVersion = 1;
		int32 DeltaCount = TerrainDeltas.Num();
		DeltaWriter << DeltaMagic;
		DeltaWriter << DeltaVersion;
		DeltaWriter << DeltaCount;

		for (auto& DeltaPair : TerrainDeltas)
		{
			int32 PX = DeltaPair.Key.X;
			int32 PY = DeltaPair.Key.Y;
			int32 DV = DeltaPair.Value;
			DeltaWriter << PX;
			DeltaWriter << PY;
			DeltaWriter << DV;
		}

		FFileHelper::SaveArrayToFile(DeltaData, *DeltaFile);
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Saved %d chunks + %d height deltas to '%s'"),
		SavedCount, TerrainDeltas.Num(), *SaveSlot);
}

void UTerraForgeSubsystem::LoadChunks(const FString& SaveSlot)
{
	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("TerraForge") / SaveSlot;

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SaveDir / TEXT("*.tfb")), true, false);

	int32 LoadedCount = 0;
	for (const FString& FileName : Files)
	{
		const FString FilePath = SaveDir / FileName;

		TArray<uint8> Data;
		if (!FFileHelper::LoadFileToArray(Data, *FilePath)) continue;

		FMemoryReader Reader(Data);

		int32 Magic, Version;
		Reader << Magic;
		Reader << Version;

		if (Magic != 0x54464247 || Version != 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("TerraForge: Invalid save file '%s'"), *FileName);
			continue;
		}

		int32 X, Y, Z;
		Reader << X;
		Reader << Y;
		Reader << Z;

		FTerraChunkKey Key(X, Y, Z);

		TUniquePtr<FTerraForgeChunk> Chunk = MakeUnique<FTerraForgeChunk>(Key);
		InitializeChunkFromLandscape(*Chunk);

		int32 ModifiedCount;
		Reader << ModifiedCount;

		for (int32 I = 0; I < ModifiedCount; ++I)
		{
			uint16 Pos;
			Reader << Pos;

			const int32 VX = Pos / (TF_CHUNK_SIZE * TF_CHUNK_SIZE);
			const int32 VY = (Pos / TF_CHUNK_SIZE) % TF_CHUNK_SIZE;
			const int32 VZ = Pos % TF_CHUNK_SIZE;

			if (!FTerraForgeChunk::IsInBounds(VX, VY, VZ)) continue;

			FGeoVoxel& V = Chunk->GetVoxelMutable(VX, VY, VZ);
			float D; Reader << D; V.Density = D;
			uint8 Mat; Reader << Mat; V.Material = Mat;
			uint8 Ore; Reader << Ore; V.OreType = Ore;
			uint8 Q; Reader << Q; V.Quality = Q;
			uint8 Moist; Reader << Moist; V.Moisture = Moist;
			uint8 Stress; Reader << Stress; V.StressLoad = Stress;
			uint8 Fl; Reader << Fl; V.Flags = Fl;
			uint16 Units; Reader << Units; V.RemainingUnits = Units;
		}

		ChunkMap.Add(Key, MoveTemp(Chunk));
		LoadedCount++;
	}

	// Load heightmap deltas
	const FString DeltaFile = SaveDir / TEXT("terrain_deltas.tfd");
	TArray<uint8> DeltaData;
	if (FFileHelper::LoadFileToArray(DeltaData, *DeltaFile) && DeltaData.Num() > 12)
	{
		FMemoryReader DeltaReader(DeltaData);
		int32 DeltaMagic, DeltaVersion, DeltaCount;
		DeltaReader << DeltaMagic;
		DeltaReader << DeltaVersion;
		DeltaReader << DeltaCount;

		if (DeltaMagic == 0x54464844 && DeltaVersion == 1)
		{
			for (int32 I = 0; I < DeltaCount; ++I)
			{
				int32 PX, PY, DV;
				DeltaReader << PX;
				DeltaReader << PY;
				DeltaReader << DV;
				TerrainDeltas.Add(FIntPoint(PX, PY), DV);
			}

			// Re-apply height deltas to landscape
			if (TerrainDeltas.Num() > 0 && bLandscapeTransformCached)
			{
				UE_LOG(LogTemp, Log, TEXT("TerraForge: Loaded %d height deltas — re-applying to landscape..."), TerrainDeltas.Num());
				// TODO: batch re-apply deltas to landscape heightmap
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Loaded %d chunks from '%s'"), LoadedCount, *SaveSlot);
}

void UTerraForgeSubsystem::SetVoxelMaterial(const FVector& WorldPos, EGeoMaterial Material)
{
	const FIntVector VoxelCoord(
		FMath::FloorToInt(WorldPos.X / TF_VOXEL_SIZE_CM),
		FMath::FloorToInt(WorldPos.Y / TF_VOXEL_SIZE_CM),
		FMath::FloorToInt(WorldPos.Z / TF_VOXEL_SIZE_CM)
	);

	FTerraChunkKey Key;
	Key.X = FMath::FloorToInt((float)VoxelCoord.X / TF_CHUNK_SIZE);
	Key.Y = FMath::FloorToInt((float)VoxelCoord.Y / TF_CHUNK_SIZE);
	Key.Z = FMath::FloorToInt((float)VoxelCoord.Z / TF_CHUNK_SIZE);

	auto* ChunkPtr = ChunkMap.Find(Key);
	if (!ChunkPtr) return;

	FTerraForgeChunk* Chunk = ChunkPtr->Get();
	if (!Chunk) return;

	const int32 LX = ((VoxelCoord.X % TF_CHUNK_SIZE) + TF_CHUNK_SIZE) % TF_CHUNK_SIZE;
	const int32 LY = ((VoxelCoord.Y % TF_CHUNK_SIZE) + TF_CHUNK_SIZE) % TF_CHUNK_SIZE;
	const int32 LZ = ((VoxelCoord.Z % TF_CHUNK_SIZE) + TF_CHUNK_SIZE) % TF_CHUNK_SIZE;

	FGeoVoxel& V = Chunk->GetVoxelMutable(LX, LY, LZ);
	V.Material = (uint8)Material;
	V.Density = (Material == EGeoMaterial::Air) ? 1.0f : -1.0f;
	Chunk->MarkDirty();

	DirtyChunkQueue.AddUnique(Key);
}

void UTerraForgeSubsystem::RegenerateMeshesInRadius(const FVector& Center, float Radius)
{
	const float VoxelSize = TF_VOXEL_SIZE_CM;
	const int32 ChunkRadius = FMath::CeilToInt(Radius / (TF_CHUNK_SIZE * VoxelSize)) + 1;

	const FIntVector CenterChunk(
		FMath::FloorToInt(Center.X / (TF_CHUNK_SIZE * VoxelSize)),
		FMath::FloorToInt(Center.Y / (TF_CHUNK_SIZE * VoxelSize)),
		FMath::FloorToInt(Center.Z / (TF_CHUNK_SIZE * VoxelSize))
	);

	for (int32 X = -ChunkRadius; X <= ChunkRadius; ++X)
	{
		for (int32 Y = -ChunkRadius; Y <= ChunkRadius; ++Y)
		{
			for (int32 Z = -ChunkRadius; Z <= ChunkRadius; ++Z)
			{
				FTerraChunkKey Key;
				Key.X = CenterChunk.X + X;
				Key.Y = CenterChunk.Y + Y;
				Key.Z = CenterChunk.Z + Z;

				if (ChunkMap.Contains(Key))
				{
					DirtyChunkQueue.AddUnique(Key);
				}
			}
		}
	}
}

FTerraForgeChunk* UTerraForgeSubsystem::GetChunkAt(const FIntVector& ChunkCoord) const
{
	FTerraChunkKey Key;
	Key.X = ChunkCoord.X;
	Key.Y = ChunkCoord.Y;
	Key.Z = ChunkCoord.Z;

	const auto* Found = ChunkMap.Find(Key);
	if (Found)
	{
		return Found->Get();
	}
	return nullptr;
}
