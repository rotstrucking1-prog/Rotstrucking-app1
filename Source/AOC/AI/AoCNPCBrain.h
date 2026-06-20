// AoCNPCBrain.h — Goal-based NPC AI component
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCBrain.generated.h"

class UAoCAnimationLab;
class UAoCSmartAnimPlayer;

/** NPC behavioral goal states */
UENUM(BlueprintType)
enum class EAoCNPCGoal : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Patrol      UMETA(DisplayName = "Patrol"),
	Investigate UMETA(DisplayName = "Investigate"),
	Alert       UMETA(DisplayName = "Alert"),
	Combat      UMETA(DisplayName = "Combat"),
	Flee        UMETA(DisplayName = "Flee"),
	Hide        UMETA(DisplayName = "Hide"),
	Social      UMETA(DisplayName = "Social"),
	Dead        UMETA(DisplayName = "Dead")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoalChanged, EAoCNPCGoal, OldGoal, EAoCNPCGoal, NewGoal);

/**
 * UAoCNPCBrain
 *
 * Goal-based AI component that drives NPC behavior.
 * Uses the Animation Laboratory to find contextual animations rather than
 * hard-coding any animation references.  Attach to an AoCNPCCharacter.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCBrain : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCBrain();

	// --- Configuration ---------------------------------------------------------

	/** Maximum sight range in Unreal units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float SightRadius = 2000.f;

	/** Maximum hearing range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float HearingRadius = 800.f;

	/** Range at which NPC becomes alert but not yet hostile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float AlertRadius = 1500.f;

	/** Threat level above which NPC enters Combat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float HighThreatThreshold = 70.f;

	/** Threat level above which NPC becomes Alert */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float MediumThreatThreshold = 30.f;

	/** Health percentage below which NPC flees (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float FleeHealthPercent = 0.2f;

	/** Weapon type tags for animation queries (e.g. "sword", "bow", "unarmed") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<FString> WeaponTags;

	/** Points the NPC patrols between (world-space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	TArray<FVector> PatrolPoints;

	// --- Runtime state (read-only from Blueprint) ------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	EAoCNPCGoal CurrentGoal = EAoCNPCGoal::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	float ThreatLevel = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	float DistanceToPlayer = 99999.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	TArray<TObjectPtr<AActor>> DetectedActors;

	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	TObjectPtr<AActor> PlayerRef;

	// --- Delegates -------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AI")
	FOnGoalChanged OnGoalChanged;

	// --- Public API ------------------------------------------------------------

	/** Force the brain into a specific goal (e.g. from TakeDamage) */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void ForceGoal(EAoCNPCGoal NewGoal);

	/** Inform the brain that we were hit from a given direction */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void NotifyHit(const FVector& HitDirection, float Damage);

	/** Inform the brain of a noise at a location */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void NotifyNoise(const FVector& NoiseLocation, float Loudness);

	/** Set the external health ratio (0-1) so the brain can make flee decisions */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetHealthRatio(float Ratio) { HealthRatio = Ratio; }

	/** Whether the NPC is considered dead by the brain */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsDead() const { return CurrentGoal == EAoCNPCGoal::Dead; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// --- Cached references -----------------------------------------------------

	UPROPERTY()
	TObjectPtr<UAoCAnimationLab> AnimLab;

	UPROPERTY()
	TObjectPtr<UAoCSmartAnimPlayer> AnimPlayer;

	// --- Internal state --------------------------------------------------------

	float HealthRatio = 1.f;
	int32 CurrentPatrolIndex = 0;
	FVector InvestigateLocation = FVector::ZeroVector;
	bool bHasInvestigateTarget = false;
	float IdleWaitRemaining = 0.f;
	float PatrolWaitRemaining = 0.f;
	FName LastAttackName = NAME_None;
	int32 AttackVarietyCounter = 0;

	// --- Timers ----------------------------------------------------------------

	FTimerHandle PerceptionTimerHandle;
	FTimerHandle GoalSelectionTimerHandle;
	FTimerHandle GoalExecutionTimerHandle;

	// --- Perception ------------------------------------------------------------

	void RunPerception();
	float CalculateThreatLevel() const;

	// --- Goal selection & execution --------------------------------------------

	void SelectGoal();
	void ExecuteGoal();

	void SetGoal(EAoCNPCGoal NewGoal);

	// --- Goal-specific execution -----------------------------------------------

	void ExecuteIdle();
	void ExecutePatrol();
	void ExecuteInvestigate();
	void ExecuteAlert();
	void ExecuteCombat();
	void ExecuteFlee();
	void ExecuteHide();
	void ExecuteSocial();
	void ExecuteDead();

	// --- Helpers ---------------------------------------------------------------

	/** Query the AnimLab for an animation and play it */
	void PlayFromLab(const FString& Category, const FString& BodyState, const TArray<FString>& Tags);

	/** Move toward a world location using simple movement */
	void MoveToward(const FVector& Location, float Speed);

	/** Get normalized direction from owner to a target */
	FVector GetDirectionTo(const AActor* Target) const;
};
