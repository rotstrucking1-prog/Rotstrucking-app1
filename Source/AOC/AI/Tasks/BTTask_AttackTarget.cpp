// Copyright Architect of Creation. All Rights Reserved.

#include "BTTask_AttackTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"

UBTTask_AttackTarget::UBTTask_AttackTarget()
{
	NodeName = TEXT("Attack Target");
	bNotifyTick = true;
	AttackRange = 200.0f;
	AttackDuration = 1.2f;
	AttackTimer = 0.0f;
	bIsAttacking = false;
}

EBTNodeResult::Type UBTTask_AttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return EBTNodeResult::Failed;
	}

	// Check range
	float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (Distance > AttackRange)
	{
		return EBTNodeResult::Failed;
	}

	// Rotate toward target
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
		OwnerPawn->GetActorLocation(),
		TargetActor->GetActorLocation()
	);
	OwnerPawn->SetActorRotation(FRotator(0.0f, LookAtRotation.Yaw, 0.0f));

	// Trigger attack ability via GAS
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerPawn);
	if (ASI)
	{
		UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
		if (ASC)
		{
			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack")));
			ASC->TryActivateAbilitiesByTag(TagContainer);
		}
	}

	// Start attack timer
	AttackTimer = 0.0f;
	bIsAttacking = true;

	return EBTNodeResult::InProgress;
}

void UBTTask_AttackTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!bIsAttacking)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AttackTimer += DeltaSeconds;

	if (AttackTimer >= AttackDuration)
	{
		bIsAttacking = false;
		AttackTimer = 0.0f;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_AttackTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	bIsAttacking = false;
	AttackTimer = 0.0f;
	return EBTNodeResult::Aborted;
}

FString UBTTask_AttackTarget::GetStaticDescription() const
{
	return FString::Printf(TEXT("Attack target within range %.0f (duration %.1fs)"), AttackRange, AttackDuration);
}
