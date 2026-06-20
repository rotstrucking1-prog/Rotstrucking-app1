// Copyright Architect of Creation. All Rights Reserved.

#include "AoCDialogueWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"

// Include dialogue data for the choice struct
#include "AoCDialogueData.h"

UAoCDialogueWidget::UAoCDialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TypewriterSpeed = 40.0f; // 40 characters per second
	TypewriterIndex = 0;
	TypewriterTimer = 0.0f;
	bIsTypewriting = false;
}

void UAoCDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Start hidden
	SetVisibility(ESlateVisibility::Hidden);
}

void UAoCDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsTypewriting || !DialogueText)
	{
		return;
	}

	TypewriterTimer += InDeltaTime;

	float CharInterval = (TypewriterSpeed > 0.0f) ? (1.0f / TypewriterSpeed) : 0.0f;

	while (TypewriterTimer >= CharInterval && TypewriterIndex < FullDialogueText.Len())
	{
		TypewriterTimer -= CharInterval;
		TypewriterIndex++;
	}

	// Update displayed text
	FString DisplayedText = FullDialogueText.Left(TypewriterIndex);
	DialogueText->SetText(FText::FromString(DisplayedText));

	// Check if typewriter is complete
	if (TypewriterIndex >= FullDialogueText.Len())
	{
		bIsTypewriting = false;
		TypewriterTimer = 0.0f;

		// Now show the choice buttons
		SpawnChoiceButtons();
	}
}

void UAoCDialogueWidget::SetDialogueNode(const FString& SpeakerName, const FString& Text, const TArray<FAoCDialogueChoice>& Choices)
{
	SetVisibility(ESlateVisibility::Visible);

	// Set speaker name
	if (SpeakerNameText)
	{
		SpeakerNameText->SetText(FText::FromString(SpeakerName));
	}

	// Start typewriter effect
	FullDialogueText = Text;
	TypewriterIndex = 0;
	TypewriterTimer = 0.0f;
	bIsTypewriting = true;

	if (DialogueText)
	{
		DialogueText->SetText(FText::GetEmpty());
	}

	// Cache choices — they'll be shown after typewriter finishes
	PendingChoices = Choices;

	// Clear existing choice buttons
	ClearChoiceButtons();
}

void UAoCDialogueWidget::SetPortrait(UTexture2D* Portrait)
{
	if (PortraitImage)
	{
		if (Portrait)
		{
			PortraitImage->SetBrushFromTexture(Portrait);
			PortraitImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			PortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UAoCDialogueWidget::SkipTypewriter()
{
	if (!bIsTypewriting)
	{
		return;
	}

	bIsTypewriting = false;
	TypewriterIndex = FullDialogueText.Len();
	TypewriterTimer = 0.0f;

	if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(FullDialogueText));
	}

	SpawnChoiceButtons();
}

void UAoCDialogueWidget::SpawnChoiceButtons()
{
	ClearChoiceButtons();

	if (!ChoiceContainer || !ChoiceButtonClass)
	{
		return;
	}

	for (int32 i = 0; i < PendingChoices.Num(); ++i)
	{
		const FAoCDialogueChoice& Choice = PendingChoices[i];

		UUserWidget* ButtonWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), ChoiceButtonClass);
		if (!ButtonWidget)
		{
			continue;
		}

		// Find the text block inside the button widget to set the choice text
		UTextBlock* LabelText = Cast<UTextBlock>(ButtonWidget->GetWidgetFromName(TEXT("ChoiceLabel")));
		if (LabelText)
		{
			LabelText->SetText(FText::FromString(Choice.ChoiceText));
		}

		// Find the button for click binding
		UButton* ClickButton = Cast<UButton>(ButtonWidget->GetWidgetFromName(TEXT("ChoiceButton")));
		if (ClickButton)
		{
			int32 ChoiceIdx = i;
			ClickButton->OnClicked.AddDynamic(this, &UAoCDialogueWidget::HandleChoiceClicked);
			// Store the index on the widget for retrieval — use a custom subwidget in production
		}

		ChoiceContainer->AddChild(ButtonWidget);
	}
}

void UAoCDialogueWidget::ClearChoiceButtons()
{
	if (ChoiceContainer)
	{
		ChoiceContainer->ClearChildren();
	}
}

void UAoCDialogueWidget::HandleChoiceClicked(int32 ChoiceIndex)
{
	OnChoiceSelected.Broadcast(ChoiceIndex);
}
