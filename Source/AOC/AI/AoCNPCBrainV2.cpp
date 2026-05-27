// AoCNPCBrainV2.cpp — Master NPC AI controller implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCBrainV2.h"
#include "AoCNPCNeedSystem.h"
#include "AoCNPCMemory.h"
#include "AoCNPCInventory.h"
#include "AoCNPCGoalPlanner.h"
#include "AoCNPCPersonality.h"
#include "../AnimLab/AoCAnimationLab.h"
#include "../AnimLab/AoCSmartAnimPlayer.h"
#include "../AnimLab/AoCAnimationEntry.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NavigationSystem.h"

UAoCNPCBrainV2::UAoCNPCBrainV2()
{
	PrimaryComponentTick.bCanEverTick = false;
	WeaponTags = { TEXT("unarmed") };
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Cache subsystems
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		AnimLab = GI->GetSubsystem<UAoCAnimationLab>();
	}
	AnimPlayer = Owner->FindComponentByClass<UAoCSmartAnimPlayer>();

	// Cache sibling components (created by NPC character)
	NeedSystem = Owner->FindComponentByClass<UAoCNPCNeedSystem>();
	Memory = Owner->FindComponentByClass<UAoCNPCMemory>();
	Inventory = Owner->FindComponentByClass<UAoCNPCInventory>();
	GoalPlanner = Owner->FindComponentByClass<UAoCNPCGoalPlanner>();
	Personality = Owner->FindComponentByClass<UAoCNPCPersonality>();

	// Wire up the GoalPlanner's references
	if (GoalPlanner)
	{
		GoalPlanner->NeedSystem = NeedSystem;
		GoalPlanner->Memory = Memory;
		GoalPlanner->Inventory = Inventory;
		GoalPlanner->Personality = Personality;
	}

	if (!AnimLab)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrainV2] AnimationLab subsystem not found!"));
	}
	if (!AnimPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BrainV2] SmartAnimPlayer not found on %s"), *Owner->GetName());
	}

	// --- Start timers ---

	FTimerManager& TM = GetWorld()->GetTimerManager();

	// Perception: every 0.5s
	TM.SetTimer(PerceptionTimerHandle, this, &UAoCNPCBrainV2::RunPerception, 0.5f, true);

	// Planning: every 1.0s
	TM.SetTimer(PlanningTimerHandle, this, &UAoCNPCBrainV2::RunPlanning, 1.0f, true);

	// Execution: every 0.5s
	TM.SetTimer(ExecutionTimerHandle, this, &UAoCNPCBrainV2::RunExecution, 0.5f, true);

	UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s brain initialized."), *Owner->GetName());
}

void UAoCNPCBrainV2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		FTimerManager& TM = GetWorld()->GetTimerManager();
		TM.ClearTimer(PerceptionTimerHandle);
		TM.ClearTimer(PlanningTimerHandle);
		TM.ClearTimer(ExecutionTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// State management
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::SetBrainState(ENPCBrainState NewState)
{
	if (NewState == CurrentState) return;

	const ENPCBrainState OldState = CurrentState;
	CurrentState = NewState;
	OnBrainStateChanged.Broadcast(OldState, NewState);

	// Update need system flags based on state
	if (NeedSystem)
	{
		NeedSystem->bIsInCombat = (NewState == ENPCBrainState::Fighting);
		NeedSystem->bIsWorking = (NewState == ENPCBrainState::Gathering ||
		                          NewState == ENPCBrainState::Crafting);
		NeedSystem->bIsSprinting = (NewState == ENPCBrainState::Fleeing);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::SetHealthRatio(float Ratio)
{
	HealthRatio = FMath::Clamp(Ratio, 0.f, 1.f);

	if (HealthRatio <= 0.f)
	{
		SetBrainState(ENPCBrainState::Dead);
	}
}

void UAoCNPCBrainV2::NotifyHit(const FVector& HitDirection, float Damage)
{
	if (CurrentState == ENPCBrainState::Dead) return;

	// Record the attacker as a threat
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Memory && PlayerPawn)
	{
		Memory->RememberThreat(PlayerPawn, Damage);
		ACharacter* PlayerChar = Cast<ACharacter>(PlayerPawn);
		if (PlayerChar)
		{
			Memory->RecordPlayerEncounter(PlayerChar, EEncounterType::Attacked);
		}
	}

	// Play hit reaction
	if (AnimPlayer && AnimLab)
	{
		TArray<FString> Tags = { TEXT("react"), TEXT("hit") };

		const FVector Forward = GetOwner()->GetActorForwardVector();
		const float Dot = FVector::DotProduct(Forward, HitDirection.GetSafeNormal());
		if (Dot > 0.5f)       Tags.Add(TEXT("front"));
		else if (Dot < -0.5f) Tags.Add(TEXT("back"));
		else
		{
			const FVector Right = GetOwner()->GetActorRightVector();
			const float RightDot = FVector::DotProduct(Right, HitDirection.GetSafeNormal());
			Tags.Add(RightDot > 0.f ? TEXT("right") : TEXT("left"));
		}

		FAoCAnimationEntry HitReact = AnimLab->FindBestMatch(TEXT("combat_melee"), TEXT("standing"), Tags);
		AnimPlayer->InterruptWith(HitReact);
	}

	// Enter combat
	if (CurrentState != ENPCBrainState::Fighting && CurrentState != ENPCBrainState::Dead)
	{
		// Check personality: flee or fight?
		if (Personality && Personality->ShouldFlee(Damage) && HealthRatio < 0.3f)
		{
			CurrentTarget = PlayerPawn;
			SetBrainState(ENPCBrainState::Fleeing);
			ForceGoal(ENPCGoal::FleeFromThreat);
		}
		else
		{
			CurrentTarget = PlayerPawn;
			SetBrainState(ENPCBrainState::Fighting);
			ForceGoal(ENPCGoal::AttackTarget);
		}
	}

	// Drink potion if HP low and we have one
	if (HealthRatio < 0.3f && Inventory && Inventory->HasPotion())
	{
		const FNPCItem Potion = Inventory->GetBestPotion();
		if (Potion.IsValid())
		{
			Inventory->ConsumeItem(Potion.ItemID);
			UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s drinks potion!"), *GetOwner()->GetName());
		}
	}
}

void UAoCNPCBrainV2::NotifyNoise(const FVector& NoiseLocation, float Loudness)
{
	if (CurrentState == ENPCBrainState::Dead || CurrentState == ENPCBrainState::Fighting) return;

	const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), NoiseLocation);
	if (Dist <= PerceptionRadius * Loudness)
	{
		InvestigateLocation = NoiseLocation;
		bHasInvestigateTarget = true;

		if (Memory)
		{
			Memory->RememberLocation(FName("InvestigateTarget"), NoiseLocation);
		}
	}
}

void UAoCNPCBrainV2::ForceGoal(ENPCGoal NewGoal)
{
	if (NewGoal == ENPCGoal::Dead)
	{
		SetBrainState(ENPCBrainState::Dead);
		CurrentGoal = ENPCGoal::Dead;
		return;
	}

	CurrentGoal = NewGoal;

	// Force re-plan
	if (GoalPlanner)
	{
		// Save current plan for resumption if it's interruptible
		if (GoalPlanner->HasActivePlan() && GoalPlanner->CurrentPlan.GetCurrentAction().bInterruptibleByNeeds)
		{
			GoalPlanner->InterruptedPlan = GoalPlanner->CurrentPlan;
			GoalPlanner->InterruptedPlan.InterruptedAtIndex = GoalPlanner->CurrentPlan.CurrentActionIndex;
			GoalPlanner->InterruptedPlan.OriginalGoal = GoalPlanner->CurrentPlan.Goal;
			GoalPlanner->bHasInterruptedPlan = true;
		}

		GoalPlanner->CurrentPlan = GoalPlanner->CreatePlan(NewGoal);
		GoalPlanner->OnPlanChanged.Broadcast(NewGoal);
	}
}

// ---------------------------------------------------------------------------
// Perception Timer (0.5s)
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::RunPerception()
{
	if (CurrentState == ENPCBrainState::Dead) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector MyLoc = GetOwner()->GetActorLocation();

	// --- Detect nearby actors via sphere overlap ---
	DetectedActors.Empty();

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(GetOwner());

	TArray<AActor*> FoundActors;
	UKismetSystemLibrary::SphereOverlapActors(
		World, MyLoc, PerceptionRadius, ObjectTypes,
		APawn::StaticClass(), IgnoredActors, FoundActors);

	for (AActor* Actor : FoundActors)
	{
		DetectedActors.Add(Actor);
	}

	// --- Update memory with threats and allies ---
	UpdateThreatState();

	// --- Update location flags for GOAP world state ---
	UpdateLocationFlags();

	// --- Tick needs ---
	if (NeedSystem)
	{
		NeedSystem->TickNeeds(0.5f); // Perception runs every 0.5s

		// Update safety based on surroundings
		const float SafetyValue = CalculateSafetyFromSurroundings();
		NeedSystem->SetSafety(SafetyValue);
	}

	// --- Clean up old threat memories ---
	if (Memory)
	{
		Memory->ForgetOldThreats(ThreatMemoryDuration);
		Memory->AddVisitedBreadcrumb(MyLoc);
	}
}

void UAoCNPCBrainV2::UpdateLocationFlags()
{
	if (!Memory || !GoalPlanner) return;

	const FVector MyLoc = GetOwner()->GetActorLocation();
	TMap<FName, bool>& ExtState = GoalPlanner->ExternalWorldState;

	// Check proximity to each known location
	auto CheckLocation = [&](FName LocName, FName FlagName)
	{
		if (Memory->KnowsLocation(LocName))
		{
			const FVector LocPos = Memory->GetLocationOf(LocName);
			const bool bNear = FVector::Dist(MyLoc, LocPos) < LocationArrivalThreshold;
			ExtState.Add(FlagName, bNear);
		}
		else
		{
			ExtState.Add(FlagName, false);
		}
	};

	CheckLocation(FName("Mine"), FName("AtMine"));
	CheckLocation(FName("Forge"), FName("AtForge"));
	CheckLocation(FName("FishingSpot"), FName("AtWater"));
	CheckLocation(FName("Water"), FName("AtWater")); // alias
	CheckLocation(FName("Home"), FName("AtHome"));
	CheckLocation(FName("Forest"), FName("AtForest"));
	CheckLocation(FName("Market"), FName("AtMarket"));
	CheckLocation(FName("BuildSite"), FName("AtBuildSite"));
}

void UAoCNPCBrainV2::UpdateThreatState()
{
	if (!Memory || !GoalPlanner) return;

	bool bInCombat = false;
	bool bInDanger = false;
	bool bTargetVisible = false;

	for (const TObjectPtr<AActor>& Det : DetectedActors)
	{
		if (!Det) continue;

		// Check if this is a player
		APawn* PlayerPawn = Cast<APawn>(Det.Get());
		if (PlayerPawn && PlayerPawn->IsPlayerControlled())
		{
			const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Det->GetActorLocation());

			// Check hostility
			ACharacter* PlayerChar = Cast<ACharacter>(PlayerPawn);
			const float Hostility = PlayerChar ? Memory->GetPlayerHostility(PlayerChar) : 0.f;

			if (Hostility < -30.f)
			{
				// This player is hostile
				Memory->RememberThreat(Det.Get(), FMath::Abs(Hostility));
				bInDanger = true;

				if (Dist < 500.f)
				{
					bInCombat = true;
				}
			}

			bTargetVisible = true;
			CurrentTarget = Det.Get();
		}
		else
		{
			// NPC or creature — check if enemy faction
			// For now, treat any NPC that has attacked us as a threat
		}
	}

	// Also check if we have active threats in memory
	if (Memory->HasActiveThreats())
	{
		bInDanger = true;
		AActor* ThreatActor = Memory->GetHighestThreatActor();
		if (ThreatActor && DetectedActors.Contains(ThreatActor))
		{
			bTargetVisible = true;
			bInCombat = true;
			CurrentTarget = ThreatActor;
		}
	}

	// Update external world state
	TMap<FName, bool>& ExtState = GoalPlanner->ExternalWorldState;
	ExtState.Add(FName("InCombat"), bInCombat);
	ExtState.Add(FName("InDanger"), bInDanger);
	ExtState.Add(FName("TargetVisible"), bTargetVisible);
	ExtState.Add(FName("TargetDead"), false);

	// Update brain state for combat
	if (bInCombat && CurrentState != ENPCBrainState::Fighting &&
	    CurrentState != ENPCBrainState::Dead && CurrentState != ENPCBrainState::Fleeing)
	{
		// Check personality for fight or flight
		const float ThreatLevel = Memory->GetHighestThreatLevel();
		if (Personality && Personality->ShouldFlee(ThreatLevel))
		{
			SetBrainState(ENPCBrainState::Fleeing);
		}
		else
		{
			SetBrainState(ENPCBrainState::Fighting);
		}
	}
}

float UAoCNPCBrainV2::CalculateSafetyFromSurroundings() const
{
	float Safety = 100.f;

	if (!Memory) return Safety;

	// Reduce safety based on nearby threats
	for (const TObjectPtr<AActor>& Det : DetectedActors)
	{
		if (!Det) continue;

		const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Det->GetActorLocation());
		const float DistRatio = FMath::Clamp(Dist / PerceptionRadius, 0.f, 1.f);

		// Closer threats reduce safety more
		APawn* Pawn = Cast<APawn>(Det.Get());
		if (Pawn && Pawn->IsPlayerControlled())
		{
			ACharacter* PlayerChar = Cast<ACharacter>(Pawn);
			const float Hostility = PlayerChar ? Memory->GetPlayerHostility(PlayerChar) : 0.f;
			if (Hostility < 0.f)
			{
				Safety -= (1.f - DistRatio) * FMath::Abs(Hostility) * 0.5f;
			}
		}
	}

	// Active threats in memory also reduce safety
	if (Memory->HasActiveThreats())
	{
		Safety -= Memory->GetHighestThreatLevel() * 0.3f;
	}

	return FMath::Clamp(Safety, 0.f, 100.f);
}

// ---------------------------------------------------------------------------
// Planning Timer (1.0s)
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::RunPlanning()
{
	if (CurrentState == ENPCBrainState::Dead) return;
	if (!GoalPlanner) return;

	// --- Check for critical need interruptions ---
	if (NeedSystem && NeedSystem->IsAnyNeedCritical())
	{
		ENPCNeed CriticalNeed = NeedSystem->GetMostUrgentNeed();

		// Map critical need to goal
		ENPCGoal InterruptGoal = ENPCGoal::Idle;
		switch (CriticalNeed)
		{
		case ENPCNeed::Hunger:  InterruptGoal = ENPCGoal::SatisfyHunger; break;
		case ENPCNeed::Energy:  InterruptGoal = ENPCGoal::RestoreEnergy; break;
		case ENPCNeed::Safety:  InterruptGoal = ENPCGoal::SeekSafety;    break;
		default: break;
		}

		if (InterruptGoal != ENPCGoal::Idle && InterruptGoal != CurrentGoal)
		{
			if (GoalPlanner->CanInterruptCurrentPlan(InterruptGoal))
			{
				UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s: Critical need interruption! %d → %d"),
					*GetOwner()->GetName(), static_cast<int32>(CurrentGoal), static_cast<int32>(InterruptGoal));

				// Save current plan for resumption
				if (GoalPlanner->HasActivePlan())
				{
					GoalPlanner->InterruptedPlan = GoalPlanner->CurrentPlan;
					GoalPlanner->InterruptedPlan.InterruptedAtIndex = GoalPlanner->CurrentPlan.CurrentActionIndex;
					GoalPlanner->InterruptedPlan.OriginalGoal = GoalPlanner->CurrentPlan.Goal;
					GoalPlanner->bHasInterruptedPlan = true;

					SetBrainState(ENPCBrainState::Interrupted);
				}

				// Create new plan for the interrupt goal
				GoalPlanner->CurrentPlan = GoalPlanner->CreatePlan(InterruptGoal);
				CurrentGoal = InterruptGoal;
				bWaitingForActionComplete = false;
				return;
			}
		}
	}

	// --- If no active plan or plan is complete, create a new one ---
	if (!GoalPlanner->HasActivePlan())
	{
		// Check if we should resume an interrupted plan
		if (GoalPlanner->bHasInterruptedPlan)
		{
			GoalPlanner->ResumePlan();
			CurrentGoal = GoalPlanner->CurrentPlan.Goal;
			UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s: Resuming interrupted plan for goal %d"),
				*GetOwner()->GetName(), static_cast<int32>(CurrentGoal));
		}
		else
		{
			// Select the best goal and plan for it
			ENPCGoal BestGoal = GoalPlanner->SelectBestGoal();
			FNPCPlan NewPlan = GoalPlanner->CreatePlan(BestGoal);

			if (NewPlan.IsValid())
			{
				GoalPlanner->CurrentPlan = NewPlan;
				CurrentGoal = BestGoal;
				GoalPlanner->OnPlanChanged.Broadcast(BestGoal);

				UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s: New plan for goal %d with %d actions"),
					*GetOwner()->GetName(), static_cast<int32>(BestGoal), NewPlan.Actions.Num());
			}
			else
			{
				// No valid plan found — idle
				CurrentGoal = ENPCGoal::Idle;
				SetBrainState(ENPCBrainState::Planning);
			}
		}

		bWaitingForActionComplete = false;
	}
}

// ---------------------------------------------------------------------------
// Execution Timer (0.5s)
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::RunExecution()
{
	if (CurrentState == ENPCBrainState::Dead) return;

	// If in active combat, run combat turn directly
	if (CurrentState == ENPCBrainState::Fighting && CurrentTarget)
	{
		ExecuteCombatTurn();
		return;
	}

	if (!GoalPlanner || !GoalPlanner->HasActivePlan()) return;

	// If waiting for animation to finish, check if it's done
	if (bWaitingForActionComplete)
	{
		if (AnimPlayer && AnimPlayer->IsPlaying())
		{
			// Still animating, wait
			return;
		}

		// Action complete — advance plan
		bWaitingForActionComplete = false;
		GoalPlanner->CurrentPlan.CurrentActionIndex++;

		if (GoalPlanner->CurrentPlan.IsComplete())
		{
			UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s: Plan complete for goal %d"),
				*GetOwner()->GetName(), static_cast<int32>(CurrentGoal));

			// Apply need satisfaction from the plan's actions
			SetBrainState(ENPCBrainState::Planning);
			return;
		}
	}

	// Execute the current action
	const FNPCAction CurrentAction = GoalPlanner->CurrentPlan.GetCurrentAction();
	if (CurrentAction.IsValid())
	{
		ExecuteAction(CurrentAction);
	}
}

// ---------------------------------------------------------------------------
// Action execution dispatch
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteAction(const FNPCAction& Action)
{
	const FString ActionStr = Action.ActionID.ToString();

	UE_LOG(LogTemp, Verbose, TEXT("[BrainV2] %s executing: %s"),
		*GetOwner()->GetName(), *ActionStr);

	// Dispatch based on action type
	if (ActionStr.StartsWith(TEXT("MoveTo_")))
	{
		ExecuteMoveToAction(Action);
	}
	else if (ActionStr == TEXT("Mine_Ore") || ActionStr == TEXT("Chop_Wood") ||
	         ActionStr == TEXT("Quarry_Stone") || ActionStr == TEXT("Fish"))
	{
		ExecuteGatherAction(Action);
	}
	else if (ActionStr == TEXT("Cook") || ActionStr == TEXT("Smelt") ||
	         ActionStr == TEXT("Craft_Weapon") || ActionStr == TEXT("Craft_Armor") ||
	         ActionStr == TEXT("Build"))
	{
		ExecuteCraftAction(Action);
	}
	else if (ActionStr == TEXT("Attack_Melee") || ActionStr == TEXT("Attack_Ranged") ||
	         ActionStr == TEXT("Cast_Spell") || ActionStr == TEXT("Block") ||
	         ActionStr == TEXT("Dodge"))
	{
		ExecuteCombatAction(Action);
	}
	else if (ActionStr == TEXT("Eat") || ActionStr == TEXT("EatCooked") ||
	         ActionStr == TEXT("DrinkPotion"))
	{
		ExecuteConsumeAction(Action);
	}
	else if (ActionStr == TEXT("Rest"))
	{
		ExecuteRestAction(Action);
	}
	else if (ActionStr == TEXT("Socialize") || ActionStr == TEXT("Trade"))
	{
		ExecuteSocialAction(Action);
	}
	else if (ActionStr == TEXT("Flee") || ActionStr == TEXT("Hide") ||
	         ActionStr == TEXT("Peek") || ActionStr == TEXT("Equip_Gear") ||
	         ActionStr == TEXT("LootCorpse"))
	{
		ExecuteGenericAction(Action);
	}
	else
	{
		// Unknown action — just play a generic animation and wait
		ExecuteGenericAction(Action);
	}
}

// ---------------------------------------------------------------------------
// MoveTo actions
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteMoveToAction(const FNPCAction& Action)
{
	if (!Memory) return;

	// Extract location name from "MoveTo_XXX"
	const FString ActionStr = Action.ActionID.ToString();
	const FString LocationName = ActionStr.RightChop(7); // Remove "MoveTo_"
	const FName LocName = FName(*LocationName);

	FVector TargetLocation;
	if (Memory->KnowsLocation(LocName))
	{
		TargetLocation = Memory->GetLocationOf(LocName);
	}
	else
	{
		// Try alternate name
		TargetLocation = Memory->GetNearestKnownLocation(LocName, GetOwner()->GetActorLocation());
	}

	// Check if we've arrived
	if (IsNearLocation(TargetLocation))
	{
		// Arrived at destination — mark action complete
		bWaitingForActionComplete = false;
		GoalPlanner->CurrentPlan.CurrentActionIndex++;
		SetBrainState(ENPCBrainState::Executing);
		return;
	}

	// Still traveling
	SetBrainState(ENPCBrainState::Traveling);

	// Use nav system for pathfinding
	ACharacter* CharOwner = Cast<ACharacter>(GetOwner());
	if (CharOwner && CharOwner->GetController())
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(CharOwner->GetController(), TargetLocation);
	}
	else
	{
		MoveToward(TargetLocation, 300.f);
	}

	// Play walk animation
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("movement"), TEXT("standing"), { TEXT("walk") });
	}
}

// ---------------------------------------------------------------------------
// Gathering actions
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteGatherAction(const FNPCAction& Action)
{
	SetBrainState(ENPCBrainState::Gathering);

	// Play gathering animation
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		TArray<FString> Tags;
		for (const FName& Tag : Action.AnimTags)
		{
			Tags.Add(Tag.ToString());
		}

		const FString Category = Action.AnimCategory.IsNone() ? TEXT("crafting") : Action.AnimCategory.ToString();
		PlayFromLab(Category, TEXT("standing"), Tags);
		bWaitingForActionComplete = true;

		// Simulate gathering: add items to inventory after animation
		if (Inventory)
		{
			const FString ActionStr = Action.ActionID.ToString();
			FNPCItem GatheredItem;
			GatheredItem.Quantity = 1;
			GatheredItem.Category = EItemCategory::Material;
			GatheredItem.Weight = 2.f;

			if (ActionStr == TEXT("Mine_Ore"))
			{
				GatheredItem.ItemID = FName("IronOre");
				GatheredItem.DisplayName = TEXT("Iron Ore");
			}
			else if (ActionStr == TEXT("Chop_Wood"))
			{
				GatheredItem.ItemID = FName("Lumber");
				GatheredItem.DisplayName = TEXT("Lumber");
			}
			else if (ActionStr == TEXT("Quarry_Stone"))
			{
				GatheredItem.ItemID = FName("Stone");
				GatheredItem.DisplayName = TEXT("Cut Stone");
			}
			else if (ActionStr == TEXT("Fish"))
			{
				GatheredItem.ItemID = FName("RawFish");
				GatheredItem.DisplayName = TEXT("Raw Fish");
				GatheredItem.Category = EItemCategory::Food;
				GatheredItem.Stats.Add(FName("Healing"), 15.f);
			}

			Inventory->AddItem(GatheredItem);
		}

		// Gathering is purposeful work
		if (NeedSystem)
		{
			NeedSystem->SatisfyNeed(ENPCNeed::Purpose, 10.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Crafting actions
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteCraftAction(const FNPCAction& Action)
{
	SetBrainState(ENPCBrainState::Crafting);

	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		TArray<FString> Tags;
		for (const FName& Tag : Action.AnimTags)
		{
			Tags.Add(Tag.ToString());
		}

		const FString Category = Action.AnimCategory.IsNone() ? TEXT("crafting") : Action.AnimCategory.ToString();
		PlayFromLab(Category, TEXT("standing"), Tags);
		bWaitingForActionComplete = true;

		// Simulate crafting results
		if (Inventory)
		{
			const FString ActionStr = Action.ActionID.ToString();

			if (ActionStr == TEXT("Cook"))
			{
				// Consume raw food, produce cooked food
				Inventory->RemoveItem(FName("RawFish"), 1);
				FNPCItem CookedItem;
				CookedItem.ItemID = FName("CookedFish");
				CookedItem.DisplayName = TEXT("Cooked Fish");
				CookedItem.Category = EItemCategory::Food;
				CookedItem.Quantity = 1;
				CookedItem.Weight = 1.f;
				CookedItem.Stats.Add(FName("Healing"), 40.f);
				CookedItem.Stats.Add(FName("Cooked"), 1.f);
				Inventory->AddItem(CookedItem);
			}
			else if (ActionStr == TEXT("Smelt"))
			{
				Inventory->RemoveItem(FName("IronOre"), 1);
				FNPCItem IngotItem;
				IngotItem.ItemID = FName("IronIngot");
				IngotItem.DisplayName = TEXT("Iron Ingot");
				IngotItem.Category = EItemCategory::Material;
				IngotItem.Quantity = 1;
				IngotItem.Weight = 3.f;
				Inventory->AddItem(IngotItem);
			}
			else if (ActionStr == TEXT("Craft_Weapon"))
			{
				Inventory->RemoveItem(FName("IronIngot"), 2);
				FNPCItem WeaponItem;
				WeaponItem.ItemID = FName("IronSword");
				WeaponItem.DisplayName = TEXT("Iron Sword");
				WeaponItem.Category = EItemCategory::Weapon;
				WeaponItem.Quantity = 1;
				WeaponItem.Weight = 4.f;
				WeaponItem.Stats.Add(FName("Damage"), 25.f);
				WeaponItem.PreferredSlot = EEquipSlot::MainHand;
				Inventory->AddItem(WeaponItem);
			}
			else if (ActionStr == TEXT("Craft_Armor"))
			{
				Inventory->RemoveItem(FName("IronIngot"), 3);
				FNPCItem ArmorItem;
				ArmorItem.ItemID = FName("IronChestplate");
				ArmorItem.DisplayName = TEXT("Iron Chestplate");
				ArmorItem.Category = EItemCategory::Armor;
				ArmorItem.Quantity = 1;
				ArmorItem.Weight = 8.f;
				ArmorItem.Stats.Add(FName("Armor"), 20.f);
				ArmorItem.PreferredSlot = EEquipSlot::Chest;
				Inventory->AddItem(ArmorItem);
			}
			else if (ActionStr == TEXT("Build"))
			{
				Inventory->RemoveItem(FName("Lumber"), 5);
				Inventory->RemoveItem(FName("Stone"), 3);
				UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s built a structure!"), *GetOwner()->GetName());
			}
		}

		// Crafting is purposeful
		if (NeedSystem)
		{
			NeedSystem->SatisfyNeed(ENPCNeed::Purpose, 15.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Combat actions
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteCombatAction(const FNPCAction& Action)
{
	SetBrainState(ENPCBrainState::Fighting);
	ExecuteCombatTurn();
}

void UAoCNPCBrainV2::ExecuteCombatTurn()
{
	if (!CurrentTarget || CurrentState == ENPCBrainState::Dead) return;

	const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());

	// Check if target is dead
	// (We don't have direct access, so check if target is still valid and in range)
	if (!IsValid(CurrentTarget.Get()))
	{
		// Target gone
		if (GoalPlanner)
		{
			GoalPlanner->ExternalWorldState.Add(FName("TargetDead"), true);
			GoalPlanner->ExternalWorldState.Add(FName("InCombat"), false);
		}
		CurrentTarget = nullptr;
		SetBrainState(ENPCBrainState::Planning);
		return;
	}

	// If far, run toward target
	if (Dist > 300.f)
	{
		ACharacter* CharOwner = Cast<ACharacter>(GetOwner());
		if (CharOwner && CharOwner->GetController())
		{
			UAIBlueprintHelperLibrary::SimpleMoveToLocation(CharOwner->GetController(), CurrentTarget->GetActorLocation());
		}
		else
		{
			MoveToward(CurrentTarget->GetActorLocation(), 600.f);
		}

		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			TArray<FString> Tags = WeaponTags;
			Tags.Add(TEXT("run"));
			PlayFromLab(TEXT("movement"), TEXT("standing"), Tags);
		}
		return;
	}

	// Face the target
	const FVector Dir = GetDirectionTo(CurrentTarget.Get());
	GetOwner()->SetActorRotation(Dir.Rotation());

	// Attack if not currently animating
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		// Check if we should use magic
		const bool bUseMagic = Personality && Personality->ShouldUseMagic() && KnownSpells.Num() > 0;

		if (bUseMagic)
		{
			CastBestSpell(CurrentTarget.Get());
			return;
		}

		// Decide action: attack (60%), block/dodge (20%), taunt (20%)
		const int32 Roll = FMath::RandRange(0, 9);

		if (Roll < 6) // 60% attack
		{
			TArray<FString> Tags = WeaponTags;
			Tags.Add(TEXT("attack"));

			// Variety: don't repeat last attack
			if (AttackVarietyCounter > 2)
			{
				Tags.Add(TEXT("combo"));
				AttackVarietyCounter = 0;
			}
			else
			{
				AttackVarietyCounter++;
			}

			if (AnimLab)
			{
				FAoCAnimationEntry Entry = AnimLab->FindBestMatch(TEXT("combat_melee"), TEXT("standing"), Tags);
				if (Entry.EntryName == LastAttackName && AnimLab->QueryByCategory(TEXT("combat_melee")).Num() > 1)
				{
					Entry = AnimLab->GetRandomFromCategory(TEXT("combat_melee"));
				}
				LastAttackName = Entry.EntryName;
				if (AnimPlayer) AnimPlayer->PlayEntry(Entry);
			}
		}
		else if (Roll < 8) // 20% block/dodge
		{
			TArray<FString> Tags = WeaponTags;
			Tags.Add(FMath::RandBool() ? TEXT("block") : TEXT("dodge"));
			PlayFromLab(TEXT("combat_melee"), TEXT("standing"), Tags);
		}
		else // 20% taunt
		{
			PlayFromLab(TEXT("social"), TEXT("standing"), { TEXT("taunt"), TEXT("aggressive") });
		}
	}
}

void UAoCNPCBrainV2::CastBestSpell(AActor* Target)
{
	if (KnownSpells.Num() == 0 || !AnimLab || !AnimPlayer) return;

	// Pick a random known spell (in a real system, we'd pick by damage/mana cost)
	const int32 SpellIdx = FMath::RandRange(0, KnownSpells.Num() - 1);
	const FName SpellID = KnownSpells[SpellIdx];

	// Build tags from magic schools
	TArray<FString> Tags;
	Tags.Add(TEXT("cast"));
	for (const FName& School : MagicSchools)
	{
		Tags.Add(School.ToString());
	}

	FAoCAnimationEntry SpellAnim = AnimLab->FindBestMatch(TEXT("magic"), TEXT("standing"), Tags);
	AnimPlayer->PlayEntry(SpellAnim);

	UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s casts spell: %s"),
		*GetOwner()->GetName(), *SpellID.ToString());
}

// ---------------------------------------------------------------------------
// Consume actions (eat, drink)
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteConsumeAction(const FNPCAction& Action)
{
	const FString ActionStr = Action.ActionID.ToString();

	if (Inventory && NeedSystem)
	{
		if (ActionStr == TEXT("Eat") || ActionStr == TEXT("EatCooked"))
		{
			const FNPCItem BestFood = Inventory->GetBestFood();
			if (BestFood.IsValid())
			{
				const float Healing = Inventory->ConsumeItem(BestFood.ItemID);
				NeedSystem->SatisfyNeed(ENPCNeed::Hunger, Action.SatisfiesAmount > 0.f ? Action.SatisfiesAmount : 40.f);
				UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s eats %s (+%.0f hunger)"),
					*GetOwner()->GetName(), *BestFood.DisplayName, Action.SatisfiesAmount);
			}
		}
		else if (ActionStr == TEXT("DrinkPotion"))
		{
			const FNPCItem BestPotion = Inventory->GetBestPotion();
			if (BestPotion.IsValid())
			{
				Inventory->ConsumeItem(BestPotion.ItemID);
				UE_LOG(LogTemp, Log, TEXT("[BrainV2] %s drinks %s"),
					*GetOwner()->GetName(), *BestPotion.DisplayName);
			}
		}
	}

	// Play consume animation
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("misc"), TEXT("standing"), { TEXT("eat"), TEXT("drink") });
		bWaitingForActionComplete = true;
	}
}

// ---------------------------------------------------------------------------
// Social actions
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteSocialAction(const FNPCAction& Action)
{
	SetBrainState(ENPCBrainState::Socializing);

	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		TArray<FString> Tags;
		for (const FName& Tag : Action.AnimTags)
		{
			Tags.Add(Tag.ToString());
		}

		PlayFromLab(TEXT("social"), TEXT("standing"), Tags);
		bWaitingForActionComplete = true;

		// Satisfy social need
		if (NeedSystem && Action.SatisfiesAmount > 0.f)
		{
			NeedSystem->SatisfyNeed(ENPCNeed::Social, Action.SatisfiesAmount);
		}
	}
}

// ---------------------------------------------------------------------------
// Rest action
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteRestAction(const FNPCAction& Action)
{
	SetBrainState(ENPCBrainState::Resting);

	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("idle"), TEXT("standing"), { TEXT("rest"), TEXT("sleep"), TEXT("sit") });
		bWaitingForActionComplete = true;

		// Satisfy energy
		if (NeedSystem)
		{
			NeedSystem->SatisfyNeed(ENPCNeed::Energy, Action.SatisfiesAmount > 0.f ? Action.SatisfiesAmount : 50.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Generic action execution
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::ExecuteGenericAction(const FNPCAction& Action)
{
	const FString ActionStr = Action.ActionID.ToString();

	// Handle special actions
	if (ActionStr == TEXT("Equip_Gear"))
	{
		if (Inventory)
		{
			const FNPCItem BestWeapon = Inventory->GetBestWeapon();
			if (BestWeapon.IsValid())
			{
				Inventory->EquipItem(BestWeapon.ItemID, EEquipSlot::MainHand);
			}

			// Also equip armor if available
			TArray<FNPCItem> ArmorItems = Inventory->GetItemsByCategory(EItemCategory::Armor);
			for (const FNPCItem& ArmorItem : ArmorItems)
			{
				if (ArmorItem.PreferredSlot != EEquipSlot::MAX)
				{
					Inventory->EquipItem(ArmorItem.ItemID, ArmorItem.PreferredSlot);
				}
			}
		}
		// No animation needed — advance immediately
		bWaitingForActionComplete = false;
		GoalPlanner->CurrentPlan.CurrentActionIndex++;
		return;
	}

	if (ActionStr == TEXT("Flee"))
	{
		SetBrainState(ENPCBrainState::Fleeing);

		// Run AWAY from threat
		if (CurrentTarget)
		{
			const FVector MyLoc = GetOwner()->GetActorLocation();
			const FVector ThreatLoc = CurrentTarget->GetActorLocation();
			const FVector FleeDir = (MyLoc - ThreatLoc).GetSafeNormal();
			const FVector FleeTarget = MyLoc + FleeDir * 2000.f;

			ACharacter* CharOwner = Cast<ACharacter>(GetOwner());
			if (CharOwner && CharOwner->GetController())
			{
				UAIBlueprintHelperLibrary::SimpleMoveToLocation(CharOwner->GetController(), FleeTarget);
			}
			else
			{
				MoveToward(FleeTarget, 600.f);
			}
		}
		else if (Memory && Memory->KnowsLocation(FName("Home")))
		{
			// No target — flee home
			const FVector HomeLoc = Memory->GetLocationOf(FName("Home"));
			MoveToward(HomeLoc, 600.f);
		}

		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("movement"), TEXT("standing"), { TEXT("run"), TEXT("sprint") });
		}

		// Check if we're far enough
		if (CurrentTarget)
		{
			const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
			if (Dist > PerceptionRadius)
			{
				bWaitingForActionComplete = false;
				GoalPlanner->CurrentPlan.CurrentActionIndex++;
				SetBrainState(ENPCBrainState::Planning);
			}
		}
		return;
	}

	if (ActionStr == TEXT("Hide"))
	{
		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("misc"), TEXT("standing"), { TEXT("crouch"), TEXT("hide") });
			bWaitingForActionComplete = true;
		}
		return;
	}

	if (ActionStr == TEXT("Peek"))
	{
		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("misc"), TEXT("standing"), { TEXT("peek"), TEXT("look") });
			bWaitingForActionComplete = true;
		}
		return;
	}

	if (ActionStr == TEXT("LootCorpse"))
	{
		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("misc"), TEXT("standing"), { TEXT("loot"), TEXT("pickup") });
			bWaitingForActionComplete = true;

			// Simulate looting — add random loot
			if (Inventory)
			{
				FNPCItem LootItem;
				LootItem.ItemID = FName("Loot");
				LootItem.DisplayName = TEXT("Looted Goods");
				LootItem.Category = EItemCategory::CraftedGood;
				LootItem.Quantity = 1;
				LootItem.Weight = 2.f;
				Inventory->AddItem(LootItem);

				if (NeedSystem)
				{
					NeedSystem->SatisfyNeed(ENPCNeed::Wealth, 10.f);
				}
			}
		}
		return;
	}

	// Fallback: play generic animation from action's category/tags
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		TArray<FString> Tags;
		for (const FName& Tag : Action.AnimTags)
		{
			Tags.Add(Tag.ToString());
		}

		FString Category = Action.AnimCategory.IsNone() ? TEXT("idle") : Action.AnimCategory.ToString();
		PlayFromLab(Category, TEXT("standing"), Tags);
		bWaitingForActionComplete = true;
	}
}

// ---------------------------------------------------------------------------
// Animation helpers
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::PlayFromLab(const FString& Category, const FString& BodyState, const TArray<FString>& Tags)
{
	if (!AnimLab || !AnimPlayer) return;

	FAoCAnimationEntry Entry = AnimLab->FindBestMatch(Category, BodyState, Tags);
	AnimPlayer->PlayEntry(Entry);
}

void UAoCNPCBrainV2::PlayFromLabFName(FName Category, const TArray<FName>& Tags)
{
	TArray<FString> TagStrings;
	for (const FName& Tag : Tags)
	{
		TagStrings.Add(Tag.ToString());
	}
	PlayFromLab(Category.ToString(), TEXT("standing"), TagStrings);
}

// ---------------------------------------------------------------------------
// Movement helpers
// ---------------------------------------------------------------------------

void UAoCNPCBrainV2::MoveToward(const FVector& Location, float Speed)
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char) return;

	UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
	if (!CMC) return;

	const FVector MyLoc = Char->GetActorLocation();
	FVector Dir = (Location - MyLoc).GetSafeNormal2D();

	if (!Dir.IsNearlyZero())
	{
		Char->SetActorRotation(Dir.Rotation());
	}

	CMC->MaxWalkSpeed = Speed;
	Char->AddMovementInput(Dir, 1.0f);
}

bool UAoCNPCBrainV2::IsNearLocation(const FVector& Location) const
{
	return FVector::Dist2D(GetOwner()->GetActorLocation(), Location) < LocationArrivalThreshold;
}

FVector UAoCNPCBrainV2::GetDirectionTo(const AActor* Target) const
{
	if (!Target) return FVector::ForwardVector;
	return (Target->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
}
