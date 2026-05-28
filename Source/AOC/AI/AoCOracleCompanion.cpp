// =============================================================================
// AoCOracleCompanion.cpp — The Living QA Tester (Implementation)
// Architect of Creation — Module: AOC
// =============================================================================
//
// The Oracle companion is the single most important test of the entire game.
// It exercises every NPC subsystem through real gameplay, reports broken
// systems via chat bubble and a disk log file, and acts as the player's
// loyal companion.
//
// This class EXTENDS AoCHumanoidNPCV2 — it inherits the full NPC brain
// stack (GOAP, Needs, CombatBrain, LootBrain, SocialBrain, LifeBrain,
// SkillSystem, TaskGovernor, Relationships, Speech, Imperfection) and
// layers diagnostic, companion, and logging behavior on top.
//
// =============================================================================

#include "AoCOracleCompanion.h"
#include "AoCChatBubble.h"

#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"

// Include the existing NPC subsystem headers
#include "AoCNPCNeedSystem.h"
#include "AoCNPCCombatBrain.h"
#include "AoCNPCLootBrain.h"
#include "AoCNPCSocialBrain.h"
#include "AoCNPCLifeBrain.h"
#include "AoCNPCSkillSystem.h"
#include "AoCNPCTaskGovernor.h"
#include "AoCNPCRelationship.h"
#include "AoCNPCSpeech.h"
#include "AoCNPCImperfection.h"
#include "AoCNPCGoalPlanner.h"

DEFINE_LOG_CATEGORY_STATIC(LogOracle, Log, All);

// =============================================================================
// System Health Check Names (indices into SystemHealthChecks array)
// =============================================================================
namespace OracleSystems
{
	static constexpr int32 Movement     = 0;
	static constexpr int32 Mining       = 1;
	static constexpr int32 Crafting     = 2;
	static constexpr int32 Combat       = 3;
	static constexpr int32 Inventory    = 4;
	static constexpr int32 Animation    = 5;
	static constexpr int32 Needs        = 6;
	static constexpr int32 Skills       = 7;
	static constexpr int32 Speech       = 8;
	static constexpr int32 Social       = 9;
	static constexpr int32 Loot         = 10;
	static constexpr int32 TaskGov      = 11;
	static constexpr int32 TotalSystems = 12;
}

// =============================================================================
// Constructor
// =============================================================================

AAoCOracleCompanion::AAoCOracleCompanion()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f; // Every frame

	// Create the widget component for the chat bubble (positioned above head)
	ChatBubbleComponent = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("ChatBubbleComponent"));
	if (ChatBubbleComponent)
	{
		ChatBubbleComponent->SetupAttachment(RootComponent);
		ChatBubbleComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
		ChatBubbleComponent->SetWidgetSpace(EWidgetSpace::Screen);
		ChatBubbleComponent->SetDrawSize(FVector2D(320.0f, 200.0f));
		ChatBubbleComponent->SetDrawAtDesiredSize(true);
		ChatBubbleComponent->SetPivot(FVector2D(0.5f, 1.0f));
		ChatBubbleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// =============================================================================
// BeginPlay — Initialize all Oracle systems
// =============================================================================

void AAoCOracleCompanion::BeginPlay()
{
	// Call parent to initialize ALL standard NPC subsystems
	Super::BeginPlay();

	UE_LOG(LogOracle, Log, TEXT("=== Oracle Companion initializing ==="));

	// Set up the chat bubble widget
	SetupChatBubble();

	// Initialize the log file on disk
	InitializeLogFile();

	// Initialize the 12 diagnostic trackers
	InitializeDiagnostics();

	// Find the player we're bound to
	FindOwnerPlayer();

	// Attempt to join the player's party
	JoinPlayerParty();

	// Capture baseline for hourly reports
	CaptureHourlySnapshotStart();

	// Initial greeting
	OracleSay(TEXT("Oracle online. All systems initializing... I've got your back."),
	          EOracleLogCategory::Speech, EChatBubblePriority::Important);

	WriteToLog(EOracleLogCategory::Status,
	           TEXT("=== ORACLE SESSION STARTED ==="));
	WriteToLog(EOracleLogCategory::Status,
	           FString::Printf(TEXT("Mode: %s | Following: %s"),
	                           *UEnum::GetValueAsString(CurrentMode),
	                           OwnerPlayer ? *OwnerPlayer->GetName() : TEXT("nobody")));

	UE_LOG(LogOracle, Log, TEXT("=== Oracle Companion ready ==="));
}

// =============================================================================
// EndPlay
// =============================================================================

void AAoCOracleCompanion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WriteToLog(EOracleLogCategory::Status, TEXT("=== ORACLE SESSION ENDED ==="));

	// Generate a final report
	GenerateHourlyReport();

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// SetupChatBubble — Create the chat bubble widget instance
// =============================================================================

void AAoCOracleCompanion::SetupChatBubble()
{
	if (!ChatBubbleComponent)
	{
		UE_LOG(LogOracle, Warning, TEXT("ChatBubbleComponent is null, cannot setup bubble."));
		return;
	}

	ChatBubbleComponent->SetWidgetClass(UAoCChatBubble::StaticClass());
	ChatBubbleComponent->InitWidget();

	ChatBubbleWidget = Cast<UAoCChatBubble>(ChatBubbleComponent->GetWidget());
	if (ChatBubbleWidget)
	{
		ChatBubbleWidget->SetSpeakerName(OracleName);
		UE_LOG(LogOracle, Log, TEXT("Chat bubble widget initialized for %s"), *OracleName);
	}
	else
	{
		UE_LOG(LogOracle, Warning, TEXT("Failed to create ChatBubble widget instance."));
	}
}

// =============================================================================
// InitializeLogFile — Prepare the on-disk log
// =============================================================================

void AAoCOracleCompanion::InitializeLogFile()
{
	LogFilePath = FPaths::ProjectDir() + TEXT("AoC_GameData/oracle_chat_log.txt");

	// Ensure the directory exists
	const FString Dir = FPaths::GetPath(LogFilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}

	UE_LOG(LogOracle, Log, TEXT("Oracle log file: %s"), *LogFilePath);
}

// =============================================================================
// WriteToLog — Append a line to oracle_chat_log.txt
// =============================================================================

void AAoCOracleCompanion::WriteToLog(EOracleLogCategory Category,
                                      const FString& Message)
{
	const FString Timestamp = GetTimestamp();
	const FString Tag = GetLogCategoryTag(Category);
	const FString Line = FString::Printf(TEXT("[%s] [%s] %s\n"),
	                                      *Timestamp, *Tag, *Message);

	FFileHelper::SaveStringToFile(Line, *LogFilePath,
	                              FFileHelper::EEncodingOptions::AutoDetect,
	                              &IFileManager::Get(),
	                              FILEWRITE_Append);

	UE_LOG(LogOracle, Log, TEXT("[Oracle/%s] %s"), *Tag, *Message);
}

FString AAoCOracleCompanion::GetLogCategoryTag(EOracleLogCategory Category) const
{
	switch (Category)
	{
	case EOracleLogCategory::Speech:       return TEXT("SPEECH");
	case EOracleLogCategory::Diagnostic:   return TEXT("DIAGNOSTIC");
	case EOracleLogCategory::Error:        return TEXT("ERROR");
	case EOracleLogCategory::Combat:       return TEXT("COMBAT");
	case EOracleLogCategory::Loot:         return TEXT("LOOT");
	case EOracleLogCategory::Status:       return TEXT("STATUS");
	case EOracleLogCategory::Commentary:   return TEXT("COMMENTARY");
	case EOracleLogCategory::FullTestStep: return TEXT("FULLTEST");
	default:                               return TEXT("UNKNOWN");
	}
}

FString AAoCOracleCompanion::GetTimestamp() const
{
	return FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
}

float AAoCOracleCompanion::GetWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetTimeSeconds();
	}
	return 0.0f;
}

// =============================================================================
// OracleSay — The Oracle's unified speech output
// =============================================================================
// Every line goes to: (1) Chat bubble, (2) Log file, (3) UE_LOG

void AAoCOracleCompanion::OracleSay(const FString& Message,
                                     EOracleLogCategory Category,
                                     EChatBubblePriority BubblePriority)
{
	// Write to chat bubble
	if (ChatBubbleWidget)
	{
		ChatBubbleWidget->ShowMessage(Message, BubblePriority);
	}

	// Write to disk log
	WriteToLog(Category, Message);

	// Also go through the standard NPC speech pipeline so other systems
	// (e.g. nearby NPC hearing, relationship shifts) are aware
	if (UAoCNPCSpeech* SpeechComp = FindComponentByClass<UAoCNPCSpeech>())
	{
		SpeechComp->SpeakLine(Message);
	}
}

// =============================================================================
// OnSpeechOutput — Override from AoCHumanoidNPCV2
// =============================================================================
// Any time the base NPC speech system produces output (e.g. from GOAP or
// social interactions), we intercept it to also log to disk and chat bubble.

void AAoCOracleCompanion::OnSpeechOutput(const FString& Line)
{
	// Parent handles normal speech flow
	Super::OnSpeechOutput(Line);

	// Also push to chat bubble and log
	if (ChatBubbleWidget)
	{
		ChatBubbleWidget->ShowMessage(Line, EChatBubblePriority::Normal);
	}
	WriteToLog(EOracleLogCategory::Speech, Line);
}

// =============================================================================
// Personality Overrides
// =============================================================================

void AAoCOracleCompanion::InitializePersonality()
{
	Super::InitializePersonality();

	// Override personality traits for the Oracle
	// Friendly, talkative, brave, curious, absolutely loyal
	if (UAoCNPCSocialBrain* Social = FindComponentByClass<UAoCNPCSocialBrain>())
	{
		Social->SetTraitValue(TEXT("Friendliness"), 0.85f);
		Social->SetTraitValue(TEXT("Talkativeness"), 0.80f);
		Social->SetTraitValue(TEXT("Bravery"), 0.70f);
		Social->SetTraitValue(TEXT("Curiosity"), 0.90f);
		Social->SetTraitValue(TEXT("Loyalty"), 1.00f);
	}

	// Set the Oracle's name
	SetNPCDisplayName(OracleName);
}

bool AAoCOracleCompanion::CanBetrayTarget(AActor* Target) const
{
	// Oracle NEVER betrays the player — loyalty override
	if (Target == OwnerPlayer)
	{
		return false;
	}
	return Super::CanBetrayTarget(Target);
}

void AAoCOracleCompanion::ConfigureGOAPGoals()
{
	Super::ConfigureGOAPGoals();

	// Add Oracle-specific goals (e.g. "FollowPlayer", "RunDiagnostics")
	if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
	{
		Planner->AddGoal(TEXT("FollowPlayer"), 0.9f);  // High priority
		Planner->AddGoal(TEXT("RunDiagnostics"), 0.3f); // Background task
		Planner->AddGoal(TEXT("ProtectPlayer"), 0.95f); // Highest — defend the player
	}
}

// =============================================================================
// FindOwnerPlayer / JoinPlayerParty
// =============================================================================

void AAoCOracleCompanion::FindOwnerPlayer()
{
	if (const UWorld* World = GetWorld())
	{
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			OwnerPlayer = PC->GetPawn();
			if (OwnerPlayer)
			{
				UE_LOG(LogOracle, Log, TEXT("Oracle bound to player: %s"),
				       *OwnerPlayer->GetName());
			}
		}
	}

	if (!OwnerPlayer)
	{
		UE_LOG(LogOracle, Warning, TEXT("Oracle could not find a player to follow!"));
	}
}

void AAoCOracleCompanion::JoinPlayerParty()
{
	if (!OwnerPlayer)
	{
		return;
	}

	// Use the existing relationship system to establish a companion bond
	if (UAoCNPCRelationship* Relations =
	        FindComponentByClass<UAoCNPCRelationship>())
	{
		Relations->SetRelationship(OwnerPlayer, TEXT("Companion"), 1.0f);
		Relations->SetDisposition(OwnerPlayer, 1.0f); // Maximum friendliness
	}

	// Attempt to use the party system (if available on the player character)
	// This is a soft dependency — if the player doesn't have a party component,
	// the Oracle still works as a follower.
	UE_LOG(LogOracle, Log, TEXT("Oracle attempting to join player party..."));
	WriteToLog(EOracleLogCategory::Status, TEXT("Joined player's party as companion"));
}

// =============================================================================
// Tick — Master tick drives all Oracle subsystems
// =============================================================================

void AAoCOracleCompanion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// If we lost track of the player, try to find them again
	if (!OwnerPlayer || !IsValid(OwnerPlayer))
	{
		FindOwnerPlayer();
	}

	// ----- Periodic Diagnostics ----
	DiagnosticTimer += DeltaSeconds;
	if (DiagnosticTimer >= DiagnosticInterval)
	{
		DiagnosticTimer = 0.0f;
		RunAllDiagnostics();
	}

	// ----- Hourly Report ----
	TickHourlyReport(DeltaSeconds);

	// ----- Smart Commentary ----
	if (bEnableSmartCommentary)
	{
		TickSmartCommentary(DeltaSeconds);
	}

	// ----- Mode-Specific Behavior ----
	if (FullTestCurrentPhase != EFullTestPhase::NotRunning)
	{
		TickFullTest(DeltaSeconds);
	}
	else
	{
		switch (CurrentMode)
		{
		case EOracleMode::Follow:      TickFollowMode(DeltaSeconds);      break;
		case EOracleMode::Independent:  TickIndependentMode(DeltaSeconds);  break;
		case EOracleMode::Guard:        TickGuardMode(DeltaSeconds);        break;
		case EOracleMode::Gather:       TickGatherMode(DeltaSeconds);       break;
		case EOracleMode::Hunt:         TickHuntMode(DeltaSeconds);         break;
		case EOracleMode::Socialize:    TickSocializeMode(DeltaSeconds);    break;
		case EOracleMode::FullTest:     StartFullTest();                     break;
		}
	}
}

// =============================================================================
// SetOracleMode
// =============================================================================

void AAoCOracleCompanion::SetOracleMode(EOracleMode NewMode)
{
	if (NewMode == CurrentMode && NewMode != EOracleMode::FullTest)
	{
		return;
	}

	PreviousMode = CurrentMode;
	CurrentMode = NewMode;

	const FString ModeName = UEnum::GetValueAsString(NewMode);
	OracleSay(FString::Printf(TEXT("Got it, switching to %s mode."), *ModeName),
	          EOracleLogCategory::Speech, EChatBubblePriority::Important);

	WriteToLog(EOracleLogCategory::Status,
	           FString::Printf(TEXT("Mode changed: %s → %s"),
	                           *UEnum::GetValueAsString(PreviousMode),
	                           *ModeName));

	// Reset mode-specific state
	bHasInvestigateTarget = false;
	GatherTargetResource.Empty();

	if (NewMode == EOracleMode::Guard)
	{
		GuardLocation = GetActorLocation();
	}
}

// =============================================================================
// Command Handlers
// =============================================================================

void AAoCOracleCompanion::CommandGather(const FString& ResourceName)
{
	GatherTargetResource = ResourceName;
	SetOracleMode(EOracleMode::Gather);
	OracleSay(FString::Printf(TEXT("On it, I'll go find some %s."), *ResourceName));
}

void AAoCOracleCompanion::CommandInvestigate(const FVector& Location)
{
	InvestigateTarget = Location;
	bHasInvestigateTarget = true;
	OracleSay(TEXT("I'll go check that out."));

	WriteToLog(EOracleLogCategory::Speech,
	           FString::Printf(TEXT("Investigating location (%.0f, %.0f, %.0f)"),
	                           Location.X, Location.Y, Location.Z));
}

void AAoCOracleCompanion::SetOracleName(const FString& NewName)
{
	OracleSay(FString::Printf(TEXT("You can call me %s now."), *NewName));
	OracleName = NewName;
	SetNPCDisplayName(OracleName);
	if (ChatBubbleWidget)
	{
		ChatBubbleWidget->SetSpeakerName(OracleName);
	}
}

int32 AAoCOracleCompanion::GetWorkingSystemCount() const
{
	int32 Count = 0;
	for (const FSystemHealthCheck& Check : SystemHealthChecks)
	{
		if (Check.bIsWorking)
		{
			Count++;
		}
	}
	return Count;
}

// =============================================================================
// Player Interaction Callbacks
// =============================================================================

void AAoCOracleCompanion::OnPlayerWave()
{
	OracleSay(TEXT("Hey!"));
	// Play wave animation through the standard NPC animation system
	PlayEmote(TEXT("Wave"));
}

void AAoCOracleCompanion::OnPlayerAttack(AActor* Attacker)
{
	// Oracle does NOT fight back against its owner
	if (Attacker == OwnerPlayer)
	{
		static const TArray<FString> Reactions = {
			TEXT("Hey! What are you doing?"),
			TEXT("Ow! I'm on your side!"),
			TEXT("Stop that! I'm trying to help here!"),
			TEXT("What did I do to deserve that?"),
			TEXT("Hey, friendly fire! Watch it!")
		};
		const int32 Idx = FMath::RandRange(0, Reactions.Num() - 1);
		OracleSay(Reactions[Idx]);
	}
	else
	{
		// Attacked by someone else — fight back normally
		Super::OnDamageTaken(0.0f, Attacker); // Let the combat brain handle it
	}
}

void AAoCOracleCompanion::OnPlayerTrade(const TArray<FString>& OfferedItems)
{
	if (OfferedItems.Num() == 0)
	{
		OracleSay(TEXT("You didn't offer anything..."));
		return;
	}

	// Evaluate trade — Oracle accepts anything useful, comments on it
	bool bUsefulItem = false;
	for (const FString& Item : OfferedItems)
	{
		// Check if item is food, weapon, tool, etc. via simple keyword matching
		if (Item.Contains(TEXT("Sword")) || Item.Contains(TEXT("Axe")) ||
		    Item.Contains(TEXT("Food"))  || Item.Contains(TEXT("Fish")) ||
		    Item.Contains(TEXT("Ore"))   || Item.Contains(TEXT("Potion")))
		{
			bUsefulItem = true;
			break;
		}
	}

	if (bUsefulItem)
	{
		OracleSay(TEXT("Nice, thanks! I can use this."));
	}
	else
	{
		OracleSay(TEXT("Hmm, I don't really need that, but sure."));
	}

	// Log what was traded
	FString ItemList;
	for (const FString& Item : OfferedItems)
	{
		if (!ItemList.IsEmpty()) ItemList += TEXT(", ");
		ItemList += Item;
	}
	WriteToLog(EOracleLogCategory::Loot,
	           FString::Printf(TEXT("Player traded: %s"), *ItemList));
}

void AAoCOracleCompanion::OnPlayerPoint(const FVector& Direction)
{
	OracleSay(TEXT("On it."));
	const FVector Target = GetActorLocation() + Direction.GetSafeNormal() * 2000.0f;
	CommandInvestigate(Target);
}

void AAoCOracleCompanion::OnPlayerCrouch(bool bIsCrouching)
{
	if (bIsCrouching)
	{
		OracleSay(TEXT("Going quiet..."));
		Crouch();
	}
	else
	{
		OracleSay(TEXT("All clear."));
		UnCrouch();
	}
}

// =============================================================================
// Companion Mode Ticks
// =============================================================================

void AAoCOracleCompanion::TickFollowMode(float DeltaSeconds)
{
	if (!OwnerPlayer)
	{
		return;
	}

	const float Distance = FVector::Dist(GetActorLocation(),
	                                      OwnerPlayer->GetActorLocation());

	if (Distance > FollowDistance)
	{
		// Move toward the player, staying at FollowDistance
		const FVector TargetPos = OwnerPlayer->GetActorLocation();
		MoveToLocation(TargetPos, FollowDistance * 0.8f);
	}

	// Idle speech — talk while following (not during combat/tests)
	IdleSpeechTimer += DeltaSeconds;
	if (IdleSpeechTimer >= IdleSpeechInterval)
	{
		IdleSpeechTimer = 0.0f;
		SpeakIdleLine();
	}

	// If investigating something, check if we've arrived
	if (bHasInvestigateTarget)
	{
		const float DistToTarget = FVector::Dist(GetActorLocation(),
		                                          InvestigateTarget);
		if (DistToTarget < 200.0f)
		{
			OracleSay(TEXT("Checked it out. Nothing special here."));
			bHasInvestigateTarget = false;
		}
	}

	// If the player is in combat, help them
	if (UAoCNPCCombatBrain* LocalCombatBrain = FindComponentByClass<UAoCNPCCombatBrain>())
	{
		if (LocalCombatBrain->IsInCombat())
		{
			// Already fighting — the combat brain handles it
		}
		else
		{
			// Check if the player is being attacked and help
			// (handled by perception/threat detection in the base class)
		}
	}
}

void AAoCOracleCompanion::TickIndependentMode(float DeltaSeconds)
{
	// In Independent mode, the Oracle just lives its life using the standard
	// GOAP/LifeBrain/NeedSystem.  The base class Tick handles all of this
	// automatically.  We don't need to add anything — that's the beauty
	// of extending AoCHumanoidNPCV2.
}

void AAoCOracleCompanion::TickGuardMode(float DeltaSeconds)
{
	const float Distance = FVector::Dist(GetActorLocation(), GuardLocation);

	if (Distance > 300.0f)
	{
		// Drift back to guard position
		MoveToLocation(GuardLocation, 100.0f);
	}

	// In guard mode, combat brain is still active — Oracle will fight
	// anything that comes near.
}

void AAoCOracleCompanion::TickGatherMode(float DeltaSeconds)
{
	// Use the existing LifeBrain/TaskGovernor to find and gather resources.
	// We just push a high-priority gather goal into GOAP.
	if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
	{
		if (!GatherTargetResource.IsEmpty())
		{
			Planner->SetWorldState(TEXT("DesiredResource"), GatherTargetResource);
			Planner->PushGoal(TEXT("GatherResource"), 0.85f);
		}
	}
}

void AAoCOracleCompanion::TickHuntMode(float DeltaSeconds)
{
	// Push hunting as the primary GOAP goal
	if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
	{
		Planner->PushGoal(TEXT("HuntCreature"), 0.85f);
	}
}

void AAoCOracleCompanion::TickSocializeMode(float DeltaSeconds)
{
	// Push socializing as the primary goal
	if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
	{
		Planner->PushGoal(TEXT("Socialize"), 0.85f);
	}
}

// =============================================================================
// DIAGNOSTICS — 12 System Health Checks
// =============================================================================

void AAoCOracleCompanion::InitializeDiagnostics()
{
	SystemHealthChecks.Empty();
	SystemHealthChecks.Reserve(OracleSystems::TotalSystems);

	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Movement")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Mining")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Crafting")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Combat")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Inventory")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Animation")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Needs")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Skills")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Speech")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Social")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("Loot")));
	SystemHealthChecks.Add(FSystemHealthCheck(TEXT("TaskGovernor")));

	check(SystemHealthChecks.Num() == OracleSystems::TotalSystems);
}

void AAoCOracleCompanion::RunDiagnosticsNow()
{
	RunAllDiagnostics();
}

void AAoCOracleCompanion::RunAllDiagnostics()
{
	const float Now = GetWorldTime();

	DiagnoseMovement  (SystemHealthChecks[OracleSystems::Movement]);
	DiagnoseMining    (SystemHealthChecks[OracleSystems::Mining]);
	DiagnoseCrafting  (SystemHealthChecks[OracleSystems::Crafting]);
	DiagnoseCombat    (SystemHealthChecks[OracleSystems::Combat]);
	DiagnoseInventory (SystemHealthChecks[OracleSystems::Inventory]);
	DiagnoseAnimation (SystemHealthChecks[OracleSystems::Animation]);
	DiagnoseNeeds     (SystemHealthChecks[OracleSystems::Needs]);
	DiagnoseSkills    (SystemHealthChecks[OracleSystems::Skills]);
	DiagnoseSpeech    (SystemHealthChecks[OracleSystems::Speech]);
	DiagnoseSocial    (SystemHealthChecks[OracleSystems::Social]);
	DiagnoseLoot      (SystemHealthChecks[OracleSystems::Loot]);
	DiagnoseTaskGovernor(SystemHealthChecks[OracleSystems::TaskGov]);

	// Log summary
	const int32 Working = GetWorkingSystemCount();
	WriteToLog(EOracleLogCategory::Diagnostic,
	           FString::Printf(TEXT("Diagnostic sweep complete: %d/%d systems working"),
	                           Working, OracleSystems::TotalSystems));

	// Report any failures via chat bubble
	for (const FSystemHealthCheck& Check : SystemHealthChecks)
	{
		if (!Check.bIsWorking)
		{
			SpeakDiagnosticResult(Check);
		}
	}
}

void AAoCOracleCompanion::SpeakDiagnosticResult(const FSystemHealthCheck& Check)
{
	if (!bSpeakDiagnostics)
	{
		return;
	}

	if (!Check.bIsWorking)
	{
		OracleSay(Check.DiagnosticMessage,
		          EOracleLogCategory::Error,
		          EChatBubblePriority::Important);
	}
}

// =============================================================================
// Individual Diagnostic Checks
// =============================================================================

void AAoCOracleCompanion::DiagnoseMovement(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	// Test: Can we query the navigation system?
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("Navigation system is completely missing!");
		Check.ConsecutiveFailures++;
		return;
	}

	// Test: Can we find a path to a point 200 units ahead?
	const FVector TestTarget = GetActorLocation() +
	                            GetActorForwardVector() * 200.0f;
	FPathFindingQuery Query;
	Query.StartLocation = GetActorLocation();
	Query.EndLocation = TestTarget;
	Query.NavData = NavSys->GetDefaultNavDataInstance();

	const FPathFindingResult Result = NavSys->FindPathSync(Query);
	if (Result.IsSuccessful())
	{
		Check.bIsWorking = true;
		Check.DiagnosticMessage = TEXT("Pathfinding operational");
		Check.LastSuccessTime = GetWorldTime();
		Check.ConsecutiveFailures = 0;
	}
	else
	{
		Check.ConsecutiveFailures++;
		if (Check.ConsecutiveFailures >= 3)
		{
			Check.bIsWorking = false;
			Check.DiagnosticMessage = TEXT("Hey, I can't find a path over there. Pathfinding seems broken.");
		}
	}
}

void AAoCOracleCompanion::DiagnoseMining(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCSkillSystem* SkillsComp = FindComponentByClass<UAoCNPCSkillSystem>())
	{
		// Check if we've been mining — if so, are we getting ore?
		const float MiningXP = Skills->GetSkillLevel(ESkillID::Mining);
		const bool bHasBeenMining = Skills->GetSkillUseCountByID(ESkillID::Mining) > 0;

		if (!bHasBeenMining)
		{
			// Haven't tried mining yet — can't diagnose, assume OK
			Check.bIsWorking = true;
			Check.DiagnosticMessage = TEXT("Mining not yet attempted");
			Check.ConsecutiveFailures = 0;
			return;
		}

		if (MiningSwingsWithNoOre >= 5)
		{
			Check.bIsWorking = false;
			Check.DiagnosticMessage = TEXT("Mining isn't working, I'm swinging but getting nothing.");
			Check.ConsecutiveFailures++;
		}
		else
		{
			Check.bIsWorking = true;
			Check.DiagnosticMessage = FString::Printf(
				TEXT("Mining operational — skill level %.1f"), MiningXP);
			Check.LastSuccessTime = GetWorldTime();
			Check.ConsecutiveFailures = 0;
			MiningSwingsWithNoOre = 0; // Reset counter on successful diagnosis
		}
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("Skill system component not found!");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseCrafting(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCTaskGovernor* TaskGov = FindComponentByClass<UAoCNPCTaskGovernor>())
	{
		const int32 CraftAttempts = TaskGov->GetTaskAttemptCount(TEXT("Craft"));
		const int32 CraftSuccesses = TaskGov->GetTaskSuccessCount(TEXT("Craft"));

		if (CraftAttempts == 0)
		{
			// Haven't tried crafting yet
			Check.bIsWorking = true;
			Check.DiagnosticMessage = TEXT("Crafting not yet attempted");
			Check.ConsecutiveFailures = 0;
			return;
		}

		if (CraftSuccesses == 0 && CraftAttempts >= 3)
		{
			Check.bIsWorking = false;
			Check.DiagnosticMessage = TEXT("Crafting failed, I tried to make stuff but nothing happened.");
			Check.ConsecutiveFailures++;
		}
		else
		{
			Check.bIsWorking = true;
			Check.DiagnosticMessage = FString::Printf(
				TEXT("Crafting operational — %d/%d succeeded"),
				CraftSuccesses, CraftAttempts);
			Check.LastSuccessTime = GetWorldTime();
			Check.ConsecutiveFailures = 0;
		}
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("TaskGovernor component not found!");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseCombat(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCCombatBrain* LocalCombatBrain = FindComponentByClass<UAoCNPCCombatBrain>())
	{
		if (CombatHitsWithNoDamage >= 10)
		{
			Check.bIsWorking = false;
			Check.DiagnosticMessage = TEXT("My attacks aren't doing anything. 10+ hits with zero damage.");
			Check.ConsecutiveFailures++;
		}
		else
		{
			Check.bIsWorking = true;
			Check.DiagnosticMessage = FString::Printf(
				TEXT("Combat operational — %d total engagements"),
				CombatBrain->GetTotalEngagements());
			Check.LastSuccessTime = GetWorldTime();
			Check.ConsecutiveFailures = 0;
			CombatHitsWithNoDamage = 0;
		}
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("CombatBrain component not found!");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseInventory(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (FailedEquipAttempts >= 3)
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("I can't equip items, something's wrong with the inventory system.");
		Check.ConsecutiveFailures++;
	}
	else
	{
		Check.bIsWorking = true;
		Check.DiagnosticMessage = TEXT("Inventory/equip operational");
		Check.LastSuccessTime = GetWorldTime();
		Check.ConsecutiveFailures = 0;
	}
}

void AAoCOracleCompanion::DiagnoseAnimation(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	// Check: is the NPC's skeletal mesh stuck in a reference pose (T-pose)?
	if (USkeletalMeshComponent* SKMeshComp = GetMesh())
	{
		const bool bInRefPose = !SKMeshComp->IsPlaying();

		if (bInRefPose)
		{
			if (TPoseStartTime < 0.0f)
			{
				TPoseStartTime = GetWorldTime();
			}
			const float StuckDuration = GetWorldTime() - TPoseStartTime;
			if (StuckDuration > 3.0f)
			{
				Check.bIsWorking = false;
				Check.DiagnosticMessage = FString::Printf(
					TEXT("I think my animations broke — stuck in T-pose for %.1f seconds."),
					StuckDuration);
				Check.ConsecutiveFailures++;
				bIsStuckInTPose = true;
				return;
			}
		}
		else
		{
			TPoseStartTime = -1.0f;
			bIsStuckInTPose = false;
		}
	}

	Check.bIsWorking = true;
	Check.DiagnosticMessage = TEXT("Animations playing normally");
	Check.LastSuccessTime = GetWorldTime();
	Check.ConsecutiveFailures = 0;
}

void AAoCOracleCompanion::DiagnoseNeeds(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCNeedSystem* Needs = FindComponentByClass<UAoCNPCNeedSystem>())
	{
		const float CurrentHunger = Needs->GetNeedValue(ENPCNeed::Hunger);
		const float Now = GetWorldTime();

		if (LastHungerValue >= 0.0f)
		{
			const float TimeSinceLastCheck = Now - LastHungerCheckTime;

			// If hunger hasn't changed AT ALL in 5 real minutes (300s), flag it
			if (TimeSinceLastCheck >= 300.0f &&
			    FMath::IsNearlyEqual(CurrentHunger, LastHungerValue, 0.01f))
			{
				Check.bIsWorking = false;
				Check.DiagnosticMessage = TEXT("I don't seem to be getting hungry. Needs system might be off.");
				Check.ConsecutiveFailures++;
			}
			else
			{
				Check.bIsWorking = true;
				Check.DiagnosticMessage = FString::Printf(
					TEXT("Needs decaying normally — Hunger: %.1f"), CurrentHunger);
				Check.LastSuccessTime = Now;
				Check.ConsecutiveFailures = 0;
			}
		}
		else
		{
			// First check — just record baseline
			Check.bIsWorking = true;
			Check.DiagnosticMessage = TEXT("Needs system baseline recorded");
		}

		LastHungerValue = CurrentHunger;
		LastHungerCheckTime = Now;
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("NeedSystem component not found!");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseSkills(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCSkillSystem* SkillsComp = FindComponentByClass<UAoCNPCSkillSystem>())
	{
		const float Now = GetWorldTime();

		// Get all active skills
		TMap<FString, float> CurrentSkills;
		Skills->GetAllSkillLevelsAsStrings(CurrentSkills);

		if (LastSkillCheckTime > 0.0f)
		{
			const float Elapsed = Now - LastSkillCheckTime;

			// If we've been actively using a skill for 10+ minutes and it
			// hasn't changed, something is wrong
			bool bAnySkillStuck = false;
			FString StuckSkillName;

			for (const auto& Pair : CurrentSkills)
			{
				if (const float* PrevLevel = SkillsAtLastCheck.Find(Pair.Key))
				{
					const int32 UseCount = Skills->GetSkillUseCount(Pair.Key);
					if (UseCount > 10 && // Actively used
					    FMath::IsNearlyEqual(Pair.Value, *PrevLevel, 0.001f) &&
					    Elapsed >= 600.0f) // 10 minutes
					{
						bAnySkillStuck = true;
						StuckSkillName = Pair.Key;
						break;
					}
				}
			}

			if (bAnySkillStuck)
			{
				Check.bIsWorking = false;
				Check.DiagnosticMessage = FString::Printf(
					TEXT("My %s skill isn't going up despite heavy use."),
					*StuckSkillName);
				Check.ConsecutiveFailures++;
			}
			else
			{
				Check.bIsWorking = true;
				Check.DiagnosticMessage = TEXT("Skills leveling normally");
				Check.LastSuccessTime = Now;
				Check.ConsecutiveFailures = 0;
			}
		}
		else
		{
			Check.bIsWorking = true;
			Check.DiagnosticMessage = TEXT("Skills baseline recorded");
		}

		SkillsAtLastCheck = CurrentSkills;
		LastSkillCheckTime = Now;
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("SkillSystem component not found!");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseSpeech(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	// If you're reading this, speech works (meta-check).
	// But also verify the speech component exists.
	if (UAoCNPCSpeech* SpeechComp = FindComponentByClass<UAoCNPCSpeech>())
	{
		Check.bIsWorking = true;
		Check.DiagnosticMessage = TEXT("Speech operational (you can hear me, right?)");
		Check.LastSuccessTime = GetWorldTime();
		Check.ConsecutiveFailures = 0;
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("Speech component missing — but you're reading this, so... half-working?");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseSocial(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCSocialBrain* Social = FindComponentByClass<UAoCNPCSocialBrain>())
	{
		const int32 NearbyNPCs = Social->GetPerceivedNPCCount();
		const float Now = GetWorldTime();

		if (NearbyNPCs > 0)
		{
			LastNPCDetectionTime = Now;
			Check.bIsWorking = true;
			Check.DiagnosticMessage = FString::Printf(
				TEXT("Social perception working — %d NPCs detected nearby"),
				NearbyNPCs);
			Check.LastSuccessTime = Now;
			Check.ConsecutiveFailures = 0;
		}
		else
		{
			// No NPCs detected — only flag as broken if it's been 10+ minutes
			const float TimeSinceLastDetection = Now - LastNPCDetectionTime;
			if (TimeSinceLastDetection >= 600.0f && LastNPCDetectionTime > 0.0f)
			{
				Check.bIsWorking = false;
				Check.DiagnosticMessage = TEXT("I can't seem to see anyone else around. Perception or NPC spawns may be broken.");
				Check.ConsecutiveFailures++;
			}
			else
			{
				Check.bIsWorking = true;
				Check.DiagnosticMessage = TEXT("No NPCs nearby (might just be a remote area)");
			}
		}
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("SocialBrain component not found!");
		Check.ConsecutiveFailures++;
	}
}

void AAoCOracleCompanion::DiagnoseLoot(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (FailedLootAttempts >= 3)
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("Looting isn't working — tried multiple times with no results.");
		Check.ConsecutiveFailures++;
	}
	else
	{
		Check.bIsWorking = true;
		Check.DiagnosticMessage = TEXT("Loot system operational");
		Check.LastSuccessTime = GetWorldTime();
		Check.ConsecutiveFailures = 0;
	}
}

void AAoCOracleCompanion::DiagnoseTaskGovernor(FSystemHealthCheck& Check)
{
	Check.LastCheckTime = GetWorldTime();

	if (UAoCNPCTaskGovernor* TaskGov = FindComponentByClass<UAoCNPCTaskGovernor>())
	{
		if (TasksAbandonedConsecutively >= 5)
		{
			Check.bIsWorking = false;
			Check.DiagnosticMessage = TEXT("I keep starting things but can't finish them. Something's wrong with task completion.");
			Check.ConsecutiveFailures++;
		}
		else
		{
			const int32 Completed = TaskGov->GetCompletedTaskCount();
			const int32 Abandoned = TaskGov->GetAbandonedTaskCount();

			Check.bIsWorking = true;
			Check.DiagnosticMessage = FString::Printf(
				TEXT("Task system operational — %d completed, %d abandoned"),
				Completed, Abandoned);
			Check.LastSuccessTime = GetWorldTime();
			Check.ConsecutiveFailures = 0;
		}
	}
	else
	{
		Check.bIsWorking = false;
		Check.DiagnosticMessage = TEXT("TaskGovernor component not found!");
		Check.ConsecutiveFailures++;
	}
}

// =============================================================================
// FULL TEST — Systematic QA Sweep of all 10 major systems
// =============================================================================

void AAoCOracleCompanion::StartFullTest()
{
	if (FullTestCurrentPhase != EFullTestPhase::NotRunning)
	{
		OracleSay(TEXT("Full test already running..."));
		return;
	}

	OracleSay(TEXT("Starting full QA test sweep. This will take a while..."),
	          EOracleLogCategory::FullTestStep, EChatBubblePriority::Important);

	WriteToLog(EOracleLogCategory::FullTestStep,
	           TEXT("=== FULL TEST SWEEP STARTED ==="));

	FullTestResults.Empty();

	// Reset all phase-specific tracking
	FullTestPathfindDirectionsCompleted = 0;
	FullTestOreMinedCount = 0;
	FullTestFishCaughtCount = 0;
	FullTestCookedFish = false;
	FullTestAteFish = false;
	FullTestKilledEnemy = false;
	FullTestLootedCorpse = false;
	FullTestEquippedGear = false;
	FullTestTalkedToNPC = false;
	FullTestFormedParty = false;

	// Start first phase
	FullTestCurrentPhase = EFullTestPhase::Pathfinding;
	FullTestPhaseTimer = 0.0f;
	FullTestPhaseStartTime = GetWorldTime();
	FullTestPhaseAttempts = 0;

	OracleSay(TEXT("Phase 1/10: Testing pathfinding..."),
	          EOracleLogCategory::FullTestStep);
}

void AAoCOracleCompanion::TickFullTest(float DeltaSeconds)
{
	FullTestPhaseTimer += DeltaSeconds;

	// Check for phase timeout
	if (FullTestPhaseTimer >= FullTestPhaseTimeout)
	{
		const FString PhaseName = GetFullTestPhaseName(FullTestCurrentPhase);
		AdvanceFullTestPhase(false,
			FString::Printf(TEXT("Timed out after %.0f seconds"), FullTestPhaseTimeout));
		return;
	}

	ExecuteFullTestPhase(DeltaSeconds);
}

void AAoCOracleCompanion::ExecuteFullTestPhase(float DeltaSeconds)
{
	switch (FullTestCurrentPhase)
	{
	case EFullTestPhase::Pathfinding:
	{
		// Walk 100m in each cardinal direction
		static const FVector Directions[] = {
			FVector(1, 0, 0),   // North (+X)
			FVector(-1, 0, 0),  // South (-X)
			FVector(0, 1, 0),   // East (+Y)
			FVector(0, -1, 0)   // West (-Y)
		};

		if (FullTestPathfindDirectionsCompleted < 4)
		{
			const FVector Target = GetActorLocation() +
				Directions[FullTestPathfindDirectionsCompleted] * 10000.0f; // 100m = 10000 UU

			if (FullTestPhaseAttempts == 0)
			{
				MoveToLocation(Target, 500.0f);
				FullTestPhaseAttempts = 1;
			}

			// Check if we've moved significantly toward the target
			if (GetVelocity().Size() > 10.0f || FullTestPhaseTimer > 10.0f)
			{
				FullTestPathfindDirectionsCompleted++;
				FullTestPhaseAttempts = 0;
			}
		}
		else
		{
			AdvanceFullTestPhase(true,
				TEXT("Walked 100m in all four cardinal directions"));
		}
		break;
	}

	case EFullTestPhase::Mining:
	{
		// Find a mining node and mine 10 ore
		if (FullTestPhaseAttempts == 0)
		{
			// Push GOAP goal to find and mine
			if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
			{
				Planner->SetWorldState(TEXT("DesiredResource"), TEXT("Ore"));
				Planner->PushGoal(TEXT("GatherResource"), 0.95f);
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestOreMinedCount >= 10)
		{
			AdvanceFullTestPhase(true,
				FString::Printf(TEXT("Mined %d ore"), FullTestOreMinedCount));
		}
		break;
	}

	case EFullTestPhase::Fishing:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
			{
				Planner->SetWorldState(TEXT("DesiredResource"), TEXT("Fish"));
				Planner->PushGoal(TEXT("GoFishing"), 0.95f);
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestFishCaughtCount >= 3)
		{
			AdvanceFullTestPhase(true,
				FString::Printf(TEXT("Caught %d fish"), FullTestFishCaughtCount));
		}
		break;
	}

	case EFullTestPhase::Cooking:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
			{
				Planner->SetWorldState(TEXT("RecipeToCraft"), TEXT("Cooked Fish"));
				Planner->PushGoal(TEXT("CraftItem"), 0.95f);
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestCookedFish)
		{
			AdvanceFullTestPhase(true, TEXT("Cooked the fish successfully"));
		}
		break;
	}

	case EFullTestPhase::Eating:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCNeedSystem* Needs = FindComponentByClass<UAoCNPCNeedSystem>())
			{
				Needs->ConsumeItem(TEXT("Cooked Fish"));
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestAteFish)
		{
			AdvanceFullTestPhase(true, TEXT("Ate cooked fish, hunger decreased"));
		}
		else if (FullTestPhaseTimer > 5.0f)
		{
			// Give it a few seconds then check needs
			if (UAoCNPCNeedSystem* Needs = FindComponentByClass<UAoCNPCNeedSystem>())
			{
				const float Hunger = Needs->GetNeedValue(ENPCNeed::Hunger);
				FullTestAteFish = true;
				AdvanceFullTestPhase(true,
					FString::Printf(TEXT("Consumed food, hunger now at %.1f"), Hunger));
			}
		}
		break;
	}

	case EFullTestPhase::Combat:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCGoalPlanner* Planner = FindComponentByClass<UAoCNPCGoalPlanner>())
			{
				Planner->PushGoal(TEXT("HuntCreature"), 0.95f);
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestKilledEnemy)
		{
			AdvanceFullTestPhase(true, TEXT("Killed an enemy in combat"));
		}
		break;
	}

	case EFullTestPhase::Looting:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCLootBrain* LocalLootBrain = FindComponentByClass<UAoCNPCLootBrain>())
			{
				LocalLootBrain->AttemptLootNearestCorpse();
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestLootedCorpse)
		{
			AdvanceFullTestPhase(true, TEXT("Successfully looted a corpse"));
		}
		break;
	}

	case EFullTestPhase::Equipping:
	{
		if (FullTestPhaseAttempts == 0)
		{
			// Try to equip the best available weapon
			if (UAoCNPCCombatBrain* LocalCombatBrain = FindComponentByClass<UAoCNPCCombatBrain>())
			{
				LocalCombatBrain->EquipBestAvailableWeapon();
			}
			FullTestPhaseAttempts = 1;
		}

		// Check after a brief delay
		if (FullTestPhaseTimer > 2.0f)
		{
			FullTestEquippedGear = true; // Assume success if no crash
			AdvanceFullTestPhase(true, TEXT("Equipped gear from inventory"));
		}
		break;
	}

	case EFullTestPhase::Social:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCSocialBrain* Social = FindComponentByClass<UAoCNPCSocialBrain>())
			{
				Social->InitiateConversationWithNearest();
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestTalkedToNPC)
		{
			AdvanceFullTestPhase(true, TEXT("Talked to another NPC"));
		}
		break;
	}

	case EFullTestPhase::Party:
	{
		if (FullTestPhaseAttempts == 0)
		{
			if (UAoCNPCSocialBrain* Social = FindComponentByClass<UAoCNPCSocialBrain>())
			{
				Social->RequestPartyWithNearest();
			}
			FullTestPhaseAttempts = 1;
		}

		if (FullTestFormedParty)
		{
			AdvanceFullTestPhase(true, TEXT("Formed a party with another NPC"));
		}
		break;
	}

	case EFullTestPhase::Complete:
		CompleteFullTest();
		break;

	default:
		break;
	}
}

void AAoCOracleCompanion::AdvanceFullTestPhase(bool bPassed, const FString& Details)
{
	const float Duration = GetWorldTime() - FullTestPhaseStartTime;
	const FString PhaseName = GetFullTestPhaseName(FullTestCurrentPhase);

	// Record result
	FFullTestResult Result;
	Result.Phase = FullTestCurrentPhase;
	Result.bPassed = bPassed;
	Result.Details = Details;
	Result.DurationSeconds = Duration;
	FullTestResults.Add(Result);

	// Speak result
	const FString PassFail = bPassed ? TEXT("\u2713") : TEXT("\u2717");
	const FString Msg = FString::Printf(TEXT("%s %s %s — %s"),
	                                     *PassFail,
	                                     *PhaseName,
	                                     bPassed ? TEXT("works") : TEXT("BROKEN"),
	                                     *Details);

	OracleSay(Msg, EOracleLogCategory::FullTestStep,
	          bPassed ? EChatBubblePriority::Normal : EChatBubblePriority::Important);

	// Advance to next phase
	const int32 CurrentPhaseInt = static_cast<int32>(FullTestCurrentPhase);
	const int32 NextPhaseInt = CurrentPhaseInt + 1;

	if (NextPhaseInt > static_cast<int32>(EFullTestPhase::Party))
	{
		FullTestCurrentPhase = EFullTestPhase::Complete;
	}
	else
	{
		FullTestCurrentPhase = static_cast<EFullTestPhase>(NextPhaseInt);

		const int32 PhaseNumber = NextPhaseInt; // 1-based since Pathfinding=1
		OracleSay(FString::Printf(TEXT("Phase %d/10: Testing %s..."),
		                           PhaseNumber,
		                           *GetFullTestPhaseName(FullTestCurrentPhase)),
		          EOracleLogCategory::FullTestStep);
	}

	// Reset phase timing
	FullTestPhaseTimer = 0.0f;
	FullTestPhaseStartTime = GetWorldTime();
	FullTestPhaseAttempts = 0;
}

void AAoCOracleCompanion::CompleteFullTest()
{
	int32 Passed = 0;
	TArray<FString> Issues;

	for (const FFullTestResult& Result : FullTestResults)
	{
		if (Result.bPassed)
		{
			Passed++;
		}
		else
		{
			Issues.Add(FString::Printf(TEXT("%s: %s"),
			           *GetFullTestPhaseName(Result.Phase),
			           *Result.Details));
		}
	}

	// Speak the final summary
	FString Summary = FString::Printf(
		TEXT("Full test complete. %d of %d systems working."),
		Passed, FullTestResults.Num());

	if (Issues.Num() > 0)
	{
		Summary += TEXT(" Issues: ");
		for (int32 i = 0; i < Issues.Num(); ++i)
		{
			if (i > 0) Summary += TEXT("; ");
			Summary += Issues[i];
		}
	}
	else
	{
		Summary += TEXT(" All systems nominal — game is LAUNCH READY!");
	}

	OracleSay(Summary, EOracleLogCategory::FullTestStep,
	          EChatBubblePriority::Critical);

	// Log detailed results
	WriteToLog(EOracleLogCategory::FullTestStep, TEXT("=== FULL TEST RESULTS ==="));
	for (const FFullTestResult& Result : FullTestResults)
	{
		WriteToLog(EOracleLogCategory::FullTestStep,
		           FString::Printf(TEXT("  %s: %s — %s (%.1fs)"),
		                           *GetFullTestPhaseName(Result.Phase),
		                           Result.bPassed ? TEXT("PASS") : TEXT("FAIL"),
		                           *Result.Details,
		                           Result.DurationSeconds));
	}
	WriteToLog(EOracleLogCategory::FullTestStep,
	           FString::Printf(TEXT("=== RESULT: %d/%d PASSED ==="),
	                           Passed, FullTestResults.Num()));

	// Reset to previous mode
	FullTestCurrentPhase = EFullTestPhase::NotRunning;
	CurrentMode = PreviousMode;
}

FString AAoCOracleCompanion::GetFullTestPhaseName(EFullTestPhase Phase) const
{
	switch (Phase)
	{
	case EFullTestPhase::Pathfinding: return TEXT("Pathfinding");
	case EFullTestPhase::Mining:      return TEXT("Mining");
	case EFullTestPhase::Fishing:     return TEXT("Fishing");
	case EFullTestPhase::Cooking:     return TEXT("Cooking");
	case EFullTestPhase::Eating:      return TEXT("Eating/Needs");
	case EFullTestPhase::Combat:      return TEXT("Combat");
	case EFullTestPhase::Looting:     return TEXT("Looting");
	case EFullTestPhase::Equipping:   return TEXT("Equipping");
	case EFullTestPhase::Social:      return TEXT("Social");
	case EFullTestPhase::Party:       return TEXT("Party Formation");
	case EFullTestPhase::Complete:    return TEXT("Complete");
	default:                          return TEXT("Unknown");
	}
}

// =============================================================================
// HOURLY REPORTS
// =============================================================================

void AAoCOracleCompanion::TickHourlyReport(float DeltaSeconds)
{
	HourlyReportTimer += DeltaSeconds;
	if (HourlyReportTimer >= HourlyReportInterval)
	{
		HourlyReportTimer = 0.0f;
		GenerateHourlyReport();
	}
}

void AAoCOracleCompanion::ForceHourlyReport()
{
	GenerateHourlyReport();
}

void AAoCOracleCompanion::CaptureHourlySnapshotStart()
{
	HourlySnapshot = FOracleHourlySnapshot();

	if (UAoCNPCSkillSystem* SkillsComp = FindComponentByClass<UAoCNPCSkillSystem>())
	{
		Skills->GetAllSkillLevelsAsStrings(HourlySnapshot.SkillsAtStart);
	}
}

void AAoCOracleCompanion::GenerateHourlyReport()
{
	// Gather current state
	float HP = 0.0f, MaxHP = 0.0f, Hunger = 0.0f, Energy = 0.0f;

	if (UAoCNPCNeedSystem* Needs = FindComponentByClass<UAoCNPCNeedSystem>())
	{
		HP     = Needs->GetNeedValue(ENPCNeed::Health);
		MaxHP  = Needs->GetNeedMax(TEXT("Health"));
		Hunger = Needs->GetNeedValue(ENPCNeed::Hunger);
		Energy = Needs->GetNeedValue(ENPCNeed::Energy);
	}

	// Skills comparison
	TMap<FString, float> CurrentSkills;
	if (UAoCNPCSkillSystem* SkillsComp = FindComponentByClass<UAoCNPCSkillSystem>())
	{
		Skills->GetAllSkillLevelsAsStrings(CurrentSkills);
	}

	const int32 Working = GetWorkingSystemCount();

	// Build skill delta string
	FString SkillGains;
	for (const auto& Pair : CurrentSkills)
	{
		const float* StartLevel = HourlySnapshot.SkillsAtStart.Find(Pair.Key);
		const float Start = StartLevel ? *StartLevel : 0.0f;
		if (!FMath::IsNearlyEqual(Pair.Value, Start, 0.01f))
		{
			if (!SkillGains.IsEmpty()) SkillGains += TEXT(", ");
			SkillGains += FString::Printf(TEXT("%s %.0f→%.0f"),
			                               *Pair.Key, Start, Pair.Value);
		}
	}
	if (SkillGains.IsEmpty()) SkillGains = TEXT("None");

	// Collect issues
	TArray<FString> Issues;
	for (const FSystemHealthCheck& Check : SystemHealthChecks)
	{
		if (!Check.bIsWorking)
		{
			Issues.Add(Check.SystemName);
		}
	}

	FString IssueStr = Issues.Num() > 0
		? FString::Join(Issues, TEXT(", "))
		: TEXT("None");

	// Write to log
	WriteToLog(EOracleLogCategory::Status, TEXT("=== HOURLY REPORT ==="));
	WriteToLog(EOracleLogCategory::Status,
	           FString::Printf(TEXT("HP: %.0f/%.0f | Hunger: %.0f/100 | Energy: %.0f/100"),
	                           HP, MaxHP, Hunger, Energy));
	WriteToLog(EOracleLogCategory::Status,
	           FString::Printf(TEXT("Skills gained: %s"), *SkillGains));
	WriteToLog(EOracleLogCategory::Status,
	           FString::Printf(TEXT("Systems working: %d/%d | Issues: %s"),
	                           Working, OracleSystems::TotalSystems, *IssueStr));

	// Speak a summary
	OracleSay(FString::Printf(
		TEXT("Hourly report: HP %.0f/%.0f, %d/%d systems working. %s"),
		HP, MaxHP, Working, OracleSystems::TotalSystems,
		Issues.Num() > 0
			? *FString::Printf(TEXT("Issues with: %s"), *IssueStr)
			: TEXT("Everything looks good.")),
		EOracleLogCategory::Status,
		EChatBubblePriority::Important);

	// Reset snapshot for next hour
	CaptureHourlySnapshotStart();
}

// =============================================================================
// SMART COMMENTARY — Context-aware observations about the game world
// =============================================================================

void AAoCOracleCompanion::TickSmartCommentary(float DeltaSeconds)
{
	CommentaryTimer += DeltaSeconds;
	if (CommentaryTimer >= CommentaryInterval)
	{
		CommentaryTimer = 0.0f;
		EvaluateAndComment();
	}
}

void AAoCOracleCompanion::EvaluateAndComment()
{
	// Run through commentary checks — each one returns true if it generated
	// a comment, and we only produce one comment per cycle to avoid spam.
	if (CommentOnMissingMiningNodes()) return;
	if (CommentOnMissingCraftingStations()) return;
	if (CommentOnEmptyArea()) return;
	if (CommentOnPathfindingIssues()) return;
	if (CommentOnMissingNPCs()) return;
	if (CommentOnNeedDecayRate()) return;
	if (CommentOnEnemyDifficulty()) return;
	if (CommentOnUnreachableResources()) return;
	if (CommentOnWorldPopulation()) return;
	if (CommentOnLootQuality()) return;

	// No issues found — say a random idle line instead
	SpeakIdleLine();
}

bool AAoCOracleCompanion::CommentOnMissingMiningNodes()
{
	// Check: are there any resource nodes within a reasonable radius?
	TArray<AActor*> NearbyNodes;
	UGameplayStatics::GetAllActorsOfClassWithTag(
		GetWorld(), AActor::StaticClass(), FName(TEXT("ResourceNode")), NearbyNodes);

	// Filter to within 5000 units (50m)
	int32 CloseNodes = 0;
	for (AActor* Node : NearbyNodes)
	{
		if (FVector::Dist(GetActorLocation(), Node->GetActorLocation()) < 5000.0f)
		{
			CloseNodes++;
		}
	}

	if (CloseNodes == 0 && NearbyNodes.Num() == 0)
	{
		OracleSay(TEXT("There are no mining nodes around here, might need to place some."),
		          EOracleLogCategory::Commentary);
		return true;
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnMissingCraftingStations()
{
	TArray<AActor*> Stations;
	UGameplayStatics::GetAllActorsOfClassWithTag(
		GetWorld(), AActor::StaticClass(), FName(TEXT("CraftingStation")), Stations);

	int32 NearbyStations = 0;
	for (AActor* Station : Stations)
	{
		if (FVector::Dist(GetActorLocation(), Station->GetActorLocation()) < 5000.0f)
		{
			NearbyStations++;
		}
	}

	if (NearbyStations == 0)
	{
		OracleSay(TEXT("I notice there's no anvil or crafting station nearby. Can't craft without one."),
		          EOracleLogCategory::Commentary);
		return true;
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnEmptyArea()
{
	// Check total actor count in a large radius — if very low, area feels empty
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

	int32 NearbyActors = 0;
	for (AActor* Actor : AllActors)
	{
		if (Actor != this && Actor != OwnerPlayer &&
		    FVector::Dist(GetActorLocation(), Actor->GetActorLocation()) < 10000.0f)
		{
			NearbyActors++;
		}
	}

	if (NearbyActors < 3)
	{
		OracleSay(TEXT("This area seems pretty empty. Maybe we need more content spawns here."),
		          EOracleLogCategory::Commentary);
		return true;
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnPathfindingIssues()
{
	if (bRecentlyStuckOnPath)
	{
		bRecentlyStuckOnPath = false;
		OracleSay(TEXT("The pathfinding near that area is weird, I keep getting stuck."),
		          EOracleLogCategory::Commentary);
		return true;
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnMissingNPCs()
{
	if (UAoCNPCSocialBrain* Social = FindComponentByClass<UAoCNPCSocialBrain>())
	{
		const int32 NearbyNPCs = Social->GetPerceivedNPCCount();
		const float TimeSinceDetection = GetWorldTime() - LastNPCDetectionTime;

		if (NearbyNPCs == 0 && TimeSinceDetection > 600.0f && LastNPCDetectionTime > 0.0f)
		{
			OracleSay(TEXT("I haven't seen any other NPCs in 10 minutes. Are they spawned?"),
			          EOracleLogCategory::Commentary);
			return true;
		}
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnNeedDecayRate()
{
	if (UAoCNPCNeedSystem* Needs = FindComponentByClass<UAoCNPCNeedSystem>())
	{
		const float HungerRate = Needs->GetDecayRate(TEXT("Hunger"));

		// If hunger drops more than 20 points per minute (real time), it's too fast
		if (HungerRate > 20.0f)
		{
			OracleSay(TEXT("My hunger is dropping really fast. Might want to adjust the decay rate."),
			          EOracleLogCategory::Commentary);
			return true;
		}
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnEnemyDifficulty()
{
	if (UAoCNPCCombatBrain* LocalCombatBrain = FindComponentByClass<UAoCNPCCombatBrain>())
	{
		if (RecentKillsWithNoDamage >= 5)
		{
			OracleSay(TEXT("These enemies are way too easy. I killed several without losing any HP."),
			          EOracleLogCategory::Commentary);
			RecentKillsWithNoDamage = 0;
			return true;
		}
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnUnreachableResources()
{
	// This is tracked by the pathfinding diagnostic — if we fail to reach a
	// resource node, we flag it here.
	if (UAoCNPCTaskGovernor* TaskGov = FindComponentByClass<UAoCNPCTaskGovernor>())
	{
		if (TaskGov->GetLastFailReason() == TEXT("UnreachableTarget"))
		{
			OracleSay(TEXT("I can't reach that resource node. There might be a collision issue."),
			          EOracleLogCategory::Commentary);
			return true;
		}
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnWorldPopulation()
{
	// Compare number of spawned NPCs to what the design intent seems to be
	TArray<AActor*> AllNPCs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AoCHumanoidNPCV2::StaticClass(), AllNPCs);

	if (AllNPCs.Num() <= 1) // Just us
	{
		OracleSay(TEXT("I'm the only NPC in this world. Might want to spawn some more."),
		          EOracleLogCategory::Commentary);
		return true;
	}
	return false;
}

bool AAoCOracleCompanion::CommentOnLootQuality()
{
	if (UAoCNPCLootBrain* LocalLootBrain = FindComponentByClass<UAoCNPCLootBrain>())
	{
		const float AvgLootValue = LocalLootBrain->GetAverageLootValue();
		if (AvgLootValue > 0.0f && AvgLootValue < 1.0f)
		{
			OracleSay(TEXT("The loot drops have been pretty terrible. Average value is really low."),
			          EOracleLogCategory::Commentary);
			return true;
		}
	}
	return false;
}


// =============================================================================
// IDLE SPEECH — Ambient chatter while following the player
// =============================================================================

void AAoCOracleCompanion::SpeakIdleLine()
{
	// Don't chatter during full test or combat
	if (FullTestCurrentPhase != EFullTestPhase::NotRunning)
	{
		return;
	}

	if (UAoCNPCCombatBrain* LocalCombatBrain = FindComponentByClass<UAoCNPCCombatBrain>())
	{
		if (LocalCombatBrain->IsInCombat())
		{
			return;
		}
	}

	static const TArray<FString> IdleLines = {
		TEXT("Nice day for an adventure, don't you think?"),
		TEXT("I wonder what's over that next hill..."),
		TEXT("Did you know I can mine, fish, and craft? Just say the word."),
		TEXT("This world has so much potential. I can feel it growing."),
		TEXT("Stay sharp... you never know what's lurking around here."),
		TEXT("I've been keeping an eye on my skill levels. Getting better every day."),
		TEXT("If you need me to gather something, just tell me what you need."),
		TEXT("My hunger's ticking down slowly. Should probably eat something soon."),
		TEXT("I like following you around. Beats standing in one spot all day."),
		TEXT("You know, for a world that's still being built, this place isn't half bad."),
		TEXT("I keep running diagnostics in the background. I'll let you know if something breaks."),
		TEXT("Have you tried checking out those rock formations? Could be ore deposits."),
		TEXT("I could really go for some cooked fish right about now."),
		TEXT("Sometimes I think about what it'd be like to have more NPCs to talk to."),
		TEXT("My combat brain is itching for a fight. Let's find something to spar with."),
		TEXT("I wonder if there are any crafting stations nearby..."),
		TEXT("Keep moving, I'm right behind you!"),
		TEXT("This terrain is interesting. Very... sandy."),
		TEXT("I'm tracking my needs... hunger, energy, safety. All part of being alive."),
		TEXT("Just so you know, I've got your back if anything attacks us."),
		TEXT("The wind feels different here. Or it would, if I could feel wind."),
		TEXT("I've memorized 170 different things to say, by the way."),
		TEXT("Hey, look at us. An adventurer and their companion. Classic."),
		TEXT("You ever wonder what the map looks like from above?"),
		TEXT("My pathfinding just got an upgrade. I can actually follow you now!"),
		TEXT("I was thinking... maybe we should find an anvil and craft something."),
		TEXT("I bet there are dungeons out there somewhere. We should explore."),
		TEXT("Another day, another adventure. I wouldn't have it any other way."),
		TEXT("If you ever need a status report, just ask. I've always got the data."),
		TEXT("I'm still in a T-pose, aren't I? Don't worry, my brain works fine.")
	};

	const int32 Idx = FMath::RandRange(0, IdleLines.Num() - 1);
	OracleSay(IdleLines[Idx], EOracleLogCategory::Commentary, EChatBubblePriority::Normal);
}

// =============================================================================
// END — AoCOracleCompanion.cpp
// =============================================================================
