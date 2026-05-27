// Copyright Architect of Creation. All Rights Reserved.

#include "AoCGameMode.h"
#include "AoCGameState.h"
#include "AoCPlayerState.h"
#include "AoCPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAoCGameMode::AAoCGameMode()
{
	RespawnDelay = 5.0f;
	RespawnTag = FName(TEXT("Default"));

	// Set default classes — these can be overridden in Blueprint children
	DefaultPawnClass = nullptr; // Set in Blueprint to BP_PlayerCharacter
	PlayerControllerClass = AAoCPlayerController::StaticClass();
	PlayerStateClass = AAoCPlayerState::StaticClass();
	GameStateClass = AAoCGameState::StaticClass();
}

void AAoCGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("AoCGameMode: BeginPlay — server started."));
}

void AAoCGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer)
	{
		UE_LOG(LogTemp, Log, TEXT("AoCGameMode: Player logged in — %s"), *NewPlayer->GetName());
	}
}

void AAoCGameMode::Logout(AController* Exiting)
{
	// Clean up any pending respawn timers
	if (FTimerHandle* Handle = PendingRespawns.Find(Exiting))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
		PendingRespawns.Remove(Exiting);
	}

	Super::Logout(Exiting);
}

void AAoCGameMode::RequestPlayerRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	// Don't queue duplicate respawns
	if (PendingRespawns.Contains(Controller))
	{
		return;
	}

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUFunction(this, FName(TEXT("RespawnPlayer")), Controller);

	GetWorldTimerManager().SetTimer(TimerHandle, [this, Controller]()
	{
		RespawnPlayer(Controller);
	}, RespawnDelay, false);

	PendingRespawns.Add(Controller, TimerHandle);

	UE_LOG(LogTemp, Log, TEXT("AoCGameMode: Respawn requested for %s — delay %.1fs"), *Controller->GetName(), RespawnDelay);
}

void AAoCGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	PendingRespawns.Remove(Controller);

	// Find a suitable player start
	AActor* StartSpot = FindPlayerStart(Controller, RespawnTag.ToString());
	if (!StartSpot)
	{
		StartSpot = FindPlayerStart(Controller);
	}

	if (StartSpot && DefaultPawnClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		APawn* NewPawn = GetWorld()->SpawnActor<APawn>(
			DefaultPawnClass,
			StartSpot->GetActorLocation(),
			StartSpot->GetActorRotation(),
			SpawnParams
		);

		if (NewPawn)
		{
			Controller->Possess(NewPawn);
			UE_LOG(LogTemp, Log, TEXT("AoCGameMode: Player %s respawned at %s"), *Controller->GetName(), *StartSpot->GetName());
		}
	}
}
