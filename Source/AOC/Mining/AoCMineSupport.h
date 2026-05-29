// AoCMineSupport.h
// Architect of Creation - Mine Support Actor
// Structural support beams and columns that prevent tunnel collapse.
// Unsupported tunnels accumulate instability over 24h game time → INSTANT DEATH on collapse.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCVoxelTypes.h"
#include "AoCMineSupport.generated.h"

class UStaticMeshComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class AAoCVoxelWorld;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESupportType : uint8
{
	Beam			UMETA(DisplayName = "Support Beam"),
	Column			UMETA(DisplayName = "Support Column")
};

UENUM(BlueprintType)
enum class ECollapseWarningLevel : uint8
{
	None			UMETA(DisplayName = "None"),
	Low				UMETA(DisplayName = "Low — Dust Particles"),
	Medium			UMETA(DisplayName = "Medium — Creaking Sounds"),
	High			UMETA(DisplayName = "High — Cracking + Heavy Dust"),
	Critical		UMETA(DisplayName = "Critical — Imminent Collapse")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

/** Build requirements for a mine support structure. */
USTRUCT(BlueprintType)
struct AOC_API FSupportBuildRequirements
{
	GENERATED_BODY()

	/** Number of boards required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Build")
	int32 BoardsRequired = 0;

	/** Number of hardwood billets required (Column only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Build")
	int32 HardwoodBilletsRequired = 0;

	/** Display string for the build requirements. */
	FString ToString() const
	{
		if (HardwoodBilletsRequired > 0)
		{
			return FString::Printf(TEXT("%d Boards + %d Hardwood Billets"), BoardsRequired, HardwoodBilletsRequired);
		}
		return FString::Printf(TEXT("%d Boards"), BoardsRequired);
	}
};

/** Tile instability tracking data. */
USTRUCT(BlueprintType)
struct AOC_API FTileInstability
{
	GENERATED_BODY()

	/** Tile position in world voxel coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse")
	FIntVector TilePos = FIntVector::ZeroValue;

	/** Current instability value (0.0 = stable, 1.0 = collapse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Instability = 0.f;

	/** True if this tile is supported by a beam or column. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse")
	bool bIsSupported = false;

	/** Time in seconds since this tile became unsupported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse")
	float UnsupportedTime = 0.f;
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSupportPlacedDelegate, AAoCMineSupport*, PlacedSupport);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSupportDestroyedDelegate, AAoCMineSupport*, DestroyedSupport);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCollapseWarningDelegate, FIntVector, TilePos, ECollapseWarningLevel, WarningLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCollapseDelegate, FIntVector, TilePos);

// ─── Actor ───────────────────────────────────────────────────────────────────

UCLASS()
class AOC_API AAoCMineSupport : public AActor
{
	GENERATED_BODY()

public:
	AAoCMineSupport();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ── Components ──────────────────────────────────────────────────────────

	/** Visual mesh for the support structure (beam or column model). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MineSupport|Mesh")
	UStaticMeshComponent* SupportMesh;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Type of support: Beam (1 tile) or Column (3×3 tiles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Config")
	ESupportType SupportType;

	/**
	 * Coverage radius in tiles.
	 * Beam: 0 (covers only its own tile).
	 * Column: 1 (covers a 3×3 area centered on column).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Config", meta = (ClampMin = "0", ClampMax = "5"))
	int32 CoverageRadius;

	/** Current health of this support structure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|State", meta = (ClampMin = "0"))
	float Health;

	/** Maximum health of this support structure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Config", meta = (ClampMin = "1"))
	float MaxHealth;

	/** Tile position of this support in voxel coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Config")
	FIntVector TilePosition;

	/** Reference to the voxel world for collapse system registration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Config")
	AAoCVoxelWorld* VoxelWorld;

	/** Static mesh to use for beam supports (set in Blueprint or level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Mesh")
	UStaticMesh* BeamMesh;

	/** Static mesh to use for column supports (set in Blueprint or level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Mesh")
	UStaticMesh* ColumnMesh;

	// ── Collapse System Properties ──────────────────────────────────────────

	/**
	 * Game-time seconds before an unsupported tile collapses.
	 * Design: 24 hours of game time. Default assumes 1 real minute = 1 game hour.
	 * 24 game hours × 60 real seconds = 1440 real seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse")
	float CollapseTimeThreshold;

	/**
	 * Instability decay rate per second while unsupported.
	 * Instability = UnsupportedTime / CollapseTimeThreshold.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MineSupport|Collapse")
	TArray<FTileInstability> TrackedTiles;

	/** Niagara system for dust particle warning effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|VFX")
	UNiagaraSystem* DustParticleEffect;

	/** Sound to play for creaking warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Audio")
	USoundBase* CreakingSound;

	/** Sound to play for cracking warning (escalated). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Audio")
	USoundBase* CrackingSound;

	/** Sound to play on actual collapse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Audio")
	USoundBase* CollapseSound;

	/** Damage amount applied to actors in the collapse zone. Very high = instant death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse")
	float CollapseDamage;

	/** Radius in centimeters for the collapse damage zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MineSupport|Collapse")
	float CollapseDamageRadius;

	// ── Delegates ───────────────────────────────────────────────────────────

	/** Fired when this support structure is placed in the world. */
	UPROPERTY(BlueprintAssignable, Category = "MineSupport|Events")
	FOnSupportPlacedDelegate OnSupportPlaced;

	/** Fired when this support structure is destroyed (health reaches 0). */
	UPROPERTY(BlueprintAssignable, Category = "MineSupport|Events")
	FOnSupportDestroyedDelegate OnSupportDestroyed;

	/** Fired when a tracked tile transitions to a new warning level. */
	UPROPERTY(BlueprintAssignable, Category = "MineSupport|Events")
	FOnCollapseWarningDelegate OnCollapseWarning;

	/** Fired when a tile actually collapses (instant death zone). */
	UPROPERTY(BlueprintAssignable, Category = "MineSupport|Events")
	FOnCollapseDelegate OnCollapse;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Called after the support is placed in the world.
	 * Sets up the mesh, registers with the collapse system, marks covered tiles as supported.
	 */
	UFUNCTION(BlueprintCallable, Category = "MineSupport")
	void OnPlaced();

	/**
	 * Get all tile positions covered by this support.
	 * Beam: returns 1 tile (its own position).
	 * Column: returns up to 9 tiles (3×3 centered on column).
	 * @return Array of tile positions in voxel world coordinates
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MineSupport")
	TArray<FIntVector> GetSupportedTiles() const;

	/**
	 * Apply damage to this support structure.
	 * If health reaches 0, the support is destroyed and covered tiles become unsupported.
	 * @param DamageAmount Amount of damage to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "MineSupport")
	void ApplyDamage(float DamageAmount);

	/**
	 * Get the current health percentage of this support.
	 * @return 0.0 to 1.0
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MineSupport")
	float GetHealthPercentage() const;

	/**
	 * Get the build requirements for a given support type.
	 * @param Type The support type to query
	 * @return Build requirements struct
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MineSupport|Static")
	static FSupportBuildRequirements GetBuildRequirements(ESupportType Type);

	/**
	 * Check if a tile position currently has support coverage from any mine support.
	 * Queries the world for all AAoCMineSupport actors and checks coverage.
	 * @param World The UWorld to search in
	 * @param TilePos Tile position in voxel coordinates
	 * @return True if the tile is supported
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MineSupport|Static")
	static bool CheckTileHasSupport(const UWorld* World, FIntVector TilePos);

	/**
	 * Get the collapse warning level for a tile based on its instability.
	 * @param Instability Current instability value (0.0 to 1.0)
	 * @return Warning level enum
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MineSupport|Static")
	static ECollapseWarningLevel GetWarningLevel(float Instability);

	// ── Collapse Tracking ───────────────────────────────────────────────────

	/**
	 * Register a tile for instability tracking (called when air is carved nearby).
	 * @param TilePos Tile position to track
	 */
	UFUNCTION(BlueprintCallable, Category = "MineSupport|Collapse")
	void RegisterTileForTracking(FIntVector TilePos);

	/**
	 * Mark a tile as supported (resets its instability).
	 * @param TilePos Tile position to mark
	 */
	UFUNCTION(BlueprintCallable, Category = "MineSupport|Collapse")
	void MarkTileSupported(FIntVector TilePos);

	/**
	 * Mark a tile as unsupported (begins instability accumulation).
	 * @param TilePos Tile position to mark
	 */
	UFUNCTION(BlueprintCallable, Category = "MineSupport|Collapse")
	void MarkTileUnsupported(FIntVector TilePos);

private:
	/** Apply the correct mesh based on SupportType. */
	void SetupMesh();

	/** Destroy this support and unmark all covered tiles. */
	void DestroySupport();

	/**
	 * Trigger a collapse at the given tile position.
	 * Applies massive damage to all actors in range — INSTANT DEATH.
	 * @param TilePos The tile that collapsed
	 */
	void TriggerCollapse(FIntVector TilePos);

	/**
	 * Spawn warning visual/audio effects for a tile at the given warning level.
	 * @param TilePos Tile position
	 * @param Level Warning level
	 */
	void SpawnWarningEffects(FIntVector TilePos, ECollapseWarningLevel Level);

	/** Active dust particle component for warning effects. */
	UPROPERTY()
	UNiagaraComponent* ActiveDustEffect;

	/** Track the last warning level per tile to avoid spamming effects. */
	TMap<FIntVector, ECollapseWarningLevel> LastWarningLevels;

	/** Timer handle for periodic instability checks. */
	FTimerHandle InstabilityCheckTimer;

	/** Interval in seconds between instability update ticks. */
	static constexpr float InstabilityCheckInterval = 1.f;

	/** Auto-find the voxel world in the level if not set. */
	void FindVoxelWorld();
};
