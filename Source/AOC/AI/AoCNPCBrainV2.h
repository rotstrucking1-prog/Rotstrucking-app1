// AoCNPCBrainV2.h — Master NPC AI controller with GOAP + Life Simulation
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCGoalPlanner.h"
#include "AoCNPCPersonality.h"
#include "AoCNPCBrainV2.generated.h"

class UAoCNPCNeedSystem;
class UAoCNPCMemory;
class UAoCNPCInventory;
class UAoCNPCGoalPlanner;
class UAoCNPCPersonality;
class UAoCAnimationLab;
class UAoCSmartAnimPlayer;

/** V2 brain execution states */
UENUM(BlueprintType)
enum class ENPCBrainState : uint8
{
	Planning             UMETA(DisplayName = "Planning"),
	Executing            UMETA(DisplayName = "Executing"),
	Interrupted          UMETA(DisplayName = "Interrupted"),
	WaitingForAnimation  UMETA(DisplayName = "Waiting For Animation"),
	Traveling            UMETA(DisplayName = "Traveling"),
	Gathering            UMETA(DisplayName = "Gathering"),
	Crafting             UMETA(DisplayName = "Crafting"),
	Fighting             UMETA(DisplayName = "Fighting"),
	Fleeing              UMETA(DisplayName = "Fleeing"),
	Socializing          UMETA(DisplayName = "Socializing"),
	Resting              UMETA(DisplayName = "Resting"),
	Dead                 UMETA(DisplayName = "Dead")
};

/** Broadcast when the brain state changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBrainStateChanged, ENPCBrainState, OldState, ENPCBrainState, NewState);

/**
 * UAoCNPCBrainV2
 *
 * The master NPC AI controller that ties together all subsystems:
 * NeedSystem, Memory, Inventory, GoalPlanner, and Personality.
 * Replaces AoCNPCBrain with a full life simulation where NPCs
 * autonomously plan multi-step actions, get interrupted by needs,
 * and use all game systems (crafting, combat, spells, gathering).
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCBrainV2 : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCBrainV2();

	// --- Sub-components (auto-created) ------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2")
	TObjectPtr<UAoCNPCNeedSystem> NeedSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2")
	TObjectPtr<UAoCNPCMemory> Memory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2")
	TObjectPtr<UAoCNPCInventory> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2")
	TObjectPtr<UAoCNPCGoalPlanner> GoalPlanner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2")
	TObjectPtr<UAoCNPCPersonality> Personality;

	// --- Configuration ----------------------------------------------------------

	/** Perception radius for detecting threats and allies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Perception")
	float PerceptionRadius = 2000.f;

	/** Distance threshold to consider NPC "at" a location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Movement")
	float LocationArrivalThreshold = 200.f;

	/** Patrol waypoints (world-space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Behavior")
	TArray<FVector> PatrolPoints;

	/** Weapon type tags for animation queries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Combat")
	TArray<FString> WeaponTags;

	/** Known spells this NPC can cast (subset of the 160 spell IDs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Magic")
	TArray<FName> KnownSpells;

	/** Magic schools this NPC specializes in (for animation tag filtering) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Magic")
	TArray<FName> MagicSchools;

	/** Seconds between threat memory cleanup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|BrainV2|Memory")
	float ThreatMemoryDuration = 60.f;

	// --- Runtime state ----------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2|State")
	ENPCBrainState CurrentState = ENPCBrainState::Planning;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2|State")
	ENPCGoal CurrentGoal = ENPCGoal::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|BrainV2|State")
	float HealthRatio = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|AI|BrainV2|State")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|AI|BrainV2|State")
	TArray<TObjectPtr<AActor>> DetectedActors;

	// --- Delegates --------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AoC|AI|BrainV2")
	FOnBrainStateChanged OnBrainStateChanged;

	// --- Public API -------------------------------------------------------------

	/** Set the health ratio (0-1) from NPC character. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|BrainV2")
	void SetHealthRatio(float Ratio);

	/** Notify the brain that we were hit. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|BrainV2")
	void NotifyHit(const FVector& HitDirection, float Damage);

	/** Notify the brain of a noise at a location. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|BrainV2")
	void NotifyNoise(const FVector& NoiseLocation, float Loudness);

	/** Force a specific goal (e.g., from death). */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|BrainV2")
	void ForceGoal(ENPCGoal NewGoal);

	/** Is the NPC dead? */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|BrainV2")
	bool IsDead() const { return CurrentState == ENPCBrainState::Dead; }

	/** Cast the best available spell at a target. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|BrainV2")
	void CastBestSpell(AActor* Target);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Cached references ------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UAoCAnimationLab> AnimLab;

	UPROPERTY()
	TObjectPtr<UAoCSmartAnimPlayer> AnimPlayer;

	// --- Timer handles ----------------------------------------------------------

	FTimerHandle PerceptionTimerHandle;
	FTimerHandle PlanningTimerHandle;
	FTimerHandle ExecutionTimerHandle;

	// --- Internal state ---------------------------------------------------------

	/** Timer for current action duration tracking */
	float CurrentActionTimeRemaining = 0.f;

	/** Whether we're currently waiting for an action to complete */
	bool bWaitingForActionComplete = false;

	/** Combat combo variety tracking */
	FName LastAttackName = NAME_None;
	int32 AttackVarietyCounter = 0;

	/** Current patrol index */
	int32 CurrentPatrolIndex = 0;

	/** Investigate location */
	FVector InvestigateLocation = FVector::ZeroVector;
	bool bHasInvestigateTarget = false;

	/** Mana reference from owning character */
	float* OwnerManaPtr = nullptr;

	// --- Timer callbacks --------------------------------------------------------

	/** Perception: scan surroundings, update memory, update needs (0.5s) */
	void RunPerception();

	/** Planning: check for interruptions, create/update plans (1.0s) */
	void RunPlanning();

	/** Execution: execute current action in plan, play animations (0.5s) */
	void RunExecution();

	// --- State management -------------------------------------------------------

	void SetBrainState(ENPCBrainState NewState);

	// --- Perception helpers -----------------------------------------------------

	void UpdateLocationFlags();
	void UpdateThreatState();
	float CalculateSafetyFromSurroundings() const;

	// --- Execution helpers ------------------------------------------------------

	void ExecuteAction(const FNPCAction& Action);
	void ExecuteMoveToAction(const FNPCAction& Action);
	void ExecuteGatherAction(const FNPCAction& Action);
	void ExecuteCraftAction(const FNPCAction& Action);
	void ExecuteCombatAction(const FNPCAction& Action);
	void ExecuteSocialAction(const FNPCAction& Action);
	void ExecuteRestAction(const FNPCAction& Action);
	void ExecuteConsumeAction(const FNPCAction& Action);
	void ExecuteGenericAction(const FNPCAction& Action);

	/** Combat decision-making */
	void ExecuteCombatTurn();

	// --- Animation helpers ------------------------------------------------------

	void PlayFromLab(const FString& Category, const FString& BodyState, const TArray<FString>& Tags);
	void PlayFromLabFName(FName Category, const TArray<FName>& Tags);

	// --- Movement ---------------------------------------------------------------

	void MoveToward(const FVector& Location, float Speed);
	bool IsNearLocation(const FVector& Location) const;
	FVector GetDirectionTo(const AActor* Target) const;
};
