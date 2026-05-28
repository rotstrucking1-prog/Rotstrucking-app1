// Source/AOC/Combat/AoCProjectile.h
// v29 — AAA spell projectile with proper collision, per-school gravity,
// emissive materials, dynamic point lights, and impact effects.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Spellcraft/AoCSpellData.h"
#include "AoCProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UNiagaraComponent;

/**
 * Spell projectile — spawned by the spell casting component.
 * v29: AAA quality — proper collision, per-school gravity, emissive glow materials,
 * dynamic lights, impact VFX with sound.
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

	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	FAoCSpellInfo SpellInfo;

	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	float Damage;

	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	AActor* OwnerActor;

	UPROPERTY(BlueprintReadOnly, Category = "Spell")
	EAoCMagicSchool School;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxLifetime;

	// ── Interface ───────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "AoC|Projectile")
	void InitializeProjectile(const FAoCSpellInfo& InSpellInfo, AActor* Caster);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	/** Get per-school gravity scale */
	float GetSchoolGravityScale(EAoCMagicSchool InSchool) const;

	/** Pulsing glow timer */
	float GlowPulseTimer;

	/** Dynamic material for pulsing glow */
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;
};
