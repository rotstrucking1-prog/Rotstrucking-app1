// AoCSpellVFXManager.cpp - AAA Spell VFX with Niagara + Emissive Materials + Dynamic Lights
#include "Spellcraft/AoCSpellVFXManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"

UAoCSpellVFXManager::UAoCSpellVFXManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f; // ~60fps update
}

void UAoCSpellVFXManager::InitSchoolConfigs()
{
	// FIRE - Roaring flames, embers, upward draft
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(1.0f, 0.25f, 0.0f, 1.0f);
		C.LightIntensity = 15000.0f;
		C.LightRadius = 600.0f;
		C.ParticleCount = 25;
		C.ParticleMinSize = 8.0f;
		C.ParticleMaxSize = 30.0f;
		C.ParticleSpeed = 300.0f;
		C.ParticleLifetime = 1.2f;
		C.bLightFlicker = true;
		C.VelocityBias = FVector(0, 0, 250.0f); // Upward drift
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Fire");
		SchoolConfigs.Add(EAoCMagicSchool::Pyromancy, C);
	}

	// ICE - Crystalline shards, cold mist
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.3f, 0.7f, 1.0f, 1.0f);
		C.LightIntensity = 10000.0f;
		C.LightRadius = 500.0f;
		C.ParticleCount = 20;
		C.ParticleMinSize = 5.0f;
		C.ParticleMaxSize = 20.0f;
		C.ParticleSpeed = 400.0f;
		C.ParticleLifetime = 1.0f;
		C.bLightFlicker = false;
		C.VelocityBias = FVector(0, 0, -50.0f); // Slight downward (cold sinks)
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Ice");
		SchoolConfigs.Add(EAoCMagicSchool::Cryomancy, C);
	}

	// LIGHTNING - Electric arcs, bright flash, scatter
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.9f, 0.9f, 1.0f, 1.0f);
		C.LightIntensity = 25000.0f;
		C.LightRadius = 800.0f;
		C.ParticleCount = 30;
		C.ParticleMinSize = 3.0f;
		C.ParticleMaxSize = 15.0f;
		C.ParticleSpeed = 800.0f;
		C.ParticleLifetime = 0.5f;
		C.bLightFlicker = true;
		C.VelocityBias = FVector::ZeroVector;
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Lightning");
		SchoolConfigs.Add(EAoCMagicSchool::Stormcalling, C);
	}

	// ARCANE - Mystical swirls, steady purple glow
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f);
		C.LightIntensity = 12000.0f;
		C.LightRadius = 500.0f;
		C.ParticleCount = 22;
		C.ParticleMinSize = 6.0f;
		C.ParticleMaxSize = 22.0f;
		C.ParticleSpeed = 200.0f;
		C.ParticleLifetime = 1.5f;
		C.bLightFlicker = false;
		C.VelocityBias = FVector(0, 0, 100.0f);
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Arcane");
		SchoolConfigs.Add(EAoCMagicSchool::Arcana, C);
	}

	// SHADOW - Dark wisps, dim pulsing
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.3f, 0.0f, 0.5f, 1.0f);
		C.LightIntensity = 5000.0f;
		C.LightRadius = 400.0f;
		C.ParticleCount = 18;
		C.ParticleMinSize = 8.0f;
		C.ParticleMaxSize = 28.0f;
		C.ParticleSpeed = 150.0f;
		C.ParticleLifetime = 2.0f;
		C.bLightFlicker = true;
		C.VelocityBias = FVector(0, 0, 80.0f);
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Shadow");
		SchoolConfigs.Add(EAoCMagicSchool::Umbramancy, C);
	}

	// NECROMANCY - Sickly green death wisps, ghostly particles rising
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.29f, 0.49f, 0.25f, 1.0f);
		C.LightIntensity = 7000.0f;
		C.LightRadius = 500.0f;
		C.ParticleCount = 22;
		C.ParticleMinSize = 5.0f;
		C.ParticleMaxSize = 24.0f;
		C.ParticleSpeed = 180.0f;
		C.ParticleLifetime = 2.0f;
		C.bLightFlicker = true;
		C.VelocityBias = FVector(0, 0, 120.0f); // Ghost wisps rise upward
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Necro");
		SchoolConfigs.Add(EAoCMagicSchool::Necromancy, C);
	}

	// NATURE - Leafy fountain, earthy green
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.1f, 0.8f, 0.2f, 1.0f);
		C.LightIntensity = 9000.0f;
		C.LightRadius = 500.0f;
		C.ParticleCount = 22;
		C.ParticleMinSize = 5.0f;
		C.ParticleMaxSize = 20.0f;
		C.ParticleSpeed = 250.0f;
		C.ParticleLifetime = 1.5f;
		C.bLightFlicker = false;
		C.VelocityBias = FVector(0, 0, 200.0f);
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Nature");
		SchoolConfigs.Add(EAoCMagicSchool::Verdancy, C);
	}

	// HOLY - Golden radiance, bright divine rays
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(1.0f, 0.9f, 0.5f, 1.0f);
		C.LightIntensity = 20000.0f;
		C.LightRadius = 700.0f;
		C.ParticleCount = 28;
		C.ParticleMinSize = 6.0f;
		C.ParticleMaxSize = 25.0f;
		C.ParticleSpeed = 350.0f;
		C.ParticleLifetime = 1.3f;
		C.bLightFlicker = false;
		C.VelocityBias = FVector(0, 0, 150.0f);
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Holy");
		SchoolConfigs.Add(EAoCMagicSchool::Radiance, C);
	}

	// BLOOD - Crimson droplets, ominous red
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.8f, 0.0f, 0.0f, 1.0f);
		C.LightIntensity = 8000.0f;
		C.LightRadius = 450.0f;
		C.ParticleCount = 18;
		C.ParticleMinSize = 4.0f;
		C.ParticleMaxSize = 16.0f;
		C.ParticleSpeed = 200.0f;
		C.ParticleLifetime = 1.8f;
		C.bLightFlicker = true;
		C.VelocityBias = FVector(0, 0, -80.0f); // Blood drips down
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Blood");
		SchoolConfigs.Add(EAoCMagicSchool::Sangromancy, C);
	}

	// MIND - Psychic waves, indigo pulse
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.5f, 0.2f, 0.8f, 1.0f);
		C.LightIntensity = 10000.0f;
		C.LightRadius = 550.0f;
		C.ParticleCount = 20;
		C.ParticleMinSize = 5.0f;
		C.ParticleMaxSize = 22.0f;
		C.ParticleSpeed = 250.0f;
		C.ParticleLifetime = 1.2f;
		C.bLightFlicker = false;
		C.VelocityBias = FVector(0, 0, 30.0f);
		C.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Mind");
		SchoolConfigs.Add(EAoCMagicSchool::Dominion, C);
	}
}

void UAoCSpellVFXManager::BeginPlay()
{
	Super::BeginPlay();

	InitSchoolConfigs();

	// Load mesh assets
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Load Niagara templates
	ExplosionNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"));
	BurstNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"));
	FountainNiagara = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"));

	// Load all school material instances
	for (auto& Pair : SchoolConfigs)
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *Pair.Value.MaterialPath);
		if (Mat)
		{
			SchoolMaterials.Add(Pair.Key, Mat);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SpellVFX] Initialized - %d school configs, %d materials loaded, Niagara=%s"),
		SchoolConfigs.Num(), SchoolMaterials.Num(),
		ExplosionNiagara ? TEXT("YES") : TEXT("NO"));
}

void UAoCSpellVFXManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateParticles(DeltaTime);
	UpdateLights(DeltaTime);
	UpdateProjectiles(DeltaTime);
}

UAoCSpellVFXManager::FSchoolVFXConfig UAoCSpellVFXManager::GetConfig(EAoCMagicSchool School)
{
	if (FSchoolVFXConfig* Found = SchoolConfigs.Find(School))
	{
		return *Found;
	}
	// Default fallback (arcane)
	FSchoolVFXConfig Default;
	Default.Color = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f);
	Default.LightIntensity = 10000.0f;
	Default.LightRadius = 500.0f;
	Default.ParticleCount = 20;
	Default.ParticleMinSize = 6.0f;
	Default.ParticleMaxSize = 22.0f;
	Default.ParticleSpeed = 250.0f;
	Default.ParticleLifetime = 1.2f;
	Default.bLightFlicker = false;
	Default.VelocityBias = FVector(0, 0, 100.0f);
	Default.MaterialPath = TEXT("/Game/AoC/VFX/Materials/MI_Spell_Arcane");
	return Default;
}

UStaticMesh* UAoCSpellVFXManager::GetMeshForSchool(EAoCMagicSchool School)
{
	// Ice uses cubes for crystalline look, everything else uses spheres
	if (School == EAoCMagicSchool::Cryomancy)
	{
		return CubeMesh ? CubeMesh : SphereMesh;
	}
	return SphereMesh;
}

// ============================================================
// MAIN VFX SPAWN FUNCTIONS
// ============================================================

void UAoCSpellVFXManager::SpawnCastVFX(EAoCMagicSchool School, FVector Location, FRotator Rotation)
{
	FSchoolVFXConfig Config = GetConfig(School);

	// 1. Spawn particle burst at cast origin
	SpawnParticleBurst(School, Location, Config.ParticleCount, 1.0f);

	// 2. Spawn dynamic point light
	SpawnDynamicLight(School, Location, 1.0f, Config.ParticleLifetime + 0.5f);

	// 3. Spawn Niagara accent effect (adds visual complexity)
	if (ExplosionNiagara)
	{
		SpawnNiagaraAccent(Location, ExplosionNiagara);
	}

	UE_LOG(LogTemp, Log, TEXT("[SpellVFX] Cast VFX spawned for school %d at %s - %d particles"),
		(int32)School, *Location.ToString(), Config.ParticleCount);
}

void UAoCSpellVFXManager::SpawnProjectileVFX(EAoCMagicSchool School, FVector Start, FVector End, float Speed)
{
	AActor* Owner = GetOwner();
	if (!Owner || !SphereMesh) return;

	FSchoolVFXConfig Config = GetConfig(School);

	// Create projectile mesh
	FSpellProjectile Proj;
	Proj.StartLocation = Start;
	Proj.TargetLocation = End;
	Proj.Speed = Speed;
	Proj.Progress = 0.0f;
	Proj.School = School;
	Proj.TrailTimer = 0.0f;

	// Main projectile sphere - larger than particles
	Proj.Mesh = NewObject<UStaticMeshComponent>(Owner);
	Proj.Mesh->SetStaticMesh(SphereMesh);
	Proj.Mesh->SetWorldLocation(Start);
	float ProjScale = Config.ParticleMaxSize * 1.5f / 100.0f;
	Proj.Mesh->SetWorldScale3D(FVector(ProjScale));
	Proj.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Proj.Mesh->SetCastShadow(false);
	Proj.Mesh->RegisterComponent();

	// Create dynamic material for projectile
	UMaterialInterface** MatPtr = SchoolMaterials.Find(School);
	if (MatPtr && *MatPtr)
	{
		Proj.DynMaterial = UMaterialInstanceDynamic::Create(*MatPtr, Owner);
		// Boost intensity for projectile (brighter than particles)
		Proj.DynMaterial->SetScalarParameterValue(TEXT("Intensity"), Config.LightIntensity / 100.0f);
		Proj.Mesh->SetMaterial(0, Proj.DynMaterial);
	}

	// Projectile point light
	Proj.Light = NewObject<UPointLightComponent>(Owner);
	Proj.Light->SetWorldLocation(Start);
	Proj.Light->SetLightColor(Config.Color);
	Proj.Light->SetIntensity(Config.LightIntensity * 0.5f);
	Proj.Light->SetAttenuationRadius(Config.LightRadius * 0.6f);
	Proj.Light->SetCastShadows(false);
	Proj.Light->RegisterComponent();

	ActiveProjectiles.Add(Proj);

	// Spawn initial muzzle flash
	SpawnParticleBurst(School, Start, Config.ParticleCount / 3, 0.5f);
	SpawnDynamicLight(School, Start, 0.5f, 0.5f);
}

void UAoCSpellVFXManager::SpawnImpactVFX(EAoCMagicSchool School, FVector Location)
{
	FSchoolVFXConfig Config = GetConfig(School);

	// Bigger burst at impact
	SpawnParticleBurst(School, Location, (int32)(Config.ParticleCount * 1.5f), 1.5f);

	// Brighter light at impact
	SpawnDynamicLight(School, Location, 2.0f, Config.ParticleLifetime + 1.0f);

	// Niagara explosion at impact
	if (ExplosionNiagara)
	{
		SpawnNiagaraAccent(Location, ExplosionNiagara);
	}
	if (BurstNiagara)
	{
		SpawnNiagaraAccent(Location, BurstNiagara);
	}

	// Second wave of particles (delayed feel)
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, School, Location]()
	{
		SpawnParticleBurst(School, Location, 10, 0.8f);
	}, 0.15f, false);
}

// ============================================================
// PARTICLE BURST SYSTEM
// ============================================================

void UAoCSpellVFXManager::SpawnParticleBurst(EAoCMagicSchool School, FVector Location, int32 Count, float SpeedMult)
{
	AActor* Owner = GetOwner();
	if (!Owner || !SphereMesh) return;

	FSchoolVFXConfig Config = GetConfig(School);
	UStaticMesh* Mesh = GetMeshForSchool(School);
	UMaterialInterface** MatPtr = SchoolMaterials.Find(School);

	for (int32 i = 0; i < Count; i++)
	{
		FSpellParticle Particle;

		// Create mesh component
		Particle.Mesh = NewObject<UStaticMeshComponent>(Owner);
		Particle.Mesh->SetStaticMesh(Mesh);
		Particle.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Particle.Mesh->SetCastShadow(false);

		// Random scale within range
		float Scale = FMath::RandRange(Config.ParticleMinSize, Config.ParticleMaxSize) / 100.0f;
		Particle.InitialScale = Scale;
		Particle.Mesh->SetWorldScale3D(FVector(Scale));

		// Random offset from center
		FVector Offset = FMath::VRand() * FMath::RandRange(0.0f, 30.0f);
		Particle.Mesh->SetWorldLocation(Location + Offset);

		// Random velocity - outward burst + school bias
		FVector RandDir = FMath::VRand();
		float Speed = Config.ParticleSpeed * SpeedMult * FMath::RandRange(0.5f, 1.5f);
		Particle.Velocity = RandDir * Speed + Config.VelocityBias;

		// Lifetime with some variance
		Particle.MaxLifetime = Config.ParticleLifetime * FMath::RandRange(0.6f, 1.4f);
		Particle.LifetimeRemaining = Particle.MaxLifetime;

		// Apply material with dynamic instance for per-particle variation
		if (MatPtr && *MatPtr)
		{
			Particle.DynMaterial = UMaterialInstanceDynamic::Create(*MatPtr, Owner);
			// Slight color variation per particle
			float Variation = FMath::RandRange(0.8f, 1.2f);
			FLinearColor ParticleColor = Config.Color * Variation;
			Particle.DynMaterial->SetVectorParameterValue(TEXT("SpellColor"), ParticleColor);
			Particle.Mesh->SetMaterial(0, Particle.DynMaterial);
		}

		Particle.Mesh->RegisterComponent();
		ActiveParticles.Add(Particle);
	}
}

// ============================================================
// DYNAMIC LIGHT SYSTEM
// ============================================================

void UAoCSpellVFXManager::SpawnDynamicLight(EAoCMagicSchool School, FVector Location, float IntensityMult, float Lifetime)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FSchoolVFXConfig Config = GetConfig(School);

	FSpellLight SpellLight;
	SpellLight.Light = NewObject<UPointLightComponent>(Owner);
	SpellLight.Light->SetWorldLocation(Location);
	SpellLight.Light->SetLightColor(Config.Color);
	SpellLight.Light->SetIntensity(Config.LightIntensity * IntensityMult);
	SpellLight.Light->SetAttenuationRadius(Config.LightRadius);
	SpellLight.Light->SetCastShadows(false);
	SpellLight.Light->RegisterComponent();

	SpellLight.InitialIntensity = Config.LightIntensity * IntensityMult;
	SpellLight.MaxLifetime = Lifetime;
	SpellLight.LifetimeRemaining = Lifetime;
	SpellLight.bFlicker = Config.bLightFlicker;

	ActiveLights.Add(SpellLight);
}

// ============================================================
// NIAGARA ACCENT EFFECTS
// ============================================================

void UAoCSpellVFXManager::SpawnNiagaraAccent(FVector Location, UNiagaraSystem* System)
{
	if (!System || !GetWorld()) return;

	UNiagaraComponent* NComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), System, Location, FRotator::ZeroRotator,
		FVector(1.0f), true, true, ENCPoolMethod::None);

	if (NComp)
	{
		NComp->SetAutoDestroy(true);
	}
}

// ============================================================
// UPDATE / ANIMATION LOOPS
// ============================================================

void UAoCSpellVFXManager::UpdateParticles(float DeltaTime)
{
	for (int32 i = ActiveParticles.Num() - 1; i >= 0; i--)
	{
		FSpellParticle& P = ActiveParticles[i];

		if (!P.Mesh || P.LifetimeRemaining <= 0.0f)
		{
			// Cleanup
			if (P.Mesh)
			{
				P.Mesh->DestroyComponent();
			}
			ActiveParticles.RemoveAt(i);
			continue;
		}

		// Move particle
		FVector NewLoc = P.Mesh->GetComponentLocation() + P.Velocity * DeltaTime;
		P.Mesh->SetWorldLocation(NewLoc);

		// Apply drag (particles slow down)
		P.Velocity *= FMath::Max(0.0f, 1.0f - DeltaTime * 2.0f);

		// Scale down as lifetime expires
		float LifeFraction = P.LifetimeRemaining / P.MaxLifetime;
		float CurrentScale = P.InitialScale * LifeFraction;
		P.Mesh->SetWorldScale3D(FVector(FMath::Max(0.01f, CurrentScale)));

		// Fade emissive intensity based on lifetime
		if (P.DynMaterial)
		{
			float IntensityFade = LifeFraction * LifeFraction; // Quadratic fadeout
			float BaseIntensity = 80.0f; // Will come from material instance
			P.DynMaterial->SetScalarParameterValue(TEXT("Intensity"), BaseIntensity * IntensityFade);
		}

		P.LifetimeRemaining -= DeltaTime;
	}
}

void UAoCSpellVFXManager::UpdateLights(float DeltaTime)
{
	for (int32 i = ActiveLights.Num() - 1; i >= 0; i--)
	{
		FSpellLight& L = ActiveLights[i];

		if (!L.Light || L.LifetimeRemaining <= 0.0f)
		{
			if (L.Light)
			{
				L.Light->DestroyComponent();
			}
			ActiveLights.RemoveAt(i);
			continue;
		}

		float LifeFraction = L.LifetimeRemaining / L.MaxLifetime;

		// Smooth intensity fadeout
		float Intensity = L.InitialIntensity * LifeFraction * LifeFraction;

		// Flicker effect for fire/lightning/blood
		if (L.bFlicker)
		{
			float Flicker = FMath::RandRange(0.6f, 1.0f);
			Intensity *= Flicker;
		}

		L.Light->SetIntensity(FMath::Max(0.0f, Intensity));
		L.LifetimeRemaining -= DeltaTime;
	}
}

void UAoCSpellVFXManager::UpdateProjectiles(float DeltaTime)
{
	for (int32 i = ActiveProjectiles.Num() - 1; i >= 0; i--)
	{
		FSpellProjectile& Proj = ActiveProjectiles[i];

		if (!Proj.Mesh)
		{
			if (Proj.Light) Proj.Light->DestroyComponent();
			ActiveProjectiles.RemoveAt(i);
			continue;
		}

		// Move projectile toward target
		FVector Direction = (Proj.TargetLocation - Proj.StartLocation);
		float TotalDist = Direction.Size();
		if (TotalDist < 1.0f)
		{
			// Already at target
			SpawnImpactVFX(Proj.School, Proj.TargetLocation);
			Proj.Mesh->DestroyComponent();
			if (Proj.Light) Proj.Light->DestroyComponent();
			ActiveProjectiles.RemoveAt(i);
			continue;
		}

		Direction.Normalize();
		Proj.Progress += Proj.Speed * DeltaTime;

		if (Proj.Progress >= TotalDist)
		{
			// Reached target - spawn impact
			SpawnImpactVFX(Proj.School, Proj.TargetLocation);
			Proj.Mesh->DestroyComponent();
			if (Proj.Light) Proj.Light->DestroyComponent();
			ActiveProjectiles.RemoveAt(i);
			continue;
		}

		FVector NewPos = Proj.StartLocation + Direction * Proj.Progress;
		Proj.Mesh->SetWorldLocation(NewPos);
		if (Proj.Light) Proj.Light->SetWorldLocation(NewPos);

		// Spawn trail particles every 0.05 seconds
		Proj.TrailTimer += DeltaTime;
		if (Proj.TrailTimer >= 0.05f)
		{
			Proj.TrailTimer = 0.0f;
			// Small trail burst behind projectile
			FVector TrailPos = NewPos - Direction * 20.0f;
			SpawnParticleBurst(Proj.School, TrailPos, 3, 0.3f);
		}

		// Pulsing glow on projectile
		if (Proj.DynMaterial)
		{
			float Pulse = 80.0f + FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f) * 30.0f;
			Proj.DynMaterial->SetScalarParameterValue(TEXT("Intensity"), Pulse);
		}
	}
}
