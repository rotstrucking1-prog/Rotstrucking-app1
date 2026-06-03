// AoCObserveModeOverlay.h
// TerraForge — Observe mode overlay widget.
// Shows 11×11 grid with elevation numbers, color-coded flatness,
// material indicators, and structural data. Darkfall UW aesthetic.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TerraForge/TerraForgeTypes.h"
#include "AoCObserveModeOverlay.generated.h"

class UTerraForgeComponent;

/**
 * Observe mode UI overlay.
 *
 * Replicates Life is Feudal's observe mode:
 * - 11×11 grid centered on player
 * - Each tile shows elevation (meters)
 * - Color-coded: green = flat, white = uneven, red = steep
 * - Material indicator (topsoil/clay/rock/ore)
 * - Quality display for forest soil
 * - Arrow indicators for slope direction
 * - Overhead camera hint
 *
 * Darkfall UW visual style:
 * - Dark translucent background
 * - Steel frame border
 * - Sharp, readable numbers
 */
UCLASS()
class AOC_API UAoCObserveModeOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Refresh the grid with current terrain data. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge|UI")
	void UpdateGrid(UTerraForgeComponent* TerraComp);

	/** Show the observe mode overlay. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge|UI")
	void ShowOverlay();

	/** Hide the observe mode overlay. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge|UI")
	void HideOverlay();

	/** Get the color for a tile based on its slope relative to neighbors. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge|UI")
	static FLinearColor GetTileColor(float HeightDifference);

	/** Get the display character for a material type. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge|UI")
	static FString GetMaterialIndicator(EGeoMaterial Material);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Build the grid layout. */
	void BuildGrid();

	/** Update a single tile's display. */
	void UpdateTile(int32 GridX, int32 GridY, const FObserveTile& TileData);

	/** The grid of text blocks for elevation display. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> GridTiles;

	/** Reference to TerraForge component. */
	UPROPERTY()
	TObjectPtr<UTerraForgeComponent> CachedTerraComp;

	/** Grid container widget. */
	UPROPERTY()
	TObjectPtr<UPanelWidget> GridContainer;

	/** Info panel (bottom strip showing carried dirt, tool, structural state). */
	UPROPERTY()
	TObjectPtr<UPanelWidget> InfoPanel;

	/** Tick counter for throttled updates. */
	float UpdateTimer = 0.0f;

	/** Update interval (seconds). */
	static constexpr float UPDATE_INTERVAL = 0.1f; // 10 Hz grid refresh

	/** Grid dimensions. */
	static constexpr int32 GRID_SIZE = 11;

	/** Tile pixel size. */
	static constexpr float TILE_SIZE = 48.0f;

	// ── Grid Colors ─────────────────────────────────────────────────────────
	static FLinearColor GetGridBgColor()     { return FLinearColor(0.03f, 0.03f, 0.05f, 0.80f); }
	static FLinearColor GetGridBorderColor() { return FLinearColor(0.30f, 0.32f, 0.35f, 0.70f); }
	static FLinearColor GetCenterColor()     { return FLinearColor(0.90f, 0.80f, 0.40f, 0.90f); } // Gold highlight for player pos
	static FLinearColor GetFlatColor()       { return FLinearColor(0.30f, 0.80f, 0.30f, 1.0f); } // Green
	static FLinearColor GetUnevenColor()     { return FLinearColor(0.85f, 0.85f, 0.80f, 1.0f); } // White
	static FLinearColor GetSteepColor()      { return FLinearColor(0.90f, 0.25f, 0.20f, 1.0f); } // Red
	static FLinearColor GetInfoBgColor()     { return FLinearColor(0.05f, 0.05f, 0.07f, 0.85f); }
};
