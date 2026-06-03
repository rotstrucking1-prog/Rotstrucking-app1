// AoCSpellScreenFX.h - AAA Screen Effects for Spell Casting
// Camera shakes, post-process overlays, controller rumble, screen distortion
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Materials/MaterialParameterCollection.h"
#include "AoCSpellData.h"
#include "AoCSpellScreenFX.generated.h"

// Per-school screen effect configuration
USTRUCT(BlueprintType)
struct FSpellScreenFXConfig
{
	GENERATED_BODY()

	// Camera shake
	UPROPERTY(EditAnywhere) float ShakeIntensity = 1.0f;
	UPROPERTY(EditAnywhere) float ShakeDuration = 0.5f;
	UPROPERTY(EditAnywhere) float ShakeFrequency = 15.0f;
	UPROPERTY(EditAnywhere) bool bShakeRotation = true;

	// Screen flash
	UPROPERTY(EditAnywhere) FLinearColor FlashColor = FLinearColor::White;
	UPROPERTY(EditAnywhere) float FlashIntensity = 0.5f;
	UPROPERTY(EditAnywhere) float FlashDuration = 0.3f;

	// Vignette overlay
	UPROPERTY(EditAnywhere) FLinearColor VignetteColor = FLinearColor::Black;
	UPROPERTY(EditAnywhere) float VignetteIntensity = 0.0f;
	UPROPERTY(EditAnywhere) float VignetteDuration = 0.5f;

	// Post-process overrides
	UPROPERTY(EditAnywhere) float ChromaticAberration = 0.0f;
	UPROPERTY(EditAnywhere) float BloomOverride = 0.0f;
	UPROPERTY(EditAnywhere) float MotionBlurOverride = 0.0f;

	// Controller rumble
	UPROPERTY(EditAnywhere) float RumbleIntensity = 0.3f;
	UPROPERTY(EditAnywhere) float RumbleDuration = 0.3f;

	// Special effects
	UPROPERTY(EditAnywhere) bool bRadialBlur = false;
	UPROPERTY(EditAnywhere) float RadialBlurStrength = 0.0f;
	UPROPERTY(EditAnywhere) bool bScreenDistortion = false;
	UPROPERTY(EditAnywhere) float DistortionStrength = 0.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSpellScreenFX : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSpellScreenFX();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Main API - called when spells are cast
	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayCastScreenEffect(EAoCMagicSchool School, float PowerMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayImpactScreenEffect(EAoCMagicSchool School, float PowerMultiplier = 1.0f);

	// Special named effects
	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayEarthquakeEffect(float Magnitude, float Duration);

	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayFrostScreenEffect(float Intensity, float Duration);

	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayFireHeatDistortion(float Intensity, float Duration);

	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayLightningFlash(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayBloodPulse(float Intensity, float Duration);

	UFUNCTION(BlueprintCallable, Category = "SpellFX")
	void PlayMindWarp(float Intensity, float Duration);

private:
	void InitSchoolConfigs();
	FSpellScreenFXConfig GetConfig(EAoCMagicSchool School) const;

	// Camera shake
	void ApplyCameraShake(const FSpellScreenFXConfig& Config, float PowerMult);

	// Screen flash overlay
	void StartScreenFlash(FLinearColor Color, float Intensity, float Duration);
	void UpdateScreenFlash(float DeltaTime);

	// Vignette
	void StartVignette(FLinearColor Color, float Intensity, float Duration);
	void UpdateVignette(float DeltaTime);

	// Post-process manipulation
	void ApplyPostProcessOverride(const FSpellScreenFXConfig& Config, float PowerMult);
	void UpdatePostProcess(float DeltaTime);
	void ResetPostProcess();

	// Controller rumble
	void ApplyControllerRumble(float Intensity, float Duration);

	// Per-school configs
	UPROPERTY() TMap<EAoCMagicSchool, FSpellScreenFXConfig> SchoolFXConfigs;

	// Active effects state
	float ScreenFlashAlpha = 0.0f;
	float ScreenFlashTimer = 0.0f;
	float ScreenFlashDuration = 0.0f;
	FLinearColor CurrentFlashColor;

	float VignetteAlpha = 0.0f;
	float VignetteTimer = 0.0f;
	float VignetteDuration = 0.0f;
	FLinearColor CurrentVignetteColor;

	float ChromaticTimer = 0.0f;
	float ChromaticDuration = 0.0f;
	float CurrentChromatic = 0.0f;
	float TargetChromatic = 0.0f;

	float BloomTimer = 0.0f;
	float BloomDuration = 0.0f;
	float CurrentBloomOverride = 0.0f;

	// Earthquake state
	float EarthquakeTimer = 0.0f;
	float EarthquakeMagnitude = 0.0f;

	// Reference to player camera manager
	UPROPERTY() APlayerCameraManager* CameraManager;
	UPROPERTY() APlayerController* PlayerController;

	// Post-process component for screen overlays
	UPROPERTY() class UPostProcessComponent* ScreenFXPostProcess;
};
