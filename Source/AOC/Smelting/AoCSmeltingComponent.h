// AoCSmeltingComponent.h
// Architect of Creation - Smelting Component
// Player actor component managing smelting skill, furnace interaction,
// bellows pumping, metal extraction, quenching, and item recycling.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCFurnaceActor.h"
#include "AoCSmeltingComponent.generated.h"

class AAoCFurnaceActor;

// ─── Structs ─────────────────────────────────────────────────────────────────

/** Data for a hot metal item held by the player after extraction. */
USTRUCT(BlueprintType)
struct AOC_API FHotMetalItem
{
	GENERATED_BODY()

	/** The smelting result data. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|HotMetal")
	FSmeltingResult SmeltData;

	/** Current temperature of the held item (cools over 60s). */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|HotMetal")
	float CurrentTemperature = 20.f;

	/** Whether this item has been quenched in water. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|HotMetal")
	bool bIsQuenched = false;

	/** Whether the player is holding a hot metal item. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|HotMetal")
	bool bIsValid = false;

	FHotMetalItem()
		: CurrentTemperature(20.f)
		, bIsQuenched(false)
		, bIsValid(false)
	{}
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSmeltStarted, UAoCSmeltingComponent*, Component, AAoCFurnaceActor*, Furnace);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSmeltComplete, UAoCSmeltingComponent*, Component, AAoCFurnaceActor*, Furnace, uint8, FinalQuality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBellowsPumped, UAoCSmeltingComponent*, Component);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSmeltingSkillUp, UAoCSmeltingComponent*, Component, float, NewSkillLevel);

// ─── Component ───────────────────────────────────────────────────────────────

/**
 * Player component for smelting interaction.
 *
 * Manages:
 *   - Smelting skill (0-100) with XP-based progression
 *   - Furnace interaction: loading ore, pumping bellows, extracting output
 *   - Hot metal handling: cooling over 60s, optional quenching
 *   - Item recycling (skill 90+): melt items back to lumps at 80% Q
 *   - Furnace construction skill checks
 *
 * Skill requirements for furnace construction:
 *   Campfire Crucible:  Smelting 0
 *   Bloomery:           Smelting 30
 *   Blast Furnace:      Smelting 60
 *   Crucible Furnace:   Smelting 80
 *   Arcane Forge:       Smelting 100
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSmeltingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSmeltingComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ── Skill Properties ────────────────────────────────────────────────────

	/** Player's smelting skill level (0-100). Determines quality cap and furnace access. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smelting|Skill", meta = (ClampMin = "0", ClampMax = "100"))
	float SmeltingSkill;

	/** Current XP toward next skill level. */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|Skill")
	float SmeltingXP;

	/** XP required for the next skill level increase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smelting|Skill")
	float XPPerLevel;

	// ── Hot Metal State ─────────────────────────────────────────────────────

	/** Currently held hot metal item (if any). */
	UPROPERTY(BlueprintReadOnly, Category = "Smelting|HotMetal")
	FHotMetalItem HeldHotMetal;

	// ── Delegates ───────────────────────────────────────────────────────────

	/** Fired when this player starts a smelting operation. */
	UPROPERTY(BlueprintAssignable, Category = "Smelting|Events")
	FOnSmeltStarted OnSmeltStarted;

	/** Fired when smelting completes for this player. */
	UPROPERTY(BlueprintAssignable, Category = "Smelting|Events")
	FOnSmeltComplete OnSmeltComplete;

	/** Fired when this player pumps bellows. */
	UPROPERTY(BlueprintAssignable, Category = "Smelting|Events")
	FOnBellowsPumped OnBellowsPumped;

	/** Fired when smelting skill increases. */
	UPROPERTY(BlueprintAssignable, Category = "Smelting|Events")
	FOnSmeltingSkillUp OnSmeltingSkillUp;

	// ── Furnace Interaction Functions ────────────────────────────────────────

	/**
	 * Interact with a furnace — validates furnace reference and opens UI.
	 * @param Furnace  The furnace actor to interact with.
	 * @return True if interaction was initiated successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Interaction")
	bool InteractWithFurnace(AAoCFurnaceActor* Furnace);

	/**
	 * Load ore into a furnace.
	 * @param Furnace  Target furnace.
	 * @param Ore      Ore material type.
	 * @param Amount   Number of ore pieces.
	 * @param Quality  Ore quality (0-100).
	 * @return True if ore was loaded successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Interaction")
	bool LoadOreIntoFurnace(AAoCFurnaceActor* Furnace, EVoxelMaterial Ore, int32 Amount, uint8 Quality);

	/**
	 * Start smelting ore in the furnace.
	 * @param Furnace     Target furnace.
	 * @param OutputType  Desired output form (Lump, Bar, Ingot).
	 * @return True if smelting was started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Interaction")
	bool StartSmeltInFurnace(AAoCFurnaceActor* Furnace, ESmeltOutput OutputType);

	/**
	 * Pump bellows on a Bloomery furnace. Raises temp by 100°C.
	 * Grants smelting XP per pump.
	 * @param Furnace  Target Bloomery furnace.
	 * @return True if bellows were pumped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Interaction")
	bool PumpBellows(AAoCFurnaceActor* Furnace);

	/**
	 * Extract finished output from a furnace. Metal will be hot.
	 * Final quality is capped by player's smelting skill.
	 * @param Furnace  Target furnace with pending output.
	 * @return True if output was extracted successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Interaction")
	bool ExtractFromFurnace(AAoCFurnaceActor* Furnace);

	// ── Hot Metal Functions ─────────────────────────────────────────────────

	/**
	 * Quench held hot metal by dipping it in a water barrel.
	 * Instantly cools the metal. Affects hardness properties.
	 * @param WaterBarrelLocation  World-space location of the water barrel.
	 * @return True if quenching was performed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|HotMetal")
	bool QuenchMetal(FVector WaterBarrelLocation);

	/**
	 * Check if the player is currently holding a hot metal item.
	 * @return True if holding hot metal.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|HotMetal")
	bool IsHoldingHotMetal() const;

	/**
	 * Get the current hot metal item data.
	 * @return FHotMetalItem struct with current state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|HotMetal")
	FHotMetalItem GetHeldHotMetal() const;

	// ── Recycling Functions ─────────────────────────────────────────────────

	/**
	 * Recycle an item by melting it back into raw material.
	 * Requires Smelting skill 90+.
	 * Output quality = 80% of original item quality.
	 * 
	 * @param Furnace       Target furnace (must be at reduction temp for the base material).
	 * @param ItemID        Name identifier of the item to recycle.
	 * @param ItemQuality   Quality of the item being recycled (0-100).
	 * @param BaseMaterial  The base metal material of the item.
	 * @return True if recycling was started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Recycling")
	bool RecycleItem(AAoCFurnaceActor* Furnace, FName ItemID, uint8 ItemQuality, EVoxelMaterial BaseMaterial);

	// ── Furnace Construction ────────────────────────────────────────────────

	/**
	 * Check if the player's smelting skill is high enough to build a furnace of the given tier.
	 * Campfire=0, Bloomery=30, BlastFurnace=60, CrucibleFurnace=80, ArcaneForge=100.
	 * @param Tier  Furnace tier to check.
	 * @return True if skill requirement is met.
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Construction")
	bool CanBuildFurnace(EFurnaceTier Tier) const;

	/**
	 * Get the minimum smelting skill required to build a furnace of the given tier.
	 * @param Tier  Furnace tier.
	 * @return Required skill level (0-100).
	 */
	UFUNCTION(BlueprintCallable, Category = "Smelting|Construction")
	static float GetRequiredSkillForFurnace(EFurnaceTier Tier);

protected:
	/**
	 * Award smelting XP and check for level-up.
	 * @param Amount  XP to award.
	 */
	void GainSmeltingXP(float Amount);

	/** Cool held hot metal over time in TickComponent. */
	void UpdateHotMetalCooling(float DeltaTime);

private:
	/** Handle for furnace smelting-complete delegate binding. */
	FDelegateHandle SmeltCompleteHandle;

	/** Currently bound furnace for smelting-complete notification. */
	UPROPERTY()
	AAoCFurnaceActor* BoundFurnace;

	/** Callback when bound furnace completes smelting. */
	UFUNCTION()
	void OnBoundFurnaceSmeltComplete(AAoCFurnaceActor* Furnace, FSmeltingResult Output);

	/** Maximum distance (cm) from water barrel to perform quenching. */
	static constexpr float QuenchMaxDistance = 200.f;

	/** Minimum skill level required for item recycling. */
	static constexpr float RecyclingMinSkill = 90.f;

	/** Quality retention percentage for recycled items (80%). */
	static constexpr float RecyclingQualityRetention = 0.8f;

	/** Duration in seconds for hot metal to cool to ambient. */
	static constexpr float HotMetalCooldownDuration = 60.f;

	/** Ambient temperature constant. */
	static constexpr float AmbientTemp = 20.f;

	// ── XP Rewards ──────────────────────────────────────────────────────────

	/** XP awarded per completed smelt operation. */
	static constexpr float XPPerSmelt = 5.f;

	/** XP bonus multiplier per material tier (tier 1=1x, tier 6=3x). */
	static constexpr float XPTierMultiplierBase = 0.5f;

	/** XP awarded per bellows pump. */
	static constexpr float XPPerBellowsPump = 1.f;

	/** XP awarded per recycled item. */
	static constexpr float XPPerRecycle = 3.f;
};
