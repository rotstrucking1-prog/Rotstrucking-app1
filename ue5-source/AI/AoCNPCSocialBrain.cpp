// AoCNPCSocialBrain.cpp
// Full social system — relationships, groups, betrayal, gossip, reputation

#include "AoCNPCSocialBrain.h"
#include "AoCHumanoidNPCV2.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAoCSocial);

// ---------------------------------------------------------------------------
// Static Faction Matrix
// ---------------------------------------------------------------------------

TMap<FName, TMap<FName, float>> UAoCNPCSocialBrain::FactionHostilityMatrix;
bool UAoCNPCSocialBrain::bFactionMatrixInitialized = false;

void UAoCNPCSocialBrain::InitFactionMatrix()
{
	if (bFactionMatrixInitialized) return;
	bFactionMatrixInitialized = true;

	// Default faction relationships (hostility values: -100 = allied, 0 = neutral, +100 = KOS)
	auto SetRelation = [](FName A, FName B, float Hostility)
	{
		FactionHostilityMatrix.FindOrAdd(A).FindOrAdd(B) = Hostility;
		FactionHostilityMatrix.FindOrAdd(B).FindOrAdd(A) = Hostility;
	};

	SetRelation(TEXT("Town"), TEXT("Town"), -50.f);       // Towns are friendly internally
	SetRelation(TEXT("Town"), TEXT("Bandits"), 80.f);     // Towns hate bandits
	SetRelation(TEXT("Town"), TEXT("Mages"), -20.f);      // Towns tolerate mages
	SetRelation(TEXT("Town"), TEXT("Hunters"), -30.f);    // Hunters are welcome
	SetRelation(TEXT("Town"), TEXT("Hermits"), 0.f);      // Neutral to hermits
	SetRelation(TEXT("Bandits"), TEXT("Bandits"), -30.f); // Bandits cooperate loosely
	SetRelation(TEXT("Bandits"), TEXT("Mages"), 30.f);    // Bandits suspicious of mages
	SetRelation(TEXT("Bandits"), TEXT("Hunters"), 20.f);  // Mild hostility
	SetRelation(TEXT("Mages"), TEXT("Mages"), -40.f);     // Mages help each other
	SetRelation(TEXT("Hunters"), TEXT("Hunters"), -20.f);
	SetRelation(TEXT("Hermits"), TEXT("Hermits"), 0.f);   // Hermits don't care about each other
	SetRelation(TEXT("Guards"), TEXT("Bandits"), 90.f);   // Guards KOS bandits
	SetRelation(TEXT("Guards"), TEXT("Town"), -60.f);     // Guards protect town
	SetRelation(TEXT("Guards"), TEXT("Guards"), -70.f);   // Guards are strongly allied
}

// ---------------------------------------------------------------------------
// Constructor / BeginPlay
// ---------------------------------------------------------------------------

UAoCNPCSocialBrain::UAoCNPCSocialBrain()
{
	PrimaryComponentTick.bCanEverTick = false;
	SocialRand.Initialize(FMath::Rand());
}

void UAoCNPCSocialBrain::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AoCHumanoidNPCV2>(GetOwner());
	if (OwnerNPC.IsValid())
	{
		Personality = OwnerNPC->Personality;
		Memory = OwnerNPC->Memory;
		MyID = OwnerNPC->GetUniqueID();
		MyName = OwnerNPC->GetDisplayName();
		FactionID = OwnerNPC->FactionID;
	}

	InitFactionMatrix();
	SocialRand.Initialize(GetOwner() ? GetOwner()->GetUniqueID() : FMath::Rand());
}

// ---------------------------------------------------------------------------
// Relationship Management
// ---------------------------------------------------------------------------

FNPCRelationship UAoCNPCSocialBrain::GetRelationship(const FGuid& TargetID) const
{
	const FNPCRelationship* Found = Relationships.Find(TargetID);
	if (Found)
	{
		return *Found;
	}

	// Return default relationship (unknown)
	FNPCRelationship Default;
	Default.TargetID = TargetID;
	return Default;
}

FNPCRelationship& UAoCNPCSocialBrain::GetOrCreateRelationship(const FGuid& TargetID, const FString& TargetName)
{
	FNPCRelationship* Found = Relationships.Find(TargetID);
	if (Found)
	{
		// Update name if provided
		if (!TargetName.IsEmpty() && TargetName != TEXT("Unknown"))
		{
			Found->TargetName = TargetName;
		}
		return *Found;
	}

	// Create new relationship
	FNPCRelationship NewRel;
	NewRel.TargetID = TargetID;
	NewRel.TargetName = TargetName;

	// Apply faction-based starting hostility
	// We need to get the target's faction — if it's an NPC, check their faction
	// For now, use neutral as default
	NewRel.Trust = 0.f;
	NewRel.Hostility = 0.f;
	NewRel.Familiarity = 0.f;

	Relationships.Add(TargetID, NewRel);
	return Relationships[TargetID];
}

void UAoCNPCSocialBrain::ModifyTrust(const FGuid& TargetID, float Delta)
{
	FNPCRelationship& Rel = GetOrCreateRelationship(TargetID);
	Rel.Trust = FMath::Clamp(Rel.Trust + Delta, -100.f, 100.f);
	Rel.LastInteractionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Rel.UpdateCachedFlags();

	UE_LOG(LogAoCSocial, Verbose, TEXT("Social: %s trust for %s changed by %.1f → %.1f"),
		*MyName, *Rel.TargetName, Delta, Rel.Trust);
}

void UAoCNPCSocialBrain::ModifyHostility(const FGuid& TargetID, float Delta)
{
	FNPCRelationship& Rel = GetOrCreateRelationship(TargetID);
	Rel.Hostility = FMath::Clamp(Rel.Hostility + Delta, -100.f, 100.f);
	Rel.LastInteractionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Rel.UpdateCachedFlags();

	UE_LOG(LogAoCSocial, Verbose, TEXT("Social: %s hostility for %s changed by %.1f → %.1f"),
		*MyName, *Rel.TargetName, Delta, Rel.Hostility);
}

void UAoCNPCSocialBrain::RecordInteraction(const FGuid& TargetID, const FString& TargetName, EInteractionType Type, float Magnitude)
{
	FNPCRelationship& Rel = GetOrCreateRelationship(TargetID, TargetName);

	FInteractionRecord Record;
	Record.Type = Type;
	Record.Magnitude = Magnitude;
	Record.GameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Rel.InteractionHistory.Add(Record);

	// Keep history manageable
	if (Rel.InteractionHistory.Num() > 50)
	{
		Rel.InteractionHistory.RemoveAt(0, Rel.InteractionHistory.Num() - 50);
	}

	Rel.LastInteractionTime = Record.GameTime;

	// Apply the interaction effects
	ApplyInteractionEffects(Rel, Type, Magnitude);

	UE_LOG(LogAoCSocial, Log, TEXT("Social: %s recorded interaction %d with %s (magnitude: %.1f)"),
		*MyName, static_cast<int32>(Type), *TargetName, Magnitude);
}

void UAoCNPCSocialBrain::ApplyInteractionEffects(FNPCRelationship& Rel, EInteractionType Type, float Magnitude)
{
	switch (Type)
	{
	case EInteractionType::Greeting:
		Rel.Trust += 1.f * Magnitude;
		Rel.Hostility -= 2.f * Magnitude;
		Rel.Familiarity += 1.f * Magnitude;
		break;

	case EInteractionType::Trading:
		Rel.Trust += 3.f * Magnitude;
		Rel.Hostility -= 5.f * Magnitude;
		Rel.Familiarity += 3.f * Magnitude;
		break;

	case EInteractionType::GiftGiving:
		Rel.Trust += 8.f * Magnitude;
		Rel.Hostility -= 10.f * Magnitude;
		Rel.Familiarity += 5.f * Magnitude;
		break;

	case EInteractionType::FightingTogether:
		Rel.Trust += 10.f * Magnitude;
		Rel.Hostility -= 15.f * Magnitude;
		Rel.Familiarity += 8.f * Magnitude;
		break;

	case EInteractionType::SharedKill:
		Rel.Trust += 12.f * Magnitude;
		Rel.Hostility -= 15.f * Magnitude;
		Rel.Familiarity += 10.f * Magnitude;
		break;

	case EInteractionType::Insult:
		Rel.Trust -= 5.f * Magnitude;
		Rel.Hostility += 10.f * Magnitude;
		break;

	case EInteractionType::TheftAttempt:
		Rel.Trust -= 20.f * Magnitude;
		Rel.Hostility += 25.f * Magnitude;
		break;

	case EInteractionType::AttackedMe:
		Rel.Trust -= 30.f * Magnitude;
		Rel.Hostility += 50.f * Magnitude;
		break;

	case EInteractionType::BetrayedMe:
		// MASSIVE negative — nearly irreversible
		Rel.Trust = -100.f;
		Rel.Hostility = 100.f;
		Rel.bWasBetrayed = true;
		Rel.bWantsRevenge = true;
		UE_LOG(LogAoCSocial, Warning, TEXT("Social: %s was BETRAYED by %s! Permanent hostility."),
			*MyName, *Rel.TargetName);
		break;

	case EInteractionType::KilledMyAlly:
		Rel.Trust -= 50.f * Magnitude;
		Rel.Hostility = 100.f; // Permanent hostility
		Rel.bWantsRevenge = true;
		UE_LOG(LogAoCSocial, Warning, TEXT("Social: %s marks %s as enemy — killed ally!"),
			*MyName, *Rel.TargetName);
		break;

	case EInteractionType::HelpedMe:
	case EInteractionType::HealedMe:
	case EInteractionType::RescuedMe:
		Rel.Trust += 15.f * Magnitude;
		Rel.Hostility -= 20.f * Magnitude;
		Rel.Familiarity += 10.f * Magnitude;
		break;

	case EInteractionType::GaveItem:
		Rel.Trust += 10.f * Magnitude;
		Rel.Hostility -= 8.f * Magnitude;
		Rel.Familiarity += 5.f * Magnitude;
		break;

	case EInteractionType::NearbyPresence:
		// Very small passive gain
		Rel.Familiarity += 0.1f * Magnitude;
		break;
	}

	// Clamp values
	Rel.Trust = FMath::Clamp(Rel.Trust, -100.f, 100.f);
	Rel.Hostility = FMath::Clamp(Rel.Hostility, -100.f, 100.f);
	Rel.Familiarity = FMath::Clamp(Rel.Familiarity, 0.f, 100.f);

	Rel.UpdateCachedFlags();
}

// ---------------------------------------------------------------------------
// Quick Checks
// ---------------------------------------------------------------------------

bool UAoCNPCSocialBrain::ShouldAttack(AActor* Target) const
{
	if (!Target) return false;

	// Aggressive NPCs attack players on sight
	if (OwnerNPC.IsValid() && OwnerNPC->bIsAggressive)
	{
		// Check if target is a player
		if (Target->GetInstigatorController() && Target->GetInstigatorController()->IsPlayerController())
		{
			return true;
		}
	}

	// Check relationship
	FGuid TargetGUID = GetActorGUID(const_cast<AActor*>(Target));
	const FNPCRelationship* Rel = Relationships.Find(TargetGUID);
	if (Rel)
	{
		return Rel->bIsEnemy || Rel->Hostility > 60.f;
	}

	// Check faction hostility
	AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(Target);
	if (TargetNPC)
	{
		float FactionHostility = GetFactionBaseHostility(TargetNPC->FactionID);
		return FactionHostility > 70.f;
	}

	return false;
}

bool UAoCNPCSocialBrain::IsAlly(AActor* Target) const
{
	if (!Target) return false;

	FGuid TargetGUID = GetActorGUID(const_cast<AActor*>(Target));
	const FNPCRelationship* Rel = Relationships.Find(TargetGUID);
	if (Rel)
	{
		return Rel->bIsAlly;
	}

	// Same faction = ally by default
	AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(Target);
	if (TargetNPC && TargetNPC->FactionID == FactionID && !FactionID.IsNone())
	{
		return true;
	}

	return false;
}

bool UAoCNPCSocialBrain::IsEnemy(AActor* Target) const
{
	if (!Target) return false;

	FGuid TargetGUID = GetActorGUID(const_cast<AActor*>(Target));
	const FNPCRelationship* Rel = Relationships.Find(TargetGUID);
	if (Rel)
	{
		return Rel->bIsEnemy;
	}

	// Check faction
	AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(Target);
	if (TargetNPC)
	{
		float FactionHostility = GetFactionBaseHostility(TargetNPC->FactionID);
		return FactionHostility > 60.f;
	}

	return false;
}

// ---------------------------------------------------------------------------
// Group System
// ---------------------------------------------------------------------------

bool UAoCNPCSocialBrain::ShouldGroupWith(AoCHumanoidNPCV2* Other) const
{
	if (!Other || !OwnerNPC.IsValid()) return false;
	if (CurrentGroup.IsValid()) return false; // Already in a group
	if (Other == OwnerNPC.Get()) return false;

	// Must be same faction or have positive relationship
	FGuid OtherID = Other->GetUniqueID();
	const FNPCRelationship* Rel = Relationships.Find(OtherID);

	bool bFriendly = false;
	if (Rel)
	{
		bFriendly = Rel->Trust > 10.f && Rel->Hostility < 0.f;
	}
	else
	{
		bFriendly = IsSameFaction(Other);
	}

	if (!bFriendly) return false;

	// Personality check: Aggression + Social > threshold
	// Both NPCs need to be social enough to want to group
	float SocialTrait = 0.5f;  // Default
	float AggressionTrait = 0.3f;

	// In production: read from Personality component
	// SocialTrait = Personality->GetTraitValue(TEXT("Social"));
	// AggressionTrait = Personality->GetTraitValue(TEXT("Aggression"));

	float GroupDesire = SocialTrait * 0.6f + AggressionTrait * 0.4f;
	return GroupDesire > 0.3f;
}

void UAoCNPCSocialBrain::FormGroup(const TArray<AoCHumanoidNPCV2*>& InitialMembers)
{
	if (InitialMembers.Num() < 2) return;

	CurrentGroup.GroupID = FGuid::NewGuid();
	CurrentGroup.Members.Reset();
	CurrentGroup.FormationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Add all members
	float HighestPower = 0.f;
	for (AoCHumanoidNPCV2* Member : InitialMembers)
	{
		if (!Member) continue;
		CurrentGroup.Members.Add(Member);

		float Power = Member->GetCombatPower();
		if (Power > HighestPower)
		{
			HighestPower = Power;
			CurrentGroup.Leader = Member;
		}

		// Set the group on each member's social brain
		if (Member->SocialBrain && Member != OwnerNPC.Get())
		{
			Member->SocialBrain->CurrentGroup = CurrentGroup;
		}

		// Record positive interaction
		RecordInteraction(Member->GetUniqueID(), Member->GetDisplayName(), EInteractionType::FightingTogether, 0.5f);
	}

	UE_LOG(LogAoCSocial, Log, TEXT("Social: %s formed group of %d (leader: %s)"),
		*MyName, CurrentGroup.Members.Num(),
		CurrentGroup.Leader.IsValid() ? *CurrentGroup.Leader->GetDisplayName() : TEXT("???"));
}

void UAoCNPCSocialBrain::LeaveGroup()
{
	if (!CurrentGroup.IsValid()) return;

	UE_LOG(LogAoCSocial, Log, TEXT("Social: %s left group"), *MyName);

	// Remove self from group members list in other members' brains
	for (TWeakObjectPtr<AoCHumanoidNPCV2>& MemberPtr : CurrentGroup.Members)
	{
		if (MemberPtr.IsValid() && MemberPtr.Get() != OwnerNPC.Get())
		{
			AoCHumanoidNPCV2* Member = MemberPtr.Get();
			if (Member->SocialBrain)
			{
				for (int32 i = Member->SocialBrain->CurrentGroup.Members.Num() - 1; i >= 0; --i)
				{
					if (Member->SocialBrain->CurrentGroup.Members[i].Get() == OwnerNPC.Get())
					{
						Member->SocialBrain->CurrentGroup.Members.RemoveAt(i);
						break;
					}
				}
			}
		}
	}

	CurrentGroup = FNPCGroup();
}

TArray<AoCHumanoidNPCV2*> UAoCNPCSocialBrain::GetGroupMembers() const
{
	TArray<AoCHumanoidNPCV2*> Result;
	for (const TWeakObjectPtr<AoCHumanoidNPCV2>& MemberPtr : CurrentGroup.Members)
	{
		if (MemberPtr.IsValid())
		{
			Result.Add(MemberPtr.Get());
		}
	}
	return Result;
}

bool UAoCNPCSocialBrain::IsGroupLeader() const
{
	return CurrentGroup.IsValid() && CurrentGroup.Leader.IsValid() &&
		CurrentGroup.Leader.Get() == OwnerNPC.Get();
}

// ---------------------------------------------------------------------------
// Betrayal System
// ---------------------------------------------------------------------------

bool UAoCNPCSocialBrain::ConsiderBetrayal(AActor* Ally) const
{
	if (!Ally || !OwnerNPC.IsValid()) return false;

	// Read personality traits
	float Greed = 0.3f;   // Default
	float Loyalty = 0.5f;  // Default

	// In production: read from Personality component
	// Greed = Personality->GetTraitValue(TEXT("Greed"));
	// Loyalty = Personality->GetTraitValue(TEXT("Loyalty"));

	// Must have significant greed advantage over loyalty
	if (Greed <= Loyalty) return false;

	// Check if ally has valuable loot (simplified)
	AoCHumanoidNPCV2* AllyNPC = Cast<AoCHumanoidNPCV2>(Ally);
	if (!AllyNPC) return false;

	float AllyValue = AllyNPC->GetCombatPower(); // Use as proxy for gear value
	float Trust = 0.f;

	FGuid AllyID = AllyNPC->GetUniqueID();
	const FNPCRelationship* Rel = Relationships.Find(AllyID);
	if (Rel)
	{
		Trust = Rel->Trust;
	}

	// Betrayal formula: ChanceToBetray = (Greed * ItemValue) / (Loyalty * Trust * 100)
	float TrustFactor = FMath::Max(Trust * 0.01f, 0.01f); // Normalize trust 0-1, minimum 0.01
	float ChanceToBetray = (Greed * AllyValue) / (Loyalty * TrustFactor * 100.f);

	// Opportunity check: ally must be weakened (low HP after fight)
	if (AllyNPC->CombatBrain)
	{
		float AllyHPPercent = AllyNPC->CombatBrain->MaxHP > 0.f ?
			AllyNPC->CombatBrain->CurrentHP / AllyNPC->CombatBrain->MaxHP : 1.f;

		// Ally is low — opportunity multiplier
		if (AllyHPPercent < 0.3f)
		{
			ChanceToBetray *= 3.0f;
		}
		else if (AllyHPPercent < 0.5f)
		{
			ChanceToBetray *= 1.5f;
		}
		else
		{
			ChanceToBetray *= 0.5f; // Risky to betray a healthy ally
		}
	}

	bool bBetrays = SocialRand.FRand() < ChanceToBetray;

	if (bBetrays)
	{
		UE_LOG(LogAoCSocial, Warning, TEXT("Social: %s considers BETRAYING %s! (chance: %.2f)"),
			*MyName, *AllyNPC->GetDisplayName(), ChanceToBetray);
	}

	return bBetrays;
}

// ---------------------------------------------------------------------------
// Reputation Spread
// ---------------------------------------------------------------------------

void UAoCNPCSocialBrain::SpreadReputation(const FGuid& TargetID, const FString& TargetName, float HostilityDelta, float Radius)
{
	if (!OwnerNPC.IsValid()) return;

	// Queue gossip to spread to nearby NPCs
	FGossipItem Gossip;
	Gossip.SubjectID = TargetID;
	Gossip.SubjectName = TargetName;
	Gossip.HostilityChange = HostilityDelta;
	Gossip.TrustChange = -HostilityDelta * 0.5f; // If hostile, reduce trust too
	Gossip.Reason = HostilityDelta > 0 ? TEXT("dangerous") : TEXT("helpful");
	Gossip.GameTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	GossipQueue.Add(Gossip);

	// Immediately tell nearby NPCs
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(World, AoCHumanoidNPCV2::StaticClass(), NearbyActors);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == OwnerNPC.Get()) continue;

		float Dist = FVector::Dist(Actor->GetActorLocation(), OwnerNPC->GetActorLocation());
		if (Dist > Radius) continue;

		AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Actor);
		if (!OtherNPC || !OtherNPC->SocialBrain) continue;

		// Spread is reduced by distance and familiarity
		float DistanceFactor = 1.f - (Dist / Radius);
		float SpreadedHostility = HostilityDelta * DistanceFactor * 0.5f; // 50% at max range

		// They trust our opinion based on how well they know us
		FNPCRelationship OurRel = OtherNPC->SocialBrain->GetRelationship(MyID);
		float TrustWeight = FMath::Clamp((OurRel.Trust + 50.f) / 100.f, 0.1f, 1.f);

		OtherNPC->SocialBrain->ModifyHostility(TargetID, SpreadedHostility * TrustWeight);

		UE_LOG(LogAoCSocial, Verbose, TEXT("Social: %s told %s about %s (hostility: %.1f)"),
			*MyName, *OtherNPC->GetDisplayName(), *TargetName, SpreadedHostility * TrustWeight);
	}
}

// ---------------------------------------------------------------------------
// Faction
// ---------------------------------------------------------------------------

bool UAoCNPCSocialBrain::IsSameFaction(AActor* Other) const
{
	if (!Other || FactionID.IsNone()) return false;

	AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Other);
	if (OtherNPC)
	{
		return OtherNPC->FactionID == FactionID;
	}
	return false;
}

float UAoCNPCSocialBrain::GetFactionBaseHostility(FName OtherFaction) const
{
	if (FactionID == OtherFaction && !FactionID.IsNone()) return -50.f; // Same faction = friendly

	const TMap<FName, float>* Row = FactionHostilityMatrix.Find(FactionID);
	if (Row)
	{
		const float* Value = Row->Find(OtherFaction);
		if (Value) return *Value;
	}
	return 0.f; // Neutral by default
}

// ---------------------------------------------------------------------------
// Socialize Tick
// ---------------------------------------------------------------------------

void UAoCNPCSocialBrain::SocializeTick(float DeltaTime)
{
	if (!OwnerNPC.IsValid()) return;

	// Update proximity familiarity
	UpdateProximityFamiliarity(DeltaTime);

	// Process gossip
	ProcessGossip(DeltaTime);

	// Evaluate group cohesion
	if (CurrentGroup.IsValid())
	{
		EvaluateGroupCohesion();
	}

	// Decay old hostilities/trust slightly over time (people forget)
	for (auto& Pair : Relationships)
	{
		FNPCRelationship& Rel = Pair.Value;

		// Hostility decays slowly toward neutral (unless betrayed)
		if (!Rel.bWasBetrayed && FMath::Abs(Rel.Hostility) > 1.f)
		{
			float DecayRate = 0.01f * DeltaTime; // Very slow
			Rel.Hostility = FMath::FInterpTo(Rel.Hostility, 0.f, DeltaTime, DecayRate);
		}

		// Trust also drifts toward neutral without interaction
		float TimeSinceInteraction = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) - Rel.LastInteractionTime;
		if (TimeSinceInteraction > 600.f) // 10 minutes of no interaction
		{
			if (FMath::Abs(Rel.Trust) > 5.f)
			{
				Rel.Trust = FMath::FInterpTo(Rel.Trust, 0.f, DeltaTime, 0.005f);
			}
		}

		Rel.UpdateCachedFlags();
	}
}

void UAoCNPCSocialBrain::UpdateProximityFamiliarity(float DeltaTime)
{
	if (!OwnerNPC.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	float ProximityRadius = 1500.f; // 15m
	float FamiliarityGainRate = 0.5f; // per second while nearby

	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(World, AoCHumanoidNPCV2::StaticClass(), NearbyActors);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == OwnerNPC.Get()) continue;

		float Dist = FVector::Dist(Actor->GetActorLocation(), OwnerNPC->GetActorLocation());
		if (Dist > ProximityRadius) continue;

		AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Actor);
		if (!OtherNPC) continue;

		FGuid OtherID = OtherNPC->GetUniqueID();
		FNPCRelationship& Rel = GetOrCreateRelationship(OtherID, OtherNPC->GetDisplayName());

		float DistanceMult = 1.f - (Dist / ProximityRadius);
		Rel.Familiarity = FMath::Min(Rel.Familiarity + FamiliarityGainRate * DistanceMult * DeltaTime, 100.f);

		// Passive trust gain from proximity (very slow)
		if (Rel.Hostility < 20.f)
		{
			Rel.Trust = FMath::Min(Rel.Trust + 0.05f * DistanceMult * DeltaTime, 100.f);
		}

		Rel.UpdateCachedFlags();
	}
}

void UAoCNPCSocialBrain::ProcessGossip(float DeltaTime)
{
	GossipCooldown -= DeltaTime;
	if (GossipCooldown > 0.f) return;

	if (GossipQueue.Num() == 0) return;

	// Process one gossip item
	FGossipItem Gossip = GossipQueue[0];
	GossipQueue.RemoveAt(0);

	SpreadReputation(Gossip.SubjectID, Gossip.SubjectName, Gossip.HostilityChange, 3000.f);

	GossipCooldown = 5.f; // 5 seconds between gossip spreads
}

void UAoCNPCSocialBrain::EvaluateGroupCohesion()
{
	if (!CurrentGroup.IsValid() || CurrentGroup.Members.Num() < 2) return;

	// Check if any members have conflicting relationships
	bool bShouldDisband = false;

	// Check for dead/missing members
	for (int32 i = CurrentGroup.Members.Num() - 1; i >= 0; --i)
	{
		if (!CurrentGroup.Members[i].IsValid())
		{
			CurrentGroup.Members.RemoveAt(i);
		}
	}

	if (CurrentGroup.Members.Num() < 2)
	{
		bShouldDisband = true;
	}

	// Check for trust breakdown between members
	for (int32 i = 0; i < CurrentGroup.Members.Num() && !bShouldDisband; ++i)
	{
		TWeakObjectPtr<AoCHumanoidNPCV2>& MemberA = CurrentGroup.Members[i];
		if (!MemberA.IsValid()) continue;

		for (int32 j = i + 1; j < CurrentGroup.Members.Num(); ++j)
		{
			TWeakObjectPtr<AoCHumanoidNPCV2>& MemberB = CurrentGroup.Members[j];
			if (!MemberB.IsValid()) continue;

			if (MemberA->SocialBrain)
			{
				FNPCRelationship Rel = MemberA->SocialBrain->GetRelationship(MemberB->GetUniqueID());
				if (Rel.Trust < -30.f || Rel.Hostility > 50.f)
				{
					bShouldDisband = true;
					UE_LOG(LogAoCSocial, Log, TEXT("Social: Group disbanding — %s and %s have trust issues"),
						*MemberA->GetDisplayName(), *MemberB->GetDisplayName());
					break;
				}
			}
		}
	}

	if (bShouldDisband)
	{
		LeaveGroup();
	}
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

TArray<FGuid> UAoCNPCSocialBrain::GetAllies() const
{
	TArray<FGuid> Result;
	for (const auto& Pair : Relationships)
	{
		if (Pair.Value.bIsAlly)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

TArray<FGuid> UAoCNPCSocialBrain::GetEnemies() const
{
	TArray<FGuid> Result;
	for (const auto& Pair : Relationships)
	{
		if (Pair.Value.bIsEnemy)
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
}

bool UAoCNPCSocialBrain::HasRevengeTarget() const
{
	for (const auto& Pair : Relationships)
	{
		if (Pair.Value.bWantsRevenge)
		{
			return true;
		}
	}
	return false;
}

FGuid UAoCNPCSocialBrain::GetRevengeTarget() const
{
	for (const auto& Pair : Relationships)
	{
		if (Pair.Value.bWantsRevenge)
		{
			return Pair.Key;
		}
	}
	return FGuid();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

FGuid UAoCNPCSocialBrain::GetActorGUID(AActor* Actor) const
{
	if (!Actor) return FGuid();

	AoCHumanoidNPCV2* NPC = Cast<AoCHumanoidNPCV2>(Actor);
	if (NPC)
	{
		return NPC->GetUniqueID();
	}

	// For non-NPC actors (players), generate a stable GUID from their unique ID
	uint32 ID = Actor->GetUniqueID();
	return FGuid(ID, 0, 0, 0);
}

FString UAoCNPCSocialBrain::GetActorDisplayName(AActor* Actor) const
{
	if (!Actor) return TEXT("Unknown");

	AoCHumanoidNPCV2* NPC = Cast<AoCHumanoidNPCV2>(Actor);
	if (NPC)
	{
		return NPC->GetDisplayName();
	}

	return Actor->GetName();
}
