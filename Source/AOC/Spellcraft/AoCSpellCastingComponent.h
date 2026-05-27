// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "AoCSpellCastingComponent.generated.h"

class AAoCProjectile;
class UAoCAbilitySystemComponent;

/**
 * FAoCSpellSlot
 * Represents one slot on the player's spell bar.
 */
USTRUCT(BlueprintType)
struct FAoCSpellSlot
{
	GENERATED_BODY()

	/** Row name in the spells DataTable. Empty = unoccupied slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Spell")
	FName SpellID;

	/** Cached icon texture path (loaded from DataTable). */
	UPROPERTY(BlueprintReadOnly, Category = "AoC|Spell")
	TSoftObjectPtr<UTexture2D> Icon;

	FAoCSpellSlot()
		: SpellID(NAME_None)
	{}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpellCast, int32, SlotIndex, FName, SpellID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpellFailed, int32, SlotIndex, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCastProgress, float, ElapsedTime, float, TotalCastTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCastInterrupted);

/**
 * UAoCSpellCastingComponent
 * Manages the player's spell bar, cooldowns, cast times, and spell execution.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class ARCHITECTOFCREATION_API UAoCSpellCastingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSpellCastingComponent();

	// ── Configuration ──

	/** Maximum number of spell slots on the bar. */
	static constexpr int32 MaxSpellSlots = 10;

	/** Global cooldown duration in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Spellcast|Config")
	float GlobalCooldownDuration;

	/** DataTable containing spell data rows (FAoCSpellDataRow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Spellcast|Config")
	TObjectPtr<UDataTable> SpellDataTable;

	// ── Spell Bar Management ──

	/** Current spell slots. */
	UPROPERTY(BlueprintReadOnly, Category = "AoC|Spellcast")
	TArray<FAoCSpellSlot> SpellSlots;

	/** Equip a spell into a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	bool EquipSpell(int32 SlotIndex, FName SpellID);

	/** Remove a spell from a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	void UnequipSpell(int32 SlotIndex);

	/** Swap the contents of two spell slots. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	void SwapSlots(int32 SlotA, int32 SlotB);

	// ── Casting ──

	/** Attempt to cast the spell in the given slot. Checks mana, cooldown, cast time. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	void CastSpell(int32 SlotIndex);

	/** Interrupt the current cast, if any. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	void InterruptCast();

	/** Returns true if the player is currently casting. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	bool IsCasting() const { return bIsCasting; }

	/** Get remaining cooldown for a spell by ID. Returns 0 if off cooldown. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	float GetCooldownRemaining(FName SpellID) const;

	/** Returns true if the global cooldown is active. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	bool IsGlobalCooldownActive() const { return GlobalCooldownRemaining > 0.0f; }

	// ── Delegates ──

	UPROPERTY(BlueprintAssignable, Category = "AoC|Spellcast")
	FOnSpellCast OnSpellCast;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Spellcast")
	FOnSpellFailed OnSpellFailed;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Spellcast")
	FOnCastProgress OnCastProgress;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Spellcast")
	FOnCastInterrupted OnCastInterrupted;

	// ── Helpers ──

	/** Spawn a projectile at the character's muzzle location heading toward the target. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	AAoCProjectile* SpawnProjectile(TSubclassOf<AAoCProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation);

	/** Apply an area-of-effect at the given location with a radius. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spellcast")
	void ApplyAOE(FVector Origin, float Radius, TSubclassOf<UGameplayEffect> EffectClass);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ── Cooldown Tracking ──

	/** Per-spell cooldown remaining in seconds. */
	TMap<FName, float> CooldownMap;

	/** Global cooldown remaining. */
	float GlobalCooldownRemaining;

	// ── Casting State ──

	bool bIsCasting;
	int32 CurrentCastSlot;
	FName CurrentCastSpellID;
	float CastTimeElapsed;
	float CastTimeTotal;

	/** Finish a cast that has completed its cast time. */
	void FinishCast();

	/** Execute the actual spell effect (projectile, AOE, instant, etc.). */
	void ExecuteSpell(FName SpellID);

	/** Start the global cooldown. */
	void StartGlobalCooldown();

	/** Tick down all cooldowns. */
	void TickCooldowns(float DeltaTime);
};
