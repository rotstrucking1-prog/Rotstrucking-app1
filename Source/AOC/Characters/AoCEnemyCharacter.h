// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AoCCharacterBase.h"
#include "AoCEnemyCharacter.generated.h"

class AAIController;
class UWidgetComponent;

/**
 * Loot drop entry for an enemy.
 */
USTRUCT(BlueprintType)
struct FAoCLootDrop
{
	GENERATED_BODY()

	/** Item row name in the items DataTable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Loot")
	FName ItemID;

	/** Probability of this item dropping (0.0 – 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance;

	/** Min quantity to drop (if roll succeeds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Loot", meta = (ClampMin = "1"))
	int32 MinQuantity;

	/** Max quantity to drop (if roll succeeds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Loot", meta = (ClampMin = "1"))
	int32 MaxQuantity;

	FAoCLootDrop()
		: ItemID(NAME_None)
		, DropChance(1.0f)
		, MinQuantity(1)
		, MaxQuantity(1)
	{}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDied, AAoCEnemyCharacter*, Enemy, AActor*, Killer);

/**
 * AAoCEnemyCharacter
 * AI-controlled enemy character with loot, XP reward, and aggro behaviour.
 */
UCLASS()
class AOC_API AAoCEnemyCharacter : public AAoCCharacterBase
{
	GENERATED_BODY()

public:
	AAoCEnemyCharacter();

	// ── Creature Data ──

	/** Row name in DT_Creatures to look up stats, mesh, abilities, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Enemy")
	FName CreatureDataRowName;

	// ── Aggro ──

	/** Distance at which this enemy notices players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Enemy|Aggro")
	float AggroRange;

	/** Distance from home at which the enemy gives up and returns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Enemy|Aggro")
	float LeashRange;

	/** Cached reference to the AI controller. */
	UPROPERTY(BlueprintReadOnly, Category = "AoC|Enemy")
	TObjectPtr<AAIController> AoCAIController;

	// ── Loot ──

	/** Loot table for this enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Enemy|Loot")
	TArray<FAoCLootDrop> LootTable;

	/** Roll loot from the loot table. Called on server on death. Returns item IDs and quantities. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Enemy|Loot")
	TArray<FAoCLootDrop> RollLoot() const;

	// ── XP Reward ──

	/** Base XP rewarded to the killer upon death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Enemy|Reward")
	int32 XPReward;

	/** Skill type the XP should be granted to (e.g. "Combat", "Magic"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Enemy|Reward")
	FName XPSkillType;

	// ── Health Bar Widget ──

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Enemy|UI")
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	// ── Death ──

	virtual void HandleDeath() override;

	/** Broadcast when this enemy dies, passing the killer. */
	UPROPERTY(BlueprintAssignable, Category = "AoC|Enemy")
	FOnEnemyDied OnEnemyDied;

	/** Time in seconds before the corpse is destroyed after death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Enemy")
	float CorpseLifespan;

protected:
	virtual void BeginPlay() override;

private:
	/** The actor that dealt the killing blow. Set by HandleDeath. */
	UPROPERTY()
	TObjectPtr<AActor> LastDamageInstigator;

	/** Grant XP to the killer if they have a SkillComponent. */
	void GrantXPToKiller(AActor* Killer);

	/** Spawn loot drops in the world. */
	void SpawnLootDrops();
};
