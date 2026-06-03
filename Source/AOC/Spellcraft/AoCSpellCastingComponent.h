// Source/AOC/Spellcraft/AoCSpellCastingComponent.h
// Spell casting component — manages the spell bar, cooldowns, spell execution,
// two-layer sound system, and screen effects.
// v29 — Full sound + screen effects integration

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSpellData.h"
#include "AoCSpellCastingComponent.generated.h"

class UAoCSpellVFXManager;
class UAoCSpellScreenEffects;
class AAoCProjectile;
class USoundBase;
class UAudioComponent;

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
 * v29: Two-layer sound system + screen effects integration.
 * Layer 1: School cast sound (plays when casting begins)
 * Layer 2: Spell effect sound (plays when spell fires, unique per-spell)
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpellBar")
	TArray<FAoCSpellBarSlot> SpellBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpellBar")
	int32 MaxSpellBarSlots;

	// ── Casting state ───────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	bool bIsCasting;

	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	FName CurrentCastSpellID;

	UPROPERTY(BlueprintReadOnly, Category = "Casting")
	float CastTimeRemaining;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Casting")
	float CurrentMana;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Casting")
	float MaxMana;

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

	// ── VFX + Screen Effects ─────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	UAoCSpellVFXManager* VFXManager;

	UPROPERTY(BlueprintReadOnly, Category = "VFX")
	UAoCSpellScreenEffects* ScreenEffects;

	// ── Sound System ─────────────────────────────────────────────────────

	/** School-specific cast sounds (Layer 1) — loaded at BeginPlay */
	UPROPERTY()
	TMap<EAoCMagicSchool, USoundBase*> SchoolCastSounds;

	/** Currently playing cast sound */
	UPROPERTY()
	UAudioComponent* ActiveCastSound;

	// ── Public interface ────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "AoC|SpellBar")
	void AssignSpellToSlot(int32 SlotIndex, FName SpellID);

	UFUNCTION(BlueprintCallable, Category = "AoC|SpellBar")
	void CastSpellInSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "AoC|Casting")
	void ExecuteSpell(FName SpellID);

	UFUNCTION(BlueprintCallable, Category = "AoC|Casting")
	void CancelCast();

	UFUNCTION(BlueprintCallable, Category = "AoC|Casting")
	bool CanCastSpell(FName SpellID) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|SpellBar")
	float GetCooldownFraction(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Targeting")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "AoC|Targeting")
	void SetTargetLocation(FVector NewLocation);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
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

	/** Apply damage to a target actor */
	void ApplySpellDamage(AActor* Target, float DamageAmount, const FAoCSpellInfo& Info);

	void StartCooldown(FName SpellID, float CooldownDuration);
	void UpdateCooldowns(float DeltaTime);
	void UpdateManaRegen(float DeltaTime);
	void UpdateCasting(float DeltaTime);
	void UpdateChanneling(float DeltaTime);

	// ── Sound helpers ───────────────────────────────────────────────────

	/** Play the school's cast sound (Layer 1) */
	void PlayCastSound(EAoCMagicSchool School);

	/** Play the spell's unique effect sound (Layer 2) with tier-based pitch */
	void PlaySpellEffectSound(const FAoCSpellInfo& Info);

	/** Get pitch multiplier based on spell tier */
	float GetTierPitchMultiplier(int32 SchoolLevel) const;

	/** Load all school cast sounds from /Game/AoC/Sounds/ */
	void LoadSchoolCastSounds();

	/** Fire screen effects for the school */
	void TriggerScreenEffects(EAoCMagicSchool School, int32 Tier);

	FAoCSpellInfo PendingSpell;
	float ChannelTickAccumulator;
	FAoCSpellInfo ActiveChannelSpell;
};
