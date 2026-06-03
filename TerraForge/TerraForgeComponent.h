// TerraForgeComponent.h
// TerraForge — Player component for terraforming interactions.
// Handles context menu, observe mode, dig/raise/flatten actions,
// tool validation, structural warnings, and dirt inventory.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TerraForgeTypes.h"
#include "TerraForgeComponent.generated.h"

class UTerraForgeSubsystem;

/** Delegate for UI to react to terraforming events. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerraformAction, ETerraAction, Action, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObserveModeChanged, bool, bEnabled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStructuralWarning, EStructuralState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContextMenuRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProspectingComplete, const TArray<FProspectingResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGemFound, const FGemDropResult&, GemResult);

/**
 * Player component for TerraForge terrain interaction.
 *
 * Attach to the player pawn. Handles:
 * - Context menu (right-click with shovel/pickaxe equipped)
 * - Observe mode (overhead camera, grid overlay, elevation data)
 * - Terraforming actions (lower, raise, flatten, slope)
 * - Tunnel digging
 * - Prospecting
 * - Mine support placement
 * - Dirt/material inventory tracking
 * - Structural integrity warnings
 * - Action cooldowns and tool validation
 */
UCLASS(ClassGroup=(TerraForge), meta=(BlueprintSpawnableComponent))
class AOC_API UTerraForgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTerraForgeComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ── Context Menu ────────────────────────────────────────────────────────

	/** Open the terraforming context menu at the current look-at position. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	void OpenContextMenu();

	/** Close the context menu. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	void CloseContextMenu();

	/** Is the context menu currently open? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	bool IsContextMenuOpen() const { return bContextMenuOpen; }

	/** Get available actions for the current target (depends on tool + material). */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	TArray<ETerraAction> GetAvailableActions() const;

	/** Get the display name for an action. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	static FText GetActionDisplayName(ETerraAction Action);

	/** Get the required tool for an action. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	static ETerraToolType GetRequiredTool(ETerraAction Action);

	// ── Execute Actions ─────────────────────────────────────────────────────

	/** Execute a terraforming action at the current target position. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	bool ExecuteAction(ETerraAction Action);

	/** Execute a terraforming action at a specific world location.
	 *  Bypasses the camera line trace — used for testing and server-authoritative calls.
	 *  @param WorldLocation  World-space position to perform the action at.
	 *  @param Action         The terraform action to execute.
	 *  @return True if the action was performed successfully. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	bool ExecuteActionAtLocation(FVector WorldLocation, ETerraAction Action);

	/** Select an action by number (1-6, for number key hotbar). */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	bool ExecuteActionBySlot(int32 Slot);

	// ── Edit Session (delegates to subsystem) ───────────────────────────────

	/** Begin an edit session around the player's current position.
	 *  Locks heightmap textures for rapid dig/raise/flatten without GPU thrash.
	 *  @param Radius  World-space radius to pre-lock (default 10m). */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	bool BeginEditSession(float Radius = 1000.0f);

	/** End the current edit session, committing all changes to GPU and collision. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	void EndEditSession();

	// ── Observe Mode ────────────────────────────────────────────────────────

	/** Toggle observe mode on/off. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	void ToggleObserveMode();

	/** Is observe mode active? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	bool IsObserveModeActive() const { return bObserveModeActive; }

	/** Get the current observe grid data. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	TArray<FObserveTile> GetObserveGrid() const;

	// ── Prospecting ─────────────────────────────────────────────────────────

	/** Start a prospecting action. */
	UFUNCTION(BlueprintCallable, Category = "TerraForge")
	void StartProspecting();

	/** Get the last prospecting results. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	const TArray<FProspectingResult>& GetLastProspectingResults() const { return LastProspectResults; }

	// ── Dirt/Material Inventory ─────────────────────────────────────────────

	/** Amount of dirt/material currently carried (in units). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Inventory")
	int32 CarriedDirtUnits = 0;

	/** Type of material currently carried. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Inventory")
	EGeoMaterial CarriedMaterialType = EGeoMaterial::Air;

	/** Maximum dirt units the player can carry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Inventory")
	int32 MaxDirtCapacity = 100;

	/** Current carry weight from dirt (kg). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge|Inventory")
	float GetDirtWeight() const;

	/** Is the player overburdened? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge|Inventory")
	bool IsOverburdened() const;

	// ── Tool State ──────────────────────────────────────────────────────────

	/** Currently equipped tool type. Set by the inventory/equipment system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Tools")
	ETerraToolType EquippedTool = ETerraToolType::Shovel; // Default for testing

	/** Is the correct tool equipped for the given action? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	bool HasCorrectTool(ETerraAction Action) const;

	// ── Target Info ─────────────────────────────────────────────────────────

	/** Current target position (world space, from player raycast). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Target")
	FVector TargetPosition = FVector::ZeroVector;

	/** Current target surface normal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Target")
	FVector TargetNormal = FVector::UpVector;

	/** Surface material at the target position. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Target")
	EGeoMaterial TargetMaterial = EGeoMaterial::Topsoil;

	/** Structural state at the target position. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Target")
	EStructuralState TargetStructuralState = EStructuralState::Stable;

	/** Is the target position valid for terraforming? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge|Target")
	bool bTargetValid = false;

	// ── Skill Integration ───────────────────────────────────────────────────

	/** Player's terraforming skill level (0-100). Set by skill system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Skills")
	int32 TerraformingSkill = 1;

	/** Player's prospecting skill level (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Skills")
	int32 ProspectingSkill = 1;

	/** Player's mining skill level (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Skills")
	int32 MiningSkill = 1;

	// ── Cooldown ────────────────────────────────────────────────────────────

	/** Current action cooldown remaining (seconds). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TerraForge")
	float ActionCooldown = 0.0f;

	/** Is the player currently performing an action? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "TerraForge")
	bool IsActionInProgress() const { return ActionCooldown > 0.0f; }

	// ── Events ──────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Events")
	FOnTerraformAction OnTerraformAction;

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Events")
	FOnObserveModeChanged OnObserveModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Events")
	FOnStructuralWarning OnStructuralWarning;

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Events")
	FOnContextMenuRequested OnContextMenuRequested;

	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Events")
	FOnProspectingComplete OnProspectingComplete;

	/** Fired when a gem is found while mining rock. */
	UPROPERTY(BlueprintAssignable, Category = "TerraForge|Events")
	FOnGemFound OnGemFound_Event;

	// ── Configuration ───────────────────────────────────────────────────────

	/** Max range for terraforming actions (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Config")
	float MaxActionRange = 800.0f;

	/** Prospecting search radius (meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Config")
	float ProspectingRadius = 30.0f;

	/** Flatten minimum skill requirement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Config")
	int32 FlattenMinSkill = 30;

	/** Slope flattening minimum skill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TerraForge|Config")
	int32 SlopeMinSkill = 30;

private:
	// ── Internal State ──────────────────────────────────────────────────────

	/** Cached reference to the TerraForge subsystem. */
	UPROPERTY()
	TObjectPtr<UTerraForgeSubsystem> TerraForgeSubsystem;

	/** Is the context menu currently displayed? */
	bool bContextMenuOpen = false;

	/** Is observe mode currently active? */
	bool bObserveModeActive = false;

	/** Reference height used for flattening (set when flatten action starts). */
	float FlattenReferenceHeight = 0.0f;

	/** Last prospecting results. */
	TArray<FProspectingResult> LastProspectResults;

	/** Previous structural state (for warning change detection). */
	EStructuralState PreviousStructuralState = EStructuralState::Stable;

	/** Gem drop table — initialized once at BeginPlay. */
	FGemDropTable GemDropTable;

	// ── Internal Methods ────────────────────────────────────────────────────

	/** Update the target position from the player's look direction. */
	void UpdateTarget();

	/** Get the material dig time at the target (seconds). */
	float GetDigTime() const;

	/** Perform the actual dig operation. Returns the dug material. */
	EGeoMaterial PerformDig();

	/** Perform the raise operation. Returns success. */
	bool PerformRaise();

	/** Perform the flatten operation. Returns success. */
	bool PerformFlatten();

	/** Check structural integrity and fire warnings. */
	void CheckStructuralIntegrity();

	/** Add dug material to inventory. */
	void AddToInventory(EGeoMaterial Material, int32 Units = 1);

	/** Remove material from inventory for raising terrain. */
	bool RemoveFromInventory(int32 Units = 1);

	/** Handle gem discovery — spawn world item, notify UI, add to inventory. */
	void OnGemFound(const FGemDropResult& GemResult);
};
