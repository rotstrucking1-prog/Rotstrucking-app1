// AoCTerraformContextMenu.h
// Darkfall UW-style right-click context menu for terraforming.
// Steel border, dark background, glowing action text, tool-aware.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerraForge/TerraForgeTypes.h"
#include "AoCTerraformContextMenu.generated.h"

class UTerraForgeComponent;

/**
 * Right-click context menu for terraforming actions.
 *
 * Visual style: Darkfall Unholy Wars
 * - Dark steel-frame border with subtle glow
 * - Translucent black background
 * - Each action is a hoverable row with icon + name + key hint
 * - Unavailable actions are grayed out with reason tooltip
 * - Shows terrain info (material, elevation, quality) in header
 */
UCLASS()
class AOC_API UAoCTerraformContextMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize and populate the menu with available actions. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge|UI")
	void ShowMenu(UTerraForgeComponent* TerraComp, const FVector& TargetPos);

	/** Hide and clean up the menu. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge|UI")
	void HideMenu();

	/** Select an action by index (called from keyboard shortcut). */
	UFUNCTION(BlueprintCallable, Category = "TerraForge|UI")
	void SelectAction(int32 Index);

protected:
	virtual void NativeOnInitialized() override;

private:
	/** Build the menu UI programmatically. */
	void BuildMenu();

	/** Create a single action row widget. */
	UWidget* CreateActionRow(ETerraAction Action, int32 Index, bool bEnabled, const FString& DisabledReason);

	/** Create the terrain info header. */
	UWidget* CreateTerrainHeader();

	/** The TerraForge component driving this menu. */
	UPROPERTY()
	TObjectPtr<UTerraForgeComponent> TerraComponent;

	/** Available actions list. */
	TArray<ETerraAction> AvailableActions;

	/** Menu container. */
	UPROPERTY()
	TObjectPtr<UPanelWidget> MenuContainer;

	/** Target position for display. */
	FVector MenuTargetPos = FVector::ZeroVector;

	// ── Darkfall UW Style Colors ────────────────────────────────────────────
	static constexpr float BORDER_ALPHA    = 0.85f;
	static constexpr float BG_ALPHA        = 0.75f;

	static FLinearColor GetBorderColor()      { return FLinearColor(0.35f, 0.38f, 0.42f, BORDER_ALPHA); }
	static FLinearColor GetBgColor()          { return FLinearColor(0.05f, 0.05f, 0.07f, BG_ALPHA); }
	static FLinearColor GetHeaderColor()      { return FLinearColor(0.80f, 0.75f, 0.60f, 1.0f); } // Gold
	static FLinearColor GetActionColor()      { return FLinearColor(0.85f, 0.85f, 0.82f, 1.0f); } // Off-white
	static FLinearColor GetDisabledColor()    { return FLinearColor(0.40f, 0.40f, 0.38f, 0.6f); } // Gray
	static FLinearColor GetHotkeyColor()      { return FLinearColor(0.60f, 0.55f, 0.40f, 0.9f); } // Dim gold
	static FLinearColor GetHighlightColor()   { return FLinearColor(0.55f, 0.50f, 0.35f, 0.3f); } // Hover
};
