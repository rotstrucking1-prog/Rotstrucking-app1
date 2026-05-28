// Source/AOC/Combat/AoCProjectile.cpp

#include "AoCProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DamageEvents.h"

// ─── Constructor ────────────────────────────────────────────────────────────

AAoCProjectile::AAoCProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	MaxLifetime = 5.0f;
	Damage = 0.f;
	OwnerActor = nullptr;
	School = EAoCMagicSchool::Arcana;

	// Collision sphere (root)
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(16.f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionSphere);

	// Visual mesh
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetWorldScale3D(FVector(0.25f));
	ProjectileMesh->SetupAttachment(CollisionSphere);

	// Load default sphere mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		ProjectileMesh->SetStaticMesh(SphereAsset.Object);
	}

	// Glow light
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetIntensity(5000.f);
	GlowLight->SetAttenuationRadius(200.f);
	GlowLight->SetupAttachment(CollisionSphere);

	// Projectile movement
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 6000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f; // magic projectiles ignore gravity
}

// ─── BeginPlay ──────────────────────────────────────────────────────────────

void AAoCProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(MaxLifetime);

	// Bind overlap
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AAoCProjectile::OnProjectileHit);
}

// ─── Initialization ─────────────────────────────────────────────────────────

void AAoCProjectile::InitializeProjectile(const FAoCSpellInfo& InSpellInfo, AActor* Caster)
{
	SpellInfo = InSpellInfo;
	Damage = InSpellInfo.BaseDamage;
	School = InSpellInfo.School;
	OwnerActor = Caster;

	// Ignore the caster
	if (Caster)
	{
		CollisionSphere->MoveIgnoreActors.Add(Caster);
	}

	// Set projectile speed
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpellInfo.ProjectileSpeed;
		ProjectileMovement->MaxSpeed = InSpellInfo.ProjectileSpeed * 1.5f;
		ProjectileMovement->Velocity = GetActorForwardVector() * InSpellInfo.ProjectileSpeed;
	}

	// Create school-coloured material
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	UMaterial* BaseMat = Cast<UMaterial>(
		StaticLoadObject(UMaterial::StaticClass(), nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));

	if (BaseMat && ProjectileMesh)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (MID)
		{
			MID->SetVectorParameterValue(TEXT("BaseColor"), Vis.PrimaryColor);
			FLinearColor Emissive = Vis.SecondaryColor * Vis.GlowIntensity;
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
			ProjectileMesh->SetMaterial(0, MID);
		}
	}

	// Set light colour
	if (GlowLight)
	{
		GlowLight->SetLightColor(Vis.PrimaryColor);
		GlowLight->SetIntensity(Vis.GlowIntensity * 2000.f);
	}

	// Scale mesh based on school visuals
	if (ProjectileMesh)
	{
		ProjectileMesh->SetWorldScale3D(Vis.ProjectileMeshScale);
	}
}

// ─── Hit handling ───────────────────────────────────────────────────────────

void AAoCProjectile::OnProjectileHit(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// Don't hit self or owner
	if (OtherActor == this || OtherActor == OwnerActor)
	{
		return;
	}

	// Apply damage using UGameplayStatics (NO GAS)
	if (OtherActor && Damage > 0.f)
	{
		UGameplayStatics::ApplyDamage(
			OtherActor,
			Damage,
			OwnerActor ? OwnerActor->GetInstigatorController() : nullptr,
			this,
			nullptr // DamageType — use default
		);

		UE_LOG(LogTemp, Log, TEXT("AoCProjectile [%s] hit %s for %.1f damage"),
			*SpellInfo.DisplayName, *OtherActor->GetName(), Damage);
	}

	// Spawn impact VFX (small flash at hit point)
	FVector ImpactLoc = GetActorLocation();
	if (!SweepResult.ImpactPoint.IsZero())
	{
		ImpactLoc = FVector(SweepResult.ImpactPoint.X, SweepResult.ImpactPoint.Y, SweepResult.ImpactPoint.Z);
	}
	FVector ImpactNormal = -GetActorForwardVector();
	if (!SweepResult.ImpactNormal.IsZero())
	{
		ImpactNormal = FVector(SweepResult.ImpactNormal.X, SweepResult.ImpactNormal.Y, SweepResult.ImpactNormal.Z);
	}

	// Impact flash — spawn a quick glow sphere
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);
	UWorld* World = GetWorld();
	if (World)
	{
		// Simple impact light flash
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Flash = World->SpawnActor<AActor>(AActor::StaticClass(), ImpactLoc, FRotator::ZeroRotator, SpawnParams);
		if (Flash)
		{
			USceneComponent* FlashRoot = NewObject<USceneComponent>(Flash, TEXT("Root"));
			Flash->SetRootComponent(FlashRoot);
			FlashRoot->RegisterComponent();

			UPointLightComponent* FlashLight = NewObject<UPointLightComponent>(Flash, TEXT("FlashLight"));
			FlashLight->SetLightColor(Vis.PrimaryColor);
			FlashLight->SetIntensity(Vis.GlowIntensity * 5000.f);
			FlashLight->SetAttenuationRadius(400.f);
			FlashLight->AttachToComponent(FlashRoot, FAttachmentTransformRules::SnapToTargetIncludingScale);
			FlashLight->RegisterComponent();

			Flash->SetLifeSpan(0.3f);
		}
	}

	Destroy();
}
