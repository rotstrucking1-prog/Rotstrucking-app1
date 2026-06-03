// AoCTerraformingComponent.h
// Architect of Creation - Terraforming Component
// Gives a character Life-is-Feudal-style terraforming abilities.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCTerraformingComponent.generated.h"

class AAoCTerrainTile;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ETerraformAction : uint8
{
	LowerGround			UMETA(DisplayName = "Lower Ground"),
	RaiseGround			UMETA(DisplayName = "Raise Ground"),
	Flatten				UMETA(DisplayName = "Flatten"),
	FlattenSlopeUp		UMETA(DisplayName = "Flatten Slope Up"),
	FlattenSlopeDown	UMETA(DisplayName = "Flatten Slope Down"),
	Observe				UMETA(DisplayName = "Observe")
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerraformCompleteDelegate, ETerraformAction, Action, FVector, Location);

// ─── Component ───────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCTerraformingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCTerraformingComponent();

	virtual void BeginPlay() override;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Terraforming skill level (0-100). Higher = faster & cheaper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terraforming", meta = (ClampMin = "0", ClampMax = "100"))
	float TerraformSkill;

	/** Size of each terrain tile in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terraforming")
	float TileSize;

	/** How much height changes per single action (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terraforming")
	float HeightStep;

	/** Maximum slope angle allowed (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terraforming")
	float MaxSlopeAngle;

	/** True while player is in top-down observe mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Terraforming")
	bool bIsObserving;

	/** True while a terraform action is in progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Terraforming")
	bool bIsTerraforming;

	/** The action currently being performed. */
	UPROPERTY(BlueprintReadOnly, Category = "Terraforming")
	ETerraformAction CurrentAction;

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Terraforming|Events")
	FOnTerraformCompleteDelegate OnTerraformComplete;

	// ── Functions ───────────────────────────────────────────────────────────

	/** Begin a terraforming action at TargetLocation. Returns false if requirements not met. */
	UFUNCTION(BlueprintCallable, Category = "Terraforming")
	bool StartTerraform(ETerraformAction Action, FVector TargetLocation);

	/** Cancel current terraform action. */
	UFUNCTION(BlueprintCallable, Category = "Terraforming")
	void CancelTerraform();

	/** Enter top-down camera observation mode. */
	UFUNCTION(BlueprintCallable, Category = "Terraforming")
	void EnterObserveMode();

	/** Exit observe mode back to normal camera. */
	UFUNCTION(BlueprintCallable, Category = "Terraforming")
	void ExitObserveMode();

	/** Time (seconds) for the current action. Lerps from 30s (skill 0) to 5s (skill 100). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terraforming")
	float GetActionTime() const;

	/** Stamina cost for one action, decreases with skill. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terraforming")
	float GetStaminaCost() const;

	/** Add terraforming XP — increases skill over time. */
	UFUNCTION(BlueprintCallable, Category = "Terraforming")
	void GainTerraformXP(float Amount);

	/** Placeholder — returns true. Override to check equipped tool. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Terraforming")
	bool HasShovelEquipped() const;

	/** Find an existing AAoCTerrainTile at the grid-snapped location, or spawn a new one. */
	UFUNCTION(BlueprintCallable, Category = "Terraforming")
	AAoCTerrainTile* FindOrCreateTileAt(FVector Location);

protected:
	/** Called when the timer completes — applies the terrain change. */
	void OnTerraformActionComplete();

	/** Lower the nearest height point on the tile. */
	void ApplyLowerGround(AAoCTerrainTile* Tile, FVector HitPoint);

	/** Raise the nearest height point on the tile. */
	void ApplyRaiseGround(AAoCTerrainTile* Tile, FVector HitPoint);

	/** Flatten surrounding points to their average height. */
	void ApplyFlatten(AAoCTerrainTile* Tile, FVector HitPoint);

private:
	FTimerHandle TerraformTimer;

	/** Cached target location for the in-progress action. */
	FVector PendingTargetLocation;

	/** Accumulated XP toward next skill point. */
	float TerraformXP;

	/** XP needed per skill point. */
	static constexpr float XPPerSkillPoint = 100.f;
};
