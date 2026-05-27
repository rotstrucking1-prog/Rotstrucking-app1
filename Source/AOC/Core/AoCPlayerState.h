// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AoCPlayerState.generated.h"

class UAoCSkillComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnKillDeathUpdated, int32, Kills, int32, Deaths);

/**
 * AAoCPlayerState
 * Per-player replicated state: display name, total level, kill/death stats, guild name.
 */
UCLASS()
class ARCHITECTOFCREATION_API AAoCPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AAoCPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Display Name ----

	/** Custom display name for this player. */
	UPROPERTY(ReplicatedUsing = OnRep_AoCPlayerName, BlueprintReadOnly, Category = "AoC|Player")
	FString AoCPlayerName;

	UFUNCTION()
	void OnRep_AoCPlayerName();

	UFUNCTION(BlueprintCallable, Category = "AoC|Player")
	void SetAoCPlayerName(const FString& NewName);

	// ---- Total Level ----

	/** Cached total combined level across all skills. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AoC|Player")
	int32 TotalLevel;

	/** Recalculate total level from the pawn's SkillComponent. Call on server. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Player")
	void RefreshTotalLevel();

	// ---- Kill / Death Stats ----

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AoC|Stats")
	int32 Kills;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AoC|Stats")
	int32 Deaths;

	UFUNCTION(BlueprintCallable, Category = "AoC|Stats")
	void AddKill();

	UFUNCTION(BlueprintCallable, Category = "AoC|Stats")
	void AddDeath();

	UPROPERTY(BlueprintAssignable, Category = "AoC|Stats")
	FOnKillDeathUpdated OnKillDeathUpdated;

	// ---- Guild ----

	UPROPERTY(ReplicatedUsing = OnRep_GuildName, BlueprintReadOnly, Category = "AoC|Guild")
	FString GuildName;

	UFUNCTION()
	void OnRep_GuildName();

	UFUNCTION(BlueprintCallable, Category = "AoC|Guild")
	void SetGuildName(const FString& NewGuildName);
};
