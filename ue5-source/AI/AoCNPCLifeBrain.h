// AoCNPCLifeBrain.h
// NPC Life/Activity Brain — daily routines, goals, boredom, variety
// Makes NPCs feel alive with emergent behavioral chains
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCSkillSystem.h"
#include "AoCNPCLifeBrain.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCLife, Log, All);

class AoCHumanoidNPCV2;
class UAoCNPCNeedSystem;
class UAoCNPCMemory;
class UAoCNPCInventory;
class UAoCNPCPersonality;
class UAoCNPCSkillSystem;
class UAoCNPCSocialBrain;

/** Time of day enum */
UENUM(BlueprintType)
enum class ETimeOfDay : uint8
{
	Dawn,       // Wake up, eat
	Morning,    // Productive time
	Midday,     // Continue or break
	Afternoon,  // Second productive block
	Evening,    // Wind down, socialize, return to base
	Night       // Sleep (or hunt for nocturnal NPCs)
};

/** NPC activities */
UENUM(BlueprintType)
enum class ENPCActivity : uint8
{
	Idle,
	Mining,
	Woodcutting,
	Fishing,
	Hunting,
	Farming,
	Cooking,
	Crafting,
	Building,
	Training,
	Socializing,
	Trading,
	Eating,
	Sleeping,
	Exploring,
	Patrolling,
	HuntingPlayers,
	Guarding,
	Traveling,
	Looting,
	Resting // Sit/relax but not sleeping
};

/** Resource types for finding activity locations */
UENUM(BlueprintType)
enum class EResourceType : uint8
{
	OreNode,
	Tree,
	FishingSpot,
	HerbNode,
	FarmPlot,
	CookingFire,
	CraftingStation,
	TrainingDummy,
	Merchant,
	Bed,
	Water,
	HuntingGround,
	ConstructionSite,
	GuardPost
};

/** Goal priority level */
UENUM(BlueprintType)
enum class EGoalPriority : uint8
{
	LongTerm,   // Days of game time
	Medium,     // Hours
	Short,      // Minutes
	Immediate   // RIGHT NOW (critical needs)
};

/** Goal type categories */
UENUM(BlueprintType)
enum class EGoalType : uint8
{
	AcquireItem,       // Get a specific item or resource
	CraftItem,         // Craft something
	ReachSkillLevel,   // Train a skill to target
	AccumulateWealth,  // Get X gold
	BuildStructure,    // Construction goal
	ExploreArea,       // Discover new locations
	DefeatEnemy,       // Kill a specific enemy
	SocialGoal,        // Make friends, join group
	SurvivalGoal,      // Eat, rest, stay safe
	PatrolRoute,       // Complete a patrol circuit
	GearUpgrade        // Get better equipment
};

/** A single NPC goal */
USTRUCT(BlueprintType)
struct FNPCGoal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	FName GoalName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	EGoalType Type = EGoalType::SurvivalGoal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	EGoalPriority Priority = EGoalPriority::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float Progress = 0.f; // 0-1

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float TargetValue = 0.f; // e.g. 30 ore, skill level 50, 1000 gold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float CurrentValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	FName TargetItemName; // For acquire/craft goals

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	ESkillID TargetSkill = ESkillID::MAX; // For skill goals

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	FVector TargetLocation = FVector::ZeroVector; // For explore/travel goals

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	TArray<FNPCGoal> SubGoals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	bool bCompleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float TimeStarted = 0.f;

	bool IsComplete() const { return bCompleted || Progress >= 1.0f; }
};

/**
 * UAoCNPCLifeBrain — UActorComponent
 *
 * Makes NPCs feel ALIVE. Daily routines, goal hierarchies, boredom/variety,
 * and emergent behavior chains create realistic human-like behavior.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCLifeBrain : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCLifeBrain();

	virtual void BeginPlay() override;

	// --- Core Life Tick ---

	/** Main update — drives daily routine and goal pursuit */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void LifeTick(float DeltaTime);

	// --- Activity Management ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	ENPCActivity DecideNextActivity();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	ENPCActivity GetCurrentActivity() const { return CurrentActivity; }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void SetActivity(ENPCActivity NewActivity);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	float GetActivityDuration() const { return CurrentActivityDuration; }

	// --- Goal System ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void SetLongTermGoal(const FNPCGoal& Goal);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	TArray<FNPCGoal> BreakDownGoal(const FNPCGoal& Goal) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	bool HasGoalOfType(EGoalType Type) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	float GetGoalProgress(const FNPCGoal& Goal) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void GenerateGoalsFromArchetype();

	// --- Time of Day ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	ETimeOfDay GetTimeOfDay() const;

	// --- Boredom/Variety ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void UpdateTedium(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	bool ShouldSwitchActivity() const;

	// --- World Queries ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	FVector FindNearestResource(EResourceType ResourceType) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void EvaluateOpportunity(AActor* Something);

	// --- Navigation ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void ReturnToBase();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void TravelTo(const FVector& Destination);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	bool HasReachedDestination() const;

	// --- Event Handlers ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void OnNeedBecomesCritical(uint8 NeedType);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void OnCombatEnded(bool bWon);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void OnItemAcquired(FName ItemName, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Life")
	void OnSkillGained(ESkillID Skill, float NewLevel);

	// --- Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	TArray<FVector> PatrolWaypoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	bool bIsNocturnal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float DayLengthSeconds = 2400.f; // 40 real minutes = 1 game day

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float TediumThresholdBase = 60.f; // seconds before boredom (modified by Patience)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Life")
	float TediumDecayRate = 0.1f; // how fast tedium decays for activities not being done

private:
	// --- References ---
	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> OwnerNPC;

	UPROPERTY()
	UAoCNPCNeedSystem* NeedSystem = nullptr;

	UPROPERTY()
	UAoCNPCMemory* Memory = nullptr;

	UPROPERTY()
	UAoCNPCInventory* Inventory = nullptr;

	UPROPERTY()
	UAoCNPCPersonality* PersonalityComp = nullptr;

	UPROPERTY()
	UAoCNPCSkillSystem* SkillSystem = nullptr;

	UPROPERTY()
	UAoCNPCSocialBrain* SocialBrain = nullptr;

	// --- Activity State ---
	ENPCActivity CurrentActivity = ENPCActivity::Idle;
	float CurrentActivityDuration = 0.f;
	float CurrentActivityStartTime = 0.f;
	FVector CurrentActivityLocation = FVector::ZeroVector;

	/** Tedium per activity (increases while doing, decays while not) */
	TMap<ENPCActivity, float> TediumMap;

	/** Travel state */
	FVector TravelDestination = FVector::ZeroVector;
	bool bIsTraveling = false;
	int32 CurrentPatrolIndex = 0;

	// --- Goal State ---
	TArray<FNPCGoal> LongTermGoals;
	TArray<FNPCGoal> MediumGoals;
	TArray<FNPCGoal> ShortGoals;
	FNPCGoal ImmediateGoal;
	bool bHasImmediateGoal = false;

	// --- Internal Logic ---

	/** Tick the current activity (mining grants ore + skill XP, etc.) */
	void TickCurrentActivity(float DeltaTime);

	/** Check if any needs are critical and create immediate goals */
	void CheckCriticalNeeds();

	/** Advance goals — check progress, complete sub-goals, generate new sub-goals */
	void AdvanceGoals(float DeltaTime);

	/** Generate a long-term goal appropriate to the NPC's archetype/personality */
	FNPCGoal GenerateLongTermGoal() const;

	/** Determine what activity best serves the current active goal */
	ENPCActivity GetActivityForGoal(const FNPCGoal& Goal) const;

	/** Get the skill associated with an activity */
	ESkillID GetSkillForActivity(ENPCActivity Activity) const;

	/** Get the resource type associated with an activity */
	EResourceType GetResourceForActivity(ENPCActivity Activity) const;

	/** Is this a good time to sleep? */
	bool ShouldSleep() const;

	/** Is this a good time to socialize? */
	bool ShouldSocialize() const;

	/** Priority score for an activity (higher = more important right now) */
	float ScoreActivity(ENPCActivity Activity) const;

	/** Break down a gear upgrade goal into sub-goals */
	TArray<FNPCGoal> BreakDownGearGoal(const FNPCGoal& Goal) const;

	/** Break down an acquire item goal */
	TArray<FNPCGoal> BreakDownAcquireGoal(const FNPCGoal& Goal) const;

	/** Break down a craft item goal */
	TArray<FNPCGoal> BreakDownCraftGoal(const FNPCGoal& Goal) const;

	/** Break down a skill training goal */
	TArray<FNPCGoal> BreakDownSkillGoal(const FNPCGoal& Goal) const;

	/** Get the NPC's patience-modified tedium threshold */
	float GetTediumThreshold() const;

	/** Find something useful to do near current location */
	ENPCActivity FindOpportunisticActivity() const;

	/** World time helper */
	float GetWorldTime() const;

	FRandomStream LifeRand;
};
