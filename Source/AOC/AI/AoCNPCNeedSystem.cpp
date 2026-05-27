// AoCNPCNeedSystem.cpp — Biological/Psychological need tracking implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCNeedSystem.h"

UAoCNPCNeedSystem::UAoCNPCNeedSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
// Need pointer helpers
// ---------------------------------------------------------------------------

float* UAoCNPCNeedSystem::GetNeedPtr(ENPCNeed Need)
{
	switch (Need)
	{
	case ENPCNeed::Hunger:  return &Hunger;
	case ENPCNeed::Energy:  return &Energy;
	case ENPCNeed::Safety:  return &Safety;
	case ENPCNeed::Purpose: return &Purpose;
	case ENPCNeed::Social:  return &Social;
	case ENPCNeed::Wealth:  return &Wealth;
	default: return nullptr;
	}
}

const float* UAoCNPCNeedSystem::GetNeedPtr(ENPCNeed Need) const
{
	switch (Need)
	{
	case ENPCNeed::Hunger:  return &Hunger;
	case ENPCNeed::Energy:  return &Energy;
	case ENPCNeed::Safety:  return &Safety;
	case ENPCNeed::Purpose: return &Purpose;
	case ENPCNeed::Social:  return &Social;
	case ENPCNeed::Wealth:  return &Wealth;
	default: return nullptr;
	}
}

// ---------------------------------------------------------------------------
// TickNeeds
// ---------------------------------------------------------------------------

void UAoCNPCNeedSystem::TickNeeds(float DeltaTime)
{
	// Convert DeltaTime (seconds) to per-minute decay
	const float MinuteFraction = DeltaTime / 60.f;

	// Hunger decays
	Hunger = FMath::Clamp(Hunger - (HungerDecayRate * MinuteFraction), 0.f, 100.f);

	// Energy decays — faster when sprinting or in combat
	float EnergyMult = 1.f;
	if (bIsSprinting) EnergyMult = 3.f;
	else if (bIsInCombat) EnergyMult = 2.f;
	Energy = FMath::Clamp(Energy - (EnergyDecayRate * EnergyMult * MinuteFraction), 0.f, 100.f);

	// Safety doesn't decay on its own — it's set externally by perception.
	// It does recover slowly when not being set to a lower value.
	// Recovery: +5 per minute when above 50, +10 per minute when above 80
	if (Safety < 100.f)
	{
		float SafetyRecovery = 5.f;
		if (Safety > 80.f) SafetyRecovery = 10.f;
		Safety = FMath::Clamp(Safety + (SafetyRecovery * MinuteFraction), 0.f, 100.f);
	}

	// Purpose decays when idle, not when working
	if (!bIsWorking)
	{
		Purpose = FMath::Clamp(Purpose - (PurposeDecayRate * MinuteFraction), 0.f, 100.f);
	}
	else
	{
		// Working slowly restores purpose
		Purpose = FMath::Clamp(Purpose + (1.f * MinuteFraction), 0.f, 100.f);
	}

	// Social decays
	Social = FMath::Clamp(Social - (SocialDecayRate * MinuteFraction), 0.f, 100.f);

	// Wealth does NOT decay — it only changes via trading/looting/crafting

	// Broadcast critical events
	if (Hunger < CriticalThreshold)
	{
		OnNeedCritical.Broadcast(ENPCNeed::Hunger, Hunger);
	}
	if (Energy < CriticalThreshold)
	{
		OnNeedCritical.Broadcast(ENPCNeed::Energy, Energy);
	}
	if (Safety < CriticalThreshold)
	{
		OnNeedCritical.Broadcast(ENPCNeed::Safety, Safety);
	}
}

// ---------------------------------------------------------------------------
// Query functions
// ---------------------------------------------------------------------------

ENPCNeed UAoCNPCNeedSystem::GetMostUrgentNeed() const
{
	ENPCNeed MostUrgent = ENPCNeed::Hunger;
	float HighestUrgency = -1.f;

	// Check all needs except Wealth (it doesn't have an urgent threshold the same way)
	const ENPCNeed NeedsToCheck[] = {
		ENPCNeed::Hunger, ENPCNeed::Energy, ENPCNeed::Safety,
		ENPCNeed::Purpose, ENPCNeed::Social, ENPCNeed::Wealth
	};

	for (ENPCNeed Need : NeedsToCheck)
	{
		const float Urgency = GetNeedUrgency(Need);
		if (Urgency > HighestUrgency)
		{
			HighestUrgency = Urgency;
			MostUrgent = Need;
		}
	}

	return MostUrgent;
}

void UAoCNPCNeedSystem::SatisfyNeed(ENPCNeed Need, float Amount)
{
	float* Ptr = GetNeedPtr(Need);
	if (Ptr)
	{
		*Ptr = FMath::Clamp(*Ptr + Amount, 0.f, 100.f);
	}
}

float UAoCNPCNeedSystem::GetNeedUrgency(ENPCNeed Need) const
{
	const float Value = GetNeedValue(Need);

	// Urgency is inverse of satisfaction, scaled so below-threshold values
	// rapidly approach 1.0
	// At 100 (fully satisfied) = 0 urgency
	// At 50 = 0.5 urgency
	// At 20 (critical) = 0.8 urgency
	// At 0 = 1.0 urgency

	// For Wealth, urgency is different: desire for more never becomes "critical"
	if (Need == ENPCNeed::Wealth)
	{
		return FMath::Clamp(1.f - (Value / 100.f), 0.f, 0.6f);
	}

	const float Raw = 1.f - (Value / 100.f);

	// Amplify urgency when below threshold
	if (Value < UrgentThreshold)
	{
		// Quadratic ramp: urgency spikes as it gets lower
		const float Deficit = (UrgentThreshold - Value) / UrgentThreshold;
		return FMath::Clamp(Raw + (Deficit * 0.3f), 0.f, 1.f);
	}

	return FMath::Clamp(Raw, 0.f, 1.f);
}

bool UAoCNPCNeedSystem::IsNeedCritical(ENPCNeed Need) const
{
	return GetNeedValue(Need) < CriticalThreshold;
}

bool UAoCNPCNeedSystem::IsAnyNeedCritical() const
{
	return IsNeedCritical(ENPCNeed::Hunger) ||
	       IsNeedCritical(ENPCNeed::Energy) ||
	       IsNeedCritical(ENPCNeed::Safety);
}

float UAoCNPCNeedSystem::GetNeedValue(ENPCNeed Need) const
{
	const float* Ptr = GetNeedPtr(Need);
	return Ptr ? *Ptr : 0.f;
}

void UAoCNPCNeedSystem::SetNeedValue(ENPCNeed Need, float Value)
{
	float* Ptr = GetNeedPtr(Need);
	if (Ptr)
	{
		*Ptr = FMath::Clamp(Value, 0.f, 100.f);
	}
}

void UAoCNPCNeedSystem::SetSafety(float Value)
{
	Safety = FMath::Clamp(Value, 0.f, 100.f);
}

void UAoCNPCNeedSystem::ConsumeItem(const FString& ItemName)
{
	// Stub: consuming food satisfies hunger
	SatisfyNeed(ENPCNeed::Hunger, 25.0f);
}

float UAoCNPCNeedSystem::GetNeedMax(const FString& NeedName) const
{
	return 100.0f;
}

// GetDecayRate — defined inline in header, removed duplicate
