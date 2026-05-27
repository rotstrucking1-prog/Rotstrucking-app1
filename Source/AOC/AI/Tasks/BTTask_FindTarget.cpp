// Copyright Architect of Creation. All Rights Reserved.

#include "BTTask_FindTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "GameFramework/Character.h"

UBTTask_FindTarget::UBTTask_FindTarget()
{
	NodeName = TEXT("Find Target");
}

EBTNodeResult::Type UBTTask_FindTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	UAIPerceptionComponent* PerceptionComp = AIController->GetPerceptionComponent();
	if (!PerceptionComp)
	{
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return EBTNodeResult::Failed;
	}

	float AggroRange = BB->GetValueAsFloat(AggroRangeKey.SelectedKeyName);
	if (AggroRange <= 0.0f)
	{
		AggroRange = 1000.0f;
	}

	// Get all currently perceived actors
	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);

	AActor* BestTarget = nullptr;
	float BestDistSq = AggroRange * AggroRange;

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor || Actor == OwnerPawn)
		{
			continue;
		}

		// Only target player-controlled characters
		ACharacter* TargetChar = Cast<ACharacter>(Actor);
		if (!TargetChar || !TargetChar->IsPlayerControlled())
		{
			continue;
		}

		float DistSq = FVector::DistSquared(OwnerPawn->GetActorLocation(), Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestTarget = Actor;
		}
	}

	if (BestTarget)
	{
		BB->SetValueAsObject(TargetActorKey.SelectedKeyName, BestTarget);
		return EBTNodeResult::Succeeded;
	}

	BB->ClearValue(TargetActorKey.SelectedKeyName);
	return EBTNodeResult::Failed;
}

FString UBTTask_FindTarget::GetStaticDescription() const
{
	return TEXT("Find the nearest hostile player within AggroRange using AIPerception.");
}
