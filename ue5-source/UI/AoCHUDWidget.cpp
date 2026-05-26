// AoCHUDWidget.cpp — Full C++ HUD widget implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCHUDWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"

// ---------------------------------------------------------------------------
// Helper: add widget to canvas with anchor/position
// ---------------------------------------------------------------------------

void UAoCHUDWidget::AddToCanvas(UWidget* Widget, FVector2D Alignment, FVector2D Position, FVector2D Size, bool bAutoSize)
{
	if (!RootCanvas || !Widget) return;

	UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Widget);
	if (!Slot) return;

	// Anchors = Alignment (e.g. 0,0 = top-left; 1,0 = top-right; 0.5,1 = bottom-center)
	Slot->SetAnchors(FAnchors(Alignment.X, Alignment.Y, Alignment.X, Alignment.Y));
	Slot->SetAlignment(Alignment);
	Slot->SetPosition(Position);
	Slot->SetAutoSize(bAutoSize);

	if (!bAutoSize && !Size.IsNearlyZero())
	{
		Slot->SetSize(Size);
	}
}

// ---------------------------------------------------------------------------
// Builder helpers
// ---------------------------------------------------------------------------

UProgressBar* UAoCHUDWidget::MakeBar(FLinearColor FillColor)
{
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
	Bar->SetFillColorAndOpacity(FillColor);
	Bar->SetPercent(1.f);

	// Style the bar background
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	FSlateBrush BgBrush;
	BgBrush.TintColor = FSlateColor(BarBg());
	Style.SetBackgroundImage(BgBrush);

	FSlateBrush FillBrush;
	FillBrush.TintColor = FSlateColor(FillColor);
	Style.SetFillImage(FillBrush);

	Bar->SetWidgetStyle(Style);
	return Bar;
}

UTextBlock* UAoCHUDWidget::MakeText(const FString& Initial, int32 FontSize, FLinearColor Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetText(FText::FromString(Initial));
	Text->SetColorAndOpacity(FSlateColor(Color));

	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);

	return Text;
}

UBorder* UAoCHUDWidget::MakeBorder(FLinearColor BgColor, float PadH, float PadV)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
	Border->SetBrushColor(BgColor);
	Border->SetPadding(FMargin(PadH, PadV));
	return Border;
}

// ---------------------------------------------------------------------------
// NativeConstruct — build the entire HUD
// ---------------------------------------------------------------------------

void UAoCHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// --- Root canvas ---
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	BuildPlayerBars();
	BuildMinimap();
	BuildTargetFrame();
	BuildHotbar();
	BuildChatBox();
	BuildDeathScreen();
}

// ---------------------------------------------------------------------------
// Top-Left: Player stat bars
// ---------------------------------------------------------------------------

void UAoCHUDWidget::BuildPlayerBars()
{
	UBorder* BgBorder = MakeBorder(DarkBg(), 8.f, 6.f);

	PlayerBarsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	BgBorder->AddChild(PlayerBarsBox);

	// Helper lambda: create a bar row with overlay text
	auto MakeBarRow = [&](UProgressBar*& OutBar, UTextBlock*& OutText, FLinearColor Color,
		const FString& Label, float BarHeight) -> UWidget*
	{
		USizeBox* SizeB = WidgetTree->ConstructWidget<USizeBox>();
		SizeB->SetWidthOverride(220.f);
		SizeB->SetHeightOverride(BarHeight);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		SizeB->AddChild(Overlay);

		OutBar = MakeBar(Color);
		Overlay->AddChild(OutBar);

		OutText = MakeText(Label, 11, BoneText());
		OutText->SetJustification(ETextJustify::Center);
		Overlay->AddChild(OutText);

		return SizeB;
	};

	// HP
	{
		UWidget* Row = MakeBarRow(HPBar, HPText, HPRed(), TEXT("100 / 100"), 22.f);
		UVerticalBoxSlot* S = PlayerBarsBox->AddChildToVerticalBox(Row);
		S->SetPadding(FMargin(0, 0, 0, 3));
	}
	// MP
	{
		UWidget* Row = MakeBarRow(MPBar, MPText, MPBlue(), TEXT("50 / 50"), 20.f);
		UVerticalBoxSlot* S = PlayerBarsBox->AddChildToVerticalBox(Row);
		S->SetPadding(FMargin(0, 0, 0, 3));
	}
	// Stamina
	{
		UWidget* Row = MakeBarRow(StaminaBar, StaminaText, StamGreen(), TEXT("100 / 100"), 18.f);
		UVerticalBoxSlot* S = PlayerBarsBox->AddChildToVerticalBox(Row);
		S->SetPadding(FMargin(0, 0, 0, 3));
	}
	// XP (thinner)
	{
		UWidget* Row = MakeBarRow(XPBar, XPText, XPGold(), TEXT("0 / 1000"), 12.f);
		UVerticalBoxSlot* S = PlayerBarsBox->AddChildToVerticalBox(Row);
		S->SetPadding(FMargin(0, 0, 0, 3));
	}
	// Gold text
	{
		GoldText = MakeText(FString::Printf(TEXT("Gold: %d"), 0), 13, XPGold());
		PlayerBarsBox->AddChildToVerticalBox(GoldText);
	}

	// Place at top-left
	AddToCanvas(BgBorder, FVector2D(0, 0), FVector2D(15, 15));
}

// ---------------------------------------------------------------------------
// Top-Right: Minimap placeholder
// ---------------------------------------------------------------------------

void UAoCHUDWidget::BuildMinimap()
{
	UBorder* BgBorder = MakeBorder(DarkBg(), 8.f, 6.f);

	MinimapBox = WidgetTree->ConstructWidget<UVerticalBox>();
	BgBorder->AddChild(MinimapBox);

	// Circular frame placeholder (just a bordered square for now)
	MinimapFrame = MakeBorder(FLinearColor(0.15f, 0.15f, 0.25f, 0.9f), 0.f, 0.f);

	USizeBox* MapSize = WidgetTree->ConstructWidget<USizeBox>();
	MapSize->SetWidthOverride(160.f);
	MapSize->SetHeightOverride(160.f);
	MapSize->AddChild(MinimapFrame);

	// Add "Map" placeholder text inside
	UTextBlock* MapPlaceholder = MakeText(TEXT("[Minimap]"), 12, FLinearColor(0.5f, 0.5f, 0.5f, 0.6f));
	MapPlaceholder->SetJustification(ETextJustify::Center);
	MinimapFrame->AddChild(MapPlaceholder);

	UVerticalBoxSlot* MapSlot = MinimapBox->AddChildToVerticalBox(MapSize);
	MapSlot->SetPadding(FMargin(0, 0, 0, 4));

	// Zone name
	ZoneNameText = MakeText(TEXT("Unknown Zone"), 12, BoneText());
	ZoneNameText->SetJustification(ETextJustify::Center);
	MinimapBox->AddChildToVerticalBox(ZoneNameText);

	// Coordinates
	CoordinatesText = MakeText(TEXT("X: 0  Y: 0"), 10, FLinearColor(0.6f, 0.6f, 0.6f, 1.f));
	CoordinatesText->SetJustification(ETextJustify::Center);
	MinimapBox->AddChildToVerticalBox(CoordinatesText);

	// Place at top-right
	AddToCanvas(BgBorder, FVector2D(1, 0), FVector2D(-15, 15));
}

// ---------------------------------------------------------------------------
// Top-Center: Target frame
// ---------------------------------------------------------------------------

void UAoCHUDWidget::BuildTargetFrame()
{
	UBorder* BgBorder = MakeBorder(DarkBg(), 10.f, 6.f);

	TargetFrameBox = WidgetTree->ConstructWidget<UVerticalBox>();
	BgBorder->AddChild(TargetFrameBox);

	// Target name
	TargetNameText = MakeText(TEXT("Target"), 14, CrimsonAccent());
	TargetNameText->SetJustification(ETextJustify::Center);
	TargetFrameBox->AddChildToVerticalBox(TargetNameText);

	// Target HP bar
	{
		USizeBox* SizeB = WidgetTree->ConstructWidget<USizeBox>();
		SizeB->SetWidthOverride(200.f);
		SizeB->SetHeightOverride(18.f);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		SizeB->AddChild(Overlay);

		TargetHPBar = MakeBar(CrimsonAccent());
		Overlay->AddChild(TargetHPBar);

		UVerticalBoxSlot* S = TargetFrameBox->AddChildToVerticalBox(SizeB);
		S->SetPadding(FMargin(0, 3, 0, 3));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// Distance text
	TargetDistText = MakeText(TEXT("0m"), 11, BoneText());
	TargetDistText->SetJustification(ETextJustify::Center);
	TargetFrameBox->AddChildToVerticalBox(TargetDistText);

	// Place at top-center
	AddToCanvas(BgBorder, FVector2D(0.5f, 0), FVector2D(0, 15));

	// Hidden by default
	BgBorder->SetVisibility(ESlateVisibility::Collapsed);
	// Store reference so we can show/hide the border
	TargetFrameBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// ---------------------------------------------------------------------------
// Bottom-Center: Hotbar (10 slots)
// ---------------------------------------------------------------------------

void UAoCHUDWidget::BuildHotbar()
{
	UBorder* BgBorder = MakeBorder(DarkBg(), 6.f, 6.f);

	HotbarBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	BgBorder->AddChild(HotbarBox);

	const FString SlotLabels[] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"),
		TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0") };

	HotbarSlots.Empty(10);
	HotbarLabels.Empty(10);

	for (int32 i = 0; i < 10; ++i)
	{
		// Each slot is a SizeBox → Border → VerticalBox (label + icon area)
		USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>();
		SlotSize->SetWidthOverride(60.f);
		SlotSize->SetHeightOverride(60.f);

		UBorder* SlotBorder = MakeBorder(FLinearColor(0.12f, 0.12f, 0.2f, 0.9f), 2.f, 2.f);
		SlotBorder->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.2f, 0.9f));
		SlotSize->AddChild(SlotBorder);

		UVerticalBox* SlotInner = WidgetTree->ConstructWidget<UVerticalBox>();
		SlotBorder->AddChild(SlotInner);

		// Key label at top-left
		UTextBlock* Label = MakeText(SlotLabels[i], 10, FLinearColor(0.7f, 0.7f, 0.7f, 0.8f));
		SlotInner->AddChildToVerticalBox(Label);

		HotbarSlots.Add(SlotBorder);
		HotbarLabels.Add(Label);

		UHorizontalBoxSlot* HSlot = HotbarBox->AddChildToHorizontalBox(SlotSize);
		HSlot->SetPadding(FMargin(2, 0));
	}

	// Place at bottom-center
	AddToCanvas(BgBorder, FVector2D(0.5f, 1.f), FVector2D(0, -15));
}

// ---------------------------------------------------------------------------
// Bottom-Left: Chat box
// ---------------------------------------------------------------------------

void UAoCHUDWidget::BuildChatBox()
{
	UBorder* BgBorder = MakeBorder(FLinearColor(0.08f, 0.08f, 0.14f, 0.75f), 6.f, 6.f);

	ChatBox = WidgetTree->ConstructWidget<UVerticalBox>();
	BgBorder->AddChild(ChatBox);

	// Tab row (static labels)
	{
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>();

		const FString Tabs[] = { TEXT("[Local]"), TEXT("[Global]"), TEXT("[System]") };
		for (const FString& TabName : Tabs)
		{
			UTextBlock* Tab = MakeText(TabName, 11, BoneText());
			UHorizontalBoxSlot* HS = TabRow->AddChildToHorizontalBox(Tab);
			HS->SetPadding(FMargin(0, 0, 10, 0));
		}

		UVerticalBoxSlot* VS = ChatBox->AddChildToVerticalBox(TabRow);
		VS->SetPadding(FMargin(0, 0, 0, 4));
	}

	// Scroll box for messages
	{
		USizeBox* ChatSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		ChatSizeBox->SetWidthOverride(320.f);
		ChatSizeBox->SetHeightOverride(140.f);

		ChatScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
		ChatSizeBox->AddChild(ChatScrollBox);

		// Add a welcome message
		UTextBlock* Welcome = MakeText(TEXT("[System] Welcome to Architect of Creation."), 10,
			FLinearColor(0.6f, 0.6f, 0.5f, 1.f));
		ChatScrollBox->AddChild(Welcome);

		UVerticalBoxSlot* VS = ChatBox->AddChildToVerticalBox(ChatSizeBox);
		VS->SetPadding(FMargin(0, 0, 0, 4));
	}

	// Input box
	{
		ChatInput = WidgetTree->ConstructWidget<UEditableTextBox>();
		ChatInput->SetHintText(FText::FromString(TEXT("Type here...")));

		FEditableTextBoxStyle InputStyle = ChatInput->GetWidgetStyle();
		// Dark background for input
		FSlateBrush InputBg;
		InputBg.TintColor = FSlateColor(FLinearColor(0.06f, 0.06f, 0.1f, 0.9f));
		InputStyle.SetBackgroundImageNormal(InputBg);
		InputStyle.SetBackgroundImageFocused(InputBg);
		InputStyle.SetBackgroundImageHovered(InputBg);
		ChatInput->SetWidgetStyle(InputStyle);

		ChatBox->AddChildToVerticalBox(ChatInput);
	}

	// Place at bottom-left
	AddToCanvas(BgBorder, FVector2D(0, 1), FVector2D(15, -90));
}

// ---------------------------------------------------------------------------
// Center: Death screen
// ---------------------------------------------------------------------------

void UAoCHUDWidget::BuildDeathScreen()
{
	// Full-screen dark overlay
	DeathOverlay = WidgetTree->ConstructWidget<UOverlay>();

	UBorder* DarkOverlayBg = MakeBorder(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f);

	UVerticalBox* DeathContent = WidgetTree->ConstructWidget<UVerticalBox>();
	DeathContent->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	// "YOU HAVE DIED" title
	DeathTitleText = MakeText(TEXT("YOU HAVE DIED"), 36, CrimsonAccent());
	DeathTitleText->SetJustification(ETextJustify::Center);
	{
		UVerticalBoxSlot* VS = DeathContent->AddChildToVerticalBox(DeathTitleText);
		VS->SetHorizontalAlignment(HAlign_Center);
		VS->SetPadding(FMargin(0, 0, 0, 20));
	}

	// Timer text
	DeathTimerText = MakeText(TEXT("Respawning in 10..."), 16, BoneText());
	DeathTimerText->SetJustification(ETextJustify::Center);
	{
		UVerticalBoxSlot* VS = DeathContent->AddChildToVerticalBox(DeathTimerText);
		VS->SetHorizontalAlignment(HAlign_Center);
		VS->SetPadding(FMargin(0, 0, 0, 15));
	}

	// Respawn button
	RespawnButton = WidgetTree->ConstructWidget<UButton>();
	RespawnButtonLabel = MakeText(TEXT("Release Spirit"), 14, BoneText());
	RespawnButtonLabel->SetJustification(ETextJustify::Center);
	RespawnButton->AddChild(RespawnButtonLabel);

	// Style button background (dark with crimson hint)
	FButtonStyle BtnStyle = RespawnButton->GetWidgetStyle();
	FSlateBrush BtnBrush;
	BtnBrush.TintColor = FSlateColor(FLinearColor(0.3f, 0.05f, 0.08f, 0.9f));
	BtnStyle.SetNormal(BtnBrush);
	FSlateBrush BtnHoverBrush;
	BtnHoverBrush.TintColor = FSlateColor(CrimsonAccent());
	BtnStyle.SetHovered(BtnHoverBrush);
	BtnStyle.SetPressed(BtnBrush);
	RespawnButton->SetStyle(BtnStyle);

	{
		UVerticalBoxSlot* VS = DeathContent->AddChildToVerticalBox(RespawnButton);
		VS->SetHorizontalAlignment(HAlign_Center);
	}

	DarkOverlayBg->AddChild(DeathContent);
	DeathOverlay->AddChild(DarkOverlayBg);

	// Add to canvas as full-screen
	UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(DeathOverlay);
	if (Slot)
	{
		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0, 0, 0, 0));
	}

	// Hidden by default
	DeathOverlay->SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Tick — auto-update from player pawn if available
// ---------------------------------------------------------------------------

void UAoCHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// In a full implementation, you'd get the player pawn here and read stats.
	// For now the external code calls UpdatePlayerStats() directly.
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UAoCHUDWidget::ShowDeathScreen()
{
	if (DeathOverlay)
	{
		DeathOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAoCHUDWidget::HideDeathScreen()
{
	if (DeathOverlay)
	{
		DeathOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAoCHUDWidget::ShowTargetFrame(const FString& TargetName, float HealthPercent, float Distance)
{
	if (TargetFrameBox)
	{
		// Show the parent border
		if (UWidget* Parent = TargetFrameBox->GetParent())
		{
			Parent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	if (TargetNameText)  TargetNameText->SetText(FText::FromString(TargetName));
	if (TargetHPBar)     TargetHPBar->SetPercent(FMath::Clamp(HealthPercent, 0.f, 1.f));
	if (TargetDistText)  TargetDistText->SetText(FText::FromString(FString::Printf(TEXT("%.0f m"), Distance / 100.f)));
}

void UAoCHUDWidget::HideTargetFrame()
{
	if (TargetFrameBox)
	{
		if (UWidget* Parent = TargetFrameBox->GetParent())
		{
			Parent->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UAoCHUDWidget::AddChatMessage(const FString& Channel, const FString& Message)
{
	if (!ChatScrollBox) return;

	const FString Formatted = FString::Printf(TEXT("[%s] %s"), *Channel, *Message);
	UTextBlock* Msg = MakeText(Formatted, 10, BoneText());
	ChatScrollBox->AddChild(Msg);
	ChatScrollBox->ScrollToEnd();
}

void UAoCHUDWidget::UpdatePlayerStats(float HP, float MaxHP, float MP, float MaxMP,
	float Stamina, float MaxStamina, float XP, float MaxXP, int32 Gold)
{
	if (HPBar && HPText)
	{
		HPBar->SetPercent(MaxHP > 0 ? HP / MaxHP : 0.f);
		HPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), HP, MaxHP)));
	}
	if (MPBar && MPText)
	{
		MPBar->SetPercent(MaxMP > 0 ? MP / MaxMP : 0.f);
		MPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), MP, MaxMP)));
	}
	if (StaminaBar && StaminaText)
	{
		StaminaBar->SetPercent(MaxStamina > 0 ? Stamina / MaxStamina : 0.f);
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Stamina, MaxStamina)));
	}
	if (XPBar && XPText)
	{
		XPBar->SetPercent(MaxXP > 0 ? XP / MaxXP : 0.f);
		XPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), XP, MaxXP)));
	}
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("Gold: %d"), Gold)));
	}
}

void UAoCHUDWidget::UpdateMinimap(const FString& ZoneName, float X, float Y)
{
	if (ZoneNameText)
	{
		ZoneNameText->SetText(FText::FromString(ZoneName));
	}
	if (CoordinatesText)
	{
		CoordinatesText->SetText(FText::FromString(FString::Printf(TEXT("X: %.0f  Y: %.0f"), X, Y)));
	}
}

void UAoCHUDWidget::UpdateTargetStats(float HealthPercent, float Distance)
{
	if (TargetHPBar) TargetHPBar->SetPercent(FMath::Clamp(HealthPercent, 0.f, 1.f));
	if (TargetDistText) TargetDistText->SetText(FText::FromString(FString::Printf(TEXT("%.0f m"), Distance / 100.f)));
}

void UAoCHUDWidget::SetRespawnTimer(float SecondsRemaining)
{
	if (DeathTimerText)
	{
		DeathTimerText->SetText(FText::FromString(
			FString::Printf(TEXT("Respawning in %.0f..."), FMath::Max(0.f, SecondsRemaining))));
	}
}
