// AoCPlayerPawn.h
// Architect of Creation — Player Pawn
// Complete player character with:
//   - Third-person camera (boom + follow)
//   - WASD movement with fully rebindable keys (saved to config)
//   - Interaction system (raycast-based, context-sensitive)
//   - Terraform, Mining, Smelting, Gathering, Herbalism modes
//   - Animation playback for every interaction action
//   - On-screen HUD for target info and action menus

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "AoCPlayerPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAnimSequence;
class UAoCMiningComponent;
class UAoCProspectingComponent;
class UAoCSmeltingComponent;
class UAoCTerraformingComponent;
class UAoCGatheringComponent;
class UAoCHerbalismComponent;
class UAoCInventoryComponent;
class UAoCSkillComponent;
class UAoCHUDWidget;

/** What the player's crosshair is currently pointing at. */
UENUM(BlueprintType)
enum class EInteractionTarget : uint8
{
	Nothing        UMETA(DisplayName = "Nothing"),
	Ground         UMETA(DisplayName = "Ground"),
	OreVein        UMETA(DisplayName = "Ore Vein"),
	Furnace        UMETA(DisplayName = "Furnace"),
	HerbBush       UMETA(DisplayName = "Herb Bush"),
	ResourceNode   UMETA(DisplayName = "Resource Node"),
	Creature       UMETA(DisplayName = "Creature"),
	NPC            UMETA(DisplayName = "NPC")
};

/** Current player UI/interaction mode. */
UENUM(BlueprintType)
enum class EInteractionMode : uint8
{
	Default        UMETA(DisplayName = "Default"),
	TerraformMenu  UMETA(DisplayName = "Terraform"),
	MiningMenu     UMETA(DisplayName = "Mining")
};

/**
 * Main player character for Architect of Creation.
 *
 * Third-person camera with controller rotation.
 * All keys are rebindable and saved to AoCKeyBindings.ini.
 * Interaction is raycast-based: look at something and press [E].
 * Sub-menus (Terraform, Mining) use number keys for actions.
 * Every interaction triggers a matching animation on the PeasantMan.
 */
UCLASS()
class AOC_API AAoCPlayerPawn : public ACharacter
{
	GENERATED_BODY()

public:
	AAoCPlayerPawn();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Camera ──────────────────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;

	// ── Gameplay Components ─────────────────────────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCMiningComponent* MiningComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCProspectingComponent* ProspectingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCSmeltingComponent* SmeltingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCTerraformingComponent* TerraformingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCGatheringComponent* GatheringComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCHerbalismComponent* HerbalismComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCSkillComponent* SkillComponent;

	// ── HUD Widget reference ────────────────────────────────────────────────

	UPROPERTY()
	UAoCHUDWidget* GameHUD = nullptr;

	// ── Animations (loaded at BeginPlay) ────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimIdle;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimMiningSwing;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimDigging;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimGatherHerb;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimPickupItem;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimProspecting;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimObserveGround;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimSmelting;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimWalk;

	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	UAnimSequence* AnimRun;

	/** True while a one-shot action animation is playing. */
	UPROPERTY(BlueprintReadOnly, Category = "Animation")
	bool bPlayingActionAnim;

	// ── Interaction System ──────────────────────────────────────────────────

	/** Max raycast distance for interaction detection (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionRange;

	/** What the player is currently looking at. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	EInteractionTarget CurrentTarget;

	/** Current interaction/UI mode. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	EInteractionMode CurrentMode;

	/** The actor currently under the crosshair (may be null). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> TargetActor;

	/** World-space hit point from the interaction trace. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	FVector TargetLocation;

	/** Surface normal at the hit point. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	FVector TargetNormal;

	/** Descriptive name of the current target (shown on HUD). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	FString TargetDisplayName;

	// ── Key Bindings (ALL REBINDABLE) ───────────────────────────────────────

	/** Current key bindings — action name to assigned key. Fully rebindable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Bindings")
	TMap<FName, FKey> KeyBindings;

	/** Rebind an action to a new key. Persists to config file. */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	void RebindKey(FName ActionName, FKey NewKey);

	/** Get the currently assigned key for an action. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Input|Bindings")
	FKey GetKeyForAction(FName ActionName) const;

	/** Restore all bindings to factory defaults. */
	UFUNCTION(BlueprintCallable, Category = "Input|Bindings")
	void ResetKeysToDefaults();

	/** Get all action names (for building a key-rebind settings UI). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Input|Bindings")
	TArray<FName> GetAllActionNames() const;

	// ── Animation Control ───────────────────────────────────────────────────

	/** Play a one-shot action animation, then auto-return to idle. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void PlayActionAnimation(UAnimSequence* Anim, float OverrideDuration = -1.0f);

	/** Immediately return to idle animation. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void ReturnToIdle();

private:
	// Key binding internals
	void SetupDefaultKeyBindings();
	void LoadKeyBindings();
	void SaveKeyBindings();

	// Animation loading
	void LoadAllAnimations();
	UAnimSequence* TryLoadAnim(const TCHAR* Path);

	// Per-frame processing
	void ProcessMovement(APlayerController* PC);
	void ProcessCamera(APlayerController* PC);
	void ProcessInteractionInput(APlayerController* PC);
	void UpdateInteractionTarget();

	// Raycasting
	FHitResult PerformInteractionTrace();
	EInteractionTarget ClassifyActor(AActor* Actor);
	FString BuildTargetName(AActor* Actor, EInteractionTarget Type);

	// Action handlers
	void HandleInteract();
	void HandleObserveGround();
	void HandleTerraformMenu();
	void HandleMiningMenu();
	void HandleProspect();
	void HandleInventoryToggle();
	void HandleSkillsToggle();
	void HandleCancel();
	void HandleActionSlot(int32 Slot);

	// Action execution
	void ExecuteTerraformAction(int32 ActionIndex);
	void ExecuteMiningAction(int32 ActionIndex);

	// HUD rendering
	void DisplayHUD();
	void ShowNotification(const FString& Message, FColor Color = FColor::White, float Duration = 3.0f);

	// Look-at tooltip (clean AoC-style description when looking at world objects)
	void UpdateLookAtTooltip();
	void CreateHUDWidget();

	UPROPERTY()
	TWeakObjectPtr<AActor> LastLookAtActor;

	// State
	float ActionCooldown;
	float NotificationTimer;
	FString NotificationText;
	FColor NotificationColor;
	bool bShowingInventory;
	bool bShowingSkills;

	// Animation timer for returning to idle
	FTimerHandle AnimReturnTimerHandle;
};
