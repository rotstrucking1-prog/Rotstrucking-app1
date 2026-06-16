// AoCRuntimeUI.h — Architect of Creation Master HUD Widget v3
// Darkfall-style moveable UI, 10-column hotbar, autocast, keybinds, all panels
// ONE file to rule them all.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
#include "AoCRuntimeUI.generated.h"

// ============================================================
// ENUMS
// ============================================================

UENUM(BlueprintType)
enum class ECastMode : uint8
{
	PressOnce    UMETA(DisplayName = "Press to Cast"),
	HoldRepeat   UMETA(DisplayName = "Hold to Repeat"),
	ToggleAuto   UMETA(DisplayName = "Toggle Autocast"),
	Instant      UMETA(DisplayName = "Instant (No Queue)"),
	Cycling      UMETA(DisplayName = "Cycling Sequence")
};

UENUM(BlueprintType)
enum class EChatChannel : uint8
{
	Local,
	Shout,
	Clan,
	Party,
	Whisper,
	Trade,
	System,
	Combat
};

UENUM(BlueprintType)
enum class ELootMode : uint8
{
	FreeLoot,
	RoundRobin,
	NeedGreed,
	MasterLooter
};

// ============================================================
// STRUCTS
// ============================================================

USTRUCT(BlueprintType)
struct FHotbarSlot
{
	GENERATED_BODY()
	UPROPERTY() FString ItemID;        // Spell ID, Weapon ID, Item ID, or Skill ID
	UPROPERTY() FString DisplayName;
	UPROPERTY() FString IconPath;
	UPROPERTY() ECastMode CastMode = ECastMode::PressOnce;
	UPROPERTY() int32 Quantity = 0;    // For stackable items
	UPROPERTY() float CooldownRemaining = 0.f;
	UPROPERTY() float CooldownTotal = 0.f;
	UPROPERTY() int32 CycleIndex = 0;  // For Cycling mode
	UPROPERTY() bool bIsAutocastActive = false;
};

USTRUCT(BlueprintType)
struct FPanelLayout
{
	GENERATED_BODY()
	UPROPERTY() FVector2D Position = FVector2D::ZeroVector;
	UPROPERTY() FVector2D Size = FVector2D(300, 200);
	UPROPERTY() float Opacity = 1.0f;
	UPROPERTY() bool bLockedToAction = false;  // Visible in Action Mode
	UPROPERTY() bool bVisible = true;
};

USTRUCT(BlueprintType)
struct FKeybind
{
	GENERATED_BODY()
	UPROPERTY() FKey PrimaryKey;
	UPROPERTY() FKey SecondaryKey;
	UPROPERTY() FString ActionName;
};

USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()
	UPROPERTY() FString SenderName;
	UPROPERTY() FString Message;
	UPROPERTY() EChatChannel Channel = EChatChannel::Local;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
	UPROPERTY() float Timestamp = 0.f;
};

USTRUCT(BlueprintType)
struct FPartyMemberInfo
{
	GENERATED_BODY()
	UPROPERTY() FString PlayerName;
	UPROPERTY() float HP = 100.f;
	UPROPERTY() float MaxHP = 100.f;
	UPROPERTY() float MP = 100.f;
	UPROPERTY() float MaxMP = 100.f;
	UPROPERTY() bool bIsLeader = false;
	UPROPERTY() FVector WorldLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FGraveInfo
{
	GENERATED_BODY()
	UPROPERTY() FString OwnerName;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() float TimeRemaining = 7200.f; // 2 hours
	UPROPERTY() TArray<FString> Items;
};

// ============================================================
// MAIN CLASS
// ============================================================

UCLASS(Blueprintable)
class AOC_API UAoCRuntimeUI : public UUserWidget
{
	GENERATED_BODY()

public:
	// ==================== MODE TOGGLES ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleGUIMode();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") bool IsInGUIMode() const { return bGUIMode; }

	// ==================== PANEL TOGGLES ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleInventory();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleSpellBook();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleCharacterSheet();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleCrafting();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void TogglePauseMenu();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleQuestLog();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleWorldMap();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleClanPanel();
	UFUNCTION(BlueprintCallable, Category = "AoC|UI") void ToggleSettings();

	// ==================== HOTBAR ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Hotbar") void ActivateHotbarSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category = "AoC|Hotbar") void SwitchHotbarColumn(int32 ColumnIndex);
	UFUNCTION(BlueprintCallable, Category = "AoC|Hotbar") void SetHotbarSlot(int32 Column, int32 SlotNum, const FHotbarSlot& Data);
	UFUNCTION(BlueprintCallable, Category = "AoC|Hotbar") void ClearHotbarSlot(int32 Column, int32 SlotNum);
	UFUNCTION(BlueprintCallable, Category = "AoC|Hotbar") int32 GetActiveColumn() const { return ActiveHotbarColumn; }

	// ==================== HUD UPDATES ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateHP(float Current, float Max);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateMP(float Current, float Max);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateStamina(float Current, float Max);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateXP(float Current, float Max);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateGold(int32 Amount);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateTargetInfo(const FString& Name, float HP, float MaxHP);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void ClearTarget();
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateZoneName(const FString& ZoneName);
	UFUNCTION(BlueprintCallable, Category = "AoC|HUD") void UpdateCompass(float YawDegrees);

	// ==================== DIALOGUE ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue")
	void ShowDialogue(const FString& NPCName, const FString& DialogueText, const TArray<FString>& Responses);
	UFUNCTION(BlueprintCallable, Category = "AoC|Dialogue") void HideDialogue();

	// ==================== CHAT ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Chat")
	void AddChatMessage(const FString& Sender, const FString& Msg, EChatChannel Channel);
	UFUNCTION(BlueprintCallable, Category = "AoC|Chat") void SetChatFilter(EChatChannel Channel, bool bShow);

	// ==================== PARTY ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Party")
	void UpdatePartyMember(int32 Index, const FPartyMemberInfo& Info);
	UFUNCTION(BlueprintCallable, Category = "AoC|Party") void ClearParty();
	UFUNCTION(BlueprintCallable, Category = "AoC|Party") void SetLootMode(ELootMode Mode);

	// ==================== LOOT WINDOW ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Loot")
	void ShowLootWindow(const FString& SourceName, const TArray<FString>& Items);
	UFUNCTION(BlueprintCallable, Category = "AoC|Loot") void HideLootWindow();

	// ==================== DEATH / GRAVE ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Death")
	void ShowDeathScreen(const FString& KillerName);
	UFUNCTION(BlueprintCallable, Category = "AoC|Death") void HideDeathScreen();
	UFUNCTION(BlueprintCallable, Category = "AoC|Death")
	void ShowGraveMarker(const FGraveInfo& Grave);

	// ==================== SKILL NOTIFICATIONS ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Skills")
	void ShowSkillGain(const FString& SkillName, float NewLevel, float XPGained);
	UFUNCTION(BlueprintCallable, Category = "AoC|Skills")
	void ShowFloatingDamage(const FString& Text, FLinearColor Color, FVector2D ScreenPos);

	// ==================== PANEL DRAG / LAYOUT ====================
	UFUNCTION(BlueprintCallable, Category = "AoC|Layout")
	void SaveLayout();
	UFUNCTION(BlueprintCallable, Category = "AoC|Layout")
	void LoadLayout();
	UFUNCTION(BlueprintCallable, Category = "AoC|Layout")
	void ResetLayoutToDefaults();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// ==================== STATE ====================
	bool bGUIMode = false;
	int32 ActiveHotbarColumn = 0;
	bool bDraggingPanel = false;
	FString DraggedPanelName;
	FVector2D DragOffset = FVector2D::ZeroVector;

	// ==================== THEME COLORS ====================
	static constexpr float CrimsonR = 0.7f;
	static constexpr float CrimsonG = 0.1f;
	static constexpr float CrimsonB = 0.1f;
	FLinearColor Crimson() const { return FLinearColor(CrimsonR, CrimsonG, CrimsonB, 1.f); }
	FLinearColor DarkBG() const { return FLinearColor(0.05f, 0.03f, 0.03f, 0.92f); }
	FLinearColor PanelBG() const { return FLinearColor(0.08f, 0.05f, 0.05f, 0.9f); }
	FLinearColor SlotBG() const { return FLinearColor(0.12f, 0.08f, 0.08f, 0.85f); }
	FLinearColor BarRed() const { return FLinearColor(0.8f, 0.15f, 0.15f, 1.f); }
	FLinearColor BarBlue() const { return FLinearColor(0.15f, 0.25f, 0.8f, 1.f); }
	FLinearColor BarYellow() const { return FLinearColor(0.8f, 0.7f, 0.15f, 1.f); }
	FLinearColor BarGreen() const { return FLinearColor(0.15f, 0.75f, 0.25f, 1.f); }

	// ==================== DATA ====================
	UPROPERTY() TArray<FHotbarSlot> HotbarData; // 100 slots (10 columns × 10 slots)
	TMap<FString, FPanelLayout> PanelLayouts;
	TArray<FChatMessage> ChatLog;
	TArray<FPartyMemberInfo> PartyMembers;
	TArray<bool> ChatFilters; // per EChatChannel

	// ==================== ROOT ====================
	UPROPERTY() UCanvasPanel* RootCanvas = nullptr;

	// ==================== HUD (Always-Visible) ====================
	UPROPERTY() UCanvasPanel* HUDBarsPanel = nullptr;
	UPROPERTY() UProgressBar* HPBar = nullptr;
	UPROPERTY() UProgressBar* MPBar = nullptr;
	UPROPERTY() UProgressBar* StaminaBar = nullptr;
	UPROPERTY() UProgressBar* XPBar = nullptr;
	UPROPERTY() UTextBlock* HPText = nullptr;
	UPROPERTY() UTextBlock* MPText = nullptr;
	UPROPERTY() UTextBlock* StaminaText = nullptr;
	UPROPERTY() UTextBlock* GoldText = nullptr;
	UPROPERTY() UTextBlock* XPText = nullptr;

	// ==================== TARGET FRAME ====================
	UPROPERTY() UCanvasPanel* TargetPanel = nullptr;
	UPROPERTY() UTextBlock* TargetNameText = nullptr;
	UPROPERTY() UProgressBar* TargetHPBar = nullptr;

	// ==================== MINIMAP ====================
	UPROPERTY() UCanvasPanel* MinimapPanel = nullptr;
	UPROPERTY() UImage* MinimapBG = nullptr;
	UPROPERTY() UImage* MinimapRing = nullptr;
	UPROPERTY() UTextBlock* ZoneNameText = nullptr;
	UPROPERTY() UTextBlock* CoordText = nullptr;
	UPROPERTY() UTextBlock* CompassText = nullptr;

	// ==================== HOTBAR ====================
	UPROPERTY() UCanvasPanel* HotbarPanel = nullptr;
	UPROPERTY() TArray<UBorder*> HotbarSlotBorders;
	UPROPERTY() TArray<UTextBlock*> HotbarSlotLabels;
	UPROPERTY() TArray<UTextBlock*> HotbarKeyLabels;
	UPROPERTY() UTextBlock* ColumnIndicator = nullptr;

	// ==================== CHAT ====================
	UPROPERTY() UCanvasPanel* ChatPanel = nullptr;
	UPROPERTY() UScrollBox* ChatScrollBox = nullptr;
	UPROPERTY() UEditableTextBox* ChatInput = nullptr;
	UPROPERTY() TArray<UButton*> ChatTabButtons;

	// ==================== TOGGLE PANELS ====================
	UPROPERTY() UCanvasPanel* InventoryPanel = nullptr;
	UPROPERTY() UCanvasPanel* SpellBookPanel = nullptr;
	UPROPERTY() UCanvasPanel* CharSheetPanel = nullptr;
	UPROPERTY() UCanvasPanel* CraftingPanel = nullptr;
	UPROPERTY() UCanvasPanel* PausePanel = nullptr;
	UPROPERTY() UCanvasPanel* QuestPanel = nullptr;
	UPROPERTY() UCanvasPanel* MapPanel = nullptr;
	UPROPERTY() UCanvasPanel* ClanPanel = nullptr;
	UPROPERTY() UCanvasPanel* SettingsPanel = nullptr;

	// ==================== DIALOGUE ====================
	UPROPERTY() UCanvasPanel* DialoguePanel = nullptr;
	UPROPERTY() UTextBlock* DialogueNPCName = nullptr;
	UPROPERTY() UTextBlock* DialogueBodyText = nullptr;
	UPROPERTY() TArray<UButton*> DialogueButtons;
	UPROPERTY() TArray<UTextBlock*> DialogueButtonLabels;

	// ==================== LOOT WINDOW ====================
	UPROPERTY() UCanvasPanel* LootPanel = nullptr;
	UPROPERTY() UTextBlock* LootTitleText = nullptr;
	UPROPERTY() UScrollBox* LootScrollBox = nullptr;

	// ==================== DEATH SCREEN ====================
	UPROPERTY() UCanvasPanel* DeathPanel = nullptr;
	UPROPERTY() UTextBlock* DeathText = nullptr;
	UPROPERTY() UTextBlock* DeathSubText = nullptr;

	// ==================== PARTY FRAMES ====================
	UPROPERTY() UCanvasPanel* PartyPanel = nullptr;
	UPROPERTY() TArray<UProgressBar*> PartyHPBars;
	UPROPERTY() TArray<UTextBlock*> PartyNameLabels;

	// ==================== SKILL GAIN TOAST ====================
	UPROPERTY() UCanvasPanel* SkillToastPanel = nullptr;
	UPROPERTY() UTextBlock* SkillToastText = nullptr;
	float SkillToastTimer = 0.f;

	// ==================== FLOATING DAMAGE ====================
	struct FFloatingText { UTextBlock* Text = nullptr; FVector2D Velocity; float Life; };
	TArray<FFloatingText> FloatingTexts;

	// ==================== BUILD FUNCTIONS ====================
	void BuildRootCanvas();
	void BuildHUDBars();
	void BuildTargetFrame();
	void BuildMinimap();
	void BuildHotbar();
	void BuildChatPanel();
	void BuildInventoryPanel();
	void BuildSpellBookPanel();
	void BuildCharSheetPanel();
	void BuildCraftingPanel();
	void BuildPauseMenuPanel();
	void BuildDialoguePanel();
	void BuildLootWindow();
	void BuildDeathScreen();
	void BuildPartyFrames();
	void BuildQuestPanel();
	void BuildMapPanel();
	void BuildClanPanel();
	void BuildSettingsPanel();
	void BuildSkillToast();
	void SetupDefaultLayouts();

	// ==================== HELPERS ====================
	template<typename T>
	T* CreateWidget(const FString& Name);

	UCanvasPanelSlot* AddToCanvas(UWidget* Widget, UCanvasPanel* Parent,
		float X, float Y, float W, float H);

	UProgressBar* MakeBar(UCanvasPanel* P, FLinearColor Fill, float X, float Y, float W, float H);
	UTextBlock* MakeText(UCanvasPanel* P, const FString& Txt, float X, float Y,
		float FontSz = 12.f, FLinearColor Col = FLinearColor::White);
	UImage* MakeImage(UCanvasPanel* P, FLinearColor Tint, float X, float Y, float W, float H);
	UButton* MakeButton(UCanvasPanel* P, const FString& Label, float X, float Y, float W, float H,
		FLinearColor BgCol = FLinearColor(0.15f, 0.05f, 0.05f, 0.9f));
	UBorder* MakeBorder(UCanvasPanel* P, FLinearColor BgCol, float X, float Y, float W, float H);
	UCanvasPanel* MakePanel(UCanvasPanel* P, const FString& Name, float X, float Y, float W, float H);

	void TogglePanelVisibility(UCanvasPanel* Panel, const FString& PanelName, bool& bState);
	void SetPanelPosition(UCanvasPanel* Panel, const FString& Name, float X, float Y);
	UCanvasPanel* FindPanelUnderMouse(FVector2D MousePos);
	FVector2D GetPanelPosition(UCanvasPanel* Panel);
};
