// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AoCGameMode.generated.h"

class AAoCPlayerController;

/**
 * AAoCGameMode
 * Main game mode for Architect of Creation.
 * Sets default pawn, player controller, player state, and game state classes.
 * Handles player respawn with a configurable timer.
 */
UCLASS()
class AOC_API AAoCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAoCGameMode();

	/** Time in seconds before a player respawns after death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Respawn")
	float RespawnDelay;

	/** Default spawn location tag. Pawns respawn at PlayerStart actors with this tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Respawn")
	FName RespawnTag;

	/** Request a player respawn after the configured delay. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Respawn")
	void RequestPlayerRespawn(AController* Controller);

	/** Immediately respawn a player at a suitable start location. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Respawn")
	void RespawnPlayer(AController* Controller);

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

private:
	/** Map of controllers pending respawn to their timer handles. */
	TMap<TWeakObjectPtr<AController>, FTimerHandle> PendingRespawns;
};
