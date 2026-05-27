// AoCAILODManager.h
// AI LOD (Level of Detail) Manager — UWorldSubsystem
// Performance backbone: manages simulation fidelity for all humanoid NPCs
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AoCAILODManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCNPC, Log, All);

class AoCHumanoidNPCV2;
class APlayerController;

/** LOD fidelity tiers based on distance to nearest player */
UENUM(BlueprintType)
enum class EAILODTier : uint8
{
	Full        UMETA(DisplayName = "Full"),        // 0-80m: Full mesh, animation, perception every tick, pathfinding, combat
	Reduced     UMETA(DisplayName = "Reduced"),     // 80-300m: Full mesh, animation, perception every 0.5s, path every 2s
	Background  UMETA(DisplayName = "Background"),  // 300-1000m: NO mesh, NO animation, data-only sim every 1s
	Hibernated  UMETA(DisplayName = "Hibernated")   // 1000m+: Frozen, major decisions only every 30s
};

/** Per-NPC tracking data used by the LOD manager */
USTRUCT()
struct FNPCLODRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> NPC = nullptr;

	EAILODTier CurrentTier = EAILODTier::Hibernated;
	EAILODTier PreviousTier = EAILODTier::Hibernated;

	float DistanceToNearestPlayer = MAX_FLT;

	/** Stagger group index (0-7) for spreading tick load */
	uint8 StaggerGroup = 0;

	/** Whether this NPC currently has visual components spawned */
	bool bIsMaterialized = false;

	/** Timestamp of last full tick for stagger scheduling */
	float LastFullTick = 0.f;

	/** Timestamp of last reduced perception tick */
	float LastReducedPerceptionTick = 0.f;

	/** Timestamp of last background sim tick */
	float LastBackgroundTick = 0.f;

	/** Timestamp of last hibernated decision tick */
	float LastHibernatedTick = 0.f;
};

/** Queued materialization request to avoid spawning too many meshes at once */
USTRUCT()
struct FMaterializationRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> NPC = nullptr;

	bool bMaterialize = true; // true = materialize, false = dematerialize
};

/**
 * UAoCAILODManager — UWorldSubsystem (singleton per world)
 *
 * Manages ALL humanoid NPCs and determines their simulation fidelity
 * based on distance to nearest player. Uses a spatial hash grid for
 * O(1) distance lookups, stagger groups to spread tick load, and
 * a budget system to prevent AI from exceeding frame time targets.
 */
UCLASS()
class AOC_API UAoCAILODManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UAoCAILODManager();

	// --- UWorldSubsystem interface ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Called each frame by registered tick function
	void Tick(float DeltaTime);

	// --- NPC Registration ---
	void RegisterNPC(AoCHumanoidNPCV2* NPC);
	void UnregisterNPC(AoCHumanoidNPCV2* NPC);

	// --- Queries ---
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|LOD")
	EAILODTier GetLODTier(AoCHumanoidNPCV2* NPC) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|LOD")
	TArray<AoCHumanoidNPCV2*> GetNPCsInRadius(const FVector& Center, float Radius) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|LOD")
	AoCHumanoidNPCV2* GetNearestNPCTo(AActor* Actor) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|LOD")
	int32 GetTotalManagedNPCs() const { return NPCRecords.Num(); }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|LOD")
	int32 GetNPCCountInTier(EAILODTier Tier) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|LOD")
	float GetAIFrameTimeMs() const { return LastAIFrameTimeMs; }

	// --- Materialization ---
	void MaterializeNPC(AoCHumanoidNPCV2* NPC);
	void DematerializeNPC(AoCHumanoidNPCV2* NPC);

	// --- Debug ---
	void DebugDraw() const;

	// --- Configuration ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float FullTierMaxDistance = 8000.f; // 80m in UE units (1m = 100 units)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float ReducedTierMaxDistance = 30000.f; // 300m

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float BackgroundTierMaxDistance = 100000.f; // 1000m

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float AIBudgetMs = 4.0f; // Max milliseconds per frame for AI

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	int32 MaxMaterializationsPerFrame = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float SpatialCellSize = 10000.f; // 100m cells

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	int32 NumStaggerGroups = 8;

	// Interval timers for lower LOD tiers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float ReducedPerceptionInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float BackgroundSimInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|LOD")
	float HibernatedDecisionInterval = 30.0f;

private:
	// --- Spatial Hash Grid ---
	FIntVector WorldToCell(const FVector& WorldPos) const;
	void RebuildSpatialHash();
	void UpdatePlayerPositions();
	float GetDistanceToNearestPlayer(const FVector& Location) const;

	// --- Tier Assignment ---
	EAILODTier CalculateTier(float Distance) const;
	void UpdateNPCTiers();
	void HandleTierTransition(FNPCLODRecord& Record, EAILODTier NewTier);

	// --- Stagger ---
	uint8 GetCurrentStaggerGroup() const;

	// --- Budget ---
	void ApplyBudgetThrottling();

	// --- Materialization Queue ---
	void ProcessMaterializationQueue();

	// --- Internal data ---
	TArray<FNPCLODRecord> NPCRecords;

	/** Spatial hash: cell coord → indices into NPCRecords */
	TMap<FIntVector, TArray<int32>> SpatialHash;

	/** Cached player pawn positions updated each frame */
	TArray<FVector> PlayerPositions;

	/** Queue of materialization / dematerialization requests */
	TArray<FMaterializationRequest> MaterializationQueue;

	/** Frame counter for stagger group rotation */
	uint32 FrameCounter = 0;

	/** Measured AI time last frame (ms) */
	float LastAIFrameTimeMs = 0.f;

	/** True if budget was exceeded last frame and we're throttling */
	bool bBudgetExceeded = false;

	/** Tick delegate handle */
	FDelegateHandle TickDelegateHandle;

	/** Quick lookup from NPC pointer to record index */
	TMap<TWeakObjectPtr<AoCHumanoidNPCV2>, int32> NPCToRecordIndex;
};
