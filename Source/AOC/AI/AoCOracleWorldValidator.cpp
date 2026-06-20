// AoCOracleWorldValidator.cpp
// Architect of Creation — Oracle World Content Validator
// Checks EVERYTHING in the world and reports exactly what's missing.

#include "AoCOracleWorldValidator.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

UAoCOracleWorldValidator::UAoCOracleWorldValidator()
{
    PrimaryComponentTick.bCanEverTick = false;
    ReportDirectory = FPaths::ProjectDir() / TEXT("AoC_GameData") / TEXT("OracleReports");
    InitializeRequirements();
}

void UAoCOracleWorldValidator::InitializeRequirements()
{
    // ===== GATHERING RESOURCES =====
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Iron Ore Node");
        Req.ActorTag = TEXT("Harvestable_Ore");
        Req.Category = TEXT("Mining");
        Req.MinExpected = 3;
        Req.SearchRadius = 100000.f; // 1km
        Req.FixSuggestion = TEXT("Spawn StaticMeshActors with tag 'Harvestable_Ore'. Need rock/ore vein meshes in the world. At least 3 within 1km of spawn.");
        GatheringRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Harvestable Tree");
        Req.ActorTag = TEXT("Harvestable_Wood");
        Req.Category = TEXT("Woodcutting");
        Req.MinExpected = 5;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn tree actors with tag 'Harvestable_Wood'. Need actual tree meshes that the woodcutting system can interact with.");
        GatheringRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Fishing Spot");
        Req.ActorTag = TEXT("FishingSpot");
        Req.Category = TEXT("Fishing");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn trigger volumes near water with tag 'FishingSpot'. Also need fish spawn data - empty water = no fish to catch.");
        GatheringRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Herb Node");
        Req.ActorTag = TEXT("Harvestable_Herb");
        Req.Category = TEXT("Herbalism");
        Req.MinExpected = 3;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn herb/plant actors with tag 'Harvestable_Herb'. Need gatherable plants in the world.");
        GatheringRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Huntable Animal");
        Req.ActorTag = TEXT("Huntable");
        Req.Category = TEXT("Hunting");
        Req.MinExpected = 2;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn animal NPCs with tag 'Huntable'. Deer, rabbits, boars, etc. They need health and a death state.");
        GatheringRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Farm Plot");
        Req.ActorTag = TEXT("FarmPlot");
        Req.Category = TEXT("Farming");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn farmable land actors with tag 'FarmPlot'. Need soil patches where seeds can be planted.");
        GatheringRequirements.Add(Req);
    }

    // ===== CRAFTING STATIONS =====
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Forge / Anvil");
        Req.ActorTag = TEXT("CraftStation_Smithing");
        Req.Category = TEXT("Crafting");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn a forge/anvil actor with tag 'CraftStation_Smithing'. Needed for weapon and armor smithing.");
        CraftingRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Cooking Fire / Kitchen");
        Req.ActorTag = TEXT("CraftStation_Cooking");
        Req.Category = TEXT("Crafting");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn a cooking fire/campfire with tag 'CraftStation_Cooking'. NPCs and players need this to cook food.");
        CraftingRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Alchemy Table");
        Req.ActorTag = TEXT("CraftStation_Alchemy");
        Req.Category = TEXT("Crafting");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn an alchemy table/cauldron with tag 'CraftStation_Alchemy'. Needed for potions and brews.");
        CraftingRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Woodworking Bench");
        Req.ActorTag = TEXT("CraftStation_Woodworking");
        Req.Category = TEXT("Crafting");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn a woodworking bench with tag 'CraftStation_Woodworking'. Needed for bow crafting and staff crafting.");
        CraftingRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Enchanting Altar");
        Req.ActorTag = TEXT("CraftStation_Enchanting");
        Req.Category = TEXT("Crafting");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn an enchanting altar with tag 'CraftStation_Enchanting'. Needed for enchanting weapons and armor.");
        CraftingRequirements.Add(Req);
    }

    // ===== COMBAT =====
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Hostile NPC / Monster");
        Req.ActorTag = TEXT("Hostile");
        Req.Category = TEXT("Combat");
        Req.MinExpected = 3;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn hostile NPCs or creatures with tag 'Hostile'. Need enemies to test combat against.");
        CombatRequirements.Add(Req);
    }

    // ===== INFRASTRUCTURE =====
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Merchant NPC");
        Req.ActorTag = TEXT("NPC_Merchant");
        Req.Category = TEXT("World");
        Req.MinExpected = 1;
        Req.SearchRadius = 100000.f;
        Req.FixSuggestion = TEXT("Spawn a merchant NPC with tag 'NPC_Merchant'. Players need someone to buy starter gear from.");
        InfrastructureRequirements.Add(Req);
    }
    {
        FAoCResourceRequirement Req;
        Req.Name = TEXT("Bindstone / Respawn Point");
        Req.ActorTag = TEXT("Bindstone");
        Req.Category = TEXT("World");
        Req.MinExpected = 1;
        Req.SearchRadius = 200000.f;
        Req.FixSuggestion = TEXT("Spawn a bindstone actor with tag 'Bindstone'. Players need a respawn point after death.");
        InfrastructureRequirements.Add(Req);
    }
}

// ==================== MASTER VALIDATION ====================

FAoCWorldValidationReport UAoCOracleWorldValidator::RunFullWorldValidation()
{
    FAoCWorldValidationReport Report;
    Report.Timestamp = FDateTime::Now();

    // GATHERING (7 checks)
    Report.Results.Add(CheckMiningNodes());
    Report.Results.Add(CheckTrees());
    Report.Results.Add(CheckFishingSpots());
    Report.Results.Add(CheckHerbNodes());
    Report.Results.Add(CheckHuntingTargets());
    Report.Results.Add(CheckFarmingPlots());
    Report.Results.Add(CheckSkinningTargets());

    // TOOLS (6 checks)
    Report.Results.Add(CheckPickaxeExists());
    Report.Results.Add(CheckAxeExists());
    Report.Results.Add(CheckFishingPoleExists());
    Report.Results.Add(CheckWeaponsExist());
    Report.Results.Add(CheckArmorExists());
    Report.Results.Add(CheckStarterGear());

    // CRAFTING (3 checks)
    Report.Results.Add(CheckCraftingStations());
    Report.Results.Add(CheckRecipesLoaded());
    Report.Results.Add(CheckMaterialsObtainable());

    // WORLD (7 checks)
    Report.Results.Add(CheckSpawnPoint());
    Report.Results.Add(CheckNavMesh());
    Report.Results.Add(CheckGroundExists());
    Report.Results.Add(CheckLighting());
    Report.Results.Add(CheckSkyAndWeather());
    Report.Results.Add(CheckWaterBodies());
    Report.Results.Add(CheckBuildableAreas());

    // NPCs (4 checks)
    Report.Results.Add(CheckFriendlyNPCs());
    Report.Results.Add(CheckHostileCreatures());
    Report.Results.Add(CheckNPCPathfinding());
    Report.Results.Add(CheckNPCAnimations());

    // COMBAT (5 checks)
    Report.Results.Add(CheckMeleeHitDetection());
    Report.Results.Add(CheckSpellCasting());
    Report.Results.Add(CheckDamageNumbers());
    Report.Results.Add(CheckDeathAndRespawn());
    Report.Results.Add(CheckLootDrops());

    // EQUIPMENT (3 checks)
    Report.Results.Add(CheckEquipToHand());
    Report.Results.Add(CheckArmorVisuals());
    Report.Results.Add(CheckStatChanges());

    // UI (3 checks)
    Report.Results.Add(CheckHUDVisible());
    Report.Results.Add(CheckInventoryOpens());
    Report.Results.Add(CheckChatWorks());

    // SKILLS (3 checks)
    Report.Results.Add(CheckSkillGainOnUse());
    Report.Results.Add(CheckAttributeGrowth());
    Report.Results.Add(CheckMasteryUnlocks());

    // Tally
    Report.TotalTests = Report.Results.Num();
    for (const auto& R : Report.Results)
    {
        if (R.bPassed) Report.Passed++;
        else if (R.Severity >= 2) Report.Critical++;
        else if (R.Severity >= 1) Report.Warnings++;
    }

    // Save to disk
    SaveReportToDisk(Report);

    return Report;
}

FAoCWorldValidationReport UAoCOracleWorldValidator::RunCategoryValidation(const FString& Category)
{
    FAoCWorldValidationReport Report;
    Report.Timestamp = FDateTime::Now();

    FAoCWorldValidationReport Full = RunFullWorldValidation();
    for (const auto& R : Full.Results)
    {
        if (R.Category == Category)
        {
            Report.Results.Add(R);
        }
    }

    Report.TotalTests = Report.Results.Num();
    for (const auto& R : Report.Results)
    {
        if (R.bPassed) Report.Passed++;
        else if (R.Severity >= 2) Report.Critical++;
        else if (R.Severity >= 1) Report.Warnings++;
    }

    return Report;
}

// ==================== GATHERING CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckMiningNodes()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Mining Nodes");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Nodes = FindActorsWithTag(FName("Harvestable_Ore"), 100000.f);
    int32 Count = Nodes.Num();

    if (Count >= 3)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d mining rocks within 1km. Closest is %.0fm away."),
            Count, Nodes.IsValidIndex(0) ? FVector::Dist(GetOwner()->GetActorLocation(), Nodes[0]->GetActorLocation()) / 100.f : 0.f);
        R.Severity = 0;
    }
    else if (Count > 0)
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Only found %d mining rocks within 1km. Need at least 3 for a proper mining area."), Count);
        R.FixSuggestion = TEXT("Spawn more StaticMeshActors with tag 'Harvestable_Ore' — ore veins, rock outcrops, boulders with minerals.");
        R.Severity = 1;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("ZERO mining rocks found anywhere within 1km! I tried to mine but there is literally nothing to mine.");
        R.FixSuggestion = TEXT("Spawn StaticMeshActors with tag 'Harvestable_Ore'. Need rock meshes (ore veins, boulders, mineral deposits) in the world. Place at least 3-5 near the spawn area.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckTrees()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Harvestable Trees");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Trees = FindActorsWithTag(FName("Harvestable_Wood"), 100000.f);
    int32 Count = Trees.Num();

    if (Count >= 5)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d harvestable trees within 1km. Woodcutting is possible!"), Count);
        R.Severity = 0;
    }
    else if (Count > 0)
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Only %d harvestable trees found. A forest should have at least 5+."), Count);
        R.FixSuggestion = TEXT("Add more tree actors with tag 'Harvestable_Wood'. Need enough for sustained woodcutting.");
        R.Severity = 1;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("ZERO harvestable trees found! I tried to chop wood but there are no trees to chop anywhere within 1km.");
        R.FixSuggestion = TEXT("Spawn tree actors (any tree mesh) with tag 'Harvestable_Wood'. Trees are decoration right now — they need the tag to be choppable by the gathering system.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckFishingSpots()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Fishing Spots");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Spots = FindActorsWithTag(FName("FishingSpot"), 100000.f);
    TArray<AActor*> Water = FindActorsWithTag(FName("WaterBody"), 100000.f);

    if (Spots.Num() >= 1)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d fishing spots. Fish on!"), Spots.Num());
    }
    else if (Water.Num() > 0)
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Found %d water bodies but ZERO fishing spots! There's water but no fish. I walked to the water and cast my line... nothing."), Water.Num());
        R.FixSuggestion = TEXT("Add trigger volumes with tag 'FishingSpot' near water edges. The water exists but has no fish spawn points. NPCs and players need specific spots to cast a line.");
        R.Severity = 2;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("ZERO water bodies AND zero fishing spots found! There is no water anywhere to fish in. I literally cannot fish.");
        R.FixSuggestion = TEXT("Need water bodies (lakes, rivers, ocean) with tag 'WaterBody', plus fishing trigger volumes with tag 'FishingSpot' near the shore.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckHerbNodes()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Herb Nodes");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Herbs = FindActorsWithTag(FName("Harvestable_Herb"), 100000.f);
    if (Herbs.Num() >= 3)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d herb nodes. Herbalism works!"), Herbs.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Found %d herb nodes within 1km. Need at least 3 for herbalism to function. I tried gathering herbs — nothing to pick."), Herbs.Num());
        R.FixSuggestion = TEXT("Spawn plant/flower actors with tag 'Harvestable_Herb'. Mushrooms, herbs, flowers — all need this tag.");
        R.Severity = Herbs.Num() == 0 ? 2 : 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckHuntingTargets()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Huntable Animals");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Animals = FindActorsWithTag(FName("Huntable"), 100000.f);
    if (Animals.Num() >= 2)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d huntable animals. Hunting works!"), Animals.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Found %d huntable animals. I tried to go hunting but there's nothing alive to hunt within 1km."), Animals.Num());
        R.FixSuggestion = TEXT("Spawn animal NPC characters (deer, boar, rabbit, wolf) with tag 'Huntable'. They need health components and a death state.");
        R.Severity = Animals.Num() == 0 ? 2 : 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckFarmingPlots()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Farm Plots");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Plots = FindActorsWithTag(FName("FarmPlot"), 100000.f);
    if (Plots.Num() >= 1)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d farm plots. Farming is possible!"), Plots.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No farm plots found. I can't plant anything. Need farmable land somewhere in the world.");
        R.FixSuggestion = TEXT("Spawn farm plot actors with tag 'FarmPlot'. Flat soil patches where seeds can be planted.");
        R.Severity = 1; // Lower priority than mining/combat
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckSkinningTargets()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Skinnable Corpses");
    R.Category = TEXT("WorldContent");

    // Skinning depends on hunting — if there are huntable animals, skinning works after they die
    TArray<AActor*> Animals = FindActorsWithTag(FName("Huntable"), 100000.f);
    TArray<AActor*> Skinnable = FindActorsWithTag(FName("Skinnable"), 100000.f);

    if (Skinnable.Num() > 0 || Animals.Num() >= 2)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Skinning should work — %d huntable animals exist. Kill them to skin them."), Animals.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No huntable animals found, so nothing to skin. Skinning depends on hunting — fix hunting first.");
        R.FixSuggestion = TEXT("Skinning requires huntable animals to exist first. Add animal NPCs with 'Huntable' tag. Dead animals should get 'Skinnable' tag.");
        R.Severity = 1;
    }

    return R;
}

// ==================== TOOL CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckPickaxeExists()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Pickaxe Available");
    R.Category = TEXT("Inventory");

    // Check if there's a pickaxe actor in the world (in a chest, on ground, at merchant)
    TArray<AActor*> Picks = FindActorsWithTag(FName("Item_Pickaxe"), 200000.f);
    TArray<AActor*> Merchants = FindActorsWithTag(FName("NPC_Merchant"), 200000.f);

    if (Picks.Num() > 0)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d pickaxes in the world. Mining tool exists!"), Picks.Num());
    }
    else if (Merchants.Num() > 0)
    {
        // Merchant exists — could sell pickaxes
        R.bPassed = false;
        R.Details = TEXT("No pickaxe found on the ground, but a merchant exists. Does the merchant sell pickaxes? I couldn't find one in their inventory.");
        R.FixSuggestion = TEXT("Either spawn a pickaxe actor with tag 'Item_Pickaxe' on the ground, or add pickaxe to merchant's sale inventory.");
        R.Severity = 1;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("ZERO pickaxes anywhere in the world AND no merchant to buy one from! I found ore to mine but I have nothing to mine it with.");
        R.FixSuggestion = TEXT("Spawn a pickaxe item actor with tag 'Item_Pickaxe' near spawn, or spawn a merchant NPC that sells starter tools.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckAxeExists()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Woodcutting Axe Available");
    R.Category = TEXT("Inventory");

    TArray<AActor*> Axes = FindActorsWithTag(FName("Item_WoodAxe"), 200000.f);
    if (Axes.Num() > 0)
    {
        R.bPassed = true;
        R.Details = TEXT("Found a woodcutting axe. Chopping is possible!");
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No woodcutting axe found anywhere! I see trees but have nothing to cut them with.");
        R.FixSuggestion = TEXT("Spawn a woodcutting axe with tag 'Item_WoodAxe' near spawn, or add to merchant inventory.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckFishingPoleExists()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Fishing Pole Available");
    R.Category = TEXT("Inventory");

    TArray<AActor*> Poles = FindActorsWithTag(FName("Item_FishingPole"), 200000.f);
    if (Poles.Num() > 0)
    {
        R.bPassed = true;
        R.Details = TEXT("Found a fishing pole. Can go fishing!");
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No fishing pole found! I went to the water but I have nothing to fish with. My hands are empty.");
        R.FixSuggestion = TEXT("Spawn a fishing pole with tag 'Item_FishingPole' near water or at a merchant. Players can't fish without a pole.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckWeaponsExist()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Weapons Available");
    R.Category = TEXT("Inventory");

    TArray<AActor*> Weapons = FindActorsWithTag(FName("Item_Weapon"), 200000.f);
    if (Weapons.Num() >= 1)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d weapons in the world."), Weapons.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("ZERO weapons found in the entire world! How do I fight? I can punch (Unarmed) but there should be at least a starter sword near spawn.");
        R.FixSuggestion = TEXT("Spawn weapon actors with tag 'Item_Weapon' (swords, axes, daggers, etc.). At least put a basic sword near spawn.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckArmorExists()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Armor Available");
    R.Category = TEXT("Inventory");

    TArray<AActor*> Armor = FindActorsWithTag(FName("Item_Armor"), 200000.f);
    if (Armor.Num() >= 1)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d armor pieces in the world."), Armor.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No armor found anywhere. I'm completely naked with no way to protect myself.");
        R.FixSuggestion = TEXT("Spawn armor actors with tag 'Item_Armor'. At least basic cloth armor near spawn.");
        R.Severity = 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckStarterGear()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Starter Gear Bundle");
    R.Category = TEXT("Inventory");

    // A new player needs AT MINIMUM: a weapon, a pickaxe, maybe some food
    TArray<AActor*> Weapons = FindActorsWithTag(FName("Item_Weapon"), 50000.f);
    TArray<AActor*> Tools = FindActorsWithTag(FName("Item_Pickaxe"), 50000.f);
    TArray<AActor*> Food = FindActorsWithTag(FName("Item_Food"), 50000.f);

    int32 StarterItems = 0;
    FString Missing;

    if (Weapons.Num() > 0) StarterItems++; else Missing += TEXT("weapon, ");
    if (Tools.Num() > 0) StarterItems++; else Missing += TEXT("pickaxe, ");
    if (Food.Num() > 0) StarterItems++; else Missing += TEXT("food, ");

    if (StarterItems >= 2)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Starter gear check: %d/3 essential items found near spawn."), StarterItems);
    }
    else
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("New player has almost nothing! Only %d/3 starter essentials. Missing: %s. A new player spawns with empty hands and nothing to do."),
            StarterItems, *Missing.LeftChop(2));
        R.FixSuggestion = TEXT("Put a starter gear chest or items near PlayerStart: basic sword (Item_Weapon), pickaxe (Item_Pickaxe), bread (Item_Food). New players need SOMETHING to start with.");
        R.Severity = 2;
    }

    return R;
}

// ==================== CRAFTING CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckCraftingStations()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Crafting Stations");
    R.Category = TEXT("WorldContent");

    int32 StationsFound = 0;
    FString Missing;

    for (const auto& Req : CraftingRequirements)
    {
        TArray<AActor*> Found = FindActorsWithTag(FName(*Req.ActorTag), Req.SearchRadius);
        if (Found.Num() >= Req.MinExpected)
        {
            StationsFound++;
        }
        else
        {
            Missing += FString::Printf(TEXT("%s (%s), "), *Req.Name, *Req.ActorTag);
        }
    }

    if (StationsFound == CraftingRequirements.Num())
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("All %d crafting station types found!"), StationsFound);
    }
    else
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Only %d/%d crafting stations found. Missing: %s. I have materials but nowhere to craft them."),
            StationsFound, CraftingRequirements.Num(), *Missing.LeftChop(2));
        R.FixSuggestion = TEXT("Spawn crafting station actors with appropriate tags. Forge (CraftStation_Smithing), Cooking Fire (CraftStation_Cooking), Alchemy Table (CraftStation_Alchemy), Workbench (CraftStation_Woodworking), Enchanting Altar (CraftStation_Enchanting).");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckRecipesLoaded()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Crafting Recipes");
    R.Category = TEXT("System");

    // Check if recipe DataTable or data exists
    FString RecipePath = FPaths::ProjectDir() / TEXT("AoC_GameData") / TEXT("aoc_crafting_recipes.csv");
    bool bRecipeFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*RecipePath);

    if (bRecipeFileExists)
    {
        FString Content;
        FFileHelper::LoadFileToString(Content, *RecipePath);
        int32 LineCount = 0;
        for (const TCHAR& C : Content) { if (C == '\n') LineCount++; }

        if (LineCount > 5)
        {
            R.bPassed = true;
            R.Details = FString::Printf(TEXT("Crafting recipe file found with ~%d recipes."), LineCount - 1);
        }
        else
        {
            R.bPassed = false;
            R.Details = TEXT("Recipe file exists but has very few entries. Crafting will feel empty.");
            R.FixSuggestion = TEXT("Populate aoc_crafting_recipes.csv with full recipe data from the spec.");
            R.Severity = 1;
        }
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("Crafting recipe data file not found! I tried to open the crafting menu — no recipes loaded. There's nothing to craft.");
        R.FixSuggestion = TEXT("Create AoC_GameData/aoc_crafting_recipes.csv with all recipe data. Or load recipes into a UDataTable.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckMaterialsObtainable()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Materials Obtainable");
    R.Category = TEXT("System");

    // Check: are gathering nodes AND crafting stations both present?
    TArray<AActor*> OreNodes = FindActorsWithTag(FName("Harvestable_Ore"), 100000.f);
    TArray<AActor*> Forge = FindActorsWithTag(FName("CraftStation_Smithing"), 100000.f);

    if (OreNodes.Num() > 0 && Forge.Num() > 0)
    {
        R.bPassed = true;
        R.Details = TEXT("Full material pipeline exists: ore nodes → smelt at forge. Raw-to-product chain works.");
    }
    else if (OreNodes.Num() > 0)
    {
        R.bPassed = false;
        R.Details = TEXT("I mined ore but there's no forge to smelt it at! I have raw materials with no way to process them.");
        R.FixSuggestion = TEXT("Spawn a forge/anvil with tag 'CraftStation_Smithing'.");
        R.Severity = 2;
    }
    else if (Forge.Num() > 0)
    {
        R.bPassed = false;
        R.Details = TEXT("There's a forge but no ore nodes to mine! The forge is useless without raw materials.");
        R.FixSuggestion = TEXT("Spawn ore node actors with tag 'Harvestable_Ore'.");
        R.Severity = 2;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No ore AND no forge. The entire smithing pipeline is missing.");
        R.FixSuggestion = TEXT("Need both: ore nodes ('Harvestable_Ore') AND a forge ('CraftStation_Smithing').");
        R.Severity = 2;
    }

    return R;
}

// ==================== WORLD INFRASTRUCTURE ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckSpawnPoint()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Player Spawn Point");
    R.Category = TEXT("World");

    TArray<AActor*> Spawns;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Spawns);

    if (Spawns.Num() >= 1)
    {
        FVector Loc = Spawns[0]->GetActorLocation();
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("PlayerStart found at (%.0f, %.0f, %.0f)."), Loc.X, Loc.Y, Loc.Z);
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("NO PLAYER START! Players will spawn at world origin (0,0,0) which might be underground or in the void.");
        R.FixSuggestion = TEXT("Drag a PlayerStart actor into the level at a safe spawn location.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckNavMesh()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Navigation Mesh");
    R.Category = TEXT("World");

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    if (NavSys)
    {
        // Try a simple path query to test NavMesh
        FVector MyLoc = GetOwner()->GetActorLocation();
        FVector TestLoc = MyLoc + FVector(500.f, 0.f, 0.f);
        FNavLocation Result;
        bool bOnNavMesh = NavSys->ProjectPointToNavigation(MyLoc, Result);

        if (bOnNavMesh)
        {
            R.bPassed = true;
            R.Details = TEXT("NavMesh exists and I'm standing on it. I can pathfind!");
        }
        else
        {
            R.bPassed = false;
            R.Details = TEXT("NavMesh exists but I'm NOT on it! I can't pathfind to anything. My current position isn't on the navigation mesh.");
            R.FixSuggestion = TEXT("The NavMesh bounds volume might not cover this area. Extend the NavMesh Bounds Volume or rebuild navigation (Build → Build Paths).");
            R.Severity = 2;
        }
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("NO NAVIGATION SYSTEM! NPCs cannot pathfind at all. We're all stuck in place.");
        R.FixSuggestion = TEXT("Add a NavMeshBoundsVolume to the level covering the playable area, then Build Paths.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckGroundExists()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Ground / Terrain");
    R.Category = TEXT("World");

    // Trace downward to check if there's ground beneath us
    FVector Start = GetOwner()->GetActorLocation();
    FVector End = Start - FVector(0, 0, 100000.f); // 1km down

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    bool bHitGround = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);

    if (bHitGround)
    {
        float DistToGround = (Start - Hit.ImpactPoint).Size() / 100.f;
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Ground found %.1fm below me. Terrain exists."), DistToGround);
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("NO GROUND DETECTED! I'm floating in the void. There's no terrain or landscape below me.");
        R.FixSuggestion = TEXT("Need a Landscape or static mesh floor. The world has no terrain to walk on.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckLighting()
{
    FAoCValidationResult R;
    R.TestName = TEXT("World Lighting");
    R.Category = TEXT("World");

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

    bool bHasDirectionalLight = false;
    for (AActor* Actor : AllActors)
    {
        if (Actor->FindComponentByClass<UDirectionalLightComponent>())
        {
            bHasDirectionalLight = true;
            break;
        }
    }

    if (bHasDirectionalLight)
    {
        R.bPassed = true;
        R.Details = TEXT("Directional light found. The world is lit.");
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No directional light! The world might be completely dark. I can't see anything.");
        R.FixSuggestion = TEXT("Add a Directional Light actor to the level for sun lighting.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckSkyAndWeather()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Sky & Atmosphere");
    R.Category = TEXT("World");

    // Check for sky-related actors
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

    bool bHasSky = false;
    for (AActor* Actor : AllActors)
    {
        FString Name = Actor->GetName();
        if (Name.Contains(TEXT("Sky")) || Name.Contains(TEXT("Atmosphere")) || Name.Contains(TEXT("Fog")) || Name.Contains(TEXT("Cloud")))
        {
            bHasSky = true;
            break;
        }
    }

    if (bHasSky)
    {
        R.bPassed = true;
        R.Details = TEXT("Sky/atmosphere actors found.");
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No sky or atmosphere found. The sky is black/void.");
        R.FixSuggestion = TEXT("Add SkyAtmosphere, ExponentialHeightFog, and VolumetricCloud actors.");
        R.Severity = 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckWaterBodies()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Water Bodies");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Water = FindActorsWithTag(FName("WaterBody"), 200000.f);

    if (Water.Num() > 0)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d water bodies. Lakes/rivers exist."), Water.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No water bodies found! No lakes, rivers, or ocean. Can't fish, can't swim.");
        R.FixSuggestion = TEXT("Add water body actors (UE5 Water plugin) or static mesh water planes with tag 'WaterBody'.");
        R.Severity = 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckBuildableAreas()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Buildable Areas");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> BuildZones = FindActorsWithTag(FName("BuildZone"), 200000.f);

    if (BuildZones.Num() > 0)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d build zones. Construction is possible!"), BuildZones.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No designated build zones. Players can't place structures anywhere. Masonry/Carpentry/Architecture skills have no purpose.");
        R.FixSuggestion = TEXT("Add build zone trigger volumes with tag 'BuildZone' in flat open areas where players should be able to build.");
        R.Severity = 1;
    }

    return R;
}

// ==================== NPC CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckFriendlyNPCs()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Friendly NPCs");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Merchants = FindActorsWithTag(FName("NPC_Merchant"), 200000.f);
    TArray<AActor*> QuestGivers = FindActorsWithTag(FName("NPC_QuestGiver"), 200000.f);
    TArray<AActor*> Guards = FindActorsWithTag(FName("NPC_Guard"), 200000.f);

    int32 Total = Merchants.Num() + QuestGivers.Num() + Guards.Num();

    if (Total >= 3)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d friendly NPCs (%d merchants, %d quest givers, %d guards)."),
            Total, Merchants.Num(), QuestGivers.Num(), Guards.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Only %d friendly NPCs found. The world feels empty — no merchants to buy from, no guards, no quest givers."), Total);
        R.FixSuggestion = TEXT("Spawn NPC characters with tags: 'NPC_Merchant' (sells items), 'NPC_QuestGiver' (gives quests), 'NPC_Guard' (patrols town).");
        R.Severity = 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckHostileCreatures()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Hostile Creatures");
    R.Category = TEXT("WorldContent");

    TArray<AActor*> Hostiles = FindActorsWithTag(FName("Hostile"), 100000.f);

    if (Hostiles.Num() >= 3)
    {
        R.bPassed = true;
        R.Details = FString::Printf(TEXT("Found %d hostile creatures. Combat is possible!"), Hostiles.Num());
    }
    else
    {
        R.bPassed = false;
        R.Details = FString::Printf(TEXT("Only %d hostile creatures found. The world is too safe — nothing to fight outside of other NPCs/players."), Hostiles.Num());
        R.FixSuggestion = TEXT("Spawn hostile creature NPCs with tag 'Hostile'. Goblins, wolves, skeletons — whatever fits the area. Need at least 3-5 for basic combat testing.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckNPCPathfinding()
{
    FAoCValidationResult R;
    R.TestName = TEXT("NPC Pathfinding");
    R.Category = TEXT("System");

    // The Oracle tests its OWN pathfinding
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys)
    {
        R.bPassed = false;
        R.Details = TEXT("No navigation system — NPCs are completely stuck. Cannot pathfind anywhere.");
        R.FixSuggestion = TEXT("Add NavMeshBoundsVolume and build paths.");
        R.Severity = 2;
        return R;
    }

    // Try to find a path to a random point 20m away
    FVector Start = GetOwner()->GetActorLocation();
    FVector End = Start + FVector(FMath::RandRange(-2000.f, 2000.f), FMath::RandRange(-2000.f, 2000.f), 0);

    FPathFindingQuery Query(GetOwner(), *NavSys->GetDefaultNavDataInstance(), Start, End);
    FPathFindingResult PathResult = NavSys->FindPathSync(Query);

    if (PathResult.IsSuccessful())
    {
        R.bPassed = true;
        R.Details = TEXT("Pathfinding works! I can navigate to nearby locations.");
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("Pathfinding FAILED! I tried to walk 20m in a random direction and the navigation system couldn't find a path. NPCs are stuck.");
        R.FixSuggestion = TEXT("NavMesh may be too small or terrain has gaps. Rebuild navigation: Build → Build Paths. Make sure NavMeshBoundsVolume covers the playable area.");
        R.Severity = 2;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckNPCAnimations()
{
    FAoCValidationResult R;
    R.TestName = TEXT("NPC Animations");
    R.Category = TEXT("System");

    // Check if our own mesh has an anim instance
    AActor* Owner = GetOwner();
    USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;

    if (Mesh && Mesh->GetAnimInstance())
    {
        R.bPassed = true;
        R.Details = TEXT("Skeletal mesh has an animation instance. Animations should play.");
    }
    else if (Mesh)
    {
        R.bPassed = false;
        R.Details = TEXT("I have a skeletal mesh but NO animation blueprint assigned! I'm probably T-posing. My arms are stuck out like a scarecrow.");
        R.FixSuggestion = TEXT("Assign ABP_AoC_Character as the Anim Class on the skeletal mesh component. Build the Animation Blueprint state machine.");
        R.Severity = 2;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("I don't even have a skeletal mesh! I'm invisible or a floating collision box.");
        R.FixSuggestion = TEXT("Assign SK_PeasantMan (or final character mesh) to the NPC's SkeletalMeshComponent.");
        R.Severity = 2;
    }

    return R;
}

// ==================== COMBAT CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckMeleeHitDetection()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Melee Hit Detection");
    R.Category = TEXT("System");
    // This is tested dynamically during combat — set as pending
    R.bPassed = false;
    R.Details = TEXT("Melee hit detection not yet tested. Need to actually swing a weapon at a target. Will test during combat phase.");
    R.FixSuggestion = TEXT("Implement trace-based hit detection on weapon swing animations. Notify events on attack montages.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckSpellCasting()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Spell Casting");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Spell system not yet tested. Need to equip a staff and attempt to cast a spell. Will test during magic phase.");
    R.FixSuggestion = TEXT("Implement spell casting: read from spell DataTable, play cast animation, spawn projectile/effect, apply damage.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckDamageNumbers()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Damage Numbers");
    R.Category = TEXT("UI");
    R.bPassed = false;
    R.Details = TEXT("Floating damage numbers not yet tested. Need to hit something and check if numbers appear above the target.");
    R.FixSuggestion = TEXT("Implement floating damage text widget — spawn on hit, float upward, fade out. Red for damage, green for heals.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckDeathAndRespawn()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Death & Respawn");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Death system not tested yet. Need to take lethal damage and check if death screen appears + respawn works.");
    R.FixSuggestion = TEXT("Implement: HP reaches 0 → ragdoll → death screen widget → respawn timer → teleport to bindstone.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckLootDrops()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Loot Drops");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Loot system not tested. Need to kill something and check if a lootable corpse/grave appears with items.");
    R.FixSuggestion = TEXT("Implement: on death → spawn grave actor with inventory contents → interactable loot window.");
    R.Severity = 1;
    return R;
}

// ==================== EQUIPMENT CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckEquipToHand()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Equip to Hand");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Equipment visuals not tested. Need to pick up a sword and check if it appears in my hand on the character mesh.");
    R.FixSuggestion = TEXT("Implement: equip item → attach weapon mesh to hand socket on skeleton → update anim state.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckArmorVisuals()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Armor Visuals");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Armor display not tested. Need to equip armor and check if character appearance changes.");
    R.FixSuggestion = TEXT("Implement: equip armor → swap/overlay mesh components or material parameters on character mesh.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckStatChanges()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Equipment Stat Changes");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Stat modification not tested. Need to equip a weapon and check if my damage/defense values change.");
    R.FixSuggestion = TEXT("Implement: equip item → read item stats → add to character stats → update HUD display.");
    R.Severity = 1;
    return R;
}

// ==================== UI CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckHUDVisible()
{
    FAoCValidationResult R;
    R.TestName = TEXT("HUD Visible");
    R.Category = TEXT("UI");

    // Check if any HUD widget is in the viewport
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        // We can't easily check widgets from NPC context — flag as needs-player-test
        R.bPassed = false;
        R.Details = TEXT("HUD visibility check requires player context. Need to verify HP/MP/Stamina bars are showing on screen.");
        R.FixSuggestion = TEXT("Create AoCHUDWidget and add to viewport from PlayerController BeginPlay or GameMode.");
        R.Severity = 1;
    }
    else
    {
        R.bPassed = false;
        R.Details = TEXT("No PlayerController found — can't check HUD.");
        R.Severity = 1;
    }

    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckInventoryOpens()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Inventory Panel");
    R.Category = TEXT("UI");
    R.bPassed = false;
    R.Details = TEXT("Inventory panel test requires player input (I key). Not testable from NPC context. Manual check needed.");
    R.FixSuggestion = TEXT("Wire I key → toggle inventory widget. Verify 56 bag slots + 13 equipment slots + paper doll.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckChatWorks()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Chat System");
    R.Category = TEXT("UI");

    // The Oracle CAN test this — it sends chat messages via the speech system
    R.bPassed = false;
    R.Details = TEXT("Chat system test: I'll try to send a message. If you see chat bubbles above my head, the display works. If messages appear in the chat log, the full system works.");
    R.FixSuggestion = TEXT("Implement chat widget with channels: Local, Global, Clan, Party, Trade, System, Combat, Whisper.");
    R.Severity = 1;
    return R;
}

// ==================== SKILL CHECKS ====================

FAoCValidationResult UAoCOracleWorldValidator::CheckSkillGainOnUse()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Skill Gain on Use");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Skill leveling not tested yet. Need to mine something and check if Mining skill increased. Will test during mining phase.");
    R.FixSuggestion = TEXT("Implement: perform action → check skill ID → calculate XP gain → add to skill → check level up → toast notification.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckAttributeGrowth()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Attribute Growth");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Attribute leveling not tested. Need to use skills that exercise attributes (STR from mining, DEX from archery, etc.).");
    R.FixSuggestion = TEXT("Implement: skill use → check associated attribute → add fractional XP → attribute levels over time.");
    R.Severity = 1;
    return R;
}

FAoCValidationResult UAoCOracleWorldValidator::CheckMasteryUnlocks()
{
    FAoCValidationResult R;
    R.TestName = TEXT("Mastery Tier Unlocks");
    R.Category = TEXT("System");
    R.bPassed = false;
    R.Details = TEXT("Mastery system not tested. Need to reach skill level 100 and check if mastery tier becomes available.");
    R.FixSuggestion = TEXT("Implement: weapon skill reaches 100 → unlock mastery tier → new abilities/bonuses → separate mastery XP track.");
    R.Severity = 0; // Low priority — endgame feature
    return R;
}

// ==================== CONTEXTUAL VALIDATION ====================

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeMining()
{
    TArray<FAoCValidationResult> Results;

    // 1. Are there rocks to mine?
    Results.Add(CheckMiningNodes());

    // 2. Do I have a pickaxe?
    Results.Add(CheckPickaxeExists());

    // 3. Can I pathfind to the nearest rock?
    FAoCValidationResult PathCheck;
    PathCheck.TestName = TEXT("Path to Mining Node");
    PathCheck.Category = TEXT("System");

    TArray<AActor*> Nodes = FindActorsWithTag(FName("Harvestable_Ore"), 100000.f);
    if (Nodes.Num() > 0)
    {
        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        if (NavSys)
        {
            FPathFindingQuery Query(GetOwner(), *NavSys->GetDefaultNavDataInstance(),
                GetOwner()->GetActorLocation(), Nodes[0]->GetActorLocation());
            FPathFindingResult PathResult = NavSys->FindPathSync(Query);

            if (PathResult.IsSuccessful())
            {
                float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Nodes[0]->GetActorLocation()) / 100.f;
                PathCheck.bPassed = true;
                PathCheck.Details = FString::Printf(TEXT("Can reach nearest mining node (%.0fm away). Path is clear!"), Dist);
            }
            else
            {
                PathCheck.bPassed = false;
                PathCheck.Details = TEXT("Found a mining rock but CAN'T REACH IT! Pathfinding fails. Rock might be on unreachable terrain or NavMesh doesn't extend there.");
                PathCheck.FixSuggestion = TEXT("Extend NavMesh to cover the mining area, or move mining nodes to walkable terrain.");
                PathCheck.Severity = 2;
            }
        }
    }
    else
    {
        PathCheck.bPassed = false;
        PathCheck.Details = TEXT("No mining nodes to pathfind to — see mining node check above.");
        PathCheck.Severity = 2;
    }
    Results.Add(PathCheck);

    // 4. Is the mining animation available?
    FAoCValidationResult AnimCheck;
    AnimCheck.TestName = TEXT("Mining Animation");
    AnimCheck.Category = TEXT("Animation");
    // Check if mining anim exists
    AnimCheck.bPassed = false;
    AnimCheck.Details = TEXT("Mining animation check pending — need to verify a pickaxe swing animation exists and plays correctly.");
    AnimCheck.FixSuggestion = TEXT("Need a mining/pickaxe swing animation montage. Can use a generic overhead swing from Mixamo library.");
    AnimCheck.Severity = 1;
    Results.Add(AnimCheck);

    return Results;
}

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeFishing()
{
    TArray<FAoCValidationResult> Results;

    Results.Add(CheckFishingSpots());
    Results.Add(CheckFishingPoleExists());
    Results.Add(CheckWaterBodies());

    // Is the fishing pole in my hand?
    FAoCValidationResult PoleInHand;
    PoleInHand.TestName = TEXT("Fishing Pole Equipped");
    PoleInHand.Category = TEXT("Inventory");
    PoleInHand.bPassed = false;
    PoleInHand.Details = TEXT("Fishing pole equip check pending — need to verify pole appears in hand and cast animation plays.");
    PoleInHand.FixSuggestion = TEXT("Implement: equip fishing pole → attach to hand socket → play cast animation → start fishing minigame.");
    PoleInHand.Severity = 1;
    Results.Add(PoleInHand);

    // Are there actual fish to catch?
    FAoCValidationResult FishData;
    FishData.TestName = TEXT("Fish Species Data");
    FishData.Category = TEXT("System");

    FString FishPath = FPaths::ProjectDir() / TEXT("AoC_GameData") / TEXT("aoc_fish_species.csv");
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*FishPath))
    {
        FishData.bPassed = true;
        FishData.Details = TEXT("Fish species data file found.");
    }
    else
    {
        FishData.bPassed = false;
        FishData.Details = TEXT("No fish species data! Even if I cast my line, there are no fish defined to catch.");
        FishData.FixSuggestion = TEXT("Create AoC_GameData/aoc_fish_species.csv with fish types, sizes, rarity, locations.");
        FishData.Severity = 1;
    }
    Results.Add(FishData);

    return Results;
}

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeWoodcutting()
{
    TArray<FAoCValidationResult> Results;

    Results.Add(CheckTrees());
    Results.Add(CheckAxeExists());

    return Results;
}

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeCrafting(const FString& RecipeName)
{
    TArray<FAoCValidationResult> Results;

    Results.Add(CheckCraftingStations());
    Results.Add(CheckRecipesLoaded());

    // Check if specific recipe is in data
    FAoCValidationResult SpecificRecipe;
    SpecificRecipe.TestName = FString::Printf(TEXT("Recipe: %s"), *RecipeName);
    SpecificRecipe.Category = TEXT("System");
    SpecificRecipe.bPassed = false;
    SpecificRecipe.Details = FString::Printf(TEXT("Looking for recipe '%s' — need to check crafting DataTable for this specific item."), *RecipeName);
    SpecificRecipe.Severity = 1;
    Results.Add(SpecificRecipe);

    return Results;
}

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeCombat()
{
    TArray<FAoCValidationResult> Results;

    Results.Add(CheckHostileCreatures());
    Results.Add(CheckWeaponsExist());
    Results.Add(CheckNPCPathfinding());

    return Results;
}

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeEquip(const FString& ItemName)
{
    TArray<FAoCValidationResult> Results;

    FAoCValidationResult EquipCheck;
    EquipCheck.TestName = FString::Printf(TEXT("Equip: %s"), *ItemName);
    EquipCheck.Category = TEXT("System");
    EquipCheck.bPassed = false;
    EquipCheck.Details = FString::Printf(TEXT("Attempting to equip '%s'. Checking if item exists in inventory and if equip socket is available."), *ItemName);
    EquipCheck.Severity = 1;
    Results.Add(EquipCheck);

    return Results;
}

TArray<FAoCValidationResult> UAoCOracleWorldValidator::ValidateBeforeEating()
{
    TArray<FAoCValidationResult> Results;

    FAoCValidationResult FoodCheck;
    FoodCheck.TestName = TEXT("Food Available");
    FoodCheck.Category = TEXT("Inventory");

    TArray<AActor*> Food = FindActorsWithTag(FName("Item_Food"), 100000.f);
    if (Food.Num() > 0)
    {
        FoodCheck.bPassed = true;
        FoodCheck.Details = FString::Printf(TEXT("Found %d food items. Can eat!"), Food.Num());
    }
    else
    {
        FoodCheck.bPassed = false;
        FoodCheck.Details = TEXT("I'm hungry but there's NO FOOD anywhere! No cooked food, no raw food, nothing. I'm starving.");
        FoodCheck.FixSuggestion = TEXT("Spawn food items with tag 'Item_Food' or ensure cooking produces food items. Fish → Cook → Cooked Fish should work.");
        FoodCheck.Severity = 2;
    }
    Results.Add(FoodCheck);

    return Results;
}

// ==================== REPORT GENERATION ====================

FString UAoCOracleWorldValidator::GenerateChatReport(const FAoCValidationResult& Result)
{
    if (Result.bPassed)
    {
        return FString::Printf(TEXT("%s: PASSED! %s"), *Result.TestName, *Result.Details);
    }

    switch (Result.Severity)
    {
    case 2:
        return FString::Printf(TEXT("CRITICAL — %s: %s"), *Result.TestName, *Result.Details);
    case 1:
        return FString::Printf(TEXT("WARNING — %s: %s"), *Result.TestName, *Result.Details);
    default:
        return FString::Printf(TEXT("INFO — %s: %s"), *Result.TestName, *Result.Details);
    }
}

FString UAoCOracleWorldValidator::GenerateLogReport(const FAoCWorldValidationReport& Report)
{
    FString Log;
    Log += FString::Printf(TEXT("=== ORACLE WORLD VALIDATION REPORT ===\n"));
    Log += FString::Printf(TEXT("Timestamp: %s\n"), *Report.Timestamp.ToString());
    Log += FString::Printf(TEXT("Total: %d | Passed: %d | Warnings: %d | Critical: %d\n\n"),
        Report.TotalTests, Report.Passed, Report.Warnings, Report.Critical);

    for (const auto& R : Report.Results)
    {
        FString Status = R.bPassed ? TEXT("[PASS]") : (R.Severity >= 2 ? TEXT("[CRIT]") : (R.Severity >= 1 ? TEXT("[WARN]") : TEXT("[INFO]")));
        Log += FString::Printf(TEXT("%s %s: %s\n"), *Status, *R.TestName, *R.Details);
        if (!R.bPassed && !R.FixSuggestion.IsEmpty())
        {
            Log += FString::Printf(TEXT("  FIX: %s\n"), *R.FixSuggestion);
        }
    }

    Log += TEXT("\n=== END REPORT ===\n");
    return Log;
}

void UAoCOracleWorldValidator::SaveReportToDisk(const FAoCWorldValidationReport& Report)
{
    // Ensure directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*ReportDirectory);

    // Write timestamped report
    FString Filename = FString::Printf(TEXT("oracle_validation_%s.txt"),
        *Report.Timestamp.ToString().Replace(TEXT(":"), TEXT("-")).Replace(TEXT(" "), TEXT("_")));
    FString FullPath = ReportDirectory / Filename;

    FString LogText = GenerateLogReport(Report);
    FFileHelper::SaveStringToFile(LogText, *FullPath);

    // Also append to main oracle chat log
    FString ChatLogPath = FPaths::ProjectDir() / TEXT("AoC_GameData") / TEXT("oracle_chat_log.txt");
    FString ChatEntry = FString::Printf(TEXT("\n[VALIDATION %s] %d/%d passed, %d critical, %d warnings\n"),
        *Report.Timestamp.ToString(), Report.Passed, Report.TotalTests, Report.Critical, Report.Warnings);

    // Add critical failures to chat log
    for (const auto& R : Report.Results)
    {
        if (!R.bPassed && R.Severity >= 2)
        {
            ChatEntry += FString::Printf(TEXT("  CRITICAL: %s — %s\n"), *R.TestName, *R.Details);
        }
    }

    FFileHelper::SaveStringToFile(ChatEntry, *ChatLogPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);

    UE_LOG(LogTemp, Log, TEXT("[Oracle] Validation report saved: %s"), *FullPath);
}

// ==================== HELPERS ====================

TArray<AActor*> UAoCOracleWorldValidator::FindActorsWithTag(const FName& Tag, float Radius)
{
    TArray<AActor*> Found;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, AllActors);

    FVector MyLoc = GetOwner()->GetActorLocation();

    for (AActor* Actor : AllActors)
    {
        if (FVector::Dist(MyLoc, Actor->GetActorLocation()) <= Radius)
        {
            Found.Add(Actor);
        }
    }

    // Sort by distance
    Found.Sort([MyLoc](const AActor& A, const AActor& B)
    {
        return FVector::Dist(MyLoc, A.GetActorLocation()) < FVector::Dist(MyLoc, B.GetActorLocation());
    });

    return Found;
}

TArray<AActor*> UAoCOracleWorldValidator::FindActorsByClass(UClass* ActorClass, float Radius)
{
    TArray<AActor*> Found;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ActorClass, AllActors);

    FVector MyLoc = GetOwner()->GetActorLocation();

    for (AActor* Actor : AllActors)
    {
        if (FVector::Dist(MyLoc, Actor->GetActorLocation()) <= Radius)
        {
            Found.Add(Actor);
        }
    }

    return Found;
}

bool UAoCOracleWorldValidator::CheckDataTablePopulated(const FString& TablePath, int32& OutRowCount)
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, *TablePath);
    if (Table)
    {
        OutRowCount = Table->GetRowMap().Num();
        return OutRowCount > 0;
    }
    OutRowCount = 0;
    return false;
}

bool UAoCOracleWorldValidator::CheckBlueprintExists(const FString& BlueprintPath)
{
    return LoadObject<UObject>(nullptr, *BlueprintPath) != nullptr;
}

bool UAoCOracleWorldValidator::CheckMeshVisible(AActor* Actor)
{
    if (!Actor) return false;
    UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
    if (SMC && SMC->IsVisible() && SMC->GetStaticMesh()) return true;
    USkeletalMeshComponent* SkMC = Actor->FindComponentByClass<USkeletalMeshComponent>();
    if (SkMC && SkMC->IsVisible() && SkMC->GetSkeletalMeshAsset()) return true;
    return false;
}

bool UAoCOracleWorldValidator::CheckActorHasCollision(AActor* Actor)
{
    if (!Actor) return false;
    UPrimitiveComponent* Prim = Actor->FindComponentByClass<UPrimitiveComponent>();
    return Prim && Prim->IsCollisionEnabled();
}
