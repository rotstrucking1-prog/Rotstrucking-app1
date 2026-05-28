// AoCSpellVFXManager.h - AAA Spell Visual Effects using Niagara + Dynamic Materials + Lights
// v30: Runtime Dynamic Material Instances from M_SpellGlow - no pre-created MI assets needed
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
	float Speed = 2000.0f;
	float Progress = 0.0f;
	EAoCMagicSchool School = EAoCMagicSchool::Arcana;

	// Trail spawning
	float TrailTimer = 0.0f;
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
		FLinearColor EmissiveColor; // Separate emissive for HDR bloom
		float LightIntensity;
		float LightRadius;
		int32 ParticleCount;
		float ParticleMinSize;
		float ParticleMaxSize;
		float ParticleSpeed;
		float ParticleLifetime;
		bool bLightFlicker;
		FVector VelocityBias; // Directional bias (e.g. upward for fire)
	};

	TMap<EAoCMagicSchool, FSchoolVFXConfig> SchoolConfigs;
	void InitSchoolConfigs();

	// Master material - used to create all school DMIs at runtime
	UPROPERTY()
	UMaterialInterface* MasterSpellMaterial;

	// Loaded assets
	UPROPERTY()
	UStaticMesh* SphereMesh;

	UPROPERTY()
	UStaticMesh* CubeMesh;

	// School materials - Dynamic Material Instances created at runtime
	UPROPERTY()
	TMap<EAoCMagicSchool, UMaterialInterface*> SchoolMaterials;

	UPROPERTY()
	UNiagaraSystem* ExplosionNiagara;

	UPROPERTY()
	UNiagaraSystem* BurstNiagara;

	UPROPERTY()
	UNiagaraSystem* FountainNiagara;

	// Active VFX elements
	UPROPERTY()
	TArray<FSpellParticle> ActiveParticles;

	UPROPERTY()
	TArray<FSpellLight> ActiveLights;

	UPROPERTY()
	TArray<FSpellProjectile> ActiveProjectiles;

	// Helper functions
	void SpawnParticleBurst(EAoCMagicSchool School, FVector Location, int32 Count, float SpeedMult = 1.0f);
	void SpawnDynamicLight(EAoCMagicSchool School, FVector Location, float IntensityMult = 1.0f, float Lifetime = 1.5f);
	void SpawnNiagaraAccent(FVector Location, UNiagaraSystem* System);
	UStaticMesh* GetMeshForSchool(EAoCMagicSchool School);
	FSchoolVFXConfig GetConfig(EAoCMagicSchool School);

	void UpdateParticles(float DeltaTime);
	void UpdateLights(float DeltaTime);
	void UpdateProjectiles(float DeltaTime);
};
