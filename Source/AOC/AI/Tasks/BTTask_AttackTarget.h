// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_AttackTarget.generated.h"

/**
 * UBTTask_AttackTarget
 * Checks range to the TargetActor, rotates toward it, and triggers an attack.
 * Returns InProgress during the attack animation, then Succeeded.
 */
UCLASS()
class AOC_API UBTTask_AttackTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_AttackTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	/** Blackboard key for the target actor. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI")
	FBlackboardKeySelector TargetActorKey;

	/** Melee attack range. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI", meta = (ClampMin = "50.0"))
	float AttackRange;

	/** Duration of the attack animation in seconds. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI", meta = (ClampMin = "0.1"))
	float AttackDuration;

private:
	float AttackTimer;
	bool bIsAttacking;
};
