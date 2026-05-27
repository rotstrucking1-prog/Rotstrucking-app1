// AoCNPCLifeBrain.cpp
// NPC Life/Activity Brain — full implementation
// Daily routines, hierarchical goals, boredom, and emergent behavior chains

#include "AoCNPCLifeBrain.h"
#include "AoCNPCRelationship.h"
#include "AoCHumanoidNPCV2.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAoCLife);

// ---------------------------------------------------------------------------
// Constructor / BeginPlay
// ---------------------------------------------------------------------------

UAoCNPCLifeBrain::UAoCNPCLifeBrain()
{
	PrimaryComponentTick.bCanEverTick = false;
	LifeRand.Initialize(FMath::Rand());
}

void UAoCNPCLifeBrain::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AoCHumanoidNPCV2>(GetOwner());
	if (OwnerNPC.IsValid())
	{
		NeedSystem = OwnerNPC->NeedSystem;
		Memory = OwnerNPC->Memory;
		Inventory = OwnerNPC->Inventory;
		PersonalityComp = OwnerNPC->Personality;
		SkillSystem = OwnerNPC->Skills;
		SocialBrain = OwnerNPC->SocialBrain;

		HomeLocation = OwnerNPC->HomeLocation;
		PatrolWaypoints = OwnerNPC->PatrolPath;
	}

	LifeRand.Initialize(GetOwner() ? GetOwner()->GetUniqueID() : FMath::Rand());

	// Initialize tedium map
	for (uint8 i = 0; i <= static_cast<uint8>(ENPCActivity::Resting); ++i)
	{
		TediumMap.Add(static_cast<ENPCActivity>(i), 0.f);
	}

	// Generate initial goals based on archetype
	GenerateGoalsFromArchetype();
}

// ---------------------------------------------------------------------------
// Main Life Tick
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::LifeTick(float DeltaTime)
{
	if (!OwnerNPC.IsValid()) return;

	// Update tedium for current activity
	UpdateTedium(DeltaTime);

	// Check critical needs first
	CheckCriticalNeeds();

	// If we have an immediate goal, handle it
	if (bHasImmediateGoal && !ImmediateGoal.IsComplete())
	{
		// Continue pursuing immediate goal
		TickCurrentActivity(DeltaTime);
		AdvanceGoals(DeltaTime);
		return;
	}
	else if (bHasImmediateGoal && ImmediateGoal.IsComplete())
	{
		bHasImmediateGoal = false;
	}

	// Check if we should switch activities
	if (ShouldSwitchActivity() || CurrentActivity == ENPCActivity::Idle)
	{
		ENPCActivity NextActivity = DecideNextActivity();
		if (NextActivity != CurrentActivity)
		{
			SetActivity(NextActivity);
		}
	}

	// Tick current activity
	TickCurrentActivity(DeltaTime);

	// Advance long-term goals
	AdvanceGoals(DeltaTime);
}

// ---------------------------------------------------------------------------
// Activity Management
// ---------------------------------------------------------------------------

ENPCActivity UAoCNPCLifeBrain::DecideNextActivity()
{
	ETimeOfDay Time = GetTimeOfDay();

	// 1. Check critical needs
	// (Handled by CheckCriticalNeeds — if critical, immediate goal is set)

	// 2. Check time of day constraints
	if (Time == ETimeOfDay::Night && !bIsNocturnal && ShouldSleep())
	{
		return ENPCActivity::Sleeping;
	}

	if (Time == ETimeOfDay::Evening && !bIsNocturnal)
	{
		// Evening: socialize, head home, or wind down
		if (ShouldSocialize() && LifeRand.FRand() > 0.4f)
		{
			return ENPCActivity::Socializing;
		}
		// Head home if far from home
		if (HomeLocation != FVector::ZeroVector && OwnerNPC.IsValid())
		{
			float DistHome = FVector::Dist(OwnerNPC->GetActorLocation(), HomeLocation);
			if (DistHome > 2000.f)
			{
				return ENPCActivity::Traveling;
			}
		}
		return ENPCActivity::Resting;
	}

	if (Time == ETimeOfDay::Dawn)
	{
		// Wake up, eat
		return ENPCActivity::Eating;
	}

	// 3. Check current goal
	if (LongTermGoals.Num() > 0)
	{
		// Find the most active medium goal
		for (FNPCGoal& MedGoal : MediumGoals)
		{
			if (!MedGoal.IsComplete())
			{
				ENPCActivity GoalActivity = GetActivityForGoal(MedGoal);
				if (GoalActivity != ENPCActivity::Idle)
				{
					return GoalActivity;
				}
			}
		}

		// Decompose long-term goal into medium goals if needed
		for (FNPCGoal& LTGoal : LongTermGoals)
		{
			if (!LTGoal.IsComplete() && MediumGoals.Num() == 0)
			{
				TArray<FNPCGoal> NewMedGoals = BreakDownGoal(LTGoal);
				for (const FNPCGoal& MG : NewMedGoals)
				{
					MediumGoals.Add(MG);
				}
			}
		}

		// Try medium goals again
		for (FNPCGoal& MedGoal : MediumGoals)
		{
			if (!MedGoal.IsComplete())
			{
				ENPCActivity GoalActivity = GetActivityForGoal(MedGoal);
				if (GoalActivity != ENPCActivity::Idle)
				{
					return GoalActivity;
				}
			}
		}
	}

	// 4. Check patrol
	if (PatrolWaypoints.Num() > 0)
	{
		return ENPCActivity::Patrolling;
	}

	// 5. Check personality-driven activities
	if (OwnerNPC.IsValid())
	{
		switch (OwnerNPC->ArchetypePreset)
		{
		case EPersonalityArchetype::Guard:
			return ENPCActivity::Guarding;

		case EPersonalityArchetype::Bandit:
			if (bIsNocturnal || Time == ETimeOfDay::Night)
			{
				return ENPCActivity::HuntingPlayers;
			}
			return ENPCActivity::Exploring;

		case EPersonalityArchetype::Merchant:
			return ENPCActivity::Trading;

		case EPersonalityArchetype::Mage:
			return ENPCActivity::Training;

		case EPersonalityArchetype::Hunter:
			return ENPCActivity::Hunting;

		case EPersonalityArchetype::Hermit:
			return FindOpportunisticActivity();

		default:
			break;
		}
	}

	// 6. Socialize if social trait is high
	if (ShouldSocialize())
	{
		return ENPCActivity::Socializing;
	}

	// 7. Find opportunistic activity
	return FindOpportunisticActivity();
}

void UAoCNPCLifeBrain::SetActivity(ENPCActivity NewActivity)
{
	if (NewActivity == CurrentActivity) return;

	ENPCActivity OldActivity = CurrentActivity;
	CurrentActivity = NewActivity;
	CurrentActivityDuration = 0.f;
	CurrentActivityStartTime = GetWorldTime();

	// Update skill system's practice skill
	if (SkillSystem)
	{
		SkillSystem->CurrentPracticeSkill = GetSkillForActivity(NewActivity);
	}

	// Find activity location if needed
	EResourceType ResType = GetResourceForActivity(NewActivity);
	if (ResType != static_cast<EResourceType>(255)) // Valid resource needed
	{
		FVector ResourceLoc = FindNearestResource(ResType);
		if (ResourceLoc != FVector::ZeroVector)
		{
			CurrentActivityLocation = ResourceLoc;
			TravelTo(ResourceLoc);
		}
	}

	UE_LOG(LogAoCLife, Log, TEXT("LifeBrain: %s activity: %d → %d"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		static_cast<int32>(OldActivity), static_cast<int32>(NewActivity));
}

// ---------------------------------------------------------------------------
// Activity Tick
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::TickCurrentActivity(float DeltaTime)
{
	if (!OwnerNPC.IsValid()) return;

	CurrentActivityDuration += DeltaTime;

	// Handle travel first
	if (bIsTraveling)
	{
		if (HasReachedDestination())
		{
			bIsTraveling = false;
		}
		else
		{
			// Continue moving toward destination
			FVector CurrentPos = OwnerNPC->GetActorLocation();
			FVector Dir = (TravelDestination - CurrentPos).GetSafeNormal();
			OwnerNPC->AddMovementInput(Dir, 1.0f);
			return;
		}
	}

	switch (CurrentActivity)
	{
	case ENPCActivity::Mining:
	{
		// Mining: periodic ore gain + skill XP
		float MiningInterval = 3.0f; // One ore every 3 seconds
		if (CurrentActivityDuration >= MiningInterval)
		{
			if (SkillSystem)
			{
				float MiningSkill = SkillSystem->GetSkillLevel(ESkillID::Mining);
				float Difficulty = FMath::Max(1.f - (MiningSkill / 100.f), 0.1f); // Higher skill = easier
				SkillSystem->GainSkillXP(ESkillID::Mining, 8.f, 1.f / FMath::Max(Difficulty, 0.1f));
			}

			// Add ore to inventory
			// Inventory->AddToBag(MakeOreItem());

			// Update goal progress
			for (FNPCGoal& Goal : MediumGoals)
			{
				if (Goal.Type == EGoalType::AcquireItem && Goal.TargetItemName == TEXT("IronOre"))
				{
					Goal.CurrentValue += 1.f;
					Goal.Progress = FMath::Min(Goal.CurrentValue / FMath::Max(Goal.TargetValue, 1.f), 1.f);
				}
			}

			// Hunger increases from labor
			// NeedSystem->ModifyNeed(ENeedType::Hunger, -2.f);
			// NeedSystem->ModifyNeed(ENeedType::Energy, -1.f);

			CurrentActivityDuration -= MiningInterval;
		}
		break;
	}
	case ENPCActivity::Woodcutting:
	{
		float ChopInterval = 4.0f;
		if (CurrentActivityDuration >= ChopInterval)
		{
			if (SkillSystem)
			{
				SkillSystem->GainSkillXP(ESkillID::Woodcutting, 7.f, 1.0f);
			}
			CurrentActivityDuration -= ChopInterval;
		}
		break;
	}
	case ENPCActivity::Fishing:
	{
		float FishInterval = 8.0f; // Fishing is slower
		if (CurrentActivityDuration >= FishInterval)
		{
			if (SkillSystem)
			{
				float FishSkill = SkillSystem->GetSkillLevel(ESkillID::Fishing);
				// Success chance based on skill
				float SuccessChance = 0.3f + FishSkill * 0.007f; // 30-100%
				if (LifeRand.FRand() < SuccessChance)
				{
					SkillSystem->GainSkillXP(ESkillID::Fishing, 10.f, 1.0f);
					// Inventory->AddToBag(MakeFishItem());
				}
				else
				{
					SkillSystem->GainSkillXP(ESkillID::Fishing, 3.f, 0.5f); // XP even on failure
				}
			}
			CurrentActivityDuration -= FishInterval;
		}
		break;
	}
	case ENPCActivity::Hunting:
	{
		// Hunting: track creatures, periodic combat
		float HuntInterval = 10.0f;
		if (CurrentActivityDuration >= HuntInterval)
		{
			if (SkillSystem)
			{
				SkillSystem->GainSkillXP(ESkillID::Hunting, 12.f, 1.2f);
				SkillSystem->GainSkillXP(ESkillID::Bow, 5.f, 0.8f);
			}
			CurrentActivityDuration -= HuntInterval;
		}
		break;
	}
	case ENPCActivity::Cooking:
	{
		float CookInterval = 5.0f;
		if (CurrentActivityDuration >= CookInterval)
		{
			if (SkillSystem)
			{
				SkillSystem->GainSkillXP(ESkillID::Cooking, 6.f, 1.0f);
			}
			// Convert raw ingredients to food
			// If successful: NeedSystem->ModifyNeed(ENeedType::Hunger, +25.f) when eaten

			// Update goals
			for (FNPCGoal& Goal : ShortGoals)
			{
				if (Goal.Type == EGoalType::SurvivalGoal && Goal.GoalName == TEXT("CookFood"))
				{
					Goal.bCompleted = true;
				}
			}

			CurrentActivityDuration -= CookInterval;
		}
		break;
	}
	case ENPCActivity::Crafting:
	{
		float CraftInterval = 8.0f;
		if (CurrentActivityDuration >= CraftInterval)
		{
			if (SkillSystem)
			{
				// Gain XP in the relevant crafting skill
				// Determine from current goal what we're crafting
				ESkillID CraftSkill = ESkillID::WeaponSmithing; // Default
				for (const FNPCGoal& Goal : MediumGoals)
				{
					if (Goal.Type == EGoalType::CraftItem)
					{
						// Determine craft skill from item name
						FString ItemStr = Goal.TargetItemName.ToString();
						if (ItemStr.Contains(TEXT("Sword")) || ItemStr.Contains(TEXT("Axe")))
							CraftSkill = ESkillID::WeaponSmithing;
						else if (ItemStr.Contains(TEXT("Armor")) || ItemStr.Contains(TEXT("Plate")))
							CraftSkill = ESkillID::ArmorSmithing;
						else if (ItemStr.Contains(TEXT("Bow")))
							CraftSkill = ESkillID::BowCrafting;
						else if (ItemStr.Contains(TEXT("Ring")) || ItemStr.Contains(TEXT("Amulet")))
							CraftSkill = ESkillID::Jewelcrafting;
						break;
					}
				}

				SkillSystem->GainSkillXP(CraftSkill, 10.f, 1.2f);
			}
			CurrentActivityDuration -= CraftInterval;
		}
		break;
	}
	case ENPCActivity::Training:
	{
		float TrainInterval = 2.0f; // Training is fast but grants less XP per hit
		if (CurrentActivityDuration >= TrainInterval)
		{
			if (SkillSystem)
			{
				// Train best weapon skill (practicing at a dummy)
				ESkillID BestWeapon = SkillSystem->GetBestWeaponSkill();
				SkillSystem->GainSkillXP(BestWeapon, 4.f, 0.8f); // Less XP than real combat

				// Sometimes train secondary weapon
				if (LifeRand.FRand() > 0.7f)
				{
					TArray<ESkillID> TopSkills = SkillSystem->GetTopSkills(3);
					for (ESkillID S : TopSkills)
					{
						if (UAoCNPCSkillSystem::IsWeaponSkill(S) && S != BestWeapon)
						{
							SkillSystem->GainSkillXP(S, 3.f, 0.7f);
							break;
						}
					}
				}
			}
			CurrentActivityDuration -= TrainInterval;
		}
		break;
	}
	case ENPCActivity::Socializing:
	{
		// Passive social interaction — familiarity and trust increase handled by SocialBrain tick
		if (SocialBrain && CurrentActivityDuration > 30.f)
		{
			// After 30 seconds of socializing, consider forming a group
			// Find nearest NPC to socialize with
			UWorld* World = GetWorld();
			if (World)
			{
				TArray<AActor*> NearbyActors;
				UGameplayStatics::GetAllActorsOfClass(World, AoCHumanoidNPCV2::StaticClass(), NearbyActors);

				for (AActor* Actor : NearbyActors)
				{
					if (Actor == OwnerNPC.Get()) continue;
					float Dist = FVector::Dist(Actor->GetActorLocation(), OwnerNPC->GetActorLocation());
					if (Dist > 1000.f) continue;

					AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Actor);
					if (OtherNPC && SocialBrain->ShouldGroupWith(OtherNPC))
					{
						TArray<AoCHumanoidNPCV2*> GroupMembers;
						GroupMembers.Add(OwnerNPC.Get());
						GroupMembers.Add(OtherNPC);
						SocialBrain->FormGroup(GroupMembers);
						break;
					}
				}
			}
		}
		break;
	}
	case ENPCActivity::Eating:
	{
		// Brief activity — eat food from inventory
		if (CurrentActivityDuration >= 3.0f) // 3 seconds to eat
		{
			// Consume food item
			// NeedSystem->ModifyNeed(ENeedType::Hunger, +40.f);

			// Complete eating goal
			for (FNPCGoal& Goal : ShortGoals)
			{
				if (Goal.Type == EGoalType::SurvivalGoal && Goal.GoalName == TEXT("Eat"))
				{
					Goal.bCompleted = true;
				}
			}

			// Return to previous productive activity
			SetActivity(DecideNextActivity());
		}
		break;
	}
	case ENPCActivity::Sleeping:
	{
		// Recover energy while sleeping
		// NeedSystem->ModifyNeed(ENeedType::Energy, +10.f * DeltaTime);

		// Wake up at dawn
		if (GetTimeOfDay() == ETimeOfDay::Dawn || GetTimeOfDay() == ETimeOfDay::Morning)
		{
			SetActivity(ENPCActivity::Eating); // Wake up → eat
		}
		break;
	}
	case ENPCActivity::Patrolling:
	{
		if (PatrolWaypoints.Num() > 0)
		{
			FVector Target = PatrolWaypoints[CurrentPatrolIndex];
			float Dist = FVector::Dist(OwnerNPC->GetActorLocation(), Target);

			if (Dist < 200.f) // Reached waypoint
			{
				CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolWaypoints.Num();
			}
			else
			{
				FVector Dir = (Target - OwnerNPC->GetActorLocation()).GetSafeNormal();
				OwnerNPC->AddMovementInput(Dir, 0.7f); // Walk pace for patrol
			}
		}
		break;
	}
	case ENPCActivity::Guarding:
	{
		// Stand at post, look around periodically
		if (HomeLocation != FVector::ZeroVector)
		{
			float DistToPost = FVector::Dist(OwnerNPC->GetActorLocation(), HomeLocation);
			if (DistToPost > 500.f)
			{
				// Return to post
				FVector Dir = (HomeLocation - OwnerNPC->GetActorLocation()).GetSafeNormal();
				OwnerNPC->AddMovementInput(Dir, 0.8f);
			}
		}
		break;
	}
	case ENPCActivity::Exploring:
	{
		// Wander in a direction, occasionally change direction
		float ExploreInterval = 15.f + LifeRand.FRandRange(0.f, 10.f);
		if (CurrentActivityDuration >= ExploreInterval)
		{
			// Pick a new direction
			FVector RandomDir = FVector(LifeRand.FRandRange(-1.f, 1.f), LifeRand.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			FVector NewDest = OwnerNPC->GetActorLocation() + RandomDir * 5000.f;
			TravelTo(NewDest);
			CurrentActivityDuration = 0.f;
		}
		else if (bIsTraveling)
		{
			FVector Dir = (TravelDestination - OwnerNPC->GetActorLocation()).GetSafeNormal();
			OwnerNPC->AddMovementInput(Dir, 0.6f);
		}
		break;
	}
	case ENPCActivity::Trading:
	{
		// At a merchant — buy/sell logic
		// Simplified: just wait at trading location
		if (CurrentActivityDuration > 20.f)
		{
			// Done trading, move on
			SetActivity(DecideNextActivity());
		}
		break;
	}
	case ENPCActivity::Farming:
	{
		float FarmInterval = 5.0f;
		if (CurrentActivityDuration >= FarmInterval)
		{
			if (SkillSystem)
			{
				SkillSystem->GainSkillXP(ESkillID::Farming, 6.f, 1.0f);
			}
			CurrentActivityDuration -= FarmInterval;
		}
		break;
	}
	case ENPCActivity::Building:
	{
		float BuildInterval = 6.0f;
		if (CurrentActivityDuration >= BuildInterval)
		{
			if (SkillSystem)
			{
				SkillSystem->GainSkillXP(ESkillID::Carpentry, 8.f, 1.0f);
				SkillSystem->GainSkillXP(ESkillID::Masonry, 5.f, 0.8f);
			}
			CurrentActivityDuration -= BuildInterval;
		}
		break;
	}
	case ENPCActivity::HuntingPlayers:
	{
		// Aggressive: seek out players
		UWorld* World = GetWorld();
		if (World)
		{
			float SearchRadius = 10000.f; // 100m
			APawn* NearestPlayer = nullptr;
			float NearestDist = SearchRadius;

			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (PC && PC->GetPawn())
				{
					float Dist = FVector::Dist(PC->GetPawn()->GetActorLocation(), OwnerNPC->GetActorLocation());
					if (Dist < NearestDist)
					{
						NearestDist = Dist;
						NearestPlayer = PC->GetPawn();
					}
				}
			}

			if (NearestPlayer && NearestDist < 5000.f) // Close enough to engage
			{
				// Enter combat
				if (OwnerNPC->CombatBrain && !OwnerNPC->CombatBrain->IsInCombat())
				{
					OwnerNPC->CombatBrain->EnterCombat(NearestPlayer);
				}
			}
			else if (NearestPlayer)
			{
				// Move toward player
				FVector Dir = (NearestPlayer->GetActorLocation() - OwnerNPC->GetActorLocation()).GetSafeNormal();
				OwnerNPC->AddMovementInput(Dir, 0.9f);
			}
			else
			{
				// No players found, explore
				SetActivity(ENPCActivity::Exploring);
			}
		}
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Critical Needs Check
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::CheckCriticalNeeds()
{
	// In production, query NeedSystem for critical needs
	// if (NeedSystem && NeedSystem->IsCritical())
	// {
	//     ENeedType Urgent = NeedSystem->GetMostUrgentNeed();
	//     OnNeedBecomesCritical(static_cast<uint8>(Urgent));
	// }
}

void UAoCNPCLifeBrain::OnNeedBecomesCritical(uint8 NeedType)
{
	// Interrupt current activity for urgent need
	FNPCGoal ImmGoal;
	ImmGoal.Priority = EGoalPriority::Immediate;
	ImmGoal.Type = EGoalType::SurvivalGoal;

	// NeedType mapping: 0=Hunger, 1=Energy, 2=Safety, 3=Purpose, 4=Social, 5=Wealth
	switch (NeedType)
	{
	case 0: // Hunger
		ImmGoal.GoalName = TEXT("Eat");
		SetActivity(ENPCActivity::Eating);
		break;
	case 1: // Energy
		ImmGoal.GoalName = TEXT("Rest");
		SetActivity(ENPCActivity::Sleeping);
		break;
	case 2: // Safety
		ImmGoal.GoalName = TEXT("FindSafety");
		ReturnToBase();
		break;
	case 4: // Social
		ImmGoal.GoalName = TEXT("Socialize");
		SetActivity(ENPCActivity::Socializing);
		break;
	default:
		return;
	}

	ImmediateGoal = ImmGoal;
	bHasImmediateGoal = true;

	UE_LOG(LogAoCLife, Log, TEXT("LifeBrain: %s critical need %d — immediate goal: %s"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		NeedType, *ImmGoal.GoalName.ToString());
}

// ---------------------------------------------------------------------------
// Goal System
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::SetLongTermGoal(const FNPCGoal& Goal)
{
	LongTermGoals.Add(Goal);

	// Immediately break down into medium goals
	TArray<FNPCGoal> MedGoals = BreakDownGoal(Goal);
	for (const FNPCGoal& MG : MedGoals)
	{
		MediumGoals.Add(MG);
	}

	UE_LOG(LogAoCLife, Log, TEXT("LifeBrain: %s set long-term goal: %s (→ %d medium goals)"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		*Goal.GoalName.ToString(), MedGoals.Num());
}

TArray<FNPCGoal> UAoCNPCLifeBrain::BreakDownGoal(const FNPCGoal& Goal) const
{
	switch (Goal.Type)
	{
	case EGoalType::GearUpgrade:
		return BreakDownGearGoal(Goal);
	case EGoalType::AcquireItem:
		return BreakDownAcquireGoal(Goal);
	case EGoalType::CraftItem:
		return BreakDownCraftGoal(Goal);
	case EGoalType::ReachSkillLevel:
		return BreakDownSkillGoal(Goal);
	case EGoalType::AccumulateWealth:
	{
		TArray<FNPCGoal> Goals;
		FNPCGoal TradeGoal;
		TradeGoal.GoalName = TEXT("TradeForGold");
		TradeGoal.Type = EGoalType::AccumulateWealth;
		TradeGoal.Priority = EGoalPriority::Medium;
		TradeGoal.TargetValue = Goal.TargetValue;
		Goals.Add(TradeGoal);
		return Goals;
	}
	default:
	{
		TArray<FNPCGoal> Empty;
		return Empty;
	}
	}
}

TArray<FNPCGoal> UAoCNPCLifeBrain::BreakDownGearGoal(const FNPCGoal& Goal) const
{
	TArray<FNPCGoal> SubGoals;

	// "Get iron armor" → need multiple pieces
	// Each piece is a CraftItem goal
	TArray<FName> PieceNames = {TEXT("IronBreastplate"), TEXT("IronGreaves"), TEXT("IronGauntlets"), TEXT("IronHelm")};

	for (const FName& Piece : PieceNames)
	{
		FNPCGoal CraftGoal;
		CraftGoal.GoalName = FName(*FString::Printf(TEXT("Craft_%s"), *Piece.ToString()));
		CraftGoal.Type = EGoalType::CraftItem;
		CraftGoal.Priority = EGoalPriority::Medium;
		CraftGoal.TargetItemName = Piece;
		CraftGoal.TargetValue = 1.f;
		SubGoals.Add(CraftGoal);
	}

	return SubGoals;
}

TArray<FNPCGoal> UAoCNPCLifeBrain::BreakDownAcquireGoal(const FNPCGoal& Goal) const
{
	TArray<FNPCGoal> SubGoals;

	// "Get 30 iron ore" → go to mine, mine
	FNPCGoal MineGoal;
	MineGoal.GoalName = TEXT("MineIronOre");
	MineGoal.Type = EGoalType::AcquireItem;
	MineGoal.Priority = EGoalPriority::Medium;
	MineGoal.TargetItemName = TEXT("IronOre");
	MineGoal.TargetValue = Goal.TargetValue;
	SubGoals.Add(MineGoal);

	return SubGoals;
}

TArray<FNPCGoal> UAoCNPCLifeBrain::BreakDownCraftGoal(const FNPCGoal& Goal) const
{
	TArray<FNPCGoal> SubGoals;

	// Crafting requires materials
	// "Craft Iron Breastplate" → need iron bars → need iron ore → mine
	FNPCGoal GetOreGoal;
	GetOreGoal.GoalName = TEXT("GetIronOre");
	GetOreGoal.Type = EGoalType::AcquireItem;
	GetOreGoal.Priority = EGoalPriority::Medium;
	GetOreGoal.TargetItemName = TEXT("IronOre");
	GetOreGoal.TargetValue = 10.f; // Need 10 ore for bars for one armor piece
	SubGoals.Add(GetOreGoal);

	FNPCGoal SmeltGoal;
	SmeltGoal.GoalName = TEXT("SmeltIronBars");
	SmeltGoal.Type = EGoalType::CraftItem;
	SmeltGoal.Priority = EGoalPriority::Medium;
	SmeltGoal.TargetItemName = TEXT("IronBar");
	SmeltGoal.TargetValue = 5.f;
	SubGoals.Add(SmeltGoal);

	FNPCGoal FinalCraft;
	FinalCraft.GoalName = FName(*FString::Printf(TEXT("FinalCraft_%s"), *Goal.TargetItemName.ToString()));
	FinalCraft.Type = EGoalType::CraftItem;
	FinalCraft.Priority = EGoalPriority::Medium;
	FinalCraft.TargetItemName = Goal.TargetItemName;
	FinalCraft.TargetValue = 1.f;
	SubGoals.Add(FinalCraft);

	return SubGoals;
}

TArray<FNPCGoal> UAoCNPCLifeBrain::BreakDownSkillGoal(const FNPCGoal& Goal) const
{
	TArray<FNPCGoal> SubGoals;

	// "Reach Sword 50" → practice sword daily
	FNPCGoal TrainGoal;
	TrainGoal.GoalName = TEXT("TrainSkill");
	TrainGoal.Type = EGoalType::ReachSkillLevel;
	TrainGoal.Priority = EGoalPriority::Medium;
	TrainGoal.TargetSkill = Goal.TargetSkill;
	TrainGoal.TargetValue = Goal.TargetValue;
	SubGoals.Add(TrainGoal);

	return SubGoals;
}

void UAoCNPCLifeBrain::GenerateGoalsFromArchetype()
{
	if (!OwnerNPC.IsValid()) return;

	FNPCGoal Goal = GenerateLongTermGoal();
	if (!Goal.GoalName.IsNone())
	{
		SetLongTermGoal(Goal);
	}
}

FNPCGoal UAoCNPCLifeBrain::GenerateLongTermGoal() const
{
	FNPCGoal Goal;
	if (!OwnerNPC.IsValid()) return Goal;

	switch (OwnerNPC->ArchetypePreset)
	{
	case EPersonalityArchetype::Villager:
	{
		int32 GoalType = LifeRand.RandRange(0, 2);
		if (GoalType == 0)
		{
			Goal.GoalName = TEXT("GetIronArmorSet");
			Goal.Type = EGoalType::GearUpgrade;
			Goal.Priority = EGoalPriority::LongTerm;
		}
		else if (GoalType == 1)
		{
			Goal.GoalName = TEXT("AccumulateGold");
			Goal.Type = EGoalType::AccumulateWealth;
			Goal.Priority = EGoalPriority::LongTerm;
			Goal.TargetValue = 500.f;
		}
		else
		{
			Goal.GoalName = TEXT("MasterFarming");
			Goal.Type = EGoalType::ReachSkillLevel;
			Goal.Priority = EGoalPriority::LongTerm;
			Goal.TargetSkill = ESkillID::Farming;
			Goal.TargetValue = 60.f;
		}
		break;
	}
	case EPersonalityArchetype::Guard:
	{
		Goal.GoalName = TEXT("MasterWeapon");
		Goal.Type = EGoalType::ReachSkillLevel;
		Goal.Priority = EGoalPriority::LongTerm;
		Goal.TargetSkill = SkillSystem ? SkillSystem->GetBestWeaponSkill() : ESkillID::Sword;
		Goal.TargetValue = 80.f;
		break;
	}
	case EPersonalityArchetype::Bandit:
	{
		Goal.GoalName = TEXT("AccumulateWealth");
		Goal.Type = EGoalType::AccumulateWealth;
		Goal.Priority = EGoalPriority::LongTerm;
		Goal.TargetValue = 1000.f;
		break;
	}
	case EPersonalityArchetype::Merchant:
	{
		Goal.GoalName = TEXT("MasterCrafting");
		Goal.Type = EGoalType::ReachSkillLevel;
		Goal.Priority = EGoalPriority::LongTerm;
		Goal.TargetSkill = ESkillID::WeaponSmithing;
		Goal.TargetValue = 70.f;
		break;
	}
	case EPersonalityArchetype::Mage:
	{
		Goal.GoalName = TEXT("MasterMagic");
		Goal.Type = EGoalType::ReachSkillLevel;
		Goal.Priority = EGoalPriority::LongTerm;
		Goal.TargetSkill = SkillSystem ? SkillSystem->GetBestMagicSkill() : ESkillID::Pyromancy;
		Goal.TargetValue = 90.f;
		break;
	}
	case EPersonalityArchetype::Hunter:
	{
		Goal.GoalName = TEXT("MasterBow");
		Goal.Type = EGoalType::ReachSkillLevel;
		Goal.Priority = EGoalPriority::LongTerm;
		Goal.TargetSkill = ESkillID::Bow;
		Goal.TargetValue = 80.f;
		break;
	}
	case EPersonalityArchetype::Hermit:
	{
		Goal.GoalName = TEXT("SelfSufficiency");
		Goal.Type = EGoalType::ReachSkillLevel;
		Goal.Priority = EGoalPriority::LongTerm;
		Goal.TargetSkill = ESkillID::Cooking;
		Goal.TargetValue = 60.f;
		break;
	}
	}

	Goal.TimeStarted = GetWorldTime();
	return Goal;
}

void UAoCNPCLifeBrain::AdvanceGoals(float DeltaTime)
{
	// Update progress for skill-based goals
	if (SkillSystem)
	{
		for (FNPCGoal& Goal : LongTermGoals)
		{
			if (Goal.Type == EGoalType::ReachSkillLevel && Goal.TargetSkill != ESkillID::MAX)
			{
				float CurrentLevel = SkillSystem->GetSkillLevel(Goal.TargetSkill);
				Goal.CurrentValue = CurrentLevel;
				Goal.Progress = FMath::Min(CurrentLevel / FMath::Max(Goal.TargetValue, 1.f), 1.f);

				if (Goal.Progress >= 1.f && !Goal.bCompleted)
				{
					Goal.bCompleted = true;
					UE_LOG(LogAoCLife, Log, TEXT("LifeBrain: %s completed long-term goal: %s!"),
						OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
						*Goal.GoalName.ToString());

					// Generate a new long-term goal
					FNPCGoal NewGoal = GenerateLongTermGoal();
					if (!NewGoal.GoalName.IsNone())
					{
						SetLongTermGoal(NewGoal);
					}
				}
			}
		}
	}

	// Clean up completed medium/short goals
	MediumGoals.RemoveAll([](const FNPCGoal& G) { return G.IsComplete(); });
	ShortGoals.RemoveAll([](const FNPCGoal& G) { return G.IsComplete(); });
}

bool UAoCNPCLifeBrain::HasGoalOfType(EGoalType Type) const
{
	for (const FNPCGoal& G : LongTermGoals)
	{
		if (G.Type == Type && !G.IsComplete()) return true;
	}
	for (const FNPCGoal& G : MediumGoals)
	{
		if (G.Type == Type && !G.IsComplete()) return true;
	}
	return false;
}

float UAoCNPCLifeBrain::GetGoalProgress(const FNPCGoal& Goal) const
{
	return Goal.Progress;
}

// ---------------------------------------------------------------------------
// Time of Day
// ---------------------------------------------------------------------------

ETimeOfDay UAoCNPCLifeBrain::GetTimeOfDay() const
{
	float WorldTime = GetWorldTime();
	float DayProgress = FMath::Fmod(WorldTime, DayLengthSeconds) / DayLengthSeconds;

	// Day phases: Dawn(0.0-0.08), Morning(0.08-0.25), Midday(0.25-0.42),
	//             Afternoon(0.42-0.58), Evening(0.58-0.75), Night(0.75-1.0)
	if (DayProgress < 0.08f) return ETimeOfDay::Dawn;
	if (DayProgress < 0.25f) return ETimeOfDay::Morning;
	if (DayProgress < 0.42f) return ETimeOfDay::Midday;
	if (DayProgress < 0.58f) return ETimeOfDay::Afternoon;
	if (DayProgress < 0.75f) return ETimeOfDay::Evening;
	return ETimeOfDay::Night;
}

// ---------------------------------------------------------------------------
// Boredom / Variety
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::UpdateTedium(float DeltaTime)
{
	// Increase tedium for current activity
	float* CurrentTedium = TediumMap.Find(CurrentActivity);
	if (CurrentTedium)
	{
		*CurrentTedium += DeltaTime;
	}

	// Decay tedium for all OTHER activities
	for (auto& Pair : TediumMap)
	{
		if (Pair.Key != CurrentActivity && Pair.Value > 0.f)
		{
			Pair.Value = FMath::Max(Pair.Value - TediumDecayRate * DeltaTime, 0.f);
		}
	}
}

bool UAoCNPCLifeBrain::ShouldSwitchActivity() const
{
	const float* CurrentTedium = TediumMap.Find(CurrentActivity);
	if (!CurrentTedium) return false;

	float Threshold = GetTediumThreshold();
	return *CurrentTedium > Threshold;
}

float UAoCNPCLifeBrain::GetTediumThreshold() const
{
	float PatientMult = 0.5f; // Default moderate patience

	// In production: read from Personality
	// PatientMult = Personality->GetTraitValue(TEXT("Patience"));

	// Low patience (0.2): threshold = 60 * 0.2 = 12 seconds before bored
	// High patience (0.8): threshold = 60 * 0.8 = 48 seconds
	return TediumThresholdBase * FMath::Max(PatientMult, 0.1f);
}

// ---------------------------------------------------------------------------
// Resource / Location Finding
// ---------------------------------------------------------------------------

FVector UAoCNPCLifeBrain::FindNearestResource(EResourceType ResourceType) const
{
	if (!OwnerNPC.IsValid()) return FVector::ZeroVector;

	// In production: query world resource system for nearest node of type
	// Memory can also help — check if we remember a resource location

	// Use navigation system to find a reachable point
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation NavLoc;
		FVector SearchOrigin = OwnerNPC->GetActorLocation();

		// Search in expanding radius
		float SearchRadius = 5000.f;
		if (NavSys->GetRandomReachablePointInRadius(SearchOrigin, SearchRadius, NavLoc))
		{
			return NavLoc.Location;
		}
	}

	// Fallback: random direction from current position
	FVector RandomDir = FVector(LifeRand.FRandRange(-1.f, 1.f), LifeRand.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	return OwnerNPC->GetActorLocation() + RandomDir * 3000.f;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::TravelTo(const FVector& Destination)
{
	TravelDestination = Destination;
	bIsTraveling = true;
}

void UAoCNPCLifeBrain::ReturnToBase()
{
	if (HomeLocation != FVector::ZeroVector)
	{
		TravelTo(HomeLocation);
		SetActivity(ENPCActivity::Traveling);
	}
}

bool UAoCNPCLifeBrain::HasReachedDestination() const
{
	if (!OwnerNPC.IsValid() || !bIsTraveling) return true;
	return FVector::Dist(OwnerNPC->GetActorLocation(), TravelDestination) < 200.f;
}

// ---------------------------------------------------------------------------
// Event Handlers
// ---------------------------------------------------------------------------

void UAoCNPCLifeBrain::OnCombatEnded(bool bWon)
{
	if (bWon)
	{
		// Return to previous activity (or loot if corpse available)
		UE_LOG(LogAoCLife, Log, TEXT("LifeBrain: %s won combat! Resuming activity."),
			OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"));
	}
	else
	{
		// Lost combat — the character is dead, life brain won't tick
	}
}

void UAoCNPCLifeBrain::OnItemAcquired(FName ItemName, int32 Count)
{
	// Update goal progress for acquire goals
	for (FNPCGoal& Goal : MediumGoals)
	{
		if (Goal.Type == EGoalType::AcquireItem && Goal.TargetItemName == ItemName)
		{
			Goal.CurrentValue += Count;
			Goal.Progress = FMath::Min(Goal.CurrentValue / FMath::Max(Goal.TargetValue, 1.f), 1.f);

			if (Goal.IsComplete())
			{
				UE_LOG(LogAoCLife, Log, TEXT("LifeBrain: %s completed goal: %s"),
					OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
					*Goal.GoalName.ToString());
			}
		}
	}
}

void UAoCNPCLifeBrain::OnSkillGained(ESkillID Skill, float NewLevel)
{
	// Update skill-related goals
	for (FNPCGoal& Goal : LongTermGoals)
	{
		if (Goal.Type == EGoalType::ReachSkillLevel && Goal.TargetSkill == Skill)
		{
			Goal.CurrentValue = NewLevel;
			Goal.Progress = FMath::Min(NewLevel / FMath::Max(Goal.TargetValue, 1.f), 1.f);
		}
	}
}

void UAoCNPCLifeBrain::EvaluateOpportunity(AActor* Something)
{
	// React to something in the world — a resource node, another NPC, danger
	if (!Something || !OwnerNPC.IsValid()) return;

	// Check if it's an enemy
	if (SocialBrain && SocialBrain->ShouldAttack(Something))
	{
		if (OwnerNPC->CombatBrain && !OwnerNPC->CombatBrain->IsInCombat())
		{
			OwnerNPC->CombatBrain->EnterCombat(Something);
		}
		return;
	}

	// Check if it's a potential group member
	AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Something);
	if (OtherNPC && SocialBrain && SocialBrain->ShouldGroupWith(OtherNPC))
	{
		// Record friendly interaction
		SocialBrain->RecordInteraction(OtherNPC->GetUniqueID(), OtherNPC->GetDisplayName(),
			ESocialInteraction::Greeting, 0.5f);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

ENPCActivity UAoCNPCLifeBrain::GetActivityForGoal(const FNPCGoal& Goal) const
{
	switch (Goal.Type)
	{
	case EGoalType::AcquireItem:
	{
		FString ItemStr = Goal.TargetItemName.ToString();
		if (ItemStr.Contains(TEXT("Ore")) || ItemStr.Contains(TEXT("Iron")))
			return ENPCActivity::Mining;
		if (ItemStr.Contains(TEXT("Wood")) || ItemStr.Contains(TEXT("Log")))
			return ENPCActivity::Woodcutting;
		if (ItemStr.Contains(TEXT("Fish")))
			return ENPCActivity::Fishing;
		if (ItemStr.Contains(TEXT("Herb")))
			return ENPCActivity::Farming;
		if (ItemStr.Contains(TEXT("Leather")) || ItemStr.Contains(TEXT("Pelt")))
			return ENPCActivity::Hunting;
		return ENPCActivity::Exploring;
	}
	case EGoalType::CraftItem:
		return ENPCActivity::Crafting;
	case EGoalType::ReachSkillLevel:
		if (Goal.TargetSkill != ESkillID::MAX && UAoCNPCSkillSystem::IsWeaponSkill(Goal.TargetSkill))
			return ENPCActivity::Training;
		if (Goal.TargetSkill != ESkillID::MAX && UAoCNPCSkillSystem::IsGatheringSkill(Goal.TargetSkill))
		{
			switch (Goal.TargetSkill)
			{
			case ESkillID::Mining: return ENPCActivity::Mining;
			case ESkillID::Woodcutting: return ENPCActivity::Woodcutting;
			case ESkillID::Fishing: return ENPCActivity::Fishing;
			case ESkillID::Farming: return ENPCActivity::Farming;
			case ESkillID::Hunting: return ENPCActivity::Hunting;
			default: return ENPCActivity::Exploring;
			}
		}
		if (Goal.TargetSkill != ESkillID::MAX && UAoCNPCSkillSystem::IsCraftingSkill(Goal.TargetSkill))
			return ENPCActivity::Crafting;
		return ENPCActivity::Training;
	case EGoalType::AccumulateWealth:
		return ENPCActivity::Trading;
	case EGoalType::BuildStructure:
		return ENPCActivity::Building;
	case EGoalType::ExploreArea:
		return ENPCActivity::Exploring;
	case EGoalType::DefeatEnemy:
		return ENPCActivity::HuntingPlayers;
	case EGoalType::SocialGoal:
		return ENPCActivity::Socializing;
	default:
		return ENPCActivity::Idle;
	}
}

ESkillID UAoCNPCLifeBrain::GetSkillForActivity(ENPCActivity Activity) const
{
	switch (Activity)
	{
	case ENPCActivity::Mining: return ESkillID::Mining;
	case ENPCActivity::Woodcutting: return ESkillID::Woodcutting;
	case ENPCActivity::Fishing: return ESkillID::Fishing;
	case ENPCActivity::Hunting: return ESkillID::Hunting;
	case ENPCActivity::Farming: return ESkillID::Farming;
	case ENPCActivity::Cooking: return ESkillID::Cooking;
	case ENPCActivity::Crafting: return ESkillID::WeaponSmithing; // Default — refined by goal context
	case ENPCActivity::Building: return ESkillID::Carpentry;
	case ENPCActivity::Training: return SkillSystem ? SkillSystem->GetBestWeaponSkill() : ESkillID::Unarmed;
	default: return ESkillID::MAX;
	}
}

EResourceType UAoCNPCLifeBrain::GetResourceForActivity(ENPCActivity Activity) const
{
	switch (Activity)
	{
	case ENPCActivity::Mining: return EResourceType::OreNode;
	case ENPCActivity::Woodcutting: return EResourceType::Tree;
	case ENPCActivity::Fishing: return EResourceType::FishingSpot;
	case ENPCActivity::Cooking: return EResourceType::CookingFire;
	case ENPCActivity::Crafting: return EResourceType::CraftingStation;
	case ENPCActivity::Training: return EResourceType::TrainingDummy;
	case ENPCActivity::Trading: return EResourceType::Merchant;
	case ENPCActivity::Sleeping: return EResourceType::Bed;
	case ENPCActivity::Guarding: return EResourceType::GuardPost;
	case ENPCActivity::Building: return EResourceType::ConstructionSite;
	default: return static_cast<EResourceType>(255); // No resource needed
	}
}

bool UAoCNPCLifeBrain::ShouldSleep() const
{
	ETimeOfDay Time = GetTimeOfDay();
	if (Time != ETimeOfDay::Night) return false;
	if (bIsNocturnal) return false;

	// Check energy need
	// float Energy = NeedSystem ? NeedSystem->GetNeedValue(ENeedType::Energy) : 50.f;
	// return Energy < 30.f;
	return true; // Default: sleep at night
}

bool UAoCNPCLifeBrain::ShouldSocialize() const
{
	float SocialTrait = 0.5f; // Default
	// In production: SocialTrait = Personality->GetTraitValue(TEXT("Social"));

	return SocialTrait > 0.4f && LifeRand.FRand() > 0.5f;
}

ENPCActivity UAoCNPCLifeBrain::FindOpportunisticActivity() const
{
	// Find something useful to do based on skills and nearby resources
	if (!SkillSystem) return ENPCActivity::Exploring;

	TArray<ESkillID> TopSkills = SkillSystem->GetTopSkills(3);

	for (ESkillID Skill : TopSkills)
	{
		if (Skill == ESkillID::Mining) return ENPCActivity::Mining;
		if (Skill == ESkillID::Woodcutting) return ENPCActivity::Woodcutting;
		if (Skill == ESkillID::Fishing) return ENPCActivity::Fishing;
		if (Skill == ESkillID::Farming) return ENPCActivity::Farming;
		if (Skill == ESkillID::Hunting) return ENPCActivity::Hunting;
		if (Skill == ESkillID::Cooking) return ENPCActivity::Cooking;
		if (UAoCNPCSkillSystem::IsCraftingSkill(Skill)) return ENPCActivity::Crafting;
		if (UAoCNPCSkillSystem::IsWeaponSkill(Skill)) return ENPCActivity::Training;
	}

	return ENPCActivity::Exploring;
}

float UAoCNPCLifeBrain::GetWorldTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UAoCNPCLifeBrain::ScoreActivity(ENPCActivity Activity) const
{
	float Score = 0.f;

	// Goal alignment
	for (const FNPCGoal& Goal : MediumGoals)
	{
		if (!Goal.IsComplete() && GetActivityForGoal(Goal) == Activity)
		{
			Score += 50.f * (1.f - Goal.Progress); // Higher score for less-progressed goals
		}
	}

	// Tedium penalty
	const float* Tedium = TediumMap.Find(Activity);
	if (Tedium)
	{
		Score -= *Tedium * 0.5f;
	}

	// Time of day bonus/penalty
	ETimeOfDay Time = GetTimeOfDay();
	if (Activity == ENPCActivity::Sleeping && Time == ETimeOfDay::Night) Score += 30.f;
	if (Activity == ENPCActivity::Sleeping && Time != ETimeOfDay::Night) Score -= 50.f;
	if (Activity == ENPCActivity::Socializing && Time == ETimeOfDay::Evening) Score += 20.f;

	return Score;
}
