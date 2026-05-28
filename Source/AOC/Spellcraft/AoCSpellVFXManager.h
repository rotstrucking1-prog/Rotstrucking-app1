// Source/AOC/Spellcraft/AoCSpellVFXManager.h
// Visual effects manager — creates runtime VFX from engine primitives and dynamic materials.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSpellData.h"
#include "AoCSpellVFXManager.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

/**
 * Manages all spell visual effects.
 * Attach to any character that casts spells.
 * Creates VFX at runtime using engine basic shapes and dynamic emissive materials.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSpellVFXManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSpellVFXManager();

	// ── Public VFX spawners ─────────────────────────────────────────────

	/** Glow effect on the caster's hands during casting */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	void SpawnCastEffect(EAoCMagicSchool School);

	/** Attach glowing trail / aura to a projectile actor */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	void SpawnProjectileVFX(EAoCMagicSchool School, AActor* ProjectileActor);

	/** Explosion / impact at a world location */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	void SpawnImpactEffect(EAoCMagicSchool School, FVector Location, FVector Normal);

	/** Persistent AOE ring / dome at location */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	void SpawnAOEEffect(EAoCMagicSchool School, FVector Location, float Radius, float Duration);

	/** Beam between two world points */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	void SpawnBeamEffect(EAoCMagicSchool School, FVector Start, FVector End);

	/** Shield bubble around an actor */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	void SpawnShieldEffect(EAoCMagicSchool School, AActor* TargetActor);

	/** Create a dynamic material instance tinted to the given school */
	UFUNCTION(BlueprintCallable, Category = "AoC|VFX")
	UMaterialInstanceDynamic* CreateSchoolMaterial(EAoCMagicSchool School);

protected:
	virtual void BeginPlay() override;

private:
	// ── Cached engine meshes ────────────────────────────────────────────
	UPROPERTY()
	UStaticMesh* SphereMesh;

	UPROPERTY()
	UStaticMesh* CylinderMesh;

	UPROPERTY()
	UMaterial* BaseMaterial;

	/** Helper: spawn a glowing sphere actor at a location with auto-destroy */
	AActor* SpawnGlowSphere(EAoCMagicSchool School, FVector Location, FVector Scale, float Lifetime);

	/** Helper: spawn a point light with school colour */
	void AttachPointLight(AActor* Target, EAoCMagicSchool School, float Intensity, float Radius);

	/** Load engine assets on first use */
	void EnsureAssetsLoaded();
	bool bAssetsLoaded;
};
