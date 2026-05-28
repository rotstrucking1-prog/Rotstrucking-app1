// Source/AOC/Spellcraft/AoCSpellCastingComponent.h
// Spell casting component — manages the spell bar, cooldowns, and spell execution.
// Standalone — no GAS plugin dependency.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSpellData.h"
#include "AoCSpellCastingComponent.generated.h"

class UAoCSpellVFXManager;
class AAoCProjectile;

/**
 * Spell bar slot — a spell assigned to a hotbar position.
 */
USTRUCT(BlueprintType)
struct FAoCSpellBarSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpellBar")
	FName SpellID;

	UPROPERTY(BlueprintReadOnly, Category = "SpellBar")
	float CooldownRemaining;

	UPROPERTY(BlueprintReadOnly, Category = "SpellBar")
	float CooldownTotal;

	UPROPERTY(BlueprintReadOnly, Category = "SpellBar")
	bool bOnCooldown;

	FAoCSpellBarSlot()
		: CooldownRemaining(0.f)
		, CooldownTotal(0.f)
		, bOnCooldown(false)
	{
	}
};

/**
 * Manages spell casting for any character.
 * Holds a spell bar (up to 10 slots), handles cooldowns, cast times, and dispatches
 * execution to type-specific handlers.
 *
 * Damage is applied via UGameplayStatics::ApplyDamage — NO GAS required.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSpellCastingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSpellCastingComponent();

	// ── Spell Bar ───────────────────────────────────────────────────────

	/** The active spell bar (up to 10 slots) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpellBar")
	TArray<FAoCSpellBarSlot> SpellBar;

	/** Maximum number of spell bar slots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpellBar")
	int32 MaxSpellBarSlots;

	// ── Casting state ───────────────────────────────────────────────────

	/** Is the character currently casting? */
	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	bool bIsCasting;

	/** Current spell being cast (valid only while bIsCasting) */
	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	FName CurrentCastSpellID;

	/** Remaining cast time */
	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	float CastTimeRemaining;

	/** Current mana */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Casting")
	float CurrentMana;

	/** Max mana */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Casting")
	float MaxMana;

	/** Mana regeneration per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Casting")
	float ManaRegenRate;

	// ── Channeling state ────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	bool bIsChanneling;

	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	float ChannelTimeRemaining;

	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	float ChannelTickInterval;

	// ── Current target ──────────────────────────────────────────────────

	UPROPERTY(BlueprintReadWrite, Category = "Targeting")
	AActor* CurrentTarget;

	UPROPERTY(BlueprintReadWrite, Category = "Targeting")
	FVector TargetLocation;

	// ── VFX reference ───────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	UAoCSpellVFXManager* VFXManager;

	// ── Public interface ────────────────────────────────────────────────

	/** Assign a spell to a slot */
	UFUNCTION(BlueprintCallable, Category = "AoC|SpellBar")
	void AssignSpellToSlot(int32 SlotIndex, FName SpellID);

	/** Cast the spell in the given slot */
	UFUNCTION(BlueprintCallable, Category = "AoC|SpellBar")
	void CastSpellInSlot(int32 SlotIndex);

	/** Execute a spell by ID — main entry point */
	UFUNCTION(BlueprintCallable, Category = "AoC|Casting")
	void ExecuteSpell(FName SpellID);

	/** Cancel the current cast or channel */
	UFUNCTION(BlueprintCallable, Category = "AoC|Casting")
	void CancelCast();

	/** Check if a spell can be cast right now */
	UFUNCTION(BlueprintCallable, Category = "AoC|Casting")
	bool CanCastSpell(FName SpellID) const;

	/** Get cooldown fraction (0 = ready, 1 = just started cooldown) */
	UFUNCTION(BlueprintCallable, Category = "AoC|SpellBar")
	float GetCooldownFraction(int32 SlotIndex) const;

	/** Set target actor for targeted spells */
	UFUNCTION(BlueprintCallable, Category = "AoC|Targeting")
	void SetTarget(AActor* NewTarget);

	/** Set target location for ground-targeted spells */
	UFUNCTION(BlueprintCallable, Category = "AoC|Targeting")
	void SetTargetLocation(FVector NewLocation);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelComponentTickEvent TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// ── Type-specific execution ─────────────────────────────────────────

	void ExecuteProjectileSpell(const FAoCSpellInfo& Info);
	void ExecuteAOESpell(const FAoCSpellInfo& Info);
	void ExecuteInstantSpell(const FAoCSpellInfo& Info);
	void ExecuteBuffSpell(const FAoCSpellInfo& Info);
	void ExecuteBeamSpell(const FAoCSpellInfo& Info);
	void ExecuteShieldSpell(const FAoCSpellInfo& Info);
	void ExecuteChanneledSpell(const FAoCSpellInfo& Info);
	void ExecuteDOTSpell(const FAoCSpellInfo& Info);
	void ExecuteHealSpell(const FAoCSpellInfo& Info);
	void ExecuteSummonSpell(const FAoCSpellInfo& Info);

	/** Apply damage to a target actor (wraps UGameplayStatics::ApplyDamage) */
	void ApplySpellDamage(AActor* Target, float DamageAmount, const FAoCSpellInfo& Info);

	/** Start cooldown for a spell in all matching slots */
	void StartCooldown(FName SpellID, float CooldownDuration);

	/** Tick cooldowns */
	void UpdateCooldowns(float DeltaTime);

	/** Tick mana regen */
	void UpdateManaRegen(float DeltaTime);

	/** Tick active cast */
	void UpdateCasting(float DeltaTime);

	/** Tick active channel */
	void UpdateChanneling(float DeltaTime);

	/** Pending spell to execute when cast completes */
	FAoCSpellInfo PendingSpell;

	/** Channel tick accumulator */
	float ChannelTickAccumulator;

	/** Cached spell info for active channel */
	FAoCSpellInfo ActiveChannelSpell;
};
