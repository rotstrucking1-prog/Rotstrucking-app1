// AoCNPCSpeech.h
// Architect of Creation — NPC Contextual Speech System
//
// NPCs communicate through the Local Chat channel using lines indistinguishable from
// real player messages. Speech is casual, uses slang/abbreviations, occasionally has
// typos, and is modulated by personality (talkative vs quiet, aggressive vs polite).
//
// CRITICAL DESIGN RULES:
// 1. Lines sound like REAL PLAYER CHAT — casual, abbreviations (lol, gg, ngl, tbh)
// 2. NEVER "Hail, traveler!" or "Well met, adventurer!" — instant NPC giveaway
// 3. Personality affects tone: Aggressive = slang/threats, Polite = courteous, Grumpy = curt
// 4. Max speech once per 15-30 seconds; many actions are SILENT
// 5. Output to Local Chat channel using the NPC's player-like name
// 6. Talkative trait (0.0 - 1.0) controls frequency — a 0.1 NPC barely ever talks
// 7. 1-2% chance of typos for very talkative NPCs: "ncie" → "nice", "teh" → "the"
// 8. Random delay before speaking (0.5-3s) — humans don't respond instantly
// 9. NEVER speak during stealth/hiding
// 10. Speak more in a party, less when alone
//
// Contains 300+ built-in speech lines organized by ESpeechContext. Each context has
// 3-8 variations so NPCs don't repeat themselves. Lines are compiled into the binary
// as static data — no external file loading needed.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCSpeech.generated.h"

// Forward declarations
class AAoCNPCCharacter;
class UAoCNPCBrainV2;
class UAoCNPCRelationship;

// ============================================================================
// ENUMS
// ============================================================================

/**
 * Every context in which an NPC might speak. Each maps to a bank of speech lines.
 * The speech system picks a line from the appropriate bank based on context + personality.
 */
UENUM(BlueprintType)
enum class ESpeechContext : uint8
{
	// --- GREETINGS ---
	GreetStranger,
	GreetFriend,
	GreetEnemy,
	Farewell,

	// --- PARTY ---
	PartyInviteAccept,
	PartyInviteDecline,
	PartyInviteOffer,
	PartyLootFair,
	PartyLootUnfair,
	PartyLootAngry,
	PartyLeaving,
	PartyBetray,
	PartyEncourage,

	// --- COMBAT ---
	CombatEngage,
	CombatTaunt,
	CombatLowHP,
	CombatKilledEnemy,
	CombatDying,
	CombatCallForHelp,
	CombatWatchOut,
	CombatNiceKill,
	CombatRunAway,
	CombatVictory,

	// --- GATHERING / CRAFTING ---
	MiningFound,
	MiningTired,
	MiningNodeEmpty,
	WoodcuttingChop,
	CraftingSuccess,
	CraftingFailed,
	CraftingStarting,
	FishingBite,
	FishingNothing,
	FishingCaught,
	CookingDone,
	SmeltingDone,
	GatheringHerb,

	// --- TRADING ---
	TradeOffer,
	TradeAccept,
	TradeDecline,
	TradeHaggle,
	TradeThanks,

	// --- SOCIAL ---
	SmallTalk,
	Gossip,
	Warning,
	Compliment,
	Insult,
	AskForHelp,
	OfferHelp,
	ThankYou,
	Apologize,
	Joke,

	// --- EMOTIONAL / NEEDS ---
	Hungry,
	Tired,
	Happy,
	Angry,
	Scared,
	Bored,
	Excited,
	Frustrated,

	// --- RELATIONSHIP ---
	TrustGained,
	TrustLost,
	BetrayalAccusation,
	ForgivenessOffer,
	RevengeWarning,
	GrudgeReminder,
	AllianceProposal,

	// --- ENVIRONMENTAL ---
	WeatherComment,
	LocationComment,
	TimeOfDayComment,
	SpottedSomething,
	SpottedDanger,
	SpottedResource,
	NightfallComment,
	DawnComment,

	MAX UMETA(Hidden)
};

/**
 * Personality tone filter for selecting appropriate lines.
 * "Any" matches all personalities.
 */
UENUM(BlueprintType)
enum class ESpeechTone : uint8
{
	Any,
	Aggressive,
	Passive,
	Polite,
	Grumpy,
	Friendly,
	Sarcastic,
	Nervous,
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * A single speech line entry in the database.
 */
USTRUCT(BlueprintType)
struct FSpeechLine
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESpeechContext Context = ESpeechContext::SmallTalk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESpeechTone Tone = ESpeechTone::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Line;

	/** Optional: minimum relationship level to use this line (default Stranger = no restriction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinRelationshipLevel = 0;
};

/**
 * Queued speech event waiting to be delivered (with delay).
 */
USTRUCT()
struct FPendingSpeech
{
	GENERATED_BODY()

	UPROPERTY()
	FString Line;

	UPROPERTY()
	float DeliverAtTime = 0.0f;

	UPROPERTY()
	ESpeechContext Context = ESpeechContext::SmallTalk;
};

/**
 * Speech cooldown tracking per context to prevent spam.
 */
USTRUCT()
struct FSpeechCooldown
{
	GENERATED_BODY()

	UPROPERTY()
	float LastSpokeTime = -100.0f;

	UPROPERTY()
	float CooldownDuration = 20.0f;
};

// ============================================================================
// DELEGATES
// ============================================================================

/** Fired when the NPC actually speaks a line to Local Chat */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNPCSpeech, const FString&, SpeakerName, const FString&, Line, ESpeechContext, Context);

// ============================================================================
// MAIN CLASS
// ============================================================================

/**
 * UAoCNPCSpeech — Contextual NPC communication system.
 *
 * Contains 300+ built-in speech lines that sound like real player chat.
 * Lines are triggered by game events (combat start, loot found, needs critical, etc.)
 * and filtered by personality/talkative trait before output. Output goes to
 * the Local Chat channel, indistinguishable from a real player typing.
 *
 * Speech features:
 * - Personality-matched line selection (aggressive, polite, grumpy, etc.)
 * - Talkative trait (0.0-1.0) controls overall speech frequency
 * - Per-context cooldowns prevent the same type of speech from spamming
 * - Random 0.5-3s delay before speaking (humans aren't instant)
 * - Occasional typos for talkative NPCs (~1-2% chance)
 * - Context-aware: speaks more in parties, less when alone
 * - Never speaks during stealth or when it would be unrealistic
 * - Recently used lines tracked to avoid immediate repetition
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCSpeech : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCSpeech();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -----------------------------------------------------------
	// SPEECH TRIGGERING
	// -----------------------------------------------------------

	/**
	 * Attempt to trigger speech for a given context. Respects cooldowns, talkative trait,
	 * and stealth checks. May queue speech with a random delay instead of speaking immediately.
	 * Returns true if speech was queued/delivered, false if suppressed.
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	bool TriggerSpeech(ESpeechContext Context);

	/**
	 * Trigger speech with a specific target entity (for relationship-dependent lines).
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	bool TriggerSpeechWithTarget(ESpeechContext Context, const FString& TargetEntityId);

	/**
	 * Force speech immediately, bypassing cooldowns and talkative checks.
	 * Use sparingly — only for critical moments like betrayal announcements.
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	void ForceSpeech(ESpeechContext Context);

	/**
	 * Say an exact line (for special cases like responding to player text).
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	void SayExactLine(const FString& Line);

	// -----------------------------------------------------------
	// CONFIGURATION
	// -----------------------------------------------------------

	/** Set how talkative this NPC is (0.0 = almost never speaks, 1.0 = chatterbox) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	void SetTalkativeLevel(float Level) { TalkativeLevel = FMath::Clamp(Level, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "AoC|Speech")
	float GetTalkativeLevel() const { return TalkativeLevel; }

	/** Set the NPC's dominant personality tone for line selection */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	void SetPersonalityTone(ESpeechTone Tone) { DominantTone = Tone; }

	UFUNCTION(BlueprintPure, Category = "AoC|Speech")
	ESpeechTone GetPersonalityTone() const { return DominantTone; }

	/** Set whether the NPC is currently in stealth (suppresses all speech) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	void SetStealthMode(bool bStealth) { bInStealth = bStealth; }

	/** Set whether the NPC is currently in a party (increases speech frequency) */
	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
	void SetInParty(bool bParty) { bIsInParty = bParty; }

	// -----------------------------------------------------------
	// QUERIES
	// -----------------------------------------------------------

	/** Get the total number of speech lines in the database */
	UFUNCTION(BlueprintPure, Category = "AoC|Speech")
	int32 GetTotalSpeechLineCount() const { return SpeechDatabase.Num(); }

	/** Get how many lines exist for a specific context */
	UFUNCTION(BlueprintPure, Category = "AoC|Speech")
	int32 GetLineCountForContext(ESpeechContext Context) const;

	/** Is this NPC currently in a speech cooldown for ANY context? */
	UFUNCTION(BlueprintPure, Category = "AoC|Speech")
	bool IsOnGlobalCooldown() const;

	UFUNCTION(BlueprintPure, Category = "AoC|Speech")
	FString GetDebugString() const;

	// -----------------------------------------------------------
	// DELEGATES
	// -----------------------------------------------------------

	/** Fired when the NPC delivers a line to Local Chat */
	UPROPERTY(BlueprintAssignable, Category = "AoC|Speech")
	FOnNPCSpeech OnNPCSpeech;

protected:

	// -----------------------------------------------------------
	// LINE SELECTION
	// -----------------------------------------------------------

	/** Select a line for the given context, matching personality and avoiding recent repeats */
	FString SelectLine(ESpeechContext Context) const;

	/** Apply random typos to a line (1-2% chance per character for talkative NPCs) */
	FString ApplyTypos(const FString& Line) const;

	/** Apply personality-specific text transformations (e.g., lowercase everything for lazy NPCs) */
	FString ApplyPersonalityFilter(const FString& Line) const;

	// -----------------------------------------------------------
	// DELIVERY
	// -----------------------------------------------------------

	/** Actually output the line to Local Chat */
	void DeliverLine(const FString& Line, ESpeechContext Context);

	/** Process pending speech queue */
	void TickPendingSpeech(float DeltaTime);

	// -----------------------------------------------------------
	// COOLDOWNS
	// -----------------------------------------------------------

	/** Check if a specific context is on cooldown */
	bool IsContextOnCooldown(ESpeechContext Context) const;

	/** Set cooldown for a context */
	void SetContextCooldown(ESpeechContext Context, float Duration);

	/** Get the cooldown duration for a context type */
	float GetCooldownForContext(ESpeechContext Context) const;

	// -----------------------------------------------------------
	// DATABASE INIT
	// -----------------------------------------------------------

	/** Build the entire speech line database. Called once at BeginPlay. */
	void InitializeSpeechDatabase();

	// -----------------------------------------------------------
	// DATA
	// -----------------------------------------------------------

	/** The complete speech line database */
	UPROPERTY()
	TArray<FSpeechLine> SpeechDatabase;

	/** Pending speech waiting to be delivered (with delay) */
	UPROPERTY()
	TArray<FPendingSpeech> PendingSpeechQueue;

	/** Per-context cooldown tracking */
	UPROPERTY()
	TMap<ESpeechContext, FSpeechCooldown> ContextCooldowns;

	/** Recently used line indices (to avoid immediate repetition). Ring buffer of last 20 lines. */
	UPROPERTY()
	TArray<int32> RecentlyUsedLines;

	UPROPERTY()
	int32 RecentLineIndex = 0;

	/** Global speech cooldown — time of last speech output */
	UPROPERTY()
	float LastGlobalSpeechTime = -100.0f;

	/** How many lines this NPC has spoken total (for stats) */
	UPROPERTY()
	int32 TotalLinesSpokeCount = 0;

	// -----------------------------------------------------------
	// CONFIGURATION
	// -----------------------------------------------------------

	/** How talkative this NPC is. 0.0 = almost never speaks. 1.0 = chatterbox. */
	UPROPERTY(EditAnywhere, Category = "AoC|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TalkativeLevel = 0.5f;

	/** The dominant personality tone for line selection */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	ESpeechTone DominantTone = ESpeechTone::Any;

	/** Minimum seconds between ANY speech output (global cooldown) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float GlobalSpeechCooldown = 15.0f;

	/** Maximum global cooldown (for quiet NPCs the cooldown is longer) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float MaxGlobalCooldown = 45.0f;

	/** Min delay before speech is delivered after triggering (seconds) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float MinSpeechDelay = 0.5f;

	/** Max delay before speech is delivered after triggering (seconds) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float MaxSpeechDelay = 3.0f;

	/** Chance of typos per character (0.0 - 0.02 for talkative NPCs) */
	UPROPERTY(EditAnywhere, Category = "AoC|Config")
	float TypoChance = 0.012f;

	/** Is the NPC currently in stealth? (suppresses all speech) */
	UPROPERTY()
	bool bInStealth = false;

	/** Is the NPC currently in a party? (modifies speech frequency) */
	UPROPERTY()
	bool bIsInParty = false;

	/** Max recently used lines to track (avoid repeats) */
	static constexpr int32 MaxRecentLines = 20;

	// -----------------------------------------------------------
	// CACHED REFERENCES
	// -----------------------------------------------------------

	UPROPERTY()
	TObjectPtr<AAoCNPCCharacter> OwnerNPC = nullptr;

	UPROPERTY()
	TObjectPtr<UAoCNPCRelationship> RelationshipComp = nullptr;

	UFUNCTION(BlueprintCallable, Category = "AoC|Speech")
public:
	void SpeakLine(const FString& Line) { SayExactLine(Line); }

};
