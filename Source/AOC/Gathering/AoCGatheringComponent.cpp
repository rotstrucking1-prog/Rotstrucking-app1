#include "AoCGatheringComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

UAoCGatheringComponent::UAoCGatheringComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	MaxStamina = 100.0f;
	CurrentStamina = 100.0f;
	bIsGathering = false;
	CurrentGatherType = EGatheringType::Mining;
	CurrentTargetNode = nullptr;
	GatherProgress = 0.0f;
	GatherHitCount = 0;

	// Initialize all gathering skills to 0
	GatheringSkills.Add(EGatheringType::Mining, 0.0f);
	GatheringSkills.Add(EGatheringType::Woodcutting, 0.0f);
	GatheringSkills.Add(EGatheringType::Quarrying, 0.0f);
	GatheringSkills.Add(EGatheringType::Herbalism, 0.0f);
	GatheringSkills.Add(EGatheringType::Sawing, 0.0f);
}

void UAoCGatheringComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAoCGatheringComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UAoCGatheringComponent::StartGathering(AActor* ResourceNode, EGatheringType Type)
{
	if (!ResourceNode)
	{
		return false;
	}

	if (bIsGathering)
	{
		StopGathering();
	}

	// Check stamina
	const float StaminaCost = GetStaminaCost(Type);
	if (CurrentStamina < StaminaCost)
	{
		return false;
	}

	// Check for required tool
	if (!HasRequiredTool(Type))
	{
		return false;
	}

	// Begin gathering
	bIsGathering = true;
	CurrentGatherType = Type;
	CurrentTargetNode = ResourceNode;
	GatherProgress = 0.0f;
	GatherHitCount = 0;

	// Set up the gather timer
	const float Interval = GetGatherInterval(Type);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GatherTimerHandle,
			this,
			&UAoCGatheringComponent::OnGatherTick,
			Interval,
			true
		);
	}

	return true;
}

void UAoCGatheringComponent::StopGathering()
{
	if (!bIsGathering)
	{
		return;
	}

	bIsGathering = false;
	CurrentTargetNode = nullptr;
	GatherProgress = 0.0f;
	GatherHitCount = 0;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GatherTimerHandle);
	}
}

void UAoCGatheringComponent::OnGatherTick()
{
	if (!bIsGathering || !CurrentTargetNode || !IsValid(CurrentTargetNode))
	{
		StopGathering();
		return;
	}

	// Deduct stamina
	const float StaminaCost = GetStaminaCost(CurrentGatherType);
	CurrentStamina = FMath::Max(0.0f, CurrentStamina - StaminaCost);

	if (CurrentStamina <= 0.0f)
	{
		StopGathering();
		return;
	}

	// Increment hit count
	GatherHitCount++;

	// Broadcast hit event
	OnGatherHit.Broadcast(CurrentGatherType, GatherHitCount);

	// Gain skill XP per hit
	GainSkillXP(CurrentGatherType, 1.0f);

	// Check if node is depleted (uses IAoCResourceNodeInterface or cast — here we use a simple approach)
	// The resource node itself manages depletion; we broadcast complete when node signals depletion
	// For now, the resource node's OnNodeDepleted should be bound externally to call StopGathering
	// We broadcast OnGatherComplete if the target node is pending kill or depleted
	if (!IsValid(CurrentTargetNode))
	{
		OnGatherComplete.Broadcast(CurrentGatherType);
		StopGathering();
	}
}

float UAoCGatheringComponent::GetGatherInterval(EGatheringType Type) const
{
	const float Skill = GetSkill(Type);
	return FMath::Lerp(3.0f, 1.0f, Skill / 100.0f);
}

float UAoCGatheringComponent::GetStaminaCost(EGatheringType Type) const
{
	const float Skill = GetSkill(Type);
	return FMath::Lerp(15.0f, 5.0f, Skill / 100.0f);
}

float UAoCGatheringComponent::GetYieldMultiplier(EGatheringType Type) const
{
	const float Skill = GetSkill(Type);
	return FMath::Lerp(1.0f, 2.5f, Skill / 100.0f);
}

void UAoCGatheringComponent::GainSkillXP(EGatheringType Type, float Amount)
{
	if (float* SkillPtr = GatheringSkills.Find(Type))
	{
		*SkillPtr = FMath::Clamp(*SkillPtr + Amount * 0.1f, 0.0f, 100.0f);
	}
}

bool UAoCGatheringComponent::HasRequiredTool(EGatheringType Type) const
{
	// Placeholder: always returns true for now
	return true;
}

FName UAoCGatheringComponent::GetGatherAnimation(EGatheringType Type) const
{
	switch (Type)
	{
	case EGatheringType::Mining:
		return FName("Mine");
	case EGatheringType::Woodcutting:
		return FName("Chop");
	case EGatheringType::Quarrying:
		return FName("Pick");
	case EGatheringType::Herbalism:
		return FName("Pick");
	case EGatheringType::Sawing:
		return FName("Saw");
	default:
		return FName("Chop");
	}
}

float UAoCGatheringComponent::GetSkill(EGatheringType Type) const
{
	if (const float* SkillPtr = GatheringSkills.Find(Type))
	{
		return *SkillPtr;
	}
	return 0.0f;
}
