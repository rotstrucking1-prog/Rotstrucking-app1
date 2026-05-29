// AoCSpellVFXManager.h - v31: Directional combat VFX
// Spells charge at hand → fly FORWARD at target → explode on impact
// Each school has unique visual identity
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSpellData.h"
#include "AoCSpellVFXManager.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UPointLightComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Tracks a single VFX particle for animation
USTRUCT()
struct FSpellParticle
{
	GENERATED_BODY()

	UPROPERTY()
	UStaticMeshComponent* Mesh = nullptr;

	FVector Velocity = FVector::ZeroVector;
	float LifetimeRemaining = 1.0f;
	float MaxLifetime = 1.0f;
	float InitialScale = 1.0f;
	float GravityScale = 0.0f;
	float DragCoeff = 0.0f;

	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial = nullptr;
};

// Tracks a dynamic light for fade-out
USTRUCT()
struct FSpellLight
{
	GENERATED_BODY()

	UPROPERTY()
	UPointLightComponent* Light = nullptr;

	float LifetimeRemaining = 1.0f;
	float MaxLifetime = 1.0f;
	float InitialIntensity = 10000.0f;
	bool bFlicker = false;
};

// Tracks a projectile in flight
USTRUCT()
struct FSpellProjectile
{
	GENERATED_BODY()

	UPROPERTY()
	UStaticMeshComponent* Mesh = nullptr;

	UPROPERTY()
	UPointLightComponent* Light = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial = nullptr;

	FVector StartLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	float Speed = 2000.0f;
	float Progress = 0.0f;
	float MaxDistance = 0.0f;
	EAoCMagicSchool School = EAoCMagicSchool::Arcana;
	float ProjectileScale = 1.0f;

	// Trail spawning
	float TrailTimer = 0.0f;
	float TrailInterval = 0.05f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSpellVFXManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSpellVFXManager();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Main VFX entry points
	UFUNCTION(BlueprintCallable)
	void SpawnCastVFX(EAoCMagicSchool School, FVector Location, FRotator Rotation);

	UFUNCTION(BlueprintCallable)
	void SpawnProjectileVFX(EAoCMagicSchool School, FVector Start, FVector End, float Speed = 2000.0f);

	UFUNCTION(BlueprintCallable)
	void SpawnImpactVFX(EAoCMagicSchool School, FVector Location);

private:
	// Per-school visual config
	struct FSchoolVFXConfig
	{
		FLinearColor Color;
		FLinearColor EmissiveColor;
		float LightIntensity;
		float LightRadius;

		// Cast (hand charge) config
		int32 CastParticleCount;
		float CastParticleSize;
		float CastGatherRadius;   // How far particles start from hand
		float CastGatherSpeed;    // How fast they swirl inward
		float CastDuration;

		// Projectile config
		float ProjectileSize;
		float ProjectileSpeed;
		int32 TrailParticleCount;
		float TrailParticleSize;
		float TrailSpread;        // How wide the trail spreads

		// Impact config
		int32 ImpactParticleCount;
		float ImpactParticleSize;
		float ImpactSpread;
		float ImpactSpeed;
		float ImpactLifetime;

		bool bLightFlicker;
	};

	TMap<EAoCMagicSchool, FSchoolVFXConfig> SchoolConfigs;
	void InitSchoolConfigs();

	// Master material for DMIs
	UPROPERTY()
	UMaterialInterface* MasterSpellMaterial;

	// Loaded meshes
	UPROPERTY()
	UStaticMesh* SphereMesh;

	UPROPERTY()
	UStaticMesh* CubeMesh;

	// School DMIs created at runtime
	UPROPERTY()
	TMap<EAoCMagicSchool, UMaterialInstanceDynamic*> SchoolDMIs;

	// Active VFX elements
	UPROPERTY()
	TArray<FSpellParticle> ActiveParticles;

	UPROPERTY()
	TArray<FSpellLight> ActiveLights;

	UPROPERTY()
	TArray<FSpellProjectile> ActiveProjectiles;

	// Per-school cast patterns
	void SpawnCast_Fire(FVector Location, FRotator Rotation);
	void SpawnCast_Ice(FVector Location, FRotator Rotation);
	void SpawnCast_Lightning(FVector Location, FRotator Rotation);
	void SpawnCast_Arcane(FVector Location, FRotator Rotation);
	void SpawnCast_Shadow(FVector Location, FRotator Rotation);
	void SpawnCast_Necro(FVector Location, FRotator Rotation);
	void SpawnCast_Nature(FVector Location, FRotator Rotation);
	void SpawnCast_Holy(FVector Location, FRotator Rotation);
	void SpawnCast_Blood(FVector Location, FRotator Rotation);
	void SpawnCast_Mind(FVector Location, FRotator Rotation);

	// Per-school impact patterns
	void SpawnImpact_Fire(FVector Location);
	void SpawnImpact_Ice(FVector Location);
	void SpawnImpact_Lightning(FVector Location);
	void SpawnImpact_Default(EAoCMagicSchool School, FVector Location);

	// Core helpers
	UMaterialInstanceDynamic* GetOrCreateDMI(EAoCMagicSchool School);
	FSpellParticle& SpawnSingleParticle(FVector Location, FVector Velocity, float Size, float Lifetime, EAoCMagicSchool School, float Gravity = 0.0f, float Drag = 0.0f);
	void SpawnTrailParticle(EAoCMagicSchool School, FVector Location, FVector ProjectileDir);
	void SpawnDynamicLight(EAoCMagicSchool School, FVector Location, float IntensityMult = 1.0f, float Lifetime = 1.5f);
	FSchoolVFXConfig GetConfig(EAoCMagicSchool School);

	void UpdateParticles(float DeltaTime);
	void UpdateLights(float DeltaTime);
	void UpdateProjectiles(float DeltaTime);
};
