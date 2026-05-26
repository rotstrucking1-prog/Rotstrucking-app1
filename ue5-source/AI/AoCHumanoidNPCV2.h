// AoCHumanoidNPCV2.h
// Master NPC Character class — wires all AI systems together
// Replaces AoCHumanoidNPCCharacter with full deep AI integration
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AoCNPCSkillSystem.h"
#include "AoCNPCCombatBrain.h"
#include "AoCNPCLootBrain.h"
#include "AoCNPCSocialBrain.h"
#include "AoCNPCLifeBrain.h"
#include "AoCHumanoidNPCV2.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCNPCV2, Log, All);

class UAoCAILODManager;
class UAoCNPCNeedSystem;
class UAoCNPCMemory;
class UAoCNPCInventory;
class UAoCNPCGoalPlanner;
class UAoCNPCPersonality;
class UAoCNPCBrainV2;

enum class EAILODTier : uint8;

/** Serializable state for full NPC save/load */
USTRUCT(BlueprintType)
struct FNPCFullState
{
	GENERATED_BODY()

	UPROPERTY()
	FString NPCName;

	UPROPERTY()
	FGuid UniqueID;

	UPROPERTY()
	FName FactionID;

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FSkillSystemState SkillState;

	UPROPERTY()
	float CurrentHP = 100.f;
	UPROPERTY()
	float MaxHP = 100.f;
	UPROPERTY()
	float CurrentMana = 50.f;
	UPROPERTY()
	float MaxMana = 50.f;

	UPROPERTY()
	uint8 Archetype = 0;

	UPROPERTY()
	FVector HomeLocation;

	UPROPERTY()
	bool bIsDead = false;

	UPROPERTY()
	float RespawnTimer = 0.f;
};

/** NPC name table for random name generation */
USTRUCT()
struct FNameTable
{
	GENERATED_BODY()

	static const TArray<FString>& GetFirstNames();
	static const TArray<FString>& GetLastNames();
	static FString GenerateRandomName(FRandomStream& Rand);
};

/**
 * AoCHumanoidNPCV2 — ACharacter
 *
 * The master character class that wires ALL AI systems together.
 * Manages LOD-aware ticking, death/respawn, state serialization,
 * and is the entity placed in the world.
 */
UCLASS()
class AOC_API AoCHumanoidNPCV2 : public ACharacter
{
	GENERATED_BODY()

public:
	AoCHumanoidNPCV2();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	// --- Initialization ---

	/** Full initialization from archetype — sets up all components */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	void InitializeNPC();

	// --- Master Tick ---

	/** LOD-aware update that delegates to appropriate subsystems */
	void MasterTick(float DeltaTime);

	// --- Damage / Death ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	void TakeDamageFromSource(float Damage, AActor* Instigator);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	void OnDeath(AActor* Killer);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	void OnRespawn();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	bool IsDead() const { return bIsDead; }

	// --- Display / UI ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	FString GetDisplayName() const { return NPCName; }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	float GetLevel() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	float GetCombatPower() const;

	// --- State Serialization ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	FNPCFullState SaveState() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	void LoadState(const FNPCFullState& State);

	// --- LOD Helpers ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	EAILODTier GetCurrentLODTier() const { return CachedLODTier; }

	/** Called by LOD manager when visual components should be spawned */
	void OnMaterialize();

	/** Called by LOD manager when visual components should be stripped */
	void OnDematerialize();

	bool IsMaterialized() const { return bIsMaterialized; }

	// --- Unique ID ---
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC")
	FGuid GetUniqueID() const { return UniqueID; }

	// --- Editor Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	EPersonalityArchetype ArchetypePreset = EPersonalityArchetype::Villager;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	FString NPCName = TEXT("NPC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	FName FactionID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	bool bIsAggressive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	bool bCanLoot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	bool bCanCraft = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	bool bCanTrade = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	bool bIsEssential = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	TArray<FVector> PatrolPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	TArray<FNPCItemData> SpawnEquipment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	float RespawnDelay = 300.f; // seconds

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC")
	bool bPermanentDeath = false;

	// --- Components (ALL exposed to editor) ---

	/** Existing systems */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCNeedSystem* NeedSystem = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCMemory* Memory = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCInventory* Inventory = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCGoalPlanner* GoalPlanner = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCPersonality* Personality = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCBrainV2* Brain = nullptr;

	/** NEW deep AI systems */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCSkillSystem* Skills = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCCombatBrain* CombatBrain = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCLootBrain* LootBrain = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCSocialBrain* SocialBrain = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|NPC|Components")
	UAoCNPCLifeBrain* LifeBrain = nullptr;

private:
	// --- Internal State ---
	FGuid UniqueID;
	bool bIsDead = false;
	bool bIsInitialized = false;
	bool bIsMaterialized = true;
	float DeathTimer = 0.f;
	float RespawnTimer = 0.f;

	/** Cached LOD tier to avoid repeated lookups */
	EAILODTier CachedLODTier;

	/** Generate starting equipment based on archetype and skill levels */
	void GenerateStartingEquipment();

	/** Calculate max HP/Mana/Stamina from attributes */
	void RecalculateVitals();

	/** Spawn lootable corpse with NPC's inventory */
	void SpawnCorpse();

	/** Drop all inventory items at death location */
	void DropAllItems();

	/** Setup component cross-references */
	void LinkComponents();

	FRandomStream NPCRand;
};
