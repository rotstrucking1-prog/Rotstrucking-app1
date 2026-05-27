// AoCNPCGoalPlanner.h — GOAP (Goal-Oriented Action Planning) system
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCGoalPlanner.generated.h"

class UAoCNPCNeedSystem;
class UAoCNPCMemory;
class UAoCNPCInventory;
class UAoCNPCPersonality;

/** All possible NPC goals */
UENUM(BlueprintType)
enum class ENPCGoal : uint8
{
	SatisfyHunger     UMETA(DisplayName = "Satisfy Hunger"),
	RestoreEnergy     UMETA(DisplayName = "Restore Energy"),
	SeekSafety        UMETA(DisplayName = "Seek Safety"),
	Socialize         UMETA(DisplayName = "Socialize"),
	GatherResource    UMETA(DisplayName = "Gather Resource"),
	CraftItem         UMETA(DisplayName = "Craft Item"),
	BuildStructure    UMETA(DisplayName = "Build Structure"),
	EquipGear         UMETA(DisplayName = "Equip Gear"),
	HuntPlayers       UMETA(DisplayName = "Hunt Players"),
	PatrolArea        UMETA(DisplayName = "Patrol Area"),
	GuardLocation     UMETA(DisplayName = "Guard Location"),
	InvestigateNoise  UMETA(DisplayName = "Investigate Noise"),
	Trade             UMETA(DisplayName = "Trade"),
	Fish              UMETA(DisplayName = "Fish"),
	Cook              UMETA(DisplayName = "Cook"),
	Mine              UMETA(DisplayName = "Mine"),
	Chop              UMETA(DisplayName = "Chop"),
	Smelt             UMETA(DisplayName = "Smelt"),
	Farm              UMETA(DisplayName = "Farm"),
	AttackTarget      UMETA(DisplayName = "Attack Target"),
	FleeFromThreat    UMETA(DisplayName = "Flee From Threat"),
	HideFromThreat    UMETA(DisplayName = "Hide From Threat"),
	ExploreWorld      UMETA(DisplayName = "Explore World"),
	ReturnHome        UMETA(DisplayName = "Return Home"),
	Idle              UMETA(DisplayName = "Idle"),
	Dead              UMETA(DisplayName = "Dead")
};

/** A single action the NPC can perform as part of a plan */
USTRUCT(BlueprintType)
struct FNPCAction
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

	/** Unique action identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	FName ActionID;

	/** Conditions that must be true in world state before this action can run */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	TMap<FName, bool> Preconditions;

	/** Changes applied to world state after this action completes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	TMap<FName, bool> Effects;

	/** Cost for A* planning — lower = preferred */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	float Cost = 5.f;

	/** Animation category to query from AnimationLab */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	FName AnimCategory;

	/** Tags to filter animations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	TArray<FName> AnimTags;

	/** How long this action takes (0 = use animation duration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	float Duration = 0.f;

	/** Can hunger/energy critical needs interrupt this action? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	bool bInterruptibleByNeeds = true;

	/** Must be at a specific location type (empty = anywhere) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	FName RequiresLocation;

	/** Which need this action satisfies and by how much (0 = doesn't satisfy any) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	FName SatisfiesNeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	float SatisfiesAmount = 0.f;

	bool IsValid() const { return !ActionID.IsNone(); }
};

/** A complete plan: an ordered sequence of actions to achieve a goal */
USTRUCT(BlueprintType)
struct FNPCPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	ENPCGoal Goal = ENPCGoal::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	TArray<FNPCAction> Actions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	int32 CurrentActionIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	float Priority = 0.f;

	/** Index where this plan was interrupted (-1 = not interrupted) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	int32 InterruptedAtIndex = -1;

	/** The goal we were working on before interruption */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	ENPCGoal OriginalGoal = ENPCGoal::Idle;

	bool IsValid() const { return Actions.Num() > 0; }
	bool IsComplete() const { return CurrentActionIndex >= Actions.Num(); }
	FNPCAction GetCurrentAction() const
	{
		return (CurrentActionIndex >= 0 && CurrentActionIndex < Actions.Num())
			? Actions[CurrentActionIndex]
			: FNPCAction();
	}
};

/** Delegate for plan events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanChanged, ENPCGoal, NewGoal);

/**
 * UAoCNPCGoalPlanner
 *
 * The GOAP (Goal-Oriented Action Planning) system. Builds multi-step plans
 * by searching through an action space using A* to find the cheapest
 * sequence of actions that transitions from the current world state to
 * a goal state.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCGoalPlanner : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCGoalPlanner();

	// --- Configuration ----------------------------------------------------------

	/** Maximum plan search depth (can be overridden by personality) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	int32 MaxPlanDepth = 8;

	/** Maximum A* nodes to expand before giving up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|GOAP")
	int32 MaxSearchNodes = 200;

	// --- Runtime state ----------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|GOAP")
	FNPCPlan CurrentPlan;

	/** The plan we were executing before an interruption (for resumption) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|GOAP")
	FNPCPlan InterruptedPlan;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|GOAP")
	bool bHasInterruptedPlan = false;

	// --- Delegates --------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AoC|AI|GOAP")
	FOnPlanChanged OnPlanChanged;

	// --- Component references (set by BrainV2) ----------------------------------

	UPROPERTY()
	TObjectPtr<UAoCNPCNeedSystem> NeedSystem;

	UPROPERTY()
	TObjectPtr<UAoCNPCMemory> Memory;

	UPROPERTY()
	TObjectPtr<UAoCNPCInventory> Inventory;

	UPROPERTY()
	TObjectPtr<UAoCNPCPersonality> Personality;

	// --- Public API -------------------------------------------------------------

	/** Create a plan to achieve the given goal. Returns an empty plan if no path found. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	FNPCPlan CreatePlan(ENPCGoal Goal);

	/** Build the current world state from all NPC components. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	TMap<FName, bool> GetWorldState() const;

	/** Get all actions available in the current context. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	TArray<FNPCAction> GetAvailableActions() const;

	/** Score all goals and pick the best one based on needs and personality. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	ENPCGoal SelectBestGoal() const;

	/** Can the new goal interrupt the current plan? */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|GOAP")
	bool CanInterruptCurrentPlan(ENPCGoal NewGoal) const;

	/** Resume a previously interrupted plan. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	void ResumePlan();

	/** Register a custom action into the action library. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	void RegisterAction(const FNPCAction& Action);

	/** Get the goal conditions for a specific goal. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	TMap<FName, bool> GetGoalConditions(ENPCGoal Goal) const;

	/** Is there a current valid plan being executed? */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|GOAP")
	bool HasActivePlan() const;

	/** Additional world state flags set externally (e.g., InCombat, TargetVisible) */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|AI|GOAP")
	TMap<FName, bool> ExternalWorldState;

protected:
	virtual void BeginPlay() override;

private:
	/** All registered actions */
	TArray<FNPCAction> ActionLibrary;

	/** Register all built-in actions */
	void RegisterBuiltInActions();

	/** A* node for planning */
	struct FPlanNode
	{
		TMap<FName, bool> State;
		TArray<int32> ActionSequence; // indices into ActionLibrary
		float GCost = 0.f;           // cost so far
		float HCost = 0.f;           // heuristic to goal
		float FCost() const { return GCost + HCost; }
	};

	/** Check if all preconditions of an action are met in the given state */
	bool ArePreconditionsMet(const FNPCAction& Action, const TMap<FName, bool>& State) const;

	/** Apply action effects to a world state (returns new state) */
	TMap<FName, bool> ApplyEffects(const TMap<FName, bool>& State, const FNPCAction& Action) const;

	/** Heuristic: count how many goal conditions are not yet satisfied */
	float CalculateHeuristic(const TMap<FName, bool>& State, const TMap<FName, bool>& GoalConditions) const;

	/** Check if the goal conditions are all satisfied in the given state */
	bool IsGoalSatisfied(const TMap<FName, bool>& State, const TMap<FName, bool>& GoalConditions) const;

	/** Helper to create a move-to action */
	FNPCAction MakeMoveToAction(FName LocationName) const;

	/** Helper to create a generic action */
	FNPCAction MakeAction(FName ID, const TMap<FName, bool>& Pre, const TMap<FName, bool>& Eff,
	                      float Cost, FName AnimCat = NAME_None, TArray<FName> Tags = {},
	                      float Dur = 0.f, bool Interruptible = true, FName ReqLoc = NAME_None,
	                      FName SatNeed = NAME_None, float SatAmt = 0.f) const;

	public:
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	void AddGoal(const FString& GoalName, float Priority);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	void PushGoal(const FString& GoalName, float Priority);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|GOAP")
	void SetWorldState(const FString& Key, const FString& Value);

};
