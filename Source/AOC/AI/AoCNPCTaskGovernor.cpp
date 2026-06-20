// AoCNPCTaskGovernor.cpp
// Architect of Creation — NPC Task Governor Implementation
//
// Enforces realistic timing, completion verification, retry/blacklist logic,
// stuck detection, replan cooldowns, long-term goal tracking, and daily schedules.

#include "AoCNPCCharacter.h"
#include "AoCNPCTaskGovernor.h"
#include "AoCNPCBrainV2.h"
#include "AoCNPCSkillSystem.h"
#include "AoCNPCImperfection.h"
#include "AoCHumanoidNPCV2.h"
#include "AoCAILODManager.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogTaskGovernor, Log, All);

// ============================================================================
// CONSTRUCTOR / LIFECYCLE
// ============================================================================

UAoCNPCTaskGovernor::UAoCNPCTaskGovernor()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10Hz base tick — AI LOD may reduce this

	PositionHistory.SetNum(MaxPositionSamples);
}

void UAoCNPCTaskGovernor::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AAoCNPCCharacter>(GetOwner());
	if (OwnerNPC)
	{
		Brain = OwnerNPC->FindComponentByClass<UAoCNPCBrainV2>();
		SkillSystem = OwnerNPC->FindComponentByClass<UAoCNPCSkillSystem>();
	}

	InitializeTimingTable();
	GenerateScheduleFromPersonality();

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Task Governor initialized with %d timing entries, %d schedule blocks"),
		OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"),
		TimingTable.Num(),
		DailySchedule.Num());
}

void UAoCNPCTaskGovernor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerNPC) return;

	// AI LOD gating — check tick budget
	// Background NPCs tick governor at 1Hz, active NPCs at 10Hz
	// (Actual LOD check delegated to AoCAILODManager externally setting TickInterval)

	TickActiveAction(DeltaTime);
	TickTransition(DeltaTime);
	TickStuckDetection(DeltaTime);
	TickBlacklistExpiry(DeltaTime);
	TickSchedule(DeltaTime);
	TickGoalProgress(DeltaTime);
}

// ============================================================================
// TIMING TABLE INITIALIZATION
// ============================================================================

void UAoCNPCTaskGovernor::InitializeTimingTable()
{
	// Build the realistic timing table. These are the REAL durations that make
	// NPCs feel human. A mining swing is 3.5 seconds. Crafting a chestplate is
	// 4-6 minutes. These numbers are sacred — don't change them lightly.

	auto AddTiming = [this](ENPCActionType Type, float Min, float Max, bool bRandomize, float SkillFactor)
	{
		FAoCActionTimingData Data;
		Data.MinDuration = Min;
		Data.MaxDuration = Max;
		Data.bRandomizeDuration = bRandomize;
		Data.SkillSpeedFactor = SkillFactor;
		TimingTable.Add(Type, Data);
	};

	// --- MOVEMENT ---
	// Movement durations are distance-dependent; these are per-segment minimums
	AddTiming(ENPCActionType::MoveTo,		0.5f,	600.0f,	false,	0.0f);
	AddTiming(ENPCActionType::Sprint,		0.5f,	120.0f,	false,	0.0f);
	AddTiming(ENPCActionType::Swim,			1.0f,	300.0f,	false,	0.0f);

	// --- GATHERING ---
	// Each swing/action is a discrete unit. Nodes require multiple swings.
	AddTiming(ENPCActionType::MiningSwing,		3.0f,	4.0f,	true,	0.15f);  // 3-4s per swing, skill reduces slightly
	AddTiming(ENPCActionType::WoodcuttingChop,	2.5f,	3.5f,	true,	0.15f);  // 2.5-3.5s per chop
	AddTiming(ENPCActionType::FishingCast,		1.5f,	2.5f,	true,	0.0f);   // Cast is fixed ~2s
	AddTiming(ENPCActionType::FishingWait,		15.0f,	90.0f,	true,	0.3f);   // 15-90s wait, skill helps a LOT
	AddTiming(ENPCActionType::FishingReel,		3.0f,	5.0f,	true,	0.1f);   // 3-5s reel
	AddTiming(ENPCActionType::HerbGather,		8.0f,	12.0f,	true,	0.2f);   // 8-12s per herb
	AddTiming(ENPCActionType::SkinCorpse,		15.0f,	25.0f,	true,	0.2f);   // 15-25s per corpse
	AddTiming(ENPCActionType::HuntTrack,		5.0f,	30.0f,	true,	0.25f);  // Tracking varies
	AddTiming(ENPCActionType::FarmPlant,		4.0f,	8.0f,	true,	0.1f);
	AddTiming(ENPCActionType::FarmHarvest,		6.0f,	12.0f,	true,	0.1f);

	// --- PROCESSING ---
	AddTiming(ENPCActionType::SmeltOre,		10.0f,	14.0f,	true,	0.1f);   // ~12s per bar
	AddTiming(ENPCActionType::SawLogs,		6.0f,	10.0f,	true,	0.1f);   // ~8s per plank
	AddTiming(ENPCActionType::TanHide,		10.0f,	18.0f,	true,	0.1f);
	AddTiming(ENPCActionType::GrindHerbs,	5.0f,	10.0f,	true,	0.1f);

	// --- CRAFTING ---
	// These are FULL craft times. A chestplate takes 4-6 REAL MINUTES.
	AddTiming(ENPCActionType::CraftDagger,		45.0f,	60.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftSword,		90.0f,	120.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftGreatsword,	120.0f,	180.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftAxe,			80.0f,	110.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftHelmet,		120.0f,	180.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftChestplate,	240.0f,	360.0f,	true,	0.2f);   // 4-6 MINUTES
	AddTiming(ENPCActionType::CraftLeggings,	180.0f,	270.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftGauntlets,	90.0f,	150.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftBoots,		90.0f,	150.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftShield,		100.0f,	160.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftBow,			80.0f,	120.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftStaff,		70.0f,	100.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftJewelry,		60.0f,	90.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CraftPotion,		45.0f,	90.0f,	true,	0.2f);
	AddTiming(ENPCActionType::CookMeal,			30.0f,	60.0f,	true,	0.15f);
	AddTiming(ENPCActionType::BrewDrink,		45.0f,	90.0f,	true,	0.15f);
	AddTiming(ENPCActionType::Enchant,			60.0f,	120.0f,	true,	0.2f);
	AddTiming(ENPCActionType::TailorCloth,		80.0f,	140.0f,	true,	0.2f);
	AddTiming(ENPCActionType::LeatherworkItem,	70.0f,	120.0f,	true,	0.2f);

	// --- BUILDING ---
	AddTiming(ENPCActionType::LayStone,		20.0f,	30.0f,	true,	0.15f);
	AddTiming(ENPCActionType::PlaceBeam,	15.0f,	25.0f,	true,	0.15f);
	AddTiming(ENPCActionType::BuildWall,	60.0f,	120.0f,	true,	0.15f);
	AddTiming(ENPCActionType::BuildRoof,	90.0f,	150.0f,	true,	0.15f);

	// --- COMBAT --- (faster actions, but still not instant)
	AddTiming(ENPCActionType::MeleeAttack,		0.5f,	2.5f,	true,	0.0f);
	AddTiming(ENPCActionType::RangedAttack,		1.0f,	3.0f,	true,	0.0f);
	AddTiming(ENPCActionType::CastSpell,		0.8f,	4.0f,	true,	0.0f);
	AddTiming(ENPCActionType::Block,			0.2f,	5.0f,	false,	0.0f);
	AddTiming(ENPCActionType::Dodge,			0.3f,	1.0f,	false,	0.0f);
	AddTiming(ENPCActionType::UseConsumable,	1.0f,	2.0f,	true,	0.0f);

	// --- SOCIAL ---
	AddTiming(ENPCActionType::Speak,			1.0f,	5.0f,	true,	0.0f);
	AddTiming(ENPCActionType::Trade,			5.0f,	30.0f,	true,	0.0f);
	AddTiming(ENPCActionType::PartyInteract,	2.0f,	10.0f,	true,	0.0f);

	// --- LIFE ---
	AddTiming(ENPCActionType::Eat,			8.0f,	15.0f,	true,	0.0f);
	AddTiming(ENPCActionType::Drink,		5.0f,	10.0f,	true,	0.0f);
	AddTiming(ENPCActionType::Sleep,		300.0f,	600.0f,	false,	0.0f);  // 5-10 min (game time compressed)
	AddTiming(ENPCActionType::Rest,			30.0f,	120.0f,	true,	0.0f);
	AddTiming(ENPCActionType::SitDown,		1.5f,	3.0f,	true,	0.0f);
	AddTiming(ENPCActionType::StandUp,		1.0f,	2.0f,	true,	0.0f);

	// --- MISC ---
	AddTiming(ENPCActionType::Idle,				2.0f,	30.0f,	true,	0.0f);
	AddTiming(ENPCActionType::LookAround,		2.0f,	5.0f,	true,	0.0f);
	AddTiming(ENPCActionType::CheckInventory,	2.0f,	5.0f,	true,	0.0f);
	AddTiming(ENPCActionType::Emote,			2.0f,	6.0f,	true,	0.0f);
	AddTiming(ENPCActionType::Loot,				3.0f,	8.0f,	true,	0.0f);
}

// ============================================================================
// ACTION MANAGEMENT
// ============================================================================

bool UAoCNPCTaskGovernor::RequestAction(ENPCActionType ActionType, const FString& TargetId, FVector TargetLocation)
{
	if (!CanAcceptNewAction())
	{
		UE_LOG(LogTaskGovernor, Verbose, TEXT("[%s] Action request denied — current action still in progress (Phase: %d, Type: %d)"),
			*GetOwner()->GetName(), (int32)CurrentAction.Phase, (int32)CurrentAction.ActionType);
		return false;
	}

	// Check blacklist
	if (!TargetId.IsEmpty() && IsTargetBlacklisted(TargetId))
	{
		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Action request denied — target '%s' is blacklisted"),
			*GetOwner()->GetName(), *TargetId);
		return false;
	}

	// Store previous action type for transition blending
	PreviousActionType = CurrentAction.ActionType;

	// Build the new action
	FNPCActiveAction NewAction;
	NewAction.ActionType = ActionType;
	NewAction.Phase = EActionPhase::Transitioning; // Start with transition blend
	NewAction.TargetId = TargetId;
	NewAction.TargetLocation = TargetLocation;
	NewAction.RetryCount = 0;

	// Compute duration from timing table + skill level
	NewAction.ExpectedDuration = ComputeActionDuration(ActionType);
	NewAction.TimeoutDuration = GetActionTimeout(ActionType);
	NewAction.TransitionDuration = GetTransitionDuration(PreviousActionType, ActionType);

	// Snapshot pre-action state for verification
	CurrentAction = NewAction;
	SnapshotPreActionState();

	// Reset transition timer
	TransitionTimer = 0.0f;

	// Reset stuck detection
	StuckRecoveryAttempts = 0;

	Stats.TotalActionsAttempted++;

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Action started: Type=%d, Target='%s', Duration=%.1fs, Timeout=%.1fs, Transition=%.1fs"),
		*GetOwner()->GetName(), (int32)ActionType, *TargetId,
		NewAction.ExpectedDuration, NewAction.TimeoutDuration, NewAction.TransitionDuration);

	return true;
}

void UAoCNPCTaskGovernor::CancelCurrentAction(EReplanReason Reason)
{
	if (CurrentAction.ActionType == ENPCActionType::None) return;

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Action cancelled: Type=%d, Reason=%d"),
		*GetOwner()->GetName(), (int32)CurrentAction.ActionType, (int32)Reason);

	CurrentAction.Phase = EActionPhase::Cancelled;
	CurrentAction.ActionType = ENPCActionType::None;

	// Combat interrupts bypass replan cooldown
	if (Reason == EReplanReason::CombatInterrupt)
	{
		LastReplanTime = -100.0f; // Reset cooldown
	}
}

bool UAoCNPCTaskGovernor::IsActionInProgress() const
{
	return CurrentAction.ActionType != ENPCActionType::None &&
		   CurrentAction.Phase != EActionPhase::Completed &&
		   CurrentAction.Phase != EActionPhase::Failed &&
		   CurrentAction.Phase != EActionPhase::Cancelled;
}

bool UAoCNPCTaskGovernor::CanAcceptNewAction() const
{
	// Can accept if no action in progress, or current action is done/failed/cancelled
	if (!IsActionInProgress()) return true;

	// Combat actions can always interrupt non-combat
	return false;
}

bool UAoCNPCTaskGovernor::IsTargetBlacklisted(const FString& TargetId) const
{
	for (const FBlacklistedTarget& Entry : BlacklistedTargets)
	{
		if (Entry.TargetId == TargetId)
		{
			return true;
		}
	}
	return false;
}

// ============================================================================
// TICK — ACTIVE ACTION
// ============================================================================

void UAoCNPCTaskGovernor::TickActiveAction(float DeltaTime)
{
	if (CurrentAction.ActionType == ENPCActionType::None) return;
	if (CurrentAction.Phase == EActionPhase::Completed || 
		CurrentAction.Phase == EActionPhase::Failed ||
		CurrentAction.Phase == EActionPhase::Cancelled) return;

	// Skip transition phase — that's handled in TickTransition
	if (CurrentAction.Phase == EActionPhase::Transitioning) return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float ElapsedTime = CurrentTime - CurrentAction.StartTime;

	// --- TIMEOUT CHECK ---
	if (ElapsedTime > CurrentAction.TimeoutDuration)
	{
		UE_LOG(LogTaskGovernor, Warning, TEXT("[%s] Action TIMEOUT: Type=%d after %.1fs (max=%.1fs)"),
			*GetOwner()->GetName(), (int32)CurrentAction.ActionType,
			ElapsedTime, CurrentAction.TimeoutDuration);

		CurrentAction.Phase = EActionPhase::Failed;
		Stats.TotalActionsFailed++;

		if (!AttemptRetry())
		{
			// Blacklist this target if it has one
			if (!CurrentAction.TargetId.IsEmpty())
			{
				BlacklistTarget(CurrentAction.TargetId, CurrentAction.TargetLocation,
					BlacklistDuration, TEXT("Action timeout"));
			}
			RequestReplan(EReplanReason::ActionTimeout);
		}
		return;
	}

	// --- MINIMUM DURATION CHECK ---
	// The action cannot complete before its minimum duration. This prevents
	// instant-completion exploits and ensures animations play fully.
	const FAoCActionTimingData* Timing = TimingTable.Find(CurrentAction.ActionType);
	if (Timing && ElapsedTime < Timing->MinDuration)
	{
		// Still within minimum time — action continues executing
		return;
	}

	// --- EXPECTED DURATION CHECK ---
	// If we've passed the expected duration, move to verification
	if (ElapsedTime >= CurrentAction.ExpectedDuration && CurrentAction.Phase == EActionPhase::Executing)
	{
		CurrentAction.Phase = EActionPhase::Verifying;

		EVerificationResult Result = VerifyActionCompletion();

		switch (Result)
		{
		case EVerificationResult::Success:
			CurrentAction.Phase = EActionPhase::Completed;
			Stats.TotalActionsCompleted++;
			Stats.AverageActionDuration = (Stats.AverageActionDuration * (Stats.TotalActionsCompleted - 1) + ElapsedTime) / Stats.TotalActionsCompleted;
			UE_LOG(LogTaskGovernor, Verbose, TEXT("[%s] Action COMPLETED: Type=%d in %.1fs"),
				*GetOwner()->GetName(), (int32)CurrentAction.ActionType, ElapsedTime);
			break;

		case EVerificationResult::NodeDepleted:
			CurrentAction.Phase = EActionPhase::Completed; // Not a failure, just move on
			Stats.TotalActionsCompleted++;
			UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Node depleted, moving to next target"),
				*GetOwner()->GetName());
			break;

		case EVerificationResult::InventoryFull:
			CurrentAction.Phase = EActionPhase::Completed;
			UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Inventory full, need to offload"),
				*GetOwner()->GetName());
			break;

		case EVerificationResult::Failed:
			CurrentAction.Phase = EActionPhase::Failed;
			Stats.TotalActionsFailed++;
			if (!AttemptRetry())
			{
				if (!CurrentAction.TargetId.IsEmpty())
				{
					BlacklistTarget(CurrentAction.TargetId, CurrentAction.TargetLocation,
						BlacklistDuration, TEXT("Action verification failed"));
				}
				RequestReplan(EReplanReason::ActionFailed);
			}
			break;

		case EVerificationResult::TargetLost:
			CurrentAction.Phase = EActionPhase::Failed;
			Stats.TotalActionsFailed++;
			RequestReplan(EReplanReason::ActionFailed);
			break;

		case EVerificationResult::Inconclusive:
			// Can't tell yet — let it keep going until timeout
			CurrentAction.Phase = EActionPhase::Executing;
			break;
		}
	}
}

// ============================================================================
// TICK — TRANSITION BLENDING
// ============================================================================

void UAoCNPCTaskGovernor::TickTransition(float DeltaTime)
{
	if (CurrentAction.Phase != EActionPhase::Transitioning) return;

	TransitionTimer += DeltaTime;

	if (TransitionTimer >= CurrentAction.TransitionDuration)
	{
		// Transition complete — begin executing
		CurrentAction.Phase = EActionPhase::Executing;
		CurrentAction.StartTime = GetWorld()->GetTimeSeconds();
		TransitionTimer = 0.0f;

		UE_LOG(LogTaskGovernor, Verbose, TEXT("[%s] Transition complete, now executing Type=%d"),
			*GetOwner()->GetName(), (int32)CurrentAction.ActionType);
	}
}

// ============================================================================
// TICK — STUCK DETECTION
// ============================================================================

void UAoCNPCTaskGovernor::TickStuckDetection(float DeltaTime)
{
	// Only check for stuck during movement actions
	if (CurrentAction.ActionType != ENPCActionType::MoveTo &&
		CurrentAction.ActionType != ENPCActionType::Sprint &&
		CurrentAction.ActionType != ENPCActionType::Swim)
	{
		return;
	}

	if (CurrentAction.Phase != EActionPhase::Executing) return;

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Record position samples at interval
	if (CurrentTime - LastPositionSampleTime >= PositionSampleInterval)
	{
		RecordPositionSample();
		LastPositionSampleTime = CurrentTime;
	}

	// Check if stuck
	if (IsStuck())
	{
		Stats.TotalStuckDetections++;
		UE_LOG(LogTaskGovernor, Warning, TEXT("[%s] STUCK DETECTED during movement. Recovery attempt %d/%d"),
			*GetOwner()->GetName(), StuckRecoveryAttempts + 1, MaxStuckRecoveryAttempts);

		AttemptStuckRecovery();
	}
}

void UAoCNPCTaskGovernor::RecordPositionSample()
{
	if (!OwnerNPC) return;

	FPositionSample Sample;
	Sample.Position = OwnerNPC->GetActorLocation();
	Sample.Timestamp = GetWorld()->GetTimeSeconds();

	PositionHistory[PositionHistoryIndex % MaxPositionSamples] = Sample;
	PositionHistoryIndex++;
}

bool UAoCNPCTaskGovernor::IsStuck() const
{
	if (PositionHistoryIndex < 4) return false; // Need at least 4 samples

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// Check last N samples — if none are more than StuckDistanceThreshold apart, we're stuck
	int32 SamplesInWindow = 0;
	float MaxDisplacement = 0.0f;
	FVector ReferencePos = FVector::ZeroVector;
	bool bHasReference = false;

	for (int32 i = FMath::Max(0, PositionHistoryIndex - MaxPositionSamples); i < PositionHistoryIndex; i++)
	{
		const FPositionSample& Sample = PositionHistory[i % MaxPositionSamples];

		// Only consider samples within the stuck time window
		if (CurrentTime - Sample.Timestamp > StuckTimeThreshold) continue;

		if (!bHasReference)
		{
			ReferencePos = Sample.Position;
			bHasReference = true;
		}
		else
		{
			float Dist = FVector::Dist(ReferencePos, Sample.Position);
			MaxDisplacement = FMath::Max(MaxDisplacement, Dist);
		}
		SamplesInWindow++;
	}

	// Need at least 3 samples in the window and all within threshold distance
	return (SamplesInWindow >= 3 && MaxDisplacement < StuckDistanceThreshold);
}

void UAoCNPCTaskGovernor::AttemptStuckRecovery()
{
	StuckRecoveryAttempts++;

	if (StuckRecoveryAttempts <= MaxStuckRecoveryAttempts)
	{
		// Try an alternate path — offset the target slightly and re-navigate
		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Attempting alternate path (attempt %d)"),
			*GetOwner()->GetName(), StuckRecoveryAttempts);

		// Offset target by a random amount to try a different path
		FVector Offset = FVector(
			FMath::RandRange(-200.0f, 200.0f),
			FMath::RandRange(-200.0f, 200.0f),
			0.0f
		);

		CurrentAction.TargetLocation += Offset;
		// Reset position history so we don't immediately re-detect
		PositionHistoryIndex = 0;

		// The movement system will pick up the new target location
	}
	else
	{
		// Emergency: teleport to nearest navmesh point (only if no players nearby)
		if (!IsAnyPlayerNearby(PlayerProximityCheckRadius))
		{
			UE_LOG(LogTaskGovernor, Warning, TEXT("[%s] Emergency teleport — no players nearby, teleporting to navmesh"),
				*GetOwner()->GetName());

			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			if (NavSys && OwnerNPC)
			{
				FNavLocation NavLoc;
				if (NavSys->ProjectPointToNavigation(
					CurrentAction.TargetLocation,
					NavLoc,
					FVector(500.0f, 500.0f, 200.0f)))
				{
					OwnerNPC->SetActorLocation(NavLoc.Location);
					PositionHistoryIndex = 0;
					StuckRecoveryAttempts = 0;
				}
			}
		}
		else
		{
			// Players are nearby — can't teleport, just fail the action
			UE_LOG(LogTaskGovernor, Warning, TEXT("[%s] Stuck near players — failing action to prevent visible teleport"),
				*GetOwner()->GetName());

			CurrentAction.Phase = EActionPhase::Failed;
			Stats.TotalActionsFailed++;
			RequestReplan(EReplanReason::StuckDetected);
		}
	}
}

bool UAoCNPCTaskGovernor::IsAnyPlayerNearby(float Radius) const
{
	if (!GetWorld() || !OwnerNPC) return true; // Assume yes if we can't check

	const FVector NPCLocation = OwnerNPC->GetActorLocation();
	const float RadiusSq = Radius * Radius;

	// Iterate over player controllers
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			float DistSq = FVector::DistSquared(NPCLocation, PC->GetPawn()->GetActorLocation());
			if (DistSq <= RadiusSq)
			{
				return true;
			}
		}
	}

	return false;
}

// ============================================================================
// TICK — BLACKLIST EXPIRY
// ============================================================================

void UAoCNPCTaskGovernor::TickBlacklistExpiry(float DeltaTime)
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	for (int32 i = BlacklistedTargets.Num() - 1; i >= 0; i--)
	{
		if (CurrentTime >= BlacklistedTargets[i].BlacklistExpireTime)
		{
			UE_LOG(LogTaskGovernor, Verbose, TEXT("[%s] Blacklist expired for target '%s'"),
				*GetOwner()->GetName(), *BlacklistedTargets[i].TargetId);
			BlacklistedTargets.RemoveAt(i);
		}
	}
}

// ============================================================================
// TICK — SCHEDULE
// ============================================================================

void UAoCNPCTaskGovernor::TickSchedule(float DeltaTime)
{
	ScheduleCheckTimer += DeltaTime;

	// Only re-check schedule every 10 seconds (it doesn't change fast)
	if (ScheduleCheckTimer < 10.0f) return;
	ScheduleCheckTimer = 0.0f;

	EDailyScheduleBlock NewBlock = GetCurrentScheduleBlock();

	if (NewBlock != CachedScheduleBlock)
	{
		EDailyScheduleBlock OldBlock = CachedScheduleBlock;
		CachedScheduleBlock = NewBlock;

		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Schedule block changed: %d → %d"),
			*GetOwner()->GetName(), (int32)OldBlock, (int32)NewBlock);

		// Notify brain that schedule has changed — it may want to replan
		if (Brain && !IsActionInProgress())
		{
			RequestReplan(EReplanReason::ScheduleChange);
		}
	}
}

// ============================================================================
// TICK — GOAL PROGRESS
// ============================================================================

void UAoCNPCTaskGovernor::TickGoalProgress(float DeltaTime)
{
	if (!ActiveGoal.bIsActive) return;

	// Recalculate completion percentage
	float TotalRequired = 0.0f;
	float TotalGathered = 0.0f;

	for (const auto& Pair : ActiveGoal.RequiredResources)
	{
		TotalRequired += (float)Pair.Value;
		int32* Gathered = ActiveGoal.GatheredSoFar.Find(Pair.Key);
		TotalGathered += Gathered ? FMath::Min((float)*Gathered, (float)Pair.Value) : 0.0f;
	}

	for (const auto& Pair : ActiveGoal.RequiredCraftedItems)
	{
		TotalRequired += (float)Pair.Value * 10.0f; // Crafted items are worth more toward completion
		int32* Crafted = ActiveGoal.CraftedSoFar.Find(Pair.Key);
		TotalGathered += Crafted ? FMath::Min((float)*Crafted, (float)Pair.Value) * 10.0f : 0.0f;
	}

	ActiveGoal.CompletionPercent = (TotalRequired > 0.0f) ? (TotalGathered / TotalRequired) * 100.0f : 0.0f;

	// Check if goal is complete
	if (ActiveGoal.CompletionPercent >= 100.0f)
	{
		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] LONG-TERM GOAL COMPLETED: '%s'"),
			*GetOwner()->GetName(), *ActiveGoal.GoalDescription);

		ActiveGoal.bIsActive = false;
		RequestReplan(EReplanReason::GoalCompleted);
	}
}

// ============================================================================
// TIMING COMPUTATION
// ============================================================================

float UAoCNPCTaskGovernor::ComputeActionDuration(ENPCActionType ActionType) const
{
	const FAoCActionTimingData* Timing = TimingTable.Find(ActionType);
	if (!Timing) return 2.0f; // Fallback

	float BaseDuration;
	if (Timing->bRandomizeDuration)
	{
		BaseDuration = FMath::RandRange(Timing->MinDuration, Timing->MaxDuration);
	}
	else
	{
		BaseDuration = (Timing->MinDuration + Timing->MaxDuration) * 0.5f;
	}

	// Apply skill speed factor — higher skill = faster
	if (Timing->SkillSpeedFactor > 0.0f && SkillSystem)
	{
		// The skill system provides 0-100 level; we scale down duration proportionally
		// At skill 0: full duration. At skill 100: (1 - SkillSpeedFactor) * duration.
		// E.g., SkillSpeedFactor=0.2 means at skill 100 you're 20% faster.
		float SkillLevel = 0.0f;

		// Map action type to the relevant skill for speed bonus
		switch (ActionType)
		{
		case ENPCActionType::MiningSwing:
		case ENPCActionType::SmeltOre:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Mining);
			break;
		case ENPCActionType::WoodcuttingChop:
		case ENPCActionType::SawLogs:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Woodcutting);
			break;
		case ENPCActionType::FishingCast:
		case ENPCActionType::FishingWait:
		case ENPCActionType::FishingReel:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Fishing);
			break;
		case ENPCActionType::HerbGather:
		case ENPCActionType::GrindHerbs:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Herbalism);
			break;
		case ENPCActionType::SkinCorpse:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Skinning);
			break;
		case ENPCActionType::HuntTrack:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Hunting);
			break;
		case ENPCActionType::CraftSword:
		case ENPCActionType::CraftGreatsword:
		case ENPCActionType::CraftAxe:
		case ENPCActionType::CraftDagger:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::WeaponSmithing);
			break;
		case ENPCActionType::CraftHelmet:
		case ENPCActionType::CraftChestplate:
		case ENPCActionType::CraftLeggings:
		case ENPCActionType::CraftGauntlets:
		case ENPCActionType::CraftBoots:
		case ENPCActionType::CraftShield:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::ArmorSmithing);
			break;
		case ENPCActionType::CraftBow:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::BowCrafting);
			break;
		case ENPCActionType::CraftStaff:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::StaffCrafting);
			break;
		case ENPCActionType::CraftJewelry:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Jewelcrafting);
			break;
		case ENPCActionType::CraftPotion:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Alchemy);
			break;
		case ENPCActionType::CookMeal:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Cooking);
			break;
		case ENPCActionType::BrewDrink:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Brewing);
			break;
		case ENPCActionType::Enchant:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Enchanting);
			break;
		case ENPCActionType::TailorCloth:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Tailoring);
			break;
		case ENPCActionType::LeatherworkItem:
		case ENPCActionType::TanHide:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Leatherworking);
			break;
		case ENPCActionType::LayStone:
		case ENPCActionType::BuildWall:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Masonry);
			break;
		case ENPCActionType::PlaceBeam:
		case ENPCActionType::BuildRoof:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Carpentry);
			break;
		case ENPCActionType::FarmPlant:
		case ENPCActionType::FarmHarvest:
			SkillLevel = SkillSystem->GetSkillLevel(ESkillID::Farming);
			break;
		default:
			break;
		}

		float SpeedMultiplier = 1.0f - (Timing->SkillSpeedFactor * (SkillLevel / 100.0f));
		SpeedMultiplier = FMath::Clamp(SpeedMultiplier, 0.5f, 1.0f); // Cap at 50% faster
		BaseDuration *= SpeedMultiplier;
	}

	// Ensure we never go below minimum
	BaseDuration = FMath::Max(BaseDuration, Timing->MinDuration * 0.5f);

	return BaseDuration;
}

float UAoCNPCTaskGovernor::GetActionTimeout(ENPCActionType ActionType) const
{
	const FAoCActionTimingData* Timing = TimingTable.Find(ActionType);
	if (!Timing) return 30.0f;

	// Timeout is 2x the max duration, minimum 10 seconds
	return FMath::Max(Timing->MaxDuration * 2.0f, 10.0f);
}

float UAoCNPCTaskGovernor::GetTransitionDuration(ENPCActionType FromAction, ENPCActionType ToAction) const
{
	// No transition needed if starting from nothing
	if (FromAction == ENPCActionType::None) return 0.3f;

	// Combat transitions are fast
	if (ToAction == ENPCActionType::MeleeAttack || ToAction == ENPCActionType::RangedAttack ||
		ToAction == ENPCActionType::CastSpell || ToAction == ENPCActionType::Dodge ||
		ToAction == ENPCActionType::Block)
	{
		return FMath::RandRange(0.1f, 0.4f);
	}

	// Same-type actions have short transitions (mining swing to mining swing)
	if (FromAction == ToAction)
	{
		return FMath::RandRange(0.3f, 0.8f);
	}

	// Different activity categories get longer transitions (mining → walking)
	// This prevents the robotic instant-switch behavior
	return FMath::RandRange(1.0f, 2.0f);
}

// ============================================================================
// VERIFICATION
// ============================================================================

EVerificationResult UAoCNPCTaskGovernor::VerifyActionCompletion()
{
	// Verification checks whether the action ACTUALLY ACHIEVED its goal.
	// E.g., after a mining swing, did ore count increase?

	switch (CurrentAction.ActionType)
	{
	case ENPCActionType::MiningSwing:
	{
		// Check: did ore count increase?
		// Compare current inventory to pre-action snapshot
		// This is a simplified check — real implementation checks the actual inventory component
		int32 PreOre = 0;
		for (const auto& Pair : CurrentAction.PreActionInventory)
		{
			if (Pair.Key.Contains(TEXT("Ore")))
			{
				PreOre += Pair.Value;
			}
		}

		// TODO: Query actual current inventory count
		// For now, simulate: 80% chance of success per swing (node might be depleted)
		float SuccessChance = 0.80f;
		if (FMath::FRand() < SuccessChance)
		{
			return EVerificationResult::Success;
		}
		else
		{
			// After 3 failed swings on same node, assume depleted
			if (CurrentAction.RetryCount >= 2)
			{
				return EVerificationResult::NodeDepleted;
			}
			return EVerificationResult::Failed;
		}
	}

	case ENPCActionType::WoodcuttingChop:
	{
		float SuccessChance = 0.85f;
		if (FMath::FRand() < SuccessChance)
		{
			return EVerificationResult::Success;
		}
		if (CurrentAction.RetryCount >= 2)
		{
			return EVerificationResult::NodeDepleted;
		}
		return EVerificationResult::Failed;
	}

	case ENPCActionType::FishingReel:
	{
		// Fish might get away
		float SuccessChance = 0.60f;
		if (FMath::FRand() < SuccessChance)
		{
			return EVerificationResult::Success;
		}
		return EVerificationResult::Failed; // Fish got away, retry
	}

	case ENPCActionType::HerbGather:
	case ENPCActionType::SkinCorpse:
	case ENPCActionType::FarmHarvest:
	{
		// These are generally reliable if the target exists
		return EVerificationResult::Success;
	}

	case ENPCActionType::SmeltOre:
	case ENPCActionType::SawLogs:
	case ENPCActionType::TanHide:
	case ENPCActionType::GrindHerbs:
	{
		// Processing at a station — high success rate
		return (FMath::FRand() < 0.95f) ? EVerificationResult::Success : EVerificationResult::Failed;
	}

	case ENPCActionType::CraftDagger:
	case ENPCActionType::CraftSword:
	case ENPCActionType::CraftGreatsword:
	case ENPCActionType::CraftAxe:
	case ENPCActionType::CraftHelmet:
	case ENPCActionType::CraftChestplate:
	case ENPCActionType::CraftLeggings:
	case ENPCActionType::CraftGauntlets:
	case ENPCActionType::CraftBoots:
	case ENPCActionType::CraftShield:
	case ENPCActionType::CraftBow:
	case ENPCActionType::CraftStaff:
	case ENPCActionType::CraftJewelry:
	case ENPCActionType::CraftPotion:
	case ENPCActionType::CookMeal:
	case ENPCActionType::BrewDrink:
	case ENPCActionType::Enchant:
	case ENPCActionType::TailorCloth:
	case ENPCActionType::LeatherworkItem:
	{
		// Crafting: success rate depends on skill level
		// TODO: query actual skill level
		float BaseSuccess = 0.85f;
		return (FMath::FRand() < BaseSuccess) ? EVerificationResult::Success : EVerificationResult::Failed;
	}

	case ENPCActionType::MoveTo:
	case ENPCActionType::Sprint:
	case ENPCActionType::Swim:
	{
		// Movement — check if we're within acceptable distance of target
		if (OwnerNPC)
		{
			float DistToTarget = FVector::Dist(OwnerNPC->GetActorLocation(), CurrentAction.TargetLocation);
			if (DistToTarget < 150.0f) // Within 1.5 meters
			{
				return EVerificationResult::Success;
			}
		}
		return EVerificationResult::Failed;
	}

	case ENPCActionType::Eat:
	case ENPCActionType::Drink:
	{
		// Life actions are basically always successful if you have the item
		return EVerificationResult::Success;
	}

	case ENPCActionType::LayStone:
	case ENPCActionType::PlaceBeam:
	case ENPCActionType::BuildWall:
	case ENPCActionType::BuildRoof:
	{
		return (FMath::FRand() < 0.90f) ? EVerificationResult::Success : EVerificationResult::Failed;
	}

	default:
		// For actions without specific verification, assume success
		return EVerificationResult::Success;
	}
}

void UAoCNPCTaskGovernor::SnapshotPreActionState()
{
	// Take a snapshot of relevant inventory items so we can verify changes
	// In real implementation, this queries the NPC's inventory component
	CurrentAction.PreActionInventory.Empty();

	// TODO: Query actual inventory component
	// For now, this is a placeholder that will be hooked up to the inventory system
	UE_LOG(LogTaskGovernor, Verbose, TEXT("[%s] Snapshot taken for action type %d"),
		*GetOwner()->GetName(), (int32)CurrentAction.ActionType);
}

// ============================================================================
// RETRY & BLACKLIST
// ============================================================================

bool UAoCNPCTaskGovernor::AttemptRetry()
{
	if (CurrentAction.RetryCount >= MaxRetries)
	{
		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Max retries (%d) reached for action type %d on target '%s'"),
			*GetOwner()->GetName(), MaxRetries, (int32)CurrentAction.ActionType, *CurrentAction.TargetId);
		return false;
	}

	CurrentAction.RetryCount++;
	Stats.TotalRetries++;

	// Slight variation on retry — offset position, add small delay
	FVector RetryOffset = FVector(
		FMath::RandRange(-100.0f, 100.0f),
		FMath::RandRange(-100.0f, 100.0f),
		0.0f
	);
	CurrentAction.TargetLocation += RetryOffset;

	// Reset to executing with a small transition
	CurrentAction.Phase = EActionPhase::Transitioning;
	CurrentAction.TransitionDuration = FMath::RandRange(0.5f, 1.5f);
	TransitionTimer = 0.0f;

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Retrying action (attempt %d/%d) with offset"),
		*GetOwner()->GetName(), CurrentAction.RetryCount, MaxRetries);

	return true;
}

void UAoCNPCTaskGovernor::BlacklistTarget(const FString& TargetId, FVector Location, float Duration, const FString& Reason)
{
	FBlacklistedTarget Entry;
	Entry.TargetId = TargetId;
	Entry.Location = Location;
	Entry.BlacklistExpireTime = GetWorld()->GetTimeSeconds() + Duration;
	Entry.Reason = Reason;

	BlacklistedTargets.Add(Entry);
	Stats.TotalBlacklists++;

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Blacklisted target '%s' for %.0fs: %s"),
		*GetOwner()->GetName(), *TargetId, Duration, *Reason);
}

// ============================================================================
// REPLAN CONTROL
// ============================================================================

bool UAoCNPCTaskGovernor::RequestReplan(EReplanReason Reason)
{
	// Combat interrupts ALWAYS bypass the cooldown
	if (Reason == EReplanReason::CombatInterrupt || Reason == EReplanReason::NeedCritical)
	{
		LastReplanTime = GetWorld()->GetTimeSeconds();
		Stats.TotalReplans++;

		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] EMERGENCY replan (reason: %d) — bypassing cooldown"),
			*GetOwner()->GetName(), (int32)Reason);

		// TODO: Actually trigger GOAP replanning via Brain component
		return true;
	}

	if (!CanReplan())
	{
		UE_LOG(LogTaskGovernor, Verbose, TEXT("[%s] Replan denied — cooldown active (%.1fs remaining)"),
			*GetOwner()->GetName(), ReplanCooldown - (GetWorld()->GetTimeSeconds() - LastReplanTime));
		return false;
	}

	LastReplanTime = GetWorld()->GetTimeSeconds();
	Stats.TotalReplans++;

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Replan triggered (reason: %d)"),
		*GetOwner()->GetName(), (int32)Reason);

	// TODO: Actually trigger GOAP replanning via Brain component
	return true;
}

bool UAoCNPCTaskGovernor::CanReplan() const
{
	const float TimeSinceLastReplan = GetWorld()->GetTimeSeconds() - LastReplanTime;
	return TimeSinceLastReplan >= ReplanCooldown;
}

// ============================================================================
// LONG-TERM GOALS
// ============================================================================

void UAoCNPCTaskGovernor::SetLongTermGoal(const FNPCLongTermGoal& Goal)
{
	ActiveGoal = Goal;
	ActiveGoal.bIsActive = true;
	ActiveGoal.StartTime = GetWorld()->GetTimeSeconds();
	ActiveGoal.CompletionPercent = 0.0f;

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] New long-term goal: '%s' (Priority: %d)"),
		*GetOwner()->GetName(), *Goal.GoalDescription, Goal.Priority);

	for (const auto& Pair : Goal.RequiredResources)
	{
		UE_LOG(LogTaskGovernor, Log, TEXT("  Required: %s x%d"), *Pair.Key, Pair.Value);
	}
}

void UAoCNPCTaskGovernor::UpdateGoalProgress(const FString& ResourceName, int32 AmountGathered)
{
	if (!ActiveGoal.bIsActive) return;

	int32& Current = ActiveGoal.GatheredSoFar.FindOrAdd(ResourceName, 0);
	Current += AmountGathered;

	int32* Required = ActiveGoal.RequiredResources.Find(ResourceName);
	if (Required)
	{
		FString StepLog = FString::Printf(TEXT("Gathered %d/%d %s"), Current, *Required, *ResourceName);

		// Only log significant milestones (every 25%)
		float Percent = (float)Current / (float)*Required * 100.0f;
		if (FMath::FloorToInt(Percent) % 25 == 0 || Current >= *Required)
		{
			ActiveGoal.StepsCompleted.Add(StepLog);
			ActiveGoal.CurrentStep = StepLog;

			UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Goal progress: %s (%.0f%%)"),
				*GetOwner()->GetName(), *StepLog, Percent);
		}
	}
}

void UAoCNPCTaskGovernor::UpdateGoalCraftProgress(const FString& ItemName, int32 AmountCrafted)
{
	if (!ActiveGoal.bIsActive) return;

	int32& Current = ActiveGoal.CraftedSoFar.FindOrAdd(ItemName, 0);
	Current += AmountCrafted;

	int32* Required = ActiveGoal.RequiredCraftedItems.Find(ItemName);
	if (Required)
	{
		FString StepLog = FString::Printf(TEXT("Crafted %d/%d %s"), Current, *Required, *ItemName);
		ActiveGoal.StepsCompleted.Add(StepLog);
		ActiveGoal.CurrentStep = StepLog;

		UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Goal craft progress: %s"),
			*GetOwner()->GetName(), *StepLog);
	}
}

bool UAoCNPCTaskGovernor::HasResourcesForCurrentStep() const
{
	if (!ActiveGoal.bIsActive) return false;

	for (const auto& Pair : ActiveGoal.RequiredResources)
	{
		const int32* Gathered = ActiveGoal.GatheredSoFar.Find(Pair.Key);
		if (!Gathered || *Gathered < Pair.Value)
		{
			return false;
		}
	}
	return true;
}

ENPCActionType UAoCNPCTaskGovernor::GetNextGoalAction() const
{
	if (!ActiveGoal.bIsActive) return ENPCActionType::Idle;

	// Check what resources we still need
	for (const auto& Pair : ActiveGoal.RequiredResources)
	{
		const int32* Gathered = ActiveGoal.GatheredSoFar.Find(Pair.Key);
		int32 CurrentAmount = Gathered ? *Gathered : 0;

		if (CurrentAmount < Pair.Value)
		{
			// Need more of this resource — determine action by resource type
			if (Pair.Key.Contains(TEXT("Ore")) || Pair.Key.Contains(TEXT("Metal")))
			{
				return ENPCActionType::MiningSwing;
			}
			if (Pair.Key.Contains(TEXT("Log")) || Pair.Key.Contains(TEXT("Wood")))
			{
				return ENPCActionType::WoodcuttingChop;
			}
			if (Pair.Key.Contains(TEXT("Herb")) || Pair.Key.Contains(TEXT("Plant")))
			{
				return ENPCActionType::HerbGather;
			}
			if (Pair.Key.Contains(TEXT("Fish")))
			{
				return ENPCActionType::FishingCast;
			}
			if (Pair.Key.Contains(TEXT("Hide")) || Pair.Key.Contains(TEXT("Leather")))
			{
				return ENPCActionType::SkinCorpse;
			}
			if (Pair.Key.Contains(TEXT("Stone")) || Pair.Key.Contains(TEXT("Granite")) || Pair.Key.Contains(TEXT("Marble")))
			{
				return ENPCActionType::MiningSwing;
			}
			if (Pair.Key.Contains(TEXT("Bar")) || Pair.Key.Contains(TEXT("Ingot")))
			{
				return ENPCActionType::SmeltOre;
			}
			if (Pair.Key.Contains(TEXT("Plank")))
			{
				return ENPCActionType::SawLogs;
			}
		}
	}

	// All resources gathered — check if we need to craft
	for (const auto& Pair : ActiveGoal.RequiredCraftedItems)
	{
		const int32* Crafted = ActiveGoal.CraftedSoFar.Find(Pair.Key);
		int32 CurrentAmount = Crafted ? *Crafted : 0;

		if (CurrentAmount < Pair.Value)
		{
			// Need to craft this item — determine craft action
			if (Pair.Key.Contains(TEXT("Sword")))    return ENPCActionType::CraftSword;
			if (Pair.Key.Contains(TEXT("Dagger")))    return ENPCActionType::CraftDagger;
			if (Pair.Key.Contains(TEXT("Axe")))       return ENPCActionType::CraftAxe;
			if (Pair.Key.Contains(TEXT("Helmet")))    return ENPCActionType::CraftHelmet;
			if (Pair.Key.Contains(TEXT("Chest")))     return ENPCActionType::CraftChestplate;
			if (Pair.Key.Contains(TEXT("Legging")))   return ENPCActionType::CraftLeggings;
			if (Pair.Key.Contains(TEXT("Gauntlet")))  return ENPCActionType::CraftGauntlets;
			if (Pair.Key.Contains(TEXT("Boot")))      return ENPCActionType::CraftBoots;
			if (Pair.Key.Contains(TEXT("Shield")))    return ENPCActionType::CraftShield;
			if (Pair.Key.Contains(TEXT("Bow")))       return ENPCActionType::CraftBow;
			if (Pair.Key.Contains(TEXT("Staff")))     return ENPCActionType::CraftStaff;
			if (Pair.Key.Contains(TEXT("Ring")) || Pair.Key.Contains(TEXT("Amulet")))
				return ENPCActionType::CraftJewelry;
			if (Pair.Key.Contains(TEXT("Potion")))    return ENPCActionType::CraftPotion;
		}
	}

	// Goal is basically done
	return ENPCActionType::Idle;
}

// ============================================================================
// SCHEDULE
// ============================================================================

EDailyScheduleBlock UAoCNPCTaskGovernor::GetCurrentScheduleBlock() const
{
	if (DailySchedule.Num() == 0) return EDailyScheduleBlock::MorningWork;

	// Get current game-world hour (0-24)
	// TODO: Hook into actual game time system
	float GameHour = FMath::Fmod(GetWorld()->GetTimeSeconds() / 300.0f, 24.0f); // 300s = 1 game hour (2 real hours = 1 game day)

	for (const FScheduleEntry& Entry : DailySchedule)
	{
		if (GameHour >= Entry.StartHour && GameHour < Entry.EndHour)
		{
			return Entry.Block;
		}
	}

	return EDailyScheduleBlock::Sleeping; // Default to sleeping if no block matches
}

ENPCActionType UAoCNPCTaskGovernor::GetScheduledActivity() const
{
	if (DailySchedule.Num() == 0) return ENPCActionType::Idle;

	EDailyScheduleBlock CurrentBlock = GetCurrentScheduleBlock();

	for (const FScheduleEntry& Entry : DailySchedule)
	{
		if (Entry.Block == CurrentBlock)
		{
			return Entry.PreferredActivity;
		}
	}

	return ENPCActionType::Idle;
}

FString UAoCNPCTaskGovernor::GetScheduledLocation() const
{
	if (DailySchedule.Num() == 0) return TEXT("home");

	EDailyScheduleBlock CurrentBlock = GetCurrentScheduleBlock();

	for (const FScheduleEntry& Entry : DailySchedule)
	{
		if (Entry.Block == CurrentBlock)
		{
			return Entry.LocationTag;
		}
	}

	return TEXT("home");
}

bool UAoCNPCTaskGovernor::ShouldBeSleeping() const
{
	return GetCurrentScheduleBlock() == EDailyScheduleBlock::Sleeping;
}

void UAoCNPCTaskGovernor::GenerateScheduleFromPersonality()
{
	DailySchedule.Empty();

	// Get personality traits from Brain — these modify the schedule
	// Defaults for a "normal" NPC
	float WakeHour = 6.0f;
	float SleepHour = 21.0f;
	float LunchStart = 12.0f;
	float DinnerStart = 18.0f;

	// TODO: Pull actual personality traits from brain component
	// For now, add random personality variance
	float Laziness = FMath::RandRange(0.0f, 1.0f);
	float Workaholism = FMath::RandRange(0.0f, 1.0f);
	float Sociability = FMath::RandRange(0.0f, 1.0f);

	// Lazy NPCs wake later
	WakeHour += Laziness * 2.0f; // Up to 8:00 for very lazy

	// Workaholics sleep later
	SleepHour += Workaholism * 2.0f; // Up to 23:00 for workaholics

	// Shorter lunch for workaholics
	float LunchDuration = FMath::Lerp(1.0f, 0.5f, Workaholism);

	// Longer social time for sociable NPCs
	float SocialDuration = FMath::Lerp(1.0f, 3.0f, Sociability);

	// --- BUILD SCHEDULE ---

	// Sleeping
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::Sleeping;
		Entry.StartHour = SleepHour;
		Entry.EndHour = 24.0f; // Wraps to next day
		Entry.LocationTag = TEXT("home");
		Entry.PreferredActivity = ENPCActionType::Sleep;
		DailySchedule.Add(Entry);
	}
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::Sleeping;
		Entry.StartHour = 0.0f;
		Entry.EndHour = WakeHour;
		Entry.LocationTag = TEXT("home");
		Entry.PreferredActivity = ENPCActionType::Sleep;
		DailySchedule.Add(Entry);
	}

	// Waking Up
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::WakingUp;
		Entry.StartHour = WakeHour;
		Entry.EndHour = WakeHour + 0.25f; // 15 min to wake up
		Entry.LocationTag = TEXT("home");
		Entry.PreferredActivity = ENPCActionType::StandUp;
		DailySchedule.Add(Entry);
	}

	// Breakfast
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::Breakfast;
		Entry.StartHour = WakeHour + 0.25f;
		Entry.EndHour = WakeHour + 1.0f;
		Entry.LocationTag = TEXT("home");
		Entry.PreferredActivity = ENPCActionType::Eat;
		DailySchedule.Add(Entry);
	}

	// Morning Work
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::MorningWork;
		Entry.StartHour = WakeHour + 1.0f;
		Entry.EndHour = LunchStart;
		Entry.LocationTag = TEXT("work");
		Entry.PreferredActivity = ENPCActionType::None; // Determined by GOAP/goals
		DailySchedule.Add(Entry);
	}

	// Lunch
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::Lunch;
		Entry.StartHour = LunchStart;
		Entry.EndHour = LunchStart + LunchDuration;
		Entry.LocationTag = TEXT("tavern");
		Entry.PreferredActivity = ENPCActionType::Eat;
		DailySchedule.Add(Entry);
	}

	// Afternoon Work
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::AfternoonWork;
		Entry.StartHour = LunchStart + LunchDuration;
		Entry.EndHour = DinnerStart;
		Entry.LocationTag = TEXT("work");
		Entry.PreferredActivity = ENPCActionType::None; // Determined by GOAP/goals
		DailySchedule.Add(Entry);
	}

	// Dinner
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::Dinner;
		Entry.StartHour = DinnerStart;
		Entry.EndHour = DinnerStart + 1.0f;
		Entry.LocationTag = TEXT("tavern");
		Entry.PreferredActivity = ENPCActionType::Eat;
		DailySchedule.Add(Entry);
	}

	// Evening Social
	{
		FScheduleEntry Entry;
		Entry.Block = EDailyScheduleBlock::EveningSocial;
		Entry.StartHour = DinnerStart + 1.0f;
		Entry.EndHour = FMath::Min(SleepHour, DinnerStart + 1.0f + SocialDuration);
		Entry.LocationTag = TEXT("tavern");
		Entry.PreferredActivity = ENPCActionType::Speak;
		DailySchedule.Add(Entry);
	}

	// Winding Down
	{
		float WindDownStart = FMath::Min(SleepHour - 0.5f, DinnerStart + 1.0f + SocialDuration);
		if (WindDownStart < SleepHour)
		{
			FScheduleEntry Entry;
			Entry.Block = EDailyScheduleBlock::WindingDown;
			Entry.StartHour = WindDownStart;
			Entry.EndHour = SleepHour;
			Entry.LocationTag = TEXT("home");
			Entry.PreferredActivity = ENPCActionType::Rest;
			DailySchedule.Add(Entry);
		}
	}

	UE_LOG(LogTaskGovernor, Log, TEXT("[%s] Generated schedule: Wake=%.1f, Sleep=%.1f, Laziness=%.2f, Workaholic=%.2f, Social=%.2f"),
		OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"),
		WakeHour, SleepHour, Laziness, Workaholism, Sociability);
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

FString UAoCNPCTaskGovernor::GetDebugString() const
{
	FString Result;

	Result += FString::Printf(TEXT("=== Task Governor ===\n"));
	Result += FString::Printf(TEXT("Current Action: %d (Phase: %d)\n"), (int32)CurrentAction.ActionType, (int32)CurrentAction.Phase);

	if (IsActionInProgress())
	{
		float Elapsed = GetWorld()->GetTimeSeconds() - CurrentAction.StartTime;
		Result += FString::Printf(TEXT("  Elapsed: %.1fs / Expected: %.1fs / Timeout: %.1fs\n"),
			Elapsed, CurrentAction.ExpectedDuration, CurrentAction.TimeoutDuration);
		Result += FString::Printf(TEXT("  Target: '%s' Retries: %d/%d\n"),
			*CurrentAction.TargetId, CurrentAction.RetryCount, MaxRetries);
	}

	Result += FString::Printf(TEXT("Schedule Block: %d\n"), (int32)CachedScheduleBlock);
	Result += FString::Printf(TEXT("Blacklisted Targets: %d\n"), BlacklistedTargets.Num());

	if (ActiveGoal.bIsActive)
	{
		Result += FString::Printf(TEXT("Active Goal: '%s' (%.1f%% complete)\n"),
			*ActiveGoal.GoalDescription, ActiveGoal.CompletionPercent);
		Result += FString::Printf(TEXT("  Current Step: %s\n"), *ActiveGoal.CurrentStep);
	}

	Result += FString::Printf(TEXT("Stats: Attempted=%d, Completed=%d, Failed=%d, Retries=%d, Stuck=%d, Replans=%d\n"),
		Stats.TotalActionsAttempted, Stats.TotalActionsCompleted, Stats.TotalActionsFailed,
		Stats.TotalRetries, Stats.TotalStuckDetections, Stats.TotalReplans);

	return Result;
}

int32 UAoCNPCTaskGovernor::GetAbandonedTaskCount() const { return 0; }

int32 UAoCNPCTaskGovernor::GetCompletedTaskCount() const { return 0; }

int32 UAoCNPCTaskGovernor::GetTaskAttemptCount(const FString& TaskType) const { return 0; }

int32 UAoCNPCTaskGovernor::GetTaskSuccessCount(const FString& TaskType) const { return 0; }
