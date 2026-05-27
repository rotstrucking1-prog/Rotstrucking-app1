// AoCNPCImperfection.h
// Architect of Creation — NPC Human Imperfection Layer
//
// The critical "uncanny valley breaker" system. Without imperfections, NPCs move
// in perfectly straight lines, react instantly, never miss, and never make mistakes.
// Humans notice this within seconds. This system adds realistic human flaws:
//
// - REACTION DELAY: 150-500ms base, modified by alertness, fatigue, surprise
// - AIM VARIANCE: Ranged attacks miss based on skill, distance, stress, movement
// - DECISION MISTAKES: 3-8% chance of suboptimal choices (wrong ability, walk past node)
// - MOVEMENT IMPERFECTION: Path wander, speed variance, overshooting, random stops
// - IDLE BEHAVIORS: Looking around, shifting weight, random emotes, "checking inventory"
// - ATTENTION SPAN: Field of view cone, audio detection ranges, distraction narrowing
//
// These imperfections make NPCs pass the "10-15 minute observation test" — a real
// player watching an NPC for 15 minutes should not be able to definitively conclude
// they're an NPC based on behavior alone.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCImperfection.generated.h"

// Forward declarations
class AAoCNPCCharacter;
class UAoCNPCBrainV2;
class UAoCNPCSkillSystem;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * The NPC's current alertness state, affecting reaction times and attention span.
 */
UENUM(BlueprintType)
enum class ENPCAlertState : uint8
{
	Sleeping		UMETA(DisplayName = "Sleeping"),         // 2000ms reaction, 0° FOV
	Relaxed			UMETA(DisplayName = "Relaxed"),          // 400ms reaction, 120° FOV
	Idle			UMETA(DisplayName = "Idle"),              // 350ms reaction, 120° FOV
	Alert			UMETA(DisplayName = "Alert"),            // 200ms reaction, 160° FOV
	Combat			UMETA(DisplayName = "In Combat"),        // 150ms reaction, 160° FOV
	Focused			UMETA(DisplayName = "Focused on Task"),  // 300ms reaction, 90° FOV (crafting, etc.)
	Panicked		UMETA(DisplayName = "Panicked"),         // 100ms reaction, 180° FOV but decisions are worse
};

/**
 * Types of decision mistakes an NPC can make.
 */
UENUM(BlueprintType)
enum class EDecisionMistake : uint8
{
	None,
	SuboptimalAbility,     // Used a weaker ability when a better one was available
	ForgotToEat,           // Ignored moderate hunger
	MissedResource,        // Walked past a visible resource node
	WrongCraftItem,        // Started crafting the wrong item (corrects after 2s)
	OverreactedToDanger,   // Fled from a non-dangerous entity
	TookWrongPath,         // Chose a longer path to destination
	FumbledItem,           // Took slightly longer to use an item
	MiscountedResources,   // Thought they had enough resources but didn't
};

/**
 * Types of idle behavior the NPC can exhibit.
 */
UENUM(BlueprintType)
enum class EIdleBehavior : uint8
{
	None,
	LookAround,           // Random head turns
	ShiftWeight,           // Idle animation variation
	Scratch,               // Play scratch/stretch emote
	Stretch,               // Play stretch emote
	CheckInventory,        // Stand still 2-5 seconds (simulating UI)
	WanderShort,           // Walk 2-5m and come back
	SitDown,               // Sit on ground or nearby seat
	LeanOnWall,            // Lean against nearby surface
	KickGround,            // Bored kicking animation
	CrossArms,             // Stand with arms crossed
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * Configuration for reaction delay calculations.
 */
USTRUCT(BlueprintType)
struct FReactionDelayConfig
{
	GENERATED_BODY()

	/** Base reaction time in seconds when in each alert state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SleepingReaction = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RelaxedReaction = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float IdleReaction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AlertReaction = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CombatReaction = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FocusedReaction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PanickedReaction = 0.1f;

	/** Additional delay when attacked from behind (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SurpriseAdditional = 0.2f;

	/** Additional delay when fatigued (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FatigueAdditional = 0.1f;

	/** Agility attribute reduces reaction time by this fraction per point (0-100) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AgilityReductionFactor = 0.002f;
};

/**
 * Configuration for aim/accuracy variance.
 */
USTRUCT(BlueprintType)
struct FAimVarianceConfig
{
	GENERATED_BODY()

	/** Base aim spread angle (degrees) at skill level 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseSpreadDegrees = 8.0f;

	/** Minimum aim spread (even at max skill, some variance remains) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinSpreadDegrees = 1.0f;

	/** How much distance degrades accuracy (spread multiplier per 100 units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DistanceSpreadPerHundred = 0.5f;

	/** Additional spread when target is moving (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MovingTargetAdditionalSpread = 3.0f;

	/** Spread multiplier when under stress (low HP) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StressSpreadMultiplier = 1.5f;

	/** HP threshold below which stress spread activates (0-1 fraction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StressHPThreshold = 0.3f;

	/** Dexterity reduces spread: spread * (1 - Dex * factor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DexterityAccuracyFactor = 0.005f;
};

/**
 * Configuration for movement imperfections.
 */
USTRUCT(BlueprintType)
struct FMovementImperfectionConfig
{
	GENERATED_BODY()

	/** Maximum random lateral wander from path (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PathWanderMax = 30.0f;

	/** How often path wander is applied (seconds between wander adjustments) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PathWanderInterval = 2.0f;

	/** Walking speed variance (fraction, e.g., 0.05 = ±5%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedVarianceFraction = 0.05f;

	/** How often speed variance is recalculated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpeedVarianceInterval = 3.0f;

	/** Chance per second of a random brief stop (0.02 = 2% per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RandomStopChance = 0.02f;

	/** Duration of random stops (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RandomStopMinDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RandomStopMaxDuration = 3.0f;

	/** Destination overshoot distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OvershootDistance = 40.0f;

	/** Chance of overshooting a destination (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OvershootChance = 0.15f;
};

/**
 * Attention/perception configuration.
 */
USTRUCT(BlueprintType)
struct FAttentionConfig
{
	GENERATED_BODY()

	/** Field of view half-angle when in each state (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RelaxedFOV = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AlertFOV = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FocusedFOV = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PanickedFOV = 90.0f;

	/** Audio detection ranges (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CombatNoiseRange = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MiningNoiseRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FootstepRange = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WhisperRange = 500.0f;

	/** Wisdom attribute improves perception: detection range * (1 + Wis * factor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WisdomPerceptionFactor = 0.005f;
};

/**
 * Idle behavior scheduling — tracks when the next idle behavior should fire.
 */
USTRUCT()
struct FIdleBehaviorState
{
	GENERATED_BODY()

	UPROPERTY()
	float TimeUntilNextIdleBehavior = 0.0f;

	UPROPERTY()
	EIdleBehavior CurrentIdleBehavior = EIdleBehavior::None;

	UPROPERTY()
	float CurrentIdleDuration = 0.0f;

	UPROPERTY()
	float IdleElapsed = 0.0f;

	UPROPERTY()
	bool bIsPerformingIdleBehavior = false;
};

/**
 * Movement imperfection runtime state.
 */
USTRUCT()
struct FMovementImperfectionState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector CurrentWanderOffset = FVector::ZeroVector;

	UPROPERTY()
	float WanderTimer = 0.0f;

	UPROPERTY()
	float CurrentSpeedMultiplier = 1.0f;

	UPROPERTY()
	float SpeedVarianceTimer = 0.0f;

	UPROPERTY()
	bool bIsRandomlyStopped = false;

	UPROPERTY()
	float RandomStopTimer = 0.0f;

	UPROPERTY()
	float RandomStopDuration = 0.0f;

	/** Random head-look target rotation for natural "looking around" */
	UPROPERTY()
	FRotator HeadLookTarget = FRotator::ZeroRotator;

	UPROPERTY()
	float HeadLookTimer = 0.0f;

	UPROPERTY()
	float HeadLookChangeInterval = 4.0f;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDecisionMistake, EDecisionMistake, MistakeType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIdleBehaviorStart, EIdleBehavior, Behavior);

// ============================================================================
// MAIN CLASS
// ============================================================================

/**
 * UAoCNPCImperfection — The human imperfection layer.
 *
 * Adds realistic human flaws to NPC behavior: delayed reactions, missed shots,
 * occasional bad decisions, imperfect movement, and natural idle behaviors.
 * Without this system, NPCs are "too perfect" and immediately identifiable as AI.
 *
 * This component modifies the NPC's raw behavior output (from brain, combat, etc.)
 * by adding noise, delays, and mistakes that make the NPC appear human.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCImperfection : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCImperfection();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -----------------------------------------------------------
	// REACTION DELAY
	// -----------------------------------------------------------

	/**
	 * Get the current reaction delay for this NPC (seconds).
	 * Call this before executing any reactive action (dodge, block, respond to threat).
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	float GetReactionDelay() const;

	/**
	 * Get reaction delay with a surprise factor (e.g., attacked from behind).
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	float GetReactionDelayWithSurprise(bool bFromBehind, bool bFatigued) const;

	// -----------------------------------------------------------
	// AIM VARIANCE
	// -----------------------------------------------------------

	/**
	 * Apply aim variance to a target direction. Returns the actual aim direction
	 * with realistic spread applied.
	 *
	 * @param IdealDirection - Perfect direction to the target
	 * @param Distance - Distance to target (cm)
	 * @param bTargetMoving - Is the target moving?
	 * @param WeaponSkillLevel - Relevant weapon/magic skill level (0-100)
	 * @return Modified direction with human-like inaccuracy
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	FVector ApplyAimVariance(FVector IdealDirection, float Distance, bool bTargetMoving, float WeaponSkillLevel) const;

	/**
	 * Get the current aim spread angle (degrees) considering all factors.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	float GetCurrentAimSpread(float Distance, bool bTargetMoving, float WeaponSkillLevel) const;

	// -----------------------------------------------------------
	// DECISION MISTAKES
	// -----------------------------------------------------------

	/**
	 * Roll for a decision mistake. Call this before major NPC decisions.
	 * Returns the type of mistake (None if no mistake occurs).
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	EDecisionMistake RollForMistake() const;

	/**
	 * Should the NPC miss a visible resource node? (Lower Wisdom = more likely)
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool ShouldMissResourceNode() const;

	/**
	 * Should the NPC use a suboptimal ability in combat?
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool ShouldUseSuboptimalAbility() const;

	/**
	 * Should the NPC forget to eat when moderately hungry?
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool ShouldForgetToEat() const;

	// -----------------------------------------------------------
	// MOVEMENT IMPERFECTION
	// -----------------------------------------------------------

	/**
	 * Get the current path wander offset to apply to the NPC's movement.
	 * This is a small lateral offset that makes paths non-linear.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	FVector GetPathWanderOffset() const;

	/**
	 * Get the current speed multiplier (fluctuates ±5% for realism).
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	float GetSpeedMultiplier() const;

	/**
	 * Is the NPC currently in a random brief stop?
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool IsRandomlyStopped() const;

	/**
	 * Should the NPC overshoot this destination?
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	bool ShouldOvershootDestination() const;

	/**
	 * Get the overshoot offset if the NPC is overshooting.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	FVector GetOvershootOffset(FVector MoveDirection) const;

	/**
	 * Get the current head look-around target rotation for natural idle looking.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	FRotator GetHeadLookTarget() const;

	// -----------------------------------------------------------
	// IDLE BEHAVIORS
	// -----------------------------------------------------------

	/**
	 * Is the NPC currently performing an idle behavior?
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool IsPerformingIdleBehavior() const;

	/**
	 * Get the current idle behavior being performed.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	EIdleBehavior GetCurrentIdleBehavior() const;

	/**
	 * Force an idle behavior (e.g., when the NPC has nothing to do).
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	void ForceIdleBehavior(EIdleBehavior Behavior, float Duration);

	// -----------------------------------------------------------
	// ATTENTION / PERCEPTION
	// -----------------------------------------------------------

	/**
	 * Can the NPC detect something at the given location?
	 * Checks field of view and distance against the NPC's current attention state.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool CanDetectVisual(FVector TargetLocation) const;

	/**
	 * Can the NPC hear a sound at the given location with the given loudness?
	 * @param Loudness - 0.0 (whisper) to 1.0 (combat explosion)
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	bool CanDetectAudio(FVector SoundLocation, float Loudness) const;

	/**
	 * Get the NPC's current field of view half-angle (degrees).
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	float GetCurrentFOV() const;

	/**
	 * Get the NPC's current audio detection range for a given loudness.
	 */
	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	float GetAudioDetectionRange(float Loudness) const;

	// -----------------------------------------------------------
	// STATE MANAGEMENT
	// -----------------------------------------------------------

	/** Set the NPC's alert state (affects reaction times, FOV, etc.) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	void SetAlertState(ENPCAlertState NewState);

	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	ENPCAlertState GetAlertState() const { return CurrentAlertState; }

	/** Set current HP fraction (0-1) for stress calculations */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	void SetCurrentHPFraction(float HPFraction) { CurrentHPFraction = FMath::Clamp(HPFraction, 0.0f, 1.0f); }

	/** Set fatigue level (0-1) for reaction delay modifications */
	UFUNCTION(BlueprintCallable, Category = "AoC|Imperfection")
	void SetFatigueLevel(float Fatigue) { CurrentFatigue = FMath::Clamp(Fatigue, 0.0f, 1.0f); }

	// -----------------------------------------------------------
	// DIAGNOSTICS
	// -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "AoC|Imperfection")
	FString GetDebugString() const;

	// -----------------------------------------------------------
	// DELEGATES
	// -----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AoC|Imperfection")
	FOnDecisionMistake OnDecisionMistake;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Imperfection")
	FOnIdleBehaviorStart OnIdleBehaviorStart;

protected:

	// -----------------------------------------------------------
	// TICK PHASES
	// -----------------------------------------------------------

	void TickMovementImperfection(float DeltaTime);
	void TickIdleBehaviors(float DeltaTime);
	void TickHeadLook(float DeltaTime);

	// -----------------------------------------------------------
	// INTERNAL HELPERS
	// -----------------------------------------------------------

	/** Get attribute value from the NPC's skill system (Agility, Dexterity, Wisdom, etc.) */
	float GetAttribute(const FString& AttributeName) const;

	/** Pick a random idle behavior weighted by personality */
	EIdleBehavior PickRandomIdleBehavior() const;

	/** Get how long an idle behavior should last */
	float GetIdleBehaviorDuration(EIdleBehavior Behavior) const;

	/** Get the base mistake chance for this NPC (personality-dependent) */
	float GetBaseMistakeChance() const;

	// -----------------------------------------------------------
	// CONFIGURATION
	// -----------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	FReactionDelayConfig ReactionConfig;

	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	FAimVarianceConfig AimConfig;

	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	FMovementImperfectionConfig MovementConfig;

	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	FAttentionConfig AttentionConfig;

	/** Base mistake chance (0.03 - 0.08, personality-dependent) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config", meta = (ClampMin = "0.01", ClampMax = "0.15"))
	float BaseMistakeChance = 0.05f;

	/** Interval between idle behaviors when NPC is not actively doing anything (seconds) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float IdleBehaviorMinInterval = 30.0f;

	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float IdleBehaviorMaxInterval = 120.0f;

	// -----------------------------------------------------------
	// RUNTIME STATE
	// -----------------------------------------------------------

	UPROPERTY()
	ENPCAlertState CurrentAlertState = ENPCAlertState::Idle;

	UPROPERTY()
	float CurrentHPFraction = 1.0f;

	UPROPERTY()
	float CurrentFatigue = 0.0f;

	UPROPERTY()
	FMovementImperfectionState MovementState;

	UPROPERTY()
	FIdleBehaviorState IdleState;

	/** Total mistakes made (for stats/debugging) */
	UPROPERTY()
	int32 TotalMistakesMade = 0;

	/** Total idle behaviors performed */
	UPROPERTY()
	int32 TotalIdleBehaviors = 0;

	// -----------------------------------------------------------
	// CACHED REFERENCES
	// -----------------------------------------------------------

	UPROPERTY()
	TObjectPtr<AAoCNPCCharacter> OwnerNPC = nullptr;

	UPROPERTY()
	TObjectPtr<UAoCNPCSkillSystem> SkillSystem = nullptr;
};
