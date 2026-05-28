// Source/AOC/Spellcraft/AoCSpellScreenEffects.cpp
// v29 — Per-school screen effects: camera shake, flash, chromatic aberration, vignette

#include "AoCSpellScreenEffects.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"

UAoCSpellScreenEffects::UAoCSpellScreenEffects()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.016f;

	bFlashing = false;
	FlashTimeRemaining = 0.f;
	FlashMaxTime = 0.f;
	CurrentFlashIntensity = 0.f;

	ActiveChromaticAberration = 0.f;
	ActiveVignette = 0.f;
	ActiveRadialBlur = 0.f;
	PostProcessFadeTime = 0.5f;
	PostProcessFadeRemaining = 0.f;
}

void UAoCSpellScreenEffects::BeginPlay()
{
	Super::BeginPlay();
	InitSchoolEffects();
}

void UAoCSpellScreenEffects::InitSchoolEffects()
{
	// ARCANA — Mystical purple flash, moderate shake
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.25f; E.ShakeDuration = 0.4f;
		E.FlashColor = FLinearColor(0.6f, 0.2f, 1.0f); E.FlashIntensity = 0.2f; E.FlashDuration = 0.12f;
		E.ChromaticAberration = 0.3f; E.VignetteIntensity = 0.1f; E.RadialBlur = 0.0f;
		SchoolEffects.Add(EAoCMagicSchool::Arcana, E);
	}
	// PYROMANCY — Orange flash, strong shake (explosions!)
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.5f; E.ShakeDuration = 0.6f;
		E.FlashColor = FLinearColor(1.0f, 0.4f, 0.0f); E.FlashIntensity = 0.4f; E.FlashDuration = 0.15f;
		E.ChromaticAberration = 0.2f; E.VignetteIntensity = 0.15f; E.RadialBlur = 0.1f;
		SchoolEffects.Add(EAoCMagicSchool::Pyromancy, E);
	}
	// CRYOMANCY — White-blue flash, subtle shake
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.15f; E.ShakeDuration = 0.3f;
		E.FlashColor = FLinearColor(0.7f, 0.9f, 1.0f); E.FlashIntensity = 0.25f; E.FlashDuration = 0.2f;
		E.ChromaticAberration = 0.4f; E.VignetteIntensity = 0.2f; E.RadialBlur = 0.0f;
		SchoolEffects.Add(EAoCMagicSchool::Cryomancy, E);
	}
	// STORMCALLING — Bright white flash (lightning!), heavy shake
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.6f; E.ShakeDuration = 0.3f;
		E.FlashColor = FLinearColor(1.0f, 1.0f, 1.0f); E.FlashIntensity = 0.6f; E.FlashDuration = 0.08f;
		E.ChromaticAberration = 0.5f; E.VignetteIntensity = 0.1f; E.RadialBlur = 0.0f;
		SchoolEffects.Add(EAoCMagicSchool::Stormcalling, E);
	}
	// NECROMANCY — Sickly green pulse, moderate shake, heavy vignette (death closing in)
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.35f; E.ShakeDuration = 0.5f;
		E.FlashColor = FLinearColor(0.29f, 0.49f, 0.25f); E.FlashIntensity = 0.3f; E.FlashDuration = 0.2f;
		E.ChromaticAberration = 0.35f; E.VignetteIntensity = 0.3f; E.RadialBlur = 0.05f;
		SchoolEffects.Add(EAoCMagicSchool::Necromancy, E);
	}
	// VERDANCY — Green flash, gentle shake (nature is calm)
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.1f; E.ShakeDuration = 0.3f;
		E.FlashColor = FLinearColor(0.2f, 0.8f, 0.2f); E.FlashIntensity = 0.15f; E.FlashDuration = 0.15f;
		E.ChromaticAberration = 0.1f; E.VignetteIntensity = 0.05f; E.RadialBlur = 0.0f;
		SchoolEffects.Add(EAoCMagicSchool::Verdancy, E);
	}
	// UMBRAMANCY — Dark purple flash, moderate shake, high chromatic aberration
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.3f; E.ShakeDuration = 0.5f;
		E.FlashColor = FLinearColor(0.3f, 0.0f, 0.5f); E.FlashIntensity = 0.25f; E.FlashDuration = 0.2f;
		E.ChromaticAberration = 0.6f; E.VignetteIntensity = 0.25f; E.RadialBlur = 0.05f;
		SchoolEffects.Add(EAoCMagicSchool::Umbramancy, E);
	}
	// RADIANCE — Bright gold flash, gentle shake
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.15f; E.ShakeDuration = 0.4f;
		E.FlashColor = FLinearColor(1.0f, 0.9f, 0.5f); E.FlashIntensity = 0.35f; E.FlashDuration = 0.12f;
		E.ChromaticAberration = 0.15f; E.VignetteIntensity = 0.0f; E.RadialBlur = 0.0f;
		SchoolEffects.Add(EAoCMagicSchool::Radiance, E);
	}
	// SANGROMANCY — Dark red flash, moderate shake, heavy vignette (blood magic)
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.35f; E.ShakeDuration = 0.5f;
		E.FlashColor = FLinearColor(0.8f, 0.0f, 0.0f); E.FlashIntensity = 0.3f; E.FlashDuration = 0.18f;
		E.ChromaticAberration = 0.4f; E.VignetteIntensity = 0.3f; E.RadialBlur = 0.08f;
		SchoolEffects.Add(EAoCMagicSchool::Sangromancy, E);
	}
	// DOMINION — Indigo flash, minimal shake (mind control is subtle), high chromatic
	{
		FSchoolScreenEffect E;
		E.ShakeIntensity = 0.2f; E.ShakeDuration = 0.3f;
		E.FlashColor = FLinearColor(0.5f, 0.2f, 0.8f); E.FlashIntensity = 0.2f; E.FlashDuration = 0.15f;
		E.ChromaticAberration = 0.5f; E.VignetteIntensity = 0.15f; E.RadialBlur = 0.0f;
		SchoolEffects.Add(EAoCMagicSchool::Dominion, E);
	}

	UE_LOG(LogTemp, Log, TEXT("[ScreenFX] Initialized %d school screen effect configs"), SchoolEffects.Num());
}

void UAoCSpellScreenEffects::TriggerSpellEffect(EAoCMagicSchool School, float IntensityScale)
{
	FSchoolScreenEffect* Effect = SchoolEffects.Find(School);
	if (!Effect) return;

	// Camera shake
	float ShakeIntensity = Effect->ShakeIntensity * IntensityScale;
	if (ShakeIntensity > 0.01f)
	{
		ApplyCameraShake(ShakeIntensity, Effect->ShakeDuration);
	}

	// Screen flash
	float FlashIntensity = Effect->FlashIntensity * IntensityScale;
	if (FlashIntensity > 0.01f)
	{
		bFlashing = true;
		FlashTimeRemaining = Effect->FlashDuration;
		FlashMaxTime = Effect->FlashDuration;
		CurrentFlashColor = Effect->FlashColor;
		CurrentFlashIntensity = FlashIntensity;
	}

	// Post-process effects (chromatic aberration, vignette, radial blur)
	ActiveChromaticAberration = Effect->ChromaticAberration * IntensityScale;
	ActiveVignette = Effect->VignetteIntensity * IntensityScale;
	ActiveRadialBlur = Effect->RadialBlur * IntensityScale;
	PostProcessFadeRemaining = 0.5f;

	UE_LOG(LogTemp, Log, TEXT("[ScreenFX] Triggered %s effects (intensity=%.2f, shake=%.2f, flash=%.2f)"),
		*UAoCSpellDatabase::GetSchoolName(School), IntensityScale, ShakeIntensity, FlashIntensity);
}

void UAoCSpellScreenEffects::ApplyCameraShake(float Intensity, float Duration)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->PlayerCameraManager) return;

	// Use lightweight camera shake via controller
	// Oscillation-based shake
	PC->ClientStartCameraShake(
		nullptr, // No specific shake class — use direct values
		Intensity,
		ECameraShakePlaySpace::CameraLocal,
		FRotator::ZeroRotator
	);

	// Alternative: Direct camera offset manipulation for more control
	// This creates a brief camera position offset
	APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
	if (CamMgr)
	{
		float ShakeX = FMath::RandRange(-Intensity, Intensity) * 5.f;
		float ShakeY = FMath::RandRange(-Intensity, Intensity) * 5.f;
		float ShakeZ = FMath::RandRange(-Intensity, Intensity) * 3.f;

		// Apply a small FOV bump for impact feel
		float CurrentFOV = CamMgr->GetFOVAngle();
		CamMgr->SetFOV(CurrentFOV + Intensity * 2.0f);

		// FOV will reset on next tick naturally or via our tick
	}
}

void UAoCSpellScreenEffects::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update flash
	if (bFlashing)
	{
		FlashTimeRemaining -= DeltaTime;
		if (FlashTimeRemaining <= 0.f)
		{
			bFlashing = false;
			FlashTimeRemaining = 0.f;
		}
		else
		{
			// Apply screen flash via post-process
			float FlashAlpha = (FlashTimeRemaining / FlashMaxTime) * CurrentFlashIntensity;

			APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
			if (PC && PC->PlayerCameraManager)
			{
				// Set camera fade for flash effect
				PC->PlayerCameraManager->StartCameraFade(
					FlashAlpha, 0.0f, FlashTimeRemaining,
					CurrentFlashColor, false, false
				);
			}
		}
	}

	// Fade post-process effects
	if (PostProcessFadeRemaining > 0.f)
	{
		PostProcessFadeRemaining -= DeltaTime;
		float FadeFraction = FMath::Max(0.f, PostProcessFadeRemaining / 0.5f);

		// Apply chromatic aberration, vignette via post-process settings
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC && PC->PlayerCameraManager)
		{
			APlayerCameraManager* CamMgr = PC->PlayerCameraManager;

			// FOV recovery (gradually return to default)
			float TargetFOV = 90.f;
			float CurrentFOV = CamMgr->GetFOVAngle();
			if (FMath::Abs(CurrentFOV - TargetFOV) > 0.1f)
			{
				CamMgr->SetFOV(FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, 10.f));
			}
		}

		if (PostProcessFadeRemaining <= 0.f)
		{
			ActiveChromaticAberration = 0.f;
			ActiveVignette = 0.f;
			ActiveRadialBlur = 0.f;
		}
	}
}
