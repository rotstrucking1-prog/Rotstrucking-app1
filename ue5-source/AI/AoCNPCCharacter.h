// AoCNPCCharacter.h — NPC Character with brain + animation player
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AoCNPCCharacter.generated.h"

class UAoCNPCBrain;
class UAoCSmartAnimPlayer;

/**
 * AAoCNPCCharacter
 *
 * Base NPC character with integrated AI brain and smart animation player.
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
	TObjectPtr<UAoCNPCBrain> NPCBrain;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAoCSmartAnimPlayer> AnimPlayer;

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
