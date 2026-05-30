// AoCHUDWidget.h — Darkfall Unholy Wars–style HUD (C++ only, no Blueprints)
// Architect of Creation — full game HUD: vitals, crosshair, look-at tooltip,
// context menus, notifications, hotbar, inventory, skills, death screen.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AoCHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UImage;
class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UProgressBar;
class USizeBox;
class UOverlay;
class AAoCPlayerPawn;

// ─────────────────────────────────────────────────────────────────────────────
// Notification entry
// ─────────────────────────────────────────────────────────────────────────────
struct FAoCNotification
{
	UTextBlock* TextWidget = nullptr;
	float TimeRemaining   = 5.f;
};

// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class AOC_API UAoCHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════
	//  Public API — called by AoCPlayerPawn
	// ═══════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowLookAtTooltip(const FString& Name, const FString& Description);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideLookAtTooltip();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowContextMenu(const FString& Title, const TArray<FString>& Options);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideContextMenu();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowNotification(const FString& Message, FLinearColor Color = FLinearColor(0.91f, 0.86f, 0.78f, 1.f));

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowInventory();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideInventory();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowSkills();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideSkills();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleSkills();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowDeathScreen();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideDeathScreen();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetRespawnTimer(float SecondsRemaining);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// ═══════════════════════════════════════════════════════════════════════
	//  Build helpers
	// ═══════════════════════════════════════════════════════════════════════
	void BuildHUD();
	void BuildPlayerVitals();
	void BuildCrosshair();
	void BuildLookAtTooltip();
	void BuildContextMenu();
	void BuildNotificationArea();
	void BuildHotbar();
	void BuildInventoryPanel();
	void BuildSkillsPanel();
	void BuildDeathScreen();
	void BuildKeyHints();

	// Widget factory helpers
	UTextBlock* MakeText(const FString& Content, int32 FontSize, FLinearColor Color, ETextJustify::Type Justify = ETextJustify::Left);
	UBorder* MakePanel(FLinearColor BgColor);
	UProgressBar* MakeBar(FLinearColor FillColor, float InitPercent = 1.f);
	UCanvasPanelSlot* PlaceOnCanvas(UWidget* Widget, FVector2D AnchorMin, FVector2D AnchorMax, FVector2D Position, FVector2D Size, FVector2D Alignment, bool bAutoSize = false);

	// ═══════════════════════════════════════════════════════════════════════
	//  Widget tree pointers
	// ═══════════════════════════════════════════════════════════════════════

	// Root
	UPROPERTY() UCanvasPanel* RootCanvas = nullptr;

	// --- Player Vitals (top-center) ---
	UPROPERTY() UBorder* VitalsPanel = nullptr;
	UPROPERTY() UTextBlock* PlayerNameText = nullptr;
	UPROPERTY() UProgressBar* HealthBar = nullptr;
	UPROPERTY() UProgressBar* ManaBar = nullptr;
	UPROPERTY() UProgressBar* StaminaBar = nullptr;
	UPROPERTY() UTextBlock* HealthValueText = nullptr;
	UPROPERTY() UTextBlock* ManaValueText = nullptr;
	UPROPERTY() UTextBlock* StaminaValueText = nullptr;
	UPROPERTY() UTextBlock* HealthLabel = nullptr;
	UPROPERTY() UTextBlock* ManaLabel = nullptr;
	UPROPERTY() UTextBlock* StaminaLabel = nullptr;

	// --- Crosshair (center) ---
	UPROPERTY() UBorder* CrosshairDot = nullptr;

	// --- Look-at tooltip (below crosshair) ---
	UPROPERTY() UBorder* LookAtPanel = nullptr;
	UPROPERTY() UTextBlock* LookAtNameText = nullptr;
	UPROPERTY() UTextBlock* LookAtDescText = nullptr;

	// --- Context menu (center-right) ---
	UPROPERTY() UBorder* ContextMenuPanel = nullptr;
	UPROPERTY() UTextBlock* ContextMenuTitle = nullptr;
	UPROPERTY() UVerticalBox* ContextMenuItems = nullptr;

	// --- Notifications (bottom-left) ---
	UPROPERTY() UVerticalBox* NotificationBox = nullptr;
	TArray<FAoCNotification> ActiveNotifications;

	// --- Hotbar (bottom-center) ---
	UPROPERTY() UBorder* HotbarPanel = nullptr;
	UPROPERTY() UHorizontalBox* HotbarRow = nullptr;

	// --- Inventory panel (toggled with I) ---
	UPROPERTY() UBorder* InventoryPanel = nullptr;
	UPROPERTY() UTextBlock* InventoryTitle = nullptr;
	UPROPERTY() UVerticalBox* InventorySlots = nullptr;
	bool bInventoryOpen = false;

	// --- Skills panel (toggled with Tab) ---
	UPROPERTY() UBorder* SkillsPanel = nullptr;
	UPROPERTY() UTextBlock* SkillsTitle = nullptr;
	UPROPERTY() UVerticalBox* SkillsList = nullptr;
	bool bSkillsOpen = false;

	// --- Death screen (full overlay) ---
	UPROPERTY() UBorder* DeathOverlay = nullptr;
	UPROPERTY() UTextBlock* DeathText = nullptr;
	UPROPERTY() UTextBlock* DeathTimerText = nullptr;

	// --- Key hints (bottom-center) ---
	UPROPERTY() UTextBlock* KeyHintText = nullptr;

	// --- Mode tracking (to avoid rebuilding context menu every frame) ---
	bool bLastTerraformShown = false;
	bool bLastTunnelShown = false;

	// --- Cached owner ---
	TWeakObjectPtr<AAoCPlayerPawn> OwnerPawn;
};
