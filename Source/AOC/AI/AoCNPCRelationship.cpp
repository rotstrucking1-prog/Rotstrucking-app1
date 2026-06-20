// AoCNPCRelationship.cpp
// Architect of Creation — NPC Relationship, Trust, Party, and Grudge Implementation
//
// Implements deep relationship tracking, party fairness, betrayal logic, grudges,
// gossip propagation, and promise fulfillment tracking.

#include "AoCNPCCharacter.h"
#include "AoCNPCRelationship.h"
#include "AoCHumanoidNPCV2.h"
#include "AoCNPCBrainV2.h"
#include "AoCNPCSpeech.h"
#include "AoCNPCSkillSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogRelationship, Log, All);

// ============================================================================
// CONSTRUCTOR / LIFECYCLE
// ============================================================================

UAoCNPCRelationship::UAoCNPCRelationship()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f; // 1Hz — relationships don't need fast updates
}

void UAoCNPCRelationship::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AAoCNPCCharacter>(GetOwner());
	if (OwnerNPC)
	{
		Brain = OwnerNPC->FindComponentByClass<UAoCNPCBrainV2>();
	}

	UE_LOG(LogRelationship, Log, TEXT("[%s] Relationship system initialized"),
		OwnerNPC ? *OwnerNPC->GetName() : TEXT("Unknown"));
}

void UAoCNPCRelationship::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickEmotionDecay(DeltaTime);
	TickPartyFairness(DeltaTime);
	TickPromiseDeadlines(DeltaTime);
	TickProximityTracking(DeltaTime);
}

// ============================================================================
// RELATIONSHIP MANAGEMENT
// ============================================================================

void UAoCNPCRelationship::RecordInteraction(const FString& EntityId, const FString& EntityName, EInteractionType Interaction)
{
	FRelationshipRecord& Record = GetOrCreateRelationship(EntityId, EntityName);

	float TrustDelta = GetInteractionTrustDelta(Interaction);
	ERelationshipLevel OldLevel = Record.Level;

	// Apply trust change
	Record.TrustScore = FMath::Clamp(Record.TrustScore + TrustDelta, -100.0f, 100.0f);
	Record.LastInteractionTime = GetWorld()->GetTimeSeconds();
	Record.TotalEncounters++;

	// Update interaction counters
	switch (Interaction)
	{
	case EInteractionType::FoughtAlongside:
		Record.FightsAlongside++;
		Record.Gratitude = FMath::Min(Record.Gratitude + 5.0f, 100.0f);
		break;
	case EInteractionType::FoughtAgainst:
		Record.FightsAgainst++;
		Record.Resentment = FMath::Min(Record.Resentment + 15.0f, 100.0f);
		break;
	case EInteractionType::SharedLoot:
		Record.LootSharedWithMe++;
		Record.Gratitude = FMath::Min(Record.Gratitude + 10.0f, 100.0f);
		break;
	case EInteractionType::StoleLoot:
		Record.LootTakenFromMe++;
		Record.Resentment = FMath::Min(Record.Resentment + 20.0f, 100.0f);
		break;
	case EInteractionType::BetrayedAlliance:
		Record.TimesBetrayed++;
		Record.Resentment = 100.0f; // Max resentment immediately
		Record.Fear = FMath::Min(Record.Fear + 30.0f, 100.0f);
		// Form a grudge for betrayals
		FormGrudge(EntityId, EntityName, TEXT("Betrayed our alliance"), 90.0f);
		break;
	case EInteractionType::KilledMe:
		Record.FightsAgainst++;
		Record.Resentment = FMath::Min(Record.Resentment + 40.0f, 100.0f);
		Record.Fear = FMath::Min(Record.Fear + 40.0f, 100.0f);
		FormGrudge(EntityId, EntityName, TEXT("Killed me"), 80.0f);
		break;
	case EInteractionType::HealedMe:
		Record.TimesHelped++;
		Record.Gratitude = FMath::Min(Record.Gratitude + 15.0f, 100.0f);
		break;
	case EInteractionType::BuffedMe:
		Record.TimesHelped++;
		Record.Gratitude = FMath::Min(Record.Gratitude + 5.0f, 100.0f);
		break;
	case EInteractionType::SavedMyLife:
		Record.TimesHelped++;
		Record.Gratitude = FMath::Min(Record.Gratitude + 30.0f, 100.0f);
		Record.Admiration = FMath::Min(Record.Admiration + 20.0f, 100.0f);
		break;
	case EInteractionType::KeptPromise:
		Record.PromisesKept++;
		break;
	case EInteractionType::BrokePromise:
		Record.PromisesBroken++;
		Record.Resentment = FMath::Min(Record.Resentment + 15.0f, 100.0f);
		break;
	case EInteractionType::IgnoredMyNeed:
		Record.TimesIgnoredMyNeed++;
		break;
	case EInteractionType::AttackedUnprovoked:
		Record.FightsAgainst++;
		Record.Resentment = FMath::Min(Record.Resentment + 30.0f, 100.0f);
		Record.Fear = FMath::Min(Record.Fear + 15.0f, 100.0f);
		FormGrudge(EntityId, EntityName, TEXT("Attacked me without provocation"), 60.0f);
		break;
	case EInteractionType::OfferedHelp:
		Record.TimesHelped++;
		break;
	case EInteractionType::GaveGift:
		Record.LootSharedWithMe++;
		Record.Gratitude = FMath::Min(Record.Gratitude + 12.0f, 100.0f);
		break;
	case EInteractionType::InsultedMe:
		Record.Resentment = FMath::Min(Record.Resentment + 5.0f, 100.0f);
		break;
	case EInteractionType::DamagedMyProperty:
		Record.Resentment = FMath::Min(Record.Resentment + 15.0f, 100.0f);
		break;
	default:
		break;
	}

	// Recalculate level
	Record.Level = ComputeRelationshipLevel(Record.TrustScore);

	// Fire event if level changed
	if (Record.Level != OldLevel)
	{
		UE_LOG(LogRelationship, Log, TEXT("[%s] Relationship with '%s' changed: %d → %d (Trust: %.1f)"),
			*GetOwner()->GetName(), *EntityName, (int32)OldLevel, (int32)Record.Level, Record.TrustScore);

		OnRelationshipChanged.Broadcast(EntityId, OldLevel, Record.Level);
	}
}

FRelationshipRecord* UAoCNPCRelationship::GetRelationship(const FString& EntityId)
{
	return Relationships.Find(EntityId);
}

ERelationshipLevel UAoCNPCRelationship::GetRelationshipLevel(const FString& EntityId) const
{
	const FRelationshipRecord* Record = Relationships.Find(EntityId);
	return Record ? Record->Level : ERelationshipLevel::Stranger;
}

float UAoCNPCRelationship::GetTrustScore(const FString& EntityId) const
{
	const FRelationshipRecord* Record = Relationships.Find(EntityId);
	return Record ? Record->TrustScore : 0.0f;
}

void UAoCNPCRelationship::ModifyTrust(const FString& EntityId, const FString& EntityName, float Delta)
{
	FRelationshipRecord& Record = GetOrCreateRelationship(EntityId, EntityName);

	ERelationshipLevel OldLevel = Record.Level;
	Record.TrustScore = FMath::Clamp(Record.TrustScore + Delta, -100.0f, 100.0f);
	Record.Level = ComputeRelationshipLevel(Record.TrustScore);
	Record.LastInteractionTime = GetWorld()->GetTimeSeconds();

	if (Record.Level != OldLevel)
	{
		OnRelationshipChanged.Broadcast(EntityId, OldLevel, Record.Level);
	}
}

FString UAoCNPCRelationship::GetMostTrustedEntity() const
{
	FString BestId;
	float BestTrust = -200.0f;

	for (const auto& Pair : Relationships)
	{
		if (Pair.Value.TrustScore > BestTrust)
		{
			BestTrust = Pair.Value.TrustScore;
			BestId = Pair.Key;
		}
	}

	return BestId;
}

FString UAoCNPCRelationship::GetMostHostileEntity() const
{
	FString WorstId;
	float WorstTrust = 200.0f;

	for (const auto& Pair : Relationships)
	{
		if (Pair.Value.TrustScore < WorstTrust)
		{
			WorstTrust = Pair.Value.TrustScore;
			WorstId = Pair.Key;
		}
	}

	return WorstId;
}

// ============================================================================
// PARTY SYSTEM
// ============================================================================

bool UAoCNPCRelationship::EvaluatePartyInvite(const FString& InviterId, EPartyGoal Goal, ELootRule LootRule)
{
	// Already in a party? Decline.
	if (CurrentParty.bIsActive)
	{
		UE_LOG(LogRelationship, Verbose, TEXT("[%s] Declining party invite from '%s' — already in party"),
			*GetOwner()->GetName(), *InviterId);
		return false;
	}

	float Trust = GetTrustScore(InviterId);
	float Sociability = GetPersonalityTrait(TEXT("Sociability"));
	float Greed = GetPersonalityTrait(TEXT("Greed"));

	// Base willingness based on trust
	float Willingness = 0.0f;

	// Strangers need higher trust than acquaintances
	ERelationshipLevel Level = GetRelationshipLevel(InviterId);

	switch (Level)
	{
	case ERelationshipLevel::Hostile:
	case ERelationshipLevel::Unfriendly:
		return false; // Never party with someone we dislike

	case ERelationshipLevel::Stranger:
		Willingness = (Trust - 10.0f) / 30.0f; // Need trust > 10, scales up
		break;

	case ERelationshipLevel::Acquaintance:
		Willingness = 0.4f + (Trust - 20.0f) / 40.0f;
		break;

	case ERelationshipLevel::Friendly:
	case ERelationshipLevel::Trusted:
	case ERelationshipLevel::BondedAlly:
		Willingness = 0.7f + (Trust - 40.0f) / 200.0f;
		break;
	}

	// Personality modifiers
	Willingness += (Sociability - 0.5f) * 0.4f; // Social NPCs more willing
	Willingness -= (Greed - 0.5f) * 0.2f; // Greedy NPCs slightly less willing (don't want to share)

	// Goal compatibility — are they going where I want to go?
	// TODO: Check if the party goal aligns with NPC's current needs/goals
	// For now, a small bonus for aligned goals
	Willingness += 0.1f; // Placeholder alignment bonus

	// Loot rule preference
	if (LootRule == ELootRule::FreeForAll && Greed < 0.5f)
	{
		Willingness -= 0.15f; // Non-greedy NPCs don't like FFA
	}
	if (LootRule == ELootRule::EqualSplit || LootRule == ELootRule::NeedBeforeGreed)
	{
		Willingness += 0.1f; // Most NPCs prefer fair loot rules
	}

	// Check for grudges against the inviter
	if (HasGrudgeAgainst(InviterId))
	{
		Willingness -= 0.5f; // Big penalty for grudges
	}

	bool bAccept = (Willingness > 0.5f) || (Willingness > 0.3f && FMath::FRand() < 0.3f);

	UE_LOG(LogRelationship, Log, TEXT("[%s] Party invite from '%s': Trust=%.1f, Sociability=%.2f, Willingness=%.2f → %s"),
		*GetOwner()->GetName(), *InviterId, Trust, Sociability, Willingness,
		bAccept ? TEXT("ACCEPT") : TEXT("DECLINE"));

	return bAccept;
}

FString UAoCNPCRelationship::CreateParty(EPartyGoal Goal, ELootRule LootRule)
{
	if (CurrentParty.bIsActive)
	{
		UE_LOG(LogRelationship, Warning, TEXT("[%s] Cannot create party — already in one"), *GetOwner()->GetName());
		return FString();
	}

	FString OwnerId = GetOwner()->GetName(); // Use actor name as ID (simplified)

	CurrentParty = FNPCParty();
	CurrentParty.PartyId = FString::Printf(TEXT("Party_%s_%d"), *OwnerId, FMath::RandRange(1000, 9999));
	CurrentParty.LeaderId = OwnerId;
	CurrentParty.MemberIds.Add(OwnerId);
	CurrentParty.CurrentGoal = Goal;
	CurrentParty.LootRule = LootRule;
	CurrentParty.FormedAtTime = GetWorld()->GetTimeSeconds();
	CurrentParty.bIsActive = true;

	// Add self to member data
	FPartyMemberData SelfData;
	SelfData.MemberId = OwnerId;
	SelfData.MemberName = OwnerId;
	CurrentParty.MemberData.Add(SelfData);

	OnPartyFormed.Broadcast(CurrentParty.PartyId);

	UE_LOG(LogRelationship, Log, TEXT("[%s] Created party '%s' (Goal: %d, Loot: %d)"),
		*GetOwner()->GetName(), *CurrentParty.PartyId, (int32)Goal, (int32)LootRule);

	return CurrentParty.PartyId;
}

bool UAoCNPCRelationship::JoinParty(const FString& PartyId)
{
	if (CurrentParty.bIsActive)
	{
		return false;
	}

	// In a real implementation, this would look up the party from a global party manager.
	// For now, we just set up the local tracking.
	FString OwnerId = GetOwner()->GetName();

	CurrentParty.PartyId = PartyId;
	CurrentParty.bIsActive = true;

	if (!CurrentParty.MemberIds.Contains(OwnerId))
	{
		CurrentParty.MemberIds.Add(OwnerId);

		FPartyMemberData SelfData;
		SelfData.MemberId = OwnerId;
		SelfData.MemberName = OwnerId;
		CurrentParty.MemberData.Add(SelfData);
	}

	UE_LOG(LogRelationship, Log, TEXT("[%s] Joined party '%s'"), *GetOwner()->GetName(), *PartyId);
	return true;
}

void UAoCNPCRelationship::LeaveParty(const FString& Reason)
{
	if (!CurrentParty.bIsActive) return;

	FString PartyId = CurrentParty.PartyId;

	UE_LOG(LogRelationship, Log, TEXT("[%s] Leaving party '%s': %s"),
		*GetOwner()->GetName(), *PartyId, *Reason);

	CurrentParty.bIsActive = false;
	CurrentParty.MemberIds.Empty();
	CurrentParty.MemberData.Empty();

	OnPartyDisbanded.Broadcast(PartyId, Reason);
}

void UAoCNPCRelationship::RecordPartyLoot(const FString& RecipientId, int32 ItemValue, int32 GoldAmount)
{
	if (!CurrentParty.bIsActive) return;

	CurrentParty.TotalLootDropped += ItemValue;
	CurrentParty.TotalGoldDropped += GoldAmount;

	FPartyMemberData* MemberData = FindPartyMember(RecipientId);
	if (MemberData)
	{
		MemberData->LootReceived += ItemValue;
		MemberData->GoldReceived += GoldAmount;
	}

	RecalculatePartyFairness();
}

void UAoCNPCRelationship::RecordPartyContribution(const FString& MemberId, float Damage, float Healing, int32 Resources)
{
	if (!CurrentParty.bIsActive) return;

	FPartyMemberData* MemberData = FindPartyMember(MemberId);
	if (MemberData)
	{
		MemberData->DamageDealt += Damage;
		MemberData->HealingDone += Healing;
		MemberData->ResourcesGathered += Resources;
	}

	RecalculatePartyFairness();
}

float UAoCNPCRelationship::GetPerceivedFairness() const
{
	if (!CurrentParty.bIsActive) return 0.0f;

	FString OwnerId = GetOwner()->GetName();

	for (const FPartyMemberData& Member : CurrentParty.MemberData)
	{
		if (Member.MemberId == OwnerId)
		{
			// Fairness = my loot% - my contribution%
			// Positive = I got more than I deserved (happy)
			// Negative = I got less (unhappy)
			return Member.PerceivedLootPercent - Member.PerceivedContributionPercent;
		}
	}

	return 0.0f;
}

bool UAoCNPCRelationship::WantsToInvite(const FString& EntityId) const
{
	float Sociability = GetPersonalityTrait(TEXT("Sociability"));
	float Trust = GetTrustScore(EntityId);

	// Social NPCs with friendly relationships want to team up
	return (Sociability > 0.5f && Trust > 30.0f && !HasGrudgeAgainst(EntityId));
}

// ============================================================================
// BETRAYAL
// ============================================================================

EBetrayalReadiness UAoCNPCRelationship::EvaluateBetrayalReadiness() const
{
	if (!CurrentParty.bIsActive) return EBetrayalReadiness::NeverBetray;

	float Loyalty = GetPersonalityTrait(TEXT("Loyalty"));
	float Greed = GetPersonalityTrait(TEXT("Greed"));
	float Aggression = GetPersonalityTrait(TEXT("Aggression"));

	// Loyal NPCs NEVER betray
	if (Loyalty > 0.8f)
	{
		return EBetrayalReadiness::NeverBetray;
	}

	float Fairness = GetPerceivedFairness();

	// If fairness is positive or barely negative, no betrayal
	if (Fairness > -FairnessMildThreshold)
	{
		return EBetrayalReadiness::NotWorthIt;
	}

	// Calculate betrayal score based on unfairness, greed, aggression, low loyalty
	float BetrayalScore = 0.0f;
	BetrayalScore += FMath::Abs(Fairness) * 0.01f; // Unfairness contributes
	BetrayalScore += Greed * 0.3f;
	BetrayalScore += Aggression * 0.2f;
	BetrayalScore -= Loyalty * 0.4f;

	// Check if we can actually win the fight
	// (simplified — in reality would check party members' gear/level)
	bool bCanWin = true; // Placeholder
	if (!bCanWin)
	{
		return EBetrayalReadiness::NotWorthIt;
	}

	// Witnesses reduce willingness (reputation damage)
	int32 Witnesses = CountWitnesses();
	BetrayalScore -= Witnesses * 0.1f;

	// Stakes — how much loot is involved?
	if (CurrentParty.TotalLootDropped < 3)
	{
		BetrayalScore -= 0.3f; // Not enough at stake
	}

	if (BetrayalScore < 0.3f)
		return EBetrayalReadiness::NotWorthIt;
	if (BetrayalScore < 0.5f)
		return EBetrayalReadiness::Considering;
	if (BetrayalScore < 0.7f && Fairness > -FairnessAngerThreshold)
		return EBetrayalReadiness::Considering;

	return EBetrayalReadiness::Ready;
}

void UAoCNPCRelationship::ExecuteBetrayal(const FString& TargetId)
{
	UE_LOG(LogRelationship, Warning, TEXT("[%s] EXECUTING BETRAYAL against '%s'!"),
		*GetOwner()->GetName(), *TargetId);

	FString OwnerId = GetOwner()->GetName();

	// Drop trust to hostile
	ModifyTrust(TargetId, TargetId, -80.0f);

	// Record the betrayal interaction for all party members
	for (const FString& MemberId : CurrentParty.MemberIds)
	{
		if (MemberId != OwnerId)
		{
			RecordInteraction(MemberId, MemberId, EInteractionType::BetrayedAlliance);
		}
	}

	// Notify witnesses — nearby NPCs see this betrayal
	// (handled by the social brain when processing the event)
	OnBetrayalTriggered.Broadcast(OwnerId, TargetId);

	// Leave party
	LeaveParty(TEXT("Betrayal"));

	// TODO: Trigger combat against the target via CombatBrain
}

bool UAoCNPCRelationship::CanWinFightAgainst(const FString& TargetId) const
{
	// Simplified power comparison. In full implementation, this would compare:
	// - Gear score, HP, skill levels, active buffs
	// For now, use a heuristic based on relationship data

	const FRelationshipRecord* Record = Relationships.Find(TargetId);
	if (!Record) return false; // Unknown target = risky

	// If they've killed us before, we're probably weaker
	if (Record->Fear > 50.0f) return false;

	// If we've fought alongside them and they're strong, risky
	if (Record->Admiration > 60.0f) return false;

	// If we've beaten them before, probably can again
	if (Record->FightsAgainst > Record->FightsAlongside && Record->Fear < 30.0f)
	{
		return true;
	}

	// Default: 50/50 with personality modifier
	float Confidence = 0.5f + GetPersonalityTrait(TEXT("Aggression")) * 0.3f;
	return FMath::FRand() < Confidence;
}

int32 UAoCNPCRelationship::CountWitnesses() const
{
	if (!OwnerNPC || !GetWorld()) return 0;

	int32 Count = 0;
	FVector MyLoc = OwnerNPC->GetActorLocation();

	// Check for nearby NPCs and players
	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAoCNPCCharacter::StaticClass(), NearbyActors);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == GetOwner()) continue;
		if (FVector::Dist(MyLoc, Actor->GetActorLocation()) <= WitnessSearchRadius)
		{
			Count++;
		}
	}

	// Also count players
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			if (FVector::Dist(MyLoc, PC->GetPawn()->GetActorLocation()) <= WitnessSearchRadius)
			{
				Count++;
			}
		}
	}

	return Count;
}

// ============================================================================
// GRUDGES
// ============================================================================

void UAoCNPCRelationship::FormGrudge(const FString& AggressorId, const FString& AggressorName, const FString& Grievance, float Intensity)
{
	// Check if we already have a grudge against this entity
	for (FGrudge& Existing : Grudges)
	{
		if (Existing.AggressorId == AggressorId && !Existing.bRevenged)
		{
			// Intensify existing grudge
			Existing.Intensity = FMath::Min(Existing.Intensity + Intensity * 0.5f, 100.0f);
			Existing.Grievance = Grievance; // Update with latest grievance
			UE_LOG(LogRelationship, Log, TEXT("[%s] Grudge INTENSIFIED against '%s': %.1f (%s)"),
				*GetOwner()->GetName(), *AggressorName, Existing.Intensity, *Grievance);
			return;
		}
	}

	// Prune if at max
	if (Grudges.Num() >= MaxGrudges)
	{
		// Remove weakest grudge
		int32 WeakestIdx = 0;
		float WeakestIntensity = 200.0f;
		for (int32 i = 0; i < Grudges.Num(); i++)
		{
			if (Grudges[i].Intensity < WeakestIntensity)
			{
				WeakestIntensity = Grudges[i].Intensity;
				WeakestIdx = i;
			}
		}
		Grudges.RemoveAt(WeakestIdx);
	}

	FGrudge NewGrudge;
	NewGrudge.AggressorId = AggressorId;
	NewGrudge.AggressorName = AggressorName;
	NewGrudge.Grievance = Grievance;
	NewGrudge.Intensity = FMath::Clamp(Intensity, 0.0f, 100.0f);
	NewGrudge.OccurredAtTime = GetWorld()->GetTimeSeconds();
	NewGrudge.bRevenged = false;
	NewGrudge.TimesWarnedOthers = 0;

	Grudges.Add(NewGrudge);

	OnGrudgeFormed.Broadcast(AggressorId, Grievance);

	UE_LOG(LogRelationship, Log, TEXT("[%s] NEW GRUDGE against '%s': %.1f intensity (%s)"),
		*GetOwner()->GetName(), *AggressorName, Intensity, *Grievance);
}

bool UAoCNPCRelationship::HasGrudgeAgainst(const FString& EntityId) const
{
	for (const FGrudge& G : Grudges)
	{
		if (G.AggressorId == EntityId && G.Intensity > 5.0f && !G.bRevenged)
		{
			return true;
		}
	}
	return false;
}

float UAoCNPCRelationship::GetGrudgeIntensity(const FString& EntityId) const
{
	float MaxIntensity = 0.0f;
	for (const FGrudge& G : Grudges)
	{
		if (G.AggressorId == EntityId && !G.bRevenged)
		{
			MaxIntensity = FMath::Max(MaxIntensity, G.Intensity);
		}
	}
	return MaxIntensity;
}

bool UAoCNPCRelationship::ShouldSeekRevenge(const FString& EntityId) const
{
	float GrudgeInt = GetGrudgeIntensity(EntityId);
	if (GrudgeInt < 30.0f) return false;

	float Aggression = GetPersonalityTrait(TEXT("Aggression"));

	// Fear check — if we're too afraid, we avoid, don't seek revenge
	const FRelationshipRecord* Record = Relationships.Find(EntityId);
	if (Record && Record->Fear > 60.0f && Aggression < 0.7f)
	{
		return false;
	}

	// Aggressive NPCs with strong grudges seek revenge
	float RevengeDesire = (GrudgeInt / 100.0f) * (0.5f + Aggression * 0.5f);
	return RevengeDesire > 0.5f;
}

void UAoCNPCRelationship::MarkGrudgeRevenged(const FString& AggressorId)
{
	for (FGrudge& G : Grudges)
	{
		if (G.AggressorId == AggressorId && !G.bRevenged)
		{
			G.bRevenged = true;
			UE_LOG(LogRelationship, Log, TEXT("[%s] Grudge against '%s' AVENGED"),
				*GetOwner()->GetName(), *G.AggressorName);
		}
	}
}

// ============================================================================
// GOSSIP / REPUTATION
// ============================================================================

void UAoCNPCRelationship::ReceiveGossip(const FReputationGossip& Gossip)
{
	ReceivedGossip.Add(Gossip);

	// How much do I trust the source?
	float SourceTrust = GetTrustScore(Gossip.SourceId);
	float Credibility = FMath::Clamp((SourceTrust + 50.0f) / 100.0f, 0.1f, 1.0f); // Scale 0.1-1.0

	// Apply a fraction of the gossip's trust modifier based on credibility
	float ActualModifier = Gossip.TrustModifier * Credibility * 0.5f; // 50% max influence

	if (FMath::Abs(ActualModifier) > 0.5f)
	{
		ModifyTrust(Gossip.SubjectId, Gossip.SubjectName, ActualModifier);

		UE_LOG(LogRelationship, Log, TEXT("[%s] Received gossip about '%s' from '%s': '%s' (modifier: %.1f, credibility: %.2f)"),
			*GetOwner()->GetName(), *Gossip.SubjectName, *Gossip.SourceId,
			*Gossip.GossipContent, ActualModifier, Credibility);
	}
}

FReputationGossip UAoCNPCRelationship::GenerateGossipAbout(const FString& EntityId) const
{
	FReputationGossip Gossip;
	Gossip.SubjectId = EntityId;
	Gossip.SourceId = GetOwner()->GetName();
	Gossip.HeardAtTime = GetWorld()->GetTimeSeconds();

	const FRelationshipRecord* Record = Relationships.Find(EntityId);
	if (!Record)
	{
		Gossip.GossipContent = TEXT("Don't know much about them");
		Gossip.TrustModifier = 0.0f;
		return Gossip;
	}

	Gossip.SubjectName = Record->EntityName;

	// Generate gossip based on our experience
	if (Record->TrustScore > 60.0f)
	{
		Gossip.GossipContent = FString::Printf(TEXT("%s is solid, helped me out a bunch"), *Record->EntityName);
		Gossip.TrustModifier = 15.0f;
	}
	else if (Record->TrustScore > 30.0f)
	{
		Gossip.GossipContent = FString::Printf(TEXT("%s seems alright"), *Record->EntityName);
		Gossip.TrustModifier = 5.0f;
	}
	else if (Record->TrustScore < -60.0f)
	{
		Gossip.GossipContent = FString::Printf(TEXT("Watch out for %s, they're dangerous"), *Record->EntityName);
		Gossip.TrustModifier = -25.0f;
	}
	else if (Record->TrustScore < -30.0f)
	{
		Gossip.GossipContent = FString::Printf(TEXT("I don't trust %s"), *Record->EntityName);
		Gossip.TrustModifier = -10.0f;
	}
	else if (Record->TimesBetrayed > 0)
	{
		Gossip.GossipContent = FString::Printf(TEXT("%s betrayed me, don't party with them"), *Record->EntityName);
		Gossip.TrustModifier = -30.0f;
	}
	else
	{
		Gossip.GossipContent = FString::Printf(TEXT("Seen %s around, nothing special"), *Record->EntityName);
		Gossip.TrustModifier = 0.0f;
	}

	// Source credibility based on how many encounters we've had (more data = more credible)
	Gossip.SourceCredibility = FMath::Clamp((float)Record->TotalEncounters / 10.0f, 0.2f, 1.0f);

	return Gossip;
}

void UAoCNPCRelationship::SpreadWarning(const FString& AggressorId)
{
	if (!OwnerNPC || !GetWorld()) return;

	// Find nearby friendly NPCs and share grudge info
	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAoCNPCCharacter::StaticClass(), NearbyActors);

	FVector MyLoc = OwnerNPC->GetActorLocation();
	FReputationGossip Gossip = GenerateGossipAbout(AggressorId);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == GetOwner()) continue;
		if (FVector::Dist(MyLoc, Actor->GetActorLocation()) > WitnessSearchRadius) continue;

		// Only warn friendly NPCs
		FString OtherId = Actor->GetName();
		if (GetTrustScore(OtherId) > 20.0f)
		{
			AAoCNPCCharacter* OtherNPC = Cast<AAoCNPCCharacter>(Actor);
			if (OtherNPC)
			{
				UAoCNPCRelationship* OtherRel = OtherNPC->FindComponentByClass<UAoCNPCRelationship>();
				if (OtherRel)
				{
					OtherRel->ReceiveGossip(Gossip);
				}
			}
		}
	}

	// Update grudge warning count
	for (FGrudge& G : Grudges)
	{
		if (G.AggressorId == AggressorId)
		{
			G.TimesWarnedOthers++;
		}
	}
}

// ============================================================================
// PROMISES
// ============================================================================

void UAoCNPCRelationship::MakePromise(const FString& ToEntityId, EPromiseType Type, const FString& Description, float Deadline)
{
	FRelationshipRecord& Record = GetOrCreateRelationship(ToEntityId, ToEntityId);

	FPromise Promise;
	Promise.Type = Type;
	Promise.PromiserId = GetOwner()->GetName();
	Promise.PromiseeId = ToEntityId;
	Promise.Description = Description;
	Promise.MadeAtTime = GetWorld()->GetTimeSeconds();
	Promise.DeadlineTime = (Deadline > 0.0f) ? (GetWorld()->GetTimeSeconds() + Deadline) : 0.0f;

	Record.ActivePromises.Add(Promise);

	UE_LOG(LogRelationship, Log, TEXT("[%s] Made promise to '%s': %s"),
		*GetOwner()->GetName(), *ToEntityId, *Description);
}

void UAoCNPCRelationship::FulfillPromise(const FString& ToEntityId, EPromiseType Type)
{
	FRelationshipRecord* Record = GetRelationship(ToEntityId);
	if (!Record) return;

	for (int32 i = Record->ActivePromises.Num() - 1; i >= 0; i--)
	{
		if (Record->ActivePromises[i].Type == Type && !Record->ActivePromises[i].bFulfilled)
		{
			Record->ActivePromises[i].bFulfilled = true;
			Record->PromisesKept++;

			RecordInteraction(ToEntityId, Record->EntityName, EInteractionType::KeptPromise);

			UE_LOG(LogRelationship, Log, TEXT("[%s] Promise FULFILLED to '%s': %s"),
				*GetOwner()->GetName(), *ToEntityId, *Record->ActivePromises[i].Description);

			Record->ActivePromises.RemoveAt(i);
			return;
		}
	}
}

void UAoCNPCRelationship::BreakPromise(const FString& ToEntityId, EPromiseType Type)
{
	FRelationshipRecord* Record = GetRelationship(ToEntityId);
	if (!Record) return;

	for (int32 i = Record->ActivePromises.Num() - 1; i >= 0; i--)
	{
		if (Record->ActivePromises[i].Type == Type && !Record->ActivePromises[i].bFulfilled && !Record->ActivePromises[i].bBroken)
		{
			Record->ActivePromises[i].bBroken = true;
			Record->PromisesBroken++;

			RecordInteraction(ToEntityId, Record->EntityName, EInteractionType::BrokePromise);

			UE_LOG(LogRelationship, Log, TEXT("[%s] Promise BROKEN to '%s': %s"),
				*GetOwner()->GetName(), *ToEntityId, *Record->ActivePromises[i].Description);

			Record->ActivePromises.RemoveAt(i);
			return;
		}
	}
}

void UAoCNPCRelationship::CheckPromiseDeadlines()
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	for (auto& Pair : Relationships)
	{
		FRelationshipRecord& Record = Pair.Value;

		for (int32 i = Record.ActivePromises.Num() - 1; i >= 0; i--)
		{
			FPromise& P = Record.ActivePromises[i];
			if (P.DeadlineTime > 0.0f && CurrentTime > P.DeadlineTime && !P.bFulfilled && !P.bBroken)
			{
				// Promise expired — treat as broken
				BreakPromise(Pair.Key, P.Type);
			}
		}
	}
}

// ============================================================================
// DECISION HELPERS
// ============================================================================

bool UAoCNPCRelationship::ShouldHelpInFight(const FString& EntityId) const
{
	float Trust = GetTrustScore(EntityId);
	float Loyalty = GetPersonalityTrait(TEXT("Loyalty"));

	// BondedAllies always help
	if (Trust >= 80.0f) return true;

	// Trusted friends usually help
	if (Trust >= 60.0f) return (FMath::FRand() < (0.7f + Loyalty * 0.3f));

	// Friendly NPCs might help
	if (Trust >= 40.0f) return (FMath::FRand() < (0.3f + Loyalty * 0.3f));

	// In a party together = help
	if (CurrentParty.bIsActive && CurrentParty.MemberIds.Contains(EntityId))
	{
		return true;
	}

	return false;
}

bool UAoCNPCRelationship::ShouldShareLoot(const FString& EntityId) const
{
	float Trust = GetTrustScore(EntityId);
	float Greed = GetPersonalityTrait(TEXT("Greed"));

	// High trust + low greed = share
	if (Trust >= 60.0f && Greed < 0.6f) return true;

	// In a party with sharing rules = share
	if (CurrentParty.bIsActive && CurrentParty.MemberIds.Contains(EntityId))
	{
		if (CurrentParty.LootRule == ELootRule::EqualSplit || CurrentParty.LootRule == ELootRule::NeedBeforeGreed)
		{
			return true;
		}
	}

	return (Trust >= 80.0f); // BondedAlly always shares
}

bool UAoCNPCRelationship::ShouldWarnOfDanger(const FString& EntityId) const
{
	float Trust = GetTrustScore(EntityId);
	return Trust >= 20.0f; // Warn anyone we're at least acquainted with
}

bool UAoCNPCRelationship::ShouldFleeFrom(const FString& EntityId) const
{
	const FRelationshipRecord* Record = Relationships.Find(EntityId);
	if (!Record) return false;

	float Aggression = GetPersonalityTrait(TEXT("Aggression"));

	// High fear + low aggression = flee
	if (Record->Fear > 60.0f && Aggression < 0.4f) return true;

	// Very hostile and they killed us before = flee unless aggressive
	if (Record->TrustScore < -60.0f && Record->Fear > 40.0f && Aggression < 0.6f) return true;

	return false;
}

// ============================================================================
// TICK PHASES
// ============================================================================

void UAoCNPCRelationship::TickEmotionDecay(float DeltaTime)
{
	EmotionDecayTimer += DeltaTime;
	if (EmotionDecayTimer < 60.0f) return; // Check every real-time minute
	EmotionDecayTimer = 0.0f;

	for (auto& Pair : Relationships)
	{
		FRelationshipRecord& R = Pair.Value;

		// Gratitude decays slowly
		R.Gratitude = FMath::Max(0.0f, R.Gratitude - GratitudeDecayPerMinute);

		// Resentment decays VERY slowly
		R.Resentment = FMath::Max(0.0f, R.Resentment - ResentmentDecayPerMinute);

		// Fear decays moderately
		R.Fear = FMath::Max(0.0f, R.Fear - FearDecayPerMinute);

		// Admiration is very stable (barely decays)
		R.Admiration = FMath::Max(0.0f, R.Admiration - 0.005f);
	}

	// Grudge decay
	for (FGrudge& G : Grudges)
	{
		if (!G.bRevenged)
		{
			G.Intensity = FMath::Max(0.0f, G.Intensity - GrudgeDecayPerMinute);
		}
	}

	// Remove dead grudges (intensity near 0)
	for (int32 i = Grudges.Num() - 1; i >= 0; i--)
	{
		if (Grudges[i].Intensity < 1.0f && Grudges[i].bRevenged)
		{
			Grudges.RemoveAt(i);
		}
	}
}

void UAoCNPCRelationship::TickPartyFairness(float DeltaTime)
{
	if (!CurrentParty.bIsActive) return;

	PartyFairnessTimer += DeltaTime;
	if (PartyFairnessTimer < PartyFairnessCheckInterval) return;
	PartyFairnessTimer = 0.0f;

	RecalculatePartyFairness();

	float Fairness = GetPerceivedFairness();

	if (Fairness < -FairnessMildThreshold && Fairness >= -FairnessSeriousThreshold)
	{
		// Mild annoyance — trigger speech
		UE_LOG(LogRelationship, Log, TEXT("[%s] Party fairness MILD complaint (fairness: %.1f%%)"),
			*GetOwner()->GetName(), Fairness);
		// Speech trigger: PartyLootUnfair
	}
	else if (Fairness < -FairnessSeriousThreshold && Fairness >= -FairnessAngerThreshold)
	{
		// Serious complaint
		UE_LOG(LogRelationship, Log, TEXT("[%s] Party fairness SERIOUS complaint (fairness: %.1f%%)"),
			*GetOwner()->GetName(), Fairness);
		// Speech trigger: PartyLootUnfair (more aggressive line)
	}
	else if (Fairness < -FairnessAngerThreshold)
	{
		// Anger — potential betrayal or leave
		UE_LOG(LogRelationship, Warning, TEXT("[%s] Party fairness ANGRY (fairness: %.1f%%)"),
			*GetOwner()->GetName(), Fairness);

		EBetrayalReadiness Readiness = EvaluateBetrayalReadiness();

		if (Readiness == EBetrayalReadiness::Ready)
		{
			// Find the member who got the most loot and betray them
			FString GreediestMember;
			float MaxLootPercent = 0.0f;
			for (const FPartyMemberData& M : CurrentParty.MemberData)
			{
				if (M.MemberId != GetOwner()->GetName() && M.PerceivedLootPercent > MaxLootPercent)
				{
					MaxLootPercent = M.PerceivedLootPercent;
					GreediestMember = M.MemberId;
				}
			}

			if (!GreediestMember.IsEmpty())
			{
				ExecuteBetrayal(GreediestMember);
			}
		}
		else if (Readiness != EBetrayalReadiness::Considering)
		{
			// Just leave
			LeaveParty(TEXT("Unfair loot distribution"));
		}
		// If Considering, stay but keep complaining
	}
}

void UAoCNPCRelationship::TickPromiseDeadlines(float DeltaTime)
{
	PromiseCheckTimer += DeltaTime;
	if (PromiseCheckTimer < 30.0f) return; // Check every 30 seconds
	PromiseCheckTimer = 0.0f;

	CheckPromiseDeadlines();
}

void UAoCNPCRelationship::TickProximityTracking(float DeltaTime)
{
	ProximityTrackTimer += DeltaTime;
	if (ProximityTrackTimer < 10.0f) return; // Every 10 seconds
	ProximityTrackTimer = 0.0f;

	if (!OwnerNPC) return;

	FVector MyLoc = OwnerNPC->GetActorLocation();
	const float ProximityRadius = 2000.0f; // 20 meters

	// Check for known entities nearby and accumulate time-together
	for (auto& Pair : Relationships)
	{
		// TODO: Look up the actual actor by ID and check distance
		// For now this is a placeholder — the real implementation would use
		// a spatial index or the NPC registry to find actors by ID
	}
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

FRelationshipRecord& UAoCNPCRelationship::GetOrCreateRelationship(const FString& EntityId, const FString& EntityName)
{
	FRelationshipRecord* Existing = Relationships.Find(EntityId);
	if (Existing) return *Existing;

	// Prune if at max capacity
	if (Relationships.Num() >= MaxRelationships)
	{
		// Remove the oldest/least important relationship
		FString RemoveKey;
		float OldestTime = TNumericLimits<float>::Max();

		for (const auto& Pair : Relationships)
		{
			// Don't prune hostile relationships (those are important)
			if (Pair.Value.Level == ERelationshipLevel::Hostile) continue;

			if (Pair.Value.LastInteractionTime < OldestTime && FMath::Abs(Pair.Value.TrustScore) < 30.0f)
			{
				OldestTime = Pair.Value.LastInteractionTime;
				RemoveKey = Pair.Key;
			}
		}

		if (!RemoveKey.IsEmpty())
		{
			Relationships.Remove(RemoveKey);
		}
	}

	FRelationshipRecord NewRecord;
	NewRecord.EntityId = EntityId;
	NewRecord.EntityName = EntityName;
	NewRecord.TrustScore = 0.0f;
	NewRecord.Level = ERelationshipLevel::Stranger;
	NewRecord.LastInteractionTime = GetWorld()->GetTimeSeconds();

	// Check if we have gossip about this entity — apply pre-existing reputation
	for (const FReputationGossip& G : ReceivedGossip)
	{
		if (G.SubjectId == EntityId)
		{
			NewRecord.TrustScore += G.TrustModifier * G.SourceCredibility * 0.3f;
		}
	}

	NewRecord.TrustScore = FMath::Clamp(NewRecord.TrustScore, -100.0f, 100.0f);
	NewRecord.Level = ComputeRelationshipLevel(NewRecord.TrustScore);

	return Relationships.Add(EntityId, NewRecord);
}

ERelationshipLevel UAoCNPCRelationship::ComputeRelationshipLevel(float TrustScore) const
{
	if (TrustScore <= -60.0f)	return ERelationshipLevel::Hostile;
	if (TrustScore <= -20.0f)	return ERelationshipLevel::Unfriendly;
	if (TrustScore <= 20.0f)	return ERelationshipLevel::Stranger;
	if (TrustScore <= 40.0f)	return ERelationshipLevel::Acquaintance;
	if (TrustScore <= 60.0f)	return ERelationshipLevel::Friendly;
	if (TrustScore <= 80.0f)	return ERelationshipLevel::Trusted;
	return ERelationshipLevel::BondedAlly;
}

float UAoCNPCRelationship::GetInteractionTrustDelta(EInteractionType Interaction) const
{
	switch (Interaction)
	{
	case EInteractionType::FoughtAlongside:			return 5.0f;
	case EInteractionType::SharedLoot:				return 8.0f;
	case EInteractionType::HealedMe:				return 10.0f;
	case EInteractionType::BuffedMe:				return 4.0f;
	case EInteractionType::SavedMyLife:				return 20.0f;
	case EInteractionType::KeptPromise:				return 12.0f;
	case EInteractionType::OfferedHelp:				return 6.0f;
	case EInteractionType::GaveGift:				return 8.0f;
	case EInteractionType::CompletedQuestTogether:	return 15.0f;
	case EInteractionType::TradedFairly:			return 3.0f;

	case EInteractionType::FoughtAgainst:			return -15.0f;
	case EInteractionType::StoleLoot:				return -20.0f;
	case EInteractionType::BrokePromise:			return -25.0f;
	case EInteractionType::KilledMe:				return -40.0f;
	case EInteractionType::AttackedUnprovoked:		return -30.0f;
	case EInteractionType::IgnoredMyNeed:			return -8.0f;
	case EInteractionType::InsultedMe:				return -5.0f;
	case EInteractionType::DamagedMyProperty:		return -15.0f;
	case EInteractionType::BetrayedAlliance:		return -50.0f;
	case EInteractionType::TradedUnfairly:			return -10.0f;

	case EInteractionType::PassedBy:				return 0.5f;
	case EInteractionType::Spoke:					return 1.0f;
	case EInteractionType::TradedNeutral:			return 1.0f;

	default: return 0.0f;
	}
}

float UAoCNPCRelationship::GetPersonalityTrait(const FString& TraitName) const
{
	// Query personality from the brain component
	// TODO: Hook into actual personality system from AoCNPCBrainV2
	// For now return a seeded random based on NPC name + trait for consistency

	if (!OwnerNPC) return 0.5f;

	uint32 Hash = GetTypeHash(OwnerNPC->GetName() + TraitName);
	FRandomStream Stream(Hash);
	return Stream.FRandRange(0.1f, 0.9f);
}

void UAoCNPCRelationship::RecalculatePartyFairness()
{
	if (!CurrentParty.bIsActive) return;
	if (CurrentParty.MemberData.Num() == 0) return;

	// Calculate total contribution score
	float TotalContribution = 0.0f;
	int32 TotalLoot = 0;
	int32 TotalGold = 0;

	for (const FPartyMemberData& M : CurrentParty.MemberData)
	{
		TotalContribution += M.DamageDealt + M.HealingDone + (float)M.ResourcesGathered * 10.0f;
		TotalLoot += M.LootReceived;
		TotalGold += M.GoldReceived;
	}

	if (TotalContribution <= 0.0f) TotalContribution = 1.0f; // Avoid div by zero
	int32 TotalLootValue = FMath::Max(TotalLoot + TotalGold, 1);

	// Calculate percentages per member
	for (FPartyMemberData& M : CurrentParty.MemberData)
	{
		float MemberContribution = M.DamageDealt + M.HealingDone + (float)M.ResourcesGathered * 10.0f;
		M.PerceivedContributionPercent = (MemberContribution / TotalContribution) * 100.0f;
		M.PerceivedLootPercent = ((float)(M.LootReceived + M.GoldReceived) / (float)TotalLootValue) * 100.0f;
	}
}

FPartyMemberData* UAoCNPCRelationship::FindPartyMember(const FString& MemberId)
{
	for (FPartyMemberData& M : CurrentParty.MemberData)
	{
		if (M.MemberId == MemberId)
		{
			return &M;
		}
	}
	return nullptr;
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

FString UAoCNPCRelationship::GetDebugString() const
{
	FString Result;

	Result += TEXT("=== Relationships ===\n");
	Result += FString::Printf(TEXT("Known entities: %d | Grudges: %d\n"), Relationships.Num(), Grudges.Num());

	for (const auto& Pair : Relationships)
	{
		const FRelationshipRecord& R = Pair.Value;
		Result += FString::Printf(TEXT("  '%s': Trust=%.1f Level=%d Gratitude=%.1f Resentment=%.1f Fear=%.1f\n"),
			*R.EntityName, R.TrustScore, (int32)R.Level, R.Gratitude, R.Resentment, R.Fear);
	}

	if (CurrentParty.bIsActive)
	{
		Result += FString::Printf(TEXT("Party: '%s' (%d members, Goal: %d, Loot Rule: %d)\n"),
			*CurrentParty.PartyId, CurrentParty.MemberIds.Num(), (int32)CurrentParty.CurrentGoal, (int32)CurrentParty.LootRule);
		Result += FString::Printf(TEXT("  Perceived Fairness: %.1f%%\n"), GetPerceivedFairness());
	}

	for (const FGrudge& G : Grudges)
	{
		if (!G.bRevenged)
		{
			Result += FString::Printf(TEXT("  GRUDGE vs '%s': %.1f intensity (%s)\n"),
				*G.AggressorName, G.Intensity, *G.Grievance);
		}
	}

	return Result;
}

void UAoCNPCRelationship::SetDisposition(AActor* Target, float Value)
{
	// Stub: set disposition toward target
}

void UAoCNPCRelationship::SetRelationship(AActor* Target, const FString& Type, float Value)
{
	// Stub: set named relationship with target
}
