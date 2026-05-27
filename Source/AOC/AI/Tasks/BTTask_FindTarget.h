// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindTarget.generated.h"

/**
 * UBTTask_FindTarget
 * Uses AIPerception to find the nearest hostile actor within aggro range.
 * Sets the TargetActor blackboard key on success.
 */
UCLASS()
class ARCHITECTOFCREATION_API UBTTask_FindTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	/** Blackboard key for the target actor. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI")
	FBlackboardKeySelector TargetActorKey;

	/** Blackboard key for the aggro range. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI")
	FBlackboardKeySelector AggroRangeKey;
};
