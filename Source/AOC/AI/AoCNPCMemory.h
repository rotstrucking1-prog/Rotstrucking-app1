// AoCNPCMemory.h — NPC world knowledge and memory system
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCMemory.generated.h"

class ACharacter;

/** Types of encounters NPCs can remember about players */
UENUM(BlueprintType)
enum class EEncounterType : uint8
{
	Attacked   UMETA(DisplayName = "Attacked"),
	Traded     UMETA(DisplayName = "Traded"),
	Helped     UMETA(DisplayName = "Helped"),
	Threatened UMETA(DisplayName = "Threatened"),
	Fled       UMETA(DisplayName = "Fled From")
};

/** A remembered threat in the world */
USTRUCT(BlueprintType)
struct FNPCThreatMemory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	TWeakObjectPtr<AActor> ThreatActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	float ThreatLevel = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	float LastSeenTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	FVector LastSeenLocation = FVector::ZeroVector;
};

/** A known ally */
USTRUCT(BlueprintType)
struct FNPCAllyMemory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	TWeakObjectPtr<AActor> AllyActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	FName Faction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	float Trust = 50.f;
};

/** Memory of a specific player character */
USTRUCT(BlueprintType)
struct FPlayerMemory
{
	GENERATED_BODY()

	/** -100 (sworn enemy) to 100 (trusted ally) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	float Hostility = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	float LastEncounterTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	FVector LastEncounterLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	int32 TimesAttacked = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	int32 TimesTraded = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	int32 TimesHelped = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	int32 KnownPlayerLevel = 1;
};

/** A visited location breadcrumb */
USTRUCT(BlueprintType)
struct FVisitedLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory")
	float VisitTime = 0.f;
};

/**
 * UAoCNPCMemory
 *
 * Stores an NPC's knowledge of the world: known locations, threats,
 * allies, player encounters, and visited places. Used by the goal
 * planner to make informed decisions.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCMemory : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCMemory();

	// --- Known Locations --------------------------------------------------------

	/** Map of named locations: "Mine", "FishingSpot", "Forge", "Home", etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory|Locations")
	TMap<FName, FVector> KnownLocations;

	// --- Threats ----------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|Memory|Threats")
	TArray<FNPCThreatMemory> KnownThreats;

	// --- Allies -----------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|Memory|Allies")
	TArray<FNPCAllyMemory> KnownAllies;

	// --- Owned Property ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory|Property")
	TArray<FName> OwnedProperty;

	// --- Visited Locations (breadcrumbs) ----------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|Memory|Visited")
	TArray<FVisitedLocation> VisitedLocations;

	/** Maximum number of breadcrumbs to keep */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Memory|Visited")
	int32 MaxVisitedLocations = 50;

	// --- Player Encounters ------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|Memory|Players")
	TMap<TObjectPtr<ACharacter>, FPlayerMemory> PlayerEncounters;

	// --- Public API: Locations --------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	void RememberLocation(FName ID, FVector Pos);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	FVector GetLocationOf(FName ID) const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Memory")
	bool KnowsLocation(FName ID) const;

	/** Find the nearest known location with a given ID prefix/type from a position. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	FVector GetNearestKnownLocation(FName Type, FVector FromPos) const;

	/** Add a breadcrumb for patrol/exploration memory. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	void AddVisitedBreadcrumb(FVector Location);

	// --- Public API: Threats ----------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	void RememberThreat(AActor* ThreatActor, float ThreatLevel);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	void ForgetOldThreats(float MaxAgeSeconds);

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Memory")
	bool HasActiveThreats() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Memory")
	float GetHighestThreatLevel() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	AActor* GetHighestThreatActor() const;

	// --- Public API: Allies -----------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	void RememberAlly(AActor* AllyActor, FName Faction, float Trust);

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Memory")
	bool IsAlly(AActor* Actor) const;

	// --- Public API: Player Encounters ------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	void RecordPlayerEncounter(ACharacter* Player, EEncounterType Type);

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Memory")
	float GetPlayerHostility(ACharacter* Player) const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Memory")
	bool HasMetPlayer(ACharacter* Player) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Memory")
	FPlayerMemory GetPlayerMemory(ACharacter* Player) const;

protected:
	virtual void BeginPlay() override;
};
