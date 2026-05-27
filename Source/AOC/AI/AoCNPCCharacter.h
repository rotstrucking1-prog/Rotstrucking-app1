// AoCNPCCharacter.h — NPC Character with V2 brain + full life simulation
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AoCNPCPersonality.h"
#include "AoCNPCCharacter.generated.h"

class UAoCNPCBrainV2;
class UAoCNPCNeedSystem;
class UAoCNPCMemory;
class UAoCNPCInventory;
class UAoCNPCGoalPlanner;
class UAoCNPCPersonality;
class UAoCSmartAnimPlayer;

/**
 * AAoCNPCCharacter
 *
 * Base NPC character with integrated V2 AI brain and full life simulation.
 * Auto-creates all AI sub-components: BrainV2, NeedSystem, Memory,
 * Inventory, GoalPlanner, Personality, and SmartAnimPlayer.
 *
 * Configurable in the editor for any NPC type (hostile, friendly, merchant, etc.)
 */
UCLASS(Blueprintable)
class AOC_API AAoCNPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAoCNPCCharacter();

	// --- Components (auto-created) ---------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCNPCBrainV2> NPCBrain;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCSmartAnimPlayer> AnimPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCNPCNeedSystem> NeedSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCNPCMemory> Memory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCNPCInventory> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCNPCGoalPlanner> GoalPlanner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCNPCPersonality> Personality;

	// --- Stats -----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float Mana = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	float MaxMana = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FString NPCName = TEXT("Peasant");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	bool bIsDead = false;

	// --- Identity --------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString Race = TEXT("Human");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FString Faction = TEXT("Neutral");

	// --- Personality Configuration (convenience — forwards to Personality component) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	ENPCArchetype NPCArchetype = ENPCArchetype::Villager;

	// --- Behavior config -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bIsAggressive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float AggroRadius = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float LeashRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float RespawnTime = 30.f;

	/** Patrol waypoints (world-space). Passed to the brain on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	TArray<FVector> PatrolPoints;

	/** Known locations for this NPC (set in editor). Key = location name, Value = world pos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	TMap<FName, FVector> InitialKnownLocations;

	/** Known spells for this NPC (from the 160 spell catalog). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Magic")
	TArray<FName> KnownSpells;

	/** Magic schools this NPC specializes in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Magic")
	TArray<FName> MagicSchools;

	/** Weapon tags for animation queries (e.g. "sword", "bow", "unarmed"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Combat")
	TArray<FString> WeaponTags;

	// --- Aggro -----------------------------------------------------------------

	/** Aggro table: Actor → threat value */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TMap<TObjectPtr<AActor>, float> AggroTable;

	// --- Public API ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "NPC")
	FString GetNPCName() const { return NPCName; }

	UFUNCTION(BlueprintCallable, Category = "NPC")
	float GetHealthPercent() const { return (MaxHealth > 0.f) ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintCallable, Category = "NPC")
	float GetManaPercent() const { return (MaxMana > 0.f) ? Mana / MaxMana : 0.f; }

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void Die();

	/** Add threat for an actor in the aggro table */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void AddAggro(AActor* Source, float Amount);

	/** Get the actor with the highest aggro */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	AActor* GetTopAggroTarget() const;

	// --- Damage ----------------------------------------------------------------

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Original spawn location for respawning */
	FVector SpawnLocation;

	/** Original spawn rotation */
	FRotator SpawnRotation;

	/** Timer for respawn */
	FTimerHandle RespawnTimerHandle;

	/** Called when the respawn timer fires */
	void Respawn();
};
