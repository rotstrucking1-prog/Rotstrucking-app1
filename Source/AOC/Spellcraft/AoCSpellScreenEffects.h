// Source/AOC/Spellcraft/AoCSpellScreenEffects.h
// v29 — Per-school camera shake, screen flash, chromatic aberration, vignette.
// Triggered by SpellCastingComponent when spells fire.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSpellData.h"
#include "AoCSpellScreenEffects.generated.h"

class APlayerCameraManager;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Per-school screen effect configuration
 */
USTRUCT()
struct FSchoolScreenEffect
{
	GENERATED_BODY()

	/** Camera shake intensity (0 = none, 1 = earthquake) */
	float ShakeIntensity = 0.3f;

	/** Duration of camera shake */
	float ShakeDuration = 0.5f;

	/** Screen flash color */
	FLinearColor FlashColor = FLinearColor::White;

	/** Flash intensity (0-1) */
	float FlashIntensity = 0.3f;

	/** Flash duration */
	float FlashDuration = 0.15f;

	/** Chromatic aberration intensity (0-1) */
	float ChromaticAberration = 0.0f;

	/** Vignette intensity */
	float VignetteIntensity = 0.0f;

	/** Radial blur amount */
	float RadialBlur = 0.0f;
};

/**
 * Manages screen post-process effects per spell school.
 * Supports camera shake, screen flash, chromatic aberration, and vignette.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSpellScreenEffects : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSpellScreenEffects();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Trigger screen effects for a spell cast */
	UFUNCTION(BlueprintCallable, Category = "AoC|ScreenFX")
	void TriggerSpellEffect(EAoCMagicSchool School, float IntensityScale = 1.0f);

private:
	/** Per-school effect configs */
	TMap<EAoCMagicSchool, FSchoolScreenEffect> SchoolEffects;

	void InitSchoolEffects();

	/** Apply camera shake */
	void ApplyCameraShake(float Intensity, float Duration);

	/** Active flash state */
	bool bFlashing;
	float FlashTimeRemaining;
	float FlashMaxTime;
	FLinearColor CurrentFlashColor;
	float CurrentFlashIntensity;

	/** Active post-process state */
	float ActiveChromaticAberration;
	float ActiveVignette;
	float ActiveRadialBlur;
	float PostProcessFadeTime;
	float PostProcessFadeRemaining;
};
