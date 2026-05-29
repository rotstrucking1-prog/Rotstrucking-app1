// AoCSpellScreenFX.cpp - AAA Screen Effects for Spell Casting
#include "AoCSpellScreenFX.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PostProcessComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformMath.h"

UAoCSpellScreenFX::UAoCSpellScreenFX()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UAoCSpellScreenFX::BeginPlay()
{
	Super::BeginPlay();
	InitSchoolConfigs();

	// Cache player controller and camera manager
	PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PlayerController)
	{
		CameraManager = PlayerController->PlayerCameraManager;
	}

	// Create a post-process component for screen overlays
	AActor* Owner = GetOwner();
	if (Owner)
	{
		ScreenFXPostProcess = NewObject<UPostProcessComponent>(Owner);
		if (ScreenFXPostProcess)
		{
			ScreenFXPostProcess->RegisterComponent();
			ScreenFXPostProcess->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			ScreenFXPostProcess->bUnbound = true; // Affects entire screen
			ScreenFXPostProcess->Priority = 100.0f; // High priority
			ScreenFXPostProcess->BlendWeight = 0.0f; // Start invisible
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SpellScreenFX: Initialized with %d school configs"), SchoolFXConfigs.Num());
}

void UAoCSpellScreenFX::InitSchoolConfigs()
{
	// ========== PYROMANCY - Fire ==========
	// Heat distortion, orange flash, heavy rumble
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 2.0f;
		C.ShakeDuration = 0.6f;
		C.ShakeFrequency = 20.0f;
		C.bShakeRotation = true;
		C.FlashColor = FLinearColor(1.0f, 0.4f, 0.05f, 1.0f); // Deep orange
		C.FlashIntensity = 0.7f;
		C.FlashDuration = 0.4f;
		C.VignetteColor = FLinearColor(1.0f, 0.2f, 0.0f, 1.0f); // Red-orange
		C.VignetteIntensity = 0.3f;
		C.VignetteDuration = 0.8f;
		C.BloomOverride = 3.0f;
		C.RumbleIntensity = 0.6f;
		C.RumbleDuration = 0.5f;
		C.bScreenDistortion = true;
		C.DistortionStrength = 0.02f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Pyromancy, C);
	}

	// ========== CRYOMANCY - Ice ==========
	// Frost vignette, blue tint, screen blur, gentle shake
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 0.5f;
		C.ShakeDuration = 0.3f;
		C.ShakeFrequency = 8.0f;
		C.bShakeRotation = false;
		C.FlashColor = FLinearColor(0.5f, 0.8f, 1.0f, 1.0f); // Ice blue
		C.FlashIntensity = 0.4f;
		C.FlashDuration = 0.5f;
		C.VignetteColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f); // Frost blue
		C.VignetteIntensity = 0.5f;
		C.VignetteDuration = 1.2f;
		C.MotionBlurOverride = 0.5f;
		C.RumbleIntensity = 0.2f;
		C.RumbleDuration = 0.4f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Cryomancy, C);
	}

	// ========== STORMCALLING - Lightning ==========
	// Sharp white flash, violent shake, electrical crackle feel
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 3.5f;
		C.ShakeDuration = 0.2f;
		C.ShakeFrequency = 40.0f;
		C.bShakeRotation = true;
		C.FlashColor = FLinearColor(0.9f, 0.95f, 1.0f, 1.0f); // White-blue
		C.FlashIntensity = 1.0f;
		C.FlashDuration = 0.15f;
		C.ChromaticAberration = 3.0f;
		C.BloomOverride = 5.0f;
		C.RumbleIntensity = 0.8f;
		C.RumbleDuration = 0.2f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Stormcalling, C);
	}

	// ========== TEMPEST - Wind/Earth ==========
	// HEAVY screen shake, sustained rumble, earthquake feel
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 5.0f; // MASSIVE
		C.ShakeDuration = 1.5f;  // Long lasting
		C.ShakeFrequency = 12.0f;
		C.bShakeRotation = true;
		C.FlashColor = FLinearColor(0.6f, 0.5f, 0.3f, 1.0f); // Dusty brown
		C.FlashIntensity = 0.3f;
		C.FlashDuration = 0.8f;
		C.VignetteColor = FLinearColor(0.4f, 0.35f, 0.2f, 1.0f); // Earth tone
		C.VignetteIntensity = 0.4f;
		C.VignetteDuration = 2.0f;
		C.MotionBlurOverride = 0.8f;
		C.bRadialBlur = true;
		C.RadialBlurStrength = 0.03f;
		C.RumbleIntensity = 1.0f; // FULL RUMBLE
		C.RumbleDuration = 1.5f;
		C.bScreenDistortion = true;
		C.DistortionStrength = 0.04f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Tempest, C);
	}

	// ========== VERDANCY - Nature ==========
	// Green mist, gentle sway, healing warmth
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 0.3f;
		C.ShakeDuration = 0.8f;
		C.ShakeFrequency = 3.0f; // Slow sway
		C.bShakeRotation = false;
		C.FlashColor = FLinearColor(0.2f, 0.9f, 0.3f, 1.0f); // Bright green
		C.FlashIntensity = 0.3f;
		C.FlashDuration = 0.6f;
		C.VignetteColor = FLinearColor(0.1f, 0.6f, 0.2f, 1.0f); // Forest green
		C.VignetteIntensity = 0.3f;
		C.VignetteDuration = 1.5f;
		C.BloomOverride = 1.5f;
		C.RumbleIntensity = 0.1f;
		C.RumbleDuration = 0.8f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Verdancy, C);
	}

	// ========== UMBRAMANCY - Shadow ==========
	// Dark vignette pulse, desaturation, creepy
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 1.0f;
		C.ShakeDuration = 0.4f;
		C.ShakeFrequency = 6.0f;
		C.bShakeRotation = true;
		C.FlashColor = FLinearColor(0.1f, 0.0f, 0.15f, 1.0f); // Dark purple
		C.FlashIntensity = 0.5f;
		C.FlashDuration = 0.6f;
		C.VignetteColor = FLinearColor(0.05f, 0.0f, 0.1f, 1.0f); // Near black purple
		C.VignetteIntensity = 0.7f; // Heavy vignette
		C.VignetteDuration = 1.0f;
		C.ChromaticAberration = 1.5f;
		C.RumbleIntensity = 0.4f;
		C.RumbleDuration = 0.5f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Umbramancy, C);
	}

	// ========== ARCANA - Arcane ==========
	// Purple shimmer, bloom burst, light shake
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 1.2f;
		C.ShakeDuration = 0.4f;
		C.ShakeFrequency = 18.0f;
		C.bShakeRotation = false;
		C.FlashColor = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f); // Bright purple
		C.FlashIntensity = 0.5f;
		C.FlashDuration = 0.3f;
		C.BloomOverride = 4.0f;
		C.ChromaticAberration = 0.8f;
		C.RumbleIntensity = 0.3f;
		C.RumbleDuration = 0.3f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Arcana, C);
	}

	// ========== RADIANCE - Holy ==========
	// Golden bloom burst, bright flash, warm glow
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 0.8f;
		C.ShakeDuration = 0.3f;
		C.ShakeFrequency = 12.0f;
		C.bShakeRotation = false;
		C.FlashColor = FLinearColor(1.0f, 0.9f, 0.5f, 1.0f); // Golden
		C.FlashIntensity = 0.8f;
		C.FlashDuration = 0.4f;
		C.VignetteColor = FLinearColor(1.0f, 0.85f, 0.4f, 1.0f); // Warm gold
		C.VignetteIntensity = 0.2f;
		C.VignetteDuration = 0.8f;
		C.BloomOverride = 6.0f; // MASSIVE bloom for holy
		C.RumbleIntensity = 0.3f;
		C.RumbleDuration = 0.3f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Radiance, C);
	}

	// ========== SANGROMANCY - Blood ==========
	// Red vignette pulse, heartbeat shake, bloody screen edges
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 1.5f;
		C.ShakeDuration = 0.8f;
		C.ShakeFrequency = 2.0f; // Slow heartbeat rhythm
		C.bShakeRotation = false;
		C.FlashColor = FLinearColor(0.8f, 0.0f, 0.0f, 1.0f); // Deep red
		C.FlashIntensity = 0.6f;
		C.FlashDuration = 0.5f;
		C.VignetteColor = FLinearColor(0.6f, 0.0f, 0.0f, 1.0f); // Blood red
		C.VignetteIntensity = 0.6f;
		C.VignetteDuration = 1.5f;
		C.ChromaticAberration = 1.0f;
		C.RumbleIntensity = 0.5f;
		C.RumbleDuration = 0.8f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Sangromancy, C);
	}

	// ========== DOMINION - Mind ==========
	// Chromatic aberration, warped distortion, psychedelic
	{
		FSpellScreenFXConfig C;
		C.ShakeIntensity = 2.0f;
		C.ShakeDuration = 0.6f;
		C.ShakeFrequency = 25.0f;
		C.bShakeRotation = true;
		C.FlashColor = FLinearColor(0.4f, 0.8f, 1.0f, 1.0f); // Psionic cyan
		C.FlashIntensity = 0.5f;
		C.FlashDuration = 0.4f;
		C.VignetteColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f); // Deep blue
		C.VignetteIntensity = 0.4f;
		C.VignetteDuration = 1.0f;
		C.ChromaticAberration = 5.0f; // HEAVY chromatic for mind spells
		C.BloomOverride = 2.0f;
		C.bScreenDistortion = true;
		C.DistortionStrength = 0.05f; // Warped reality
		C.bRadialBlur = true;
		C.RadialBlurStrength = 0.04f;
		C.RumbleIntensity = 0.6f;
		C.RumbleDuration = 0.6f;
		SchoolFXConfigs.Add(EAoCMagicSchool::Dominion, C);
	}
}

FSpellScreenFXConfig UAoCSpellScreenFX::GetConfig(EAoCMagicSchool School) const
{
	const FSpellScreenFXConfig* Found = SchoolFXConfigs.Find(School);
	if (Found) return *Found;
	// Default fallback
	FSpellScreenFXConfig Default;
	Default.FlashColor = FLinearColor(0.5f, 0.5f, 1.0f, 1.0f);
	return Default;
}

void UAoCSpellScreenFX::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateScreenFlash(DeltaTime);
	UpdateVignette(DeltaTime);
	UpdatePostProcess(DeltaTime);

	// Earthquake continuous shake
	if (EarthquakeTimer > 0.0f)
	{
		EarthquakeTimer -= DeltaTime;
		if (CameraManager)
		{
			// Continuous random offset for earthquake feel
			float Intensity = EarthquakeMagnitude * (EarthquakeTimer / FMath::Max(EarthquakeTimer + DeltaTime, 0.01f));
			FVector ShakeOffset;
			ShakeOffset.X = FMath::RandRange(-Intensity, Intensity) * 3.0f;
			ShakeOffset.Y = FMath::RandRange(-Intensity, Intensity) * 3.0f;
			ShakeOffset.Z = FMath::RandRange(-Intensity * 0.5f, Intensity * 0.5f);
			// Apply via camera location offset
			AActor* Owner = GetOwner();
			if (Owner)
			{
				FVector CurrentLoc = Owner->GetActorLocation();
				Owner->SetActorLocation(CurrentLoc + ShakeOffset * DeltaTime * 60.0f);
			}
		}
	}
}

// ============================================================
// MAIN API
// ============================================================

void UAoCSpellScreenFX::PlayCastScreenEffect(EAoCMagicSchool School, float PowerMultiplier)
{
	FSpellScreenFXConfig Config = GetConfig(School);

	// Camera shake
	ApplyCameraShake(Config, PowerMultiplier);

	// Screen flash
	StartScreenFlash(Config.FlashColor, Config.FlashIntensity * PowerMultiplier, Config.FlashDuration);

	// Vignette
	if (Config.VignetteIntensity > 0.0f)
	{
		StartVignette(Config.VignetteColor, Config.VignetteIntensity * PowerMultiplier, Config.VignetteDuration);
	}

	// Post-process overrides (bloom, chromatic aberration)
	ApplyPostProcessOverride(Config, PowerMultiplier);

	// Controller rumble
	if (Config.RumbleIntensity > 0.0f)
	{
		ApplyControllerRumble(Config.RumbleIntensity * PowerMultiplier, Config.RumbleDuration);
	}

	UE_LOG(LogTemp, Log, TEXT("SpellScreenFX: Cast effect for school %d (power=%.1f)"),
		(int32)School, PowerMultiplier);
}

void UAoCSpellScreenFX::PlayImpactScreenEffect(EAoCMagicSchool School, float PowerMultiplier)
{
	// Impact effects are stronger versions of cast effects
	PlayCastScreenEffect(School, PowerMultiplier * 1.5f);
}

// ============================================================
// SPECIAL NAMED EFFECTS
// ============================================================

void UAoCSpellScreenFX::PlayEarthquakeEffect(float Magnitude, float Duration)
{
	EarthquakeMagnitude = Magnitude;
	EarthquakeTimer = Duration;

	// Also trigger heavy camera shake
	FSpellScreenFXConfig EQ;
	EQ.ShakeIntensity = Magnitude * 8.0f;
	EQ.ShakeDuration = Duration;
	EQ.ShakeFrequency = 10.0f;
	EQ.bShakeRotation = true;
	ApplyCameraShake(EQ, 1.0f);

	// Brown dust flash
	StartScreenFlash(FLinearColor(0.5f, 0.4f, 0.2f, 1.0f), 0.4f, Duration * 0.5f);

	// Heavy rumble
	ApplyControllerRumble(1.0f, Duration);

	UE_LOG(LogTemp, Log, TEXT("SpellScreenFX: EARTHQUAKE! Magnitude=%.1f Duration=%.1f"), Magnitude, Duration);
}

void UAoCSpellScreenFX::PlayFrostScreenEffect(float Intensity, float Duration)
{
	StartVignette(FLinearColor(0.3f, 0.6f, 1.0f, 1.0f), Intensity * 0.6f, Duration);
	StartScreenFlash(FLinearColor(0.7f, 0.9f, 1.0f, 1.0f), Intensity * 0.3f, Duration * 0.3f);
}

void UAoCSpellScreenFX::PlayFireHeatDistortion(float Intensity, float Duration)
{
	StartVignette(FLinearColor(1.0f, 0.3f, 0.0f, 1.0f), Intensity * 0.4f, Duration);
	StartScreenFlash(FLinearColor(1.0f, 0.5f, 0.1f, 1.0f), Intensity * 0.5f, Duration * 0.2f);

	// Bloom burst for heat
	BloomTimer = Duration;
	BloomDuration = Duration;
	CurrentBloomOverride = Intensity * 4.0f;
}

void UAoCSpellScreenFX::PlayLightningFlash(float Intensity)
{
	// Sharp white flash - very bright, very fast
	StartScreenFlash(FLinearColor(0.95f, 0.97f, 1.0f, 1.0f), Intensity, 0.08f);

	// Instant heavy chromatic aberration that fades fast
	ChromaticTimer = 0.3f;
	ChromaticDuration = 0.3f;
	TargetChromatic = Intensity * 4.0f;
	CurrentChromatic = TargetChromatic;

	// Massive bloom spike
	BloomTimer = 0.15f;
	BloomDuration = 0.15f;
	CurrentBloomOverride = Intensity * 8.0f;
}

void UAoCSpellScreenFX::PlayBloodPulse(float Intensity, float Duration)
{
	// Heartbeat-like pulsing red vignette
	StartVignette(FLinearColor(0.7f, 0.0f, 0.0f, 1.0f), Intensity * 0.7f, Duration);
	StartScreenFlash(FLinearColor(0.9f, 0.0f, 0.0f, 1.0f), Intensity * 0.4f, Duration * 0.3f);

	// Chromatic for unsettling feel
	ChromaticTimer = Duration * 0.5f;
	ChromaticDuration = Duration * 0.5f;
	TargetChromatic = Intensity * 2.0f;
	CurrentChromatic = TargetChromatic;
}

void UAoCSpellScreenFX::PlayMindWarp(float Intensity, float Duration)
{
	// Psychedelic - heavy chromatic + distortion
	ChromaticTimer = Duration;
	ChromaticDuration = Duration;
	TargetChromatic = Intensity * 6.0f;
	CurrentChromatic = TargetChromatic;

	StartVignette(FLinearColor(0.3f, 0.5f, 1.0f, 1.0f), Intensity * 0.5f, Duration);
	StartScreenFlash(FLinearColor(0.5f, 0.8f, 1.0f, 1.0f), Intensity * 0.3f, 0.2f);

	// Camera shake with high frequency for disorienting feel
	FSpellScreenFXConfig Mind;
	Mind.ShakeIntensity = Intensity * 3.0f;
	Mind.ShakeDuration = Duration;
	Mind.ShakeFrequency = 30.0f;
	Mind.bShakeRotation = true;
	ApplyCameraShake(Mind, 1.0f);
}

// ============================================================
// CAMERA SHAKE
// ============================================================

void UAoCSpellScreenFX::ApplyCameraShake(const FSpellScreenFXConfig& Config, float PowerMult)
{
	if (!CameraManager) return;
	if (Config.ShakeIntensity <= 0.0f) return;

	float ScaledIntensity = Config.ShakeIntensity * PowerMult;

	// Use UE5's built-in camera shake via StartCameraShake
	// We create a simple oscillating shake using the legacy MatineeCameraShake
	// For maximum compatibility, use the PlayerCameraManager shake API
	CameraManager->StartCameraShake(
		UCameraShakeBase::StaticClass(),
		ScaledIntensity,
		ECameraShakePlaySpace::CameraLocal
	);

	// Manual shake fallback - directly offset camera for immediate feedback
	if (PlayerController)
	{
		FRotator ShakeRot;
		ShakeRot.Pitch = FMath::RandRange(-ScaledIntensity, ScaledIntensity) * 0.5f;
		ShakeRot.Yaw = FMath::RandRange(-ScaledIntensity, ScaledIntensity) * 0.3f;
		ShakeRot.Roll = Config.bShakeRotation ? FMath::RandRange(-ScaledIntensity, ScaledIntensity) * 0.2f : 0.0f;

		PlayerController->SetControlRotation(PlayerController->GetControlRotation() + ShakeRot);
	}

	UE_LOG(LogTemp, Verbose, TEXT("SpellScreenFX: Camera shake intensity=%.1f duration=%.1f"),
		ScaledIntensity, Config.ShakeDuration);
}

// ============================================================
// SCREEN FLASH
// ============================================================

void UAoCSpellScreenFX::StartScreenFlash(FLinearColor Color, float Intensity, float Duration)
{
	CurrentFlashColor = Color;
	ScreenFlashAlpha = Intensity;
	ScreenFlashTimer = Duration;
	ScreenFlashDuration = Duration;
}

void UAoCSpellScreenFX::UpdateScreenFlash(float DeltaTime)
{
	if (ScreenFlashTimer <= 0.0f) return;

	ScreenFlashTimer -= DeltaTime;
	float T = FMath::Clamp(ScreenFlashTimer / FMath::Max(ScreenFlashDuration, 0.01f), 0.0f, 1.0f);

	// Ease out - flash fades
	float Alpha = ScreenFlashAlpha * T * T;

	// Apply via post-process color grading
	if (ScreenFXPostProcess)
	{
		FPostProcessSettings& PP = ScreenFXPostProcess->Settings;
		PP.bOverride_SceneColorTint = true;
		FLinearColor Tint = FLinearColor::White + (CurrentFlashColor - FLinearColor::White) * Alpha;
		PP.SceneColorTint = Tint;
		ScreenFXPostProcess->BlendWeight = FMath::Max(ScreenFXPostProcess->BlendWeight, Alpha);
	}

	if (ScreenFlashTimer <= 0.0f)
	{
		ScreenFlashAlpha = 0.0f;
	}
}

// ============================================================
// VIGNETTE
// ============================================================

void UAoCSpellScreenFX::StartVignette(FLinearColor Color, float Intensity, float Duration)
{
	CurrentVignetteColor = Color;
	VignetteAlpha = Intensity;
	VignetteTimer = Duration;
	VignetteDuration = Duration;
}

void UAoCSpellScreenFX::UpdateVignette(float DeltaTime)
{
	if (VignetteTimer <= 0.0f) return;

	VignetteTimer -= DeltaTime;
	float T = FMath::Clamp(VignetteTimer / FMath::Max(VignetteDuration, 0.01f), 0.0f, 1.0f);
	float Alpha = VignetteAlpha * T;

	if (ScreenFXPostProcess)
	{
		FPostProcessSettings& PP = ScreenFXPostProcess->Settings;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = Alpha;
	}

	if (VignetteTimer <= 0.0f)
	{
		VignetteAlpha = 0.0f;
	}
}

// ============================================================
// POST-PROCESS OVERRIDES
// ============================================================

void UAoCSpellScreenFX::ApplyPostProcessOverride(const FSpellScreenFXConfig& Config, float PowerMult)
{
	if (!ScreenFXPostProcess) return;

	FPostProcessSettings& PP = ScreenFXPostProcess->Settings;

	// Chromatic aberration
	if (Config.ChromaticAberration > 0.0f)
	{
		ChromaticTimer = Config.ShakeDuration;
		ChromaticDuration = Config.ShakeDuration;
		TargetChromatic = Config.ChromaticAberration * PowerMult;
		CurrentChromatic = TargetChromatic;
	}

	// Bloom override
	if (Config.BloomOverride > 0.0f)
	{
		BloomTimer = Config.FlashDuration;
		BloomDuration = Config.FlashDuration;
		CurrentBloomOverride = Config.BloomOverride * PowerMult;
	}

	ScreenFXPostProcess->BlendWeight = 1.0f;
}

void UAoCSpellScreenFX::UpdatePostProcess(float DeltaTime)
{
	if (!ScreenFXPostProcess) return;

	FPostProcessSettings& PP = ScreenFXPostProcess->Settings;
	bool bAnyActive = false;

	// Chromatic aberration fade
	if (ChromaticTimer > 0.0f)
	{
		ChromaticTimer -= DeltaTime;
		float T = FMath::Clamp(ChromaticTimer / FMath::Max(ChromaticDuration, 0.01f), 0.0f, 1.0f);
		float Value = CurrentChromatic * T;
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = Value;
		bAnyActive = true;
	}
	else if (PP.bOverride_SceneFringeIntensity)
	{
		PP.bOverride_SceneFringeIntensity = false;
		PP.SceneFringeIntensity = 0.0f;
	}

	// Bloom fade
	if (BloomTimer > 0.0f)
	{
		BloomTimer -= DeltaTime;
		float T = FMath::Clamp(BloomTimer / FMath::Max(BloomDuration, 0.01f), 0.0f, 1.0f);
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = CurrentBloomOverride * T;
		bAnyActive = true;
	}
	else if (PP.bOverride_BloomIntensity)
	{
		PP.bOverride_BloomIntensity = false;
	}

	// Update blend weight
	if (ScreenFlashTimer > 0.0f || VignetteTimer > 0.0f || bAnyActive)
	{
		ScreenFXPostProcess->BlendWeight = 1.0f;
	}
	else
	{
		ScreenFXPostProcess->BlendWeight = 0.0f;
	}
}

void UAoCSpellScreenFX::ResetPostProcess()
{
	if (!ScreenFXPostProcess) return;

	FPostProcessSettings& PP = ScreenFXPostProcess->Settings;
	PP.bOverride_SceneColorTint = false;
	PP.bOverride_VignetteIntensity = false;
	PP.bOverride_SceneFringeIntensity = false;
	PP.bOverride_BloomIntensity = false;
	ScreenFXPostProcess->BlendWeight = 0.0f;

	ScreenFlashTimer = 0.0f;
	VignetteTimer = 0.0f;
	ChromaticTimer = 0.0f;
	BloomTimer = 0.0f;
	EarthquakeTimer = 0.0f;
}

// ============================================================
// CONTROLLER RUMBLE
// ============================================================

void UAoCSpellScreenFX::ApplyControllerRumble(float Intensity, float Duration)
{
	if (!PlayerController) return;

	// Use ForceFeedback for proper controller rumble
	PlayerController->PlayDynamicForceFeedback(
		Intensity,      // Intensity
		Duration,       // Duration
		true,           // bAffectsLeftLarge
		true,           // bAffectsLeftSmall
		true,           // bAffectsRightLarge
		true            // bAffectsRightSmall
	);
}
