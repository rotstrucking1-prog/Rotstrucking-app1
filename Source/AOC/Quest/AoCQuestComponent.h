// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCQuestData.h"
#include "AoCQuestComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAccepted, const FAoCQuestData&, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestCompleted, const FAoCQuestData&, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveUpdated, FName, QuestID, const FAoCQuestObjective&, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAbandoned, FName, QuestID);

/**
 * UAoCQuestComponent
 * Tracks active and completed quests for a player character.
 * Supports accepting, abandoning, completing, and updating quest objectives.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class ARCHITECTOFCREATION_API UAoCQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCQuestComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Maximum number of concurrently active quests. */
	static constexpr int32 MaxActiveQuests = 25;

	// ── Quest Management ──

	/** Accept a new quest. Returns false if prerequisites not met, max quests reached, or already active. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	bool AcceptQuest(const FAoCQuestData& QuestData);

	/** Abandon an active quest by its ID. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	bool AbandonQuest(FName QuestID);

	/** Mark a quest as completed. Grants rewards. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	bool CompleteQuest(FName QuestID);

	/** Update progress on objectives matching the given type and target. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	void UpdateObjective(EQuestObjectiveType Type, FName TargetID, int32 Count = 1);

	// ── Queries ──

	/** Check if a quest can be accepted (prerequisites met, not already active/completed, slot available). */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	bool CanAcceptQuest(const FAoCQuestData& QuestData) const;

	/** Get an active quest by ID. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	bool GetActiveQuest(FName QuestID, FAoCQuestData& OutQuest) const;

	/** Check if a specific quest has been completed. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	bool IsQuestCompleted(FName QuestID) const;

	/** Get the number of active quests. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	int32 GetActiveQuestCount() const { return ActiveQuests.Num(); }

	/** Get all active quest IDs. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	TArray<FName> GetActiveQuestIDs() const;

	/** Get all completed quest IDs. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Quest")
	TArray<FName> GetCompletedQuestIDs() const { return CompletedQuestIDs; }

	// ── Delegates ──

	UPROPERTY(BlueprintAssignable, Category = "AoC|Quest")
	FOnQuestAccepted OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Quest")
	FOnQuestCompleted OnQuestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Quest")
	FOnObjectiveUpdated OnObjectiveUpdated;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Quest")
	FOnQuestAbandoned OnQuestAbandoned;

protected:
	/** Map of active quests keyed by QuestID. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AoC|Quest")
	TArray<FAoCQuestData> ActiveQuestsArray;

	/** Completed quest IDs for prerequisite checks. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AoC|Quest")
	TArray<FName> CompletedQuestIDs;

private:
	/** Non-replicated map for fast lookup. Rebuilt from ActiveQuestsArray. */
	TMap<FName, FAoCQuestData> ActiveQuests;

	/** Rebuild the ActiveQuests map from the replicated array. */
	void RebuildQuestMap();

	/** Sync the replicated array from the map. */
	void SyncArrayFromMap();
};
