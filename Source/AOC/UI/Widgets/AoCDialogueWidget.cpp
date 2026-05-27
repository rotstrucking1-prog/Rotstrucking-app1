// Copyright Architect of Creation. All Rights Reserved.

#include "AoCDialogueWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"

void UAoCDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TypewriterSpeed = 0.03f;
	TypewriterTimer = 0.0f;
	CurrentCharIndex = 0;
	bIsTypewriting = false;
}

void UAoCDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsTypewriting)
	{
		TypewriterTimer += InDeltaTime;
		while (TypewriterTimer >= TypewriterSpeed && CurrentCharIndex < FullDialogueText.Len())
		{
			TypewriterTimer -= TypewriterSpeed;
			CurrentCharIndex++;
			DisplayedText = FullDialogueText.Left(CurrentCharIndex);
			if (DialogueTextBlock)
			{
				DialogueTextBlock->SetText(FText::FromString(DisplayedText));
			}
		}
		if (CurrentCharIndex >= FullDialogueText.Len())
		{
			bIsTypewriting = false;
		}
	}
}

void UAoCDialogueWidget::SetDialogueNode(const FText& SpeakerName, const FText& DialogueText, const TArray<FAoCDialogueChoice>& Choices)
{
	if (SpeakerNameText)
	{
		SpeakerNameText->SetText(SpeakerName);
	}

	FullDialogueText = DialogueText.ToString();
	DisplayedText = TEXT("");
	CurrentCharIndex = 0;
	TypewriterTimer = 0.0f;
	bIsTypewriting = true;

	if (ChoiceContainer)
	{
		ChoiceContainer->ClearChildren();
	}

	for (int32 i = 0; i < Choices.Num(); ++i)
	{
		UE_LOG(LogTemp, Log, TEXT("Dialogue Choice %d: %s"), i, *Choices[i].ChoiceText);
	}
}

void UAoCDialogueWidget::SetPortrait(UTexture2D* PortraitTexture)
{
	if (PortraitImage && PortraitTexture)
	{
		PortraitImage->SetBrushFromTexture(PortraitTexture);
	}
}

void UAoCDialogueWidget::SkipTypewriter()
{
	if (bIsTypewriting)
	{
		bIsTypewriting = false;
		CurrentCharIndex = FullDialogueText.Len();
		DisplayedText = FullDialogueText;
		if (DialogueTextBlock)
		{
			DialogueTextBlock->SetText(FText::FromString(DisplayedText));
		}
	}
}

bool UAoCDialogueWidget::IsTypewriterPlaying() const
{
	return bIsTypewriting;
}

void UAoCDialogueWidget::HandleChoiceClicked(int32 ChoiceIndex)
{
	OnChoiceSelected.Broadcast(ChoiceIndex);
}
