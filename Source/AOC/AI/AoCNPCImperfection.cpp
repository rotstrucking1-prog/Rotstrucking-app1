#include "AoCNPCImperfection.h"
// AoCNPCImperfection.cpp
// Architect of Creation — NPC Human Imperfection Implementation
//
// Adds realistic human imperfections: reaction delay, aim variance, decision mistakes,
// movement noise, idle behaviors, and attention/perception limits.

#include "AoCNPCCharacter.h"
#include "AoCHumanoidNPCV2.h"
#include "AoCNPCBrainV2.h"
#include "AoCNPCSkillSystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogImperfection, Log, All);

// ============================================================================
// CONSTRUCTOR / LIFECYCLE
// ============================================================================

UAoCNPCImperfection::UAoCNPCImperfection()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10Hz for movement smoothing

	// Initialize idle state timer
	IdleState.TimeUntilNextIdleBehavior = FMath::RandRange(30.0f, 90.0f);
}

void UAoCNPCImperfection::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AAoCNPCCharacter>(GetOwner());
	if (OwnerNPC)
	{
		SkillSystem = OwnerNPC->FindComponentByClass<UAoCNPCSkillSystem>();
	}

	// Initialize movement state with slight randomization
	MovementState.CurrentSpeedMultiplier = 1.0f + FMath::RandRange(-MovementConfig.SpeedVarianceFraction, MovementConfig.SpeedVarianceFraction);
	MovementState.HeadLookChangeInterval = FMath::RandRange(3.0f, 7.0f);

	// Initialize base mistake chance from personality (if available)
	BaseMistakeChance = GetBaseMistakeChance();

	UE_LOG(LogImperfection, Log, TEXT("[%s] Imperfection system initialized. MistakeChance=%.3f, ReactionBase=%.0fms"),
		OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"),
		BaseMistakeChance, GetReactionDelay() * 1000.0f);
}

void UAoCNPCImperfection::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickMovementImperfection(DeltaTime);
	TickIdleBehaviors(DeltaTime);
	TickHeadLook(DeltaTime);
}

// ============================================================================
// REACTION DELAY
// ============================================================================

float UAoCNPCImperfection::GetReactionDelay() const
{
	float BaseDelay;

	switch (CurrentAlertState)
	{
	case ENPCAlertState::Sleeping:  BaseDelay = ReactionConfig.SleepingReaction; break;
	case ENPCAlertState::Relaxed:   BaseDelay = ReactionConfig.RelaxedReaction; break;
	case ENPCAlertState::Idle:      BaseDelay = ReactionConfig.IdleReaction; break;
	case ENPCAlertState::Alert:     BaseDelay = ReactionConfig.AlertReaction; break;
	case ENPCAlertState::Combat:    BaseDelay = ReactionConfig.CombatReaction; break;
	case ENPCAlertState::Focused:   BaseDelay = ReactionConfig.FocusedReaction; break;
	case ENPCAlertState::Panicked:  BaseDelay = ReactionConfig.PanickedReaction; break;
	default:                        BaseDelay = ReactionConfig.IdleReaction; break;
	}

	// Agility reduces reaction time
	float Agility = GetAttribute(TEXT("Agility"));
	float AgilityReduction = Agility * ReactionConfig.AgilityReductionFactor;
	BaseDelay *= (1.0f - AgilityReduction);

	// Fatigue increases reaction time
	BaseDelay += CurrentFatigue * ReactionConfig.FatigueAdditional;

	// Add slight randomness (±15%) for natural variation
	BaseDelay *= FMath::RandRange(0.85f, 1.15f);

	// Clamp to reasonable bounds
	return FMath::Clamp(BaseDelay, 0.05f, 3.0f);
}

float UAoCNPCImperfection::GetReactionDelayWithSurprise(bool bFromBehind, bool bFatigued) const
{
	float Delay = GetReactionDelay();

	if (bFromBehind)
	{
		Delay += ReactionConfig.SurpriseAdditional;
	}

	if (bFatigued)
	{
		Delay += ReactionConfig.FatigueAdditional;
	}

	return FMath::Clamp(Delay, 0.05f, 4.0f);
}

// ============================================================================
// AIM VARIANCE
// ============================================================================

FVector UAoCNPCImperfection::ApplyAimVariance(FVector IdealDirection, float Distance, bool bTargetMoving, float WeaponSkillLevel) const
{
	float SpreadDegrees = GetCurrentAimSpread(Distance, bTargetMoving, WeaponSkillLevel);

	// Convert spread to radians for rotation
	float SpreadRadians = FMath::DegreesToRadians(SpreadDegrees);

	// Generate random offset within a cone
	float RandomAngle = FMath::FRand() * 2.0f * PI;
	float RandomMagnitude = FMath::FRand() * SpreadRadians;

	// Create a rotation around the ideal direction
	FVector Right = FVector::CrossProduct(IdealDirection, FVector::UpVector);
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(IdealDirection, FVector::RightVector);
	}
	Right.Normalize();
	FVector Up = FVector::CrossProduct(Right, IdealDirection);
	Up.Normalize();

	FVector Offset = Right * FMath::Cos(RandomAngle) * RandomMagnitude + Up * FMath::Sin(RandomAngle) * RandomMagnitude;
	FVector AdjustedDirection = (IdealDirection.GetSafeNormal() + Offset).GetSafeNormal();

	return AdjustedDirection;
}

float UAoCNPCImperfection::GetCurrentAimSpread(float Distance, bool bTargetMoving, float WeaponSkillLevel) const
{
	// Base spread from skill level
	float SkillFraction = FMath::Clamp(WeaponSkillLevel / 100.0f, 0.0f, 1.0f);
	float Spread = FMath::Lerp(AimConfig.BaseSpreadDegrees, AimConfig.MinSpreadDegrees, SkillFraction);

	// Distance degrades accuracy
	float DistanceFactor = (Distance / 100.0f) * AimConfig.DistanceSpreadPerHundred;
	Spread += DistanceFactor;

	// Moving targets are harder to hit
	if (bTargetMoving)
	{
		Spread += AimConfig.MovingTargetAdditionalSpread;
	}

	// Stress (low HP) makes aim worse
	if (CurrentHPFraction < AimConfig.StressHPThreshold)
	{
		Spread *= AimConfig.StressSpreadMultiplier;
	}

	// Dexterity improves accuracy
	float Dexterity = GetAttribute(TEXT("Dexterity"));
	float DexReduction = Dexterity * AimConfig.DexterityAccuracyFactor;
	Spread *= (1.0f - DexReduction);

	// Panicked state makes aim much worse
	if (CurrentAlertState == ENPCAlertState::Panicked)
	{
		Spread *= 2.0f;
	}

	// Clamp to prevent absurd values
	return FMath::Clamp(Spread, AimConfig.MinSpreadDegrees * 0.5f, 30.0f);
}

// ============================================================================
// DECISION MISTAKES
// ============================================================================

EDecisionMistake UAoCNPCImperfection::RollForMistake() const
{
	// Base chance modified by alertness and fatigue
	float MistakeChance = BaseMistakeChance;

	// Fatigue increases mistakes
	MistakeChance += CurrentFatigue * 0.05f;

	// Alert state reduces mistakes
	if (CurrentAlertState == ENPCAlertState::Alert || CurrentAlertState == ENPCAlertState::Combat)
	{
		MistakeChance *= 0.5f;
	}

	// Panicked state increases mistakes significantly
	if (CurrentAlertState == ENPCAlertState::Panicked)
	{
		MistakeChance *= 2.0f;
	}

	// Roll the dice
	if (FMath::FRand() > MistakeChance)
	{
		return EDecisionMistake::None;
	}

	// Pick a random mistake type (weighted)
	float Roll = FMath::FRand();

	if (Roll < 0.25f)      return EDecisionMistake::SuboptimalAbility;
	if (Roll < 0.40f)      return EDecisionMistake::ForgotToEat;
	if (Roll < 0.55f)      return EDecisionMistake::MissedResource;
	if (Roll < 0.65f)      return EDecisionMistake::WrongCraftItem;
	if (Roll < 0.75f)      return EDecisionMistake::OverreactedToDanger;
	if (Roll < 0.85f)      return EDecisionMistake::TookWrongPath;
	if (Roll < 0.93f)      return EDecisionMistake::FumbledItem;
	return EDecisionMistake::MiscountedResources;
}

bool UAoCNPCImperfection::ShouldMissResourceNode() const
{
	// Lower Wisdom = more likely to miss
	float Wisdom = GetAttribute(TEXT("Wisdom"));
	float MissChance = 0.08f * (1.0f - Wisdom / 150.0f); // At 0 Wis: 8%. At 100 Wis: ~2.7%

	// Focused state = less likely to miss
	if (CurrentAlertState == ENPCAlertState::Focused)
	{
		MissChance *= 0.3f;
	}

	// Distracted (socializing, panicked) = more likely
	if (CurrentAlertState == ENPCAlertState::Panicked || CurrentAlertState == ENPCAlertState::Relaxed)
	{
		MissChance *= 2.0f;
	}

	return FMath::FRand() < MissChance;
}

bool UAoCNPCImperfection::ShouldUseSuboptimalAbility() const
{
	// 3-8% chance depending on combat alertness
	float Chance = FMath::Lerp(0.08f, 0.03f,
		(CurrentAlertState == ENPCAlertState::Combat) ? 1.0f : 0.0f);

	// Intelligence reduces this chance
	float Intelligence = GetAttribute(TEXT("Intelligence"));
	Chance *= (1.0f - Intelligence / 200.0f);

	return FMath::FRand() < Chance;
}

bool UAoCNPCImperfection::ShouldForgetToEat() const
{
	// Only when moderately hungry (not critical)
	// The brain should pass hunger level; here we just provide the probability
	float ForgetChance = 0.06f;

	// Focused NPCs forget to eat more often
	if (CurrentAlertState == ENPCAlertState::Focused || CurrentAlertState == ENPCAlertState::Combat)
	{
		ForgetChance *= 2.0f;
	}

	// Wisdom reduces forgetfulness
	float Wisdom = GetAttribute(TEXT("Wisdom"));
	ForgetChance *= (1.0f - Wisdom / 200.0f);

	return FMath::FRand() < ForgetChance;
}

// ============================================================================
// MOVEMENT IMPERFECTION
// ============================================================================

void UAoCNPCImperfection::TickMovementImperfection(float DeltaTime)
{
	// --- PATH WANDER ---
	MovementState.WanderTimer += DeltaTime;
	if (MovementState.WanderTimer >= MovementConfig.PathWanderInterval)
	{
		MovementState.WanderTimer = 0.0f;

		// New random lateral offset
		FVector NewWander = FVector(
			FMath::RandRange(-MovementConfig.PathWanderMax, MovementConfig.PathWanderMax),
			FMath::RandRange(-MovementConfig.PathWanderMax, MovementConfig.PathWanderMax),
			0.0f
		);

		// Smooth interpolation to new wander target
		MovementState.CurrentWanderOffset = FMath::VInterpTo(
			MovementState.CurrentWanderOffset,
			NewWander,
			DeltaTime,
			2.0f
		);
	}

	// --- SPEED VARIANCE ---
	MovementState.SpeedVarianceTimer += DeltaTime;
	if (MovementState.SpeedVarianceTimer >= MovementConfig.SpeedVarianceInterval)
	{
		MovementState.SpeedVarianceTimer = 0.0f;
		float NewMultiplier = 1.0f + FMath::RandRange(-MovementConfig.SpeedVarianceFraction, MovementConfig.SpeedVarianceFraction);
		MovementState.CurrentSpeedMultiplier = FMath::FInterpTo(
			MovementState.CurrentSpeedMultiplier, NewMultiplier, DeltaTime, 1.0f);
	}

	// --- RANDOM STOPS ---
	if (MovementState.bIsRandomlyStopped)
	{
		MovementState.RandomStopTimer += DeltaTime;
		if (MovementState.RandomStopTimer >= MovementState.RandomStopDuration)
		{
			MovementState.bIsRandomlyStopped = false;
			MovementState.RandomStopTimer = 0.0f;
		}
	}
	else
	{
		// Check for random stop (only during non-combat, non-alert states)
		if (CurrentAlertState != ENPCAlertState::Combat && CurrentAlertState != ENPCAlertState::Alert &&
			CurrentAlertState != ENPCAlertState::Panicked)
		{
			if (FMath::FRand() < MovementConfig.RandomStopChance * DeltaTime)
			{
				MovementState.bIsRandomlyStopped = true;
				MovementState.RandomStopTimer = 0.0f;
				MovementState.RandomStopDuration = FMath::RandRange(
					MovementConfig.RandomStopMinDuration,
					MovementConfig.RandomStopMaxDuration);
			}
		}
	}
}

FVector UAoCNPCImperfection::GetPathWanderOffset() const
{
	// Don't wander during combat or when panicked
	if (CurrentAlertState == ENPCAlertState::Combat || CurrentAlertState == ENPCAlertState::Panicked)
	{
		return FVector::ZeroVector;
	}

	return MovementState.CurrentWanderOffset;
}

float UAoCNPCImperfection::GetSpeedMultiplier() const
{
	if (MovementState.bIsRandomlyStopped)
	{
		return 0.0f; // Stopped
	}

	return MovementState.CurrentSpeedMultiplier;
}

bool UAoCNPCImperfection::IsRandomlyStopped() const
{
	return MovementState.bIsRandomlyStopped;
}

bool UAoCNPCImperfection::ShouldOvershootDestination() const
{
	// Don't overshoot during combat or focused tasks
	if (CurrentAlertState == ENPCAlertState::Combat || CurrentAlertState == ENPCAlertState::Focused)
	{
		return false;
	}

	return FMath::FRand() < MovementConfig.OvershootChance;
}

FVector UAoCNPCImperfection::GetOvershootOffset(FVector MoveDirection) const
{
	return MoveDirection.GetSafeNormal() * MovementConfig.OvershootDistance;
}

FRotator UAoCNPCImperfection::GetHeadLookTarget() const
{
	return MovementState.HeadLookTarget;
}

// ============================================================================
// HEAD LOOK
// ============================================================================

void UAoCNPCImperfection::TickHeadLook(float DeltaTime)
{
	MovementState.HeadLookTimer += DeltaTime;

	if (MovementState.HeadLookTimer >= MovementState.HeadLookChangeInterval)
	{
		MovementState.HeadLookTimer = 0.0f;

		// Pick a new random head look direction
		float YawRange = 60.0f; // Look left/right up to 60 degrees
		float PitchRange = 20.0f; // Look up/down up to 20 degrees

		// Alert NPCs scan more actively
		if (CurrentAlertState == ENPCAlertState::Alert || CurrentAlertState == ENPCAlertState::Panicked)
		{
			YawRange = 90.0f;
			MovementState.HeadLookChangeInterval = FMath::RandRange(1.5f, 3.0f);
		}
		else if (CurrentAlertState == ENPCAlertState::Focused)
		{
			// Focused NPCs barely look around
			YawRange = 15.0f;
			PitchRange = 5.0f;
			MovementState.HeadLookChangeInterval = FMath::RandRange(5.0f, 10.0f);
		}
		else
		{
			MovementState.HeadLookChangeInterval = FMath::RandRange(3.0f, 7.0f);
		}

		// Sometimes just look forward (30% of the time)
		if (FMath::FRand() < 0.3f)
		{
			MovementState.HeadLookTarget = FRotator::ZeroRotator;
		}
		else
		{
			MovementState.HeadLookTarget = FRotator(
				FMath::RandRange(-PitchRange, PitchRange),
				FMath::RandRange(-YawRange, YawRange),
				0.0f
			);
		}
	}
}

// ============================================================================
// IDLE BEHAVIORS
// ============================================================================

void UAoCNPCImperfection::TickIdleBehaviors(float DeltaTime)
{
	// Only do idle behaviors when idle or relaxed
	if (CurrentAlertState == ENPCAlertState::Combat ||
		CurrentAlertState == ENPCAlertState::Alert ||
		CurrentAlertState == ENPCAlertState::Panicked ||
		CurrentAlertState == ENPCAlertState::Sleeping)
	{
		IdleState.bIsPerformingIdleBehavior = false;
		return;
	}

	// Currently performing an idle behavior?
	if (IdleState.bIsPerformingIdleBehavior)
	{
		IdleState.IdleElapsed += DeltaTime;
		if (IdleState.IdleElapsed >= IdleState.CurrentIdleDuration)
		{
			// Idle behavior complete
			IdleState.bIsPerformingIdleBehavior = false;
			IdleState.CurrentIdleBehavior = EIdleBehavior::None;
			IdleState.TimeUntilNextIdleBehavior = FMath::RandRange(IdleBehaviorMinInterval, IdleBehaviorMaxInterval);

			UE_LOG(LogImperfection, Verbose, TEXT("[%s] Idle behavior finished"),
				OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"));
		}
		return;
	}

	// Count down to next idle behavior
	IdleState.TimeUntilNextIdleBehavior -= DeltaTime;
	if (IdleState.TimeUntilNextIdleBehavior <= 0.0f)
	{
		// Start a new idle behavior
		EIdleBehavior Behavior = PickRandomIdleBehavior();
		float Duration = GetIdleBehaviorDuration(Behavior);

		IdleState.CurrentIdleBehavior = Behavior;
		IdleState.CurrentIdleDuration = Duration;
		IdleState.IdleElapsed = 0.0f;
		IdleState.bIsPerformingIdleBehavior = true;

		TotalIdleBehaviors++;

		OnIdleBehaviorStart.Broadcast(Behavior);

		UE_LOG(LogImperfection, Verbose, TEXT("[%s] Starting idle behavior: %d for %.1fs"),
			OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"),
			(int32)Behavior, Duration);
	}
}

bool UAoCNPCImperfection::IsPerformingIdleBehavior() const
{
	return IdleState.bIsPerformingIdleBehavior;
}

EIdleBehavior UAoCNPCImperfection::GetCurrentIdleBehavior() const
{
	return IdleState.CurrentIdleBehavior;
}

void UAoCNPCImperfection::ForceIdleBehavior(EIdleBehavior Behavior, float Duration)
{
	IdleState.CurrentIdleBehavior = Behavior;
	IdleState.CurrentIdleDuration = Duration;
	IdleState.IdleElapsed = 0.0f;
	IdleState.bIsPerformingIdleBehavior = true;

	OnIdleBehaviorStart.Broadcast(Behavior);
}

EIdleBehavior UAoCNPCImperfection::PickRandomIdleBehavior() const
{
	// Weighted selection — some behaviors are more common
	struct FWeightedBehavior
	{
		EIdleBehavior Behavior;
		float Weight;
	};

	static const TArray<FWeightedBehavior> Behaviors = {
		{EIdleBehavior::LookAround,     25.0f},
		{EIdleBehavior::ShiftWeight,    20.0f},
		{EIdleBehavior::Scratch,         8.0f},
		{EIdleBehavior::Stretch,         8.0f},
		{EIdleBehavior::CheckInventory, 15.0f},
		{EIdleBehavior::WanderShort,    10.0f},
		{EIdleBehavior::KickGround,      5.0f},
		{EIdleBehavior::CrossArms,       9.0f},
	};

	float TotalWeight = 0.0f;
	for (const auto& B : Behaviors) TotalWeight += B.Weight;

	float Roll = FMath::FRand() * TotalWeight;
	float Accumulator = 0.0f;

	for (const auto& B : Behaviors)
	{
		Accumulator += B.Weight;
		if (Roll <= Accumulator)
		{
			return B.Behavior;
		}
	}

	return EIdleBehavior::LookAround; // Fallback
}

float UAoCNPCImperfection::GetIdleBehaviorDuration(EIdleBehavior Behavior) const
{
	switch (Behavior)
	{
	case EIdleBehavior::LookAround:       return FMath::RandRange(2.0f, 5.0f);
	case EIdleBehavior::ShiftWeight:      return FMath::RandRange(1.0f, 3.0f);
	case EIdleBehavior::Scratch:          return FMath::RandRange(2.0f, 4.0f);
	case EIdleBehavior::Stretch:          return FMath::RandRange(3.0f, 5.0f);
	case EIdleBehavior::CheckInventory:   return FMath::RandRange(2.0f, 5.0f);
	case EIdleBehavior::WanderShort:      return FMath::RandRange(4.0f, 10.0f);
	case EIdleBehavior::SitDown:          return FMath::RandRange(10.0f, 60.0f);
	case EIdleBehavior::LeanOnWall:       return FMath::RandRange(5.0f, 20.0f);
	case EIdleBehavior::KickGround:       return FMath::RandRange(1.5f, 3.0f);
	case EIdleBehavior::CrossArms:        return FMath::RandRange(5.0f, 15.0f);
	default:                              return 3.0f;
	}
}

// ============================================================================
// ATTENTION / PERCEPTION
// ============================================================================

bool UAoCNPCImperfection::CanDetectVisual(FVector TargetLocation) const
{
	if (!OwnerNPC) return false;

	// Get NPC's forward direction and position
	FVector MyLoc = OwnerNPC->GetActorLocation();
	FVector MyForward = OwnerNPC->GetActorForwardVector();
	FVector ToTarget = (TargetLocation - MyLoc);
	float Distance = ToTarget.Size();

	// Check distance (visual range based on alert state)
	float MaxVisualRange = 8000.0f; // 80 meters base
	if (CurrentAlertState == ENPCAlertState::Alert)
	{
		MaxVisualRange = 12000.0f; // 120m when alert
	}
	else if (CurrentAlertState == ENPCAlertState::Sleeping)
	{
		return false; // Can't see while sleeping
	}

	// Wisdom extends visual range
	float Wisdom = GetAttribute(TEXT("Wisdom"));
	MaxVisualRange *= (1.0f + Wisdom * AttentionConfig.WisdomPerceptionFactor);

	if (Distance > MaxVisualRange)
	{
		return false;
	}

	// Check field of view
	ToTarget.Normalize();
	float DotProduct = FVector::DotProduct(MyForward, ToTarget);
	float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

	float FOVHalfAngle = GetCurrentFOV();
	if (AngleDegrees > FOVHalfAngle)
	{
		return false; // Outside field of view
	}

	// TODO: Line-of-sight check (raycast for obstacles)

	return true;
}

bool UAoCNPCImperfection::CanDetectAudio(FVector SoundLocation, float Loudness) const
{
	if (!OwnerNPC) return false;

	// Sleeping NPCs can still hear loud sounds
	if (CurrentAlertState == ENPCAlertState::Sleeping && Loudness < 0.7f)
	{
		return false;
	}

	float DetectionRange = GetAudioDetectionRange(Loudness);
	float Distance = FVector::Dist(OwnerNPC->GetActorLocation(), SoundLocation);

	return Distance <= DetectionRange;
}

float UAoCNPCImperfection::GetCurrentFOV() const
{
	switch (CurrentAlertState)
	{
	case ENPCAlertState::Sleeping:   return 0.0f;
	case ENPCAlertState::Relaxed:    return AttentionConfig.RelaxedFOV;
	case ENPCAlertState::Idle:       return AttentionConfig.RelaxedFOV;
	case ENPCAlertState::Alert:      return AttentionConfig.AlertFOV;
	case ENPCAlertState::Combat:     return AttentionConfig.AlertFOV;
	case ENPCAlertState::Focused:    return AttentionConfig.FocusedFOV;
	case ENPCAlertState::Panicked:   return AttentionConfig.PanickedFOV;
	default:                         return AttentionConfig.RelaxedFOV;
	}
}

float UAoCNPCImperfection::GetAudioDetectionRange(float Loudness) const
{
	// Interpolate between whisper range and combat noise range based on loudness
	float BaseRange = FMath::Lerp(AttentionConfig.WhisperRange, AttentionConfig.CombatNoiseRange, Loudness);

	// Wisdom improves hearing
	float Wisdom = GetAttribute(TEXT("Wisdom"));
	BaseRange *= (1.0f + Wisdom * AttentionConfig.WisdomPerceptionFactor);

	// Alert state modifies
	switch (CurrentAlertState)
	{
	case ENPCAlertState::Sleeping:   BaseRange *= 0.3f; break;
	case ENPCAlertState::Relaxed:    BaseRange *= 0.8f; break;
	case ENPCAlertState::Alert:      BaseRange *= 1.3f; break;
	case ENPCAlertState::Focused:    BaseRange *= 0.6f; break; // Focused on task, less aware
	case ENPCAlertState::Panicked:   BaseRange *= 1.5f; break; // Hyper-aware of sounds
	default: break;
	}

	return BaseRange;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void UAoCNPCImperfection::SetAlertState(ENPCAlertState NewState)
{
	if (CurrentAlertState != NewState)
	{
		UE_LOG(LogImperfection, Verbose, TEXT("[%s] Alert state: %d → %d"),
			OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"),
			(int32)CurrentAlertState, (int32)NewState);

		CurrentAlertState = NewState;

		// Reset random stops if entering combat/alert
		if (NewState == ENPCAlertState::Combat || NewState == ENPCAlertState::Alert ||
			NewState == ENPCAlertState::Panicked)
		{
			MovementState.bIsRandomlyStopped = false;
			IdleState.bIsPerformingIdleBehavior = false;
		}
	}
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

float UAoCNPCImperfection::GetAttribute(const FString& AttributeName) const
{
	if (SkillSystem)
	{
		// Query the skill system for attribute values (0-100 range)
		return SkillSystem->GetAttributeByName(AttributeName);
	}

	// Default fallback: mid-range
	return 50.0f;
}

float UAoCNPCImperfection::GetBaseMistakeChance() const
{
	// Base 3-8% depending on personality
	// TODO: Query actual personality from brain component
	// "Careful" personalities make fewer mistakes, "Reckless" make more

	float BaseChance = 0.05f; // 5% default

	// Intelligence reduces mistakes
	float Intelligence = GetAttribute(TEXT("Intelligence"));
	BaseChance *= (1.0f - Intelligence / 250.0f); // At 100 Int: ~3%

	return FMath::Clamp(BaseChance, 0.02f, 0.10f);
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

FString UAoCNPCImperfection::GetDebugString() const
{
	FString Result;
	Result += TEXT("=== Imperfection System ===\n");
	Result += FString::Printf(TEXT("Alert State: %d | Reaction: %.0fms | FOV: %.0f°\n"),
		(int32)CurrentAlertState, GetReactionDelay() * 1000.0f, GetCurrentFOV());
	Result += FString::Printf(TEXT("HP: %.0f%% | Fatigue: %.0f%% | Mistakes Made: %d\n"),
		CurrentHPFraction * 100.0f, CurrentFatigue * 100.0f, TotalMistakesMade);
	Result += FString::Printf(TEXT("Speed Mult: %.2f | Wander: (%.1f, %.1f) | Stopped: %s\n"),
		MovementState.CurrentSpeedMultiplier,
		MovementState.CurrentWanderOffset.X, MovementState.CurrentWanderOffset.Y,
		MovementState.bIsRandomlyStopped ? TEXT("YES") : TEXT("NO"));
	Result += FString::Printf(TEXT("Idle: %s (Type: %d) | Total Idles: %d\n"),
		IdleState.bIsPerformingIdleBehavior ? TEXT("ACTIVE") : TEXT("WAITING"),
		(int32)IdleState.CurrentIdleBehavior, TotalIdleBehaviors);
	Result += FString::Printf(TEXT("Head Look: P=%.1f Y=%.1f\n"),
		MovementState.HeadLookTarget.Pitch, MovementState.HeadLookTarget.Yaw);
	return Result;
}
