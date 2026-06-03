// Source/AOC/Combat/AoCProjectile.cpp
// v29 — AAA projectile with M_SpellGlow material, per-school gravity,
// proper collision, pulsing emissive glow, dynamic lights, impact effects.

#include "AoCProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DamageEvents.h"

// ─── Constructor ────────────────────────────────────────────────────────────

AAoCProjectile::AAoCProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	MaxLifetime = 5.0f;
	Damage = 0.f;
	OwnerActor = nullptr;
	School = EAoCMagicSchool::Arcana;
	GlowPulseTimer = 0.f;
	DynamicMaterial = nullptr;

	// Collision sphere (root)
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(20.f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	// Block world static for wall/terrain collision
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SetRootComponent(CollisionSphere);

	// Visual mesh
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetWorldScale3D(FVector(0.3f));
	ProjectileMesh->SetCastShadow(false);
	ProjectileMesh->SetupAttachment(CollisionSphere);

	// Load default sphere mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereAsset.Object);
	}

	// Glow light — dynamic per-school color
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetIntensity(8000.f);
	GlowLight->SetAttenuationRadius(300.f);
	GlowLight->SetCastShadows(false);
	GlowLight->SetupAttachment(CollisionSphere);

	// Projectile movement
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 8000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f; // Default: no gravity (overridden per-school)
	ProjectileMovement->bIsHomingProjectile = false;
}

// ─── BeginPlay ──────────────────────────────────────────────────────────────

void AAoCProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(MaxLifetime);

	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AAoCProjectile::OnProjectileHit);
}

// ─── Per-school gravity ─────────────────────────────────────────────────────

float AAoCProjectile::GetSchoolGravityScale(EAoCMagicSchool InSchool) const
{
	switch (InSchool)
	{
	case EAoCMagicSchool::Pyromancy:    return 0.05f;  // Fire floats slightly upward
	case EAoCMagicSchool::Cryomancy:    return 0.02f;  // Ice is nearly weightless
	case EAoCMagicSchool::Stormcalling: return 0.0f;   // Lightning is instant
	case EAoCMagicSchool::Necromancy:   return 0.08f;  // Death energy sinks slightly
	case EAoCMagicSchool::Verdancy:     return 0.1f;   // Nature has slight arc
	case EAoCMagicSchool::Umbramancy:   return 0.0f;   // Shadow ignores physics
	case EAoCMagicSchool::Radiance:     return 0.0f;   // Holy light flies straight
	case EAoCMagicSchool::Sangromancy:  return 0.12f;  // Blood has weight
	case EAoCMagicSchool::Dominion:     return 0.0f;   // Psychic force is direct
	case EAoCMagicSchool::Arcana:       return 0.03f;  // Arcane has slight drift
	default: return 0.0f;
	}
}

// ─── Initialization ─────────────────────────────────────────────────────────

void AAoCProjectile::InitializeProjectile(const FAoCSpellInfo& InSpellInfo, AActor* Caster)
{
	SpellInfo = InSpellInfo;
	Damage = InSpellInfo.BaseDamage;
	School = InSpellInfo.School;
	OwnerActor = Caster;

	if (Caster)
	{
		CollisionSphere->MoveIgnoreActors.Add(Caster);
	}

	// Set projectile speed + per-school gravity
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpellInfo.ProjectileSpeed;
		ProjectileMovement->MaxSpeed = InSpellInfo.ProjectileSpeed * 1.5f;
		ProjectileMovement->Velocity = GetActorForwardVector() * InSpellInfo.ProjectileSpeed;
		ProjectileMovement->ProjectileGravityScale = GetSchoolGravityScale(School);
	}

	// Get school visuals
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	// ★ Use M_SpellGlow material instead of BasicShapeMaterial (fixes grey ball bug)
	// Try school-specific material first, fallback to M_SpellGlow
	FString SchoolMatPath;
	switch (School)
	{
	case EAoCMagicSchool::Arcana:       SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Arcana"); break;
	case EAoCMagicSchool::Pyromancy:    SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Pyromancy"); break;
	case EAoCMagicSchool::Cryomancy:    SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Cryomancy"); break;
	case EAoCMagicSchool::Stormcalling: SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Stormcalling"); break;
	case EAoCMagicSchool::Necromancy:   SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Necromancy"); break;
	case EAoCMagicSchool::Verdancy:     SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Verdancy"); break;
	case EAoCMagicSchool::Umbramancy:   SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Umbramancy"); break;
	case EAoCMagicSchool::Radiance:     SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Radiance"); break;
	case EAoCMagicSchool::Sangromancy:  SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Sangromancy"); break;
	case EAoCMagicSchool::Dominion:     SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Dominion"); break;
	default: SchoolMatPath = TEXT("/Game/AoC/VFX/Materials/MI_Arcana"); break;
	}

	// Try loading school material instance
	UMaterialInterface* SchoolMat = LoadObject<UMaterialInterface>(nullptr, *SchoolMatPath);
	if (!SchoolMat)
	{
		// Fallback to M_SpellGlow master material
		SchoolMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AoC/VFX/Materials/M_SpellGlow"));
	}

	if (SchoolMat && ProjectileMesh)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(SchoolMat, this);
		if (DynamicMaterial)
		{
			// Set school-specific emissive color
			DynamicMaterial->SetVectorParameterValue(TEXT("SpellColor"), Vis.PrimaryColor);
			DynamicMaterial->SetScalarParameterValue(TEXT("Intensity"), Vis.GlowIntensity * 20.f);
			ProjectileMesh->SetMaterial(0, DynamicMaterial);
		}
	}

	// Set light colour + intensity per school
	if (GlowLight)
	{
		GlowLight->SetLightColor(Vis.PrimaryColor);
		GlowLight->SetIntensity(Vis.GlowIntensity * 2000.f);
		GlowLight->SetAttenuationRadius(Vis.GlowIntensity * 80.f);
	}

	// Scale mesh based on school visuals
	if (ProjectileMesh)
	{
		ProjectileMesh->SetWorldScale3D(Vis.ProjectileMeshScale);
	}

	UE_LOG(LogTemp, Log, TEXT("[Projectile] Initialized %s — speed=%.0f, gravity=%.2f, school=%s"),
		*InSpellInfo.DisplayName, InSpellInfo.ProjectileSpeed,
		GetSchoolGravityScale(School), *UAoCSpellDatabase::GetSchoolName(School));
}

// ─── Tick — Pulsing glow animation ─────────────────────────────────────────

void AAoCProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	GlowPulseTimer += DeltaTime;

	// Pulsing emissive glow
	if (DynamicMaterial)
	{
		FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);
		float BaseIntensity = Vis.GlowIntensity * 20.f;
		float Pulse = BaseIntensity + FMath::Sin(GlowPulseTimer * 8.f) * (BaseIntensity * 0.4f);
		DynamicMaterial->SetScalarParameterValue(TEXT("Intensity"), Pulse);
	}

	// Pulsing light
	if (GlowLight)
	{
		FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);
		float BaseLight = Vis.GlowIntensity * 2000.f;
		float LightPulse = BaseLight + FMath::Sin(GlowPulseTimer * 6.f) * (BaseLight * 0.3f);
		GlowLight->SetIntensity(LightPulse);
	}
}

// ─── Hit handling ───────────────────────────────────────────────────────────

void AAoCProjectile::OnProjectileHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == this || OtherActor == OwnerActor) return;

	// Apply damage
	if (OtherActor && Damage > 0.f)
	{
		UGameplayStatics::ApplyDamage(
			OtherActor,
			Damage,
			OwnerActor ? OwnerActor->GetInstigatorController() : nullptr,
			this,
			nullptr
		);

		UE_LOG(LogTemp, Log, TEXT("AoCProjectile [%s] hit %s for %.1f damage"),
			*SpellInfo.DisplayName, *OtherActor->GetName(), Damage);
	}

	// Impact location
	FVector ImpactLoc = GetActorLocation();
	if (!SweepResult.ImpactPoint.IsZero())
	{
		ImpactLoc = FVector(SweepResult.ImpactPoint.X, SweepResult.ImpactPoint.Y, SweepResult.ImpactPoint.Z);
	}

	// ★ AAA Impact VFX — bright flash + expanding light
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);
	UWorld* World = GetWorld();
	if (World)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Flash = World->SpawnActor<AActor>(AActor::StaticClass(), ImpactLoc, FRotator::ZeroRotator, SpawnParams);
		if (Flash)
		{
			// Root component
			USceneComponent* FlashRoot = NewObject<USceneComponent>(Flash, TEXT("Root"));
			Flash->SetRootComponent(FlashRoot);
			FlashRoot->RegisterComponent();

			// Impact light — bright flash
			UPointLightComponent* FlashLight = NewObject<UPointLightComponent>(Flash, TEXT("FlashLight"));
			FlashLight->SetLightColor(Vis.PrimaryColor);
			FlashLight->SetIntensity(Vis.GlowIntensity * 8000.f);
			FlashLight->SetAttenuationRadius(600.f);
			FlashLight->SetCastShadows(false);
			FlashLight->AttachToComponent(FlashRoot, FAttachmentTransformRules::SnapToTargetIncludingScale);
			FlashLight->RegisterComponent();

			// Impact glow sphere (expanding)
			UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			if (SphereMesh)
			{
				UStaticMeshComponent* ImpactSphere = NewObject<UStaticMeshComponent>(Flash, TEXT("ImpactSphere"));
				ImpactSphere->SetStaticMesh(SphereMesh);
				ImpactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				ImpactSphere->SetCastShadow(false);
				ImpactSphere->SetWorldScale3D(FVector(0.5f));

				// Use school material for impact sphere
				if (DynamicMaterial)
				{
					UMaterialInstanceDynamic* ImpactMat = UMaterialInstanceDynamic::Create(
						DynamicMaterial->GetMaterial(), Flash);
					if (ImpactMat)
					{
						ImpactMat->SetVectorParameterValue(TEXT("SpellColor"), Vis.PrimaryColor);
						ImpactMat->SetScalarParameterValue(TEXT("Intensity"), Vis.GlowIntensity * 50.f);
						ImpactSphere->SetMaterial(0, ImpactMat);
					}
				}

				ImpactSphere->AttachToComponent(FlashRoot, FAttachmentTransformRules::SnapToTargetIncludingScale);
				ImpactSphere->RegisterComponent();
			}

			Flash->SetLifeSpan(0.4f);
		}

		// Play impact sound at location
		// Sound path: /Game/AoC/Sounds/Effects/impact_{school}
		FString ImpactSoundPath = FString::Printf(TEXT("/Game/AoC/Sounds/Effects/impact_%s.impact_%s"),
			*UAoCSpellDatabase::GetSchoolName(School).ToLower(),
			*UAoCSpellDatabase::GetSchoolName(School).ToLower());
		USoundBase* ImpactSound = LoadObject<USoundBase>(nullptr, *ImpactSoundPath);
		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, ImpactLoc, 1.0f, 1.0f);
		}
	}

	Destroy();
}
