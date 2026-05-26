// AoCNPCCombatBrain.h
// NPC Combat AI — makes NPCs fight like real players
// Pattern recognition, adaptive rotations, weapon switching, group tactics
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCSkillSystem.h"
#include "AoCNPCCombatBrain.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCCombat, Log, All);

class AoCHumanoidNPCV2;
class UAoCNPCSkillSystem;
class UAoCNPCInventory;
class UAoCNPCPersonality;
class UAoCNPCMemory;

/** Combat phase state machine */
UENUM(BlueprintType)
enum class ECombatPhase : uint8
{
	None,
	Evaluating,    // Assessing target before engaging
	Opening,       // First strike — use best opener
	Engaging,      // Active combat — main rotation
	Pressing,      // Target is low, go aggressive
	Surviving,     // Own HP low, defensive mode
	Executing,     // Target nearly dead, finish them
	Disengaging,   // Rare — fleeing
	Looting        // Target dead, approach and loot corpse
};

/** Derived combat style from skills */
UENUM(BlueprintType)
enum class ECombatStyle : uint8
{
	MeleeBrawler,
	RangedKiter,
	Battlemage,
	PureMage,
	Tank,
	Assassin,
	Berserker,
	Unarmed
};

/** Recommended approach for a target */
UENUM(BlueprintType)
enum class ECombatApproach : uint8
{
	Aggressive,
	Cautious,
	Ambush,
	GroupUp,
	Avoid
};

/** Ability slot types in the rotation */
UENUM(BlueprintType)
enum class EAbilitySlot : uint8
{
	Opener,
	Core,
	Finisher,
	Defensive,
	Panic,
	Interrupt
};

/** A single combat ability the NPC can use */
USTRUCT(BlueprintType)
struct FCombatAbility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AbilityName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillID RequiredSkill = ESkillID::MAX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinSkillLevel = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BaseDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ManaCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StaminaCost = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CooldownDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Range = 200.f; // UE units

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilitySlot PreferredSlot = EAbilitySlot::Core;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsAoE = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsHeal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsInterrupt = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsRanged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequiresBow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsMagic = false;

	/** Current cooldown remaining */
	float CooldownRemaining = 0.f;

	bool IsReady() const { return CooldownRemaining <= 0.f; }
};

/** Pre-fight assessment of a target */
USTRUCT(BlueprintType)
struct FTargetAssessment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float EstimatedCombatPower = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ThreatLevel = 0.f; // 0-1

	UPROPERTY(BlueprintReadOnly)
	float ConfidenceToWin = 0.5f; // 0-1

	UPROPERTY(BlueprintReadOnly)
	ECombatApproach RecommendedApproach = ECombatApproach::Cautious;

	UPROPERTY(BlueprintReadOnly)
	TArray<FName> ObservedWeapons;

	UPROPERTY(BlueprintReadOnly)
	TArray<FName> ObservedSpells;
};

/** Tracked pattern for a specific actor */
USTRUCT(BlueprintType)
struct FPlayerPattern
{
	GENERATED_BODY()

	/** Last N observed actions */
	TArray<FName> RecentActions;

	/** Which direction they dodge most often: -1=left, 0=none, 1=right */
	float DodgeDirectionBias = 0.f;

	/** HP percentage at which they typically heal */
	float HealThreshold = 0.3f;

	/** Preferred combat range in UE units */
	float PreferredRange = 200.f;

	/** Average time between attacks (seconds) */
	float AttackTiming = 0.8f;

	/** Ratio of spell casts to total actions (0-1) */
	float SpellCastFrequency = 0.f;

	/** Number of total observations */
	int32 TotalObservations = 0;

	/** Does the player tend to kite? */
	bool bTendsToKite = false;

	/** Does the player play aggressively? */
	bool bIsAggressive = false;
};

/** The NPC's built combat rotation */
USTRUCT()
struct FCombatRotation
{
	GENERATED_BODY()

	UPROPERTY()
	FName Opener;

	UPROPERTY()
	TArray<FName> CoreRotation; // 3-5 abilities cycled

	UPROPERTY()
	FName Finisher;

	UPROPERTY()
	FName DefensiveAbility;

	UPROPERTY()
	FName PanicAbility;

	UPROPERTY()
	FName InterruptAbility;

	/** Current index in core rotation */
	int32 CoreIndex = 0;
};

/**
 * UAoCNPCCombatBrain — UActorComponent
 *
 * Deep combat AI that makes NPCs fight like real players.
 * Features: adaptive rotations, pattern recognition, weapon switching,
 * group tactics, consumable usage, and fight-to-death mentality.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCCombatBrain : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCCombatBrain();

	virtual void BeginPlay() override;

	// --- Core Combat Interface ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void EnterCombat(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void ExitCombat();

	/** Main combat update — called by Brain each tick when in combat */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void CombatTick(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	FTargetAssessment EvaluateTarget(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	FName SelectAction();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void ExecuteAttack(FName AbilityName);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void DodgeOrBlock();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void UseConsumable(FName ItemName);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void UpdatePatternRecognition(AActor* Target, FName ObservedAction);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void SwitchWeapon(ESkillID NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void OnTargetKilled(AActor* DeadTarget);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void OnDamageTaken(float Damage, AActor* Instigator);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	ECombatStyle GetCombatStyle() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void BuildRotation();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	bool ShouldFlee() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	bool IsInCombat() const { return bInCombat; }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	ECombatPhase GetCombatPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	// --- Group Combat ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void SetGroupFocusTarget(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Combat")
	void RequestHelp(float Radius);

	// --- Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float GlobalCooldownMin = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float GlobalCooldownMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float ReactionDelayMin = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float ReactionDelayMax = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float MeleeRange = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float RangedMaxRange = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float SpellMaxRange = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float HealthPotionBaseThreshold = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Combat")
	float ManaPotionThreshold = 0.20f;

	// --- State ---
	UPROPERTY(BlueprintReadOnly, Category = "AoC|NPC|Combat")
	float CurrentHP = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|NPC|Combat")
	float MaxHP = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|NPC|Combat")
	float CurrentMana = 50.f;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|NPC|Combat")
	float MaxMana = 50.f;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|NPC|Combat")
	float CurrentStamina = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|NPC|Combat")
	float MaxStamina = 100.f;

private:
	// --- References (cached on BeginPlay) ---
	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> OwnerNPC;

	UPROPERTY()
	UAoCNPCSkillSystem* SkillSystem = nullptr;

	UPROPERTY()
	UAoCNPCInventory* Inventory = nullptr;

	UPROPERTY()
	UAoCNPCPersonality* Personality = nullptr;

	UPROPERTY()
	UAoCNPCMemory* Memory = nullptr;

	// --- Combat State ---
	bool bInCombat = false;
	ECombatPhase CurrentPhase = ECombatPhase::None;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<AActor> GroupFocusTarget;

	float CombatTimer = 0.f;
	float GlobalCooldownTimer = 0.f;
	float ReactionDelayTimer = 0.f;
	float TimeSinceLastAction = 0.f;
	float TimeSinceCombatStart = 0.f;

	// --- Active weapon ---
	ESkillID ActiveWeaponSkill = ESkillID::Unarmed;
	bool bHasBow = false;
	bool bHasShield = false;

	// --- Ability Pool & Rotation ---
	TArray<FCombatAbility> KnownAbilities;
	FCombatRotation CurrentRotation;
	ECombatStyle CurrentStyle = ECombatStyle::Unarmed;

	// --- Pattern Recognition ---
	TMap<uint32, FPlayerPattern> TargetPatterns; // keyed by target UniqueID

	// --- Consumable tracking ---
	int32 HealthPotionsRemaining = 0;
	int32 ManaPotionsRemaining = 0;

	// --- Internal functions ---

	/** Update combat phase based on current situation */
	void UpdateCombatPhase();

	/** Get distance to current target */
	float GetDistanceToTarget() const;

	/** Move toward or away from target based on style */
	void ManageRange();

	/** Check and use consumables if needed */
	void CheckConsumables();

	/** Select the best opener for the current situation */
	FName SelectOpener() const;

	/** Select next core rotation ability */
	FName SelectCoreAbility();

	/** Select a finisher for low-HP target */
	FName SelectFinisher() const;

	/** Select defensive action */
	FName SelectDefensiveAction() const;

	/** Select panic action */
	FName SelectPanicAction() const;

	/** Try to interrupt enemy cast */
	bool TryInterrupt();

	/** Check if we should switch weapons mid-fight */
	bool ShouldSwitchWeapons() const;

	/** Determine combat style from current skills */
	ECombatStyle DetermineCombatStyle() const;

	/** Build ability pool from NPC's current skill levels */
	void BuildAbilityPool();

	/** Tick all ability cooldowns */
	void TickCooldowns(float DeltaTime);

	/** Get pattern data for a target */
	FPlayerPattern& GetOrCreatePattern(AActor* Target);

	/** Predict target's next action based on patterns */
	FName PredictTargetAction(AActor* Target) const;

	/** Get the HP% at which this NPC should use a health potion */
	float GetHealthPotionThreshold() const;

	/** Calculate reaction delay based on DEX attribute */
	float CalculateReactionDelay() const;

	/** Chance of making a mistake (fumble) based on skill level */
	float GetMistakeChance() const;

	/** Apply a random mistake — miss dodge window, slow reaction, etc. */
	bool RollForMistake();

	/** Count available potions in inventory */
	void RefreshConsumableCount();

	/** Get best available ability for a slot */
	FCombatAbility* GetBestAbilityForSlot(EAbilitySlot Slot);

	/** Check if an ability can be used right now */
	bool CanUseAbility(const FCombatAbility& Ability) const;

	/** Get the ability struct by name */
	FCombatAbility* FindAbility(FName Name);

	/** Flanking calculation */
	bool ShouldFlank() const;
	FVector GetFlankPosition() const;

	/** Random stream for combat variance */
	FRandomStream CombatRand;
};
