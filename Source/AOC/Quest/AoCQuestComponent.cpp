// Copyright Architect of Creation. All Rights Reserved.

#include "AoCQuestComponent.h"
#include "Net/UnrealNetwork.h"

UAoCQuestComponent::UAoCQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAoCQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAoCQuestComponent, ActiveQuestsArray);
	DOREPLIFETIME(UAoCQuestComponent, CompletedQuestIDs);
}

bool UAoCQuestComponent::AcceptQuest(const FAoCQuestData& QuestData)
{
	if (!CanAcceptQuest(QuestData))
	{
		return false;
	}

	FAoCQuestData NewQuest = QuestData;
	NewQuest.Status = EQuestStatus::InProgress;

	// Reset objective progress
	for (FAoCQuestObjective& Obj : NewQuest.Objectives)
	{
		Obj.CurrentCount = 0;
	}

	ActiveQuests.Add(NewQuest.QuestID, NewQuest);
	SyncArrayFromMap();

	OnQuestAccepted.Broadcast(NewQuest);

	UE_LOG(LogTemp, Log, TEXT("AoCQuestComponent: Accepted quest '%s' — %s"), *NewQuest.QuestID.ToString(), *NewQuest.Title);

	return true;
}

bool UAoCQuestComponent::AbandonQuest(FName QuestID)
{
	if (!ActiveQuests.Contains(QuestID))
	{
		return false;
	}

	ActiveQuests.Remove(QuestID);
	SyncArrayFromMap();

	OnQuestAbandoned.Broadcast(QuestID);

	UE_LOG(LogTemp, Log, TEXT("AoCQuestComponent: Abandoned quest '%s'"), *QuestID.ToString());

	return true;
}

bool UAoCQuestComponent::CompleteQuest(FName QuestID)
{
	FAoCQuestData* Quest = ActiveQuests.Find(QuestID);
	if (!Quest)
	{
		return false;
	}

	if (!Quest->AreAllObjectivesComplete())
	{
		UE_LOG(LogTemp, Warning, TEXT("AoCQuestComponent: Cannot complete quest '%s' — not all objectives met."), *QuestID.ToString());
		return false;
	}

	Quest->Status = EQuestStatus::Completed;
	FAoCQuestData CompletedQuest = *Quest;

	// Move to completed list
	CompletedQuestIDs.Add(QuestID);
	ActiveQuests.Remove(QuestID);
	SyncArrayFromMap();

	OnQuestCompleted.Broadcast(CompletedQuest);

	UE_LOG(LogTemp, Log, TEXT("AoCQuestComponent: Completed quest '%s' — XP Reward: %d"), *QuestID.ToString(), CompletedQuest.XPReward);

	return true;
}

void UAoCQuestComponent::UpdateObjective(EQuestObjectiveType Type, FName TargetID, int32 Count)
{
	bool bAnyUpdated = false;

	for (auto& Pair : ActiveQuests)
	{
		FAoCQuestData& Quest = Pair.Value;
		if (Quest.Status != EQuestStatus::InProgress)
		{
			continue;
		}

		for (FAoCQuestObjective& Obj : Quest.Objectives)
		{
			if (Obj.Type == Type && Obj.TargetID == TargetID && !Obj.IsComplete())
			{
				Obj.CurrentCount = FMath::Min(Obj.CurrentCount + Count, Obj.RequiredCount);
				OnObjectiveUpdated.Broadcast(Quest.QuestID, Obj);
				bAnyUpdated = true;

				UE_LOG(LogTemp, Log, TEXT("AoCQuestComponent: Updated objective '%s' (%d/%d) for quest '%s'"),
					*Obj.Description, Obj.CurrentCount, Obj.RequiredCount, *Quest.QuestID.ToString());

				// Auto-complete quest if all objectives are done
				if (Quest.AreAllObjectivesComplete())
				{
					// Don't auto-complete — let the player turn it in at an NPC.
					// But log that it's ready.
					UE_LOG(LogTemp, Log, TEXT("AoCQuestComponent: Quest '%s' is ready for turn-in!"), *Quest.QuestID.ToString());
				}
			}
		}
	}

	if (bAnyUpdated)
	{
		SyncArrayFromMap();
	}
}

bool UAoCQuestComponent::CanAcceptQuest(const FAoCQuestData& QuestData) const
{
	// Check max active quests
	if (ActiveQuests.Num() >= MaxActiveQuests)
	{
		return false;
	}

	// Check not already active
	if (ActiveQuests.Contains(QuestData.QuestID))
	{
		return false;
	}

	// Check not already completed
	if (CompletedQuestIDs.Contains(QuestData.QuestID))
	{
		return false;
	}

	// Check prerequisites
	for (const FName& PrereqID : QuestData.Prerequisites)
	{
		if (!CompletedQuestIDs.Contains(PrereqID))
		{
			return false;
		}
	}

	return true;
}

bool UAoCQuestComponent::GetActiveQuest(FName QuestID, FAoCQuestData& OutQuest) const
{
	const FAoCQuestData* Found = ActiveQuests.Find(QuestID);
	if (Found)
	{
		OutQuest = *Found;
		return true;
	}
	return false;
}

bool UAoCQuestComponent::IsQuestCompleted(FName QuestID) const
{
	return CompletedQuestIDs.Contains(QuestID);
}

TArray<FName> UAoCQuestComponent::GetActiveQuestIDs() const
{
	TArray<FName> IDs;
	ActiveQuests.GetKeys(IDs);
	return IDs;
}

void UAoCQuestComponent::RebuildQuestMap()
{
	ActiveQuests.Empty();
	for (const FAoCQuestData& Quest : ActiveQuestsArray)
	{
		ActiveQuests.Add(Quest.QuestID, Quest);
	}
}

void UAoCQuestComponent::SyncArrayFromMap()
{
	ActiveQuestsArray.Empty(ActiveQuests.Num());
	for (const auto& Pair : ActiveQuests)
	{
		ActiveQuestsArray.Add(Pair.Value);
	}
}
