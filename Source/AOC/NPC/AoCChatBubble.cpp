// =============================================================================
// AoCChatBubble.cpp — Universal NPC Chat Bubble Widget (Implementation)
// Architect of Creation — Module: AOC
// =============================================================================
//
// Builds the entire UMG widget hierarchy in C++ (no Blueprint dependency).
// Drives a state machine in NativeTick that handles:
//   Hidden → FadingIn → Typing → Displaying → FadingOut → Hidden
//
// The typewriter effect respects punctuation pauses and queues messages so
// that NPCs in rapid-fire conversation don't drop lines.
//
// This widget is GENERIC — any NPC attaches it via a UWidgetComponent set to
// Screen or World space with "Draw at Desired Size" enabled.
//
// =============================================================================

#include "AoCChatBubble.h"

#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogChatBubble, Log, All);

// =============================================================================
// Construction
// =============================================================================

UAoCChatBubble::UAoCChatBubble(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Ensure ticking is enabled for this widget
	bHasScriptImplementedTick = true;
}

// =============================================================================
// NativeConstruct — Build widget tree entirely in C++
// =============================================================================

void UAoCChatBubble::NativeConstruct()
{
	Super::NativeConstruct();

	BuildWidgetLayout();

	// Start hidden
	SetBubbleState(EBubbleState::Hidden);
	ApplyOpacity(0.0f);
}

void UAoCChatBubble::NativeDestruct()
{
	ClearAll();
	Super::NativeDestruct();
}

// =============================================================================
// BuildWidgetLayout — Programmatic UMG construction
// =============================================================================
//
// Hierarchy:
//   [CanvasPanel] (root)
//     └─ [SizeBox] (max-width constraint = 300)
//          └─ [Border] (background #1a1a2eCC, crimson border, rounded)
//               └─ [Border] (inner padding)
//                    └─ [VerticalBox]
//                         ├─ [TextBlock] SpeakerName  (optional, bold, small)
//                         └─ [TextBlock] MessageText  (the typing text)
//
// =============================================================================

void UAoCChatBubble::BuildWidgetLayout()
{
	if (!WidgetTree)
	{
		UE_LOG(LogChatBubble, Error, TEXT("WidgetTree is null — cannot build layout."));
		return;
	}

	// ---- Root Canvas ----
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
	                                                       TEXT("RootCanvas"));
	if (!RootCanvas)
	{
		UE_LOG(LogChatBubble, Error, TEXT("Failed to construct RootCanvas."));
		return;
	}
	WidgetTree->RootWidget = RootCanvas;

	// ---- Size Box (max width) ----
	SizeConstraint = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
	                                                       TEXT("SizeConstraint"));
	SizeConstraint->SetMaxDesiredWidth(MaxBubbleWidth);

	UCanvasPanelSlot* SizeSlot = RootCanvas->AddChildToCanvas(SizeConstraint);
	if (SizeSlot)
	{
		// Center horizontally, anchor bottom-center so bubble grows upward
		SizeSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		SizeSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		SizeSlot->SetAutoSize(true);
	}

	// ---- Background Border ----
	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
	                                                        TEXT("BackgroundBorder"));
	BackgroundBorder->SetBrushColor(BackgroundColor);

	// Rounded outline via a 2px border effect using padding + nested border
	// UBorder doesn't natively support corner radius in C++, but we can set the
	// brush to a rounded material in data or use padding to approximate.
	// For now we set a solid brush and rely on the border color for the outline.
	BackgroundBorder->SetPadding(FMargin(2.0f)); // Acts as "border width"
	BackgroundBorder->SetBrushColor(BorderColor); // The outer ring is crimson

	SizeConstraint->AddChild(BackgroundBorder);

	// ---- Inner Padding (the actual dark background) ----
	InnerPadding = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
	                                                     TEXT("InnerPadding"));
	InnerPadding->SetBrushColor(BackgroundColor);
	InnerPadding->SetPadding(FMargin(10.0f, 6.0f, 10.0f, 6.0f)); // L T R B

	BackgroundBorder->AddChild(InnerPadding);

	// ---- Content Vertical Box ----
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
	                                                       TEXT("ContentBox"));
	InnerPadding->AddChild(ContentBox);

	// ---- Speaker Name (optional, starts hidden) ----
	SpeakerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
	                                                           TEXT("SpeakerNameText"));
	{
		FSlateFontInfo NameFont = SpeakerNameText->GetFont();
		NameFont.Size = FontSize - 2;
		// Bold style
		NameFont.TypefaceFontName = FName(TEXT("Bold"));
		SpeakerNameText->SetFont(NameFont);

		// Slightly dimmer than message text
		FLinearColor NameColor = TextColor;
		NameColor.A = 0.7f;
		SpeakerNameText->SetColorAndOpacity(FSlateColor(NameColor));
		SpeakerNameText->SetText(FText::GetEmpty());
		SpeakerNameText->SetVisibility(ESlateVisibility::Collapsed);
	}

	UVerticalBoxSlot* NameSlot = ContentBox->AddChildToVerticalBox(SpeakerNameText);
	if (NameSlot)
	{
		NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	// ---- Message Text ----
	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
	                                                      TEXT("MessageText"));
	{
		FSlateFontInfo MsgFont = MessageText->GetFont();
		MsgFont.Size = FontSize;
		MessageText->SetFont(MsgFont);
		MessageText->SetColorAndOpacity(FSlateColor(TextColor));
		MessageText->SetAutoWrapText(true);
		MessageText->SetText(FText::GetEmpty());
	}

	UVerticalBoxSlot* MsgSlot = ContentBox->AddChildToVerticalBox(MessageText);
	if (MsgSlot)
	{
		MsgSlot->SetPadding(FMargin(0.0f));
	}

	UE_LOG(LogChatBubble, Verbose, TEXT("Chat bubble widget layout built successfully."));
}

// =============================================================================
// Public API
// =============================================================================

void UAoCChatBubble::ShowMessage(const FString& Message,
                                  EChatBubblePriority InPriority)
{
	if (Message.IsEmpty())
	{
		return;
	}

	// If currently hidden and queue is empty, start immediately
	if (CurrentState == EBubbleState::Hidden && MessageQueue.Num() == 0)
	{
		FullMessage      = Message;
		DisplayedText.Empty();
		CurrentCharIndex = 0;
		CharTimer        = 0.0f;
		EllipsisDotCount = 0;
		bEllipsisPending = false;
		SetBubbleState(EBubbleState::FadingIn);
	}
	else
	{
		EnqueueMessage(Message, InPriority);
	}
}

void UAoCChatBubble::ClearAll()
{
	MessageQueue.Empty();
	FullMessage.Empty();
	DisplayedText.Empty();
	CurrentCharIndex = 0;
	CharTimer        = 0.0f;
	DisplayTimer     = 0.0f;
	FadeTimer        = 0.0f;
	EllipsisDotCount = 0;
	bEllipsisPending = false;

	SetBubbleState(EBubbleState::Hidden);
	ApplyOpacity(0.0f);

	if (MessageText)
	{
		MessageText->SetText(FText::GetEmpty());
	}
}

bool UAoCChatBubble::IsDisplayingMessage() const
{
	return CurrentState != EBubbleState::Hidden;
}

int32 UAoCChatBubble::GetQueueDepth() const
{
	return MessageQueue.Num();
}

void UAoCChatBubble::SetSpeakerName(const FString& Name)
{
	SpeakerName = Name;
	if (SpeakerNameText)
	{
		if (Name.IsEmpty())
		{
			SpeakerNameText->SetText(FText::GetEmpty());
			SpeakerNameText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			SpeakerNameText->SetText(FText::FromString(Name));
			SpeakerNameText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

// =============================================================================
// Queue Management
// =============================================================================

void UAoCChatBubble::EnqueueMessage(const FString& Msg, EChatBubblePriority InPriority)
{
	// Enforce max depth — drop oldest Normal if needed
	while (MessageQueue.Num() >= MaxQueueDepth)
	{
		if (!DropOldestNormal())
		{
			// Queue is all Important/Critical — drop oldest anyway
			MessageQueue.RemoveAt(0);
		}
	}

	FQueuedChatMessage Entry(Msg, InPriority);

	if (InPriority == EChatBubblePriority::Critical)
	{
		// Insert at front (after any other Critical messages already there)
		int32 InsertIdx = 0;
		for (int32 i = 0; i < MessageQueue.Num(); ++i)
		{
			if (MessageQueue[i].MsgPriority == EChatBubblePriority::Critical)
			{
				InsertIdx = i + 1;
			}
			else
			{
				break;
			}
		}
		MessageQueue.Insert(Entry, InsertIdx);
	}
	else if (InPriority == EChatBubblePriority::Important)
	{
		// Insert after all Critical and existing Important messages
		int32 InsertIdx = 0;
		for (int32 i = 0; i < MessageQueue.Num(); ++i)
		{
			if (MessageQueue[i].MsgPriority >= EChatBubblePriority::Important)
			{
				InsertIdx = i + 1;
			}
			else
			{
				break;
			}
		}
		MessageQueue.Insert(Entry, InsertIdx);
	}
	else
	{
		// Normal — append to end
		MessageQueue.Add(Entry);
	}
}

bool UAoCChatBubble::DropOldestNormal()
{
	for (int32 i = MessageQueue.Num() - 1; i >= 0; --i)
	{
		if (MessageQueue[i].MsgPriority == EChatBubblePriority::Normal)
		{
			UE_LOG(LogChatBubble, Verbose, TEXT("Dropping queued message: %s"),
			       *MessageQueue[i].Message);
			MessageQueue.RemoveAt(i);
			return true;
		}
	}
	return false;
}

// =============================================================================
// State Machine
// =============================================================================

void UAoCChatBubble::SetBubbleState(EBubbleState NewState)
{
	CurrentState = NewState;

	switch (NewState)
	{
	case EBubbleState::Hidden:
		FadeTimer    = 0.0f;
		DisplayTimer = 0.0f;
		CharTimer    = 0.0f;
		break;

	case EBubbleState::FadingIn:
		FadeTimer      = 0.0f;
		CurrentOpacity = 0.0f;
		// Set initial text display
		if (MessageText)
		{
			MessageText->SetText(FText::GetEmpty());
		}
		break;

	case EBubbleState::Typing:
		CharTimer        = 0.0f;
		EllipsisDotCount = 0;
		bEllipsisPending = false;
		break;

	case EBubbleState::Displaying:
		DisplayTimer = GetDisplayDuration(FullMessage);
		break;

	case EBubbleState::FadingOut:
		FadeTimer = 0.0f;
		break;
	}
}

void UAoCChatBubble::AdvanceToNextMessage()
{
	// Fire delegate for the completed message
	FString CompletedMsg = FullMessage;
	if (!CompletedMsg.IsEmpty())
	{
		OnMessageFinished.Broadcast(CompletedMsg);
	}

	if (MessageQueue.Num() > 0)
	{
		FQueuedChatMessage Next = MessageQueue[0];
		MessageQueue.RemoveAt(0);

		FullMessage      = Next.Message;
		DisplayedText.Empty();
		CurrentCharIndex = 0;
		EllipsisDotCount = 0;
		bEllipsisPending = false;

		// If we're still visible, go straight to typing (skip fade-in)
		if (CurrentOpacity > 0.8f)
		{
			if (MessageText)
			{
				MessageText->SetText(FText::GetEmpty());
			}
			SetBubbleState(EBubbleState::Typing);
		}
		else
		{
			SetBubbleState(EBubbleState::FadingIn);
		}
	}
	else
	{
		// No more messages — fade out
		SetBubbleState(EBubbleState::FadingOut);
		OnQueueEmpty.Broadcast();
	}
}

// =============================================================================
// NativeTick — Drives the state machine each frame
// =============================================================================

void UAoCChatBubble::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	switch (CurrentState)
	{
	case EBubbleState::Hidden:
		// Nothing to do — check if a message arrived (defensive)
		if (MessageQueue.Num() > 0)
		{
			AdvanceToNextMessage();
		}
		break;

	case EBubbleState::FadingIn:
		TickFadingIn(InDeltaTime);
		break;

	case EBubbleState::Typing:
		TickTyping(InDeltaTime);
		break;

	case EBubbleState::Displaying:
		TickDisplaying(InDeltaTime);
		break;

	case EBubbleState::FadingOut:
		TickFadingOut(InDeltaTime);
		break;
	}
}

// =============================================================================
// Tick — FadingIn
// =============================================================================

void UAoCChatBubble::TickFadingIn(float DeltaTime)
{
	FadeTimer += DeltaTime;
	const float Alpha = (FadeInDuration > 0.0f)
		? FMath::Clamp(FadeTimer / FadeInDuration, 0.0f, 1.0f)
		: 1.0f;

	CurrentOpacity = Alpha;
	ApplyOpacity(CurrentOpacity);

	if (Alpha >= 1.0f)
	{
		SetBubbleState(EBubbleState::Typing);
	}
}

// =============================================================================
// Tick — Typing (typewriter with punctuation pauses)
// =============================================================================

void UAoCChatBubble::TickTyping(float DeltaTime)
{
	if (CurrentCharIndex >= FullMessage.Len())
	{
		// Done typing
		SetBubbleState(EBubbleState::Displaying);
		return;
	}

	CharTimer += DeltaTime;

	// Reveal characters as timer permits
	while (CurrentCharIndex < FullMessage.Len())
	{
		const float Delay = GetCharDelay(CurrentCharIndex);

		if (CharTimer < Delay)
		{
			break; // Not enough time accumulated
		}

		CharTimer -= Delay;

		// Reveal the next character
		DisplayedText.AppendChar(FullMessage[CurrentCharIndex]);
		CurrentCharIndex++;

		// Update the widget
		if (MessageText)
		{
			MessageText->SetText(FText::FromString(DisplayedText));
		}
	}
}

// =============================================================================
// Tick — Displaying (hold after typing finishes)
// =============================================================================

void UAoCChatBubble::TickDisplaying(float DeltaTime)
{
	DisplayTimer -= DeltaTime;

	if (DisplayTimer <= 0.0f)
	{
		AdvanceToNextMessage();
	}
}

// =============================================================================
// Tick — FadingOut
// =============================================================================

void UAoCChatBubble::TickFadingOut(float DeltaTime)
{
	FadeTimer += DeltaTime;
	const float Alpha = (FadeOutDuration > 0.0f)
		? 1.0f - FMath::Clamp(FadeTimer / FadeOutDuration, 0.0f, 1.0f)
		: 0.0f;

	CurrentOpacity = Alpha;
	ApplyOpacity(CurrentOpacity);

	if (Alpha <= 0.0f)
	{
		// Fully hidden
		if (MessageText)
		{
			MessageText->SetText(FText::GetEmpty());
		}
		FullMessage.Empty();
		DisplayedText.Empty();

		// If new messages arrived during fade-out, start them
		if (MessageQueue.Num() > 0)
		{
			AdvanceToNextMessage();
		}
		else
		{
			SetBubbleState(EBubbleState::Hidden);
		}
	}
}

// =============================================================================
// GetCharDelay — Punctuation-aware typing speed
// =============================================================================

float UAoCChatBubble::GetCharDelay(int32 CharIndex) const
{
	if (CharIndex < 0 || CharIndex >= FullMessage.Len())
	{
		return BaseCharDelay;
	}

	const TCHAR Ch = FullMessage[CharIndex];

	// --- Ellipsis detection: three consecutive dots → one big pause --------
	if (Ch == TEXT('.'))
	{
		// Look ahead: is this part of "..."?
		int32 DotRun = 0;
		for (int32 i = CharIndex; i < FullMessage.Len() && FullMessage[i] == TEXT('.'); ++i)
		{
			DotRun++;
		}

		if (DotRun >= 3)
		{
			// Only apply the big pause on the LAST dot of the ellipsis
			int32 DotsBeforeThis = 0;
			for (int32 i = CharIndex - 1; i >= 0 && FullMessage[i] == TEXT('.'); --i)
			{
				DotsBeforeThis++;
			}

			const int32 TotalDots = DotsBeforeThis + DotRun;
			if (TotalDots >= 3 && DotsBeforeThis == 2)
			{
				// This is the 3rd dot — apply ellipsis pause
				return EllipsisPause;
			}

			// Other dots in the run get minimal delay
			return BaseCharDelay * 0.5f;
		}

		// Single period (sentence end)
		return BaseCharDelay * SentenceEndDelayMultiplier;
	}

	// --- Sentence-ending punctuation ---
	if (Ch == TEXT('!') || Ch == TEXT('?'))
	{
		return BaseCharDelay * SentenceEndDelayMultiplier;
	}

	// --- Clause punctuation ---
	if (Ch == TEXT(',') || Ch == TEXT(';') || Ch == TEXT(':'))
	{
		return BaseCharDelay * ClauseDelayMultiplier;
	}

	// --- Paragraph / newline ---
	if (Ch == TEXT('\n'))
	{
		return BaseCharDelay * SentenceEndDelayMultiplier;
	}

	// --- Default ---
	return BaseCharDelay;
}

// =============================================================================
// GetDisplayDuration — How long the completed message stays on screen
// =============================================================================
// Rule: 30 seconds real-time so player can read at leisure.

float UAoCChatBubble::GetDisplayDuration(const FString& Msg) const
{
	// Brad: flat 30 seconds real-time for all Oracle speech
	return 30.0f;
}

// =============================================================================
// ApplyOpacity — Fade the entire bubble
// =============================================================================

void UAoCChatBubble::ApplyOpacity(float Opacity)
{
	SetRenderOpacity(Opacity);

	// Also hide completely when at zero to prevent hit-testing
	if (Opacity <= 0.0f)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}
