// AoCOracleWorldValidator.h
// Architect of Creation — Oracle World Content Validator
// The Oracle's EYES — checks if the world has everything needed for gameplay.
// Every test says exactly what's missing and reports via chat bubble + log.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCOracleWorldValidator.generated.h"

// Detailed report for a single validation check
USTRUCT(BlueprintType)
struct FAoCValidationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString TestName;             // "MiningNodes", "FishingPoles", "Trees", etc.

    UPROPERTY(BlueprintReadOnly)
    bool bPassed = false;

    UPROPERTY(BlueprintReadOnly)
    FString Details;              // Human-readable: "Found 0 mining rocks in 500m radius"

    UPROPERTY(BlueprintReadOnly)
    FString FixSuggestion;        // "Need to spawn rock actors with tag 'Harvestable_Ore'"

    UPROPERTY(BlueprintReadOnly)
    FString Category;             // "WorldContent", "Inventory", "System", "Animation", "UI"

    UPROPERTY(BlueprintReadOnly)
    int32 Severity = 0;           // 0=info, 1=warning, 2=critical (blocks gameplay)
};

// Full validation report
USTRUCT(BlueprintType)
struct FAoCWorldValidationReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FAoCValidationResult> Results;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalTests = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Passed = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Warnings = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 Critical = 0;

    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;
};

// Resource requirement definition
USTRUCT()
struct FAoCResourceRequirement
{
    GENERATED_BODY()

    FString Name;                  // "Iron Ore Node"
    FString ActorTag;              // "Harvestable_Ore" or "Resource_Iron"
    FString ActorClass;            // "AStaticMeshActor" or custom class
    FString Category;              // "Mining", "Woodcutting", "Fishing", etc.
    int32 MinExpected = 1;         // At least 1 should exist
    float SearchRadius = 50000.f;  // 500m default search radius
    FString FixSuggestion;         // What to do if missing
};

UCLASS(ClassGroup=(AoC), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCOracleWorldValidator : public UActorComponent
{
    GENERATED_BODY()

public:
    UAoCOracleWorldValidator();

    // ========== MASTER VALIDATION ==========

    // Run ALL validation checks — full world audit
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    FAoCWorldValidationReport RunFullWorldValidation();

    // Run a specific category only
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    FAoCWorldValidationReport RunCategoryValidation(const FString& Category);

    // ========== RESOURCE CHECKS ==========
    // Each returns a specific, actionable result

    // GATHERING RESOURCES — Are there things to gather?
    FAoCValidationResult CheckMiningNodes();         // Any rocks/ore veins?
    FAoCValidationResult CheckTrees();               // Any harvestable trees?
    FAoCValidationResult CheckFishingSpots();         // Any water with fish spawns?
    FAoCValidationResult CheckHerbNodes();            // Any herb/plant pickups?
    FAoCValidationResult CheckHuntingTargets();       // Any animals to hunt?
    FAoCValidationResult CheckFarmingPlots();         // Any farmable land?
    FAoCValidationResult CheckSkinningTargets();      // Any dead animals to skin?

    // TOOLS & EQUIPMENT — Do the tools exist?
    FAoCValidationResult CheckPickaxeExists();        // Can I get a pickaxe?
    FAoCValidationResult CheckAxeExists();            // Can I get a woodcutting axe?
    FAoCValidationResult CheckFishingPoleExists();    // Can I get a fishing pole?
    FAoCValidationResult CheckWeaponsExist();         // Are there weapons to equip?
    FAoCValidationResult CheckArmorExists();          // Is there armor to wear?
    FAoCValidationResult CheckStarterGear();          // Does a new player have ANYTHING?

    // CRAFTING — Can I actually craft things?
    FAoCValidationResult CheckCraftingStations();     // Forges, anvils, workbenches?
    FAoCValidationResult CheckRecipesLoaded();        // Are crafting recipes in DataTables?
    FAoCValidationResult CheckMaterialsObtainable();  // Can raw materials become products?

    // WORLD INFRASTRUCTURE — Is the world playable?
    FAoCValidationResult CheckSpawnPoint();           // Is there a PlayerStart?
    FAoCValidationResult CheckNavMesh();              // Can NPCs pathfind?
    FAoCValidationResult CheckGroundExists();         // Is there terrain to walk on?
    FAoCValidationResult CheckLighting();             // Is the world visible?
    FAoCValidationResult CheckSkyAndWeather();        // Sky, fog, atmosphere?
    FAoCValidationResult CheckWaterBodies();          // Any water for fishing?
    FAoCValidationResult CheckBuildableAreas();       // Can players build structures?

    // NPCS & CREATURES — Is the world populated?
    FAoCValidationResult CheckFriendlyNPCs();         // Merchants, quest givers?
    FAoCValidationResult CheckHostileCreatures();     // Monsters to fight?
    FAoCValidationResult CheckNPCPathfinding();       // Can NPCs actually move?
    FAoCValidationResult CheckNPCAnimations();        // Are NPCs animated or T-posing?

    // COMBAT — Does fighting work?
    FAoCValidationResult CheckMeleeHitDetection();    // Do swings connect?
    FAoCValidationResult CheckSpellCasting();         // Can spells be cast?
    FAoCValidationResult CheckDamageNumbers();        // Do damage values appear?
    FAoCValidationResult CheckDeathAndRespawn();      // Can I die and come back?
    FAoCValidationResult CheckLootDrops();            // Do dead things drop loot?

    // EQUIPMENT — Does gear actually work?
    FAoCValidationResult CheckEquipToHand();          // Does weapon appear in hand?
    FAoCValidationResult CheckArmorVisuals();         // Does armor show on body?
    FAoCValidationResult CheckStatChanges();          // Does equipment change stats?

    // UI — Can the player see their info?
    FAoCValidationResult CheckHUDVisible();           // HP/MP/Stamina bars showing?
    FAoCValidationResult CheckInventoryOpens();       // Does I key open inventory?
    FAoCValidationResult CheckChatWorks();            // Can chat messages be sent?

    // SKILLS — Does progression work?
    FAoCValidationResult CheckSkillGainOnUse();       // Does mining increase Mining skill?
    FAoCValidationResult CheckAttributeGrowth();      // Do attributes go up?
    FAoCValidationResult CheckMasteryUnlocks();       // Do mastery tiers work?

    // ========== CONTEXTUAL VALIDATION ==========
    // These run DURING gameplay when Oracle tries to do something

    // Called when Oracle is about to mine — checks everything needed for mining
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeMining();

    // Called when Oracle is about to fish
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeFishing();

    // Called when Oracle is about to chop wood
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeWoodcutting();

    // Called when Oracle is about to craft
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeCrafting(const FString& RecipeName);

    // Called when Oracle is about to fight
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeCombat();

    // Called when Oracle is about to equip something
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeEquip(const FString& ItemName);

    // Called when Oracle is about to cook/eat
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    TArray<FAoCValidationResult> ValidateBeforeEating();

    // ========== REPORT GENERATION ==========

    // Generate human-readable report for chat bubble
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    FString GenerateChatReport(const FAoCValidationResult& Result);

    // Generate full report for log file
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    FString GenerateLogReport(const FAoCWorldValidationReport& Report);

    // Write report to disk
    UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Validation")
    void SaveReportToDisk(const FAoCWorldValidationReport& Report);

private:
    // Helper: find actors by tag within radius
    TArray<AActor*> FindActorsWithTag(const FName& Tag, float Radius);

    // Helper: find actors by class within radius
    TArray<AActor*> FindActorsByClass(UClass* ActorClass, float Radius);

    // Helper: check if DataTable has rows
    bool CheckDataTablePopulated(const FString& TablePath, int32& OutRowCount);

    // Helper: check if a Blueprint class exists
    bool CheckBlueprintExists(const FString& BlueprintPath);

    // Helper: check if mesh is visible (not invisible/has material)
    bool CheckMeshVisible(AActor* Actor);

    // Helper: check if actor has valid collision
    bool CheckActorHasCollision(AActor* Actor);

    // Validation requirement definitions
    TArray<FAoCResourceRequirement> GatheringRequirements;
    TArray<FAoCResourceRequirement> CraftingRequirements;
    TArray<FAoCResourceRequirement> CombatRequirements;
    TArray<FAoCResourceRequirement> InfrastructureRequirements;

    void InitializeRequirements();

    // Report output path
    FString ReportDirectory;
};
