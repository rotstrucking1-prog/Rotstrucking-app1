// TerraForgeStructural.h
// TerraForge — Structural integrity simulation.
// Real stress propagation through geological material network.
// Supports + collapse + landslide cascade.

#pragma once

#include "CoreMinimal.h"
#include "TerraForgeTypes.h"
#include "TerraForgeStructural.generated.h"

class UTerraForgeSubsystem;

/**
 * Mine support actor.
 * Placed by players to prevent collapse in underground tunnels.
 * Has a radius of influence and degrades over time.
 */
USTRUCT(BlueprintType)
struct FMineSupportData
{
	GENERATED_BODY()

	/** World position of the support. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Position = FVector::ZeroVector;

	/** Support radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SupportRadius = 400.0f; // 4m radius

	/** Current structural HP (0-100). Degrades over time under load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StructuralHP = 100.0f;

	/** Load on this support (calculated from surrounding voxels). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentLoad = 0.0f;

	/** Maximum load before failure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxLoad = 500.0f;

	/** Time placed (for degradation). */
	UPROPERTY()
	float TimePlaced = 0.0f;

	/** Support tier (affects radius + max load). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Tier = 1;

	/** Unique ID for save/load. */
	UPROPERTY()
	int32 SupportID = 0;

	bool IsValid() const { return StructuralHP > 0.0f; }
	bool IsOverloaded() const { return CurrentLoad > MaxLoad; }
	float GetLoadRatio() const { return MaxLoad > 0.0f ? CurrentLoad / MaxLoad : 1.0f; }
};

/**
 * Collapse event data.
 */
USTRUCT(BlueprintType)
struct FCollapseEvent
{
	GENERATED_BODY()

	/** Center of collapse. */
	UPROPERTY(BlueprintReadOnly)
	FVector Center = FVector::ZeroVector;

	/** Radius of collapse. */
	UPROPERTY(BlueprintReadOnly)
	float Radius = 0.0f;

	/** Volume of material that collapsed. */
	UPROPERTY(BlueprintReadOnly)
	int32 CollapsedVoxels = 0;

	/** Material that fell. */
	UPROPERTY(BlueprintReadOnly)
	EGeoMaterial Material = EGeoMaterial::Granite;
};

/**
 * Landslide event data.
 */
USTRUCT(BlueprintType)
struct FLandslideEvent
{
	GENERATED_BODY()

	/** Start position (where dirt originated). */
	UPROPERTY(BlueprintReadOnly)
	FVector StartPosition = FVector::ZeroVector;

	/** End position (where dirt settled). */
	UPROPERTY(BlueprintReadOnly)
	FVector EndPosition = FVector::ZeroVector;

	/** Volume of material. */
	UPROPERTY(BlueprintReadOnly)
	int32 Volume = 0;

	/** Material that slid. */
	UPROPERTY(BlueprintReadOnly)
	EGeoMaterial Material = EGeoMaterial::Topsoil;
};

/** Delegates for events. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCollapseEvent, const FCollapseEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLandslideEvent, const FLandslideEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSupportWarning, const FMineSupportData&, Support);

/**
 * Structural integrity simulation engine.
 *
 * Two systems:
 * 1. Underground stress propagation — voxels have weight, weight flows down
 *    to supports or bedrock. Unsupported spans collapse.
 * 2. Surface landslide — steep height differences cascade dirt downhill.
 *
 * Performance: Runs as a batched background calculation, never on game thread.
 * Only recalculates affected regions when terrain changes.
 */
UCLASS()
class AOC_API UTerraForgeStructural : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize with subsystem reference. */
	void Initialize(UTerraForgeSubsystem* InSubsystem);

	/** Tick structural calculations. Called from subsystem tick. */
	void Tick(float DeltaTime);

	// ── Support Management ──────────────────────────────────────────────────

	/** Place a mine support at the given position. Returns support ID or -1. */
	int32 PlaceSupport(const FVector& Position, int32 Tier = 1);

	/** Remove a support by ID. */
	void RemoveSupport(int32 SupportID);

	/** Get all supports in a region. */
	TArray<FMineSupportData> GetSupportsInRadius(const FVector& Center, float Radius) const;

	/** Get a specific support by ID. */
	const FMineSupportData* GetSupport(int32 SupportID) const;

	// ── Structural Queries ──────────────────────────────────────────────────

	/** Get the structural state at a position. */
	EStructuralState GetStructuralState(const FVector& Position) const;

	/** Get the stress value at a position (0-1, 1 = about to collapse). */
	float GetStressAt(const FVector& Position) const;

	/** Is this position underground (has voxels above)? */
	bool IsUnderground(const FVector& Position) const;

	/** Get the nearest support distance from a position. */
	float GetNearestSupportDistance(const FVector& Position) const;

	// ── Dirty Region ────────────────────────────────────────────────────────

	/** Mark a region as needing structural recalculation. */
	void MarkDirty(const FVector& Center, float Radius);

	// ── Events ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Structural")
	FOnCollapseEvent OnCollapse;

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Structural")
	FOnLandslideEvent OnLandslide;

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Structural")
	FOnSupportWarning OnSupportWarning;

	// ── Configuration ───────────────────────────────────────────────────────

	/** Maximum unsupported span before collapse (in voxels). */
	UPROPERTY(EditAnywhere, Category = "TerraForge|Structural")
	int32 MaxUnsupportedSpan = 6; // 3m (6 voxels at 0.5m)

	/** Height difference threshold for landslide cascade (cm). */
	UPROPERTY(EditAnywhere, Category = "TerraForge|Structural")
	float LandslideThreshold = 200.0f; // 2m

	/** Support degradation rate per second under full load. */
	UPROPERTY(EditAnywhere, Category = "TerraForge|Structural")
	float SupportDegradeRate = 0.01f; // Very slow — supports last hours

	/** Update interval for structural calculations (seconds). */
	UPROPERTY(EditAnywhere, Category = "TerraForge|Structural")
	float UpdateInterval = 0.5f; // 2 Hz structural update

private:
	// ── Internal ────────────────────────────────────────────────────────────

	/** Reference to the TerraForge subsystem. */
	UPROPERTY()
	TObjectPtr<UTerraForgeSubsystem> Subsystem;

	/** All placed mine supports. */
	UPROPERTY()
	TArray<FMineSupportData> Supports;

	/** Next support ID counter. */
	int32 NextSupportID = 1;

	/** Dirty regions needing recalculation. */
	TArray<TPair<FVector, float>> DirtyRegions;

	/** Timer for throttled updates. */
	float UpdateTimer = 0.0f;

	/** Cached stress map: ChunkKey → array of stress values. */
	TMap<FIntVector, TArray<float>> StressCache;

	// ── Calculation Methods ─────────────────────────────────────────────────

	/** Recalculate stress in a dirty region. */
	void RecalculateRegion(const FVector& Center, float Radius);

	/** Calculate stress for a single column of voxels. */
	float CalculateColumnStress(const FIntVector& ColumnXY, int32 MinZ, int32 MaxZ) const;

	/** Find the nearest support's contribution to a position. */
	float GetSupportContribution(const FVector& VoxelWorldPos) const;

	/** Execute a collapse at a position. */
	void ExecuteCollapse(const FVector& Center, float Radius, EGeoMaterial Material);

	/** Check and execute surface landslides. */
	void CheckLandslides(const FVector& Center, float Radius);

	/** Degrade supports under load. */
	void DegradeSupports(float DeltaTime);

	/** Get material weight multiplier. */
	float GetMaterialWeight(EGeoMaterial Material) const;
};
