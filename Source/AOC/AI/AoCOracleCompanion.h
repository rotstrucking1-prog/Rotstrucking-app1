// =============================================================================
// AoCOracleCompanion.h — The Living QA Tester / Permanent Player Companion
// Architect of Creation — Module: AOC
// =============================================================================
//
// PURPOSE:
//   The Oracle is a SPECIALIZED AoCHumanoidNPCV2 that serves as the player's
//   permanent companion AND a continuous system-health monitor.  It uses every
//   NPC subsystem (GOAP, Needs, CombatBrain, LootBrain, SocialBrain, LifeBrain,
//   SkillSystem, TaskGovernor, Relationships, Speech, Imperfection) and adds:
//
//     1. SELF-DIAGNOSIS  — Every 30-60 s it checks 12 game systems and
//        reports failures via chat bubble AND a disk log file.
//     2. COMPANION MODES — Follow, Independent, Guard, Gather, Hunt,
//        Socialize, and the all-important FullTest QA sweep.
//     3. CHAT LOG        — Every line the Oracle speaks is appended to
//        AoC_GameData/oracle_chat_log.txt so an external agent can parse it.
//     4. HOURLY REPORTS  — Comprehensive status summary each real-time hour.
//     5. SMART COMMENTARY — Context-aware observations about the game world.
//     6. PLAYER INTERACTION — Reacts to waves, attacks, trades, crouching, etc.
//
// DESIGN PHILOSOPHY:
//   If the Oracle can live a full life (mine, eat, fight, craft, socialize,
//   level up) without hitting a broken system, the game is LAUNCH-READY.
//   The Oracle is the litmus test for the entire project.
//
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "AoCHumanoidNPCV2.h"
#include "AoCChatBubble.h"
#include "AoCOracleCompanion.generated.h"

class UAoCChatBubble;
class UWidgetComponent;

// ---------------------------------------------------------------------------
// EOracleMode — What the Oracle is currently doing
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EOracleMode : uint8
{
	Follow       UMETA(DisplayName = "Follow Player"),
	Independent  UMETA(DisplayName = "Live Independently"),
	Guard        UMETA(DisplayName = "Guard Area"),
	Gather       UMETA(DisplayName = "Gather Resources"),
	Hunt         UMETA(DisplayName = "Hunt Creatures"),
	Socialize    UMETA(DisplayName = "Socialize with NPCs"),
	FullTest     UMETA(DisplayName = "Full QA Test Sweep")
};

// ---------------------------------------------------------------------------
// EOracleLogCategory — Tag for each log line
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EOracleLogCategory : uint8
{
	Speech,
	Diagnostic,
	Error,
	Combat,
	Loot,
	Status,
	Commentary,
	FullTestStep
};

// ---------------------------------------------------------------------------
// EFullTestPhase — Stages of the systematic QA sweep
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EFullTestPhase : uint8
{
	NotRunning,
	Pathfinding,    // Walk 100m N/S/E/W
	Mining,         // Find node, mine 10 ore
	Fishing,        // Find water, fish 3 fish
	Cooking,        // Cook the fish
	Eating,         // Eat the fish (needs test)
	Combat,         // Find enemy, fight
	Looting,        // Loot the corpse
	Equipping,      // Equip any gear found
	Social,         // Find NPC, talk
	Party,          // Form party with NPC
	Complete        // Report results
};

// ---------------------------------------------------------------------------
// FSystemHealthCheck — One monitored subsystem's diagnostic state
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FSystemHealthCheck
{
	GENERATED_BODY()

	/** Human-readable name (e.g. "Movement", "Mining") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SystemName;

	/** True if last check passed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsWorking = true;

	/** Explanation of current state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString DiagnosticMessage;

	/** World time of the last check */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LastCheckTime = 0.0f;

	/** World time of the last SUCCESSFUL check */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LastSuccessTime = 0.0f;

	/** How many consecutive checks have failed */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ConsecutiveFailures = 0;

	FSystemHealthCheck() = default;
	explicit FSystemHealthCheck(const FString& InName)
		: SystemName(InName) {}
};

// ---------------------------------------------------------------------------
// FFullTestResult — Result of one phase in the FullTest sweep
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FFullTestResult
{
	GENERATED_BODY()

	UPROPERTY()
	EFullTestPhase Phase = EFullTestPhase::NotRunning;

	UPROPERTY()
	bool bPassed = false;

	UPROPERTY()
	FString Details;

	UPROPERTY()
	float DurationSeconds = 0.0f;
};

// ---------------------------------------------------------------------------
// FOracleHourlySnapshot — Data for hourly reports
// ---------------------------------------------------------------------------
USTRUCT()
struct FOracleHourlySnapshot
{
	GENERATED_BODY()

	float HP = 0.0f;
	float MaxHP = 0.0f;
	float Hunger = 0.0f;
	float Energy = 0.0f;

	TMap<FString, float> SkillsAtStart;   // Skill → level at hour start
	TMap<FString, float> SkillsNow;       // Skill → level now

	int32 ItemsGathered = 0;
	int32 ItemsCrafted = 0;
	int32 FightsWon = 0;
	int32 FightsLost = 0;
	int32 SystemsWorking = 0;
	int32 SystemsTotal = 0;
	TArray<FString> Issues;
};

// ---------------------------------------------------------------------------
// AAoCOracleCompanion — The Oracle NPC
// ---------------------------------------------------------------------------
UCLASS(Blueprintable, meta = (DisplayName = "Oracle Companion NPC"))
class AOC_API AAoCOracleCompanion : public AoCHumanoidNPCV2
{
	GENERATED_BODY()

public:
	AAoCOracleCompanion();

	// ----- Lifecycle -------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ----- Commands (player can call these) --------------------------------

	/** Change the Oracle's operational mode. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle")
	void SetOracleMode(EOracleMode NewMode);

	/** Get current mode. */
	UFUNCTION(BlueprintPure, Category = "AoC|Oracle")
	EOracleMode GetOracleMode() const { return CurrentMode; }

	/** Command the Oracle to gather a specific resource. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle")
	void CommandGather(const FString& ResourceName);

	/** Command Oracle to investigate a world location. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle")
	void CommandInvestigate(const FVector& Location);

	/** Rename the Oracle. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle")
	void SetOracleName(const FString& NewName);

	/** Get the Oracle's display name. */
	UFUNCTION(BlueprintPure, Category = "AoC|Oracle")
	FString GetOracleName() const { return OracleName; }

	/** Get all current system health data. */
	UFUNCTION(BlueprintPure, Category = "AoC|Oracle")
	const TArray<FSystemHealthCheck>& GetSystemHealth() const { return SystemHealthChecks; }

	/** Get the number of systems currently reporting as working. */
	UFUNCTION(BlueprintPure, Category = "AoC|Oracle")
	int32 GetWorkingSystemCount() const;

	/** Is the FullTest currently running? */
	UFUNCTION(BlueprintPure, Category = "AoC|Oracle")
	bool IsFullTestRunning() const { return FullTestCurrentPhase != EFullTestPhase::NotRunning; }

	/** Force an immediate diagnostic check of all systems. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle")
	void RunDiagnosticsNow();

	/** Force an immediate hourly report. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle")
	void ForceHourlyReport();

	// ----- Player Interaction Callbacks ------------------------------------

	/** Called when the player performs a gesture/emote near the Oracle. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Interaction")
	void OnPlayerWave();

	/** Called when the player attacks the Oracle. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Interaction")
	void OnPlayerAttack(AActor* Attacker);

	/** Called when the player initiates a trade. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Interaction")
	void OnPlayerTrade(const TArray<FString>& OfferedItems);

	/** Called when the player points in a direction. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Interaction")
	void OnPlayerPoint(const FVector& Direction);

	/** Called when the player crouches or un-crouches. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Oracle|Interaction")
	void OnPlayerCrouch(bool bIsCrouching);

	// ----- Configuration ---------------------------------------------------

	/** Interval between diagnostic sweeps (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config",
	          meta = (ClampMin = "10", ClampMax = "300"))
	float DiagnosticInterval = 45.0f;

	/** Interval between hourly reports (seconds). 3600 = 1 real hour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config")
	float HourlyReportInterval = 3600.0f;

	/** Interval between smart commentary observations (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config")
	float CommentaryInterval = 45.0f;

	/** How close the Oracle tries to stay to the player in Follow mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config",
	          meta = (ClampMin = "50", ClampMax = "1000"))
	float FollowDistance = 250.0f;

	/** Maximum time a single FullTest phase is allowed before it's marked failed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config")
	float FullTestPhaseTimeout = 120.0f;

	/** Whether the Oracle should speak diagnostic results aloud (via chat bubble). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config")
	bool bSpeakDiagnostics = true;

	/** Whether the Oracle should make smart commentary about the game world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Oracle|Config")
	bool bEnableSmartCommentary = true;

protected:
	// ----- Overrides from AoCHumanoidNPCV2 --------------------------------

	/** Override to inject Oracle personality traits. */
	virtual void InitializePersonality();

	/** Override to prevent Oracle from ever betraying the player. */
	virtual bool CanBetrayTarget(AActor* Target) const;

	/** Override to add Oracle-specific GOAP goals. */
	virtual void ConfigureGOAPGoals();

	/** Override speech output to also log to disk. */
	virtual void OnSpeechOutput(const FString& Line);

private:
	// ----- Identity --------------------------------------------------------
	UPROPERTY()
	FString OracleName = TEXT("Oracle");

	UPROPERTY()
	EOracleMode CurrentMode = EOracleMode::Follow;

	UPROPERTY()
	EOracleMode PreviousMode = EOracleMode::Follow;

	// ----- Chat Bubble (attached as WidgetComponent) -----------------------
	UPROPERTY()
	UWidgetComponent* ChatBubbleComponent = nullptr;

	UPROPERTY()
	UAoCChatBubble* ChatBubbleWidget = nullptr;

	void SetupChatBubble();

	// ----- Self-Diagnosis --------------------------------------------------
	UPROPERTY()
	TArray<FSystemHealthCheck> SystemHealthChecks;

	float DiagnosticTimer = 0.0f;

	void InitializeDiagnostics();
	void RunAllDiagnostics();

	// Individual system checks (12 total)
	void DiagnoseMovement(FSystemHealthCheck& Check);
	void DiagnoseMining(FSystemHealthCheck& Check);
	void DiagnoseCrafting(FSystemHealthCheck& Check);
	void DiagnoseCombat(FSystemHealthCheck& Check);
	void DiagnoseInventory(FSystemHealthCheck& Check);
	void DiagnoseAnimation(FSystemHealthCheck& Check);
	void DiagnoseNeeds(FSystemHealthCheck& Check);
	void DiagnoseSkills(FSystemHealthCheck& Check);
	void DiagnoseSpeech(FSystemHealthCheck& Check);
	void DiagnoseSocial(FSystemHealthCheck& Check);
	void DiagnoseLoot(FSystemHealthCheck& Check);
	void DiagnoseTaskGovernor(FSystemHealthCheck& Check);

	// Tracking data for diagnostics
	int32 MiningSwingsWithNoOre = 0;
	int32 CombatHitsWithNoDamage = 0;
	int32 FailedEquipAttempts = 0;
	int32 FailedLootAttempts = 0;
	int32 TasksAbandonedConsecutively = 0;
	float LastHungerValue = -1.0f;
	float LastHungerCheckTime = 0.0f;
	float LastSkillCheckTime = 0.0f;
	TMap<FString, float> SkillsAtLastCheck;
	float LastNPCDetectionTime = 0.0f;
	float TPoseStartTime = -1.0f;    // -1 = not in T-pose
	bool  bIsStuckInTPose = false;

	// ----- Companion Behavior ----------------------------------------------
	UPROPERTY()
	AActor* OwnerPlayer = nullptr;

	FVector GuardLocation = FVector::ZeroVector;
	FString GatherTargetResource;
	FVector InvestigateTarget = FVector::ZeroVector;
	bool    bHasInvestigateTarget = false;

	void TickFollowMode(float DeltaSeconds);
	void TickIndependentMode(float DeltaSeconds);
	void TickGuardMode(float DeltaSeconds);
	void TickGatherMode(float DeltaSeconds);
	void TickHuntMode(float DeltaSeconds);
	void TickSocializeMode(float DeltaSeconds);

	/** Find the owning player (first player controller's pawn). */
	void FindOwnerPlayer();

	/** Attempt to join the player's party at startup. */
	void JoinPlayerParty();

	// ----- FullTest QA Sweep -----------------------------------------------
	UPROPERTY()
	EFullTestPhase FullTestCurrentPhase = EFullTestPhase::NotRunning;

	UPROPERTY()
	TArray<FFullTestResult> FullTestResults;

	float FullTestPhaseTimer = 0.0f;
	float FullTestPhaseStartTime = 0.0f;
	int32 FullTestPhaseAttempts = 0;

	// Phase-specific tracking
	int32 FullTestPathfindDirectionsCompleted = 0;
	int32 FullTestOreMinedCount = 0;
	int32 FullTestFishCaughtCount = 0;
	bool  FullTestCookedFish = false;
	bool  FullTestAteFish = false;
	bool  FullTestKilledEnemy = false;
	bool  FullTestLootedCorpse = false;
	bool  FullTestEquippedGear = false;
	bool  FullTestTalkedToNPC = false;
	bool  FullTestFormedParty = false;

	void StartFullTest();
	void TickFullTest(float DeltaSeconds);
	void AdvanceFullTestPhase(bool bPassed, const FString& Details);
	void CompleteFullTest();
	FString GetFullTestPhaseName(EFullTestPhase Phase) const;
	void ExecuteFullTestPhase(float DeltaSeconds);

	// ----- Chat Log to Disk ------------------------------------------------
	FString LogFilePath;

	void InitializeLogFile();
	void WriteToLog(EOracleLogCategory Category, const FString& Message);
	FString GetLogCategoryTag(EOracleLogCategory Category) const;

	// ----- Hourly Reports --------------------------------------------------
	float HourlyReportTimer = 0.0f;
	FOracleHourlySnapshot HourlySnapshot;

	void TickHourlyReport(float DeltaSeconds);
	void GenerateHourlyReport();
	void CaptureHourlySnapshotStart();

	// ----- Smart Commentary ------------------------------------------------
	float CommentaryTimer = 0.0f;
	float LastCommentaryTime = 0.0f;

	void TickSmartCommentary(float DeltaSeconds);
	void EvaluateAndComment();

	// Commentary checks (return true if they produced a comment)
	bool CommentOnMissingMiningNodes();
	bool CommentOnMissingCraftingStations();
	bool CommentOnEmptyArea();
	bool CommentOnPathfindingIssues();
	bool CommentOnMissingNPCs();
	bool CommentOnNeedDecayRate();
	bool CommentOnEnemyDifficulty();
	bool CommentOnUnreachableResources();
	bool CommentOnWorldPopulation();
	bool CommentOnLootQuality();

	// Commentary tracking
	int32 RecentKillsWithNoDamage = 0;
	float LastPathStuckTime = 0.0f;
	bool  bRecentlyStuckOnPath = false;

	// ----- Idle Speech (ambient chatter while following) --------------------
	float IdleSpeechTimer = 0.0f;
	float IdleSpeechInterval = 30.0f;  // Speak every ~30 seconds when idle

	/** Speak a random ambient/idle line. */
	void SpeakIdleLine();

	// ----- Oracle Speech (wrapper that also logs) --------------------------

	/** Say something via chat bubble AND write to log. */
	void OracleSay(const FString& Message,
	               EOracleLogCategory Category = EOracleLogCategory::Speech,
	               EChatBubblePriority BubblePriority = EChatBubblePriority::Normal);

	/** Speak a diagnostic result aloud if bSpeakDiagnostics is on. */
	void SpeakDiagnosticResult(const FSystemHealthCheck& Check);

	// ----- Utility ---------------------------------------------------------
	FString GetTimestamp() const;
	float GetWorldTime() const;
};
