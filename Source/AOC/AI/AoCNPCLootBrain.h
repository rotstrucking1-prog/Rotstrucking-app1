// AoCNPCLootBrain.h
// NPC Loot AI — intelligent post-kill looting like a real player
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCInventory.h"
#include "AoCNPCLootBrain.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCLoot, Log, All);

class AoCHumanoidNPCV2;
class UAoCNPCSkillSystem;
class UAoCNPCInventory;
class UAoCNPCPersonality;
class UAoCNPCMemory;

/** Item data struct (mirrors AoCNPCInventory::FNPCItemData) */
USTRUCT(BlueprintType)
struct FNPCItemData
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	EEquipSlot Slot = EEquipSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BaseDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BaseArmor = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float Value = 0.f; // Gold value

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	FName RequiredSkill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float RequiredLevel = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	bool bIsConsumable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	bool bIsGold = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	bool bIsMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float GoldAmount = 0.f;

	/** Stat bonuses */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BonusSTR = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BonusDEX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BonusAGI = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BonusINT = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BonusWIS = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BonusEND = 0.f;
};

/** Loot evaluation result */
USTRUCT(BlueprintType)
struct FLootScore
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FNPCItemData Item;

	UPROPERTY(BlueprintReadOnly)
	float Score = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ImprovementOverEquipped = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bShouldEquip = false;

	UPROPERTY(BlueprintReadOnly)
	bool bCanUse = true;
};

/**
 * UAoCNPCLootBrain — UActorComponent
 *
 * After killing a target, NPCs loot intelligently like a real player.
 * Evaluates gear upgrades, manages weight, equips improvements immediately,
 * and adjusts behavior based on danger level.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCLootBrain : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCLootBrain();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	float GetAverageLootValue() const
	{
		// Stub until inventory integration
		return 0.0f;
	}

	virtual void BeginPlay() override;

	// --- Core Loot Interface ---

	/** Begin looting a corpse — full evaluation and pickup sequence */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	void BeginLooting(AActor* Corpse);

	/** Evaluate a single item's desirability (0-1000 scale) */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	float EvaluateItem(const FNPCItemData& Item) const;

	/** Compare an item to what's currently equipped in its slot */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	float CompareToEquipped(const FNPCItemData& Item, EEquipSlot Slot) const;

	/** Take an item from the loot, potentially equipping it */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	void TakeItem(const FNPCItemData& Item);

	/** Quick loot — grab best weapon + gold and go (for dangerous situations) */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	void QuickLoot(AActor* Corpse);

	/** Tick the loot sequence (called by brain during looting phase) */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	void LootTick(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	bool IsLooting() const { return bIsLooting; }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Loot")
	float GetLootTimeRemaining() const { return LootTimeRemaining; }

	// --- Configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float BaseLootDuration = 3.0f; // seconds to fully loot

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float QuickLootDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Loot")
	float MaxCarryWeight = 300.f;

private:
	// --- References ---
	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> OwnerNPC;

	UPROPERTY()
	UAoCNPCSkillSystem* SkillSystem = nullptr;

	UPROPERTY()
	UAoCNPCInventory* Inventory = nullptr;

	UPROPERTY()
	UAoCNPCPersonality* PersonalityComp = nullptr;

	UPROPERTY()
	UAoCNPCMemory* Memory = nullptr;

	// --- Loot State ---
	bool bIsLooting = false;
	float LootTimeRemaining = 0.f;
	TWeakObjectPtr<AActor> CurrentCorpse;

	/** Sorted list of items to pick up from current corpse */
	TArray<FLootScore> PrioritizedLoot;
	int32 CurrentLootIndex = 0;

	/** Whether we're in quick-loot mode */
	bool bQuickMode = false;

	// --- Internal ---

	/** Score all items on a corpse and sort by priority */
	void EvaluateCorpseLoot(AActor* Corpse);

	/** Check if NPC can actually use an item (skill requirements) */
	bool CanUseItem(const FNPCItemData& Item) const;

	/** Get the "value density" of an item (value / weight) */
	float GetValueDensity(const FNPCItemData& Item) const;

	/** Make room in bag by dropping lowest-value items */
	bool MakeRoomForItem(const FNPCItemData& Item);

	/** Get current carry weight */
	float GetCurrentWeight() const;

	/** Check if area is dangerous (enemies nearby) */
	bool IsAreaDangerous() const;

	/** Score multiplier based on NPC's personality/archetype */
	float GetPersonalityScoreMultiplier(const FNPCItemData& Item) const;

	/** Notify other systems of loot completion (memory, combat power update) */
	void OnLootingComplete();

	FRandomStream LootRand;

	public:
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Loot")
	bool AttemptLootNearestCorpse();

};
