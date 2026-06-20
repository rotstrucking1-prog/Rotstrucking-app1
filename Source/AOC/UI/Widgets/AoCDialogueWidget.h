// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AoCDialogueWidget.generated.h"

USTRUCT(BlueprintType)
struct FAoCDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 NextNodeID = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RequiredQuestID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredSkillLevel = 0;
};


class UTextBlock;
class UVerticalBox;
class UImage;
class UButton;
struct FAoCDialogueNode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueChoiceSelected, int32, ChoiceIndex);

/**
 * UAoCDialogueWidget
 * Dialogue UI with speaker name, typewriter text, choice buttons, and an optional portrait.
 */
UCLASS()
class AOC_API UAoCDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAoCDialogueWidget(const FObjectInitializer& ObjectInitializer);

	// ── Bound Widgets ──

	/** Speaker name label. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SpeakerNameText;

	/** Dialogue body text (filled by typewriter effect). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DialogueText;

	/** Container for dynamically spawned choice buttons. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> ChoiceContainer;

	/** Optional portrait image for the speaker. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PortraitImage;

	// ── Configuration ──

	/** Speed of the typewriter effect (characters per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|UI|Dialogue")
	float TypewriterSpeed;

	/** Blueprint class for choice buttons. Must have a UTextBlock named "ChoiceLabel". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|UI|Dialogue")
	TSubclassOf<UUserWidget> ChoiceButtonClass;

	// ── API ──

	/** Set a dialogue node. Starts the typewriter effect and spawns choice buttons. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Dialogue")
	void SetDialogueNode(const FString& SpeakerName, const FString& Text, const TArray<FAoCDialogueChoice>& Choices);

	/** Set the portrait image. Pass nullptr to hide the portrait. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Dialogue")
	void SetPortrait(UTexture2D* Portrait);

	/** Skip the typewriter effect and show the full text immediately. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Dialogue")
	void SkipTypewriter();

	/** Check if the typewriter effect is still playing. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Dialogue")
	bool IsTypewriterPlaying() const { return bIsTypewriting; }

	// ── Delegate ──

	UPROPERTY(BlueprintAssignable, Category = "AoC|UI|Dialogue")
	FOnDialogueChoiceSelected OnChoiceSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Full text to display. */
	FString FullDialogueText;

	/** Current character index for the typewriter. */
	int32 TypewriterIndex;

	/** Accumulated time for typewriter tick. */
	float TypewriterTimer;

	/** Whether the typewriter is active. */
	bool bIsTypewriting;

	/** Cached choices to spawn buttons after typewriter completes. */
	TArray<FAoCDialogueChoice> PendingChoices;

	/** Create choice buttons from the pending choices array. */
	void SpawnChoiceButtons();

	/** Clear all choice buttons from the container. */
	void ClearChoiceButtons();

	/** Handler for when a choice button is clicked. */
	UFUNCTION()
	void HandleChoiceClicked(int32 ChoiceIndex);
};
