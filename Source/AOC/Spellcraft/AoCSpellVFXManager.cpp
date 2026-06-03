// AoCSpellVFXManager.cpp - v31: Directional Combat VFX
// Cast = energy gathers at hand | Projectile = flies FORWARD at target | Impact = explodes at target
// Each school has unique visual pattern. No more fountains/fireworks.
#include "AoCSpellVFXManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"

// Runtime material creation includes
#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#endif

UAoCSpellVFXManager::UAoCSpellVFXManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f; // ~60fps
}

void UAoCSpellVFXManager::BeginPlay()
{
	Super::BeginPlay();
	InitSchoolConfigs();

	// Load master material - try known paths first
	MasterSpellMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AoC/VFX/M_SpellGlow.M_SpellGlow"));
	if (!MasterSpellMaterial)
	{
		MasterSpellMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AoC/VFX/Materials/M_SpellGlow.M_SpellGlow"));
	}
	if (!MasterSpellMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VFX] M_SpellGlow not found as asset — creating runtime material"));
		CreateRuntimeSpellMaterial();
	}
	if (MasterSpellMaterial)
	{
		UE_LOG(LogTemp, Log, TEXT("[VFX] Spell material ready: %s"), *MasterSpellMaterial->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VFX] CRITICAL: No spell material available — VFX will be white!"));
	}

	// Load meshes
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));

	UE_LOG(LogTemp, Log, TEXT("[VFX] v31 VFXManager initialized - directional combat VFX ready"));
}

void UAoCSpellVFXManager::InitSchoolConfigs()
{
	// PYROMANCY - Fiery orange, intense heat glow
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(1.0f, 0.3f, 0.0f, 1.0f);
		C.EmissiveColor = FLinearColor(5.0f, 1.5f, 0.0f, 1.0f);
		C.LightIntensity = 15000.0f;
		C.LightRadius = 600.0f;
		C.CastParticleCount = 12;
		C.CastParticleSize = 6.0f;
		C.CastGatherRadius = 80.0f;
		C.CastGatherSpeed = 200.0f;
		C.CastDuration = 0.6f;
		C.ProjectileSize = 20.0f;
		C.ProjectileSpeed = 2500.0f;
		C.TrailParticleCount = 3;
		C.TrailParticleSize = 8.0f;
		C.TrailSpread = 20.0f;
		C.ImpactParticleCount = 20;
		C.ImpactParticleSize = 12.0f;
		C.ImpactSpread = 400.0f;
		C.ImpactSpeed = 500.0f;
		C.ImpactLifetime = 1.0f;
		C.bLightFlicker = true;
		SchoolConfigs.Add(EAoCMagicSchool::Pyromancy, C);
	}

	// CRYOMANCY - Cold blue crystals
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.3f, 0.7f, 1.0f, 1.0f);
		C.EmissiveColor = FLinearColor(1.0f, 3.0f, 5.0f, 1.0f);
		C.LightIntensity = 10000.0f;
		C.LightRadius = 500.0f;
		C.CastParticleCount = 10;
		C.CastParticleSize = 5.0f;
		C.CastGatherRadius = 60.0f;
		C.CastGatherSpeed = 180.0f;
		C.CastDuration = 0.5f;
		C.ProjectileSize = 18.0f;
		C.ProjectileSpeed = 2800.0f;
		C.TrailParticleCount = 2;
		C.TrailParticleSize = 5.0f;
		C.TrailSpread = 10.0f;
		C.ImpactParticleCount = 18;
		C.ImpactParticleSize = 8.0f;
		C.ImpactSpread = 350.0f;
		C.ImpactSpeed = 400.0f;
		C.ImpactLifetime = 1.2f;
		C.bLightFlicker = false;
		SchoolConfigs.Add(EAoCMagicSchool::Cryomancy, C);
	}

	// STORMCALLING - Bright white-blue lightning
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.8f, 0.9f, 1.0f, 1.0f);
		C.EmissiveColor = FLinearColor(4.0f, 4.5f, 5.0f, 1.0f);
		C.LightIntensity = 30000.0f;
		C.LightRadius = 900.0f;
		C.CastParticleCount = 8;
		C.CastParticleSize = 4.0f;
		C.CastGatherRadius = 50.0f;
		C.CastGatherSpeed = 400.0f;
		C.CastDuration = 0.3f;
		C.ProjectileSize = 12.0f;
		C.ProjectileSpeed = 4000.0f;
		C.TrailParticleCount = 4;
		C.TrailParticleSize = 3.0f;
		C.TrailSpread = 30.0f;
		C.ImpactParticleCount = 25;
		C.ImpactParticleSize = 5.0f;
		C.ImpactSpread = 500.0f;
		C.ImpactSpeed = 800.0f;
		C.ImpactLifetime = 0.4f;
		C.bLightFlicker = true;
		SchoolConfigs.Add(EAoCMagicSchool::Stormcalling, C);
	}

	// ARCANA - Purple mystical energy
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f);
		C.EmissiveColor = FLinearColor(3.0f, 1.0f, 5.0f, 1.0f);
		C.LightIntensity = 12000.0f;
		C.LightRadius = 500.0f;
		C.CastParticleCount = 14;
		C.CastParticleSize = 5.0f;
		C.CastGatherRadius = 70.0f;
		C.CastGatherSpeed = 160.0f;
		C.CastDuration = 0.7f;
		C.ProjectileSize = 16.0f;
		C.ProjectileSpeed = 2200.0f;
		C.TrailParticleCount = 3;
		C.TrailParticleSize = 6.0f;
		C.TrailSpread = 15.0f;
		C.ImpactParticleCount = 18;
		C.ImpactParticleSize = 10.0f;
		C.ImpactSpread = 350.0f;
		C.ImpactSpeed = 350.0f;
		C.ImpactLifetime = 1.5f;
		C.bLightFlicker = false;
		SchoolConfigs.Add(EAoCMagicSchool::Arcana, C);
	}

	// UMBRAMANCY - Dark shadow wisps
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.3f, 0.0f, 0.5f, 1.0f);
		C.EmissiveColor = FLinearColor(1.5f, 0.0f, 2.5f, 1.0f);
		C.LightIntensity = 5000.0f;
		C.LightRadius = 400.0f;
		C.CastParticleCount = 10;
		C.CastParticleSize = 8.0f;
		C.CastGatherRadius = 90.0f;
		C.CastGatherSpeed = 120.0f;
		C.CastDuration = 0.8f;
		C.ProjectileSize = 22.0f;
		C.ProjectileSpeed = 1800.0f;
		C.TrailParticleCount = 3;
		C.TrailParticleSize = 10.0f;
		C.TrailSpread = 25.0f;
		C.ImpactParticleCount = 15;
		C.ImpactParticleSize = 14.0f;
		C.ImpactSpread = 300.0f;
		C.ImpactSpeed = 200.0f;
		C.ImpactLifetime = 2.0f;
		C.bLightFlicker = true;
		SchoolConfigs.Add(EAoCMagicSchool::Umbramancy, C);
	}

	// NECROMANCY - Sickly green death energy
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.2f, 0.6f, 0.1f, 1.0f);
		C.EmissiveColor = FLinearColor(0.8f, 3.0f, 0.4f, 1.0f);
		C.LightIntensity = 7000.0f;
		C.LightRadius = 500.0f;
		C.CastParticleCount = 10;
		C.CastParticleSize = 7.0f;
		C.CastGatherRadius = 85.0f;
		C.CastGatherSpeed = 140.0f;
		C.CastDuration = 0.7f;
		C.ProjectileSize = 18.0f;
		C.ProjectileSpeed = 2000.0f;
		C.TrailParticleCount = 3;
		C.TrailParticleSize = 8.0f;
		C.TrailSpread = 20.0f;
		C.ImpactParticleCount = 18;
		C.ImpactParticleSize = 12.0f;
		C.ImpactSpread = 350.0f;
		C.ImpactSpeed = 300.0f;
		C.ImpactLifetime = 1.8f;
		C.bLightFlicker = true;
		SchoolConfigs.Add(EAoCMagicSchool::Necromancy, C);
	}

	// VERDANCY - Earthy green nature energy
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.1f, 0.8f, 0.2f, 1.0f);
		C.EmissiveColor = FLinearColor(0.4f, 4.0f, 0.8f, 1.0f);
		C.LightIntensity = 9000.0f;
		C.LightRadius = 500.0f;
		C.CastParticleCount = 10;
		C.CastParticleSize = 6.0f;
		C.CastGatherRadius = 75.0f;
		C.CastGatherSpeed = 150.0f;
		C.CastDuration = 0.6f;
		C.ProjectileSize = 16.0f;
		C.ProjectileSpeed = 2200.0f;
		C.TrailParticleCount = 2;
		C.TrailParticleSize = 7.0f;
		C.TrailSpread = 18.0f;
		C.ImpactParticleCount = 16;
		C.ImpactParticleSize = 10.0f;
		C.ImpactSpread = 350.0f;
		C.ImpactSpeed = 350.0f;
		C.ImpactLifetime = 1.5f;
		C.bLightFlicker = false;
		SchoolConfigs.Add(EAoCMagicSchool::Verdancy, C);
	}

	// RADIANCE - Golden holy light
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(1.0f, 0.85f, 0.4f, 1.0f);
		C.EmissiveColor = FLinearColor(5.0f, 4.25f, 2.0f, 1.0f);
		C.LightIntensity = 20000.0f;
		C.LightRadius = 700.0f;
		C.CastParticleCount = 16;
		C.CastParticleSize = 5.0f;
		C.CastGatherRadius = 70.0f;
		C.CastGatherSpeed = 200.0f;
		C.CastDuration = 0.5f;
		C.ProjectileSize = 15.0f;
		C.ProjectileSpeed = 2600.0f;
		C.TrailParticleCount = 3;
		C.TrailParticleSize = 5.0f;
		C.TrailSpread = 12.0f;
		C.ImpactParticleCount = 22;
		C.ImpactParticleSize = 8.0f;
		C.ImpactSpread = 400.0f;
		C.ImpactSpeed = 450.0f;
		C.ImpactLifetime = 1.0f;
		C.bLightFlicker = false;
		SchoolConfigs.Add(EAoCMagicSchool::Radiance, C);
	}

	// SANGROMANCY - Deep crimson blood magic
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.8f, 0.0f, 0.1f, 1.0f);
		C.EmissiveColor = FLinearColor(4.0f, 0.0f, 0.5f, 1.0f);
		C.LightIntensity = 8000.0f;
		C.LightRadius = 450.0f;
		C.CastParticleCount = 10;
		C.CastParticleSize = 6.0f;
		C.CastGatherRadius = 65.0f;
		C.CastGatherSpeed = 180.0f;
		C.CastDuration = 0.6f;
		C.ProjectileSize = 16.0f;
		C.ProjectileSpeed = 2200.0f;
		C.TrailParticleCount = 3;
		C.TrailParticleSize = 7.0f;
		C.TrailSpread = 15.0f;
		C.ImpactParticleCount = 16;
		C.ImpactParticleSize = 10.0f;
		C.ImpactSpread = 300.0f;
		C.ImpactSpeed = 350.0f;
		C.ImpactLifetime = 1.3f;
		C.bLightFlicker = true;
		SchoolConfigs.Add(EAoCMagicSchool::Sangromancy, C);
	}

	// DOMINION - Psychic cyan pulse
	{
		FSchoolVFXConfig C;
		C.Color = FLinearColor(0.0f, 0.8f, 0.9f, 1.0f);
		C.EmissiveColor = FLinearColor(0.0f, 4.0f, 4.5f, 1.0f);
		C.LightIntensity = 10000.0f;
		C.LightRadius = 600.0f;
		C.CastParticleCount = 8;
		C.CastParticleSize = 5.0f;
		C.CastGatherRadius = 100.0f;
		C.CastGatherSpeed = 250.0f;
		C.CastDuration = 0.4f;
		C.ProjectileSize = 14.0f;
		C.ProjectileSpeed = 3000.0f;
		C.TrailParticleCount = 2;
		C.TrailParticleSize = 4.0f;
		C.TrailSpread = 8.0f;
		C.ImpactParticleCount = 20;
		C.ImpactParticleSize = 6.0f;
		C.ImpactSpread = 450.0f;
		C.ImpactSpeed = 600.0f;
		C.ImpactLifetime = 0.6f;
		C.bLightFlicker = false;
		SchoolConfigs.Add(EAoCMagicSchool::Dominion, C);
	}
}

// ================================================================
// RUNTIME MATERIAL CREATION
// ================================================================

void UAoCSpellVFXManager::CreateRuntimeSpellMaterial()
{
#if WITH_EDITOR
	// Create a proper Additive + Unlit material entirely from code
	// This ensures spell particles are brightly colored even without M_SpellGlow asset
	UMaterial* Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_SpellGlow_Runtime"));
	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;

	// Create EmissiveColor vector parameter (will be overridden per-school via DMI)
	UMaterialExpressionVectorParameter* EmissiveParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
	EmissiveParam->ParameterName = FName(TEXT("EmissiveColor"));
	EmissiveParam->DefaultValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// Create EmissiveStrength scalar parameter (brightness multiplier)
	UMaterialExpressionScalarParameter* StrengthParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	StrengthParam->ParameterName = FName(TEXT("EmissiveStrength"));
	StrengthParam->DefaultValue = 10.0f;

	// Create Multiply node: EmissiveColor * EmissiveStrength
	UMaterialExpressionMultiply* MultiplyNode = NewObject<UMaterialExpressionMultiply>(Mat);

	// Create Opacity scalar parameter
	UMaterialExpressionScalarParameter* OpacityParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	OpacityParam->ParameterName = FName(TEXT("Opacity"));
	OpacityParam->DefaultValue = 0.9f;

	// Add all expressions to the material
	Mat->GetExpressionCollection().AddExpression(EmissiveParam);
	Mat->GetExpressionCollection().AddExpression(StrengthParam);
	Mat->GetExpressionCollection().AddExpression(MultiplyNode);
	Mat->GetExpressionCollection().AddExpression(OpacityParam);

	// Wire the graph: EmissiveColor * EmissiveStrength → Emissive output
	MultiplyNode->A.Connect(0, EmissiveParam);
	MultiplyNode->B.Connect(0, StrengthParam);

	// Connect to material outputs
	Mat->GetEditorOnlyData()->EmissiveColor.Connect(0, MultiplyNode);
	Mat->GetEditorOnlyData()->Opacity.Connect(0, OpacityParam);

	// Trigger shader compilation
	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();

	MasterSpellMaterial = Mat;

	UE_LOG(LogTemp, Log, TEXT("[VFX] Runtime M_SpellGlow created: Additive + Unlit + EmissiveColor/Strength/Opacity parameters"));
#else
	UE_LOG(LogTemp, Error, TEXT("[VFX] Cannot create runtime material outside editor — need M_SpellGlow asset!"));
#endif
}

// ================================================================
// MAIN ENTRY POINTS
// ================================================================

void UAoCSpellVFXManager::SpawnCastVFX(EAoCMagicSchool School, FVector Location, FRotator Rotation)
{
	FSchoolVFXConfig Config = GetConfig(School);

	// Cast VFX = energy gathering at the hand
	// Particles start spread out around Location and swirl INWARD
	FVector Forward = Rotation.Vector();

	for (int32 i = 0; i < Config.CastParticleCount; i++)
	{
		// Random point on a sphere around the cast location
		float Theta = FMath::RandRange(0.0f, 2.0f * PI);
		float Phi = FMath::RandRange(0.0f, PI);
		FVector Offset(
			FMath::Sin(Phi) * FMath::Cos(Theta),
			FMath::Sin(Phi) * FMath::Sin(Theta),
			FMath::Cos(Phi)
		);
		FVector StartPos = Location + Offset * Config.CastGatherRadius;

		// Velocity points TOWARD the hand (inward gathering)
		FVector ToCenter = (Location - StartPos).GetSafeNormal();
		FVector Vel = ToCenter * Config.CastGatherSpeed;

		float Size = Config.CastParticleSize * FMath::RandRange(0.5f, 1.5f);
		SpawnSingleParticle(StartPos, Vel, Size, Config.CastDuration, School);
	}

	// Bright flash at cast point
	SpawnDynamicLight(School, Location, 1.5f, Config.CastDuration);

	UE_LOG(LogTemp, Log, TEXT("[VFX] Cast VFX: %d particles gathering at hand"), Config.CastParticleCount);
}

void UAoCSpellVFXManager::SpawnProjectileVFX(EAoCMagicSchool School, FVector Start, FVector End, float Speed)
{
	FSchoolVFXConfig Config = GetConfig(School);

	if (!SphereMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[VFX] No sphere mesh for projectile!"));
		return;
	}

	FSpellProjectile Proj;
	Proj.StartLocation = Start;
	Proj.TargetLocation = End;
	Proj.Direction = (End - Start).GetSafeNormal();
	Proj.Speed = (Speed > 0) ? Speed : Config.ProjectileSpeed;
	Proj.MaxDistance = FVector::Dist(Start, End);
	Proj.Progress = 0.0f;
	Proj.School = School;
	Proj.ProjectileScale = Config.ProjectileSize / 100.0f; // Sphere is 100cm default
	Proj.TrailInterval = 0.04f;
	Proj.TrailTimer = 0.0f;

	// Create projectile mesh
	AActor* Owner = GetOwner();
	if (!Owner) return;

	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Owner);
	MeshComp->SetStaticMesh(SphereMesh);
	MeshComp->SetWorldLocation(Start);
	MeshComp->SetWorldScale3D(FVector(Proj.ProjectileScale));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCastShadow(false);
	MeshComp->RegisterComponent();
	MeshComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

	// Apply school-colored material
	UMaterialInstanceDynamic* DMI = GetOrCreateDMI(School);
	if (DMI)
	{
		UMaterialInstanceDynamic* ProjDMI = UMaterialInstanceDynamic::Create(MasterSpellMaterial, Owner);
		ProjDMI->SetVectorParameterValue(TEXT("EmissiveColor"), Config.EmissiveColor);
		ProjDMI->SetScalarParameterValue(TEXT("EmissiveStrength"), 15.0f); // Extra bright projectile
		ProjDMI->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		MeshComp->SetMaterial(0, ProjDMI);
		Proj.DynMaterial = ProjDMI;
	}

	Proj.Mesh = MeshComp;

	// Projectile light - travels with the bolt
	UPointLightComponent* LightComp = NewObject<UPointLightComponent>(Owner);
	LightComp->SetIntensity(Config.LightIntensity * 2.0f);
	LightComp->SetLightColor(Config.Color);
	LightComp->SetAttenuationRadius(Config.LightRadius);
	LightComp->SetCastShadows(false);
	LightComp->SetWorldLocation(Start);
	LightComp->RegisterComponent();
	LightComp->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepWorldTransform);
	Proj.Light = LightComp;

	ActiveProjectiles.Add(Proj);

	UE_LOG(LogTemp, Log, TEXT("[VFX] Projectile launched: %s → target at %.0f units, speed %.0f"),
		*UEnum::GetValueAsString(School), Proj.MaxDistance, Proj.Speed);
}

void UAoCSpellVFXManager::SpawnImpactVFX(EAoCMagicSchool School, FVector Location)
{
	FSchoolVFXConfig Config = GetConfig(School);

	// Impact = particles scatter OUTWARD from impact point in a HORIZONTAL ring
	// Not up, not down — outward from the hit point like a shockwave
	for (int32 i = 0; i < Config.ImpactParticleCount; i++)
	{
		// Random direction mostly horizontal (slight vertical variance)
		float Angle = FMath::RandRange(0.0f, 2.0f * PI);
		float ZVar = FMath::RandRange(-0.2f, 0.3f); // Slight upward bias for explosion feel
		FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), ZVar);
		Dir.Normalize();

		FVector Vel = Dir * Config.ImpactSpeed * FMath::RandRange(0.5f, 1.5f);
		float Size = Config.ImpactParticleSize * FMath::RandRange(0.4f, 1.2f);

		// Impact particles get some gravity so they arc and fall
		SpawnSingleParticle(Location, Vel, Size, Config.ImpactLifetime, School, 200.0f, 0.5f);
	}

	// Big impact flash
	SpawnDynamicLight(School, Location, 2.5f, 0.8f);

	// Secondary ring of smaller particles for shockwave feel
	int32 RingCount = Config.ImpactParticleCount / 2;
	for (int32 i = 0; i < RingCount; i++)
	{
		float Angle = (float)i / (float)RingCount * 2.0f * PI;
		FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		FVector Vel = Dir * Config.ImpactSpeed * 2.0f;
		float Size = Config.ImpactParticleSize * 0.3f;

		SpawnSingleParticle(Location, Vel, Size, Config.ImpactLifetime * 0.5f, School, 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("[VFX] Impact VFX: %d particles + %d ring at location"), Config.ImpactParticleCount, RingCount);
}

// ================================================================
// CORE HELPERS
// ================================================================

UMaterialInstanceDynamic* UAoCSpellVFXManager::GetOrCreateDMI(EAoCMagicSchool School)
{
	if (UMaterialInstanceDynamic** Found = SchoolDMIs.Find(School))
	{
		return *Found;
	}

	if (!MasterSpellMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("[VFX] Cannot create DMI - no master material!"));
		return nullptr;
	}

	FSchoolVFXConfig Config = GetConfig(School);
	UMaterialInstanceDynamic* DMI = UMaterialInstanceDynamic::Create(MasterSpellMaterial, GetOwner());
	if (DMI)
	{
		DMI->SetVectorParameterValue(TEXT("EmissiveColor"), Config.EmissiveColor);
		DMI->SetScalarParameterValue(TEXT("EmissiveStrength"), 10.0f);
		DMI->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
		SchoolDMIs.Add(School, DMI);
		UE_LOG(LogTemp, Log, TEXT("[VFX] Created DMI for %s: Color(%.1f, %.1f, %.1f)"),
			*UEnum::GetValueAsString(School), Config.EmissiveColor.R, Config.EmissiveColor.G, Config.EmissiveColor.B);
	}
	return DMI;
}

FSpellParticle& UAoCSpellVFXManager::SpawnSingleParticle(FVector Location, FVector Velocity, float Size, float Lifetime, EAoCMagicSchool School, float Gravity, float Drag)
{
	AActor* Owner = GetOwner();

	FSpellParticle P;
	P.Velocity = Velocity;
	P.LifetimeRemaining = Lifetime;
	P.MaxLifetime = Lifetime;
	P.InitialScale = Size / 100.0f; // Sphere is 100cm
	P.GravityScale = Gravity;
	P.DragCoeff = Drag;

	if (Owner && SphereMesh)
	{
		UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Owner);
		MeshComp->SetStaticMesh(SphereMesh);
		MeshComp->SetWorldLocation(Location);
		MeshComp->SetWorldScale3D(FVector(P.InitialScale));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCastShadow(false);
		MeshComp->RegisterComponent();
		MeshComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

		// Apply school-colored material
		if (MasterSpellMaterial)
		{
			UMaterialInstanceDynamic* DMI = UMaterialInstanceDynamic::Create(MasterSpellMaterial, Owner);
			FSchoolVFXConfig Config = GetConfig(School);
			DMI->SetVectorParameterValue(TEXT("EmissiveColor"), Config.EmissiveColor);
			DMI->SetScalarParameterValue(TEXT("EmissiveStrength"), 10.0f);
			DMI->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
			MeshComp->SetMaterial(0, DMI);
			P.DynMaterial = DMI;
		}

		P.Mesh = MeshComp;
	}

	int32 Idx = ActiveParticles.Add(P);
	return ActiveParticles[Idx];
}

void UAoCSpellVFXManager::SpawnTrailParticle(EAoCMagicSchool School, FVector Location, FVector ProjectileDir)
{
	FSchoolVFXConfig Config = GetConfig(School);

	for (int32 i = 0; i < Config.TrailParticleCount; i++)
	{
		// Trail goes slightly backward + random spread perpendicular to direction
		FVector Right = FVector::CrossProduct(ProjectileDir, FVector::UpVector).GetSafeNormal();
		FVector Up = FVector::CrossProduct(Right, ProjectileDir).GetSafeNormal();

		FVector Spread = Right * FMath::RandRange(-Config.TrailSpread, Config.TrailSpread)
			+ Up * FMath::RandRange(-Config.TrailSpread, Config.TrailSpread);

		// Trail velocity = slight backward drift + spread
		FVector Vel = -ProjectileDir * 50.0f + Spread * 3.0f;

		float Size = Config.TrailParticleSize * FMath::RandRange(0.3f, 1.0f);
		SpawnSingleParticle(Location + Spread, Vel, Size, 0.4f, School, 0.0f, 2.0f);
	}
}

void UAoCSpellVFXManager::SpawnDynamicLight(EAoCMagicSchool School, FVector Location, float IntensityMult, float Lifetime)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FSchoolVFXConfig Config = GetConfig(School);

	UPointLightComponent* LightComp = NewObject<UPointLightComponent>(Owner);
	LightComp->SetIntensity(Config.LightIntensity * IntensityMult);
	LightComp->SetLightColor(Config.Color);
	LightComp->SetAttenuationRadius(Config.LightRadius);
	LightComp->SetCastShadows(false);
	LightComp->SetWorldLocation(Location);
	LightComp->RegisterComponent();
	LightComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

	FSpellLight SL;
	SL.Light = LightComp;
	SL.LifetimeRemaining = Lifetime;
	SL.MaxLifetime = Lifetime;
	SL.InitialIntensity = Config.LightIntensity * IntensityMult;
	SL.bFlicker = Config.bLightFlicker;
	ActiveLights.Add(SL);
}

UAoCSpellVFXManager::FSchoolVFXConfig UAoCSpellVFXManager::GetConfig(EAoCMagicSchool School)
{
	if (FSchoolVFXConfig* Found = SchoolConfigs.Find(School))
	{
		return *Found;
	}

	// Default fallback (bright white)
	FSchoolVFXConfig Default;
	Default.Color = FLinearColor(1, 1, 1, 1);
	Default.EmissiveColor = FLinearColor(4, 4, 4, 1);
	Default.LightIntensity = 10000.0f;
	Default.LightRadius = 500.0f;
	Default.CastParticleCount = 10;
	Default.CastParticleSize = 6.0f;
	Default.CastGatherRadius = 70.0f;
	Default.CastGatherSpeed = 180.0f;
	Default.CastDuration = 0.6f;
	Default.ProjectileSize = 16.0f;
	Default.ProjectileSpeed = 2000.0f;
	Default.TrailParticleCount = 2;
	Default.TrailParticleSize = 6.0f;
	Default.TrailSpread = 15.0f;
	Default.ImpactParticleCount = 16;
	Default.ImpactParticleSize = 10.0f;
	Default.ImpactSpread = 350.0f;
	Default.ImpactSpeed = 400.0f;
	Default.ImpactLifetime = 1.0f;
	Default.bLightFlicker = false;
	return Default;
}

// ================================================================
// TICK UPDATES
// ================================================================

void UAoCSpellVFXManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateParticles(DeltaTime);
	UpdateLights(DeltaTime);
	UpdateProjectiles(DeltaTime);
}

void UAoCSpellVFXManager::UpdateParticles(float DeltaTime)
{
	for (int32 i = ActiveParticles.Num() - 1; i >= 0; i--)
	{
		FSpellParticle& P = ActiveParticles[i];
		P.LifetimeRemaining -= DeltaTime;

		if (P.LifetimeRemaining <= 0.0f)
		{
			if (P.Mesh)
			{
				P.Mesh->DestroyComponent();
			}
			ActiveParticles.RemoveAt(i);
			continue;
		}

		// Apply gravity
		if (P.GravityScale > 0.0f)
		{
			P.Velocity.Z -= P.GravityScale * DeltaTime;
		}

		// Apply drag (slows particles over time)
		if (P.DragCoeff > 0.0f)
		{
			P.Velocity *= FMath::Max(0.0f, 1.0f - P.DragCoeff * DeltaTime);
		}

		// Move
		if (P.Mesh)
		{
			FVector NewLoc = P.Mesh->GetComponentLocation() + P.Velocity * DeltaTime;
			P.Mesh->SetWorldLocation(NewLoc);

			// Fade out: scale down and reduce opacity
			float Alpha = P.LifetimeRemaining / P.MaxLifetime;
			float Scale = P.InitialScale * FMath::Max(Alpha, 0.1f);
			P.Mesh->SetWorldScale3D(FVector(Scale));

			if (P.DynMaterial)
			{
				P.DynMaterial->SetScalarParameterValue(TEXT("Opacity"), Alpha * 0.9f);
			}
		}
	}
}

void UAoCSpellVFXManager::UpdateLights(float DeltaTime)
{
	for (int32 i = ActiveLights.Num() - 1; i >= 0; i--)
	{
		FSpellLight& L = ActiveLights[i];
		L.LifetimeRemaining -= DeltaTime;

		if (L.LifetimeRemaining <= 0.0f)
		{
			if (L.Light)
			{
				L.Light->DestroyComponent();
			}
			ActiveLights.RemoveAt(i);
			continue;
		}

		if (L.Light)
		{
			float Alpha = L.LifetimeRemaining / L.MaxLifetime;
			float Intensity = L.InitialIntensity * Alpha;

			// Flicker effect for fire/lightning
			if (L.bFlicker)
			{
				Intensity *= FMath::RandRange(0.7f, 1.3f);
			}

			L.Light->SetIntensity(Intensity);
		}
	}
}

void UAoCSpellVFXManager::UpdateProjectiles(float DeltaTime)
{
	for (int32 i = ActiveProjectiles.Num() - 1; i >= 0; i--)
	{
		FSpellProjectile& P = ActiveProjectiles[i];

		// Move projectile FORWARD
		float DistThisFrame = P.Speed * DeltaTime;
		P.Progress += DistThisFrame;

		if (P.Mesh)
		{
			FVector NewLoc = P.StartLocation + P.Direction * P.Progress;
			P.Mesh->SetWorldLocation(NewLoc);

			if (P.Light)
			{
				P.Light->SetWorldLocation(NewLoc);
			}

			// Spawn trail particles behind the projectile
			P.TrailTimer += DeltaTime;
			if (P.TrailTimer >= P.TrailInterval)
			{
				P.TrailTimer = 0.0f;
				SpawnTrailParticle(P.School, NewLoc, P.Direction);
			}
		}

		// Hit target?
		if (P.Progress >= P.MaxDistance)
		{
			// Spawn impact at target
			SpawnImpactVFX(P.School, P.TargetLocation);

			// Cleanup projectile
			if (P.Mesh) P.Mesh->DestroyComponent();
			if (P.Light) P.Light->DestroyComponent();
			ActiveProjectiles.RemoveAt(i);
		}
		// Safety: destroy after 10 seconds max
		else if (P.Progress > 50000.0f)
		{
			if (P.Mesh) P.Mesh->DestroyComponent();
			if (P.Light) P.Light->DestroyComponent();
			ActiveProjectiles.RemoveAt(i);
		}
	}
}
