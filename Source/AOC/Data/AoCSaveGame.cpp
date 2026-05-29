// AoCSaveGame.cpp
// Architect of Creation - Save Game Implementation

#include "AoCSaveGame.h"
#include "AoCFarmingTile.h"
#include "AoCResourceNode.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

UAoCSaveGame::UAoCSaveGame()
	: SaveSlotName(TEXT("Default"))
	, SaveVersion(CurrentSaveVersion)
	, SaveTimestamp(FDateTime::UtcNow())
{
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveWorld — serialize world state into this object
// ─────────────────────────────────────────────────────────────────────────────

void UAoCSaveGame::SaveWorld(UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("AoCSaveGame::SaveWorld — null World pointer."));
		return;
	}

	// Clear previous data
	DepletedVeins.Empty();
	PlacedSupports.Empty();
	FarmTiles.Empty();
	TerrainMods.Empty();

	// Update metadata
	SaveTimestamp = FDateTime::UtcNow();
	SaveVersion = CurrentSaveVersion;

	// Serialize each category
	SerializeFarmTiles(World);
	SerializeVeins(World);
	SerializeSupports(World);
	SerializeTerrainMods(World);

	UE_LOG(LogTemp, Log, TEXT("AoCSaveGame::SaveWorld — Saved %d farm tiles, %d veins, %d supports, %d terrain mods, %d chunks."),
		FarmTiles.Num(), DepletedVeins.Num(), PlacedSupports.Num(), TerrainMods.Num(), ModifiedChunks.Num());
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadWorld — apply saved state to actors in the world
// ─────────────────────────────────────────────────────────────────────────────

void UAoCSaveGame::LoadWorld(UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("AoCSaveGame::LoadWorld — null World pointer."));
		return;
	}

	DeserializeFarmTiles(World);
	DeserializeVeins(World);
	DeserializeSupports(World);
	DeserializeTerrainMods(World);

	UE_LOG(LogTemp, Log, TEXT("AoCSaveGame::LoadWorld — Applied %d farm tiles, %d veins, %d supports, %d terrain mods."),
		FarmTiles.Num(), DepletedVeins.Num(), PlacedSupports.Num(), TerrainMods.Num());
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveToSlot / LoadFromSlot — disk persistence
// ─────────────────────────────────────────────────────────────────────────────

bool UAoCSaveGame::SaveToSlot(const FString& SlotName)
{
	SaveSlotName = SlotName;
	SaveTimestamp = FDateTime::UtcNow();

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(this, SlotName, 0);

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("AoCSaveGame: Saved to slot '%s' (version %d, ~%lld bytes)."),
			*SlotName, SaveVersion, GetSaveSize());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AoCSaveGame: FAILED to save to slot '%s'."), *SlotName);
	}

	return bSuccess;
}

UAoCSaveGame* UAoCSaveGame::LoadFromSlot(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("AoCSaveGame: No save exists in slot '%s'."), *SlotName);
		return nullptr;
	}

	USaveGame* LoadedRaw = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
	UAoCSaveGame* LoadedSave = Cast<UAoCSaveGame>(LoadedRaw);

	if (!LoadedSave)
	{
		UE_LOG(LogTemp, Error, TEXT("AoCSaveGame: Failed to cast loaded save from slot '%s'."), *SlotName);
		return nullptr;
	}

	// Version migration hook
	if (LoadedSave->SaveVersion < CurrentSaveVersion)
	{
		UE_LOG(LogTemp, Warning, TEXT("AoCSaveGame: Loaded save version %d, current is %d. Migration may be needed."),
			LoadedSave->SaveVersion, CurrentSaveVersion);
		// Future: call MigrateSave(LoadedSave->SaveVersion) here
	}

	UE_LOG(LogTemp, Log, TEXT("AoCSaveGame: Loaded from slot '%s' (version %d, saved %s)."),
		*SlotName, LoadedSave->SaveVersion, *LoadedSave->SaveTimestamp.ToString());

	return LoadedSave;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetSaveSize — byte estimate for UI
// ─────────────────────────────────────────────────────────────────────────────

int64 UAoCSaveGame::GetSaveSize() const
{
	int64 TotalBytes = 0;

	// Modified chunks: key (12 bytes) + array overhead + density bytes
	for (const auto& Pair : ModifiedChunks)
	{
		TotalBytes += sizeof(FIntVector); // key
		TotalBytes += Pair.Value.DensityData.Num();   // density data
		TotalBytes += 16;                 // TArray overhead
	}

	// Veins: FVector(24) + FName(12) + int32(4) = ~40 bytes each
	TotalBytes += DepletedVeins.Num() * 40;

	// Supports: FVector(24) + FName(12) + float(4) = ~40 bytes each
	TotalBytes += PlacedSupports.Num() * 40;

	// Farm tiles: FVector(24) + uint8(1) + FName(12) + 3×float(12) = ~49 bytes each
	TotalBytes += FarmTiles.Num() * 52;

	// Terrain mods: FIntVector(12) + 9×float(36) + array overhead = ~64 bytes each
	TotalBytes += TerrainMods.Num() * 64;

	// Metadata
	TotalBytes += SaveSlotName.Len() * sizeof(TCHAR);
	TotalBytes += sizeof(int32) + sizeof(FDateTime);

	return TotalBytes;
}

// ─────────────────────────────────────────────────────────────────────────────
// Serialize helpers — gather world actors into save arrays
// ─────────────────────────────────────────────────────────────────────────────

void UAoCSaveGame::SerializeFarmTiles(UWorld* World)
{
	for (TActorIterator<AAoCFarmingTile> It(World); It; ++It)
	{
		AAoCFarmingTile* Tile = *It;
		if (!Tile)
		{
			continue;
		}

		FFarmTileSaveData Data;
		Data.Location       = Tile->GetActorLocation();
		Data.TileState      = static_cast<uint8>(Tile->TileState);
		Data.CropID         = Tile->PlantedCrop.CropID;
		Data.GrowthProgress = Tile->GrowthProgress;
		Data.SoilQuality    = static_cast<float>(Tile->SoilQuality); // ESoilQuality → float
		Data.WaterLevel     = Tile->WaterLevel;
		FarmTiles.Add(Data);
	}
}

void UAoCSaveGame::SerializeVeins(UWorld* World)
{
	for (TActorIterator<AAoCResourceNode> It(World); It; ++It)
	{
		AAoCResourceNode* Node = *It;
		if (!Node || Node->NodeType != EResourceNodeType::OreVein)
		{
			continue;
		}

		// Only save veins that have been partially or fully depleted
		if (Node->CurrentHealth < Node->MaxHealth)
		{
			FVeinSaveData Data;
			Data.Location     = Node->GetActorLocation();
			Data.Material     = Node->ResourceID;
			Data.RemainingOre = Node->CurrentHealth;
			DepletedVeins.Add(Data);
		}
	}
}

void UAoCSaveGame::SerializeSupports(UWorld* World)
{
	// Mine supports are generic actors tagged "MineSupport"
	TArray<AActor*> SupportActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("MineSupport")), SupportActors);

	for (AActor* Actor : SupportActors)
	{
		if (!Actor)
		{
			continue;
		}

		FSupportSaveData Data;
		Data.Location    = Actor->GetActorLocation();
		Data.SupportType = Actor->ActorHasTag(FName(TEXT("Column")))
			? FName(TEXT("Column"))
			: FName(TEXT("Beam"));
		// Health stored as a custom float property — in a full integration this would
		// come from a support component. Default to 100.
		Data.Health = 100.0f;
		PlacedSupports.Add(Data);
	}
}

void UAoCSaveGame::SerializeTerrainMods(UWorld* World)
{
	// Terrain modifications are tracked by the terrain manager subsystem.
	// During full integration this would query that subsystem.
	// For now the TerrainMods array is populated externally before SaveWorld().
	// No-op here — data is expected to be filled in by the terraforming system.
}

// ─────────────────────────────────────────────────────────────────────────────
// Deserialize helpers — apply save data back to world actors
// ─────────────────────────────────────────────────────────────────────────────

void UAoCSaveGame::DeserializeFarmTiles(UWorld* World)
{
	// Build a spatial lookup of existing tiles
	TMap<FIntVector, AAoCFarmingTile*> ExistingTiles;
	for (TActorIterator<AAoCFarmingTile> It(World); It; ++It)
	{
		AAoCFarmingTile* Tile = *It;
		if (Tile)
		{
			FVector Loc = Tile->GetActorLocation();
			FIntVector Key(
				FMath::RoundToInt32(Loc.X),
				FMath::RoundToInt32(Loc.Y),
				FMath::RoundToInt32(Loc.Z));
			ExistingTiles.Add(Key, Tile);
		}
	}

	for (const FFarmTileSaveData& Data : FarmTiles)
	{
		FIntVector Key(
			FMath::RoundToInt32(Data.Location.X),
			FMath::RoundToInt32(Data.Location.Y),
			FMath::RoundToInt32(Data.Location.Z));

		AAoCFarmingTile** FoundTile = ExistingTiles.Find(Key);
		AAoCFarmingTile* Tile = FoundTile ? *FoundTile : nullptr;

		if (!Tile)
		{
			// Spawn a new farming tile at this location
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Tile = World->SpawnActor<AAoCFarmingTile>(AAoCFarmingTile::StaticClass(), Data.Location, FRotator::ZeroRotator, SpawnParams);
		}

		if (Tile)
		{
			Tile->TileState      = static_cast<EFarmTileState>(Data.TileState);
			Tile->PlantedCrop.CropID = Data.CropID;
			Tile->GrowthProgress = Data.GrowthProgress;
			Tile->WaterLevel     = Data.WaterLevel;
			// SoilQuality is an enum in AoCFarmingTile — map float → enum bracket
			if (Data.SoilQuality >= 80.0f)
			{
				Tile->SoilQuality = ESoilQuality::Excellent;
			}
			else if (Data.SoilQuality >= 60.0f)
			{
				Tile->SoilQuality = ESoilQuality::Great;
			}
			else if (Data.SoilQuality >= 40.0f)
			{
				Tile->SoilQuality = ESoilQuality::Good;
			}
			else if (Data.SoilQuality >= 20.0f)
			{
				Tile->SoilQuality = ESoilQuality::Fair;
			}
			else
			{
				Tile->SoilQuality = ESoilQuality::Poor;
			}
		}
	}
}

void UAoCSaveGame::DeserializeVeins(UWorld* World)
{
	// Build a spatial lookup of existing resource nodes
	TMap<FIntVector, AAoCResourceNode*> ExistingNodes;
	for (TActorIterator<AAoCResourceNode> It(World); It; ++It)
	{
		AAoCResourceNode* Node = *It;
		if (Node && Node->NodeType == EResourceNodeType::OreVein)
		{
			FVector Loc = Node->GetActorLocation();
			FIntVector Key(
				FMath::RoundToInt32(Loc.X),
				FMath::RoundToInt32(Loc.Y),
				FMath::RoundToInt32(Loc.Z));
			ExistingNodes.Add(Key, Node);
		}
	}

	for (const FVeinSaveData& Data : DepletedVeins)
	{
		FIntVector Key(
			FMath::RoundToInt32(Data.Location.X),
			FMath::RoundToInt32(Data.Location.Y),
			FMath::RoundToInt32(Data.Location.Z));

		AAoCResourceNode** Found = ExistingNodes.Find(Key);
		if (Found && *Found)
		{
			AAoCResourceNode* Node = *Found;
			Node->CurrentHealth = Data.RemainingOre;
			Node->bIsDepleted = (Data.RemainingOre <= 0);

			if (Node->bIsDepleted)
			{
				Node->OnDepleted();
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("AoCSaveGame: Could not find vein at (%d, %d, %d) for material '%s'. Vein may have been regenerated."),
				Key.X, Key.Y, Key.Z, *Data.Material.ToString());
		}
	}
}

void UAoCSaveGame::DeserializeSupports(UWorld* World)
{
	// In a full integration, this would spawn the correct support actor class.
	// For now, log the intent — support actor classes will be defined later.
	for (const FSupportSaveData& Data : PlacedSupports)
	{
		UE_LOG(LogTemp, Log, TEXT("AoCSaveGame: Would spawn %s at %s with HP %.1f"),
			*Data.SupportType.ToString(),
			*Data.Location.ToString(),
			Data.Health);

		// Future: spawn AAoCMineSupport actor at Data.Location
		// with type = Data.SupportType and health = Data.Health
	}
}

void UAoCSaveGame::DeserializeTerrainMods(UWorld* World)
{
	// Terrain modifications are applied by the terrain manager subsystem.
	// Pass the saved data to it for reconstruction.
	for (const FTerrainModification& Mod : TerrainMods)
	{
		UE_LOG(LogTemp, Verbose, TEXT("AoCSaveGame: Applying terrain mod at tile (%d, %d, %d) — %d control points."),
			Mod.TileCoord.X, Mod.TileCoord.Y, Mod.TileCoord.Z, Mod.HeightDeltas.Num());

		// Future: call terrain subsystem to apply height deltas
		// TerrainSubsystem->ApplyHeightDeltas(Mod.TileCoord, Mod.HeightDeltas);
	}
}
