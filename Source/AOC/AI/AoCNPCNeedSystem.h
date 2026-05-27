// AoCNPCNeedSystem.h — Biological/Psychological need tracking for NPCs
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCNeedSystem.generated.h"

/** Enumeration of all NPC needs */
UENUM(BlueprintType)
enum class ENPCNeed : uint8
{
	Hunger   UMETA(DisplayName = "Hunger"),
	Energy   UMETA(DisplayName = "Energy"),
	Safety   UMETA(DisplayName = "Safety"),
	Purpose  UMETA(DisplayName = "Purpose"),
	Social   UMETA(DisplayName = "Social"),
	Wealth   UMETA(DisplayName = "Wealth"),
	Health   UMETA(DisplayName = "Health")
};

/** Broadcast when any need becomes critical */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNeedCritical, ENPCNeed, Need, float, Value);

/**
 * UAoCNPCNeedSystem
 *
 * Tracks biological and psychological needs for an NPC.
 * Needs decay over time and are satisfied by NPC actions (eating, resting, etc).
 * The goal planner queries urgency to decide what the NPC should do.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCNeedSystem : public UActorComponent
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

public:
	UAoCNPCNeedSystem();

	// --- Need values (0-100, higher = more satisfied) --------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs", meta = (ClampMin = "0", ClampMax = "100"))
	float Hunger = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs", meta = (ClampMin = "0", ClampMax = "100"))
	float Energy = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs", meta = (ClampMin = "0", ClampMax = "100"))
	float Safety = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs", meta = (ClampMin = "0", ClampMax = "100"))
	float Purpose = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs", meta = (ClampMin = "0", ClampMax = "100"))
	float Social = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs", meta = (ClampMin = "0", ClampMax = "100"))
	float Wealth = 30.f;

	// --- Decay rates (per minute) -----------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs|Decay")
	float HungerDecayRate = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs|Decay")
	float EnergyDecayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs|Decay")
	float PurposeDecayRate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs|Decay")
	float SocialDecayRate = 0.3f;

	// --- Thresholds -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs|Thresholds")
	float UrgentThreshold = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Needs|Thresholds")
	float CriticalThreshold = 20.f;

	// --- Current activity modifier (set by brain) -------------------------------

	/** If true, NPC is sprinting — Energy decays 3x faster */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|AI|Needs")
	bool bIsSprinting = false;

	/** If true, NPC is in combat — Energy decays 2x faster */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|AI|Needs")
	bool bIsInCombat = false;

	/** If true, NPC is doing purposeful work — Purpose does not decay */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|AI|Needs")
	bool bIsWorking = false;

	// --- Delegates --------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AoC|AI|Needs")
	FOnNeedCritical OnNeedCritical;

	// --- Public API -------------------------------------------------------------

	/** Tick all needs by DeltaTime seconds. Called by the brain timer. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	void TickNeeds(float DeltaTime);

	/** Returns the need that is most below threshold (most urgent). */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	ENPCNeed GetMostUrgentNeed() const;

	/** Satisfy a need by the given amount (clamped 0-100). */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	void SatisfyNeed(ENPCNeed Need, float Amount);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	float GetDecayRate(const FString& NeedName) const
	{
		if (NeedName == TEXT("Hunger")) return HungerDecayRate;
		if (NeedName == TEXT("Energy")) return EnergyDecayRate;
		if (NeedName == TEXT("Purpose")) return PurposeDecayRate;
		if (NeedName == TEXT("Social")) return SocialDecayRate;
		return 1.0f;
	}

	/** Get urgency score 0-1 for a specific need (1 = maximally urgent). */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Needs")
	float GetNeedUrgency(ENPCNeed Need) const;

	/** Returns true if the given need is below the critical threshold. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Needs")
	bool IsNeedCritical(ENPCNeed Need) const;

	/** Returns true if ANY need is critical. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Needs")
	bool IsAnyNeedCritical() const;

	/** Get the raw value for a need. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Needs")
	float GetNeedValue(ENPCNeed Need) const;

	/** Set the raw value for a need. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	void SetNeedValue(ENPCNeed Need, float Value);

	/** Set Safety directly (called by perception when threats appear/vanish). */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	void SetSafety(float Value);

private:
	/** Helper: get the pointer to the float for a given need enum. */
	float* GetNeedPtr(ENPCNeed Need);
	const float* GetNeedPtr(ENPCNeed Need) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Needs")
	void ConsumeItem(const FString& ItemName);

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Needs")
	float GetNeedMax(const FString& NeedName) const;

};