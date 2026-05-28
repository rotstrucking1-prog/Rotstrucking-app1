// Source/AOC/Spellcraft/AoCSpellVFXManager.cpp

#include "AoCSpellVFXManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

// ─── Constructor ────────────────────────────────────────────────────────────

UAoCSpellVFXManager::UAoCSpellVFXManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAssetsLoaded = false;
	SphereMesh = nullptr;
	CylinderMesh = nullptr;
	BaseMaterial = nullptr;
}

void UAoCSpellVFXManager::BeginPlay()
{
	Super::BeginPlay();
	EnsureAssetsLoaded();
}

// ─── Asset loading ──────────────────────────────────────────────────────────

void UAoCSpellVFXManager::EnsureAssetsLoaded()
{
	if (bAssetsLoaded) return;

	SphereMesh = Cast<UStaticMesh>(
		StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
			TEXT("/Engine/BasicShapes/Sphere.Sphere")));

	CylinderMesh = Cast<UStaticMesh>(
		StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
			TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));

	// Use the default lit material as base for dynamic instances
	BaseMaterial = Cast<UMaterial>(
		StaticLoadObject(UMaterial::StaticClass(), nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));

	bAssetsLoaded = true;
}

// ─── Dynamic material ───────────────────────────────────────────────────────

UMaterialInstanceDynamic* UAoCSpellVFXManager::CreateSchoolMaterial(EAoCMagicSchool School)
{
	EnsureAssetsLoaded();
	if (!BaseMaterial) return nullptr;

	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (MID)
	{
		// Set base colour to primary, emissive to secondary * glow
		MID->SetVectorParameterValue(TEXT("BaseColor"),
			FLinearColor(Vis.PrimaryColor.R, Vis.PrimaryColor.G, Vis.PrimaryColor.B, 1.f));
		FLinearColor Emissive = Vis.SecondaryColor * Vis.GlowIntensity;
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
	}
	return MID;
}

// ─── Helpers ────────────────────────────────────────────────────────────────

AActor* UAoCSpellVFXManager::SpawnGlowSphere(EAoCMagicSchool School, FVector Location,
                                              FVector Scale, float Lifetime)
{
	EnsureAssetsLoaded();
	UWorld* World = GetWorld();
	if (!World || !SphereMesh) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SphereActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!SphereActor) return nullptr;

	// Root component
	USceneComponent* Root = NewObject<USceneComponent>(SphereActor, TEXT("Root"));
	SphereActor->SetRootComponent(Root);
	Root->RegisterComponent();

	// Mesh
	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(SphereActor, TEXT("SphereMesh"));
	MeshComp->SetStaticMesh(SphereMesh);
	MeshComp->SetWorldScale3D(Scale);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetIncludingScale);
	MeshComp->RegisterComponent();

	// Apply school material
	UMaterialInstanceDynamic* MID = CreateSchoolMaterial(School);
	if (MID)
	{
		MeshComp->SetMaterial(0, MID);
	}

	// Point light for glow
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);
	UPointLightComponent* Light = NewObject<UPointLightComponent>(SphereActor, TEXT("GlowLight"));
	Light->SetLightColor(Vis.PrimaryColor);
	Light->SetIntensity(Vis.GlowIntensity * 2000.f);
	Light->SetAttenuationRadius(Scale.X * 500.f);
	Light->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetIncludingScale);
	Light->RegisterComponent();

	// Auto destroy
	SphereActor->SetLifeSpan(Lifetime);

	return SphereActor;
}

void UAoCSpellVFXManager::AttachPointLight(AActor* Target, EAoCMagicSchool School,
                                            float Intensity, float AttenuationRadius)
{
	if (!Target) return;

	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	UPointLightComponent* Light = NewObject<UPointLightComponent>(Target, TEXT("SchoolLight"));
	Light->SetLightColor(Vis.PrimaryColor);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(AttenuationRadius);
	Light->AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	Light->RegisterComponent();
}

// ─── Cast effect (hand glow) ────────────────────────────────────────────────

void UAoCSpellVFXManager::SpawnCastEffect(EAoCMagicSchool School)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Spawn a small glowing orb near the owner's location (offset upward for hand area)
	FVector SpawnLoc = Owner->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	SpawnGlowSphere(School, SpawnLoc, FVector(0.15f), 2.0f);
}

// ─── Projectile VFX ─────────────────────────────────────────────────────────

void UAoCSpellVFXManager::SpawnProjectileVFX(EAoCMagicSchool School, AActor* ProjectileActor)
{
	if (!ProjectileActor) return;

	EnsureAssetsLoaded();

	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	// Attach a glowing sphere mesh to the projectile
	if (SphereMesh)
	{
		UStaticMeshComponent* TrailSphere = NewObject<UStaticMeshComponent>(ProjectileActor, TEXT("TrailSphere"));
		TrailSphere->SetStaticMesh(SphereMesh);
		TrailSphere->SetWorldScale3D(Vis.ProjectileMeshScale * 0.5f);
		TrailSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UMaterialInstanceDynamic* MID = CreateSchoolMaterial(School);
		if (MID)
		{
			TrailSphere->SetMaterial(0, MID);
		}

		TrailSphere->AttachToComponent(ProjectileActor->GetRootComponent(),
			FAttachmentTransformRules::SnapToTargetIncludingScale);
		TrailSphere->RegisterComponent();
	}

	// Attach a point light
	AttachPointLight(ProjectileActor, School, Vis.GlowIntensity * 1500.f, 300.f);
}

// ─── Impact effect ──────────────────────────────────────────────────────────

void UAoCSpellVFXManager::SpawnImpactEffect(EAoCMagicSchool School, FVector Location, FVector Normal)
{
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	// Large flash sphere
	SpawnGlowSphere(School, Location, FVector(0.8f) * Vis.ParticleScale, 0.5f);

	// Smaller debris spheres
	UWorld* World = GetWorld();
	if (World)
	{
		for (int32 i = 0; i < 5; ++i)
		{
			FVector Offset = FMath::VRand() * FMath::RandRange(50.f, 150.f);
			SpawnGlowSphere(School, Location + Offset, FVector(0.1f + FMath::FRandRange(0.f, 0.15f)), 0.8f);
		}
	}
}

// ─── AOE effect ─────────────────────────────────────────────────────────────

void UAoCSpellVFXManager::SpawnAOEEffect(EAoCMagicSchool School, FVector Location,
                                          float Radius, float Duration)
{
	EnsureAssetsLoaded();
	UWorld* World = GetWorld();
	if (!World || !CylinderMesh) return;

	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	// Flat cylinder as AOE ring
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* AOEActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!AOEActor) return;

	USceneComponent* Root = NewObject<USceneComponent>(AOEActor, TEXT("Root"));
	AOEActor->SetRootComponent(Root);
	Root->RegisterComponent();

	UStaticMeshComponent* RingMesh = NewObject<UStaticMeshComponent>(AOEActor, TEXT("AOERing"));
	RingMesh->SetStaticMesh(CylinderMesh);
	// Scale: X/Y = radius, Z = very thin
	float MeshScale = Radius / 50.f; // engine cylinder is ~100 units diameter
	RingMesh->SetWorldScale3D(FVector(MeshScale, MeshScale, 0.05f));
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* MID = CreateSchoolMaterial(School);
	if (MID)
	{
		// Make semi-transparent
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
		RingMesh->SetMaterial(0, MID);
	}

	RingMesh->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetIncludingScale);
	RingMesh->RegisterComponent();

	// Central glow
	UPointLightComponent* Light = NewObject<UPointLightComponent>(AOEActor, TEXT("AOELight"));
	Light->SetLightColor(Vis.PrimaryColor);
	Light->SetIntensity(Vis.GlowIntensity * 3000.f);
	Light->SetAttenuationRadius(Radius * 1.5f);
	Light->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetIncludingScale);
	Light->RegisterComponent();

	AOEActor->SetLifeSpan(Duration > 0.f ? Duration : 3.f);
}

// ─── Beam effect ────────────────────────────────────────────────────────────

void UAoCSpellVFXManager::SpawnBeamEffect(EAoCMagicSchool School, FVector Start, FVector End)
{
	EnsureAssetsLoaded();
	UWorld* World = GetWorld();
	if (!World || !CylinderMesh) return;

	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	FVector MidPoint = (Start + End) * 0.5f;
	FVector Dir = End - Start;
	float Length = Dir.Size();
	FRotator Rot = Dir.Rotation();
	// Cylinder is Z-up, so rotate to align along beam direction
	Rot.Pitch += 90.f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* BeamActor = World->SpawnActor<AActor>(AActor::StaticClass(), MidPoint, Rot, Params);
	if (!BeamActor) return;

	USceneComponent* Root = NewObject<USceneComponent>(BeamActor, TEXT("Root"));
	BeamActor->SetRootComponent(Root);
	Root->RegisterComponent();

	UStaticMeshComponent* BeamMesh = NewObject<UStaticMeshComponent>(BeamActor, TEXT("BeamMesh"));
	BeamMesh->SetStaticMesh(CylinderMesh);
	// Thin cylinder stretched along length
	float LenScale = Length / 100.f; // cylinder is ~100 units tall
	BeamMesh->SetWorldScale3D(FVector(0.05f, 0.05f, LenScale));
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* MID = CreateSchoolMaterial(School);
	if (MID)
	{
		BeamMesh->SetMaterial(0, MID);
	}
	BeamMesh->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetIncludingScale);
	BeamMesh->RegisterComponent();

	// Light along beam
	UPointLightComponent* Light = NewObject<UPointLightComponent>(BeamActor, TEXT("BeamLight"));
	Light->SetLightColor(Vis.PrimaryColor);
	Light->SetIntensity(Vis.GlowIntensity * 2000.f);
	Light->SetAttenuationRadius(200.f);
	Light->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetIncludingScale);
	Light->RegisterComponent();

	BeamActor->SetLifeSpan(0.1f); // very short — caller refreshes every tick
}

// ─── Shield effect ──────────────────────────────────────────────────────────

void UAoCSpellVFXManager::SpawnShieldEffect(EAoCMagicSchool School, AActor* TargetActor)
{
	EnsureAssetsLoaded();
	if (!TargetActor || !SphereMesh) return;

	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(School);

	UStaticMeshComponent* ShieldMesh = NewObject<UStaticMeshComponent>(TargetActor, TEXT("ShieldBubble"));
	ShieldMesh->SetStaticMesh(SphereMesh);
	ShieldMesh->SetWorldScale3D(FVector(2.5f)); // large bubble around character
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* MID = CreateSchoolMaterial(School);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.25f);
		ShieldMesh->SetMaterial(0, MID);
	}

	ShieldMesh->AttachToComponent(TargetActor->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetIncludingScale);
	ShieldMesh->RegisterComponent();

	// Inner glow
	UPointLightComponent* Light = NewObject<UPointLightComponent>(TargetActor, TEXT("ShieldLight"));
	Light->SetLightColor(Vis.SecondaryColor);
	Light->SetIntensity(Vis.GlowIntensity * 1000.f);
	Light->SetAttenuationRadius(300.f);
	Light->AttachToComponent(TargetActor->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetIncludingScale);
	Light->RegisterComponent();
}
