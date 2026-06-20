// AoCHUDWidget.h — Full C++ HUD widget (medieval/dark theme)
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AoCHUDWidget.generated.h"

class UCanvasPanel;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UBorder;
class UScrollBox;
class UEditableTextBox;
class UHorizontalBox;
class UVerticalBox;
class UButton;
class USizeBox;
class UImage;

/**
 * UAoCHUDWidget
 *
 * Complete in-game HUD built entirely in C++.
 * Dark medieval theme: background #1a1a2e, crimson #dc143c, bone text #e8d5b7.
 * Creates all widgets programmatically in NativeConstruct() — NO Blueprint wiring needed.
 */
UCLASS()
class AOC_API UAoCHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// --- Panel show/hide API ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowDeathScreen();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideDeathScreen();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowTargetFrame(const FString& TargetName, float HealthPercent, float Distance);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideTargetFrame();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void AddChatMessage(const FString& Channel, const FString& Message);

	// --- Stat update API -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdatePlayerStats(float HP, float MaxHP, float MP, float MaxMP,
		float Stamina, float MaxStamina, float XP, float MaxXP, int32 Gold);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdateMinimap(const FString& ZoneName, float X, float Y);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdateTargetStats(float HealthPercent, float Distance);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetRespawnTimer(float SecondsRemaining);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	// --- Theme colors ----------------------------------------------------------

	static FLinearColor DarkBg()       { return FLinearColor(0.102f, 0.102f, 0.180f, 0.85f); }   // #1a1a2e
	static FLinearColor CrimsonAccent(){ return FLinearColor(0.863f, 0.078f, 0.235f, 1.f); }       // #dc143c
	static FLinearColor BoneText()     { return FLinearColor(0.910f, 0.835f, 0.718f, 1.f); }       // #e8d5b7
	static FLinearColor HPRed()        { return FLinearColor(0.8f, 0.1f, 0.1f, 1.f); }
	static FLinearColor MPBlue()       { return FLinearColor(0.15f, 0.25f, 0.85f, 1.f); }
	static FLinearColor StamGreen()    { return FLinearColor(0.15f, 0.7f, 0.15f, 1.f); }
	static FLinearColor XPGold()       { return FLinearColor(0.85f, 0.75f, 0.2f, 1.f); }
	static FLinearColor BarBg()        { return FLinearColor(0.08f, 0.08f, 0.12f, 0.9f); }

	// --- Root ------------------------------------------------------------------

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;

	// --- Top-Left: Player Bars -------------------------------------------------

	UPROPERTY() TObjectPtr<UVerticalBox> PlayerBarsBox;
	UPROPERTY() TObjectPtr<UProgressBar> HPBar;
	UPROPERTY() TObjectPtr<UTextBlock>   HPText;
	UPROPERTY() TObjectPtr<UProgressBar> MPBar;
	UPROPERTY() TObjectPtr<UTextBlock>   MPText;
	UPROPERTY() TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY() TObjectPtr<UTextBlock>   StaminaText;
	UPROPERTY() TObjectPtr<UProgressBar> XPBar;
	UPROPERTY() TObjectPtr<UTextBlock>   XPText;
	UPROPERTY() TObjectPtr<UTextBlock>   GoldText;

	// --- Top-Right: Minimap placeholder ----------------------------------------

	UPROPERTY() TObjectPtr<UVerticalBox> MinimapBox;
	UPROPERTY() TObjectPtr<UBorder>      MinimapFrame;
	UPROPERTY() TObjectPtr<UTextBlock>   ZoneNameText;
	UPROPERTY() TObjectPtr<UTextBlock>   CoordinatesText;

	// --- Top-Center: Target Frame ----------------------------------------------

	UPROPERTY() TObjectPtr<UVerticalBox> TargetFrameBox;
	UPROPERTY() TObjectPtr<UTextBlock>   TargetNameText;
	UPROPERTY() TObjectPtr<UProgressBar> TargetHPBar;
	UPROPERTY() TObjectPtr<UTextBlock>   TargetDistText;

	// --- Bottom-Center: Hotbar -------------------------------------------------

	UPROPERTY() TObjectPtr<UHorizontalBox> HotbarBox;
	UPROPERTY() TArray<TObjectPtr<UBorder>> HotbarSlots;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> HotbarLabels;

	// --- Bottom-Left: Chat Box -------------------------------------------------

	UPROPERTY() TObjectPtr<UVerticalBox>       ChatBox;
	UPROPERTY() TObjectPtr<UScrollBox>         ChatScrollBox;
	UPROPERTY() TObjectPtr<UEditableTextBox>   ChatInput;

	// --- Center: Death Screen --------------------------------------------------

	UPROPERTY() TObjectPtr<UOverlay>     DeathOverlay;
	UPROPERTY() TObjectPtr<UTextBlock>   DeathTitleText;
	UPROPERTY() TObjectPtr<UTextBlock>   DeathTimerText;
	UPROPERTY() TObjectPtr<UButton>      RespawnButton;
	UPROPERTY() TObjectPtr<UTextBlock>   RespawnButtonLabel;

	// --- Builder helpers -------------------------------------------------------

	/** Create a styled progress bar */
	UProgressBar* MakeBar(FLinearColor FillColor);

	/** Create a styled text block */
	UTextBlock* MakeText(const FString& Initial, int32 FontSize = 14, FLinearColor Color = FLinearColor::White);

	/** Create a styled border */
	UBorder* MakeBorder(FLinearColor BgColor, float PadH = 4.f, float PadV = 2.f);

	/** Build the top-left player bars panel */
	void BuildPlayerBars();

	/** Build the top-right minimap placeholder */
	void BuildMinimap();

	/** Build the top-center target frame */
	void BuildTargetFrame();

	/** Build the bottom-center hotbar */
	void BuildHotbar();

	/** Build the bottom-left chat box */
	void BuildChatBox();

	/** Build the center death screen overlay */
	void BuildDeathScreen();

	/** Utility: add a widget to canvas at a given anchor/offset */
	void AddToCanvas(UWidget* Widget, FVector2D Alignment, FVector2D Position, FVector2D Size = FVector2D::ZeroVector, bool bAutoSize = true);
};
