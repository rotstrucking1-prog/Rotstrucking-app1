// Source/AOC/Combat/AoCProjectile.h
// Base projectile actor for all spell projectiles.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Spellcraft/AoCSpellData.h"
#include "AoCProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UAoCSpellVFXManager;

/**
 * Spell projectile — spawned by the spell casting component.
 * Flies toward target, deals damage on impact, auto-destroys after MaxLifetime.
 */
UCLASS()
class AOC_API AAoCProjectile : public AActor
{
	GENERATED_BODY()

public:
	AAoCProjectile();

	// ── Components ──────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* GlowLight;

	// ── Spell data ──────────────────────────────────────────────────────

	/** The full spell info this projectile was spawned from */
	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	FAoCSpellInfo SpellInfo;

	/** Cached damage value */
	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	float Damage;

	/** Who fired this projectile */
	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	AActor* OwnerActor;

	/** Which school — used for VFX colour */
	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	EAoCMagicSchool School;

	/** Auto-destroy timer */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxLifetime;

	// ── Interface ───────────────────────────────────────────────────────

	/** Set up the projectile after spawning */
	UFUNCTION(BlueprintCallable, Category = "AoC|Projectile")
	void InitializeProjectile(const FAoCSpellInfo& InSpellInfo, AActor* Caster);

protected:
	virtual void BeginPlay() override;

private:
	/** Called when the sphere component overlaps another actor */
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
