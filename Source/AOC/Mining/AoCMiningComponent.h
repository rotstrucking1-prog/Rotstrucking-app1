// AoCMiningComponent.h
// Architect of Creation - Mining Component
// Gives a character the ability to mine ore, dig tunnels, and prospect for veins.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCVoxelTypes.h"
#include "AoCMiningComponent.generated.h"

class AAoCVoxelWorld;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EMiningAction : uint8
{
	None			UMETA(DisplayName = "None"),
	TunnelForward	UMETA(DisplayName = "Tunnel Forward"),
	TunnelDown		UMETA(DisplayName = "Tunnel Down"),
	TunnelUp		UMETA(DisplayName = "Tunnel Up"),
	MineOre			UMETA(DisplayName = "Mine Ore"),
	Prospect		UMETA(DisplayName = "Prospect")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FMiningResult
{
	GENERATED_BODY()

	/** Material that was mined. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	EVoxelMaterial MinedMaterial = EVoxelMaterial::Air;

	/** Quality of the mined material (0-100). */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	uint8 Quality = 0;

	/** Quantity of ore/material yielded. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	int32 Quantity = 0;

	/** XP gained from this mining action. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	float XPGained = 0.f;

	/** True if mining was successful. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	bool bSuccess = false;
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMiningHitDelegate, FVector, HitLocation, EVoxelMaterial, HitMaterial);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiningCompleteDelegate, FMiningResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOreExtractedDelegate, EVoxelMaterial, OreMaterial, int32, Quantity, uint8, Quality);

// ─── Component ───────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCMiningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCMiningComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ── Properties ──────────────────────────────────────────────────────────

	/** Mining skill level (0-100). Affects speed, yield, and prospecting radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining", meta = (ClampMin = "0", ClampMax = "100"))
	float MiningSkill;

	/** Current stamina (shared with other systems via owner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Stamina")
	float CurrentStamina;

	/** Maximum stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Stamina")
	float MaxStamina;

	/** Stamina cost per mining hit. Decreases with skill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Stamina")
	float BaseStaminaCostPerHit;

	/** Quality of the equipped pickaxe (0-100). Affects mining speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Tool", meta = (ClampMin = "0", ClampMax = "100"))
	float ToolQuality;

	/** Base time (seconds) per mining hit at skill 0 with tool quality 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Config")
	float BaseMiningTime;

	/** XP gained per mining hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Config")
	float XPPerHit;

	/** XP gained per ore unit extracted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Config")
	float XPPerOreExtracted;

	/** Width of carved tunnels in centimeters (default 200cm = 2m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Config")
	float TunnelWidth;

	/** Height of carved tunnels in centimeters (default 250cm = 2.5m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Config")
	float TunnelHeight;

	/** True while a mining action is in progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	bool bIsMining;

	/** Current mining action being performed. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	EMiningAction CurrentAction;

	/** Target location of the current mining action. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	FVector TargetLocation;

	/** Material at the target location. */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	EVoxelMaterial TargetMaterial;

	/** Progress of current mining action (0.0 to 1.0). */
	UPROPERTY(BlueprintReadOnly, Category = "Mining")
	float MiningProgress;

	/** Reference to the voxel world (set in BeginPlay or manually). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mining|Config")
	AAoCVoxelWorld* VoxelWorld;

	// ── Delegates ───────────────────────────────────────────────────────────

	/** Fired each time the pickaxe makes a hit during mining. */
	UPROPERTY(BlueprintAssignable, Category = "Mining|Events")
	FOnMiningHitDelegate OnMiningHit;

	/** Fired when a mining action completes. */
	UPROPERTY(BlueprintAssignable, Category = "Mining|Events")
	FOnMiningCompleteDelegate OnMiningComplete;

	/** Fired when ore is successfully extracted from a vein. */
	UPROPERTY(BlueprintAssignable, Category = "Mining|Events")
	FOnOreExtractedDelegate OnOreExtracted;

	// ── Functions ───────────────────────────────────────────────────────────

	/**
	 * Begin mining at a target location.
	 * @param InTargetLocation World position to mine
	 * @param InTargetMaterial Expected material at the location
	 * @return True if mining was started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool StartMining(FVector InTargetLocation, EVoxelMaterial InTargetMaterial);

	/**
	 * Stop the current mining action.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void StopMining();

	/**
	 * Carve a horizontal tunnel forward (in the owner's facing direction).
	 * Tunnel is 2m wide × 2.5m tall.
	 * @return True if the tunnel action started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool TunnelForward();

	/**
	 * Carve a tunnel section downward at -0.5m slope.
	 * @return True if the tunnel action started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool TunnelDown();

	/**
	 * Carve a tunnel section upward at +0.5m slope.
	 * @return True if the tunnel action started successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool TunnelUp();

	/**
	 * Mine ore from an exposed vein at the given location.
	 * Yield is based on mining skill and ore quality.
	 * @param Location World position of the ore to mine
	 * @return Mining result with extracted ore info
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	FMiningResult MineOre(FVector Location);

	/**
	 * Prospect for ore veins near the player.
	 * Search radius = 1 to 10 tiles based on mining skill.
	 * @param SearchCenter World position to search from
	 * @param OutVeins Found veins
	 * @return True if any veins were found
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	bool Prospect(FVector SearchCenter, TArray<FVeinData>& OutVeins);

	/**
	 * Get the effective mining time (seconds) for a single hit.
	 * Accounts for skill and tool quality.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mining")
	float GetEffectiveMiningTime() const;

	/**
	 * Get the stamina cost for a single mining hit.
	 * Decreases with skill level.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mining")
	float GetStaminaCost() const;

	/**
	 * Get the prospecting radius based on skill level.
	 * @return Radius in centimeters (1-10 tiles, 50-500cm base → scaled)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mining")
	float GetProspectRadius() const;

	/**
	 * Check if the player has a pickaxe equipped.
	 * Placeholder — returns true. Override in derived class.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mining")
	bool HasPickaxeEquipped() const;

	/**
	 * Check if the player has enough stamina for a mining hit.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mining")
	bool HasEnoughStamina() const;

	/**
	 * Add mining skill XP. Increases MiningSkill over time.
	 * @param Amount XP amount to add
	 */
	UFUNCTION(BlueprintCallable, Category = "Mining")
	void GainMiningXP(float Amount);

	/**
	 * Get the animation name for the current mining action.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mining")
	FName GetMiningAnimation() const;

private:
	/** Timer handle for the mining hit interval. */
	FTimerHandle MiningTimerHandle;

	/** Accumulated XP toward next skill point. */
	float MiningXP;

	/** XP required per skill level increase. */
	static constexpr float XPPerSkillPoint = 150.f;

	/** Called when the mining timer fires (one hit). */
	void OnMiningTick();

	/**
	 * Carve a tunnel section at the given location with the specified vertical offset.
	 * @param VerticalOffset Z offset in cm (0=level, -50=down, +50=up)
	 * @return True if the carve was performed
	 */
	bool CarveTunnelSection(float VerticalOffset);

	/** Consume stamina for a mining action. Returns false if insufficient. */
	bool ConsumeStamina(float Amount);

	/** Try to find the voxel world in the level if not set. */
	void FindVoxelWorld();
};
