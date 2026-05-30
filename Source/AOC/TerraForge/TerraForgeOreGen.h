// TerraForgeOreGen.h
// TerraForge — Geological ore vein generation.
// Places all 22 metals at correct depths in appropriate host rock,
// with realistic vein shapes, quality distribution, and rarity.

#pragma once

#include "CoreMinimal.h"
#include "TerraForgeTypes.h"
#include "TerraForgeOreGen.generated.h"

class UTerraForgeSubsystem;

/**
 * Ore vein instance — a deposit embedded in the rock layer.
 * Generated procedurally based on geological rules.
 */
USTRUCT(BlueprintType)
struct FOreVeinInstance
{
	GENERATED_BODY()

	/** Center position of the vein (world space). */
	UPROPERTY(BlueprintReadOnly)
	FVector Center = FVector::ZeroVector;

	/** Ore type. */
	UPROPERTY(BlueprintReadOnly)
	EGeoOreType OreType = EGeoOreType::Copper;

	/** Quality (0-100). Higher quality near vein center. */
	UPROPERTY(BlueprintReadOnly)
	float Quality = 50.0f;

	/** Remaining ore units. */
	UPROPERTY(BlueprintReadOnly)
	int32 RemainingOre = 50;

	/** Maximum ore units (at generation). */
	UPROPERTY(BlueprintReadOnly)
	int32 MaxOre = 50;

	/** Vein radius (cm). */
	UPROPERTY(BlueprintReadOnly)
	float VeinRadius = 150.0f;

	/** Is this a mega node (visible on surface)? */
	UPROPERTY(BlueprintReadOnly)
	bool bIsMegaNode = false;

	/** Generation seed (for deterministic shape). */
	UPROPERTY()
	int32 Seed = 0;

	/** Has been discovered by a player via prospecting? */
	UPROPERTY()
	bool bDiscovered = false;

	/** Unique ID. */
	UPROPERTY()
	int32 VeinID = 0;

	bool IsDepleted() const { return RemainingOre <= 0; }
};

/**
 * Ore generation rule — defines how a metal type spawns in the world.
 */
USTRUCT(BlueprintType)
struct FOreGenRule
{
	GENERATED_BODY()

	/** Ore type. */
	UPROPERTY(EditAnywhere)
	EGeoOreType OreType = EGeoOreType::Copper;

	/** Display name. */
	UPROPERTY(EditAnywhere)
	FString Name;

	/** Minimum depth below surface (meters). */
	UPROPERTY(EditAnywhere)
	float MinDepth = 3.0f;

	/** Maximum depth below surface (meters). */
	UPROPERTY(EditAnywhere)
	float MaxDepth = 12.0f;

	/** Host rock types where this ore spawns. */
	UPROPERTY(EditAnywhere)
	TArray<EGeoMaterial> HostRocks;

	/** Rarity (0.0 = never, 1.0 = extremely common). */
	UPROPERTY(EditAnywhere)
	float Rarity = 0.5f;

	/** Quality range (min-max). */
	UPROPERTY(EditAnywhere)
	FVector2D QualityRange = FVector2D(20.0f, 80.0f);

	/** Small node ore yield range. */
	UPROPERTY(EditAnywhere)
	FVector2D SmallNodeYield = FVector2D(30, 70);

	/** Mega node ore yield range. */
	UPROPERTY(EditAnywhere)
	FVector2D MegaNodeYield = FVector2D(500, 1000);

	/** Vein size range (meters radius). */
	UPROPERTY(EditAnywhere)
	FVector2D VeinSizeRange = FVector2D(1.0f, 3.0f);

	/** Real-world smelting temperature (°C). */
	UPROPERTY(EditAnywhere)
	float SmeltingTemp = 1085.0f;

	/** Required furnace tier to smelt (1-5). */
	UPROPERTY(EditAnywhere)
	int32 RequiredFurnaceTier = 1;
};

/**
 * Ore generation engine.
 *
 * Uses Perlin noise + geological rules to procedurally place ore veins
 * throughout the world. Veins are only generated when chunks are loaded
 * (lazy generation). Deterministic — same seed = same veins.
 *
 * All 22 metals have defined depth ranges, host rock requirements,
 * and rarity distribution. Deeper = rarer + higher quality.
 */
UCLASS()
class AOC_API UTerraForgeOreGen : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize with world seed and subsystem reference. */
	void Initialize(UTerraForgeSubsystem* InSubsystem, int32 WorldSeed);

	/** Generate ore veins for a chunk at the given chunk coordinate. */
	TArray<FOreVeinInstance> GenerateVeinsForChunk(const FIntVector& ChunkCoord);

	/** Get all known veins in a radius (for prospecting). */
	TArray<FOreVeinInstance> GetVeinsInRadius(const FVector& Center, float Radius) const;

	/** Extract ore from a vein. Returns units extracted. */
	int32 ExtractOre(int32 VeinID, int32 SkillLevel);

	/** Get a vein by ID. */
	const FOreVeinInstance* GetVein(int32 VeinID) const;

	/** Mark a vein as discovered. */
	void DiscoverVein(int32 VeinID);

	/** Get the ore generation rules (for UI/debug). */
	const TArray<FOreGenRule>& GetOreRules() const { return OreRules; }

	// ── Mega Node System ────────────────────────────────────────────────────

	/** Spawn a new mega node somewhere in the world. Called on timer. */
	FOreVeinInstance SpawnMegaNode();

	/** Remove a depleted mega node. */
	void RemoveMegaNode(int32 VeinID);

	/** Get all active mega nodes. */
	TArray<FOreVeinInstance> GetActiveMegaNodes() const;

private:
	/** Reference to subsystem. */
	UPROPERTY()
	TObjectPtr<UTerraForgeSubsystem> Subsystem;

	/** World generation seed. */
	int32 WorldSeed = 12345;

	/** Next vein ID counter. */
	int32 NextVeinID = 1;

	/** All generation rules (initialized in Initialize). */
	TArray<FOreGenRule> OreRules;

	/** All generated veins. */
	UPROPERTY()
	TArray<FOreVeinInstance> AllVeins;

	/** Active mega nodes. */
	UPROPERTY()
	TArray<FOreVeinInstance> MegaNodes;

	/** Set of chunk coordinates that have already been generated. */
	TSet<FIntVector> GeneratedChunks;

	// ── Internal Methods ────────────────────────────────────────────────────

	/** Initialize all 22 ore generation rules with real metallurgy data. */
	void InitializeOreRules();

	/** 3D Perlin noise for ore placement. */
	float OreNoise3D(float X, float Y, float Z, int32 OctaveSeed) const;

	/** Determine the host rock at a depth (simplified geological model). */
	EGeoMaterial GetHostRockAtDepth(float DepthMeters, const FVector2D& WorldXY) const;

	/** Check if a host rock matches the ore's requirements. */
	bool IsValidHostRock(const FOreGenRule& Rule, EGeoMaterial Rock) const;

	/** Generate a vein shape (returns voxel offsets from center). */
	TArray<FIntVector> GenerateVeinShape(int32 Seed, float RadiusMeters) const;
};
