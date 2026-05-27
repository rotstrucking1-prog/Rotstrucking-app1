// AoCNPCMemory.cpp — NPC world knowledge and memory implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCMemory.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

UAoCNPCMemory::UAoCNPCMemory()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAoCNPCMemory::BeginPlay()
{
	Super::BeginPlay();

	// Remember spawn location as "Home" if not already set
	if (!KnownLocations.Contains(FName("Home")))
	{
		KnownLocations.Add(FName("Home"), GetOwner()->GetActorLocation());
	}
}

// ---------------------------------------------------------------------------
// Locations
// ---------------------------------------------------------------------------

void UAoCNPCMemory::RememberLocation(FName ID, FVector Pos)
{
	KnownLocations.Add(ID, Pos);
}

FVector UAoCNPCMemory::GetLocationOf(FName ID) const
{
	const FVector* Found = KnownLocations.Find(ID);
	if (Found)
	{
		return *Found;
	}
	// Return owner location as fallback
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

bool UAoCNPCMemory::KnowsLocation(FName ID) const
{
	return KnownLocations.Contains(ID);
}

FVector UAoCNPCMemory::GetNearestKnownLocation(FName Type, FVector FromPos) const
{
	float BestDist = TNumericLimits<float>::Max();
	FVector BestPos = FromPos;
	bool bFound = false;

	for (const auto& Pair : KnownLocations)
	{
		// Match locations whose name contains the Type string
		// e.g., Type="Mine" matches "Mine", "Mine_North", "Mine_2"
		const FString LocName = Pair.Key.ToString();
		const FString TypeStr = Type.ToString();

		if (LocName.Contains(TypeStr))
		{
			const float Dist = FVector::Dist(FromPos, Pair.Value);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				BestPos = Pair.Value;
				bFound = true;
			}
		}
	}

	// Exact match fallback if substring didn't find anything
	if (!bFound)
	{
		const FVector* Exact = KnownLocations.Find(Type);
		if (Exact)
		{
			BestPos = *Exact;
		}
	}

	return BestPos;
}

void UAoCNPCMemory::AddVisitedBreadcrumb(FVector Location)
{
	FVisitedLocation Breadcrumb;
	Breadcrumb.Location = Location;
	Breadcrumb.VisitTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	VisitedLocations.Add(Breadcrumb);

	// Trim old entries
	while (VisitedLocations.Num() > MaxVisitedLocations)
	{
		VisitedLocations.RemoveAt(0);
	}
}

// ---------------------------------------------------------------------------
// Threats
// ---------------------------------------------------------------------------

void UAoCNPCMemory::RememberThreat(AActor* ThreatActor, float ThreatLevel)
{
	if (!ThreatActor) return;

	// Update existing entry or add new one
	for (FNPCThreatMemory& Existing : KnownThreats)
	{
		if (Existing.ThreatActor.Get() == ThreatActor)
		{
			Existing.ThreatLevel = FMath::Max(Existing.ThreatLevel, ThreatLevel);
			Existing.LastSeenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			Existing.LastSeenLocation = ThreatActor->GetActorLocation();
			return;
		}
	}

	// New threat
	FNPCThreatMemory NewThreat;
	NewThreat.ThreatActor = ThreatActor;
	NewThreat.ThreatLevel = ThreatLevel;
	NewThreat.LastSeenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	NewThreat.LastSeenLocation = ThreatActor->GetActorLocation();
	KnownThreats.Add(NewThreat);
}

void UAoCNPCMemory::ForgetOldThreats(float MaxAgeSeconds)
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	KnownThreats.RemoveAll([CurrentTime, MaxAgeSeconds](const FNPCThreatMemory& Threat)
	{
		return (CurrentTime - Threat.LastSeenTime) > MaxAgeSeconds;
	});
}

bool UAoCNPCMemory::HasActiveThreats() const
{
	for (const FNPCThreatMemory& Threat : KnownThreats)
	{
		if (Threat.ThreatActor.IsValid())
		{
			return true;
		}
	}
	return false;
}

float UAoCNPCMemory::GetHighestThreatLevel() const
{
	float Highest = 0.f;
	for (const FNPCThreatMemory& Threat : KnownThreats)
	{
		if (Threat.ThreatActor.IsValid() && Threat.ThreatLevel > Highest)
		{
			Highest = Threat.ThreatLevel;
		}
	}
	return Highest;
}

AActor* UAoCNPCMemory::GetHighestThreatActor() const
{
	AActor* Best = nullptr;
	float Highest = 0.f;
	for (const FNPCThreatMemory& Threat : KnownThreats)
	{
		if (Threat.ThreatActor.IsValid() && Threat.ThreatLevel > Highest)
		{
			Highest = Threat.ThreatLevel;
			Best = Threat.ThreatActor.Get();
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------
// Allies
// ---------------------------------------------------------------------------

void UAoCNPCMemory::RememberAlly(AActor* AllyActor, FName Faction, float Trust)
{
	if (!AllyActor) return;

	for (FNPCAllyMemory& Existing : KnownAllies)
	{
		if (Existing.AllyActor.Get() == AllyActor)
		{
			Existing.Trust = Trust;
			return;
		}
	}

	FNPCAllyMemory NewAlly;
	NewAlly.AllyActor = AllyActor;
	NewAlly.Faction = Faction;
	NewAlly.Trust = Trust;
	KnownAllies.Add(NewAlly);
}

bool UAoCNPCMemory::IsAlly(AActor* Actor) const
{
	if (!Actor) return false;
	for (const FNPCAllyMemory& Ally : KnownAllies)
	{
		if (Ally.AllyActor.Get() == Actor && Ally.Trust > 0.f)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Player Encounters
// ---------------------------------------------------------------------------

void UAoCNPCMemory::RecordPlayerEncounter(ACharacter* Player, EEncounterType Type)
{
	if (!Player) return;

	FPlayerMemory& Mem = PlayerEncounters.FindOrAdd(Player);
	Mem.LastEncounterTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Mem.LastEncounterLocation = Player->GetActorLocation();

	switch (Type)
	{
	case EEncounterType::Attacked:
		Mem.TimesAttacked++;
		Mem.Hostility = FMath::Clamp(Mem.Hostility - 20.f, -100.f, 100.f);
		break;
	case EEncounterType::Traded:
		Mem.TimesTraded++;
		Mem.Hostility = FMath::Clamp(Mem.Hostility + 10.f, -100.f, 100.f);
		break;
	case EEncounterType::Helped:
		Mem.TimesHelped++;
		Mem.Hostility = FMath::Clamp(Mem.Hostility + 15.f, -100.f, 100.f);
		break;
	case EEncounterType::Threatened:
		Mem.Hostility = FMath::Clamp(Mem.Hostility - 10.f, -100.f, 100.f);
		break;
	case EEncounterType::Fled:
		Mem.Hostility = FMath::Clamp(Mem.Hostility - 5.f, -100.f, 100.f);
		break;
	}
}

float UAoCNPCMemory::GetPlayerHostility(ACharacter* Player) const
{
	if (!Player) return 0.f;

	const FPlayerMemory* Mem = PlayerEncounters.Find(Player);
	return Mem ? Mem->Hostility : 0.f;
}

bool UAoCNPCMemory::HasMetPlayer(ACharacter* Player) const
{
	return Player && PlayerEncounters.Contains(Player);
}

FPlayerMemory UAoCNPCMemory::GetPlayerMemory(ACharacter* Player) const
{
	if (!Player) return FPlayerMemory();

	const FPlayerMemory* Mem = PlayerEncounters.Find(Player);
	return Mem ? *Mem : FPlayerMemory();
}
