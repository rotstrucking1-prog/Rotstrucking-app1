// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Patrol.generated.h"

/**
 * UBTTask_Patrol
 * Moves the AI pawn to the next patrol point in sequence.
 * Reads PatrolIndex from the blackboard and increments it upon arrival.
 */
UCLASS()
class AOC_API UBTTask_Patrol : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Patrol();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	/** Patrol waypoints in world space. Can also be populated from the owning character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Patrol")
	TArray<FVector> PatrolPoints;

	/** Blackboard key for the current patrol index. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI|Patrol")
	FBlackboardKeySelector PatrolIndexKey;

	/** Acceptable distance to the waypoint to consider arrival. */
	UPROPERTY(EditAnywhere, Category = "AoC|AI|Patrol", meta = (ClampMin = "10.0"))
	float AcceptanceRadius;
};
