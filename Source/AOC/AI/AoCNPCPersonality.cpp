// AoCNPCPersonality.cpp — NPC personality traits implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCPersonality.h"
#include "AoCNPCGoalPlanner.h"

UAoCNPCPersonality::UAoCNPCPersonality()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAoCNPCPersonality::BeginPlay()
{
	Super::BeginPlay();

	// Apply archetype on start if not Custom
	if (Archetype != ENPCArchetype::Custom)
	{
		SetArchetype(Archetype);
	}
}

// ---------------------------------------------------------------------------
// Archetype presets
// ---------------------------------------------------------------------------

void UAoCNPCPersonality::SetArchetype(ENPCArchetype NewArchetype)
{
	Archetype = NewArchetype;

	switch (NewArchetype)
	{
	case ENPCArchetype::Villager:
		Aggression = 0.1f; Industry = 0.8f; Courage = 0.3f; Sociability = 0.7f;
		Greed = 0.3f; Curiosity = 0.2f; Intelligence = 0.5f; MagicAffinity = 0.1f;
		break;
	case ENPCArchetype::Guard:
		Aggression = 0.5f; Industry = 0.4f; Courage = 0.8f; Sociability = 0.4f;
		Greed = 0.2f; Curiosity = 0.3f; Intelligence = 0.5f; MagicAffinity = 0.2f;
		break;
	case ENPCArchetype::Bandit:
		Aggression = 0.8f; Industry = 0.3f; Courage = 0.6f; Sociability = 0.2f;
		Greed = 0.9f; Curiosity = 0.4f; Intelligence = 0.4f; MagicAffinity = 0.1f;
		break;
	case ENPCArchetype::Merchant:
		Aggression = 0.1f; Industry = 0.6f; Courage = 0.3f; Sociability = 0.9f;
		Greed = 0.7f; Curiosity = 0.3f; Intelligence = 0.7f; MagicAffinity = 0.1f;
		break;
	case ENPCArchetype::Mage:
		Aggression = 0.4f; Industry = 0.5f; Courage = 0.5f; Sociability = 0.3f;
		Greed = 0.3f; Curiosity = 0.7f; Intelligence = 0.9f; MagicAffinity = 1.0f;
		break;
	case ENPCArchetype::Hunter:
		Aggression = 0.7f; Industry = 0.6f; Courage = 0.7f; Sociability = 0.1f;
		Greed = 0.5f; Curiosity = 0.5f; Intelligence = 0.6f; MagicAffinity = 0.2f;
		break;
	case ENPCArchetype::Hermit:
		Aggression = 0.2f; Industry = 0.9f; Courage = 0.4f; Sociability = 0.0f;
		Greed = 0.1f; Curiosity = 0.1f; Intelligence = 0.8f; MagicAffinity = 0.5f;
		break;
	case ENPCArchetype::Custom:
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Goal weights
// ---------------------------------------------------------------------------

float UAoCNPCPersonality::GetGoalWeight(ENPCGoal Goal) const
{
	// Base weight is 1.0, personality scales it
	switch (Goal)
	{
	// Survival goals — always high priority, personality doesn't change much
	case ENPCGoal::SatisfyHunger:   return 1.0f;
	case ENPCGoal::RestoreEnergy:   return 1.0f;
	case ENPCGoal::SeekSafety:      return 1.0f + (1.0f - Courage) * 0.5f; // cowards prioritize safety more

	// Combat goals
	case ENPCGoal::HuntPlayers:     return Aggression * 1.5f + Greed * 0.5f;
	case ENPCGoal::AttackTarget:    return Aggression * 1.2f;
	case ENPCGoal::FleeFromThreat:  return (1.0f - Courage) * 1.5f;
	case ENPCGoal::HideFromThreat:  return (1.0f - Courage) * 1.0f;

	// Industry goals
	case ENPCGoal::GatherResource:  return Industry * 1.2f;
	case ENPCGoal::CraftItem:       return Industry * 1.0f + Intelligence * 0.3f;
	case ENPCGoal::BuildStructure:  return Industry * 1.0f;
	case ENPCGoal::Mine:            return Industry * 1.0f;
	case ENPCGoal::Chop:            return Industry * 1.0f;
	case ENPCGoal::Smelt:           return Industry * 0.8f + Intelligence * 0.2f;
	case ENPCGoal::Farm:            return Industry * 0.9f;
	case ENPCGoal::Fish:            return Industry * 0.7f + (1.0f - Aggression) * 0.3f;
	case ENPCGoal::Cook:            return Industry * 0.6f;
	case ENPCGoal::EquipGear:       return 0.8f; // most NPCs want to gear up

	// Social/trade goals
	case ENPCGoal::Socialize:       return Sociability * 1.2f;
	case ENPCGoal::Trade:           return Sociability * 0.5f + Greed * 0.5f;

	// Patrol/guard goals
	case ENPCGoal::PatrolArea:      return Courage * 0.5f + (1.0f - Curiosity) * 0.3f;
	case ENPCGoal::GuardLocation:   return Courage * 0.7f + (1.0f - Curiosity) * 0.3f;
	case ENPCGoal::InvestigateNoise: return Curiosity * 1.0f + Courage * 0.3f;

	// Exploration
	case ENPCGoal::ExploreWorld:    return Curiosity * 1.5f;
	case ENPCGoal::ReturnHome:      return (1.0f - Curiosity) * 0.5f;

	// Idle — nobody really wants to be idle
	case ENPCGoal::Idle:            return 0.1f;

	default: return 0.5f;
	}
}

// ---------------------------------------------------------------------------
// Decisions
// ---------------------------------------------------------------------------

bool UAoCNPCPersonality::ShouldFlee(float ThreatLevel) const
{
	// ThreatLevel is 0-100
	// Courage acts as a threshold: higher courage = withstands more threat
	// NPC flees if ThreatLevel exceeds (Courage * 100)
	const float FleeThreshold = Courage * 100.f;
	return ThreatLevel > FleeThreshold;
}

bool UAoCNPCPersonality::ShouldUseMagic() const
{
	// Probabilistic: MagicAffinity is the chance of preferring magic
	return FMath::FRand() < MagicAffinity;
}

int32 UAoCNPCPersonality::GetMaxPlanDepth() const
{
	// Intelligence 0 → 1 step, Intelligence 1 → 10 steps
	return FMath::Clamp(FMath::RoundToInt32(Intelligence * 10.f), 1, 10);
}

float UAoCNPCPersonality::WeighNeedResponse(ENPCNeed Need) const
{
	// Personality modifies how urgently the NPC responds to needs
	switch (Need)
	{
	case ENPCNeed::Hunger:
		// Everyone responds to hunger fairly equally
		return 1.0f;

	case ENPCNeed::Energy:
		// Industrious NPCs push through fatigue; lazy ones rest sooner
		return 1.0f - (Industry * 0.3f);

	case ENPCNeed::Safety:
		// Cowards respond more urgently to safety threats
		return 1.0f + (1.0f - Courage) * 0.5f;

	case ENPCNeed::Purpose:
		// Industrious NPCs hate being idle
		return 0.5f + Industry * 0.5f;

	case ENPCNeed::Social:
		// Social NPCs respond more to social need
		return Sociability;

	case ENPCNeed::Wealth:
		// Greedy NPCs respond more to wealth need
		return Greed;

	default:
		return 1.0f;
	}
}

// ---------------------------------------------------------------------------
// Randomization
// ---------------------------------------------------------------------------

void UAoCNPCPersonality::RandomizeTraits(float Variance)
{
	// Apply random variance around current values
	auto Jitter = [Variance](float Base) -> float
	{
		return FMath::Clamp(Base + FMath::FRandRange(-Variance, Variance), 0.f, 1.f);
	};

	Aggression    = Jitter(Aggression);
	Industry      = Jitter(Industry);
	Courage       = Jitter(Courage);
	Sociability   = Jitter(Sociability);
	Greed         = Jitter(Greed);
	Curiosity     = Jitter(Curiosity);
	Intelligence  = Jitter(Intelligence);
	MagicAffinity = Jitter(MagicAffinity);
}
