// AoCTerraformContextMenu.cpp
// Darkfall UW-style terraforming context menu implementation.

#include "AoCTerraformContextMenu.h"
#include "TerraForge/TerraForgeComponent.h"
#include "TerraForge/TerraForgeSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Spacer.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateColor.h"

// ============================================================================
// LIFECYCLE
// ============================================================================

void UAoCTerraformContextMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed);
}

// ============================================================================
// MENU CONTROL
// ============================================================================

void UAoCTerraformContextMenu::ShowMenu(UTerraForgeComponent* TerraComp, const FVector& TargetPos)
{
	TerraComponent = TerraComp;
	MenuTargetPos = TargetPos;

	if (!TerraComponent) return;

	AvailableActions = TerraComponent->GetAvailableActions();
	BuildMenu();
	SetVisibility(ESlateVisibility::Visible);
}

void UAoCTerraformContextMenu::HideMenu()
{
	SetVisibility(ESlateVisibility::Collapsed);
	TerraComponent = nullptr;
	AvailableActions.Empty();
}

void UAoCTerraformContextMenu::SelectAction(int32 Index)
{
	if (!TerraComponent) return;

	if (Index >= 0 && Index < AvailableActions.Num())
	{
		TerraComponent->ExecuteAction(AvailableActions[Index]);
		HideMenu();
	}
}

// ============================================================================
// MENU CONSTRUCTION
// ============================================================================

void UAoCTerraformContextMenu::BuildMenu()
{
	if (!WidgetTree) return;

	// Clear existing
	WidgetTree->RootWidget = nullptr;

	// ── Root: Steel-bordered panel ──────────────────────────────────────────
	UBorder* OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterBorder"));
	OuterBorder->SetBrushColor(GetBorderColor());
	OuterBorder->SetPadding(FMargin(2.0f));

	UBorder* InnerBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InnerBg"));
	InnerBg->SetBrushColor(GetBgColor());
	InnerBg->SetPadding(FMargin(8.0f, 6.0f));

	OuterBorder->SetContent(InnerBg);

	// ── Vertical layout ─────────────────────────────────────────────────────
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuVBox"));
	InnerBg->SetContent(VBox);

	// ── Header: Terrain Info ────────────────────────────────────────────────
	UWidget* Header = CreateTerrainHeader();
	if (Header)
	{
		UVerticalBoxSlot* HeaderSlot = VBox->AddChildToVerticalBox(Header);
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// ── Divider ─────────────────────────────────────────────────────────────
	UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Divider"));
	Divider->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.32f, 0.5f));
	Divider->SetDesiredSizeScale(FVector2D(1.0f, 1.0f));

	USpacer* DivSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DivSpacer"));
	DivSpacer->SetSize(FVector2D(200.0f, 1.0f));
	Divider->SetContent(DivSpacer);

	UVerticalBoxSlot* DivSlot = VBox->AddChildToVerticalBox(Divider);
	DivSlot->SetPadding(FMargin(0.0f, 2.0f));

	// ── Action Title ────────────────────────────────────────────────────────
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("TERRAFORMING")));

	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 11;
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(FSlateColor(GetHeaderColor()));

	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 4.0f));

	// ── Action Rows ─────────────────────────────────────────────────────────
	for (int32 I = 0; I < AvailableActions.Num(); ++I)
	{
		const ETerraAction Action = AvailableActions[I];
		const bool bEnabled = TerraComponent ? TerraComponent->HasCorrectTool(Action) : false;
		FString DisabledReason = bEnabled ? TEXT("") : TEXT("Wrong tool equipped");

		UWidget* Row = CreateActionRow(Action, I + 1, bEnabled, DisabledReason);
		if (Row)
		{
			UVerticalBoxSlot* RowSlot = VBox->AddChildToVerticalBox(Row);
			RowSlot->SetPadding(FMargin(0.0f, 1.0f));
		}
	}

	// ── Footer hint ─────────────────────────────────────────────────────────
	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	HintText->SetText(FText::FromString(TEXT("[Esc] Cancel")));

	FSlateFontInfo HintFont = HintText->GetFont();
	HintFont.Size = 8;
	HintText->SetFont(HintFont);
	HintText->SetColorAndOpacity(FSlateColor(GetDisabledColor()));

	UVerticalBoxSlot* HintSlot = VBox->AddChildToVerticalBox(HintText);
	HintSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	// Set as root
	WidgetTree->RootWidget = OuterBorder;
}

UWidget* UAoCTerraformContextMenu::CreateTerrainHeader()
{
	if (!WidgetTree || !TerraComponent) return nullptr;

	UVerticalBox* HeaderBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("HeaderBox"));

	// Material name
	const EGeoMaterial Mat = TerraComponent->TargetMaterial;
	FString MaterialName = TEXT("Unknown");

	// Get from subsystem's material table
	UWorld* World = GetWorld();
	if (World)
	{
		UTerraForgeSubsystem* Subsystem = World->GetSubsystem<UTerraForgeSubsystem>();
		if (Subsystem)
		{
			const FGeoMaterialProperties* Props = Subsystem->MaterialTable.Find(Mat);
			if (Props)
			{
				MaterialName = Props->DisplayName.ToString();
			}
		}
	}

	// Terrain material
	UTextBlock* MatText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("MatText"));
	MatText->SetText(FText::FromString(FString::Printf(TEXT("Terrain: %s"), *MaterialName)));

	FSlateFontInfo MatFont = MatText->GetFont();
	MatFont.Size = 9;
	MatText->SetFont(MatFont);
	MatText->SetColorAndOpacity(FSlateColor(GetActionColor()));
	HeaderBox->AddChildToVerticalBox(MatText);

	// Elevation
	UTextBlock* ElevText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ElevText"));
	const float ElevM = MenuTargetPos.Z / 100.0f;
	ElevText->SetText(FText::FromString(FString::Printf(TEXT("Elevation: %.1fm"), ElevM)));

	FSlateFontInfo ElevFont = ElevText->GetFont();
	ElevFont.Size = 9;
	ElevText->SetFont(ElevFont);
	ElevText->SetColorAndOpacity(FSlateColor(GetDisabledColor()));
	HeaderBox->AddChildToVerticalBox(ElevText);

	// Structural state
	if (TerraComponent->TargetStructuralState != EStructuralState::Stable)
	{
		UTextBlock* StressText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("StressText"));

		FString StateStr;
		FLinearColor StateColor;
		switch (TerraComponent->TargetStructuralState)
		{
		case EStructuralState::Warning:
			StateStr = TEXT("⚠ Unstable");
			StateColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);
			break;
		case EStructuralState::Critical:
			StateStr = TEXT("⛔ COLLAPSE IMMINENT");
			StateColor = FLinearColor(1.0f, 0.2f, 0.1f, 1.0f);
			break;
		default:
			StateStr = TEXT("Stable");
			StateColor = GetActionColor();
			break;
		}

		StressText->SetText(FText::FromString(StateStr));
		FSlateFontInfo StressFont = StressText->GetFont();
		StressFont.Size = 9;
		StressText->SetFont(StressFont);
		StressText->SetColorAndOpacity(FSlateColor(StateColor));
		HeaderBox->AddChildToVerticalBox(StressText);
	}

	return HeaderBox;
}

UWidget* UAoCTerraformContextMenu::CreateActionRow(
	ETerraAction Action, int32 Index, bool bEnabled, const FString& DisabledReason)
{
	if (!WidgetTree) return nullptr;

	const FString RowName = FString::Printf(TEXT("ActionRow_%d"), Index);

	// Row container with hover highlight
	UBorder* RowBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), *FString::Printf(TEXT("RowBorder_%d"), Index));
	RowBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent, hover changes this
	RowBorder->SetPadding(FMargin(4.0f, 3.0f));

	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), *FString::Printf(TEXT("RowHBox_%d"), Index));
	RowBorder->SetContent(HBox);

	// Hotkey number
	UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), *FString::Printf(TEXT("KeyText_%d"), Index));
	KeyText->SetText(FText::FromString(FString::Printf(TEXT("[%d]"), Index)));

	FSlateFontInfo KeyFont = KeyText->GetFont();
	KeyFont.Size = 10;
	KeyText->SetFont(KeyFont);
	KeyText->SetColorAndOpacity(FSlateColor(bEnabled ? GetHotkeyColor() : GetDisabledColor()));

	UHorizontalBoxSlot* KeySlot = HBox->AddChildToHorizontalBox(KeyText);
	KeySlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	// Action name
	const FText ActionName = UTerraForgeComponent::GetActionDisplayName(Action);
	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), *FString::Printf(TEXT("NameText_%d"), Index));
	NameText->SetText(ActionName);

	FSlateFontInfo NameFont = NameText->GetFont();
	NameFont.Size = 10;
	NameText->SetFont(NameFont);
	NameText->SetColorAndOpacity(FSlateColor(bEnabled ? GetActionColor() : GetDisabledColor()));

	UHorizontalBoxSlot* NameSlot = HBox->AddChildToHorizontalBox(NameText);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // Fill remaining space

	// Tool requirement icon (if different from current)
	if (!bEnabled)
	{
		UTextBlock* ToolText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("ToolText_%d"), Index));
		ToolText->SetText(FText::FromString(DisabledReason));

		FSlateFontInfo ToolFont = ToolText->GetFont();
		ToolFont.Size = 8;
		ToolText->SetFont(ToolFont);
		ToolText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.3f, 0.2f, 0.7f)));

		HBox->AddChildToHorizontalBox(ToolText);
	}

	return RowBorder;
}
