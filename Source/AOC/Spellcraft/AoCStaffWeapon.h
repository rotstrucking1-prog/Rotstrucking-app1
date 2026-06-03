// Source/AOC/Spellcraft/AoCStaffWeapon.h
// Procedural staff weapon — cylinder shaft + sphere gem, school-coloured glow.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCSpellData.h"
#include "AoCStaffWeapon.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * A procedurally constructed staff / wand weapon.
 * Uses engine basic shapes (cylinder + sphere) — no external mesh assets required.
 * The gem at the top glows with the active school's colour and pulses during casting.
 */
UCLASS()
class AOC_API AAoCStaffWeapon : public AActor
{
	GENERATED_BODY()

public:
	AAoCStaffWeapon();

	// ── Components ──────────────────────────────────────────────────────

	/** The staff shaft (stretched cylinder) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ShaftMesh;

	/** The gem orb at the top (sphere) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* GemMesh;

	/** Glow light emanating from the gem */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* GemLight;

	// ── State ───────────────────────────────────────────────────────────

	/** Currently active magic school (determines gem colour) */
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	EAoCMagicSchool ActiveSchool;

	/** Is the owner currently casting? (drives pulse animation) */
	UPROPERTY(BlueprintReadOnly, Category = "Staff")
	bool bIsCasting;

	// ── Public interface ────────────────────────────────────────────────

	/** Attach the staff to a character's hand socket */
	UFUNCTION(BlueprintCallable, Category = "AoC|Staff")
	void AttachToCharacter(ACharacter* Character, FName SocketName = TEXT("hand_r"));

	/** Change the gem/light colour to match a new school */
	UFUNCTION(BlueprintCallable, Category = "AoC|Staff")
	void SetActiveSchool(EAoCMagicSchool NewSchool);

	/** Enable/disable the casting pulse effect */
	UFUNCTION(BlueprintCallable, Category = "AoC|Staff")
	void SetCasting(bool bNewCasting);

	virtual void Tick(float DeltaTime) override;

private:
	/** Dynamic material on the gem */
	UPROPERTY()
	UMaterialInstanceDynamic* GemMaterialInstance;

	/** Base glow intensity (set per-school) */
	float BaseGlowIntensity;

	/** Accumulated time for pulse animation */
	float PulseTimer;

	/** Update the gem material and light to match the active school */
	void ApplySchoolVisuals();
};
