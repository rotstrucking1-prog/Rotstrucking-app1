// AoCNPCRelationship.h
// Architect of Creation — NPC Relationship, Trust, Party, and Grudge System
//
// Deep relationship and trust tracking between NPCs and players. This system enables
// emergent social dynamics: teaming up, betrayal, grudges, revenge, reputation spread,
// party loot fairness, and promise tracking.
//
// KEY DESIGN: Every interaction modifies trust. Trust determines what NPCs will do
// for or against an entity. A player who helps an NPC fight wolves gains trust. One
// who steals loot loses it. The NPC REMEMBERS — grudges fade over real-time hours,
// not minutes. Betrayal is remembered FOREVER by the victim and witnessed by nearby NPCs.
//
// PARTY SYSTEM: NPCs can form parties with players OR other NPCs. They track their
// contribution vs loot received. If the ratio gets too skewed, they get angry. If it
// gets extreme and their personality allows it, they BETRAY — attacking the party
// member and taking loot. This creates real social tension identical to player behavior.
//
// INTEGRATION: Used by AoCNPCBrainV2 for decision-making, AoCNPCSocialBrain for
// social interactions, AoCNPCSpeech for contextual dialogue, and AoCNPCCombatBrain
// for target selection and alliance tracking.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCRelationship.generated.h"

// Forward declarations
class AAoCNPCCharacter;
class UAoCNPCBrainV2;
class UAoCNPCSpeech;
class UAoCNPCSkillSystem;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * Discrete relationship levels derived from the continuous TrustScore (-100 to 100).
 * Each level unlocks or restricts different NPC behaviors.
 */
UENUM(BlueprintType)
enum class ERelationshipLevel : uint8
{
	Hostile			UMETA(DisplayName = "Hostile"),          // -100 to -60: Attack on sight
	Unfriendly		UMETA(DisplayName = "Unfriendly"),       // -60 to -20: Won't help, may insult
	Stranger		UMETA(DisplayName = "Stranger"),         // -20 to 20: Default, neutral
	Acquaintance	UMETA(DisplayName = "Acquaintance"),     // 20 to 40: Recognizes, basic trade
	Friendly		UMETA(DisplayName = "Friendly"),         // 40 to 60: Will help, share info
	Trusted			UMETA(DisplayName = "Trusted"),          // 60 to 80: Team up, share loot
	BondedAlly		UMETA(DisplayName = "Bonded Ally"),      // 80 to 100: Fight to death together
};

/**
 * What a party is currently trying to accomplish.
 */
UENUM(BlueprintType)
enum class EPartyGoal : uint8
{
	None			UMETA(DisplayName = "No Goal"),
	Hunt			UMETA(DisplayName = "Hunting"),
	Mine			UMETA(DisplayName = "Mining"),
	Explore			UMETA(DisplayName = "Exploring"),
	Raid			UMETA(DisplayName = "Raiding"),
	Defend			UMETA(DisplayName = "Defending"),
	Craft			UMETA(DisplayName = "Crafting Together"),
	Fish			UMETA(DisplayName = "Fishing"),
	Build			UMETA(DisplayName = "Building"),
	Trade			UMETA(DisplayName = "Trading Run"),
	Grind			UMETA(DisplayName = "Grinding"),
};

/**
 * How loot is distributed in a party.
 */
UENUM(BlueprintType)
enum class ELootRule : uint8
{
	FreeForAll			UMETA(DisplayName = "Free For All"),
	RoundRobin			UMETA(DisplayName = "Round Robin"),
	NeedBeforeGreed		UMETA(DisplayName = "Need Before Greed"),
	LeaderDecides		UMETA(DisplayName = "Leader Decides"),
	EqualSplit			UMETA(DisplayName = "Equal Split"),
};

/**
 * Type of promise an NPC has made or received.
 */
UENUM(BlueprintType)
enum class EPromiseType : uint8
{
	ShareLoot		UMETA(DisplayName = "Share Loot"),
	HelpFight		UMETA(DisplayName = "Help in Fight"),
	HelpMine		UMETA(DisplayName = "Help Mining"),
	HelpCraft		UMETA(DisplayName = "Help Crafting"),
	ProtectArea		UMETA(DisplayName = "Protect Area"),
	TradeItem		UMETA(DisplayName = "Trade Item"),
	PayDebt			UMETA(DisplayName = "Pay Debt"),
	MeetLater		UMETA(DisplayName = "Meet Later"),
};

/**
 * How an NPC feels about a betrayal decision.
 */
UENUM(BlueprintType)
enum class EBetrayalReadiness : uint8
{
	NeverBetray,        // Loyal personality — won't do it
	NotWorthIt,         // Too risky or not enough at stake
	Considering,        // Building toward betrayal
	Ready,              // Will betray at next opportunity
	Executing,          // Currently betraying
};

/**
 * Type of interaction event for trust modification.
 */
UENUM(BlueprintType)
enum class EInteractionType : uint8
{
	// POSITIVE
	FoughtAlongside,		// +5 per fight
	SharedLoot,				// +8 per significant share
	HealedMe,				// +10
	BuffedMe,				// +4
	SavedMyLife,			// +20
	KeptPromise,			// +12
	OfferedHelp,			// +6
	GaveGift,				// +8
	CompletedQuestTogether,	// +15
	TradedFairly,			// +3

	// NEGATIVE
	FoughtAgainst,			// -15 per fight
	StoleLoot,				// -20
	BrokePromise,			// -25
	KilledMe,				// -40
	AttackedUnprovoked,		// -30
	IgnoredMyNeed,			// -8
	InsultedMe,				// -5
	DamagedMyProperty,		// -15
	BetrayedAlliance,		// -50 (the big one)
	TradedUnfairly,			// -10

	// NEUTRAL
	PassedBy,				// +0.5 (familiarity)
	Spoke,					// +1
	TradedNeutral,			// +1
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * A promise made between two entities. Tracked for fulfillment or breaking.
 */
USTRUCT(BlueprintType)
struct FPromise
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPromiseType Type = EPromiseType::ShareLoot;

	/** Who made the promise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PromiserId;

	/** Who the promise was made to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PromiseeId;

	/** What was specifically promised (e.g., "50% of iron ore") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	/** When was this promise made (world time) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MadeAtTime = 0.0f;

	/** Deadline — when the promise should be fulfilled by. 0 = no deadline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DeadlineTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bFulfilled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bBroken = false;
};

/**
 * A single relationship record between this NPC and another entity.
 * Contains trust score, interaction history, promises, and emotional state.
 */
USTRUCT(BlueprintType)
struct FRelationshipRecord
{
	GENERATED_BODY()

	/** Unique ID of the other entity (player or NPC) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString EntityId;

	/** Display name for speech/debug */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString EntityName;

	/** Core trust score: -100 (sworn enemy) to +100 (bonded ally) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TrustScore = 0.0f;

	/** Computed relationship level from TrustScore */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ERelationshipLevel Level = ERelationshipLevel::Stranger;

	// --- INTERACTION HISTORY ---

	/** Times we fought on the same side */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 FightsAlongside = 0;

	/** Times we fought each other */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 FightsAgainst = 0;

	/** Items/gold they shared with me */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LootSharedWithMe = 0;

	/** Items I felt I deserved but they took */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LootTakenFromMe = 0;

	/** Times they turned on me mid-alliance */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TimesBetrayed = 0;

	/** Times they helped me (healed, buffed, saved) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TimesHelped = 0;

	/** Times I asked for help and they didn't respond */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TimesIgnoredMyNeed = 0;

	/** Total real-time hours spent in proximity/party */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TotalTimeSpentTogether = 0.0f;

	/** World time of last interaction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LastInteractionTime = 0.0f;

	/** How many times we've met / been in proximity */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalEncounters = 0;

	// --- PROMISES ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FPromise> ActivePromises;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PromisesKept = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PromisesBroken = 0;

	// --- EMOTIONAL STATE toward this entity ---

	/** Gratitude: 0-100, decays slowly (~1/hour) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Gratitude = 0.0f;

	/** Resentment: 0-100, decays VERY slowly (~1/hour real time) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Resentment = 0.0f;

	/** Fear: 0-100, they killed me before or are much stronger */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Fear = 0.0f;

	/** Admiration: 0-100, they've demonstrated impressive skill */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Admiration = 0.0f;
};

/**
 * A member's tracking data within a party — contribution, loot, and fairness perception.
 */
USTRUCT(BlueprintType)
struct FPartyMemberData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString MemberId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString MemberName;

	/** Damage dealt to enemies during this party session */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DamageDealt = 0.0f;

	/** Healing done to party members */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float HealingDone = 0.0f;

	/** Resources gathered while in party */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ResourcesGathered = 0;

	/** Total loot items received by this member */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LootReceived = 0;

	/** Gold received by this member */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GoldReceived = 0;

	/** This NPC's perception of what % of contribution this member did */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float PerceivedContributionPercent = 0.0f;

	/** This NPC's perception of what % of loot this member received */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float PerceivedLootPercent = 0.0f;
};

/**
 * A party/team that NPCs and players can form.
 */
USTRUCT(BlueprintType)
struct FNPCParty
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString PartyId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString LeaderId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FString> MemberIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EPartyGoal CurrentGoal = EPartyGoal::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ELootRule LootRule = ELootRule::FreeForAll;

	/** Per-member tracking data */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FPartyMemberData> MemberData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalLootDropped = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalGoldDropped = 0;

	/** When the party was formed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float FormedAtTime = 0.0f;

	/** Is this party still active? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsActive = false;
};

/**
 * A grudge record — persistent memory of being wronged. Decays very slowly.
 */
USTRUCT(BlueprintType)
struct FGrudge
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString AggressorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString AggressorName;

	/** What they did: "Killed me", "Stole my loot", "Betrayed our alliance" */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString Grievance;

	/** Intensity of the grudge: 0-100. Decays ~1 point per real-time hour. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Intensity = 0.0f;

	/** World time when the grievance occurred */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float OccurredAtTime = 0.0f;

	/** Has the NPC gotten revenge? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bRevenged = false;

	/** Number of times this NPC has warned others about the aggressor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TimesWarnedOthers = 0;
};

/**
 * Reputation gossip — information about another entity spread through the social network.
 */
USTRUCT(BlueprintType)
struct FReputationGossip
{
	GENERATED_BODY()

	/** Entity the gossip is about */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SubjectId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SubjectName;

	/** Who told me this gossip */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SourceId;

	/** The gossip: "They're a thief", "They helped me fight bandits", etc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString GossipContent;

	/** Trust modifier suggested by this gossip: negative = bad reputation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TrustModifier = 0.0f;

	/** When I heard this gossip */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float HeardAtTime = 0.0f;

	/** How much I trust the source (affects how much I believe the gossip) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float SourceCredibility = 0.5f;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRelationshipChanged, const FString&, EntityId, ERelationshipLevel, OldLevel, ERelationshipLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBetrayalTriggered, const FString&, BetrayerId, const FString&, VictimId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGrudgeFormed, const FString&, AggressorId, const FString&, Grievance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyFormed, const FString&, PartyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPartyDisbanded, const FString&, PartyId, const FString&, Reason);

// ============================================================================
// MAIN CLASS
// ============================================================================

/**
 * UAoCNPCRelationship — Deep relationship, trust, party, and grudge system.
 *
 * Tracks this NPC's relationship with every entity it has interacted with.
 * Enables emergent social dynamics: forming parties, tracking fairness,
 * betrayal based on personality + unfairness, grudge formation, reputation
 * gossip spreading through the NPC social network.
 *
 * Every NPC has an opinion of every entity they've met, and those opinions
 * change organically through gameplay interactions.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCRelationship : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCRelationship();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -----------------------------------------------------------
	// RELATIONSHIP MANAGEMENT
	// -----------------------------------------------------------

	/** Record an interaction with another entity. This is the PRIMARY way trust changes. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void RecordInteraction(const FString& EntityId, const FString& EntityName, EInteractionType Interaction);

	/** Get the relationship record for a specific entity. Returns nullptr if unknown. */
	FRelationshipRecord* GetRelationship(const FString& EntityId);

	/** Get the relationship level with an entity (defaults to Stranger if unknown) */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	ERelationshipLevel GetRelationshipLevel(const FString& EntityId) const;

	/** Get the trust score with an entity (defaults to 0 if unknown) */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	float GetTrustScore(const FString& EntityId) const;

	/** Directly modify trust score (clamped to -100..100). Use RecordInteraction instead when possible. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void ModifyTrust(const FString& EntityId, const FString& EntityName, float Delta);

	/** Get all known relationships */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	const TMap<FString, FRelationshipRecord>& GetAllRelationships() const { return Relationships; }

	/** Get the entity this NPC trusts the most */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	FString GetMostTrustedEntity() const;

	/** Get the entity this NPC trusts the least (most hostile toward) */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	FString GetMostHostileEntity() const;

	// -----------------------------------------------------------
	// PARTY SYSTEM
	// -----------------------------------------------------------

	/** Evaluate whether to accept a party invite from this entity */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	bool EvaluatePartyInvite(const FString& InviterId, EPartyGoal Goal, ELootRule LootRule);

	/** Create a new party (this NPC as leader) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	FString CreateParty(EPartyGoal Goal, ELootRule LootRule);

	/** Join an existing party */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	bool JoinParty(const FString& PartyId);

	/** Leave the current party */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void LeaveParty(const FString& Reason);

	/** Is this NPC currently in a party? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool IsInParty() const { return CurrentParty.bIsActive; }

	/** Get the current party data */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	const FNPCParty& GetCurrentParty() const { return CurrentParty; }

	/** Record that loot was distributed in the party */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void RecordPartyLoot(const FString& RecipientId, int32 ItemValue, int32 GoldAmount);

	/** Record party member contribution (damage, healing, resources) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void RecordPartyContribution(const FString& MemberId, float Damage, float Healing, int32 Resources);

	/** Get this NPC's perceived fairness score for the current party (-100 to 100) */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	float GetPerceivedFairness() const;

	/** Check if this NPC wants to invite someone to their party */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	bool WantsToInvite(const FString& EntityId) const;

	// -----------------------------------------------------------
	// BETRAYAL
	// -----------------------------------------------------------

	/** Evaluate whether this NPC is ready to betray their party. Considers personality, fairness, stakes. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	EBetrayalReadiness EvaluateBetrayalReadiness() const;

	/** Execute betrayal — attack party members, take loot */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void ExecuteBetrayal(const FString& TargetId);

	/** Can this NPC win a fight against the target? (simplified power check) */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool CanWinFightAgainst(const FString& TargetId) const;

	/** Are there witnesses nearby who would see a betrayal? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	int32 CountWitnesses() const;

	// -----------------------------------------------------------
	// GRUDGES
	// -----------------------------------------------------------

	/** Form a grudge against an entity */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void FormGrudge(const FString& AggressorId, const FString& AggressorName, const FString& Grievance, float Intensity);

	/** Get all active grudges */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	const TArray<FGrudge>& GetGrudges() const { return Grudges; }

	/** Does this NPC have a grudge against a specific entity? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool HasGrudgeAgainst(const FString& EntityId) const;

	/** Get the grudge intensity against an entity (0 if no grudge) */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	float GetGrudgeIntensity(const FString& EntityId) const;

	/** Should this NPC seek revenge on the grudge target? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool ShouldSeekRevenge(const FString& EntityId) const;

	/** Mark a grudge as avenged */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void MarkGrudgeRevenged(const FString& AggressorId);

	// -----------------------------------------------------------
	// GOSSIP / REPUTATION
	// -----------------------------------------------------------

	/** Receive gossip about an entity from another NPC */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void ReceiveGossip(const FReputationGossip& Gossip);

	/** Generate gossip about an entity to share with nearby NPCs */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	FReputationGossip GenerateGossipAbout(const FString& EntityId) const;

	/** Spread grudge warnings to nearby friendly NPCs */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void SpreadWarning(const FString& AggressorId);

	// -----------------------------------------------------------
	// PROMISES
	// -----------------------------------------------------------

	/** Make a promise to another entity */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void MakePromise(const FString& ToEntityId, EPromiseType Type, const FString& Description, float Deadline = 0.0f);

	/** Fulfill a promise */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void FulfillPromise(const FString& ToEntityId, EPromiseType Type);

	/** Break a promise (or let it expire) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void BreakPromise(const FString& ToEntityId, EPromiseType Type);

	/** Check for expired promises and mark them as broken */
	UFUNCTION(BlueprintCallable, Category = "AoC|Relationship")
	void CheckPromiseDeadlines();

	// -----------------------------------------------------------
	// DECISION HELPERS
	// -----------------------------------------------------------

	/** Should this NPC help the given entity in a fight? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool ShouldHelpInFight(const FString& EntityId) const;

	/** Should this NPC share loot with the given entity? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool ShouldShareLoot(const FString& EntityId) const;

	/** Should this NPC warn the given entity about danger? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool ShouldWarnOfDanger(const FString& EntityId) const;

	/** Should this NPC flee from this entity (high fear, low trust)? */
	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	bool ShouldFleeFrom(const FString& EntityId) const;

	// -----------------------------------------------------------
	// DIAGNOSTICS
	// -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "AoC|Relationship")
	FString GetDebugString() const;

	// -----------------------------------------------------------
	// DELEGATES
	// -----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AoC|Relationship")
	FOnRelationshipChanged OnRelationshipChanged;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Relationship")
	FOnBetrayalTriggered OnBetrayalTriggered;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Relationship")
	FOnGrudgeFormed OnGrudgeFormed;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Relationship")
	FOnPartyFormed OnPartyFormed;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Relationship")
	FOnPartyDisbanded OnPartyDisbanded;

protected:

	// -----------------------------------------------------------
	// TICK PHASES
	// -----------------------------------------------------------

	/** Decay emotions over time (gratitude, resentment, fear, grudge intensity) */
	void TickEmotionDecay(float DeltaTime);

	/** Check party fairness and update mood */
	void TickPartyFairness(float DeltaTime);

	/** Check for promise deadlines */
	void TickPromiseDeadlines(float DeltaTime);

	/** Tick time-spent-together for nearby known entities */
	void TickProximityTracking(float DeltaTime);

	// -----------------------------------------------------------
	// INTERNAL HELPERS
	// -----------------------------------------------------------

	/** Create or get a relationship record for an entity */
	FRelationshipRecord& GetOrCreateRelationship(const FString& EntityId, const FString& EntityName);

	/** Recalculate the ERelationshipLevel from the TrustScore */
	ERelationshipLevel ComputeRelationshipLevel(float TrustScore) const;

	/** Get the trust change amount for a given interaction type */
	float GetInteractionTrustDelta(EInteractionType Interaction) const;

	/** Get this NPC's personality trait value (0-1). Name: "Loyalty", "Greed", "Aggression", "Sociability" */
	float GetPersonalityTrait(const FString& TraitName) const;

	/** Calculate per-member contribution and loot percentages */
	void RecalculatePartyFairness();

	/** Find the member data for a specific member in the current party */
	FPartyMemberData* FindPartyMember(const FString& MemberId);

	// -----------------------------------------------------------
	// DATA
	// -----------------------------------------------------------

	/** All relationship records, keyed by entity ID */
	UPROPERTY()
	TMap<FString, FRelationshipRecord> Relationships;

	/** Active grudges */
	UPROPERTY()
	TArray<FGrudge> Grudges;

	/** Received gossip */
	UPROPERTY()
	TArray<FReputationGossip> ReceivedGossip;

	/** Current party (empty/inactive if not in a party) */
	UPROPERTY()
	FNPCParty CurrentParty;

	/** Timer for party fairness checks */
	UPROPERTY()
	float PartyFairnessTimer = 0.0f;

	/** Timer for emotion decay */
	UPROPERTY()
	float EmotionDecayTimer = 0.0f;

	/** Timer for promise deadline checks */
	UPROPERTY()
	float PromiseCheckTimer = 0.0f;

	/** Timer for proximity tracking */
	UPROPERTY()
	float ProximityTrackTimer = 0.0f;

	// -----------------------------------------------------------
	// CONFIGURATION
	// -----------------------------------------------------------

	/** Interval between party fairness recalculations */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float PartyFairnessCheckInterval = 60.0f;

	/** Fairness difference threshold for mild annoyance (%) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float FairnessMildThreshold = 15.0f;

	/** Fairness difference threshold for serious complaint (%) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float FairnessSeriousThreshold = 30.0f;

	/** Fairness difference threshold for anger / potential betrayal (%) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float FairnessAngerThreshold = 50.0f;

	/** Emotion decay rate — gratitude points lost per real-time minute */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float GratitudeDecayPerMinute = 0.02f;

	/** Resentment decays VERY slowly */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float ResentmentDecayPerMinute = 0.017f; // ~1 per hour

	/** Fear decay rate */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float FearDecayPerMinute = 0.03f;

	/** Grudge intensity decay per real-time minute */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float GrudgeDecayPerMinute = 0.017f; // ~1 per hour

	/** How far to search for witnesses (cm) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float WitnessSearchRadius = 3000.0f;

	/** Maximum number of relationships to track (oldest/least important get pruned) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	int32 MaxRelationships = 100;

	/** Maximum grudges to track */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	int32 MaxGrudges = 20;

	// -----------------------------------------------------------
	// CACHED REFERENCES
	// -----------------------------------------------------------

	UPROPERTY()
	TObjectPtr<AAoCNPCCharacter> OwnerNPC = nullptr;

	UPROPERTY()
	TObjectPtr<UAoCNPCBrainV2> Brain = nullptr;

	public:
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Relations")
	void SetDisposition(AActor* Target, float Value);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Relations")
	void SetRelationship(AActor* Target, const FString& Type, float Value);

};
