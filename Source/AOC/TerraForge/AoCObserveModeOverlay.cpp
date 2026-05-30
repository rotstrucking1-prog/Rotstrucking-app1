// AoCObserveModeOverlay.cpp
// TerraForge — Observe mode overlay implementation.
// Builds a grid of tiles showing elevation, material, slope, quality.

#include "AoCObserveModeOverlay.h"
#include "TerraForge/TerraForgeComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAoCObserveModeOverlay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed);
	BuildGrid();
}

void UAoCObserveModeOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible() || !CachedTerraComp) return;

	// Throttle updates
	UpdateTimer += InDeltaTime;
	if (UpdateTimer < UPDATE_INTERVAL) return;
	UpdateTimer = 0.0f;

	// Get fresh grid data
	TArray<FObserveTile> GridData = CachedTerraComp->GetObserveGrid();

	// Update each tile
	for (int32 Y = 0; Y < GRID_SIZE; ++Y)
	{
		for (int32 X = 0; X < GRID_SIZE; ++X)
		{
			const int32 Index = Y * GRID_SIZE + X;
			if (Index < GridData.Num())
			{
				UpdateTile(X, Y, GridData[Index]);
			}
		}
	}
}

// ============================================================================
// SHOW / HIDE
// ============================================================================

void UAoCObserveModeOverlay::ShowOverlay()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UAoCObserveModeOverlay::HideOverlay()
{
	SetVisibility(ESlateVisibility::Collapsed);
	CachedTerraComp = nullptr;
}

void UAoCObserveModeOverlay::UpdateGrid(UTerraForgeComponent* TerraComp)
{
	CachedTerraComp = TerraComp;
}

// ============================================================================
// GRID CONSTRUCTION
// ============================================================================

void UAoCObserveModeOverlay::BuildGrid()
{
	if (!WidgetTree) return;

	GridTiles.Empty();
	GridTiles.SetNum(GRID_SIZE * GRID_SIZE);

	// ── Root container ──────────────────────────────────────────────────────
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ObserveRoot"));

	// ── Outer steel frame ───────────────────────────────────────────────────
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GridFrame"));
	Frame->SetBrushColor(GetGridBorderColor());
	Frame->SetPadding(FMargin(2.0f));

	UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	const float TotalSize = GRID_SIZE * TILE_SIZE + 4.0f; // +4 for border
	FrameSlot->SetSize(FVector2D(TotalSize, TotalSize + 60.0f)); // +60 for info panel
	FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	// ── Main vertical layout ────────────────────────────────────────────────
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("MainVBox"));
	Frame->SetContent(MainVBox);

	// ── Background for grid ─────────────────────────────────────────────────
	UBorder* GridBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GridBg"));
	GridBg->SetBrushColor(GetGridBgColor());

	UVerticalBoxSlot* GridBgSlot = MainVBox->AddChildToVerticalBox(GridBg);
	GridBgSlot->SetSize(FSlateChildSize(1.0f)); // Fill

	// ── Grid panel ──────────────────────────────────────────────────────────
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(), TEXT("TerrainGrid"));
	Grid->SetSlotPadding(FMargin(0.5f));
	GridBg->SetContent(Grid);
	GridContainer = Grid;

	// ── Create tiles ────────────────────────────────────────────────────────
	for (int32 Y = 0; Y < GRID_SIZE; ++Y)
	{
		for (int32 X = 0; X < GRID_SIZE; ++X)
		{
			const int32 Index = Y * GRID_SIZE + X;
			const FString TileName = FString::Printf(TEXT("Tile_%d_%d"), X, Y);
			const bool bIsCenter = (X == GRID_SIZE / 2 && Y == GRID_SIZE / 2);

			// Tile border (provides the colored background)
			UBorder* TileBorder = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), *FString::Printf(TEXT("TileBorder_%d"), Index));

			FLinearColor TileBgColor = bIsCenter ?
				FLinearColor(0.15f, 0.12f, 0.05f, 0.9f) :
				FLinearColor(0.06f, 0.06f, 0.08f, 0.6f);
			TileBorder->SetBrushColor(TileBgColor);
			TileBorder->SetPadding(FMargin(1.0f));

			// Tile content: Vertical stack of elevation + material indicator
			UVerticalBox* TileVBox = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), *FString::Printf(TEXT("TileVBox_%d"), Index));
			TileBorder->SetContent(TileVBox);

			// Elevation text
			UTextBlock* ElevText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), *FString::Printf(TEXT("ElevText_%d"), Index));
			ElevText->SetText(FText::FromString(TEXT("--")));
			ElevText->SetJustification(ETextJustify::Center);

			FSlateFontInfo ElevFont = ElevText->GetFont();
			ElevFont.Size = 8;
			ElevText->SetFont(ElevFont);
			ElevText->SetColorAndOpacity(FSlateColor(bIsCenter ? GetCenterColor() : GetUnevenColor()));

			UVerticalBoxSlot* ElevSlot = TileVBox->AddChildToVerticalBox(ElevText);
			ElevSlot->SetHorizontalAlignment(HAlign_Center);
			ElevSlot->SetVerticalAlignment(VAlign_Center);

			// Material indicator (small text below)
			UTextBlock* MatText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), *FString::Printf(TEXT("MatText_%d"), Index));
			MatText->SetText(FText::FromString(TEXT("")));
			MatText->SetJustification(ETextJustify::Center);

			FSlateFontInfo MatFont = MatText->GetFont();
			MatFont.Size = 6;
			MatText->SetFont(MatFont);
			MatText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.7f)));

			UVerticalBoxSlot* MatSlot = TileVBox->AddChildToVerticalBox(MatText);
			MatSlot->SetHorizontalAlignment(HAlign_Center);

			// Add to grid
			UUniformGridSlot* TileGridSlot = Grid->AddChildToUniformGrid(TileBorder, Y, X);

			// Store reference for updates
			GridTiles[Index] = TileBorder;
		}
	}

	// ── Info panel (bottom strip) ───────────────────────────────────────────
	UBorder* InfoBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("InfoBg"));
	InfoBg->SetBrushColor(GetInfoBgColor());
	InfoBg->SetPadding(FMargin(6.0f, 4.0f));

	UHorizontalBox* InfoHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("InfoHBox"));
	InfoBg->SetContent(InfoHBox);
	InfoPanel = InfoHBox;

	// Carried dirt
	UTextBlock* DirtText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DirtText"));
	DirtText->SetText(FText::FromString(TEXT("Dirt: 0/100")));
	FSlateFontInfo DirtFont = DirtText->GetFont();
	DirtFont.Size = 9;
	DirtText->SetFont(DirtFont);
	DirtText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.65f, 0.55f, 1.0f)));
	UHorizontalBoxSlot* DirtSlot = InfoHBox->AddChildToHorizontalBox(DirtText);
	DirtSlot->SetSize(FSlateChildSize(1.0f));

	// Tool text
	UTextBlock* ToolText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ToolText"));
	ToolText->SetText(FText::FromString(TEXT("Tool: Shovel")));
	FSlateFontInfo ToolFont = ToolText->GetFont();
	ToolFont.Size = 9;
	ToolText->SetFont(ToolFont);
	ToolText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.65f, 0.55f, 1.0f)));
	UHorizontalBoxSlot* ToolSlot = InfoHBox->AddChildToHorizontalBox(ToolText);
	ToolSlot->SetSize(FSlateChildSize(1.0f));

	// Skill level
	UTextBlock* SkillText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SkillText"));
	SkillText->SetText(FText::FromString(TEXT("Skill: 1")));
	FSlateFontInfo SkillFont = SkillText->GetFont();
	SkillFont.Size = 9;
	SkillText->SetFont(SkillFont);
	SkillText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.65f, 0.55f, 1.0f)));
	InfoHBox->AddChildToHorizontalBox(SkillText);

	UVerticalBoxSlot* InfoSlot = MainVBox->AddChildToVerticalBox(InfoBg);
	InfoSlot->SetPadding(FMargin(0.0f));

	// ── Hint overlay ────────────────────────────────────────────────────────
	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ObserveHint"));
	HintText->SetText(FText::FromString(TEXT("[G] Exit Observe    [Scroll] Zoom    Green=Flat  White=Uneven  Red=Steep")));

	FSlateFontInfo HintFont = HintText->GetFont();
	HintFont.Size = 8;
	HintText->SetFont(HintFont);
	HintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.6f)));

	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(HintText);
	HintSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	HintSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	HintSlot->SetPosition(FVector2D(0.0f, -20.0f));
	HintSlot->SetAutoSize(true);

	// Set as root
	WidgetTree->RootWidget = Root;
}

// ============================================================================
// TILE UPDATES
// ============================================================================

void UAoCObserveModeOverlay::UpdateTile(int32 GridX, int32 GridY, const FObserveTile& TileData)
{
	const int32 Index = GridY * GRID_SIZE + GridX;
	if (Index < 0 || Index >= GridTiles.Num() || !GridTiles[Index]) return;

	UBorder* TileBorder = Cast<UBorder>(GridTiles[Index]);
	if (!TileBorder) return;

	// Get child widgets
	UVerticalBox* TileVBox = Cast<UVerticalBox>(TileBorder->GetContent());
	if (!TileVBox || TileVBox->GetChildrenCount() < 2) return;

	// Update elevation text
	UTextBlock* ElevText = Cast<UTextBlock>(TileVBox->GetChildAt(0));
	if (ElevText)
	{
		// Display in meters with 1 decimal
		const float ElevM = TileData.Elevation / 100.0f;
		ElevText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), ElevM)));

		// Color based on slope
		const FLinearColor TileColor = GetTileColor(TileData.SlopeAngle);
		const bool bIsCenter = (GridX == GRID_SIZE / 2 && GridY == GRID_SIZE / 2);
		ElevText->SetColorAndOpacity(FSlateColor(bIsCenter ? GetCenterColor() : TileColor));
	}

	// Update material indicator
	UTextBlock* MatText = Cast<UTextBlock>(TileVBox->GetChildAt(1));
	if (MatText)
	{
		MatText->SetText(FText::FromString(GetMaterialIndicator(TileData.SurfaceMaterial)));

		// Color material indicator by type
		FLinearColor MatColor;
		switch (TileData.SurfaceMaterial)
		{
		case EGeoMaterial::Topsoil:     MatColor = FLinearColor(0.40f, 0.30f, 0.15f, 0.8f); break;
		case EGeoMaterial::ForestSoil:  MatColor = FLinearColor(0.25f, 0.50f, 0.20f, 0.8f); break;
		case EGeoMaterial::Clay:        MatColor = FLinearColor(0.65f, 0.40f, 0.25f, 0.8f); break;
		case EGeoMaterial::Sand:        MatColor = FLinearColor(0.85f, 0.80f, 0.55f, 0.8f); break;
		case EGeoMaterial::OreVein:     MatColor = FLinearColor(1.00f, 0.85f, 0.20f, 1.0f); break;
		default:                        MatColor = FLinearColor(0.50f, 0.50f, 0.50f, 0.6f); break;
		}
		MatText->SetColorAndOpacity(FSlateColor(MatColor));
	}

	// Tile background color: subtle coloring by material
	float BgIntensity = 0.08f;
	FLinearColor BgColor;
	switch (TileData.SurfaceMaterial)
	{
	case EGeoMaterial::Topsoil:    BgColor = FLinearColor(0.12f, 0.08f, 0.04f, 0.5f); break;
	case EGeoMaterial::ForestSoil: BgColor = FLinearColor(0.06f, 0.12f, 0.04f, 0.5f); break;
	case EGeoMaterial::Clay:       BgColor = FLinearColor(0.12f, 0.08f, 0.04f, 0.5f); break;
	case EGeoMaterial::Sand:       BgColor = FLinearColor(0.14f, 0.13f, 0.08f, 0.5f); break;
	case EGeoMaterial::OreVein:    BgColor = FLinearColor(0.18f, 0.15f, 0.04f, 0.7f); break;
	default:                       BgColor = FLinearColor(0.06f, 0.06f, 0.08f, 0.5f); break;
	}

	const bool bIsCenter = (GridX == GRID_SIZE / 2 && GridY == GRID_SIZE / 2);
	if (bIsCenter)
	{
		BgColor = FLinearColor(0.15f, 0.12f, 0.05f, 0.9f);
	}

	TileBorder->SetBrushColor(BgColor);
}

// ============================================================================
// UTILITY
// ============================================================================

FLinearColor UAoCObserveModeOverlay::GetTileColor(float HeightDifference)
{
	// < 0.2m difference = flat (green)
	// 0.2-1.0m = uneven (white)
	// > 1.0m = steep (red)

	const float AbsDiff = FMath::Abs(HeightDifference);

	if (AbsDiff < 20.0f) // 0.2m in cm
	{
		return FLinearColor(0.30f, 0.80f, 0.30f, 1.0f); // Green — flat
	}
	else if (AbsDiff < 100.0f) // 1.0m in cm
	{
		// Lerp green → white
		const float T = (AbsDiff - 20.0f) / 80.0f;
		return FMath::Lerp(
			FLinearColor(0.30f, 0.80f, 0.30f, 1.0f),
			FLinearColor(0.85f, 0.85f, 0.80f, 1.0f), T);
	}
	else
	{
		// Lerp white → red
		const float T = FMath::Clamp((AbsDiff - 100.0f) / 200.0f, 0.0f, 1.0f);
		return FMath::Lerp(
			FLinearColor(0.85f, 0.85f, 0.80f, 1.0f),
			FLinearColor(0.90f, 0.25f, 0.20f, 1.0f), T);
	}
}

FString UAoCObserveModeOverlay::GetMaterialIndicator(EGeoMaterial Material)
{
	switch (Material)
	{
	case EGeoMaterial::Topsoil:      return TEXT("SOIL");
	case EGeoMaterial::ForestSoil:   return TEXT("FRST");
	case EGeoMaterial::Clay:         return TEXT("CLAY");
	case EGeoMaterial::Sand:         return TEXT("SAND");
	case EGeoMaterial::Sandstone:    return TEXT("SNDS");
	case EGeoMaterial::Limestone:    return TEXT("LIME");
	case EGeoMaterial::Slate:        return TEXT("SLAT");
	case EGeoMaterial::Granite:      return TEXT("GRNT");
	case EGeoMaterial::Basalt:       return TEXT("BSLT");
	case EGeoMaterial::Marble:       return TEXT("MRBL");
	case EGeoMaterial::Obsidian:     return TEXT("OBSD");
	case EGeoMaterial::Quartzite:    return TEXT("QRTZ");
	case EGeoMaterial::OreVein:      return TEXT("ORE!");
	case EGeoMaterial::Bedrock:      return TEXT("BDRK");
	case EGeoMaterial::WaterTable:   return TEXT("WATR");
	default:                          return TEXT("");
	}
}
