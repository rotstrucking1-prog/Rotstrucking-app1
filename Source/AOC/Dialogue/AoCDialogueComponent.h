// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCDialogueData.h"
#include "AoCDialogueComponent.generated.h"

class APlayerController;
class UAoCQuestComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueStarted, APlayerController*, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeChanged, const FAoCDialogueNode&, NewNode);

/**
 * UAoCDialogueComponent
 * Manages dialogue state for an NPC. Drives the dialogue UI on the interacting player's client.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCDialogueComponent();

	/** The dialogue tree data asset to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Dialogue")
	TObjectPtr<UAoCDialogueTree> DialogueTree;

	// ── Dialogue Flow ──

	/** Start a dialogue session with a player. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void StartDialogue(APlayerController* Player);

	/** End the current dialogue session. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void EndDialogue();

	/** Select a dialogue choice by index (from the currently available choices). */
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void SelectChoice(int32 ChoiceIndex);

	// ── Queries ──

	/** Get the current dialogue node. Returns false if no dialogue is active. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	bool GetCurrentNode(FAoCDialogueNode& OutNode) const;

	/** Get the list of available choices for the current node, filtered by quest/skill requirements. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	TArray<FAoCDialogueChoice> GetAvailableChoices() const;

	/** Returns true if a dialogue session is currently active. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	bool IsDialogueActive() const { return bIsDialogueActive; }

	// ── Delegates ──

	UPROPERTY(BlueprintAssignable, Category = "AoC|Dialogue")
	FOnDialogueStarted OnDialogueStarted;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Dialogue")
	FOnDialogueEnded OnDialogueEnded;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Dialogue")
	FOnDialogueNodeChanged OnDialogueNodeChanged;

private:
	/** Currently active dialogue state. */
	bool bIsDialogueActive;
	int32 CurrentNodeID;

	/** Reference to the player currently in dialogue. */
	UPROPERTY()
	TObjectPtr<APlayerController> ActivePlayer;

	/** Navigate to a specific node by ID. Handles quest give/complete triggers. */
	void NavigateToNode(int32 NodeID);

	/** Check if a dialogue choice's requirements are met by the active player. */
	bool IsChoiceAvailable(const FAoCDialogueChoice& Choice) const;

	/** Get the quest component from the active player's pawn. */
	UAoCQuestComponent* GetPlayerQuestComponent() const;
};
