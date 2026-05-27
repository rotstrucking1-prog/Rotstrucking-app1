// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "AoCAIController.generated.h"

class UBehaviorTree;
class UBlackboardData;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

/**
 * AAoCAIController
 * AI controller for enemy characters with behavior tree and perception.
 */
UCLASS()
class AOC_API AAoCAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAoCAIController();

	// ── Behavior Tree ──

	/** Default behavior tree to run. Can be overridden per-enemy in Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|AI")
	TObjectPtr<UBehaviorTree> DefaultBehaviorTree;

	/** Default blackboard data asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|AI")
	TObjectPtr<UBlackboardData> DefaultBlackboardData;

	/** Set and run a behavior tree at runtime. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI")
	void SetBehaviorTree(UBehaviorTree* NewTree);

	// ── Perception ──

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;

	/** Sight sense configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Hearing sense configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|AI|Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	// ── Blackboard Key Names ──

	static const FName BB_TargetActor;
	static const FName BB_HomeLocation;
	static const FName BB_PatrolIndex;
	static const FName BB_CombatState;
	static const FName BB_AggroRange;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Called when a perceived actor updates (seen or lost). */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
