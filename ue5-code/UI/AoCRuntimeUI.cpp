// AoCRuntimeUI.cpp — Architect of Creation Master HUD Widget v3
// Complete Darkfall-style moveable UI system
// Copyright 2024-2026 Architect of Creation. All rights reserved.

#include "AoCRuntimeUI.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"

// ============================================================
// TEMPLATE HELPER — Create any UWidget subclass on WidgetTree
// ============================================================
template<typename T>
T* UAoCRuntimeUI::CreateWidget(const FString& Name)
{
	if (!WidgetTree) return nullptr;
	return WidgetTree->ConstructWidget<T>(T::StaticClass(), FName(*Name));
}

// ============================================================
// AddToCanvas — add widget to canvas at absolute position
// This is THE safe method. Returns the slot for further config.
// ============================================================
UCanvasPanelSlot* UAoCRuntimeUI::AddToCanvas(UWidget* Widget, UCanvasPanel* Parent,
	float X, float Y, float W, float H)
{
	if (!Widget || !Parent) return nullptr;
	UCanvasPanelSlot* CanvasSlot = Parent->AddChildToCanvas(Widget);
	if (CanvasSlot)
	{
		CanvasSlot->SetPosition(FVector2D(X, Y));
		CanvasSlot->SetSize(FVector2D(W, H));
		CanvasSlot->SetAutoSize(false);
	}
	return CanvasSlot;
}

// ============================================================
// HELPER — MakeBar
// ============================================================
UProgressBar* UAoCRuntimeUI::MakeBar(UCanvasPanel* P, FLinearColor Fill,
	float X, float Y, float W, float H)
{
	UProgressBar* Bar = CreateWidget<UProgressBar>(
		FString::Printf(TEXT("Bar_%d_%d"), (int)X, (int)Y));
	if (!Bar) return nullptr;
	Bar->SetFillColorAndOpacity(Fill);
	Bar->SetPercent(1.0f);
	FProgressBarStyle Style;
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.05f, 0.02f, 0.02f, 0.8f));
	Bar->SetWidgetStyle(Style);
	AddToCanvas(Bar, P, X, Y, W, H);
	return Bar;
}

// ============================================================
// HELPER — MakeText
// ============================================================
UTextBlock* UAoCRuntimeUI::MakeText(UCanvasPanel* P, const FString& Txt,
	float X, float Y, float FontSz, FLinearColor Col)
{
	UTextBlock* T = CreateWidget<UTextBlock>(
		FString::Printf(TEXT("Txt_%s_%d"), *Txt.Left(8), (int)X));
	if (!T) return nullptr;
	T->SetText(FText::FromString(Txt));
	T->SetColorAndOpacity(FSlateColor(Col));
	FSlateFontInfo Font = T->GetFont();
	Font.Size = (int32)FontSz;
	T->SetFont(Font);
	AddToCanvas(T, P, X, Y, 200, FontSz + 6);
	return T;
}

// ============================================================
// HELPER — MakeImage
// ============================================================
UImage* UAoCRuntimeUI::MakeImage(UCanvasPanel* P, FLinearColor Tint,
	float X, float Y, float W, float H)
{
	UImage* Img = CreateWidget<UImage>(
		FString::Printf(TEXT("Img_%d_%d"), (int)X, (int)Y));
	if (!Img) return nullptr;
	Img->SetColorAndOpacity(Tint);
	AddToCanvas(Img, P, X, Y, W, H);
	return Img;
}

// ============================================================
// HELPER — MakeButton
// ============================================================
UButton* UAoCRuntimeUI::MakeButton(UCanvasPanel* P, const FString& Label,
	float X, float Y, float W, float H, FLinearColor BgCol)
{
	UButton* Btn = CreateWidget<UButton>(
		FString::Printf(TEXT("Btn_%s"), *Label.Left(12)));
	if (!Btn) return nullptr;

	FButtonStyle Style;
	FSlateBrush NormalBrush;
	NormalBrush.TintColor = FSlateColor(BgCol);
	Style.SetNormal(NormalBrush);
	FSlateBrush HoverBrush;
	HoverBrush.TintColor = FSlateColor(Crimson());
	Style.SetHovered(HoverBrush);
	FSlateBrush PressBrush;
	PressBrush.TintColor = FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f, 1.f));
	Style.SetPressed(PressBrush);
	Btn->SetStyle(Style);

	AddToCanvas(Btn, P, X, Y, W, H);

	// Add label text as child of button
	UTextBlock* BtnText = CreateWidget<UTextBlock>(
		FString::Printf(TEXT("BtnTxt_%s"), *Label.Left(12)));
	if (BtnText)
	{
		BtnText->SetText(FText::FromString(Label));
		BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo BtnFont = BtnText->GetFont();
		BtnFont.Size = 11;
		BtnText->SetFont(BtnFont);
		BtnText->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnText);
	}
	return Btn;
}

// ============================================================
// HELPER — MakeBorder
// ============================================================
UBorder* UAoCRuntimeUI::MakeBorder(UCanvasPanel* P, FLinearColor BgCol,
	float X, float Y, float W, float H)
{
	UBorder* B = CreateWidget<UBorder>(
		FString::Printf(TEXT("Brd_%d_%d"), (int)X, (int)Y));
	if (!B) return nullptr;
	B->SetBrushColor(BgCol);
	AddToCanvas(B, P, X, Y, W, H);
	return B;
}

// ============================================================
// HELPER — MakePanel (sub-canvas)
// ============================================================
UCanvasPanel* UAoCRuntimeUI::MakePanel(UCanvasPanel* P, const FString& Name,
	float X, float Y, float W, float H)
{
	UCanvasPanel* Sub = CreateWidget<UCanvasPanel>(Name);
	if (!Sub) return nullptr;
	AddToCanvas(Sub, P, X, Y, W, H);
	return Sub;
}

// ============================================================
// DEFAULT PANEL LAYOUTS
// ============================================================
void UAoCRuntimeUI::SetupDefaultLayouts()
{
	// Viewport is assumed 1920×1080 for defaults
	PanelLayouts.Add(TEXT("HUDBars"), {FVector2D(20, 20), FVector2D(240, 90), 1.f, true, true});
	PanelLayouts.Add(TEXT("Target"), {FVector2D(800, 20), FVector2D(220, 70), 1.f, true, true});
	PanelLayouts.Add(TEXT("Minimap"), {FVector2D(1700, 20), FVector2D(200, 200), 1.f, true, true});
	PanelLayouts.Add(TEXT("Hotbar"), {FVector2D(660, 1020), FVector2D(600, 52), 1.f, true, true});
	PanelLayouts.Add(TEXT("Chat"), {FVector2D(20, 800), FVector2D(420, 220), 0.85f, true, true});
	PanelLayouts.Add(TEXT("XPBar"), {FVector2D(0, 1068), FVector2D(1920, 12), 1.f, true, true});
	PanelLayouts.Add(TEXT("Inventory"), {FVector2D(700, 150), FVector2D(420, 550), 0.95f, false, false});
	PanelLayouts.Add(TEXT("SpellBook"), {FVector2D(750, 150), FVector2D(520, 500), 0.95f, false, false});
	PanelLayouts.Add(TEXT("CharSheet"), {FVector2D(650, 150), FVector2D(380, 550), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Crafting"), {FVector2D(700, 180), FVector2D(480, 450), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Pause"), {FVector2D(760, 250), FVector2D(400, 500), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Quest"), {FVector2D(1400, 150), FVector2D(380, 450), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Map"), {FVector2D(200, 80), FVector2D(1520, 900), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Clan"), {FVector2D(700, 200), FVector2D(420, 400), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Settings"), {FVector2D(600, 150), FVector2D(720, 500), 0.95f, false, false});
	PanelLayouts.Add(TEXT("Dialogue"), {FVector2D(560, 780), FVector2D(800, 200), 1.f, true, true});
	PanelLayouts.Add(TEXT("Loot"), {FVector2D(800, 350), FVector2D(320, 350), 0.95f, true, true});
	PanelLayouts.Add(TEXT("Death"), {FVector2D(610, 350), FVector2D(700, 300), 1.f, true, false});
	PanelLayouts.Add(TEXT("Party"), {FVector2D(20, 130), FVector2D(180, 250), 0.9f, true, true});
	PanelLayouts.Add(TEXT("SkillToast"), {FVector2D(700, 900), FVector2D(520, 40), 1.f, true, true});
}

// ============================================================
// NATIVE ON INITIALIZED — MASTER BUILD
// ============================================================
void UAoCRuntimeUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Initialize hotbar data: 10 columns × 10 slots = 100
	HotbarData.SetNum(100);
	ActiveHotbarColumn = 0;

	// Chat filters — all enabled by default
	ChatFilters.SetNum(8);
	for (int i = 0; i < 8; i++) ChatFilters[i] = true;

	SetupDefaultLayouts();
	BuildRootCanvas();
	BuildHUDBars();
	BuildTargetFrame();
	BuildMinimap();
	BuildHotbar();
	BuildChatPanel();
	BuildInventoryPanel();
	BuildSpellBookPanel();
	BuildCharSheetPanel();
	BuildCraftingPanel();
	BuildPauseMenuPanel();
	BuildDialoguePanel();
	BuildLootWindow();
	BuildDeathScreen();
	BuildPartyFrames();
	BuildQuestPanel();
	BuildMapPanel();
	BuildClanPanel();
	BuildSettingsPanel();
	BuildSkillToast();

	// Set initial focus mode
	SetIsFocusable(true);
}

// ============================================================
// BUILD ROOT CANVAS
// ============================================================
void UAoCRuntimeUI::BuildRootCanvas()
{
	RootCanvas = CreateWidget<UCanvasPanel>(TEXT("RootCanvas"));
	if (!RootCanvas || !WidgetTree) return;
	WidgetTree->RootWidget = RootCanvas;
}

// ============================================================
// BUILD HUD BARS — HP, MP, Stamina (top-left)
// ============================================================
void UAoCRuntimeUI::BuildHUDBars()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("HUDBars")];

	HUDBarsPanel = MakePanel(RootCanvas, TEXT("HUDBarsPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!HUDBarsPanel) return;

	// Dark background
	MakeBorder(HUDBarsPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);

	// Title bar (drag handle)
	MakeBorder(HUDBarsPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.6f), 0, 0, L.Size.X, 3);

	// HP Bar
	HPBar = MakeBar(HUDBarsPanel, BarRed(), 8, 10, 160, 18);
	HPText = MakeText(HUDBarsPanel, TEXT("500 / 500"), 172, 10, 10.f, FLinearColor::White);

	// MP Bar
	MPBar = MakeBar(HUDBarsPanel, BarBlue(), 8, 32, 160, 18);
	MPText = MakeText(HUDBarsPanel, TEXT("300 / 300"), 172, 32, 10.f, FLinearColor::White);

	// Stamina Bar
	StaminaBar = MakeBar(HUDBarsPanel, BarYellow(), 8, 54, 160, 18);
	StaminaText = MakeText(HUDBarsPanel, TEXT("200 / 200"), 172, 54, 10.f, FLinearColor::White);

	// Gold display
	GoldText = MakeText(HUDBarsPanel, TEXT("Gold: 0"), 8, 76, 10.f, FLinearColor(1.f, 0.84f, 0.f, 1.f));

	// XP Bar (full width, bottom of screen)
	const FPanelLayout& XPL = PanelLayouts[TEXT("XPBar")];
	XPBar = MakeBar(RootCanvas, BarGreen(), XPL.Position.X, XPL.Position.Y, XPL.Size.X, XPL.Size.Y);
	XPText = MakeText(RootCanvas, TEXT("XP"), XPL.Position.X + 900, XPL.Position.Y - 2, 8.f, FLinearColor::White);
}

// ============================================================
// BUILD TARGET FRAME
// ============================================================
void UAoCRuntimeUI::BuildTargetFrame()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Target")];

	TargetPanel = MakePanel(RootCanvas, TEXT("TargetPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!TargetPanel) return;

	MakeBorder(TargetPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(TargetPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.6f), 0, 0, L.Size.X, 3);

	TargetNameText = MakeText(TargetPanel, TEXT(""), 8, 8, 12.f, Crimson());
	TargetHPBar = MakeBar(TargetPanel, BarRed(), 8, 30, L.Size.X - 16, 16);

	// Start hidden
	TargetPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD MINIMAP
// ============================================================
void UAoCRuntimeUI::BuildMinimap()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Minimap")];

	MinimapPanel = MakePanel(RootCanvas, TEXT("MinimapPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!MinimapPanel) return;

	// Circular dark background
	MinimapBG = MakeImage(MinimapPanel, FLinearColor(0.03f, 0.02f, 0.02f, 0.9f), 0, 0, L.Size.X, L.Size.Y);

	// Crimson ring border (slightly larger, behind)
	MinimapRing = MakeImage(MinimapPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.8f), -3, -3, L.Size.X + 6, L.Size.Y + 6);

	// Zone name
	ZoneNameText = MakeText(MinimapPanel, TEXT("AoCWorld"), 10, L.Size.Y + 6, 10.f, Crimson());

	// Coordinates
	CoordText = MakeText(MinimapPanel, TEXT("0, 0, 0"), 10, L.Size.Y + 20, 8.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

	// Compass
	CompassText = MakeText(MinimapPanel, TEXT("N"), L.Size.X / 2 - 5, 4, 12.f, FLinearColor::White);
}

// ============================================================
// BUILD HOTBAR — 10 slots, bottom center
// ============================================================
void UAoCRuntimeUI::BuildHotbar()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Hotbar")];

	HotbarPanel = MakePanel(RootCanvas, TEXT("HotbarPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!HotbarPanel) return;

	MakeBorder(HotbarPanel, FLinearColor(0.04f, 0.02f, 0.02f, 0.85f), 0, 0, L.Size.X, L.Size.Y);

	// Column indicator on the left
	ColumnIndicator = MakeText(HotbarPanel, TEXT("1"), 4, 16, 14.f, Crimson());

	const float SlotSize = 48.f;
	const float SlotGap = 4.f;
	const float StartX = 28.f;

	HotbarSlotBorders.Empty();
	HotbarSlotLabels.Empty();
	HotbarKeyLabels.Empty();

	for (int32 i = 0; i < 10; i++)
	{
		float X = StartX + i * (SlotSize + SlotGap);

		// Slot background border
		UBorder* SlotBorder = MakeBorder(HotbarPanel, SlotBG(), X, 2, SlotSize, SlotSize);
		HotbarSlotBorders.Add(SlotBorder);

		// Slot content label (spell/item name abbreviation)
		UTextBlock* SlotLabel = MakeText(HotbarPanel, TEXT(""), X + 4, 14, 9.f, FLinearColor::White);
		HotbarSlotLabels.Add(SlotLabel);

		// Key number label (1-0)
		FString KeyStr = (i < 9) ? FString::FromInt(i + 1) : TEXT("0");
		UTextBlock* KeyLabel = MakeText(HotbarPanel, KeyStr, X + SlotSize - 14, 2, 8.f,
			FLinearColor(0.6f, 0.6f, 0.6f, 0.8f));
		HotbarKeyLabels.Add(KeyLabel);
	}
}

// ============================================================
// BUILD CHAT PANEL
// ============================================================
void UAoCRuntimeUI::BuildChatPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Chat")];

	ChatPanel = MakePanel(RootCanvas, TEXT("ChatPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!ChatPanel) return;

	MakeBorder(ChatPanel, FLinearColor(0.04f, 0.02f, 0.02f, L.Opacity * 0.85f), 0, 0, L.Size.X, L.Size.Y);

	// Tab bar
	TArray<FString> TabNames = {TEXT("System"), TEXT("Local"), TEXT("Clan"), TEXT("Party"), TEXT("Trade")};
	ChatTabButtons.Empty();
	for (int32 i = 0; i < TabNames.Num(); i++)
	{
		UButton* Tab = MakeButton(ChatPanel, TabNames[i], i * 82 + 4, 2, 78, 22,
			FLinearColor(0.1f, 0.05f, 0.05f, 0.9f));
		ChatTabButtons.Add(Tab);
	}

	// Scroll box for messages
	ChatScrollBox = CreateWidget<UScrollBox>(TEXT("ChatScroll"));
	if (ChatScrollBox)
	{
		AddToCanvas(ChatScrollBox, ChatPanel, 4, 28, L.Size.X - 8, L.Size.Y - 58);
	}

	// Input box
	ChatInput = CreateWidget<UEditableTextBox>(TEXT("ChatInput"));
	if (ChatInput)
	{
		FEditableTextBoxStyle InputStyle;
		FSlateBrush InputBg;
		InputBg.TintColor = FSlateColor(FLinearColor(0.06f, 0.03f, 0.03f, 0.9f));
		InputStyle.SetBackgroundImageNormal(InputBg);
		InputStyle.SetForegroundColor(FSlateColor(FLinearColor::White));
		ChatInput->SetWidgetStyle(InputStyle);
		ChatInput->SetHintText(FText::FromString(TEXT("Press Enter to chat...")));
		AddToCanvas(ChatInput, ChatPanel, 4, L.Size.Y - 26, L.Size.X - 8, 24);
	}
}

// ============================================================
// BUILD INVENTORY PANEL
// ============================================================
void UAoCRuntimeUI::BuildInventoryPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Inventory")];

	InventoryPanel = MakePanel(RootCanvas, TEXT("InventoryPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!InventoryPanel) return;

	// Background
	MakeBorder(InventoryPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);

	// Title bar with drag handle
	MakeBorder(InventoryPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(InventoryPanel, TEXT("INVENTORY"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(InventoryPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Tab bar: CHARACTER | INVENTORY | MAP | QUESTS | JOURNAL | SETTINGS
	TArray<FString> Tabs = {TEXT("CHAR"), TEXT("INV"), TEXT("MAP"), TEXT("QUEST"), TEXT("JOURNAL"), TEXT("SET")};
	for (int32 i = 0; i < Tabs.Num(); i++)
	{
		MakeButton(InventoryPanel, Tabs[i], i * 68 + 4, 32, 64, 22,
			(i == 1) ? Crimson() : FLinearColor(0.1f, 0.06f, 0.06f, 0.9f));
	}

	// Equipment slots area (left side — paper doll)
	MakeBorder(InventoryPanel, PanelBG(), 8, 60, 140, 280);
	MakeText(InventoryPanel, TEXT("Equipment"), 30, 62, 10.f, Crimson());

	// 13 equipment slot boxes
	TArray<FString> EquipNames = {
		TEXT("Head"), TEXT("Chest"), TEXT("Legs"), TEXT("Feet"), TEXT("Hands"),
		TEXT("Shoulders"), TEXT("Back"), TEXT("MainHand"), TEXT("OffHand"),
		TEXT("Ring1"), TEXT("Ring2"), TEXT("Amulet"), TEXT("Belt")
	};
	for (int32 i = 0; i < EquipNames.Num(); i++)
	{
		float EX = 14 + (i % 3) * 44;
		float EY = 80 + (i / 3) * 44;
		MakeBorder(InventoryPanel, SlotBG(), EX, EY, 40, 40);
		MakeText(InventoryPanel, EquipNames[i].Left(3), EX + 4, EY + 12, 7.f,
			FLinearColor(0.5f, 0.5f, 0.5f, 0.7f));
	}

	// Attributes (below equipment)
	MakeBorder(InventoryPanel, PanelBG(), 8, 348, 140, 130);
	TArray<FString> Attrs = {TEXT("STR"), TEXT("DEX"), TEXT("AGI"), TEXT("INT"), TEXT("WIS"), TEXT("END")};
	for (int32 i = 0; i < Attrs.Num(); i++)
	{
		MakeText(InventoryPanel, Attrs[i], 14, 354 + i * 20, 9.f, Crimson());
		MakeText(InventoryPanel, TEXT("10"), 70, 354 + i * 20, 9.f, FLinearColor::White);
	}

	// Bag grid area (right side — 4 bags, 7×7 = 196 slots, we show condensed)
	MakeBorder(InventoryPanel, PanelBG(), 155, 60, 258, 430);
	MakeText(InventoryPanel, TEXT("Bag Slots"), 250, 62, 10.f, Crimson());

	// Draw bag slots in a 7×8 grid (56 visible slots for Bag 1)
	for (int32 row = 0; row < 8; row++)
	{
		for (int32 col = 0; col < 7; col++)
		{
			float SX = 160 + col * 36;
			float SY = 80 + row * 36;
			MakeBorder(InventoryPanel, SlotBG(), SX, SY, 34, 34);
		}
	}

	// Gold + Weight at bottom
	MakeText(InventoryPanel, TEXT("Gold: 0"), 160, 378, 10.f, FLinearColor(1.f, 0.84f, 0.f, 1.f));
	MakeText(InventoryPanel, TEXT("Weight: 0 / 200"), 280, 378, 10.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

	// Bag tabs
	for (int32 b = 0; b < 4; b++)
	{
		MakeButton(InventoryPanel, FString::Printf(TEXT("Bag %d"), b + 1),
			160 + b * 62, 396, 58, 22, PanelBG());
	}

	// Start hidden
	InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD SPELLBOOK PANEL
// ============================================================
void UAoCRuntimeUI::BuildSpellBookPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("SpellBook")];

	SpellBookPanel = MakePanel(RootCanvas, TEXT("SpellBookPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!SpellBookPanel) return;

	MakeBorder(SpellBookPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);

	// Title bar
	MakeBorder(SpellBookPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(SpellBookPanel, TEXT("SPELLBOOK"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(SpellBookPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// School sidebar (left, 10 schools)
	TArray<FString> Schools = {
		TEXT("Arcana"), TEXT("Pyromancy"), TEXT("Cryomancy"), TEXT("Stormcalling"), TEXT("Tempest"),
		TEXT("Verdancy"), TEXT("Umbramancy"), TEXT("Radiance"), TEXT("Sangromancy"), TEXT("Dominion")
	};
	TArray<FLinearColor> SchoolColors = {
		FLinearColor(0.6f, 0.3f, 0.9f, 1.f),   // Arcana - purple
		FLinearColor(0.9f, 0.3f, 0.1f, 1.f),   // Pyromancy - orange-red
		FLinearColor(0.3f, 0.7f, 0.9f, 1.f),   // Cryomancy - ice blue
		FLinearColor(0.9f, 0.9f, 0.3f, 1.f),   // Stormcalling - yellow
		FLinearColor(0.5f, 0.8f, 0.9f, 1.f),   // Tempest - light blue
		FLinearColor(0.2f, 0.8f, 0.3f, 1.f),   // Verdancy - green
		FLinearColor(0.3f, 0.1f, 0.3f, 1.f),   // Umbramancy - dark purple
		FLinearColor(0.9f, 0.9f, 0.7f, 1.f),   // Radiance - golden white
		FLinearColor(0.7f, 0.1f, 0.1f, 1.f),   // Sangromancy - blood red
		FLinearColor(0.4f, 0.2f, 0.6f, 1.f)    // Dominion - deep purple
	};

	for (int32 i = 0; i < Schools.Num(); i++)
	{
		UButton* SchoolBtn = MakeButton(SpellBookPanel, Schools[i], 4, 34 + i * 44, 110, 40,
			FLinearColor(SchoolColors[i].R * 0.3f, SchoolColors[i].G * 0.3f, SchoolColors[i].B * 0.3f, 0.9f));
	}

	// Spell grid area (right side — 4×4 = 16 spells per school)
	MakeBorder(SpellBookPanel, PanelBG(), 120, 34, L.Size.X - 128, L.Size.Y - 80);

	// School title
	MakeText(SpellBookPanel, TEXT("Arcana"), 130, 38, 14.f, FLinearColor(0.6f, 0.3f, 0.9f, 1.f));
	MakeText(SpellBookPanel, TEXT("Level: 1"), 300, 40, 10.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

	// 4×4 spell grid
	for (int32 row = 0; row < 4; row++)
	{
		for (int32 col = 0; col < 4; col++)
		{
			float SX = 130 + col * 90;
			float SY = 62 + row * 100;
			MakeBorder(SpellBookPanel, SlotBG(), SX, SY, 82, 82);

			// Spell number
			int32 SpellIdx = row * 4 + col + 1;
			MakeText(SpellBookPanel, FString::Printf(TEXT("Spell %d"), SpellIdx),
				SX + 8, SY + 30, 8.f, FLinearColor(0.5f, 0.5f, 0.5f, 0.7f));

			// Lock icon for locked spells (show for higher-tier spells)
			if (SpellIdx > 4)
			{
				MakeText(SpellBookPanel, TEXT("Locked"), SX + 16, SY + 50, 7.f,
					FLinearColor(0.4f, 0.4f, 0.4f, 0.5f));
			}
		}
	}

	// Drag to hotbar instruction
	MakeText(SpellBookPanel, TEXT("Drag spells to Hotbar"), 200, L.Size.Y - 40, 10.f,
		FLinearColor(0.5f, 0.5f, 0.5f, 0.7f));

	SpellBookPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD CHARACTER SHEET PANEL
// ============================================================
void UAoCRuntimeUI::BuildCharSheetPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("CharSheet")];

	CharSheetPanel = MakePanel(RootCanvas, TEXT("CharSheetPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!CharSheetPanel) return;

	MakeBorder(CharSheetPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);

	// Title
	MakeBorder(CharSheetPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(CharSheetPanel, TEXT("CHARACTER"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(CharSheetPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Attributes section
	MakeText(CharSheetPanel, TEXT("ATTRIBUTES"), 10, 34, 11.f, Crimson());
	TArray<FString> Attrs = {TEXT("Strength"), TEXT("Dexterity"), TEXT("Agility"), TEXT("Intelligence"), TEXT("Wisdom"), TEXT("Endurance")};
	for (int32 i = 0; i < Attrs.Num(); i++)
	{
		float AY = 52 + i * 22;
		MakeText(CharSheetPanel, Attrs[i], 14, AY, 10.f, FLinearColor::White);
		MakeText(CharSheetPanel, TEXT("10"), 140, AY, 10.f, Crimson());
		MakeBar(CharSheetPanel, FLinearColor(0.3f, 0.3f, 0.3f, 0.5f), 170, AY + 2, 120, 14);
	}

	// Weapon Mastery section
	MakeText(CharSheetPanel, TEXT("WEAPON MASTERY"), 10, 190, 11.f, Crimson());
	TArray<FString> Weapons = {
		TEXT("Sword"), TEXT("Greatsword"), TEXT("Dagger"), TEXT("Axe"), TEXT("Great Axe"),
		TEXT("Hammer"), TEXT("Mace"), TEXT("Scythe"), TEXT("Spear"), TEXT("Bow"),
		TEXT("Shield"), TEXT("1H Staff"), TEXT("2H Staff"), TEXT("Unarmed")
	};
	for (int32 i = 0; i < FMath::Min(Weapons.Num(), 14); i++)
	{
		float WY = 208 + i * 18;
		MakeText(CharSheetPanel, Weapons[i], 14, WY, 9.f, FLinearColor::White);
		MakeText(CharSheetPanel, TEXT("1"), 140, WY, 9.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));
		MakeBar(CharSheetPanel, FLinearColor(0.5f, 0.3f, 0.1f, 0.5f), 160, WY + 2, 100, 10);
	}

	// Gathering section
	float GatherY = 466;
	MakeText(CharSheetPanel, TEXT("GATHERING"), 10, GatherY, 11.f, Crimson());
	TArray<FString> Gathering = {TEXT("Mining"), TEXT("Woodcutting"), TEXT("Fishing"), TEXT("Herbalism"), TEXT("Hunting"), TEXT("Farming"), TEXT("Skinning")};
	for (int32 i = 0; i < Gathering.Num(); i++)
	{
		float GY = GatherY + 18 + i * 18;
		if (GY + 18 > L.Size.Y - 10) break;
		MakeText(CharSheetPanel, Gathering[i], 14, GY, 9.f, FLinearColor::White);
		MakeText(CharSheetPanel, TEXT("1"), 140, GY, 9.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));
	}

	CharSheetPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD CRAFTING PANEL
// ============================================================
void UAoCRuntimeUI::BuildCraftingPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Crafting")];

	CraftingPanel = MakePanel(RootCanvas, TEXT("CraftingPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!CraftingPanel) return;

	MakeBorder(CraftingPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);

	// Title
	MakeBorder(CraftingPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(CraftingPanel, TEXT("CRAFTING"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(CraftingPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// 8 Discipline tabs
	TArray<FString> Disciplines = {
		TEXT("Wpn Smith"), TEXT("Arm Smith"), TEXT("Bow"), TEXT("Staff"),
		TEXT("Jewel"), TEXT("Enchant"), TEXT("Alchemy"), TEXT("Cooking")
	};
	for (int32 i = 0; i < Disciplines.Num(); i++)
	{
		float TX = 4 + (i % 4) * 116;
		float TY = 32 + (i / 4) * 26;
		MakeButton(CraftingPanel, Disciplines[i], TX, TY, 112, 24,
			(i == 0) ? Crimson() : FLinearColor(0.1f, 0.06f, 0.06f, 0.9f));
	}

	// Skill progress
	MakeText(CraftingPanel, TEXT("Weapon Smithing: Lv 1"), 10, 88, 10.f, Crimson());
	MakeBar(CraftingPanel, FLinearColor(0.5f, 0.3f, 0.1f, 0.5f), 200, 90, 180, 14);

	// Recipe list area
	MakeBorder(CraftingPanel, PanelBG(), 8, 108, L.Size.X / 2 - 12, L.Size.Y - 160);
	MakeText(CraftingPanel, TEXT("Recipes"), 14, 112, 10.f, Crimson());

	// Sample recipes
	TArray<FString> SampleRecipes = {
		TEXT("Iron Sword"), TEXT("Iron Dagger"), TEXT("Iron Axe"),
		TEXT("Steel Sword"), TEXT("Bronze Shield")
	};
	for (int32 i = 0; i < SampleRecipes.Num(); i++)
	{
		MakeButton(CraftingPanel, SampleRecipes[i], 12, 130 + i * 28, L.Size.X / 2 - 20, 26, PanelBG());
	}

	// Recipe detail area (right side)
	MakeBorder(CraftingPanel, PanelBG(), L.Size.X / 2 + 4, 108, L.Size.X / 2 - 12, L.Size.Y - 160);
	MakeText(CraftingPanel, TEXT("Iron Sword"), L.Size.X / 2 + 14, 112, 12.f, FLinearColor::White);
	MakeText(CraftingPanel, TEXT("Required:"), L.Size.X / 2 + 14, 138, 10.f, Crimson());
	MakeText(CraftingPanel, TEXT("Iron Ingot x3"), L.Size.X / 2 + 14, 156, 9.f, FLinearColor::White);
	MakeText(CraftingPanel, TEXT("Leather Strip x1"), L.Size.X / 2 + 14, 174, 9.f, FLinearColor::White);
	MakeText(CraftingPanel, TEXT("Oak Plank x1"), L.Size.X / 2 + 14, 192, 9.f, FLinearColor::White);

	// CRAFT button
	MakeButton(CraftingPanel, TEXT("CRAFT"), L.Size.X / 2 + 80, L.Size.Y - 46, 120, 36,
		FLinearColor(CrimsonR * 0.8f, CrimsonG, CrimsonB, 0.95f));

	// Brewcraft/Tailoring/Leatherworking additional tabs
	MakeButton(CraftingPanel, TEXT("Brewing"), 4, L.Size.Y - 46, 80, 24, PanelBG());
	MakeButton(CraftingPanel, TEXT("Tailor"), 88, L.Size.Y - 46, 80, 24, PanelBG());
	MakeButton(CraftingPanel, TEXT("Leather"), 172, L.Size.Y - 46, 80, 24, PanelBG());

	CraftingPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD PAUSE MENU
// ============================================================
void UAoCRuntimeUI::BuildPauseMenuPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Pause")];

	PausePanel = MakePanel(RootCanvas, TEXT("PausePanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!PausePanel) return;

	MakeBorder(PausePanel, FLinearColor(0.03f, 0.01f, 0.01f, 0.95f), 0, 0, L.Size.X, L.Size.Y);

	// Title
	MakeText(PausePanel, TEXT("ARCHITECT OF CREATION"), L.Size.X / 2 - 120, 30, 16.f, Crimson());

	// Menu buttons
	TArray<FString> MenuItems = {
		TEXT("Resume"), TEXT("Settings"), TEXT("Keybinds"), TEXT("Save Layout"),
		TEXT("Load Layout"), TEXT("Help"), TEXT("Return to Menu"), TEXT("Quit Game")
	};
	for (int32 i = 0; i < MenuItems.Num(); i++)
	{
		MakeButton(PausePanel, MenuItems[i], L.Size.X / 2 - 100, 80 + i * 48, 200, 40,
			FLinearColor(0.12f, 0.05f, 0.05f, 0.9f));
	}

	PausePanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD DIALOGUE PANEL
// ============================================================
void UAoCRuntimeUI::BuildDialoguePanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Dialogue")];

	DialoguePanel = MakePanel(RootCanvas, TEXT("DialoguePanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!DialoguePanel) return;

	MakeBorder(DialoguePanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(DialoguePanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.6f), 0, 0, L.Size.X, 3);

	DialogueNPCName = MakeText(DialoguePanel, TEXT(""), 16, 8, 14.f, Crimson());
	DialogueBodyText = MakeText(DialoguePanel, TEXT(""), 16, 30, 11.f, FLinearColor::White);

	// 4 response buttons
	DialogueButtons.Empty();
	DialogueButtonLabels.Empty();
	for (int32 i = 0; i < 4; i++)
	{
		UButton* RBtn = MakeButton(DialoguePanel, TEXT(""), 16, 100 + i * 30, L.Size.X - 32, 26,
			FLinearColor(0.1f, 0.05f, 0.05f, 0.9f));
		DialogueButtons.Add(RBtn);
	}

	DialoguePanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD LOOT WINDOW
// ============================================================
void UAoCRuntimeUI::BuildLootWindow()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Loot")];

	LootPanel = MakePanel(RootCanvas, TEXT("LootPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!LootPanel) return;

	MakeBorder(LootPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(LootPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	LootTitleText = MakeText(LootPanel, TEXT("LOOT"), 10, 4, 12.f, FLinearColor::White);
	MakeButton(LootPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Loot scroll area
	LootScrollBox = CreateWidget<UScrollBox>(TEXT("LootScroll"));
	if (LootScrollBox)
	{
		AddToCanvas(LootScrollBox, LootPanel, 4, 32, L.Size.X - 8, L.Size.Y - 72);
	}

	// Loot All button
	MakeButton(LootPanel, TEXT("LOOT ALL"), L.Size.X / 2 - 50, L.Size.Y - 36, 100, 30,
		FLinearColor(CrimsonR * 0.7f, CrimsonG, CrimsonB, 0.95f));

	LootPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD DEATH SCREEN
// ============================================================
void UAoCRuntimeUI::BuildDeathScreen()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Death")];

	DeathPanel = MakePanel(RootCanvas, TEXT("DeathPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!DeathPanel) return;

	MakeBorder(DeathPanel, FLinearColor(0.02f, 0.0f, 0.0f, 0.9f), 0, 0, L.Size.X, L.Size.Y);

	DeathText = MakeText(DeathPanel, TEXT("YOU HAVE FALLEN"), L.Size.X / 2 - 100, 50, 20.f, Crimson());
	DeathSubText = MakeText(DeathPanel, TEXT("Slain by: Unknown"), L.Size.X / 2 - 80, 90, 12.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

	MakeText(DeathPanel, TEXT("Awaiting revival..."), L.Size.X / 2 - 70, 140, 11.f, FLinearColor(0.5f, 0.5f, 0.5f, 1.f));
	MakeText(DeathPanel, TEXT("Press SPACE to release"), L.Size.X / 2 - 80, 170, 11.f, FLinearColor(0.5f, 0.5f, 0.5f, 1.f));

	MakeButton(DeathPanel, TEXT("Release"), L.Size.X / 2 - 60, 210, 120, 36, Crimson());

	DeathPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD PARTY FRAMES
// ============================================================
void UAoCRuntimeUI::BuildPartyFrames()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Party")];

	PartyPanel = MakePanel(RootCanvas, TEXT("PartyPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!PartyPanel) return;

	MakeBorder(PartyPanel, FLinearColor(0.04f, 0.02f, 0.02f, 0.8f), 0, 0, L.Size.X, L.Size.Y);

	PartyHPBars.Empty();
	PartyNameLabels.Empty();

	// Pre-build 8 party member slots (hidden until populated)
	for (int32 i = 0; i < 8; i++)
	{
		float PY = 4 + i * 30;
		UTextBlock* NameLabel = MakeText(PartyPanel, TEXT(""), 6, PY, 9.f, FLinearColor::White);
		PartyNameLabels.Add(NameLabel);

		UProgressBar* HPBar_Party = MakeBar(PartyPanel, BarRed(), 6, PY + 14, 120, 10);
		PartyHPBars.Add(HPBar_Party);
	}

	// Start hidden (no party)
	PartyPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD QUEST PANEL
// ============================================================
void UAoCRuntimeUI::BuildQuestPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Quest")];

	QuestPanel = MakePanel(RootCanvas, TEXT("QuestPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!QuestPanel) return;

	MakeBorder(QuestPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(QuestPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(QuestPanel, TEXT("QUEST LOG"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(QuestPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Quest categories
	MakeButton(QuestPanel, TEXT("Active"), 8, 34, 80, 24, Crimson());
	MakeButton(QuestPanel, TEXT("Complete"), 92, 34, 80, 24, PanelBG());

	// Quest list area
	MakeBorder(QuestPanel, PanelBG(), 8, 64, L.Size.X - 16, L.Size.Y - 80);
	MakeText(QuestPanel, TEXT("No active quests"), 20, 80, 10.f, FLinearColor(0.5f, 0.5f, 0.5f, 0.7f));

	QuestPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD WORLD MAP PANEL
// ============================================================
void UAoCRuntimeUI::BuildMapPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Map")];

	MapPanel = MakePanel(RootCanvas, TEXT("MapPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!MapPanel) return;

	MakeBorder(MapPanel, FLinearColor(0.03f, 0.02f, 0.02f, 0.95f), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(MapPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(MapPanel, TEXT("WORLD MAP"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(MapPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Map area (placeholder)
	MakeBorder(MapPanel, FLinearColor(0.06f, 0.04f, 0.04f, 0.9f), 8, 34, L.Size.X - 16, L.Size.Y - 72);
	MakeText(MapPanel, TEXT("Map rendering area"), L.Size.X / 2 - 60, L.Size.Y / 2, 12.f,
		FLinearColor(0.4f, 0.4f, 0.4f, 0.5f));

	// Player position marker
	MakeText(MapPanel, TEXT("+"), L.Size.X / 2, L.Size.Y / 2 - 20, 16.f, Crimson());

	// Coordinates
	MakeText(MapPanel, TEXT("Position: 0, 0"), 8, L.Size.Y - 34, 10.f, FLinearColor(0.7f, 0.7f, 0.7f, 1.f));

	MapPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD CLAN PANEL
// ============================================================
void UAoCRuntimeUI::BuildClanPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Clan")];

	ClanPanel = MakePanel(RootCanvas, TEXT("ClanPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!ClanPanel) return;

	MakeBorder(ClanPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(ClanPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(ClanPanel, TEXT("CLAN"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(ClanPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Tabs
	MakeButton(ClanPanel, TEXT("Members"), 8, 34, 90, 24, Crimson());
	MakeButton(ClanPanel, TEXT("Ranks"), 102, 34, 90, 24, PanelBG());
	MakeButton(ClanPanel, TEXT("Vault"), 196, 34, 90, 24, PanelBG());
	MakeButton(ClanPanel, TEXT("Wars"), 290, 34, 90, 24, PanelBG());

	// Clan info area
	MakeBorder(ClanPanel, PanelBG(), 8, 64, L.Size.X - 16, L.Size.Y - 120);
	MakeText(ClanPanel, TEXT("No clan. Create or join one!"), 20, 100, 10.f,
		FLinearColor(0.5f, 0.5f, 0.5f, 0.7f));

	// Clan actions
	MakeButton(ClanPanel, TEXT("Create Clan"), 8, L.Size.Y - 46, 120, 36, Crimson());
	MakeButton(ClanPanel, TEXT("Leave Clan"), 140, L.Size.Y - 46, 120, 36, PanelBG());

	// MOTD
	MakeText(ClanPanel, TEXT("MOTD: -"), 8, L.Size.Y - 74, 9.f, FLinearColor(0.6f, 0.6f, 0.6f, 1.f));

	ClanPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD SETTINGS PANEL
// ============================================================
void UAoCRuntimeUI::BuildSettingsPanel()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("Settings")];

	SettingsPanel = MakePanel(RootCanvas, TEXT("SettingsPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!SettingsPanel) return;

	MakeBorder(SettingsPanel, DarkBG(), 0, 0, L.Size.X, L.Size.Y);
	MakeBorder(SettingsPanel, FLinearColor(CrimsonR, CrimsonG, CrimsonB, 0.7f), 0, 0, L.Size.X, 28);
	MakeText(SettingsPanel, TEXT("SETTINGS"), 10, 4, 13.f, FLinearColor::White);
	MakeButton(SettingsPanel, TEXT("X"), L.Size.X - 28, 2, 24, 24, FLinearColor(0.4f, 0.05f, 0.05f, 0.9f));

	// Tabs
	MakeButton(SettingsPanel, TEXT("Video"), 8, 34, 80, 24, Crimson());
	MakeButton(SettingsPanel, TEXT("Audio"), 92, 34, 80, 24, PanelBG());
	MakeButton(SettingsPanel, TEXT("Keybinds"), 176, 34, 80, 24, PanelBG());
	MakeButton(SettingsPanel, TEXT("UI"), 260, 34, 80, 24, PanelBG());
	MakeButton(SettingsPanel, TEXT("Gameplay"), 344, 34, 80, 24, PanelBG());

	// Settings content area
	MakeBorder(SettingsPanel, PanelBG(), 8, 64, L.Size.X - 16, L.Size.Y - 120);

	// Video settings (default tab)
	TArray<FString> VideoSettings = {
		TEXT("Resolution: 1920x1080"),
		TEXT("Window Mode: Fullscreen"),
		TEXT("Quality: High"),
		TEXT("V-Sync: On"),
		TEXT("FPS Limit: 120"),
		TEXT("Draw Distance: Far"),
		TEXT("Shadow Quality: High"),
		TEXT("Anti-Aliasing: TAA"),
		TEXT("Motion Blur: Off"),
		TEXT("Bloom: Medium")
	};
	for (int32 i = 0; i < VideoSettings.Num(); i++)
	{
		MakeText(SettingsPanel, VideoSettings[i], 20, 74 + i * 28, 10.f, FLinearColor::White);
	}

	// Auto-Detect button
	MakeButton(SettingsPanel, TEXT("Auto-Detect"), 8, L.Size.Y - 46, 120, 36,
		FLinearColor(0.2f, 0.4f, 0.2f, 0.9f));
	MakeButton(SettingsPanel, TEXT("Apply"), L.Size.X - 136, L.Size.Y - 46, 120, 36, Crimson());

	SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// BUILD SKILL GAIN TOAST
// ============================================================
void UAoCRuntimeUI::BuildSkillToast()
{
	if (!RootCanvas) return;
	const FPanelLayout& L = PanelLayouts[TEXT("SkillToast")];

	SkillToastPanel = MakePanel(RootCanvas, TEXT("SkillToastPanel"), L.Position.X, L.Position.Y, L.Size.X, L.Size.Y);
	if (!SkillToastPanel) return;

	MakeBorder(SkillToastPanel, FLinearColor(0.05f, 0.02f, 0.02f, 0.85f), 0, 0, L.Size.X, L.Size.Y);
	SkillToastText = MakeText(SkillToastPanel, TEXT(""), 10, 10, 11.f, FLinearColor(1.f, 0.84f, 0.f, 1.f));

	SkillToastPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// NATIVE TICK — Update timers, floating text, autocast
// ============================================================
void UAoCRuntimeUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Skill toast fade
	if (SkillToastTimer > 0.f)
	{
		SkillToastTimer -= InDeltaTime;
		if (SkillToastTimer <= 0.f && SkillToastPanel)
		{
			SkillToastPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Floating damage text animation
	for (int32 i = FloatingTexts.Num() - 1; i >= 0; i--)
	{
		FFloatingText& FT = FloatingTexts[i];
		FT.Life -= InDeltaTime;
		if (FT.Life <= 0.f)
		{
			if (FT.Text)
			{
				FT.Text->RemoveFromParent();
			}
			FloatingTexts.RemoveAt(i);
		}
		else if (FT.Text)
		{
			// Float upward and fade
			UCanvasPanelSlot* TSlot = Cast<UCanvasPanelSlot>(FT.Text->Slot);
			if (TSlot)
			{
				FVector2D Pos = TSlot->GetPosition();
				Pos += FT.Velocity * InDeltaTime;
				TSlot->SetPosition(Pos);
			}
			float Alpha = FMath::Clamp(FT.Life / 2.f, 0.f, 1.f);
			FT.Text->SetRenderOpacity(Alpha);
		}
	}

	// Hotbar cooldown updates
	for (int32 i = 0; i < 100; i++)
	{
		if (HotbarData[i].CooldownRemaining > 0.f)
		{
			HotbarData[i].CooldownRemaining -= InDeltaTime;
			if (HotbarData[i].CooldownRemaining < 0.f)
				HotbarData[i].CooldownRemaining = 0.f;
		}
	}
}

// ============================================================
// MOUSE INPUT — Panel Dragging
// ============================================================
FReply UAoCRuntimeUI::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bGUIMode) return FReply::Unhandled();

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FVector2D MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

		// Check if mouse is over any panel's title bar (top 28px)
		UCanvasPanel* HitPanel = FindPanelUnderMouse(MousePos);
		if (HitPanel)
		{
			bDraggingPanel = true;
			FVector2D PanelPos = GetPanelPosition(HitPanel);
			DragOffset = MousePos - PanelPos;

			// Find panel name
			DraggedPanelName = HitPanel->GetName();

			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAoCRuntimeUI::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingPanel)
	{
		bDraggingPanel = false;
		DraggedPanelName = TEXT("");
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UAoCRuntimeUI::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingPanel && !DraggedPanelName.IsEmpty())
	{
		FVector2D MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		FVector2D NewPos = MousePos - DragOffset;

		// Find the panel by name and move it
		// Check each known panel
		TArray<TPair<FString, UCanvasPanel*>> AllPanels = {
			{TEXT("HUDBarsPanel"), HUDBarsPanel},
			{TEXT("TargetPanel"), TargetPanel},
			{TEXT("MinimapPanel"), MinimapPanel},
			{TEXT("HotbarPanel"), HotbarPanel},
			{TEXT("ChatPanel"), ChatPanel},
			{TEXT("InventoryPanel"), InventoryPanel},
			{TEXT("SpellBookPanel"), SpellBookPanel},
			{TEXT("CharSheetPanel"), CharSheetPanel},
			{TEXT("CraftingPanel"), CraftingPanel},
			{TEXT("PausePanel"), PausePanel},
			{TEXT("DialoguePanel"), DialoguePanel},
			{TEXT("LootPanel"), LootPanel},
			{TEXT("DeathPanel"), DeathPanel},
			{TEXT("PartyPanel"), PartyPanel},
			{TEXT("QuestPanel"), QuestPanel},
			{TEXT("MapPanel"), MapPanel},
			{TEXT("ClanPanel"), ClanPanel},
			{TEXT("SettingsPanel"), SettingsPanel}
		};

		for (auto& Pair : AllPanels)
		{
			if (Pair.Key == DraggedPanelName && Pair.Value)
			{
				UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(Pair.Value->Slot);
				if (PSlot)
				{
					PSlot->SetPosition(NewPos);
				}
				break;
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// ============================================================
// KEY INPUT — Panel toggles, hotbar, mode switch
// ============================================================
FReply UAoCRuntimeUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// ESC — Toggle GUI Mode (always works)
	if (Key == EKeys::Escape)
	{
		// If any panel is open, close it first
		if (PausePanel && PausePanel->GetVisibility() == ESlateVisibility::Visible)
		{
			PausePanel->SetVisibility(ESlateVisibility::Collapsed);
			return FReply::Handled();
		}
		ToggleGUIMode();
		return FReply::Handled();
	}

	// Only process UI keybinds in GUI mode or for always-available keys
	if (bGUIMode)
	{
		if (Key == EKeys::I) { ToggleInventory(); return FReply::Handled(); }
		if (Key == EKeys::B) { ToggleSpellBook(); return FReply::Handled(); }
		if (Key == EKeys::P) { ToggleCharacterSheet(); return FReply::Handled(); }
		if (Key == EKeys::K) { ToggleCrafting(); return FReply::Handled(); }
		if (Key == EKeys::J) { ToggleQuestLog(); return FReply::Handled(); }
		if (Key == EKeys::M) { ToggleWorldMap(); return FReply::Handled(); }
		if (Key == EKeys::G) { ToggleClanPanel(); return FReply::Handled(); }
		if (Key == EKeys::N) { ToggleSettings(); return FReply::Handled(); }
	}

	// Hotbar keys (1-0) — work in both modes
	if (Key == EKeys::One) { ActivateHotbarSlot(0); return FReply::Handled(); }
	if (Key == EKeys::Two) { ActivateHotbarSlot(1); return FReply::Handled(); }
	if (Key == EKeys::Three) { ActivateHotbarSlot(2); return FReply::Handled(); }
	if (Key == EKeys::Four) { ActivateHotbarSlot(3); return FReply::Handled(); }
	if (Key == EKeys::Five) { ActivateHotbarSlot(4); return FReply::Handled(); }
	if (Key == EKeys::Six) { ActivateHotbarSlot(5); return FReply::Handled(); }
	if (Key == EKeys::Seven) { ActivateHotbarSlot(6); return FReply::Handled(); }
	if (Key == EKeys::Eight) { ActivateHotbarSlot(7); return FReply::Handled(); }
	if (Key == EKeys::Nine) { ActivateHotbarSlot(8); return FReply::Handled(); }
	if (Key == EKeys::Zero) { ActivateHotbarSlot(9); return FReply::Handled(); }

	// CTRL+number for column switching
	if (InKeyEvent.IsControlDown())
	{
		if (Key == EKeys::One) { SwitchHotbarColumn(0); return FReply::Handled(); }
		if (Key == EKeys::Two) { SwitchHotbarColumn(1); return FReply::Handled(); }
		if (Key == EKeys::Three) { SwitchHotbarColumn(2); return FReply::Handled(); }
		if (Key == EKeys::Four) { SwitchHotbarColumn(3); return FReply::Handled(); }
		if (Key == EKeys::Five) { SwitchHotbarColumn(4); return FReply::Handled(); }
		if (Key == EKeys::Six) { SwitchHotbarColumn(5); return FReply::Handled(); }
		if (Key == EKeys::Seven) { SwitchHotbarColumn(6); return FReply::Handled(); }
		if (Key == EKeys::Eight) { SwitchHotbarColumn(7); return FReply::Handled(); }
		if (Key == EKeys::Nine) { SwitchHotbarColumn(8); return FReply::Handled(); }
		if (Key == EKeys::Zero) { SwitchHotbarColumn(9); return FReply::Handled(); }
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================
// PANEL HELPERS
// ============================================================
void UAoCRuntimeUI::TogglePanelVisibility(UCanvasPanel* Panel, const FString& PanelName, bool& bState)
{
	if (!Panel) return;
	bState = !bState;
	Panel->SetVisibility(bState ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UAoCRuntimeUI::SetPanelPosition(UCanvasPanel* Panel, const FString& Name, float X, float Y)
{
	if (!Panel) return;
	UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(Panel->Slot);
	if (PSlot)
	{
		PSlot->SetPosition(FVector2D(X, Y));
	}
}

FVector2D UAoCRuntimeUI::GetPanelPosition(UCanvasPanel* Panel)
{
	if (!Panel) return FVector2D::ZeroVector;
	UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(Panel->Slot);
	if (PSlot)
	{
		return PSlot->GetPosition();
	}
	return FVector2D::ZeroVector;
}

UCanvasPanel* UAoCRuntimeUI::FindPanelUnderMouse(FVector2D MousePos)
{
	// Check all draggable panels (title bar = top 28px)
	TArray<UCanvasPanel*> Panels = {
		HUDBarsPanel, TargetPanel, MinimapPanel, HotbarPanel, ChatPanel,
		InventoryPanel, SpellBookPanel, CharSheetPanel, CraftingPanel,
		PausePanel, QuestPanel, MapPanel, ClanPanel, SettingsPanel,
		DialoguePanel, LootPanel, PartyPanel
	};

	for (UCanvasPanel* P : Panels)
	{
		if (!P || P->GetVisibility() != ESlateVisibility::Visible) continue;

		UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(P->Slot);
		if (!PSlot) continue;

		FVector2D Pos = PSlot->GetPosition();
		FVector2D Size = PSlot->GetSize();

		// Title bar hit test (top 28 pixels)
		if (MousePos.X >= Pos.X && MousePos.X <= Pos.X + Size.X &&
			MousePos.Y >= Pos.Y && MousePos.Y <= Pos.Y + 28.f)
		{
			return P;
		}
	}
	return nullptr;
}

// ============================================================
// MODE TOGGLES
// ============================================================
void UAoCRuntimeUI::ToggleGUIMode()
{
	bGUIMode = !bGUIMode;

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		if (bGUIMode)
		{
			// Show mouse cursor, allow UI interaction
			PC->SetShowMouseCursor(true);
			FInputModeGameAndUI Mode;
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
		}
		else
		{
			// Hide cursor, game-only input
			PC->SetShowMouseCursor(false);
			PC->SetInputMode(FInputModeGameOnly());

			// Close all popup panels when exiting GUI mode
			// Keep HUD elements visible
		}
	}
}

// ============================================================
// PANEL TOGGLE IMPLEMENTATIONS
// ============================================================

// Track open states
static bool bInvOpen = false;
static bool bSpellOpen = false;
static bool bCharOpen = false;
static bool bCraftOpen = false;
static bool bPauseOpen = false;
static bool bQuestOpen = false;
static bool bMapOpen = false;
static bool bClanOpen = false;
static bool bSettingsOpen = false;

void UAoCRuntimeUI::ToggleInventory()
{
	TogglePanelVisibility(InventoryPanel, TEXT("Inventory"), bInvOpen);
	if (bInvOpen && !bGUIMode) ToggleGUIMode(); // Auto-enter GUI mode
}

void UAoCRuntimeUI::ToggleSpellBook()
{
	TogglePanelVisibility(SpellBookPanel, TEXT("SpellBook"), bSpellOpen);
	if (bSpellOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::ToggleCharacterSheet()
{
	TogglePanelVisibility(CharSheetPanel, TEXT("CharSheet"), bCharOpen);
	if (bCharOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::ToggleCrafting()
{
	TogglePanelVisibility(CraftingPanel, TEXT("Crafting"), bCraftOpen);
	if (bCraftOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::TogglePauseMenu()
{
	TogglePanelVisibility(PausePanel, TEXT("Pause"), bPauseOpen);
	if (bPauseOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::ToggleQuestLog()
{
	TogglePanelVisibility(QuestPanel, TEXT("Quest"), bQuestOpen);
	if (bQuestOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::ToggleWorldMap()
{
	TogglePanelVisibility(MapPanel, TEXT("Map"), bMapOpen);
	if (bMapOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::ToggleClanPanel()
{
	TogglePanelVisibility(ClanPanel, TEXT("Clan"), bClanOpen);
	if (bClanOpen && !bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::ToggleSettings()
{
	TogglePanelVisibility(SettingsPanel, TEXT("Settings"), bSettingsOpen);
	if (bSettingsOpen && !bGUIMode) ToggleGUIMode();
}

// ============================================================
// HOTBAR SYSTEM
// ============================================================
void UAoCRuntimeUI::ActivateHotbarSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= 10) return;

	int32 DataIndex = ActiveHotbarColumn * 10 + SlotIndex;
	if (DataIndex >= HotbarData.Num()) return;

	FHotbarSlot& HotSlot = HotbarData[DataIndex];
	if (HotSlot.ItemID.IsEmpty()) return;

	// Visual feedback — flash the slot border
	if (SlotIndex < HotbarSlotBorders.Num() && HotbarSlotBorders[SlotIndex])
	{
		HotbarSlotBorders[SlotIndex]->SetBrushColor(Crimson());
		// Will reset next tick (simplified — proper implementation uses timer)
	}

	// TODO: Hook into game systems — cast spell, equip weapon, use item, etc.
	UE_LOG(LogTemp, Log, TEXT("AoC Hotbar: Activated slot %d (column %d) — %s"),
		SlotIndex, ActiveHotbarColumn, *HotSlot.DisplayName);
}

void UAoCRuntimeUI::SwitchHotbarColumn(int32 ColumnIndex)
{
	if (ColumnIndex < 0 || ColumnIndex >= 10) return;
	ActiveHotbarColumn = ColumnIndex;

	// Update column indicator
	if (ColumnIndicator)
	{
		ColumnIndicator->SetText(FText::FromString(FString::FromInt(ColumnIndex + 1)));
	}

	// Update slot visuals for new column
	for (int32 i = 0; i < 10; i++)
	{
		int32 DataIndex = ColumnIndex * 10 + i;
		if (DataIndex < HotbarData.Num() && i < HotbarSlotLabels.Num() && HotbarSlotLabels[i])
		{
			FHotbarSlot& HSlot = HotbarData[DataIndex];
			HotbarSlotLabels[i]->SetText(FText::FromString(HSlot.DisplayName.Left(6)));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AoC Hotbar: Switched to column %d"), ColumnIndex + 1);
}

void UAoCRuntimeUI::SetHotbarSlot(int32 Column, int32 SlotNum, const FHotbarSlot& Data)
{
	int32 Idx = Column * 10 + SlotNum;
	if (Idx >= 0 && Idx < HotbarData.Num())
	{
		HotbarData[Idx] = Data;
		if (Column == ActiveHotbarColumn)
		{
			SwitchHotbarColumn(ActiveHotbarColumn); // Refresh display
		}
	}
}

void UAoCRuntimeUI::ClearHotbarSlot(int32 Column, int32 SlotNum)
{
	int32 Idx = Column * 10 + SlotNum;
	if (Idx >= 0 && Idx < HotbarData.Num())
	{
		HotbarData[Idx] = FHotbarSlot();
		if (Column == ActiveHotbarColumn)
		{
			SwitchHotbarColumn(ActiveHotbarColumn);
		}
	}
}

// ============================================================
// HUD UPDATE FUNCTIONS
// ============================================================
void UAoCRuntimeUI::UpdateHP(float Current, float Max)
{
	if (HPBar) HPBar->SetPercent(Max > 0 ? Current / Max : 0.f);
	if (HPText) HPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Current, Max)));
}

void UAoCRuntimeUI::UpdateMP(float Current, float Max)
{
	if (MPBar) MPBar->SetPercent(Max > 0 ? Current / Max : 0.f);
	if (MPText) MPText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Current, Max)));
}

void UAoCRuntimeUI::UpdateStamina(float Current, float Max)
{
	if (StaminaBar) StaminaBar->SetPercent(Max > 0 ? Current / Max : 0.f);
	if (StaminaText) StaminaText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Current, Max)));
}

void UAoCRuntimeUI::UpdateXP(float Current, float Max)
{
	if (XPBar) XPBar->SetPercent(Max > 0 ? Current / Max : 0.f);
	if (XPText) XPText->SetText(FText::FromString(FString::Printf(TEXT("XP: %.0f / %.0f"), Current, Max)));
}

void UAoCRuntimeUI::UpdateGold(int32 Amount)
{
	if (GoldText) GoldText->SetText(FText::FromString(FString::Printf(TEXT("Gold: %d"), Amount)));
}

void UAoCRuntimeUI::UpdateTargetInfo(const FString& Name, float HP, float MaxHP)
{
	if (TargetPanel) TargetPanel->SetVisibility(ESlateVisibility::Visible);
	if (TargetNameText) TargetNameText->SetText(FText::FromString(Name));
	if (TargetHPBar) TargetHPBar->SetPercent(MaxHP > 0 ? HP / MaxHP : 0.f);
}

void UAoCRuntimeUI::ClearTarget()
{
	if (TargetPanel) TargetPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UAoCRuntimeUI::UpdateZoneName(const FString& ZoneName)
{
	if (ZoneNameText) ZoneNameText->SetText(FText::FromString(ZoneName));
}

void UAoCRuntimeUI::UpdateCompass(float YawDegrees)
{
	if (!CompassText) return;
	FString Dir;
	if (YawDegrees >= -22.5f && YawDegrees < 22.5f) Dir = TEXT("N");
	else if (YawDegrees >= 22.5f && YawDegrees < 67.5f) Dir = TEXT("NE");
	else if (YawDegrees >= 67.5f && YawDegrees < 112.5f) Dir = TEXT("E");
	else if (YawDegrees >= 112.5f && YawDegrees < 157.5f) Dir = TEXT("SE");
	else if (YawDegrees >= 157.5f || YawDegrees < -157.5f) Dir = TEXT("S");
	else if (YawDegrees >= -157.5f && YawDegrees < -112.5f) Dir = TEXT("SW");
	else if (YawDegrees >= -112.5f && YawDegrees < -67.5f) Dir = TEXT("W");
	else Dir = TEXT("NW");
	CompassText->SetText(FText::FromString(Dir));
}

// ============================================================
// DIALOGUE
// ============================================================
void UAoCRuntimeUI::ShowDialogue(const FString& NPCName, const FString& DialogueText, const TArray<FString>& Responses)
{
	if (!DialoguePanel) return;
	DialoguePanel->SetVisibility(ESlateVisibility::Visible);
	if (DialogueNPCName) DialogueNPCName->SetText(FText::FromString(NPCName));
	if (DialogueBodyText) DialogueBodyText->SetText(FText::FromString(DialogueText));

	// Update response buttons
	for (int32 i = 0; i < DialogueButtons.Num(); i++)
	{
		if (i < Responses.Num())
		{
			DialogueButtons[i]->SetVisibility(ESlateVisibility::Visible);
			// Button text update would need child access
		}
		else
		{
			DialogueButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Auto-enter GUI mode for dialogue
	if (!bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::HideDialogue()
{
	if (DialoguePanel) DialoguePanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// CHAT SYSTEM
// ============================================================
void UAoCRuntimeUI::AddChatMessage(const FString& Sender, const FString& Msg, EChatChannel Channel)
{
	FChatMessage NewMsg;
	NewMsg.SenderName = Sender;
	NewMsg.Message = Msg;
	NewMsg.Channel = Channel;

	// Determine color by channel
	switch (Channel)
	{
		case EChatChannel::Local:   NewMsg.Color = FLinearColor::White; break;
		case EChatChannel::Shout:   NewMsg.Color = FLinearColor(1.f, 1.f, 0.3f, 1.f); break;
		case EChatChannel::Clan:    NewMsg.Color = FLinearColor(0.3f, 0.9f, 0.3f, 1.f); break;
		case EChatChannel::Party:   NewMsg.Color = FLinearColor(0.3f, 0.8f, 1.f, 1.f); break;
		case EChatChannel::Whisper: NewMsg.Color = FLinearColor(0.8f, 0.4f, 1.f, 1.f); break;
		case EChatChannel::Trade:   NewMsg.Color = FLinearColor(1.f, 0.6f, 0.2f, 1.f); break;
		case EChatChannel::System:  NewMsg.Color = Crimson(); break;
		case EChatChannel::Combat:  NewMsg.Color = FLinearColor(1.f, 0.2f, 0.2f, 1.f); break;
	}

	ChatLog.Add(NewMsg);

	// Add to scroll box
	if (ChatScrollBox)
	{
		UTextBlock* MsgText = NewObject<UTextBlock>(ChatScrollBox);
		if (MsgText)
		{
			FString FormattedMsg = FString::Printf(TEXT("[%s] %s: %s"),
				*UEnum::GetValueAsString(Channel).RightChop(15), *Sender, *Msg);
			MsgText->SetText(FText::FromString(FormattedMsg));
			MsgText->SetColorAndOpacity(FSlateColor(NewMsg.Color));
			FSlateFontInfo ChatFont = MsgText->GetFont();
			ChatFont.Size = 10;
			MsgText->SetFont(ChatFont);
			ChatScrollBox->AddChild(MsgText);
			ChatScrollBox->ScrollToEnd();
		}
	}
}

void UAoCRuntimeUI::SetChatFilter(EChatChannel Channel, bool bShow)
{
	int32 Idx = (int32)Channel;
	if (Idx >= 0 && Idx < ChatFilters.Num())
	{
		ChatFilters[Idx] = bShow;
	}
}

// ============================================================
// PARTY SYSTEM
// ============================================================
void UAoCRuntimeUI::UpdatePartyMember(int32 Index, const FPartyMemberInfo& Info)
{
	if (Index < 0 || Index >= 8) return;

	// Show party panel
	if (PartyPanel && PartyPanel->GetVisibility() != ESlateVisibility::Visible)
	{
		PartyPanel->SetVisibility(ESlateVisibility::Visible);
	}

	// Ensure array is big enough
	while (PartyMembers.Num() <= Index) PartyMembers.Add(FPartyMemberInfo());
	PartyMembers[Index] = Info;

	// Update visuals
	if (Index < PartyNameLabels.Num() && PartyNameLabels[Index])
	{
		FString Label = Info.bIsLeader ? FString::Printf(TEXT("★ %s"), *Info.PlayerName) : Info.PlayerName;
		PartyNameLabels[Index]->SetText(FText::FromString(Label));
	}
	if (Index < PartyHPBars.Num() && PartyHPBars[Index])
	{
		PartyHPBars[Index]->SetPercent(Info.MaxHP > 0 ? Info.HP / Info.MaxHP : 0.f);
	}
}

void UAoCRuntimeUI::ClearParty()
{
	PartyMembers.Empty();
	if (PartyPanel) PartyPanel->SetVisibility(ESlateVisibility::Collapsed);

	for (UTextBlock* Label : PartyNameLabels)
	{
		if (Label) Label->SetText(FText::GetEmpty());
	}
	for (UProgressBar* Bar : PartyHPBars)
	{
		if (Bar) Bar->SetPercent(0.f);
	}
}

void UAoCRuntimeUI::SetLootMode(ELootMode Mode)
{
	// Store and notify party members
	UE_LOG(LogTemp, Log, TEXT("AoC: Loot mode set to %s"), *UEnum::GetValueAsString(Mode));
}

// ============================================================
// LOOT WINDOW
// ============================================================
void UAoCRuntimeUI::ShowLootWindow(const FString& SourceName, const TArray<FString>& Items)
{
	if (!LootPanel) return;
	LootPanel->SetVisibility(ESlateVisibility::Visible);

	if (LootTitleText)
	{
		LootTitleText->SetText(FText::FromString(FString::Printf(TEXT("LOOT: %s"), *SourceName)));
	}

	// Populate scroll box with items
	if (LootScrollBox)
	{
		LootScrollBox->ClearChildren();
		for (const FString& Item : Items)
		{
			UTextBlock* ItemText = NewObject<UTextBlock>(LootScrollBox);
			if (ItemText)
			{
				ItemText->SetText(FText::FromString(Item));
				ItemText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				FSlateFontInfo ItemFont = ItemText->GetFont();
				ItemFont.Size = 10;
				ItemText->SetFont(ItemFont);
				LootScrollBox->AddChild(ItemText);
			}
		}
	}

	if (!bGUIMode) ToggleGUIMode();
}

void UAoCRuntimeUI::HideLootWindow()
{
	if (LootPanel) LootPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================
// DEATH SYSTEM
// ============================================================
void UAoCRuntimeUI::ShowDeathScreen(const FString& KillerName)
{
	if (!DeathPanel) return;
	DeathPanel->SetVisibility(ESlateVisibility::Visible);
	if (DeathSubText)
	{
		DeathSubText->SetText(FText::FromString(FString::Printf(TEXT("Slain by: %s"), *KillerName)));
	}
}

void UAoCRuntimeUI::HideDeathScreen()
{
	if (DeathPanel) DeathPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UAoCRuntimeUI::ShowGraveMarker(const FGraveInfo& Grave)
{
	// This would integrate with the minimap/HUD to show grave location
	UE_LOG(LogTemp, Log, TEXT("AoC: Grave marker for %s at (%f, %f, %f) — %d items, %.0f seconds remaining"),
		*Grave.OwnerName, Grave.Location.X, Grave.Location.Y, Grave.Location.Z,
		Grave.Items.Num(), Grave.TimeRemaining);
}

// ============================================================
// SKILL GAIN NOTIFICATION
// ============================================================
void UAoCRuntimeUI::ShowSkillGain(const FString& SkillName, float NewLevel, float XPGained)
{
	if (!SkillToastPanel || !SkillToastText) return;

	SkillToastPanel->SetVisibility(ESlateVisibility::Visible);
	SkillToastText->SetText(FText::FromString(
		FString::Printf(TEXT("⚔ %s leveled up! (%.1f) +%.1f XP"), *SkillName, NewLevel, XPGained)));
	SkillToastTimer = 4.0f; // Show for 4 seconds
}

// ============================================================
// FLOATING DAMAGE TEXT
// ============================================================
void UAoCRuntimeUI::ShowFloatingDamage(const FString& Text, FLinearColor Color, FVector2D ScreenPos)
{
	if (!RootCanvas) return;

	UTextBlock* DmgText = CreateWidget<UTextBlock>(
		FString::Printf(TEXT("Dmg_%d"), FMath::RandRange(0, 99999)));
	if (!DmgText) return;

	DmgText->SetText(FText::FromString(Text));
	DmgText->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo DmgFont = DmgText->GetFont();
	DmgFont.Size = 16;
	DmgText->SetFont(DmgFont);

	AddToCanvas(DmgText, RootCanvas, ScreenPos.X, ScreenPos.Y, 100, 24);

	FFloatingText FT;
	FT.Text = DmgText;
	FT.Velocity = FVector2D(FMath::RandRange(-30.f, 30.f), -80.f); // Float up
	FT.Life = 2.0f;
	FloatingTexts.Add(FT);
}

// ============================================================
// LAYOUT SAVE / LOAD
// ============================================================
void UAoCRuntimeUI::SaveLayout()
{
	// In full implementation, serialize PanelLayouts to SaveGame or config file
	// For now, read positions from current canvas slots
	TArray<TPair<FString, UCanvasPanel*>> AllPanels = {
		{TEXT("HUDBars"), HUDBarsPanel}, {TEXT("Target"), TargetPanel},
		{TEXT("Minimap"), MinimapPanel}, {TEXT("Hotbar"), HotbarPanel},
		{TEXT("Chat"), ChatPanel}, {TEXT("Inventory"), InventoryPanel},
		{TEXT("SpellBook"), SpellBookPanel}, {TEXT("CharSheet"), CharSheetPanel},
		{TEXT("Crafting"), CraftingPanel}, {TEXT("Pause"), PausePanel},
		{TEXT("Quest"), QuestPanel}, {TEXT("Map"), MapPanel},
		{TEXT("Clan"), ClanPanel}, {TEXT("Settings"), SettingsPanel},
		{TEXT("Party"), PartyPanel}
	};

	for (auto& Pair : AllPanels)
	{
		if (Pair.Value)
		{
			UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(Pair.Value->Slot);
			if (PSlot)
			{
				FPanelLayout& Layout = PanelLayouts.FindOrAdd(Pair.Key);
				Layout.Position = PSlot->GetPosition();
				Layout.Size = PSlot->GetSize();
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AoC: UI Layout saved (%d panels)"), AllPanels.Num());
}

void UAoCRuntimeUI::LoadLayout()
{
	// Apply saved positions
	TArray<TPair<FString, UCanvasPanel*>> AllPanels = {
		{TEXT("HUDBars"), HUDBarsPanel}, {TEXT("Target"), TargetPanel},
		{TEXT("Minimap"), MinimapPanel}, {TEXT("Hotbar"), HotbarPanel},
		{TEXT("Chat"), ChatPanel}, {TEXT("Inventory"), InventoryPanel},
		{TEXT("SpellBook"), SpellBookPanel}, {TEXT("CharSheet"), CharSheetPanel},
		{TEXT("Crafting"), CraftingPanel}, {TEXT("Pause"), PausePanel},
		{TEXT("Quest"), QuestPanel}, {TEXT("Map"), MapPanel},
		{TEXT("Clan"), ClanPanel}, {TEXT("Settings"), SettingsPanel},
		{TEXT("Party"), PartyPanel}
	};

	for (auto& Pair : AllPanels)
	{
		if (Pair.Value && PanelLayouts.Contains(Pair.Key))
		{
			const FPanelLayout& Layout = PanelLayouts[Pair.Key];
			UCanvasPanelSlot* PSlot = Cast<UCanvasPanelSlot>(Pair.Value->Slot);
			if (PSlot)
			{
				PSlot->SetPosition(Layout.Position);
				PSlot->SetSize(Layout.Size);
			}
			Pair.Value->SetRenderOpacity(Layout.Opacity);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AoC: UI Layout loaded"));
}

void UAoCRuntimeUI::ResetLayoutToDefaults()
{
	PanelLayouts.Empty();
	SetupDefaultLayouts();
	LoadLayout();
	UE_LOG(LogTemp, Log, TEXT("AoC: UI Layout reset to defaults"));
}
