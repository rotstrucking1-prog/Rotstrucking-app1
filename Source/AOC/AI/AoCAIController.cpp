// Copyright Architect of Creation. All Rights Reserved.

#include "AoCAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "GameFramework/Character.h"

// Blackboard key name constants
const FName AAoCAIController::BB_TargetActor		= FName(TEXT("TargetActor"));
const FName AAoCAIController::BB_HomeLocation		= FName(TEXT("HomeLocation"));
const FName AAoCAIController::BB_PatrolIndex		= FName(TEXT("PatrolIndex"));
const FName AAoCAIController::BB_CombatState		= FName(TEXT("CombatState"));
const FName AAoCAIController::BB_AggroRange			= FName(TEXT("AggroRange"));

AAoCAIController::AAoCAIController()
{
	// Create perception component
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
	SetPerceptionComponent(*AIPerceptionComp);

	// Sight config
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 2000.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 500.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	AIPerceptionComp->ConfigureSense(*SightConfig);

	// Hearing config
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 800.0f;
	HearingConfig->SetMaxAge(3.0f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = false;
	AIPerceptionComp->ConfigureSense(*HearingConfig);

	AIPerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
}

void AAoCAIController::BeginPlay()
{
	Super::BeginPlay();

	// Bind perception delegate
	if (AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AAoCAIController::OnTargetPerceptionUpdated);
	}
}

void AAoCAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Initialize blackboard and start behavior tree
	if (DefaultBehaviorTree && DefaultBlackboardData)
	{
		UBlackboardComponent* BBComp = nullptr;
		UseBlackboard(DefaultBlackboardData, BBComp);

		if (BBComp)
		{
			// Set home location to the pawn's spawn position
			BBComp->SetValueAsVector(BB_HomeLocation, InPawn->GetActorLocation());
			BBComp->SetValueAsInt(BB_PatrolIndex, 0);
			BBComp->SetValueAsInt(BB_CombatState, 0);
			BBComp->SetValueAsFloat(BB_AggroRange, 1000.0f);
		}

		RunBehaviorTree(DefaultBehaviorTree);
	}
}

void AAoCAIController::SetBehaviorTree(UBehaviorTree* NewTree)
{
	if (!NewTree)
	{
		return;
	}

	DefaultBehaviorTree = NewTree;

	// If we already have a blackboard, restart with the new tree
	if (Blackboard)
	{
		RunBehaviorTree(NewTree);
	}
}

void AAoCAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !Blackboard)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		// Actor detected — set as target if it's a player character
		ACharacter* PerceivedCharacter = Cast<ACharacter>(Actor);
		if (PerceivedCharacter && PerceivedCharacter->IsPlayerControlled())
		{
			Blackboard->SetValueAsObject(BB_TargetActor, Actor);
			Blackboard->SetValueAsInt(BB_CombatState, 1); // 1 = In Combat

			UE_LOG(LogTemp, Log, TEXT("AoCAIController: Detected target %s"), *Actor->GetName());
		}
	}
	else
	{
		// Actor lost — clear target if it was the current target
		UObject* CurrentTarget = Blackboard->GetValueAsObject(BB_TargetActor);
		if (CurrentTarget == Actor)
		{
			Blackboard->ClearValue(BB_TargetActor);
			Blackboard->SetValueAsInt(BB_CombatState, 0); // 0 = Idle

			UE_LOG(LogTemp, Log, TEXT("AoCAIController: Lost target %s"), *Actor->GetName());
		}
	}
}
