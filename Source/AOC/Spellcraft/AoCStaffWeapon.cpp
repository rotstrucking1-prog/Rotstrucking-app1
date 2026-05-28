// Source/AOC/Spellcraft/AoCStaffWeapon.cpp

#include "AoCStaffWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

// ─── Constructor ────────────────────────────────────────────────────────────

AAoCStaffWeapon::AAoCStaffWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	ActiveSchool = EAoCMagicSchool::Arcana;
	bIsCasting = false;
	GemMaterialInstance = nullptr;
	BaseGlowIntensity = 3.f;
	PulseTimer = 0.f;

	// ── Root scene component ────────────────────────────────────────────
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("StaffRoot"));
	SetRootComponent(Root);

	// ── Shaft (cylinder) ────────────────────────────────────────────────
	ShaftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShaftMesh"));
	ShaftMesh->SetupAttachment(Root);
	ShaftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Scale: thin and tall — default cylinder is 100 units diameter, 100 tall
	// Staff shaft: ~6 units diameter, ~120 units tall
	ShaftMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 1.2f));
	ShaftMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		ShaftMesh->SetStaticMesh(CylinderAsset.Object);
	}

	// ── Gem (sphere at top) ─────────────────────────────────────────────
	GemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GemMesh"));
	GemMesh->SetupAttachment(Root);
	GemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GemMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.12f));
	// Position gem at the top of the shaft
	GemMesh->SetRelativeLocation(FVector(0.f, 0.f, 65.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		GemMesh->SetStaticMesh(SphereAsset.Object);
	}

	// ── Gem light ───────────────────────────────────────────────────────
	GemLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GemLight"));
	GemLight->SetupAttachment(GemMesh);
	GemLight->SetIntensity(3000.f);
	GemLight->SetAttenuationRadius(200.f);
	GemLight->SetRelativeLocation(FVector::ZeroVector);
}

// ─── AttachToCharacter ──────────────────────────────────────────────────────

void AAoCStaffWeapon::AttachToCharacter(ACharacter* Character, FName SocketName)
{
	if (!Character) return;

	USkeletalMeshComponent* CharMesh = Character->GetMesh();
	if (!CharMesh) return;

	AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	UE_LOG(LogTemp, Log, TEXT("Staff attached to %s socket: %s"), *Character->GetName(), *SocketName.ToString());
}

// ─── SetActiveSchool ────────────────────────────────────────────────────────

void AAoCStaffWeapon::SetActiveSchool(EAoCMagicSchool NewSchool)
{
	ActiveSchool = NewSchool;
	ApplySchoolVisuals();
}

// ─── SetCasting ─────────────────────────────────────────────────────────────

void AAoCStaffWeapon::SetCasting(bool bNewCasting)
{
	bIsCasting = bNewCasting;
	if (!bIsCasting)
	{
		PulseTimer = 0.f;
		// Reset light to base intensity
		if (GemLight)
		{
			GemLight->SetIntensity(BaseGlowIntensity * 1000.f);
		}
	}
}

// ─── Tick (pulse animation) ─────────────────────────────────────────────────

void AAoCStaffWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsCasting && GemLight)
	{
		PulseTimer += DeltaTime;

		// Sinusoidal pulse: oscillates between 0.5x and 2.0x base intensity
		float Pulse = 0.5f + 1.5f * FMath::Abs(FMath::Sin(PulseTimer * 4.f));
		GemLight->SetIntensity(BaseGlowIntensity * 1000.f * Pulse);

		// Also pulse emissive on the gem material
		if (GemMaterialInstance)
		{
			FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(ActiveSchool);
			FLinearColor Emissive = Vis.SecondaryColor * Vis.GlowIntensity * Pulse;
			GemMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
		}
	}
}

// ─── Apply school colours ───────────────────────────────────────────────────

void AAoCStaffWeapon::ApplySchoolVisuals()
{
	FAoCSchoolVisuals Vis = UAoCSpellDatabase::GetSchoolVisuals(ActiveSchool);
	BaseGlowIntensity = Vis.GlowIntensity;

	// Gem material
	if (GemMesh)
	{
		UMaterial* BaseMat = Cast<UMaterial>(
			StaticLoadObject(UMaterial::StaticClass(), nullptr,
				TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));

		if (BaseMat)
		{
			GemMaterialInstance = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (GemMaterialInstance)
			{
				GemMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), Vis.PrimaryColor);
				FLinearColor Emissive = Vis.SecondaryColor * Vis.GlowIntensity;
				GemMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
				GemMesh->SetMaterial(0, GemMaterialInstance);
			}
		}
	}

	// Shaft — darker version of primary colour
	if (ShaftMesh)
	{
		UMaterial* BaseMat = Cast<UMaterial>(
			StaticLoadObject(UMaterial::StaticClass(), nullptr,
				TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));

		if (BaseMat)
		{
			UMaterialInstanceDynamic* ShaftMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (ShaftMID)
			{
				FLinearColor ShaftColor = Vis.PrimaryColor * 0.3f;
				ShaftColor.A = 1.f;
				ShaftMID->SetVectorParameterValue(TEXT("BaseColor"), ShaftColor);
				ShaftMesh->SetMaterial(0, ShaftMID);
			}
		}
	}

	// Light
	if (GemLight)
	{
		GemLight->SetLightColor(Vis.PrimaryColor);
		GemLight->SetIntensity(Vis.GlowIntensity * 1000.f);
	}

	UE_LOG(LogTemp, Log, TEXT("Staff school set to: %s"), *UAoCSpellDatabase::GetSchoolName(ActiveSchool));
}
