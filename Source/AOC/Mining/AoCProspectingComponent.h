// AoCProspectingComponent.h
// Architect of Creation - Prospecting Component
// Gives a character the ability to prospect for underground ore veins.
// Uses skill-based radius and detail level for discovery.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCVoxelTypes.h"
#include "AoCProspectingComponent.generated.h"

class AAoCVoxelWorld;
class AAoCOreVein;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EProspectDetailLevel : uint8
{
	Vague			UMETA(DisplayName = "Vague (Skill 0-30)"),
	Basic			UMETA(DisplayName = "Basic (Skill 30-60)"),
	Detailed		UMETA(DisplayName = "Detailed (Skill 60-90)"),
	Precise			UMETA(DisplayName = "Precise (Skill 90-100)")
};

UENUM(BlueprintType)
enum class ECompassDirection : uint8
{
	North			UMETA(DisplayName = "North"),
	NorthEast		UMETA(DisplayName = "Northeast"),
	East			UMETA(DisplayName = "East"),
	SouthEast		UMETA(DisplayName = "Southeast"),
	South			UMETA(DisplayName = "South"),
	SouthWest		UMETA(DisplayName = "Southwest"),
	West			UMETA(DisplayName = "West"),
	NorthWest		UMETA(DisplayName = "Northwest"),
	Above			UMETA(DisplayName = "Above"),
	Below			UMETA(DisplayName = "Below")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FProspectResult
{
	GENERATED_BODY()

	/** Display name of the ore found (e.g. "Copper Ore"). */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	FString OreName;

	/** Compass direction from player to vein. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	ECompassDirection Direction = ECompassDirection::North;

	/** Distance in tiles from the prospect point to the vein center. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	float DistanceTiles = 0.f;

	/** Quality of the ore vein (0-100). Only shown at Precise detail. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	uint8 Quality = 0;

	/** Detail level of this result, determined by skill at time of prospecting. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	EProspectDetailLevel DetailLevel = EProspectDetailLevel::Vague;

	/** World location where the prospect was performed. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	FVector ProspectLocation = FVector::ZeroVector;

	/** World location of the vein center (only accurate at Precise detail). */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	FVector VeinLocation = FVector::ZeroVector;

	/** Material type of the ore. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	EVoxelMaterial OreMaterial = EVoxelMaterial::Air;

	/** Estimated remaining ore in the vein. Only shown at Precise detail. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	int32 EstimatedRemainingOre = 0;

	/**
	 * Build a human-readable description based on detail level.
	 * Vague:    "You found something nearby" / "Nothing found"
	 * Basic:    "You found [ore name] nearby"
	 * Detailed: "You found [ore name] to the [direction], about [distance] tiles"
	 * Precise:  "Rich vein of [ore name] at [exact coords], quality [Q]"
	 */
	FString GetDescription() const;
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProspectCompleteDelegate, const TArray<FProspectResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProspectResultAddedDelegate, const FProspectResult&, NewResult);

// ─── Component ───────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCProspectingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCProspectingComponent();

	virtual void BeginPlay() override;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Prospecting skill level (0-100). Affects search radius and result detail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float ProspectingSkill;

	/** Current stamina (shared with other systems via owner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Stamina")
	float CurrentStamina;

	/** Maximum stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Stamina")
	float MaxStamina;

	/** Base stamina cost per prospect action. Decreases with skill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Stamina")
	float BaseStaminaCost;

	/** Base XP gained per prospect action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Config")
	float XPPerProspect;

	/** Bonus XP when a vein is actually discovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Config")
	float XPPerDiscovery;

	/** Maximum number of prospect results stored in history. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Config", meta = (ClampMin = "1", ClampMax = "50"))
	int32 MaxHistorySize;

	/** True while a prospect action is in progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	bool bIsProspecting;

	/** Time in seconds for a prospect action to complete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Config")
	float ProspectDuration;

	/** Reference to the voxel world (set in BeginPlay or manually). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prospecting|Config")
	AAoCVoxelWorld* VoxelWorld;

	/** History of past prospect results for triangulation. Limited to MaxHistorySize. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	TArray<FProspectResult> ProspectHistory;

	/** Results from the most recent prospect action. */
	UPROPERTY(BlueprintReadOnly, Category = "Prospecting")
	TArray<FProspectResult> LastProspectResults;

	// ── Delegates ───────────────────────────────────────────────────────────

	/** Fired when a prospect action completes with all results. */
	UPROPERTY(BlueprintAssignable, Category = "Prospecting|Events")
	FOnProspectCompleteDelegate OnProspectComplete;

	/** Fired for each individual result found during prospecting. */
	UPROPERTY(BlueprintAssignable, Category = "Prospecting|Events")
	FOnProspectResultAddedDelegate OnProspectResultAdded;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Begin prospecting at the given world location.
	 * Searches a sphere for ore veins, radius based on skill.
	 * @param Location World position to prospect from
	 * @return True if prospecting started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Prospecting")
	bool StartProspect(FVector Location);

	/**
	 * Cancel an in-progress prospect action.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prospecting")
	void CancelProspect();

	/**
	 * Get all results from the most recent prospect action.
	 * @return Array of prospect results
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	TArray<FProspectResult> GetProspectResults() const;

	/**
	 * Get the full prospect history for triangulation.
	 * @return Array of all stored prospect results (up to MaxHistorySize)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	TArray<FProspectResult> GetProspectHistory() const;

	/**
	 * Clear the prospect history.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prospecting")
	void ClearHistory();

	/**
	 * Get the current search radius in world units (centimeters).
	 * Radius = (1 + ProspectingSkill / 10) tiles × VOXEL_SIZE.
	 * @return Search radius in centimeters
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	float GetSearchRadius() const;

	/**
	 * Get the search radius in tiles.
	 * @return Radius in tiles (1-11)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	float GetSearchRadiusTiles() const;

	/**
	 * Get the detail level for the current skill level.
	 * @return Detail level enum
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	EProspectDetailLevel GetDetailLevel() const;

	/**
	 * Get the stamina cost for a prospect action at current skill.
	 * @return Stamina cost
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	float GetStaminaCost() const;

	/**
	 * Check if the player has enough stamina to prospect.
	 * @return True if stamina is sufficient
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	bool HasEnoughStamina() const;

	/**
	 * Add prospecting skill XP. Increases ProspectingSkill over time.
	 * @param Amount XP to add
	 */
	UFUNCTION(BlueprintCallable, Category = "Prospecting")
	void GainProspectingXP(float Amount);

	/**
	 * Determine the compass direction from one point to another.
	 * @param From Source position
	 * @param To Target position
	 * @return Compass direction enum
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	static ECompassDirection GetCompassDirection(FVector From, FVector To);

	/**
	 * Get a display string for a compass direction.
	 * @param Dir The direction enum
	 * @return Human-readable string (e.g. "northwest")
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Prospecting")
	static FString GetDirectionString(ECompassDirection Dir);

private:
	/** Timer handle for the prospect duration. */
	FTimerHandle ProspectTimerHandle;

	/** Accumulated XP toward next skill point. */
	float ProspectingXP;

	/** XP required per skill level increase. */
	static constexpr float XPPerSkillPoint = 120.f;

	/** The location where the current prospect action was started. */
	FVector PendingProspectLocation;

	/** Called when the prospect timer completes. */
	void OnProspectTimerComplete();

	/**
	 * Perform the actual search logic: query VoxelWorld for nearby veins,
	 * build FProspectResult entries based on skill detail level.
	 * @param Location World position to search from
	 * @return Array of prospect results
	 */
	TArray<FProspectResult> PerformProspectSearch(FVector Location);

	/**
	 * Build a single FProspectResult from a discovered vein, filtered by detail level.
	 * @param Vein The vein data found
	 * @param ProspectLocation Where the prospect was performed
	 * @param Detail Current detail level
	 * @return Populated prospect result
	 */
	FProspectResult BuildResult(const FVeinData& Vein, FVector ProspectLocation, EProspectDetailLevel Detail) const;

	/** Add a result to the history, trimming to MaxHistorySize. */
	void AddToHistory(const FProspectResult& Result);

	/** Consume stamina for a prospect action. Returns false if insufficient. */
	bool ConsumeStamina(float Amount);

	/** Try to find the voxel world in the level if not set. */
	void FindVoxelWorld();
};
