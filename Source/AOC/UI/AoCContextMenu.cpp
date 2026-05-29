// AoCContextMenu.cpp
// Architect of Creation - Context Menu Manager Implementation

#include "AoCContextMenu.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────────────────────────
// Construction / lifecycle
// ─────────────────────────────────────────────────────────────────────────────

UAoCContextMenuManager::UAoCContextMenuManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	ActiveMenuWidget = nullptr;
	CachedTargetActor = nullptr;
	CachedMenuType = EContextMenuType::GenericInteract;
}

void UAoCContextMenuManager::BeginPlay()
{
	Super::BeginPlay();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UAoCContextMenuManager::ShowContextMenu(FVector2D ScreenPosition, EContextMenuType MenuType, AActor* TargetActor)
{
	// Close any existing menu first
	HideContextMenu();

	CachedMenuType = MenuType;
	CachedTargetActor = TargetActor;

	// Create widget if class is assigned
	if (ContextMenuWidgetClass)
	{
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (PC)
		{
			ActiveMenuWidget = CreateWidget<UUserWidget>(PC, ContextMenuWidgetClass);
			if (ActiveMenuWidget)
			{
				ActiveMenuWidget->AddToViewport(100); // High Z-order
				ActiveMenuWidget->SetPositionInViewport(ScreenPosition);
			}
		}
	}

	OnMenuOpened.Broadcast(MenuType, TargetActor);

	UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Opened menu type %d at (%.0f, %.0f)"),
		static_cast<int32>(MenuType), ScreenPosition.X, ScreenPosition.Y);
}

void UAoCContextMenuManager::HideContextMenu()
{
	if (ActiveMenuWidget)
	{
		ActiveMenuWidget->RemoveFromParent();
		ActiveMenuWidget = nullptr;
	}

	CachedTargetActor = nullptr;
	OnMenuClosed.Broadcast();
}

bool UAoCContextMenuManager::IsMenuVisible() const
{
	return ActiveMenuWidget != nullptr && ActiveMenuWidget->IsInViewport();
}

TArray<FContextMenuOption> UAoCContextMenuManager::GetMenuOptions(EContextMenuType MenuType, AActor* TargetActor) const
{
	switch (MenuType)
	{
	case EContextMenuType::ShovelOnGround:
		return BuildShovelOnGroundOptions(TargetActor);

	case EContextMenuType::PickaxeOnGround:
		return BuildPickaxeOnGroundOptions(TargetActor);

	case EContextMenuType::PickaxeOnRock:
		return BuildPickaxeOnRockOptions(TargetActor);

	case EContextMenuType::FurnaceInteract:
		return BuildFurnaceOptions(TargetActor);

	case EContextMenuType::FarmTileInteract:
		return BuildFarmTileOptions(TargetActor);

	case EContextMenuType::GenericInteract:
	default:
		return BuildGenericOptions(TargetActor);
	}
}

void UAoCContextMenuManager::OnOptionSelected(FName OptionID)
{
	OnOptionChosen.Broadcast(OptionID, CachedTargetActor);

	// Dispatch to the correct system based on cached menu type
	switch (CachedMenuType)
	{
	case EContextMenuType::ShovelOnGround:
		DispatchShovelAction(OptionID);
		break;

	case EContextMenuType::PickaxeOnGround:
	case EContextMenuType::PickaxeOnRock:
		DispatchPickaxeAction(OptionID);
		break;

	case EContextMenuType::FurnaceInteract:
		DispatchFurnaceAction(OptionID);
		break;

	case EContextMenuType::FarmTileInteract:
		DispatchFarmAction(OptionID);
		break;

	case EContextMenuType::GenericInteract:
	default:
		DispatchGenericAction(OptionID);
		break;
	}

	// Close menu after selection
	HideContextMenu();
}

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

FContextMenuOption UAoCContextMenuManager::MakeOption(
	FName InID,
	const FString& InText,
	bool bInEnabled,
	bool bSkillCheck,
	float Skill,
	const FString& InIconPath)
{
	FContextMenuOption Opt;
	Opt.OptionID = InID;
	Opt.DisplayText = InText;
	Opt.bEnabled = bInEnabled;
	Opt.bRequiresSkillCheck = bSkillCheck;
	Opt.RequiredSkill = Skill;
	if (!InIconPath.IsEmpty())
	{
		Opt.IconPath = FSoftObjectPath(InIconPath);
	}
	return Opt;
}

FContextMenuOption UAoCContextMenuManager::MakeSubmenu(
	FName InID,
	const FString& InText,
	const TArray<FContextMenuOption>& SubItems,
	const FString& InIconPath)
{
	FContextMenuOption Opt;
	Opt.OptionID = InID;
	Opt.DisplayText = InText;
	Opt.bEnabled = true;
	Opt.SubMenuOptions = SubItems;
	if (!InIconPath.IsEmpty())
	{
		Opt.IconPath = FSoftObjectPath(InIconPath);
	}
	return Opt;
}

// ─────────────────────────────────────────────────────────────────────────────
// Shovel on Ground — Terraforming + Farming sub-options
// ─────────────────────────────────────────────────────────────────────────────

TArray<FContextMenuOption> UAoCContextMenuManager::BuildShovelOnGroundOptions(AActor* Target) const
{
	TArray<FContextMenuOption> Options;

	// ── Core terraforming options (always available with shovel) ─────────
	Options.Add(MakeOption(
		FName(TEXT("LowerGround")),
		TEXT("Lower Ground Level"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_LowerGround")));

	Options.Add(MakeOption(
		FName(TEXT("RaiseGround")),
		TEXT("Raise Ground Level"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_RaiseGround")));

	Options.Add(MakeOption(
		FName(TEXT("FlattenGround")),
		TEXT("Flatten Ground"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_FlattenGround")));

	Options.Add(MakeOption(
		FName(TEXT("Observe")),
		TEXT("Observe"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Observe")));

	Options.Add(MakeOption(
		FName(TEXT("Plow")),
		TEXT("Plow"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Plow")));

	Options.Add(MakeOption(
		FName(TEXT("PourWater")),
		TEXT("Pour Water"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_PourWater")));

	// ── Farming sub-options (appear when soil is Fertile) ───────────────
	// In a real implementation, we'd check the target tile's soil type.
	// Here we always include them; the UI widget greys them out via bEnabled.

	Options.Add(MakeOption(
		FName(TEXT("Fertilize")),
		TEXT("Fertilize"),
		true, true, 90.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Fertilize")));

	Options.Add(MakeOption(
		FName(TEXT("InspectSoil")),
		TEXT("Inspect"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Inspect")));

	// ── Sow sub-menu — crops gated by farming skill ─────────────────────
	TArray<FContextMenuOption> SowSubItems;

	// Skill 30 crops
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Carrot")),    TEXT("Carrot"),    true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Onion")),     TEXT("Onion"),     true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Peas")),      TEXT("Peas"),      true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Wheat")),     TEXT("Wheat"),     true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Barley")),    TEXT("Barley"),    true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Oat")),       TEXT("Oat"),       true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Rye")),       TEXT("Rye"),       true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Flax")),      TEXT("Flax"),      true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Turnip")),    TEXT("Turnip"),    true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Leek")),      TEXT("Leek"),      true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Beetroot")),  TEXT("Beetroot"),  true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Parsnip")),   TEXT("Parsnip"),   true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Hemp")),      TEXT("Hemp"),      true, true, 30.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Mulberry")),  TEXT("Mulberry"),  true, false, 0.0f));

	// Skill 60 crops
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Cabbage")),   TEXT("Cabbage"),   true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Potato")),    TEXT("Potato"),    true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Grapes")),    TEXT("Grapes"),    true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Apple")),     TEXT("Apple"),     true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Garlic")),    TEXT("Garlic"),    true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Pumpkin")),   TEXT("Pumpkin"),   true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Hops")),      TEXT("Hops"),      true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Cotton")),    TEXT("Cotton"),    true, true, 60.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Lavender")),  TEXT("Lavender"),  true, true, 60.0f));

	// Skill 90 crops
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Sunflower")), TEXT("Sunflower"), true, true, 90.0f));
	SowSubItems.Add(MakeOption(FName(TEXT("Sow_Rice")),      TEXT("Rice"),      true, true, 90.0f));

	Options.Add(MakeSubmenu(
		FName(TEXT("Sow")),
		TEXT("Sow"),
		SowSubItems,
		TEXT("/Game/UI/Icons/Actions/IC_Sow")));

	return Options;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pickaxe on Ground — prospecting and tunneling
// ─────────────────────────────────────────────────────────────────────────────

TArray<FContextMenuOption> UAoCContextMenuManager::BuildPickaxeOnGroundOptions(AActor* Target) const
{
	TArray<FContextMenuOption> Options;

	Options.Add(MakeOption(
		FName(TEXT("Prospect")),
		TEXT("Prospect"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Prospect")));

	Options.Add(MakeOption(
		FName(TEXT("TunnelForward")),
		TEXT("Tunnel Forward"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_TunnelForward")));

	Options.Add(MakeOption(
		FName(TEXT("TunnelDown")),
		TEXT("Tunnel Down"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_TunnelDown")));

	Options.Add(MakeOption(
		FName(TEXT("TunnelUp")),
		TEXT("Tunnel Up"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_TunnelUp")));

	return Options;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pickaxe on Rock — mining exposed veins + reinforcement
// ─────────────────────────────────────────────────────────────────────────────

TArray<FContextMenuOption> UAoCContextMenuManager::BuildPickaxeOnRockOptions(AActor* Target) const
{
	TArray<FContextMenuOption> Options;

	Options.Add(MakeOption(
		FName(TEXT("Prospect")),
		TEXT("Prospect"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Prospect")));

	Options.Add(MakeOption(
		FName(TEXT("TunnelForward")),
		TEXT("Tunnel Forward"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_TunnelForward")));

	Options.Add(MakeOption(
		FName(TEXT("TunnelDown")),
		TEXT("Tunnel Down"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_TunnelDown")));

	Options.Add(MakeOption(
		FName(TEXT("TunnelUp")),
		TEXT("Tunnel Up"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_TunnelUp")));

	// Mine ore — in production this label would be dynamic based on the exposed vein
	Options.Add(MakeOption(
		FName(TEXT("MineOre")),
		TEXT("Mine Ore"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_MineOre")));

	// ── Reinforce sub-menu ──────────────────────────────────────────────
	TArray<FContextMenuOption> ReinforceSubItems;

	ReinforceSubItems.Add(MakeOption(
		FName(TEXT("PlaceSupportBeam")),
		TEXT("Place Support Beam"),
		true, true, 30.0f,
		TEXT("/Game/UI/Icons/Actions/IC_SupportBeam")));

	ReinforceSubItems.Add(MakeOption(
		FName(TEXT("PlaceSupportColumn")),
		TEXT("Place Support Column"),
		true, true, 60.0f,
		TEXT("/Game/UI/Icons/Actions/IC_SupportColumn")));

	Options.Add(MakeSubmenu(
		FName(TEXT("Reinforce")),
		TEXT("Reinforce"),
		ReinforceSubItems,
		TEXT("/Game/UI/Icons/Actions/IC_Reinforce")));

	return Options;
}

// ─────────────────────────────────────────────────────────────────────────────
// Furnace interaction
// ─────────────────────────────────────────────────────────────────────────────

TArray<FContextMenuOption> UAoCContextMenuManager::BuildFurnaceOptions(AActor* Target) const
{
	TArray<FContextMenuOption> Options;

	Options.Add(MakeOption(
		FName(TEXT("LoadFuel")),
		TEXT("Load Fuel"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_LoadFuel")));

	Options.Add(MakeOption(
		FName(TEXT("Ignite")),
		TEXT("Ignite"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Ignite")));

	Options.Add(MakeOption(
		FName(TEXT("PumpBellows")),
		TEXT("Pump Bellows"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Bellows")));

	Options.Add(MakeOption(
		FName(TEXT("LoadOre")),
		TEXT("Load Ore"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_LoadOre")));

	// ── Select output form sub-menu ─────────────────────────────────────
	TArray<FContextMenuOption> OutputSubItems;

	OutputSubItems.Add(MakeOption(
		FName(TEXT("OutputLump")),
		TEXT("Lump (1 ore)"),
		true, false, 0.0f));

	OutputSubItems.Add(MakeOption(
		FName(TEXT("OutputBar")),
		TEXT("Bar (4 ore)"),
		true, true, 30.0f));

	OutputSubItems.Add(MakeOption(
		FName(TEXT("OutputIngot")),
		TEXT("Ingot (20 ore)"),
		true, true, 60.0f));

	Options.Add(MakeSubmenu(
		FName(TEXT("SelectOutput")),
		TEXT("Select Output"),
		OutputSubItems,
		TEXT("/Game/UI/Icons/Actions/IC_SelectOutput")));

	Options.Add(MakeOption(
		FName(TEXT("ExtractMetal")),
		TEXT("Extract Metal"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Extract")));

	Options.Add(MakeOption(
		FName(TEXT("Quench")),
		TEXT("Quench in Water"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Quench")));

	Options.Add(MakeOption(
		FName(TEXT("CleanSlag")),
		TEXT("Clean Slag"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_CleanSlag")));

	return Options;
}

// ─────────────────────────────────────────────────────────────────────────────
// Farm tile interaction (direct tile, already planted / ready)
// ─────────────────────────────────────────────────────────────────────────────

TArray<FContextMenuOption> UAoCContextMenuManager::BuildFarmTileOptions(AActor* Target) const
{
	TArray<FContextMenuOption> Options;

	Options.Add(MakeOption(
		FName(TEXT("InspectCrop")),
		TEXT("Inspect Crop"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Inspect")));

	Options.Add(MakeOption(
		FName(TEXT("WaterTile")),
		TEXT("Water"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Water")));

	Options.Add(MakeOption(
		FName(TEXT("HarvestCrop")),
		TEXT("Harvest"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Harvest")));

	Options.Add(MakeOption(
		FName(TEXT("UprootCrop")),
		TEXT("Uproot"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Uproot")));

	Options.Add(MakeOption(
		FName(TEXT("Fertilize")),
		TEXT("Fertilize"),
		true, true, 90.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Fertilize")));

	return Options;
}

// ─────────────────────────────────────────────────────────────────────────────
// Generic interaction (catch-all for non-tool-specific objects)
// ─────────────────────────────────────────────────────────────────────────────

TArray<FContextMenuOption> UAoCContextMenuManager::BuildGenericOptions(AActor* Target) const
{
	TArray<FContextMenuOption> Options;

	Options.Add(MakeOption(
		FName(TEXT("Examine")),
		TEXT("Examine"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Examine")));

	Options.Add(MakeOption(
		FName(TEXT("PickUp")),
		TEXT("Pick Up"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_PickUp")));

	Options.Add(MakeOption(
		FName(TEXT("Use")),
		TEXT("Use"),
		true, false, 0.0f,
		TEXT("/Game/UI/Icons/Actions/IC_Use")));

	return Options;
}

// ─────────────────────────────────────────────────────────────────────────────
// Action dispatchers — stub implementations that log and will be expanded
// when their respective gameplay systems are integrated.
// ─────────────────────────────────────────────────────────────────────────────

void UAoCContextMenuManager::DispatchShovelAction(FName OptionID)
{
	const FString ID = OptionID.ToString();

	if (ID == TEXT("LowerGround"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching LowerGround on target %s"),
			CachedTargetActor ? *CachedTargetActor->GetName() : TEXT("Ground"));
		// TODO: Call terraforming subsystem — lower terrain at tile
	}
	else if (ID == TEXT("RaiseGround"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching RaiseGround"));
		// TODO: Call terraforming subsystem — raise terrain at tile
	}
	else if (ID == TEXT("FlattenGround"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching FlattenGround"));
		// TODO: Call terraforming subsystem — flatten to reference height
	}
	else if (ID == TEXT("Observe"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Observe (height overlay)"));
		// TODO: Enable height-colored overlay on terrain
	}
	else if (ID == TEXT("Plow"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Plow"));
		// TODO: Convert grass → fertile soil via farming tile system
	}
	else if (ID == TEXT("PourWater"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching PourWater"));
		// TODO: Regrow grass on bare soil
	}
	else if (ID == TEXT("Fertilize"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Fertilize"));
		// TODO: Check for Dung in inventory, apply fertilizer bonus
	}
	else if (ID == TEXT("InspectSoil"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching InspectSoil"));
		// TODO: Show growth/quality info popup
	}
	else if (ID.StartsWith(TEXT("Sow_")))
	{
		const FString CropName = ID.RightChop(4); // Strip "Sow_"
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Sow crop: %s"), *CropName);
		// TODO: Plant crop on farming tile via AoCFarmingTile::PlantCrop
	}
}

void UAoCContextMenuManager::DispatchPickaxeAction(FName OptionID)
{
	const FString ID = OptionID.ToString();

	if (ID == TEXT("Prospect"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Prospect"));
		// TODO: Perform sphere search for ore veins, radius based on Mining skill
	}
	else if (ID == TEXT("TunnelForward"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching TunnelForward"));
		// TODO: Carve horizontal tunnel block via voxel system
	}
	else if (ID == TEXT("TunnelDown"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching TunnelDown (-0.5m)"));
		// TODO: Carve downward tunnel block
	}
	else if (ID == TEXT("TunnelUp"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching TunnelUp (+0.5m)"));
		// TODO: Carve upward tunnel block
	}
	else if (ID == TEXT("MineOre"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching MineOre"));
		// TODO: Extract ore from exposed vein in tunnel wall
	}
	else if (ID == TEXT("PlaceSupportBeam"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching PlaceSupportBeam (1 tile)"));
		// TODO: Require 2 boards, place support beam actor
	}
	else if (ID == TEXT("PlaceSupportColumn"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching PlaceSupportColumn (3x3)"));
		// TODO: Require 4 boards + 2 billets, place support column actor
	}
}

void UAoCContextMenuManager::DispatchFurnaceAction(FName OptionID)
{
	const FString ID = OptionID.ToString();

	if (ID == TEXT("LoadFuel"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching LoadFuel"));
		// TODO: Transfer charcoal/billets from player inventory to furnace fuel slot
	}
	else if (ID == TEXT("Ignite"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Ignite"));
		// TODO: Start furnace heating cycle
	}
	else if (ID == TEXT("PumpBellows"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching PumpBellows"));
		// TODO: Raise furnace temperature toward max
	}
	else if (ID == TEXT("LoadOre"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching LoadOre"));
		// TODO: Transfer raw ore from player inventory to furnace input
	}
	else if (ID == TEXT("OutputLump") || ID == TEXT("OutputBar") || ID == TEXT("OutputIngot"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching SelectOutput → %s"), *ID);
		// TODO: Set furnace output form selection
	}
	else if (ID == TEXT("ExtractMetal"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching ExtractMetal"));
		// TODO: Pull hot metal from furnace with tongs
	}
	else if (ID == TEXT("Quench"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Quench"));
		// TODO: Instantly cool hot metal in water barrel
	}
	else if (ID == TEXT("CleanSlag"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching CleanSlag"));
		// TODO: Remove waste byproduct from furnace
	}
}

void UAoCContextMenuManager::DispatchFarmAction(FName OptionID)
{
	const FString ID = OptionID.ToString();

	if (ID == TEXT("InspectCrop"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching InspectCrop"));
		// TODO: Show growth stage, time remaining, quality popup
	}
	else if (ID == TEXT("WaterTile"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching WaterTile"));
		// TODO: Call AoCFarmingTile::WaterTile
	}
	else if (ID == TEXT("HarvestCrop"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching HarvestCrop"));
		// TODO: Call AoCFarmingTile::HarvestCrop, add to inventory
	}
	else if (ID == TEXT("UprootCrop"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching UprootCrop"));
		// TODO: Remove planted crop, recover seed (chance-based)
	}
	else if (ID == TEXT("Fertilize"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Fertilize (farm tile)"));
		// TODO: Apply Dung to tile, increase soil quality
	}
}

void UAoCContextMenuManager::DispatchGenericAction(FName OptionID)
{
	const FString ID = OptionID.ToString();

	if (ID == TEXT("Examine"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Examine"));
		// TODO: Show info panel for examined actor
	}
	else if (ID == TEXT("PickUp"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching PickUp"));
		// TODO: Add ground-dropped item to player inventory
	}
	else if (ID == TEXT("Use"))
	{
		UE_LOG(LogTemp, Log, TEXT("AoCContextMenu: Dispatching Use"));
		// TODO: Activate useable actor interaction
	}
}
