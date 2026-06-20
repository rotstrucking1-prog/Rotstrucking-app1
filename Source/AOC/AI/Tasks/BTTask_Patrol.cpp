// Copyright Architect of Creation. All Rights Reserved.

#include "BTTask_Patrol.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_Patrol::UBTTask_Patrol()
{
	NodeName = TEXT("Patrol");
	bNotifyTick = true;
	AcceptanceRadius = 100.0f;
}

EBTNodeResult::Type UBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	if (PatrolPoints.Num() == 0)
	{
		return EBTNodeResult::Failed;
	}

	int32 CurrentIndex = BB->GetValueAsInt(PatrolIndexKey.SelectedKeyName);
	CurrentIndex = CurrentIndex % PatrolPoints.Num();

	FVector Destination = PatrolPoints[CurrentIndex];

	EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(
		Destination,
		AcceptanceRadius,
		true,  // bStopOnOverlap
		true,  // bUsePathfinding
		false, // bProjectDestinationToNavigation
		true,  // bCanStrafe
		nullptr,
		true   // bAllowPartialPath
	);

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		return EBTNodeResult::Failed;
	}

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Already there — increment index
		int32 NextIndex = (CurrentIndex + 1) % PatrolPoints.Num();
		BB->SetValueAsInt(PatrolIndexKey.SelectedKeyName, NextIndex);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_Patrol::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// Check if we've reached the destination
	EPathFollowingStatus::Type Status = AIController->GetMoveStatus();

	if (Status == EPathFollowingStatus::Idle)
	{
		// Movement completed — increment patrol index
		int32 CurrentIndex = BB->GetValueAsInt(PatrolIndexKey.SelectedKeyName);
		int32 NextIndex = (CurrentIndex + 1) % FMath::Max(1, PatrolPoints.Num());
		BB->SetValueAsInt(PatrolIndexKey.SelectedKeyName, NextIndex);

		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_Patrol::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		AIController->StopMovement();
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_Patrol::GetStaticDescription() const
{
	return FString::Printf(TEXT("Patrol through %d waypoints (acceptance: %.0f)"), PatrolPoints.Num(), AcceptanceRadius);
}
