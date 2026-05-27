// =============================================================================
// AoCChatBubble.h — Universal NPC Chat Bubble Widget
// Architect of Creation — Module: AOC
// =============================================================================
//
// PURPOSE:
//   A UUserWidget that renders floating speech bubbles above ANY NPC's head.
//   Designed to be attached via UWidgetComponent in world space (billboard mode).
//   Features a typewriter text reveal, punctuation-aware pauses, a priority
//   message queue, and smooth fade-in / fade-out transitions.
//
// VISUAL SPEC:
//   - Dark semi-transparent background (#1a1a2eCC) with crimson border (#dc143c)
//   - White text, 14-16pt, max-width 300px, auto-height
//   - Positioned 30-40 units above character head via the owning WidgetComponent
//   - Always faces camera (Screen space or billboard on the WidgetComponent)
//
// TIMING SPEC:
//   - Base character delay:    40 ms  (~25 chars/sec)
//   - Period / ! / ? pause:   200 ms
//   - Comma / ; pause:        100 ms
//   - Ellipsis "..." pause:   400 ms  (detected as three consecutive dots)
//   - Display hold:           1 s per 10 chars, clamped [3 s, 8 s]
//   - Fade-in duration:       0.2 s
//   - Fade-out duration:      0.5 s
//
// QUEUE RULES:
//   - Max depth: 5 messages
//   - If full, oldest Normal-priority message is dropped
//   - Priority::Critical inserts at front of queue
//   - Priority::Important inserts after current Critical messages
//   - Each message plays fully (type + hold) before the next begins
//
// INTEGRATION:
//   AoCNPCSpeech (or any caller) simply invokes ShowMessage() on the NPC's
//   chat bubble.  The bubble is a standalone component—no Blueprint asset
//   required.  All child widgets are constructed in C++ via NativeConstruct().
//
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AoCChatBubble.generated.h"

// Forward declarations — we reference these but build them in C++
class UTextBlock;
class UBorder;
class USizeBox;
class UCanvasPanel;
class UCanvasPanelSlot;
class UVerticalBox;

// ---------------------------------------------------------------------------
// EChatBubblePriority — Controls queue insertion order
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EChatBubblePriority : uint8
{
	Normal     UMETA(DisplayName = "Normal"),
	Important  UMETA(DisplayName = "Important"),
	Critical   UMETA(DisplayName = "Critical")
};

// ---------------------------------------------------------------------------
// FQueuedChatMessage — Internal queue entry
// ---------------------------------------------------------------------------
USTRUCT()
struct FQueuedChatMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Message;

	UPROPERTY()
	EChatBubblePriority MsgPriority = EChatBubblePriority::Normal;

	FQueuedChatMessage() = default;
	FQueuedChatMessage(const FString& InMsg, EChatBubblePriority InPri)
		: Message(InMsg), MsgPriority(InPri) {}
};

// ---------------------------------------------------------------------------
// EBubbleState — State machine for the chat bubble lifecycle
// ---------------------------------------------------------------------------
UENUM()
enum class EBubbleState : uint8
{
	Hidden,
	FadingIn,
	Typing,
	Displaying,
	FadingOut
};

// ---------------------------------------------------------------------------
// UAoCChatBubble — The widget itself
// ---------------------------------------------------------------------------
UCLASS(meta = (DisableNativeTick = false))
class AOC_API UAoCChatBubble : public UUserWidget
{
	GENERATED_BODY()

public:
	UAoCChatBubble(const FObjectInitializer& ObjectInitializer);

	// ----- Public API (usable by any NPC / system) -------------------------

	/** Queue a message for display.  Priority controls insertion order. */
	UFUNCTION(BlueprintCallable, Category = "AoC|ChatBubble")
	void ShowMessage(const FString& Message,
	                 EChatBubblePriority InPriority = EChatBubblePriority::Normal);

	/** Immediately clear the current message and the entire queue. */
	UFUNCTION(BlueprintCallable, Category = "AoC|ChatBubble")
	void ClearAll();

	/** True if a message is currently being typed, held, or fading. */
	UFUNCTION(BlueprintPure, Category = "AoC|ChatBubble")
	bool IsDisplayingMessage() const;

	/** Returns the number of messages waiting in queue (excludes current). */
	UFUNCTION(BlueprintPure, Category = "AoC|ChatBubble")
	int32 GetQueueDepth() const;

	/** Set the NPC display name shown above the bubble (optional). */
	UFUNCTION(BlueprintCallable, Category = "AoC|ChatBubble")
	void SetSpeakerName(const FString& Name);

	// ----- Configuration (tweakable per-NPC if desired) --------------------

	/** Base delay between characters in seconds.  Default 0.04 (40 ms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Timing")
	float BaseCharDelay = 0.04f;

	/** Multiplier applied to delay after period / ! / ? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Timing")
	float SentenceEndDelayMultiplier = 5.0f;  // 0.04 * 5 = 0.2s

	/** Multiplier applied to delay after comma / semicolon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Timing")
	float ClauseDelayMultiplier = 2.5f;  // 0.04 * 2.5 = 0.1s

	/** Fixed delay for "..." ellipsis in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Timing")
	float EllipsisPause = 0.4f;

	/** Fade-in duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Timing")
	float FadeInDuration = 0.2f;

	/** Fade-out duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Timing")
	float FadeOutDuration = 0.5f;

	/** Maximum messages in the queue (oldest normal-priority dropped if full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Queue",
	          meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxQueueDepth = 5;

	/** Background colour — default #1a1a2eCC */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Visual")
	FLinearColor BackgroundColor = FLinearColor(0.102f, 0.102f, 0.180f, 0.80f);

	/** Border colour — default #dc143c (crimson) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Visual")
	FLinearColor BorderColor = FLinearColor(0.863f, 0.078f, 0.235f, 1.0f);

	/** Text colour — default white */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Visual")
	FLinearColor TextColor = FLinearColor::White;

	/** Font size in points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Visual",
	          meta = (ClampMin = "8", ClampMax = "32"))
	int32 FontSize = 15;

	/** Maximum width of the bubble in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|ChatBubble|Visual")
	float MaxBubbleWidth = 300.0f;

	// ----- Delegate (optional — lets other systems know when a message finishes)
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageFinished, const FString&, Message);

	UPROPERTY(BlueprintAssignable, Category = "AoC|ChatBubble")
	FOnMessageFinished OnMessageFinished;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQueueEmpty);

	UPROPERTY(BlueprintAssignable, Category = "AoC|ChatBubble")
	FOnQueueEmpty OnQueueEmpty;

protected:
	// ----- UUserWidget overrides -------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	// ----- Widget tree (built in C++) --------------------------------------
	UPROPERTY()
	UCanvasPanel* RootCanvas = nullptr;

	UPROPERTY()
	USizeBox* SizeConstraint = nullptr;

	UPROPERTY()
	UBorder* BackgroundBorder = nullptr;

	UPROPERTY()
	UBorder* InnerPadding = nullptr;

	UPROPERTY()
	UVerticalBox* ContentBox = nullptr;

	UPROPERTY()
	UTextBlock* SpeakerNameText = nullptr;

	UPROPERTY()
	UTextBlock* MessageText = nullptr;

	// ----- State -----------------------------------------------------------
	EBubbleState CurrentState = EBubbleState::Hidden;

	FString FullMessage;          // Complete message being typed out
	FString DisplayedText;        // Currently visible portion
	int32   CurrentCharIndex = 0; // Next character to reveal
	float   CharTimer   = 0.0f;  // Accumulator for typewriter
	float   DisplayTimer = 0.0f; // Hold timer after typing completes
	float   FadeTimer    = 0.0f; // Progress through fade-in or fade-out
	float   CurrentOpacity = 0.0f;

	FString SpeakerName;          // Optional NPC name above text
	bool    bEllipsisPending = false; // Waiting on an ellipsis pause
	int32   EllipsisDotCount = 0;     // Tracks consecutive dots

	// ----- Queue -----------------------------------------------------------
	UPROPERTY()
	TArray<FQueuedChatMessage> MessageQueue;

	// ----- Internal helpers ------------------------------------------------

	/** Build the entire UMG widget tree from C++ (no Blueprint asset). */
	void BuildWidgetLayout();

	/** Get the appropriate delay after revealing the character at Index. */
	float GetCharDelay(int32 CharIndex) const;

	/** Calculate how long a completed message should remain visible. */
	float GetDisplayDuration(const FString& Msg) const;

	/** Dequeue the next message and begin the FadingIn state. */
	void AdvanceToNextMessage();

	/** Transition to a new state, resetting timers as needed. */
	void SetBubbleState(EBubbleState NewState);

	/** Apply current opacity to all visual elements. */
	void ApplyOpacity(float Opacity);

	/** Insert a message into the queue respecting priority ordering. */
	void EnqueueMessage(const FString& Msg, EChatBubblePriority InPriority);

	/** Drop the oldest Normal-priority message from the queue.
	    Returns true if something was dropped. */
	bool DropOldestNormal();

	// ----- Tick sub-routines per state ------------------------------------
	void TickFadingIn(float DeltaTime);
	void TickTyping(float DeltaTime);
	void TickDisplaying(float DeltaTime);
	void TickFadingOut(float DeltaTime);
};
