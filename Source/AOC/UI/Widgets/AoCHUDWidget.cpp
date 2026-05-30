// AoCHUDWidget.cpp — Darkfall Unholy Wars–style HUD implementation
// Architect of Creation
#include "UI/Widgets/AoCHUDWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

#include "AoCPlayerPawn.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  Darkfall-style colour palette
// ═══════════════════════════════════════════════════════════════════════════════
namespace AoCUI
{
	// Text
	static const FLinearColor Parchment  (0.91f, 0.86f, 0.78f, 1.f);
	static const FLinearColor Gold       (0.83f, 0.67f, 0.31f, 1.f);
	static const FLinearColor Muted      (0.55f, 0.50f, 0.44f, 1.f);
	static const FLinearColor White      (0.95f, 0.93f, 0.90f, 1.f);

	// Panels
	static const FLinearColor PanelDark  (0.035f, 0.030f, 0.025f, 0.88f);
	static const FLinearColor PanelMed   (0.06f, 0.055f, 0.045f, 0.82f);
	static const FLinearColor PanelBorder(0.55f, 0.45f, 0.22f, 0.45f);

	// Bars
	static const FLinearColor HealthFill (0.72f, 0.10f, 0.10f, 1.f);
	static const FLinearColor HealthBg   (0.18f, 0.04f, 0.04f, 0.8f);
	static const FLinearColor ManaFill   (0.12f, 0.22f, 0.72f, 1.f);
	static const FLinearColor ManaBg     (0.04f, 0.06f, 0.18f, 0.8f);
	static const FLinearColor StaminaFill(0.72f, 0.62f, 0.12f, 1.f);
	static const FLinearColor StaminaBg  (0.18f, 0.15f, 0.04f, 0.8f);

	// Misc
	static const FLinearColor Danger     (0.85f, 0.18f, 0.12f, 1.f);
	static const FLinearColor CrosshairC (0.90f, 0.85f, 0.70f, 0.70f);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

void UAoCHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildHUD();
}

void UAoCHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// ── Acquire owner pawn ──
	if (!OwnerPawn.IsValid())
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			OwnerPawn = Cast<AAoCPlayerPawn>(PC->GetPawn());
		}
	}

	// ── Update vitals (read from pawn's combat component when available) ──
	// For now these stay at defaults; as CombatComponent integration lands
	// we'll pull real values here.
	if (OwnerPawn.IsValid())
	{
		// Future: read from OwnerPawn->CombatComponent
	}

	// ── Fade & remove expired notifications ──
	for (int32 i = ActiveNotifications.Num() - 1; i >= 0; --i)
	{
		FAoCNotification& N = ActiveNotifications[i];
		N.TimeRemaining -= InDeltaTime;

		if (N.TimeRemaining <= 0.f)
		{
			if (N.TextWidget)
			{
				N.TextWidget->RemoveFromParent();
			}
			ActiveNotifications.RemoveAt(i);
		}
		else if (N.TimeRemaining < 1.5f)
		{
			// Fade out in last 1.5 seconds
			float Alpha = N.TimeRemaining / 1.5f;
			if (N.TextWidget)
			{
				FLinearColor C = N.TextWidget->GetColorAndOpacity().GetSpecifiedColor();
				C.A = Alpha;
				N.TextWidget->SetColorAndOpacity(FSlateColor(C));
			}
		}
	}

	// ── Update context menus & key hints based on pawn mode ──
	if (OwnerPawn.IsValid())
	{
		AAoCPlayerPawn* P = OwnerPawn.Get();

		// Auto-show/hide context menus when mode changes
		bool bTF = (P->CurrentMode == EInteractionMode::TerraformMenu);
		bool bTN = (P->CurrentMode == EInteractionMode::MiningMenu);

		if (bTF != bLastTerraformShown || bTN != bLastTunnelShown)
		{
			bLastTerraformShown = bTF;
			bLastTunnelShown = bTN;

			if (bTF)
			{
				TArray<FString> Opts;
				Opts.Add(TEXT("[1] Raise Ground"));
				Opts.Add(TEXT("[2] Lower Ground"));
				Opts.Add(TEXT("[3] Flatten"));
				Opts.Add(TEXT("[4] Slope Upward"));
				Opts.Add(TEXT("[5] Slope Downward"));
				Opts.Add(TEXT("[Esc] Cancel"));
				ShowContextMenu(TEXT("TERRAFORM"), Opts);
			}
			else if (bTN)
			{
				TArray<FString> Opts;
				Opts.Add(TEXT("[1] Tunnel Forward"));
				Opts.Add(TEXT("[2] Tunnel Down"));
				Opts.Add(TEXT("[3] Tunnel Up"));
				Opts.Add(TEXT("[4] Reinforce"));
				Opts.Add(TEXT("[5] Observe"));
				Opts.Add(TEXT("[Esc] Cancel"));
				ShowContextMenu(TEXT("TUNNEL"), Opts);
			}
			else
			{
				HideContextMenu();
			}
		}

		// Key hints
		if (KeyHintText)
		{
			FString Hint;
			if (bTF)
			{
				Hint = TEXT("[1-5] Select Action   [T/Esc] Cancel");
			}
			else if (bTN)
			{
				Hint = TEXT("[1-5] Select Action   [M/Esc] Cancel");
			}
			else
			{
				Hint = TEXT("[E] Interact   [G] Ground   [R] Prospect   [T] Terraform   [M] Tunnel   [I] Inventory   [Tab] Skills");
			}
			KeyHintText->SetText(FText::FromString(Hint));
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Factory helpers
// ═══════════════════════════════════════════════════════════════════════════════

UTextBlock* UAoCHUDWidget::MakeText(const FString& Content, int32 FontSize,
	FLinearColor Color, ETextJustify::Type Justify)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
	T->SetText(FText::FromString(Content));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetJustification(Justify);

	FSlateFontInfo Font = T->GetFont();
	Font.Size = FontSize;
	T->SetFont(Font);

	if (Justify == ETextJustify::Center)
	{
		T->SetAutoWrapText(false);
	}

	return T;
}

UBorder* UAoCHUDWidget::MakePanel(FLinearColor BgColor)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>();
	B->SetBrushColor(BgColor);
	B->SetPadding(FMargin(10.f, 6.f, 10.f, 6.f));
	return B;
}

UProgressBar* UAoCHUDWidget::MakeBar(FLinearColor FillColor, float InitPercent)
{
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
	Bar->SetPercent(InitPercent);
	Bar->SetFillColorAndOpacity(FillColor);
	return Bar;
}

UCanvasPanelSlot* UAoCHUDWidget::PlaceOnCanvas(UWidget* Widget,
	FVector2D AnchorMin, FVector2D AnchorMax, FVector2D Position,
	FVector2D Size, FVector2D Alignment, bool bAutoSize)
{
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Widget);
	PanelSlot->SetAnchors(FAnchors(AnchorMin.X, AnchorMin.Y, AnchorMax.X, AnchorMax.Y));
	PanelSlot->SetPosition(Position);
	PanelSlot->SetSize(Size);
	PanelSlot->SetAlignment(Alignment);
	PanelSlot->SetAutoSize(bAutoSize);
	return PanelSlot;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  BuildHUD — master layout
// ═══════════════════════════════════════════════════════════════════════════════

void UAoCHUDWidget::BuildHUD()
{
	if (!WidgetTree) return;

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = RootCanvas;

	BuildPlayerVitals();
	BuildCrosshair();
	BuildLookAtTooltip();
	BuildContextMenu();
	BuildNotificationArea();
	BuildHotbar();
	BuildInventoryPanel();
	BuildSkillsPanel();
	BuildDeathScreen();
	BuildKeyHints();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Player Vitals — top-center
//  ┌─────────────────────────────────────┐
//  │         « Player Name »             │
//  │  HP ████████████████████░░░  847/1000│
//  │  MP ██████████░░░░░░░░░░░░  312/500 │
//  │  ST ████████████████░░░░░░  420/500 │
//  └─────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildPlayerVitals()
{
	// Container panel
	VitalsPanel = MakePanel(AoCUI::PanelDark);
	VitalsPanel->SetPadding(FMargin(14.f, 8.f, 14.f, 8.f));

	// Inner vertical layout
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	VitalsPanel->SetContent(VBox);

	// Player name
	PlayerNameText = MakeText(TEXT("Adventurer"), 14, AoCUI::Gold, ETextJustify::Center);
	UVerticalBoxSlot* NameSlot = VBox->AddChildToVerticalBox(PlayerNameText);
	NameSlot->SetHorizontalAlignment(HAlign_Center);
	NameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	// ── Health row ──
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 2.f));

		HealthLabel = MakeText(TEXT("HP"), 10, AoCUI::HealthFill, ETextJustify::Left);
		UHorizontalBoxSlot* LblSlot = Row->AddChildToHorizontalBox(HealthLabel);
		LblSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		LblSlot->SetVerticalAlignment(VAlign_Center);

		// Bar in a size box
		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(220.f);
		SB->SetHeightOverride(14.f);
		HealthBar = MakeBar(AoCUI::HealthFill);
		SB->AddChild(HealthBar);
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(SB);
		BarSlot->SetVerticalAlignment(VAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

		HealthValueText = MakeText(TEXT("1000/1000"), 10, AoCUI::Parchment, ETextJustify::Right);
		UHorizontalBoxSlot* ValSlot = Row->AddChildToHorizontalBox(HealthValueText);
		ValSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ── Mana row ──
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 2.f));

		ManaLabel = MakeText(TEXT("MP"), 10, AoCUI::ManaFill, ETextJustify::Left);
		UHorizontalBoxSlot* LblSlot = Row->AddChildToHorizontalBox(ManaLabel);
		LblSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		LblSlot->SetVerticalAlignment(VAlign_Center);

		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(220.f);
		SB->SetHeightOverride(14.f);
		ManaBar = MakeBar(AoCUI::ManaFill);
		SB->AddChild(ManaBar);
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(SB);
		BarSlot->SetVerticalAlignment(VAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

		ManaValueText = MakeText(TEXT("500/500"), 10, AoCUI::Parchment, ETextJustify::Right);
		UHorizontalBoxSlot* ValSlot = Row->AddChildToHorizontalBox(ManaValueText);
		ValSlot->SetVerticalAlignment(VAlign_Center);
	}

	// ── Stamina row ──
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, 2.f));

		StaminaLabel = MakeText(TEXT("ST"), 10, AoCUI::StaminaFill, ETextJustify::Left);
		UHorizontalBoxSlot* LblSlot = Row->AddChildToHorizontalBox(StaminaLabel);
		LblSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		LblSlot->SetVerticalAlignment(VAlign_Center);

		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(220.f);
		SB->SetHeightOverride(14.f);
		StaminaBar = MakeBar(AoCUI::StaminaFill);
		SB->AddChild(StaminaBar);
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(SB);
		BarSlot->SetVerticalAlignment(VAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

		StaminaValueText = MakeText(TEXT("500/500"), 10, AoCUI::Parchment, ETextJustify::Right);
		UHorizontalBoxSlot* ValSlot = Row->AddChildToHorizontalBox(StaminaValueText);
		ValSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Place on canvas — top center
	PlaceOnCanvas(VitalsPanel,
		FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f),  // anchor top-center
		FVector2D(0.f, 12.f),                          // 12px from top
		FVector2D(360.f, 120.f),                       // size
		FVector2D(0.5f, 0.f),                          // pivot top-center
		true);                                          // auto-size
}

// ─────────────────────────────────────────────────────────────────────────────
//  Crosshair — small parchment dot at screen center
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildCrosshair()
{
	CrosshairDot = WidgetTree->ConstructWidget<UBorder>();
	CrosshairDot->SetBrushColor(AoCUI::CrosshairC);
	CrosshairDot->SetPadding(FMargin(0.f));

	// The dot itself is just the border background, sized explicitly
	PlaceOnCanvas(CrosshairDot,
		FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f),
		FVector2D(0.f, 0.f),
		FVector2D(4.f, 4.f),
		FVector2D(0.5f, 0.5f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Look-at tooltip — appears below crosshair when looking at an object
//  ┌────────────────────┐
//  │   Iron Ore Vein    │  ← gold name
//  │   Quality: 70      │  ← parchment description
//  └────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildLookAtTooltip()
{
	LookAtPanel = MakePanel(AoCUI::PanelDark);
	LookAtPanel->SetPadding(FMargin(12.f, 6.f, 12.f, 6.f));
	LookAtPanel->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	LookAtPanel->SetContent(VBox);

	LookAtNameText = MakeText(TEXT(""), 13, AoCUI::Gold, ETextJustify::Center);
	UVerticalBoxSlot* NS = VBox->AddChildToVerticalBox(LookAtNameText);
	NS->SetHorizontalAlignment(HAlign_Center);

	LookAtDescText = MakeText(TEXT(""), 10, AoCUI::Parchment, ETextJustify::Center);
	UVerticalBoxSlot* DS = VBox->AddChildToVerticalBox(LookAtDescText);
	DS->SetHorizontalAlignment(HAlign_Center);
	DS->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));

	// Place below crosshair
	PlaceOnCanvas(LookAtPanel,
		FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f),
		FVector2D(0.f, 30.f),   // 30px below center
		FVector2D(280.f, 50.f),
		FVector2D(0.5f, 0.f),   // pivot top-center
		true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Context menu — terraform / tunnel mode options
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildContextMenu()
{
	ContextMenuPanel = MakePanel(AoCUI::PanelDark);
	ContextMenuPanel->SetPadding(FMargin(14.f, 10.f, 14.f, 10.f));
	ContextMenuPanel->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	ContextMenuPanel->SetContent(VBox);

	ContextMenuTitle = MakeText(TEXT(""), 14, AoCUI::Gold, ETextJustify::Center);
	UVerticalBoxSlot* TS = VBox->AddChildToVerticalBox(ContextMenuTitle);
	TS->SetHorizontalAlignment(HAlign_Center);
	TS->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	// Thin separator line
	UBorder* Sep = WidgetTree->ConstructWidget<UBorder>();
	Sep->SetBrushColor(AoCUI::PanelBorder);
	Sep->SetPadding(FMargin(0.f));
	USizeBox* SepBox = WidgetTree->ConstructWidget<USizeBox>();
	SepBox->SetHeightOverride(1.f);
	SepBox->AddChild(Sep);
	UVerticalBoxSlot* SepSlot = VBox->AddChildToVerticalBox(SepBox);
	SepSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	ContextMenuItems = WidgetTree->ConstructWidget<UVerticalBox>();
	VBox->AddChildToVerticalBox(ContextMenuItems);

	// Place center-right
	PlaceOnCanvas(ContextMenuPanel,
		FVector2D(0.65f, 0.4f), FVector2D(0.65f, 0.4f),
		FVector2D(0.f, 0.f),
		FVector2D(240.f, 200.f),
		FVector2D(0.f, 0.f),
		true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Notification area — bottom-left, above hotbar
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildNotificationArea()
{
	NotificationBox = WidgetTree->ConstructWidget<UVerticalBox>();

	PlaceOnCanvas(NotificationBox,
		FVector2D(0.01f, 0.75f), FVector2D(0.01f, 0.75f),
		FVector2D(0.f, 0.f),
		FVector2D(400.f, 200.f),
		FVector2D(0.f, 1.f),
		true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hotbar — 10 slots, bottom-center
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildHotbar()
{
	HotbarPanel = MakePanel(FLinearColor(0.03f, 0.025f, 0.02f, 0.75f));
	HotbarPanel->SetPadding(FMargin(6.f, 4.f, 6.f, 4.f));

	HotbarRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	HotbarPanel->SetContent(HotbarRow);

	// 10 slots
	for (int32 i = 0; i < 10; ++i)
	{
		FString KeyLabel = (i < 9) ? FString::Printf(TEXT("%d"), i + 1) : TEXT("0");

		UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>();
		SlotBorder->SetBrushColor(FLinearColor(0.07f, 0.06f, 0.05f, 0.9f));
		SlotBorder->SetPadding(FMargin(2.f));

		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
		SB->SetWidthOverride(44.f);
		SB->SetHeightOverride(44.f);

		// Key number label in top-left of slot
		UTextBlock* KeyNum = MakeText(KeyLabel, 9, AoCUI::Muted, ETextJustify::Left);
		SlotBorder->SetContent(KeyNum);
		SB->AddChild(SlotBorder);

		UHorizontalBoxSlot* HSlot = HotbarRow->AddChildToHorizontalBox(SB);
		HSlot->SetPadding(FMargin(2.f, 0.f));
	}

	PlaceOnCanvas(HotbarPanel,
		FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f),
		FVector2D(0.f, -50.f),   // 50px from bottom
		FVector2D(520.f, 56.f),
		FVector2D(0.5f, 1.f),
		true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Inventory panel — dark grid panel, toggled with I
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildInventoryPanel()
{
	InventoryPanel = MakePanel(AoCUI::PanelDark);
	InventoryPanel->SetPadding(FMargin(16.f, 12.f, 16.f, 12.f));
	InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	InventoryPanel->SetContent(VBox);

	// Title
	InventoryTitle = MakeText(TEXT("INVENTORY"), 16, AoCUI::Gold, ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(InventoryTitle);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Separator
	UBorder* Sep = WidgetTree->ConstructWidget<UBorder>();
	Sep->SetBrushColor(AoCUI::PanelBorder);
	Sep->SetPadding(FMargin(0.f));
	USizeBox* SepBox = WidgetTree->ConstructWidget<USizeBox>();
	SepBox->SetHeightOverride(1.f);
	SepBox->AddChild(Sep);
	UVerticalBoxSlot* SepSlot = VBox->AddChildToVerticalBox(SepBox);
	SepSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Slot grid — 4 rows × 7 columns = 28 slots
	InventorySlots = WidgetTree->ConstructWidget<UVerticalBox>();
	VBox->AddChildToVerticalBox(InventorySlots);

	for (int32 Row = 0; Row < 4; ++Row)
	{
		UHorizontalBox* HRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RSlot = InventorySlots->AddChildToVerticalBox(HRow);
		RSlot->SetPadding(FMargin(0.f, 2.f));

		for (int32 Col = 0; Col < 7; ++Col)
		{
			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>();
			Cell->SetBrushColor(FLinearColor(0.07f, 0.06f, 0.05f, 0.9f));
			Cell->SetPadding(FMargin(2.f));

			USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>();
			SB->SetWidthOverride(48.f);
			SB->SetHeightOverride(48.f);
			SB->AddChild(Cell);

			UHorizontalBoxSlot* CSlot = HRow->AddChildToHorizontalBox(SB);
			CSlot->SetPadding(FMargin(2.f, 0.f));
		}
	}

	// Carry weight text
	UTextBlock* WeightText = MakeText(TEXT("Weight: 0 / 200"), 10, AoCUI::Muted, ETextJustify::Right);
	UVerticalBoxSlot* WSlot = VBox->AddChildToVerticalBox(WeightText);
	WSlot->SetHorizontalAlignment(HAlign_Right);
	WSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	// Place right-center
	PlaceOnCanvas(InventoryPanel,
		FVector2D(0.75f, 0.35f), FVector2D(0.75f, 0.35f),
		FVector2D(0.f, 0.f),
		FVector2D(420.f, 350.f),
		FVector2D(0.5f, 0.f),
		true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Skills panel — list with progress bars, toggled with Tab
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildSkillsPanel()
{
	SkillsPanel = MakePanel(AoCUI::PanelDark);
	SkillsPanel->SetPadding(FMargin(16.f, 12.f, 16.f, 12.f));
	SkillsPanel->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SkillsPanel->SetContent(VBox);

	// Title
	SkillsTitle = MakeText(TEXT("SKILLS"), 16, AoCUI::Gold, ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(SkillsTitle);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Separator
	UBorder* Sep = WidgetTree->ConstructWidget<UBorder>();
	Sep->SetBrushColor(AoCUI::PanelBorder);
	Sep->SetPadding(FMargin(0.f));
	USizeBox* SepBox = WidgetTree->ConstructWidget<USizeBox>();
	SepBox->SetHeightOverride(1.f);
	SepBox->AddChild(Sep);
	UVerticalBoxSlot* SepSlot = VBox->AddChildToVerticalBox(SepBox);
	SepSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	// Skill list
	SkillsList = WidgetTree->ConstructWidget<UVerticalBox>();
	VBox->AddChildToVerticalBox(SkillsList);

	// Populate skills — these are the core AoC skills
	const TArray<FString> SkillNames = {
		TEXT("Mining"), TEXT("Smelting"), TEXT("Blacksmithing"),
		TEXT("Woodcutting"), TEXT("Carpentry"), TEXT("Masonry"),
		TEXT("Farming"), TEXT("Herbalism"), TEXT("Alchemy"),
		TEXT("Combat"), TEXT("Archery"), TEXT("Magic")
	};

	for (const FString& Name : SkillNames)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RSlot = SkillsList->AddChildToVerticalBox(Row);
		RSlot->SetPadding(FMargin(0.f, 3.f));

		// Skill name
		UTextBlock* NameText = MakeText(Name, 11, AoCUI::Parchment, ETextJustify::Left);
		UHorizontalBoxSlot* NSlot = Row->AddChildToHorizontalBox(NameText);
		NSlot->SetVerticalAlignment(VAlign_Center);
		NSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// XP bar
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
		BarBox->SetWidthOverride(120.f);
		BarBox->SetHeightOverride(10.f);
		UProgressBar* XPBar = MakeBar(AoCUI::Gold, 0.0f);
		BarBox->AddChild(XPBar);
		UHorizontalBoxSlot* BSlot = Row->AddChildToHorizontalBox(BarBox);
		BSlot->SetVerticalAlignment(VAlign_Center);
		BSlot->SetPadding(FMargin(8.f, 0.f));

		// Level number
		UTextBlock* LvlText = MakeText(TEXT("1"), 11, AoCUI::Gold, ETextJustify::Right);
		UHorizontalBoxSlot* LSlot = Row->AddChildToHorizontalBox(LvlText);
		LSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Place left-center
	PlaceOnCanvas(SkillsPanel,
		FVector2D(0.25f, 0.35f), FVector2D(0.25f, 0.35f),
		FVector2D(0.f, 0.f),
		FVector2D(340.f, 420.f),
		FVector2D(0.5f, 0.f),
		true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Death screen — full overlay
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildDeathScreen()
{
	DeathOverlay = WidgetTree->ConstructWidget<UBorder>();
	DeathOverlay->SetBrushColor(FLinearColor(0.02f, 0.0f, 0.0f, 0.85f));
	DeathOverlay->SetPadding(FMargin(0.f));
	DeathOverlay->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
	DeathOverlay->SetContent(VBox);

	// "YOU HAVE FALLEN" text
	DeathText = MakeText(TEXT("YOU HAVE FALLEN"), 28, AoCUI::Danger, ETextJustify::Center);
	UVerticalBoxSlot* DSlot = VBox->AddChildToVerticalBox(DeathText);
	DSlot->SetHorizontalAlignment(HAlign_Center);
	DSlot->SetVerticalAlignment(VAlign_Center);
	DSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	DeathTimerText = MakeText(TEXT("Respawn in 10..."), 16, AoCUI::Parchment, ETextJustify::Center);
	UVerticalBoxSlot* TSlot = VBox->AddChildToVerticalBox(DeathTimerText);
	TSlot->SetHorizontalAlignment(HAlign_Center);
	TSlot->SetVerticalAlignment(VAlign_Center);

	// Full-screen stretch
	UCanvasPanelSlot* DeathSlot = RootCanvas->AddChildToCanvas(DeathOverlay);
	DeathSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	DeathSlot->SetOffsets(FMargin(0.f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Key hints — bottom center, above hotbar
// ─────────────────────────────────────────────────────────────────────────────

void UAoCHUDWidget::BuildKeyHints()
{
	KeyHintText = MakeText(
		TEXT("[E] Interact   [G] Ground   [R] Prospect   [T] Terraform   [M] Tunnel   [I] Inventory   [Tab] Skills"),
		9, AoCUI::Muted, ETextJustify::Center);

	PlaceOnCanvas(KeyHintText,
		FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f),
		FVector2D(0.f, -28.f),   // just above hotbar
		FVector2D(800.f, 20.f),
		FVector2D(0.5f, 1.f),
		true);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Public API implementations
// ═══════════════════════════════════════════════════════════════════════════════

void UAoCHUDWidget::ShowLookAtTooltip(const FString& Name, const FString& Description)
{
	if (LookAtPanel)
	{
		LookAtPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (LookAtNameText)
	{
		LookAtNameText->SetText(FText::FromString(Name));
	}
	if (LookAtDescText)
	{
		if (Description.IsEmpty())
		{
			LookAtDescText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			LookAtDescText->SetVisibility(ESlateVisibility::HitTestInvisible);
			LookAtDescText->SetText(FText::FromString(Description));
		}
	}
}

void UAoCHUDWidget::HideLookAtTooltip()
{
	if (LookAtPanel)
	{
		LookAtPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAoCHUDWidget::ShowContextMenu(const FString& Title, const TArray<FString>& Options)
{
	if (!ContextMenuPanel || !ContextMenuTitle || !ContextMenuItems) return;

	ContextMenuTitle->SetText(FText::FromString(Title));
	ContextMenuPanel->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Clear old items
	ContextMenuItems->ClearChildren();

	// Add new options
	for (const FString& Opt : Options)
	{
		UTextBlock* OptText = MakeText(Opt, 12, AoCUI::Parchment, ETextJustify::Left);
		UVerticalBoxSlot* OSlot = ContextMenuItems->AddChildToVerticalBox(OptText);
		OSlot->SetPadding(FMargin(0.f, 3.f));
	}
}

void UAoCHUDWidget::HideContextMenu()
{
	if (ContextMenuPanel)
	{
		ContextMenuPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAoCHUDWidget::ShowNotification(const FString& Message, FLinearColor Color)
{
	if (!NotificationBox || !WidgetTree) return;

	UTextBlock* Msg = MakeText(Message, 12, Color, ETextJustify::Left);

	// Add shadow effect — just make text bold-ish with outline color
	FSlateFontInfo Font = Msg->GetFont();
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
	Msg->SetFont(Font);

	UVerticalBoxSlot* MsgSlot = NotificationBox->AddChildToVerticalBox(Msg);
	MsgSlot->SetPadding(FMargin(0.f, 1.f));

	FAoCNotification Entry;
	Entry.TextWidget = Msg;
	Entry.TimeRemaining = 5.f;
	ActiveNotifications.Add(Entry);

	// Cap at 8 visible notifications
	while (ActiveNotifications.Num() > 8)
	{
		if (ActiveNotifications[0].TextWidget)
		{
			ActiveNotifications[0].TextWidget->RemoveFromParent();
		}
		ActiveNotifications.RemoveAt(0);
	}
}

void UAoCHUDWidget::ShowInventory()
{
	bInventoryOpen = true;
	if (InventoryPanel)
	{
		InventoryPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAoCHUDWidget::HideInventory()
{
	bInventoryOpen = false;
	if (InventoryPanel)
	{
		InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAoCHUDWidget::ToggleInventory()
{
	if (bInventoryOpen) HideInventory(); else ShowInventory();
}

void UAoCHUDWidget::ShowSkills()
{
	bSkillsOpen = true;
	if (SkillsPanel)
	{
		SkillsPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAoCHUDWidget::HideSkills()
{
	bSkillsOpen = false;
	if (SkillsPanel)
	{
		SkillsPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAoCHUDWidget::ToggleSkills()
{
	if (bSkillsOpen) HideSkills(); else ShowSkills();
}

void UAoCHUDWidget::ShowDeathScreen()
{
	if (DeathOverlay)
	{
		DeathOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UAoCHUDWidget::HideDeathScreen()
{
	if (DeathOverlay)
	{
		DeathOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAoCHUDWidget::SetRespawnTimer(float SecondsRemaining)
{
	if (DeathTimerText)
	{
		if (SecondsRemaining > 0.f)
		{
			FString Txt = FString::Printf(TEXT("Respawn in %d..."), FMath::CeilToInt(SecondsRemaining));
			DeathTimerText->SetText(FText::FromString(Txt));
		}
		else
		{
			DeathTimerText->SetText(FText::FromString(TEXT("Press [Space] to respawn")));
		}
	}
}
