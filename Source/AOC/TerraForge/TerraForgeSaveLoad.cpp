// TerraForgeSaveLoad.cpp
// TerraForge — Persistence implementation.
// Atomic saves, RLE compression, delta encoding, nature reclaim.

#include "TerraForgeSaveLoad.h"
#include "TerraForgeSubsystem.h"
#include "TerraForgeChunk.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

// ============================================================================
// INITIALIZATION
// ============================================================================

void UTerraForgeSaveLoad::Initialize(const FString& InSaveDir, UTerraForgeSubsystem* InSubsystem)
{
	SaveDir = InSaveDir;
	Subsystem = InSubsystem;

	// Ensure save directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SaveDir))
	{
		PlatformFile.CreateDirectoryTree(*SaveDir);
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge SaveLoad: Initialized at %s"), *SaveDir);
}

// ============================================================================
// TICK
// ============================================================================

void UTerraForgeSaveLoad::Tick(float DeltaTime)
{
	if (AutoSaveInterval <= 0.0f) return;

	AutoSaveTimer += DeltaTime;
	if (AutoSaveTimer >= AutoSaveInterval)
	{
		AutoSaveTimer = 0.0f;

		if (DirtyChunks.Num() > 0)
		{
			SaveDirtyChunks();
			UE_LOG(LogTemp, Log, TEXT("TerraForge: Auto-saved %d dirty chunks."), DirtyChunks.Num());
		}
	}
}

// ============================================================================
// SAVE
// ============================================================================

bool UTerraForgeSaveLoad::SaveAll()
{
	if (!Subsystem) return false;

	// Build header
	FTerraForgeSaveHeader Header;
	Header.WorldSeed = 12345; // TODO: Get from subsystem
	Header.ChunkCount = SavedChunks.Num();
	Header.SaveTime = FDateTime::Now();

	// Serialize to buffer
	FBufferArchive Archive;

	// Write header
	Archive << Header.Magic;
	Archive << Header.Version;
	Archive << Header.WorldSeed;
	Archive << Header.ChunkCount;
	Archive << Header.VeinCount;
	Archive << Header.SupportCount;
	Archive << Header.SaveTime;

	// Write chunk index (coord → file mapping)
	for (const auto& Pair : SavedChunks)
	{
		FIntVector Coord = Pair.Key;
		Archive << Coord.X;
		Archive << Coord.Y;
		Archive << Coord.Z;

		FChunkSaveData Data = Pair.Value;
		Archive << Data.bOnClaimedLand;
		Archive << Data.LastModified;
		Archive << Data.UncompressedSize;
		Archive << Data.CompressedData;
	}

	// Atomic write
	const FString MainPath = GetMainSaveFilePath();
	const bool bSuccess = AtomicWriteFile(MainPath, Archive);

	if (bSuccess)
	{
		DirtyChunks.Empty();
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Saved %d chunks to %s"), SavedChunks.Num(), *MainPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: FAILED to save to %s"), *MainPath);
	}

	return bSuccess;
}

bool UTerraForgeSaveLoad::SaveDirtyChunks()
{
	if (!Subsystem || DirtyChunks.Num() == 0) return true;

	int32 SavedCount = 0;

	for (const FIntVector& ChunkCoord : DirtyChunks)
	{
		// Get chunk from subsystem
		FTerraForgeChunk* Chunk = Subsystem->GetChunkAt(ChunkCoord);
		if (Chunk)
		{
			SaveChunk(ChunkCoord, Chunk);
			SavedCount++;
		}
	}

	DirtyChunks.Empty();

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Incrementally saved %d chunks."), SavedCount);
	return true;
}

bool UTerraForgeSaveLoad::SaveChunk(const FIntVector& ChunkCoord, const FTerraForgeChunk* Chunk)
{
	if (!Chunk) return false;

	// Delta encode
	TArray<uint8> DeltaData = DeltaEncode(Chunk);

	// RLE compress
	TArray<uint8> CompressedData = CompressRLE(DeltaData);

	// Create save data
	FChunkSaveData Data;
	Data.ChunkCoord = ChunkCoord;
	Data.CompressedData = MoveTemp(CompressedData);
	Data.LastModified = FDateTime::Now();
	Data.UncompressedSize = DeltaData.Num();
	Data.bOnClaimedLand = false; // TODO: Check claim system

	// Store in map
	SavedChunks.Add(ChunkCoord, MoveTemp(Data));

	// Write individual chunk file
	const FString ChunkPath = GetChunkFilePath(ChunkCoord);
	return AtomicWriteFile(ChunkPath, SavedChunks[ChunkCoord].CompressedData);
}

// ============================================================================
// LOAD
// ============================================================================

bool UTerraForgeSaveLoad::LoadAll()
{
	const FString MainPath = GetMainSaveFilePath();

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *MainPath))
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: No save file found at %s — fresh world."), *MainPath);
		return true; // Not an error — just no save
	}

	if (!ValidateSaveFile(FileData))
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: Save file CORRUPT at %s — starting fresh."), *MainPath);
		return false;
	}

	FMemoryReader Archive(FileData, true);

	// Read header
	FTerraForgeSaveHeader Header;
	Archive << Header.Magic;
	Archive << Header.Version;
	Archive << Header.WorldSeed;
	Archive << Header.ChunkCount;
	Archive << Header.VeinCount;
	Archive << Header.SupportCount;
	Archive << Header.SaveTime;

	// Read chunk index
	SavedChunks.Empty();
	for (int32 I = 0; I < Header.ChunkCount; ++I)
	{
		FIntVector Coord;
		Archive << Coord.X;
		Archive << Coord.Y;
		Archive << Coord.Z;

		FChunkSaveData Data;
		Data.ChunkCoord = Coord;
		Archive << Data.bOnClaimedLand;
		Archive << Data.LastModified;
		Archive << Data.UncompressedSize;
		Archive << Data.CompressedData;

		SavedChunks.Add(Coord, MoveTemp(Data));
	}

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Loaded %d chunks from save."), SavedChunks.Num());
	return true;
}

bool UTerraForgeSaveLoad::LoadChunk(const FIntVector& ChunkCoord, FTerraForgeChunk* OutChunk)
{
	if (!OutChunk) return false;

	const FChunkSaveData* Data = SavedChunks.Find(ChunkCoord);
	if (!Data)
	{
		return false; // No save data for this chunk
	}

	// Decompress
	TArray<uint8> DecompressedData = DecompressRLE(Data->CompressedData, Data->UncompressedSize);

	// Delta decode
	DeltaDecode(DecompressedData, OutChunk);

	return true;
}

void UTerraForgeSaveLoad::DeleteChunkData(const FIntVector& ChunkCoord)
{
	SavedChunks.Remove(ChunkCoord);
	DirtyChunks.Remove(ChunkCoord);

	// Delete chunk file
	const FString ChunkPath = GetChunkFilePath(ChunkCoord);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteFile(*ChunkPath);

	UE_LOG(LogTemp, Log, TEXT("TerraForge: Deleted chunk data at (%d, %d, %d)"),
		ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);
}

// ============================================================================
// NATURE RECLAIM
// ============================================================================

void UTerraForgeSaveLoad::ProcessNatureReclaim()
{
	const FDateTime Now = FDateTime::Now();
	TArray<FIntVector> ToDelete;

	for (const auto& Pair : SavedChunks)
	{
		// Skip claimed land
		if (Pair.Value.bOnClaimedLand) continue;

		// Check age
		const FTimespan Age = Now - Pair.Value.LastModified;
		if (Age.GetTotalSeconds() > NatureReclaimTime)
		{
			ToDelete.Add(Pair.Key);
		}
	}

	for (const FIntVector& Coord : ToDelete)
	{
		DeleteChunkData(Coord);
	}

	if (ToDelete.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Nature reclaimed %d chunks."), ToDelete.Num());
	}
}

// ============================================================================
// COMPRESSION
// ============================================================================

TArray<uint8> UTerraForgeSaveLoad::CompressRLE(const TArray<uint8>& RawData)
{
	TArray<uint8> Compressed;
	if (RawData.Num() == 0) return Compressed;

	Compressed.Reserve(RawData.Num()); // Worst case: same size

	int32 I = 0;
	while (I < RawData.Num())
	{
		const uint8 Current = RawData[I];
		int32 RunLength = 1;

		// Count consecutive identical bytes (max 255 for uint8 length)
		while (I + RunLength < RawData.Num() &&
			RawData[I + RunLength] == Current &&
			RunLength < 255)
		{
			RunLength++;
		}

		if (RunLength >= 3)
		{
			// RLE: marker (0xFF) + length + value
			// If the value itself is 0xFF, we still use this encoding
			Compressed.Add(0xFF);
			Compressed.Add((uint8)RunLength);
			Compressed.Add(Current);
		}
		else
		{
			// Literal bytes
			for (int32 J = 0; J < RunLength; ++J)
			{
				if (Current == 0xFF)
				{
					// Escape literal 0xFF
					Compressed.Add(0xFF);
					Compressed.Add(1);
					Compressed.Add(0xFF);
				}
				else
				{
					Compressed.Add(Current);
				}
			}
		}

		I += RunLength;
	}

	return Compressed;
}

TArray<uint8> UTerraForgeSaveLoad::DecompressRLE(
	const TArray<uint8>& CompressedData, int32 UncompressedSize)
{
	TArray<uint8> Decompressed;
	Decompressed.Reserve(UncompressedSize);

	int32 I = 0;
	while (I < CompressedData.Num() && Decompressed.Num() < UncompressedSize)
	{
		if (CompressedData[I] == 0xFF && I + 2 < CompressedData.Num())
		{
			// RLE run
			const uint8 RunLength = CompressedData[I + 1];
			const uint8 Value = CompressedData[I + 2];

			for (int32 J = 0; J < RunLength && Decompressed.Num() < UncompressedSize; ++J)
			{
				Decompressed.Add(Value);
			}
			I += 3;
		}
		else
		{
			// Literal byte
			Decompressed.Add(CompressedData[I]);
			I++;
		}
	}

	// Validate size
	if (Decompressed.Num() != UncompressedSize)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("TerraForge: Decompressed size mismatch! Expected %d, got %d"),
			UncompressedSize, Decompressed.Num());
	}

	return Decompressed;
}

TArray<uint8> UTerraForgeSaveLoad::DeltaEncode(const FTerraForgeChunk* Chunk) const
{
	TArray<uint8> DeltaData;
	if (!Chunk) return DeltaData;

	// Get chunk's raw voxel data
	const int32 VoxelCount = 16 * 16 * 16; // 4096 voxels per chunk
	DeltaData.SetNum(VoxelCount * 2); // Material byte + density byte per voxel

	for (int32 I = 0; I < VoxelCount; ++I)
	{
		// Each voxel stores: material enum (uint8) + density (uint8)
		// Delta against "default" = what the geological model would generate
		// For now, store absolute values (delta requires regenerating defaults)
		const int32 X = I % 16;
		const int32 Y = (I / 16) % 16;
		const int32 Z = I / 256;

		const FGeoVoxel& Voxel = Chunk->GetVoxel(X, Y, Z);
		DeltaData[I * 2] = Voxel.Material;
		DeltaData[I * 2 + 1] = (uint8)FMath::Clamp(Voxel.Density * 127.0f + 128.0f, 0.0f, 255.0f);
	}

	return DeltaData;
}

void UTerraForgeSaveLoad::DeltaDecode(
	const TArray<uint8>& DeltaData, FTerraForgeChunk* OutChunk) const
{
	if (!OutChunk || DeltaData.Num() == 0) return;

	const int32 VoxelCount = 16 * 16 * 16;

	for (int32 I = 0; I < VoxelCount && I * 2 + 1 < DeltaData.Num(); ++I)
	{
		const int32 X = I % 16;
		const int32 Y = (I / 16) % 16;
		const int32 Z = I / 256;

		FGeoVoxel Voxel;
		Voxel.Material = DeltaData[I * 2];
		Voxel.Density = ((float)DeltaData[I * 2 + 1] - 128.0f) / 127.0f;

		OutChunk->SetVoxel(X, Y, Z, Voxel);
	}

	OutChunk->MarkDirty();
}

// ============================================================================
// FILE I/O
// ============================================================================

FString UTerraForgeSaveLoad::GetChunkFilePath(const FIntVector& ChunkCoord) const
{
	return FPaths::Combine(SaveDir, FString::Printf(
		TEXT("chunk_%d_%d_%d.tfc"), ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z));
}

FString UTerraForgeSaveLoad::GetMainSaveFilePath() const
{
	return FPaths::Combine(SaveDir, TEXT("terraforge.sav"));
}

bool UTerraForgeSaveLoad::AtomicWriteFile(const FString& FilePath, const TArray<uint8>& Data)
{
	const FString TempPath = FilePath + TEXT(".tmp");

	// Write to temp file first
	if (!FFileHelper::SaveArrayToFile(Data, *TempPath))
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: Failed to write temp file %s"), *TempPath);
		return false;
	}

	// Rename temp → final (atomic on most filesystems)
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Delete old file if exists
	if (PlatformFile.FileExists(*FilePath))
	{
		PlatformFile.DeleteFile(*FilePath);
	}

	// Rename temp → final
	if (!PlatformFile.MoveFile(*FilePath, *TempPath))
	{
		UE_LOG(LogTemp, Error, TEXT("TerraForge: Failed to rename %s → %s"), *TempPath, *FilePath);
		// Clean up temp
		PlatformFile.DeleteFile(*TempPath);
		return false;
	}

	return true;
}

bool UTerraForgeSaveLoad::ValidateSaveFile(const TArray<uint8>& Data) const
{
	if (Data.Num() < 4) return false;

	// Check magic number
	const uint32 Magic = *reinterpret_cast<const uint32*>(Data.GetData());
	return Magic == 0x54464F52; // "TFOR"
}

int64 UTerraForgeSaveLoad::GetSaveFileSize() const
{
	const FString MainPath = GetMainSaveFilePath();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.FileSize(*MainPath);
}
