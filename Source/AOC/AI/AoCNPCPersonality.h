// AoCNPCPersonality.h — NPC personality traits and archetype system
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCNeedSystem.h"
#include "AoCNPCPersonality.generated.h"

// Forward declare the goal enum from the planner
enum class ENPCGoal : uint8;

/** Preset personality archetypes */
UENUM(BlueprintType)
enum class ENPCArchetype : uint8
{
	Custom    UMETA(DisplayName = "Custom"),
	Villager  UMETA(DisplayName = "Villager"),
	Guard     UMETA(DisplayName = "Guard"),
	Bandit    UMETA(DisplayName = "Bandit"),
	Merchant  UMETA(DisplayName = "Merchant"),
	Mage      UMETA(DisplayName = "Mage"),
	Hunter    UMETA(DisplayName = "Hunter"),
	Hermit    UMETA(DisplayName = "Hermit")
};

/**
 * UAoCNPCPersonality
 *
 * Personality traits that influence goal selection weights, combat behavior,
 * and planning depth. Each NPC's personality makes them behave uniquely.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCPersonality : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCPersonality();

	// --- Archetype --------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality")
	ENPCArchetype Archetype = ENPCArchetype::Villager;

	// --- Traits (0.0 - 1.0) ----------------------------------------------------

	/** 0=peaceful, 1=bloodthirsty. Weights combat goals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Aggression = 0.1f;

	/** 0=lazy, 1=workaholic. Weights crafting/building/gathering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Industry = 0.8f;

	/** 0=cowardly, 1=fearless. Threshold for flee vs fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Courage = 0.3f;

	/** 0=hermit, 1=social butterfly. Weights social/trade goals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Sociability = 0.7f;

	/** 0=generous, 1=hoarding. Weights wealth/looting goals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Greed = 0.3f;

	/** 0=stay home, 1=explorer. Weights exploration/investigation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Curiosity = 0.2f;

	/** 0=simple plans, 1=complex multi-step plans. Affects max plan depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float Intelligence = 0.5f;

	/** 0=never casts, 1=prefers spells. Weights spell usage in combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Personality", meta = (ClampMin = "0", ClampMax = "1"))
	float MagicAffinity = 0.1f;

	// --- Public API -------------------------------------------------------------

	/** Apply a preset archetype's trait values. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Personality")
	void SetArchetype(ENPCArchetype NewArchetype);

	/** Get the personality weight multiplier for a given goal. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Personality")
	float GetGoalWeight(ENPCGoal Goal) const;

	/** Should the NPC flee given a threat level (0-100)? */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Personality")
	bool ShouldFlee(float ThreatLevel) const;

	/** Should the NPC prefer magic over physical attacks? */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Personality")
	bool ShouldUseMagic() const;

	/** Max steps the NPC's plan can contain (Intelligence * 10, clamped 1-10). */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Personality")
	int32 GetMaxPlanDepth() const;

	/** Personality-modified need urgency multiplier. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Personality")
	float WeighNeedResponse(ENPCNeed Need) const;

	/** Randomize all traits within a range around the archetype center. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Personality")
	void RandomizeTraits(float Variance = 0.15f);

protected:
	virtual void BeginPlay() override;
};
