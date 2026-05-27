// AoCNPCTaskGovernor.h
// Architect of Creation — NPC Task Governor System
// 
// The Task Governor is the safety, realism, and scheduling layer that wraps ALL NPC actions.
// It ensures NPCs obey realistic time scales for gathering, crafting, building, and living.
// It prevents rapid task switching, detects stuck states, verifies action completion,
// and manages day/night schedules so NPCs feel like living creatures with routines.
//
// KEY DESIGN: NPCs ARE the game. When only 3 real players are online, 200 NPCs must
// make the world feel alive. Players should NOT be able to tell NPC from human for
// at least 10-15 minutes of observation. The Task Governor enforces the pacing that
// makes this possible — no instant teleporting between tasks, no superhuman efficiency.
//
// INTEGRATION: Sits between the GOAP planner and the action execution layer. The planner
// decides WHAT to do; the Task Governor decides WHEN and HOW LONG it takes, and verifies
// the result. Works with AoCAILODManager to scale tick frequency by NPC importance.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCTaskGovernor.generated.h"

// Forward declarations
class UAoCNPCBrainV2;
class UAoCNPCSkillSystem;
class UAoCNPCImperfection;
class AAoCNPCCharacter;
class UAoCAILODManager;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * All discrete action types an NPC can perform. Each maps to a minimum and maximum
 * duration in the task governor's timing tables. These are the atomic units of
 * NPC behavior — the GOAP planner chains them into plans.
 */
UENUM(BlueprintType)
enum class ENPCActionType : uint8
{
	None					UMETA(DisplayName = "None"),

	// --- MOVEMENT ---
	MoveTo					UMETA(DisplayName = "Move To Location"),
	Sprint					UMETA(DisplayName = "Sprint"),
	Swim					UMETA(DisplayName = "Swim"),

	// --- GATHERING ---
	MiningSwing				UMETA(DisplayName = "Mining Swing"),
	WoodcuttingChop			UMETA(DisplayName = "Woodcutting Chop"),
	FishingCast				UMETA(DisplayName = "Fishing Cast"),
	FishingWait				UMETA(DisplayName = "Fishing Wait"),
	FishingReel				UMETA(DisplayName = "Fishing Reel"),
	HerbGather				UMETA(DisplayName = "Herb Gather"),
	SkinCorpse				UMETA(DisplayName = "Skin Corpse"),
	HuntTrack				UMETA(DisplayName = "Hunt Track"),
	FarmPlant				UMETA(DisplayName = "Farm Plant"),
	FarmHarvest				UMETA(DisplayName = "Farm Harvest"),

	// --- PROCESSING ---
	SmeltOre				UMETA(DisplayName = "Smelt Ore"),
	SawLogs					UMETA(DisplayName = "Saw Logs"),
	TanHide					UMETA(DisplayName = "Tan Hide"),
	GrindHerbs				UMETA(DisplayName = "Grind Herbs"),

	// --- CRAFTING ---
	CraftDagger				UMETA(DisplayName = "Craft Dagger"),
	CraftSword				UMETA(DisplayName = "Craft Sword"),
	CraftGreatsword			UMETA(DisplayName = "Craft Greatsword"),
	CraftAxe				UMETA(DisplayName = "Craft Axe"),
	CraftHelmet				UMETA(DisplayName = "Craft Helmet"),
	CraftChestplate			UMETA(DisplayName = "Craft Chestplate"),
	CraftLeggings			UMETA(DisplayName = "Craft Leggings"),
	CraftGauntlets			UMETA(DisplayName = "Craft Gauntlets"),
	CraftBoots				UMETA(DisplayName = "Craft Boots"),
	CraftShield				UMETA(DisplayName = "Craft Shield"),
	CraftBow				UMETA(DisplayName = "Craft Bow"),
	CraftStaff				UMETA(DisplayName = "Craft Staff"),
	CraftJewelry			UMETA(DisplayName = "Craft Jewelry"),
	CraftPotion				UMETA(DisplayName = "Brew Potion"),
	CookMeal				UMETA(DisplayName = "Cook Meal"),
	BrewDrink				UMETA(DisplayName = "Brew Drink"),
	Enchant					UMETA(DisplayName = "Enchant Item"),
	TailorCloth				UMETA(DisplayName = "Tailor Cloth"),
	LeatherworkItem			UMETA(DisplayName = "Leatherwork Item"),

	// --- BUILDING ---
	LayStone				UMETA(DisplayName = "Lay Stone Block"),
	PlaceBeam				UMETA(DisplayName = "Place Beam"),
	BuildWall				UMETA(DisplayName = "Build Wall Section"),
	BuildRoof				UMETA(DisplayName = "Build Roof Section"),

	// --- COMBAT ---
	MeleeAttack				UMETA(DisplayName = "Melee Attack"),
	RangedAttack			UMETA(DisplayName = "Ranged Attack"),
	CastSpell				UMETA(DisplayName = "Cast Spell"),
	Block					UMETA(DisplayName = "Block"),
	Dodge					UMETA(DisplayName = "Dodge"),
	UseConsumable			UMETA(DisplayName = "Use Consumable"),

	// --- SOCIAL ---
	Speak					UMETA(DisplayName = "Speak"),
	Trade					UMETA(DisplayName = "Trade"),
	PartyInteract			UMETA(DisplayName = "Party Interaction"),

	// --- LIFE ---
	Eat						UMETA(DisplayName = "Eat"),
	Drink					UMETA(DisplayName = "Drink"),
	Sleep					UMETA(DisplayName = "Sleep"),
	Rest					UMETA(DisplayName = "Rest"),
	SitDown					UMETA(DisplayName = "Sit Down"),
	StandUp					UMETA(DisplayName = "Stand Up"),

	// --- MISC ---
	Idle					UMETA(DisplayName = "Idle"),
	LookAround				UMETA(DisplayName = "Look Around"),
	CheckInventory			UMETA(DisplayName = "Check Inventory"),
	Emote					UMETA(DisplayName = "Emote"),
	Loot					UMETA(DisplayName = "Loot Corpse/Container"),

	MAX						UMETA(Hidden)
};

/**
 * The current phase of an action's lifecycle. Actions go through:
 * Pending → Transitioning → Executing → Verifying → Completed/Failed
 */
UENUM(BlueprintType)
enum class EActionPhase : uint8
{
	Pending			UMETA(DisplayName = "Pending"),
	Transitioning	UMETA(DisplayName = "Transitioning"),
	Executing		UMETA(DisplayName = "Executing"),
	Verifying		UMETA(DisplayName = "Verifying"),
	Completed		UMETA(DisplayName = "Completed"),
	Failed			UMETA(DisplayName = "Failed"),
	Cancelled		UMETA(DisplayName = "Cancelled"),
};

/**
 * What time-block of the NPC's daily schedule we're in.
 */
UENUM(BlueprintType)
enum class EDailyScheduleBlock : uint8
{
	Sleeping		UMETA(DisplayName = "Sleeping"),
	WakingUp		UMETA(DisplayName = "Waking Up"),
	Breakfast		UMETA(DisplayName = "Breakfast"),
	MorningWork		UMETA(DisplayName = "Morning Work"),
	Lunch			UMETA(DisplayName = "Lunch"),
	AfternoonWork	UMETA(DisplayName = "Afternoon Work"),
	Dinner			UMETA(DisplayName = "Dinner"),
	EveningSocial	UMETA(DisplayName = "Evening Social"),
	WindingDown		UMETA(DisplayName = "Winding Down"),
};

/**
 * Result of a completion verification check.
 */
UENUM(BlueprintType)
enum class EVerificationResult : uint8
{
	Success,
	Failed,
	Inconclusive,
	NodeDepleted,
	TargetLost,
	InventoryFull,
};

/**
 * Reason a replan was requested.
 */
UENUM(BlueprintType)
enum class EReplanReason : uint8
{
	ActionFailed,
	ActionTimeout,
	StuckDetected,
	CombatInterrupt,
	NeedCritical,
	GoalCompleted,
	ScheduleChange,
	ExternalRequest,
	TargetBlacklisted,
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * Timing constraints for each action type. Built from the realistic time scales.
 * MinDuration prevents instant completion; MaxDuration prevents infinite hangs.
 */
USTRUCT(BlueprintType)
struct FAoCActionTimingData
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MinDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxDuration = 10.0f;

	/** If true, this action's duration is randomized between min and max each time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bRandomizeDuration = false;

	/** Skill level scaling factor. Higher skill = faster (multiplied by (1 - SkillFactor * SkillLevel/100)) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SkillSpeedFactor = 0.0f;
};

/**
 * A single action currently in-flight or queued.
 */
USTRUCT(BlueprintType)
struct FNPCActiveAction
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ENPCActionType ActionType = ENPCActionType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EActionPhase Phase = EActionPhase::Pending;

	/** World time when this action began executing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float StartTime = 0.0f;

	/** Computed duration for this specific instance (skill-adjusted, randomized) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ExpectedDuration = 0.0f;

	/** Maximum allowed time before we force-fail this action */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TimeoutDuration = 0.0f;

	/** Target actor or location for this action (e.g., ore node, crafting station, enemy) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString TargetId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	/** How many times we've retried this specific action instance */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RetryCount = 0;

	/** Snapshot of relevant inventory counts before this action started (for verification) */
	UPROPERTY()
	TMap<FString, int32> PreActionInventory;

	/** The transition duration before this action starts (idle blend) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TransitionDuration = 0.0f;
};

/**
 * A blacklisted target — something that failed multiple times and should be avoided.
 */
USTRUCT(BlueprintType)
struct FBlacklistedTarget
{
	GENERATED_BODY()

	UPROPERTY()
	FString TargetId;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float BlacklistExpireTime = 0.0f;

	UPROPERTY()
	FString Reason;
};

/**
 * Long-term goal tracking — e.g., "Craft Iron Plate Armor Set". The NPC persists
 * toward this goal across many individual actions (mine ore, smelt, craft pieces).
 */
USTRUCT(BlueprintType)
struct FNPCLongTermGoal
{
	GENERATED_BODY()

	/** Human-readable description: "Craft Iron Plate Armor Set" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString GoalDescription;

	/** Resources required to complete the goal: {"Iron Ore": 150, "Coal": 50} */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, int32> RequiredResources;

	/** Resources gathered/produced so far */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FString, int32> GatheredSoFar;

	/** Sub-items that must be crafted: {"Iron Bar": 50, "Iron Chestplate": 1} */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FString, int32> RequiredCraftedItems;

	/** Sub-items crafted so far */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FString, int32> CraftedSoFar;

	/** Steps completed in natural language for debug/speech */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FString> StepsCompleted;

	/** Current step description */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString CurrentStep;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float StartTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float EstimatedCompletionTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CompletionPercent = 0.0f;

	/** Priority — higher = more important. NPCs attend to highest-priority active goal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority = 50;
};

/**
 * Position sample for stuck detection.
 */
USTRUCT()
struct FPositionSample
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	float Timestamp = 0.0f;
};

/**
 * Daily schedule entry — one block of time in the NPC's day.
 */
USTRUCT(BlueprintType)
struct FScheduleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EDailyScheduleBlock Block = EDailyScheduleBlock::Sleeping;

	/** Hour of day (0-24) when this block starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StartHour = 0.0f;

	/** Hour of day when this block ends */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EndHour = 0.0f;

	/** Location tag — where the NPC should be during this block ("home", "mine", "tavern", "market") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LocationTag;

	/** What activity the NPC prefers during this block */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ENPCActionType PreferredActivity = ENPCActionType::Idle;
};

/**
 * Stats tracking for the task governor's diagnostic output.
 */
USTRUCT(BlueprintType)
struct FTaskGovernorStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalActionsAttempted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalActionsCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalActionsFailed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalRetries = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalStuckDetections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalReplans = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float AverageActionDuration = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalBlacklists = 0;
};

// ============================================================================
// MAIN CLASS
// ============================================================================

/**
 * UAoCNPCTaskGovernor — The safety, timing, and realism enforcement layer.
 *
 * Sits between the GOAP planner and raw action execution. Every NPC action passes
 * through the Task Governor, which enforces:
 * - Realistic durations (mining takes 3.5s/swing, crafting a chestplate takes 4-6 min)
 * - Action transition blending (1-2s idle between tasks)
 * - Completion verification (did the ore count go up after mining?)
 * - Retry logic with blacklisting
 * - Stuck detection and recovery
 * - Replan cooldowns to prevent oscillation
 * - Long-term goal tracking across many actions
 * - Day/night schedules with personality variance
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCTaskGovernor : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCTaskGovernor();

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Tasks")
	FString GetLastFailReason() const { return LastFailReason; }

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -----------------------------------------------------------
	// ACTION MANAGEMENT
	// -----------------------------------------------------------

	/**
	 * Request a new action to be executed. The governor will check timing, blacklists,
	 * and current state before allowing it. Returns false if the action is denied.
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	bool RequestAction(ENPCActionType ActionType, const FString& TargetId, FVector TargetLocation);

	/** Force-cancel the current action (used for combat interrupts) */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	void CancelCurrentAction(EReplanReason Reason);

	/** Get the current action in progress */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	const FNPCActiveAction& GetCurrentAction() const { return CurrentAction; }

	/** Is the NPC currently executing an action? */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	bool IsActionInProgress() const;

	/** Can the NPC accept a new action right now? */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	bool CanAcceptNewAction() const;

	/** Is this specific target currently blacklisted? */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	bool IsTargetBlacklisted(const FString& TargetId) const;

	// -----------------------------------------------------------
	// REPLAN CONTROL
	// -----------------------------------------------------------

	/** Request a replan from the GOAP planner. Enforces cooldown unless bypass. */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	bool RequestReplan(EReplanReason Reason);

	/** Check if a replan is allowed right now (cooldown check) */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	bool CanReplan() const;

	// -----------------------------------------------------------
	// LONG-TERM GOALS
	// -----------------------------------------------------------

	/** Set a long-term goal (e.g., "Craft Iron Plate Armor Set") */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	void SetLongTermGoal(const FNPCLongTermGoal& Goal);

	/** Update progress on the active long-term goal */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	void UpdateGoalProgress(const FString& ResourceName, int32 AmountGathered);

	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	void UpdateGoalCraftProgress(const FString& ItemName, int32 AmountCrafted);

	/** Get the active long-term goal */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	const FNPCLongTermGoal& GetActiveLongTermGoal() const { return ActiveGoal; }

	/** Does the NPC have all resources needed for the current goal step? */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	bool HasResourcesForCurrentStep() const;

	/** Get the next action the NPC should do to advance the long-term goal */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	ENPCActionType GetNextGoalAction() const;

	// -----------------------------------------------------------
	// SCHEDULE
	// -----------------------------------------------------------

	/** Get the current schedule block based on in-game time */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	EDailyScheduleBlock GetCurrentScheduleBlock() const;

	/** Get what the NPC should be doing according to their schedule */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	ENPCActionType GetScheduledActivity() const;

	/** Get where the NPC should be according to their schedule */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	FString GetScheduledLocation() const;

	/** Should the NPC be sleeping right now? */
	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	bool ShouldBeSleeping() const;

	/** Initialize the schedule based on NPC personality traits */
	UFUNCTION(BlueprintCallable, Category = "AoC|TaskGovernor")
	void GenerateScheduleFromPersonality();

	// -----------------------------------------------------------
	// DIAGNOSTICS
	// -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	const FTaskGovernorStats& GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "AoC|TaskGovernor")
	FString GetDebugString() const;

protected:
	FString LastFailReason;

	// -----------------------------------------------------------
	// INTERNAL TICK PHASES
	// -----------------------------------------------------------

	void TickActiveAction(float DeltaTime);
	void TickTransition(float DeltaTime);
	void TickStuckDetection(float DeltaTime);
	void TickBlacklistExpiry(float DeltaTime);
	void TickSchedule(float DeltaTime);
	void TickGoalProgress(float DeltaTime);

	// -----------------------------------------------------------
	// TIMING
	// -----------------------------------------------------------

	/** Build the static timing table for all action types */
	void InitializeTimingTable();

	/** Get the computed duration for an action, adjusted for skill level */
	float ComputeActionDuration(ENPCActionType ActionType) const;

	/** Get the maximum timeout for an action */
	float GetActionTimeout(ENPCActionType ActionType) const;

	/** Get the transition/blend time before the next action */
	float GetTransitionDuration(ENPCActionType FromAction, ENPCActionType ToAction) const;

	// -----------------------------------------------------------
	// VERIFICATION
	// -----------------------------------------------------------

	/** Verify whether the current action actually succeeded */
	EVerificationResult VerifyActionCompletion();

	/** Take a snapshot of inventory state before an action for comparison */
	void SnapshotPreActionState();

	// -----------------------------------------------------------
	// RETRY & BLACKLIST
	// -----------------------------------------------------------

	/** Attempt to retry the failed action with variation */
	bool AttemptRetry();

	/** Blacklist a target for a duration */
	void BlacklistTarget(const FString& TargetId, FVector Location, float Duration, const FString& Reason);

	// -----------------------------------------------------------
	// STUCK DETECTION
	// -----------------------------------------------------------

	/** Record current position for stuck detection */
	void RecordPositionSample();

	/** Check if the NPC appears stuck (hasn't moved during a MoveTo) */
	bool IsStuck() const;

	/** Attempt recovery from stuck state */
	void AttemptStuckRecovery();

	/** Is any player close enough to see a teleport? */
	bool IsAnyPlayerNearby(float Radius) const;

	// -----------------------------------------------------------
	// DATA
	// -----------------------------------------------------------

	UPROPERTY()
	FNPCActiveAction CurrentAction;

	UPROPERTY()
	ENPCActionType PreviousActionType = ENPCActionType::None;

	/** Static timing data per action type */
	UPROPERTY()
	TMap<ENPCActionType, FAoCActionTimingData> TimingTable;

	/** Blacklisted targets */
	UPROPERTY()
	TArray<FBlacklistedTarget> BlacklistedTargets;

	/** Position samples for stuck detection (circular buffer, last ~10 samples) */
	UPROPERTY()
	TArray<FPositionSample> PositionHistory;

	UPROPERTY()
	int32 PositionHistoryIndex = 0;

	/** Time of last replan request (for cooldown enforcement) */
	UPROPERTY()
	float LastReplanTime = -100.0f;

	/** Active long-term goal */
	UPROPERTY()
	FNPCLongTermGoal ActiveGoal;

	/** This NPC's daily schedule (generated from personality) */
	UPROPERTY()
	TArray<FScheduleEntry> DailySchedule;

	/** Current schedule block cache */
	UPROPERTY()
	EDailyScheduleBlock CachedScheduleBlock = EDailyScheduleBlock::Sleeping;

	UPROPERTY()
	float ScheduleCheckTimer = 0.0f;

	/** Stuck recovery attempts for the current action */
	UPROPERTY()
	int32 StuckRecoveryAttempts = 0;

	UPROPERTY()
	float LastPositionSampleTime = 0.0f;

	/** Accumulator for transition blending */
	UPROPERTY()
	float TransitionTimer = 0.0f;

	/** Stats */
	UPROPERTY()
	FTaskGovernorStats Stats;

	// -----------------------------------------------------------
	// CONFIGURATION
	// -----------------------------------------------------------

	/** Minimum seconds between replans (prevents GOAP oscillation) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float ReplanCooldown = 5.0f;

	/** Max retries before blacklisting a target */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	int32 MaxRetries = 3;

	/** How long a blacklisted target stays blacklisted (seconds) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float BlacklistDuration = 300.0f;

	/** Position sample interval for stuck detection */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float PositionSampleInterval = 2.0f;

	/** Distance threshold for "hasn't moved" in stuck detection (cm) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float StuckDistanceThreshold = 50.0f;

	/** Time without movement before declaring stuck (seconds) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float StuckTimeThreshold = 8.0f;

	/** Max stuck recovery attempts before emergency teleport */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	int32 MaxStuckRecoveryAttempts = 2;

	/** Radius to check for nearby players before teleport-unstuck */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float PlayerProximityCheckRadius = 5000.0f;

	/** Max position samples to keep */
	static constexpr int32 MaxPositionSamples = 10;

	// -----------------------------------------------------------
	// CACHED REFERENCES
	// -----------------------------------------------------------

	UPROPERTY()
	TObjectPtr<AAoCNPCCharacter> OwnerNPC = nullptr;

	UPROPERTY()
	TObjectPtr<UAoCNPCBrainV2> Brain = nullptr;

	UPROPERTY()
	TObjectPtr<UAoCNPCSkillSystem> SkillSystem = nullptr;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Tasks")
public:
	int32 GetAbandonedTaskCount() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Tasks")
	int32 GetCompletedTaskCount() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Tasks")
	int32 GetTaskAttemptCount(const FString& TaskType) const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Tasks")
	int32 GetTaskSuccessCount(const FString& TaskType) const;

};
