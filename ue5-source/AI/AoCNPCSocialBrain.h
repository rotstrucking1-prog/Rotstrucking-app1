// AoCNPCSocialBrain.h
// NPC Social AI — relationships, groups, betrayal, reputation, gossip
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCSocialBrain.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCSocial, Log, All);

class AoCHumanoidNPCV2;
class UAoCNPCPersonality;
class UAoCNPCMemory;
class UAoCNPCCombatBrain;

/** Types of social interaction */
UENUM(BlueprintType)
enum class EInteractionType : uint8
{
	Greeting,
	Trading,
	GiftGiving,
	FightingTogether,
	SharedKill,
	Insult,
	TheftAttempt,
	AttackedMe,
	BetrayedMe,
	KilledMyAlly,
	HelpedMe,
	HealedMe,
	RescuedMe,
	GaveItem,
	NearbyPresence // passive — just being near each other
};

/** Record of a single interaction */
USTRUCT(BlueprintType)
struct FInteractionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInteractionType Type = EInteractionType::NearbyPresence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Magnitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GameTime = 0.f;
};

/** Full relationship with another actor */
USTRUCT(BlueprintType)
struct FNPCRelationship
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	FGuid TargetID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	FString TargetName;

	/** Trust: -100 (total distrust) to +100 (unconditional trust) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	float Trust = 0.f;

	/** Hostility: -100 (extremely friendly) to +100 (kill on sight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	float Hostility = 0.f;

	/** Familiarity: 0 (stranger) to 100 (known very well) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	float Familiarity = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	float LastInteractionTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	TArray<FInteractionRecord> InteractionHistory;

	/** Cached flags for quick checks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	bool bIsAlly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	bool bIsEnemy = false;

	/** Betrayal memory — nearly irreversible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	bool bWasBetrayed = false;

	/** Revenge flag — NPC wants payback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	bool bWantsRevenge = false;

	void UpdateCachedFlags()
	{
		bIsAlly = Trust > 30.f && Hostility < -20.f;
		bIsEnemy = Hostility > 40.f || bWasBetrayed;
	}
};

/** A temporary group of NPCs */
USTRUCT(BlueprintType)
struct FNPCGroup
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GroupID;

	UPROPERTY()
	TArray<TWeakObjectPtr<AoCHumanoidNPCV2>> Members;

	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> Leader;

	UPROPERTY()
	FName SharedGoal; // e.g. "MineIron", "HuntPlayers", "Patrol"

	UPROPERTY()
	float FormationTime = 0.f;

	bool IsValid() const { return Members.Num() > 0; }
};

/** Gossip item — information to share about someone */
USTRUCT()
struct FGossipItem
{
	GENERATED_BODY()

	FGuid SubjectID;
	FString SubjectName;
	float HostilityChange = 0.f;
	float TrustChange = 0.f;
	FString Reason; // "attacked me", "helped me", etc.
	float GameTime = 0.f;
};

/**
 * UAoCNPCSocialBrain — UActorComponent
 *
 * Full social system: relationships, groups, betrayal, gossip, reputation.
 * NPCs form bonds, hold grudges, betray allies, and spread information.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCSocialBrain : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCSocialBrain();

	virtual void BeginPlay() override;

	// --- Relationship Management ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	FNPCRelationship GetRelationship(const FGuid& TargetID) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void ModifyTrust(const FGuid& TargetID, float Delta);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void ModifyHostility(const FGuid& TargetID, float Delta);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void RecordInteraction(const FGuid& TargetID, const FString& TargetName, EInteractionType Type, float Magnitude = 1.0f);

	// --- Quick Checks ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool ShouldAttack(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool IsAlly(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool IsEnemy(AActor* Target) const;

	// --- Group System ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool ShouldGroupWith(AoCHumanoidNPCV2* Other) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void FormGroup(const TArray<AoCHumanoidNPCV2*>& InitialMembers);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void LeaveGroup();

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	TArray<AoCHumanoidNPCV2*> GetGroupMembers() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool IsInGroup() const { return CurrentGroup.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool IsGroupLeader() const;

	// --- Betrayal ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool ConsiderBetrayal(AActor* Ally) const;

	// --- Reputation Spread ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void SpreadReputation(const FGuid& TargetID, const FString& TargetName, float HostilityDelta, float Radius);

	// --- Faction ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	FName FactionID;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool IsSameFaction(AActor* Other) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	float GetFactionBaseHostility(FName OtherFaction) const;

	// --- Passive Updates ---

	/** Called periodically — passive relationship changes from proximity, gossip, etc. */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	void SocializeTick(float DeltaTime);

	// --- Queries ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	TArray<FGuid> GetAllies() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	TArray<FGuid> GetEnemies() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	bool HasRevengeTarget() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Social")
	FGuid GetRevengeTarget() const;

	// --- NPC Unique ID ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	FGuid MyID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Social")
	FString MyName;

private:
	// --- References ---
	UPROPERTY()
	TWeakObjectPtr<AoCHumanoidNPCV2> OwnerNPC;

	UPROPERTY()
	UAoCNPCPersonality* Personality = nullptr;

	UPROPERTY()
	UAoCNPCMemory* Memory = nullptr;

	// --- Relationship Data ---
	TMap<FGuid, FNPCRelationship> Relationships;

	// --- Group ---
	FNPCGroup CurrentGroup;

	// --- Gossip Queue ---
	TArray<FGossipItem> GossipQueue;
	float GossipCooldown = 0.f;

	// --- Internal helpers ---

	/** Get or create a relationship entry for a target */
	FNPCRelationship& GetOrCreateRelationship(const FGuid& TargetID, const FString& TargetName = TEXT("Unknown"));

	/** Apply interaction effects on trust/hostility */
	void ApplyInteractionEffects(FNPCRelationship& Rel, EInteractionType Type, float Magnitude);

	/** Process gossip queue — share opinions with nearby NPCs */
	void ProcessGossip(float DeltaTime);

	/** Update familiarity for NPCs within line of sight */
	void UpdateProximityFamiliarity(float DeltaTime);

	/** Check if group should split due to disagreements */
	void EvaluateGroupCohesion();

	/** Get unique ID from an actor (uses NPC GUID or actor unique id) */
	FGuid GetActorGUID(AActor* Actor) const;

	/** Get actor name for relationship storage */
	FString GetActorDisplayName(AActor* Actor) const;

	/** Faction hostility defaults */
	static TMap<FName, TMap<FName, float>> FactionHostilityMatrix;
	static bool bFactionMatrixInitialized;
	static void InitFactionMatrix();

	FRandomStream SocialRand;
};
