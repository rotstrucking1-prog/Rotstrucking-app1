// Copyright Architect of Creation. All Rights Reserved.

#include "AoCDialogueComponent.h"
#include "../Quest/AoCQuestComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

UAoCDialogueComponent::UAoCDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bIsDialogueActive = false;
	CurrentNodeID = -1;
}

void UAoCDialogueComponent::StartDialogue(APlayerController* Player)
{
	if (!Player || !DialogueTree)
	{
		return;
	}

	if (bIsDialogueActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("AoCDialogueComponent: Dialogue already active."));
		return;
	}

	ActivePlayer = Player;
	bIsDialogueActive = true;

	NavigateToNode(DialogueTree->StartNodeID);

	OnDialogueStarted.Broadcast(Player);

	UE_LOG(LogTemp, Log, TEXT("AoCDialogueComponent: Dialogue started with %s"), *Player->GetName());
}

void UAoCDialogueComponent::EndDialogue()
{
	if (!bIsDialogueActive)
	{
		return;
	}

	bIsDialogueActive = false;
	CurrentNodeID = -1;
	ActivePlayer = nullptr;

	OnDialogueEnded.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("AoCDialogueComponent: Dialogue ended."));
}

void UAoCDialogueComponent::SelectChoice(int32 ChoiceIndex)
{
	if (!bIsDialogueActive || !DialogueTree)
	{
		return;
	}

	TArray<FAoCDialogueChoice> Available = GetAvailableChoices();

	if (ChoiceIndex < 0 || ChoiceIndex >= Available.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("AoCDialogueComponent: Invalid choice index %d (available: %d)"), ChoiceIndex, Available.Num());
		return;
	}

	const FAoCDialogueChoice& Choice = Available[ChoiceIndex];

	if (Choice.NextNodeID < 0)
	{
		// End of conversation
		EndDialogue();
		return;
	}

	NavigateToNode(Choice.NextNodeID);
}

bool UAoCDialogueComponent::GetCurrentNode(FAoCDialogueNode& OutNode) const
{
	if (!bIsDialogueActive || !DialogueTree)
	{
		return false;
	}

	const FAoCDialogueNode* Node = DialogueTree->FindNode(CurrentNodeID);
	if (Node)
	{
		OutNode = *Node;
		return true;
	}
	return false;
}

TArray<FAoCDialogueChoice> UAoCDialogueComponent::GetAvailableChoices() const
{
	TArray<FAoCDialogueChoice> Available;

	if (!bIsDialogueActive || !DialogueTree)
	{
		return Available;
	}

	const FAoCDialogueNode* Node = DialogueTree->FindNode(CurrentNodeID);
	if (!Node)
	{
		return Available;
	}

	for (const FAoCDialogueChoice& Choice : Node->Choices)
	{
		if (IsChoiceAvailable(Choice))
		{
			Available.Add(Choice);
		}
	}

	return Available;
}

void UAoCDialogueComponent::NavigateToNode(int32 NodeID)
{
	if (!DialogueTree)
	{
		return;
	}

	const FAoCDialogueNode* Node = DialogueTree->FindNode(NodeID);
	if (!Node)
	{
		UE_LOG(LogTemp, Warning, TEXT("AoCDialogueComponent: Node ID %d not found in tree."), NodeID);
		EndDialogue();
		return;
	}

	CurrentNodeID = NodeID;

	// Handle quest triggers on this node
	UAoCQuestComponent* QuestComp = GetPlayerQuestComponent();

	if (QuestComp)
	{
		// Give quest if specified
		if (!Node->QuestToGive.IsNone())
		{
			UE_LOG(LogTemp, Log, TEXT("AoCDialogueComponent: Quest '%s' should be offered to player."), *Node->QuestToGive.ToString());
		}

		// Complete quest if specified
		if (!Node->QuestToComplete.IsNone())
		{
			QuestComp->CompleteQuest(Node->QuestToComplete);
		}
	}

	OnDialogueNodeChanged.Broadcast(*Node);

	// If this is an end node, auto-end after broadcasting
	if (Node->bIsEndNode)
	{
		EndDialogue();
	}
}

bool UAoCDialogueComponent::IsChoiceAvailable(const FAoCDialogueChoice& Choice) const
{
	// Check quest requirement
	if (!Choice.RequiredQuestID.IsNone())
	{
		UAoCQuestComponent* QuestComp = GetPlayerQuestComponent();
		if (!QuestComp)
		{
			return false;
		}

		// Check if quest is active or completed
		FAoCQuestData QuestData;
		bool bActive = QuestComp->GetActiveQuest(Choice.RequiredQuestID, QuestData);
		bool bCompleted = QuestComp->IsQuestCompleted(Choice.RequiredQuestID);

		if (!bActive && !bCompleted)
		{
			return false;
		}
	}

	// Check skill requirement
	if (!Choice.RequiredSkillType.IsNone() && Choice.RequiredSkillLevel > 0)
	{
		// Would check the player's SkillComponent for the required skill level
	}

	return true;
}

UAoCQuestComponent* UAoCDialogueComponent::GetPlayerQuestComponent() const
{
	if (!ActivePlayer)
	{
		return nullptr;
	}

	APawn* PlayerPawn = ActivePlayer->GetPawn();
	if (!PlayerPawn)
	{
		return nullptr;
	}

	return PlayerPawn->FindComponentByClass<UAoCQuestComponent>();
}
