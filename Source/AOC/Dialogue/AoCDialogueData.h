// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AoCDialogueData.generated.h"

/**
 * FAoCDialogueChoice
 * A single player-selectable dialogue choice.
 */
USTRUCT(BlueprintType)
struct ARCHITECTOFCREATION_API FAoCDialogueChoice
{
	GENERATED_BODY()

	/** Text displayed for this choice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	FString ChoiceText;

	/** Node ID to navigate to when this choice is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	int32 NextNodeID;

	/** Optional: quest that must be active/completed for this choice to appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	FName RequiredQuestID;

	/** Optional: skill type required for this choice (empty = no requirement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	FName RequiredSkillType;

	/** Optional: minimum skill level required (0 = no requirement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	int32 RequiredSkillLevel;

	FAoCDialogueChoice()
		: NextNodeID(-1)
		, RequiredQuestID(NAME_None)
		, RequiredSkillType(NAME_None)
		, RequiredSkillLevel(0)
	{}
};

/**
 * FAoCDialogueNode
 * A single node in a dialogue tree containing text and choices.
 */
USTRUCT(BlueprintType)
struct ARCHITECTOFCREATION_API FAoCDialogueNode
{
	GENERATED_BODY()

	/** Unique identifier for this node within the tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	int32 NodeID;

	/** Name of the speaking character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	FString SpeakerName;

	/** The dialogue text shown to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue", meta = (MultiLine = true))
	FString DialogueText;

	/** Available choices for the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	TArray<FAoCDialogueChoice> Choices;

	/** If true, this node ends the conversation (no choices needed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	bool bIsEndNode;

	/** Optional: quest to offer to the player when this node is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	FName QuestToGive;

	/** Optional: quest to turn in (complete) when this node is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	FName QuestToComplete;

	FAoCDialogueNode()
		: NodeID(0)
		, bIsEndNode(false)
		, QuestToGive(NAME_None)
		, QuestToComplete(NAME_None)
	{}
};

/**
 * UAoCDialogueTree
 * Data asset containing a complete dialogue tree for an NPC.
 */
UCLASS(BlueprintType)
class ARCHITECTOFCREATION_API UAoCDialogueTree : public UDataAsset
{
	GENERATED_BODY()

public:
	/** All dialogue nodes in this tree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AoC|Dialogue")
	TArray<FAoCDialogueNode> Nodes;

	/** ID of the first node to display when dialogue starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AoC|Dialogue")
	int32 StartNodeID;

	/** Find a node by its ID. Returns nullptr if not found. */
	const FAoCDialogueNode* FindNode(int32 NodeID) const
	{
		for (const FAoCDialogueNode& Node : Nodes)
		{
			if (Node.NodeID == NodeID)
			{
				return &Node;
			}
		}
		return nullptr;
	}

	UAoCDialogueTree()
		: StartNodeID(0)
	{}
};
