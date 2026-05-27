// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../Dialogue/AoCDialogueData.h"
#include "AoCDialogueWidget.generated.h"

class UTextBlock;
class UImage;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueChoiceSelected, int32, ChoiceIndex);

UCLASS()
class AOC_API UAoCDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "AoC|Dialogue")
	FOnDialogueChoiceSelected OnChoiceSelected;

	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void SetDialogueNode(const FText& SpeakerName, const FText& DialogueText, const TArray<FAoCDialogueChoice>& Choices);

	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void SetPortrait(UTexture2D* PortraitTexture);

	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void SkipTypewriter();

	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	bool IsTypewriterPlaying() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void HandleChoiceClicked(int32 ChoiceIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FString FullDialogueText;
	FString DisplayedText;
	float TypewriterSpeed = 0.03f;
	float TypewriterTimer = 0.0f;
	int32 CurrentCharIndex = 0;
	bool bIsTypewriting = false;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* SpeakerNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* DialogueTextBlock;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* PortraitImage;

	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* ChoiceContainer;
};
