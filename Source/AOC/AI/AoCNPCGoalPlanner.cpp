// AoCNPCGoalPlanner.cpp — GOAP planning system implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCGoalPlanner.h"
#include "AoCNPCNeedSystem.h"
#include "AoCNPCMemory.h"
#include "AoCNPCInventory.h"
#include "AoCNPCPersonality.h"
#include "Algo/Sort.h"

UAoCNPCGoalPlanner::UAoCNPCGoalPlanner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAoCNPCGoalPlanner::BeginPlay()
{
	Super::BeginPlay();
	RegisterBuiltInActions();
}

// ---------------------------------------------------------------------------
// Action helpers
// ---------------------------------------------------------------------------

FNPCAction UAoCNPCGoalPlanner::MakeAction(
	FName ID,
	const TMap<FName, bool>& Pre,
	const TMap<FName, bool>& Eff,
	float Cost,
	FName AnimCat,
	TArray<FName> Tags,
	float Dur,
	bool Interruptible,
	FName ReqLoc,
	FName SatNeed,
	float SatAmt) const
{
	FNPCAction A;
	A.ActionID = ID;
	A.Preconditions = Pre;
	A.Effects = Eff;
	A.Cost = Cost;
	A.AnimCategory = AnimCat;
	A.AnimTags = Tags;
	A.Duration = Dur;
	A.bInterruptibleByNeeds = Interruptible;
	A.RequiresLocation = ReqLoc;
	A.SatisfiesNeed = SatNeed;
	A.SatisfiesAmount = SatAmt;
	return A;
}

FNPCAction UAoCNPCGoalPlanner::MakeMoveToAction(FName LocationName) const
{
	FName ActionName = FName(*FString::Printf(TEXT("MoveTo_%s"), *LocationName.ToString()));
	FName EffKey = FName(*FString::Printf(TEXT("At%s"), *LocationName.ToString()));

	TMap<FName, bool> Pre;
	TMap<FName, bool> Eff;
	Eff.Add(EffKey, true);

	// Moving to a new location clears other location flags via the world state rebuild,
	// but for planning purposes we just set the target location.
	return MakeAction(ActionName, Pre, Eff, 5.f, FName("movement"), { FName("walk") }, 0.f, true);
}

// ---------------------------------------------------------------------------
// Built-in action library (30 actions)
// ---------------------------------------------------------------------------

void UAoCNPCGoalPlanner::RegisterBuiltInActions()
{
	// --- Movement actions ---
	// 1. MoveTo_Mine
	RegisterAction(MakeMoveToAction(FName("Mine")));
	// 2. MoveTo_Water
	RegisterAction(MakeMoveToAction(FName("Water")));
	// 3. MoveTo_Forge
	RegisterAction(MakeMoveToAction(FName("Forge")));
	// 4. MoveTo_Forest
	RegisterAction(MakeMoveToAction(FName("Forest")));
	// 5. MoveTo_Home
	RegisterAction(MakeMoveToAction(FName("Home")));
	// 6. MoveTo_BuildSite
	RegisterAction(MakeMoveToAction(FName("BuildSite")));
	// 7. MoveTo_Market
	RegisterAction(MakeMoveToAction(FName("Market")));

	// --- Gathering actions ---

	// 8. Mine_Ore: Pre={AtMine}, Eff={HasOre}, Cost=10
	{
		TMap<FName, bool> Pre; Pre.Add(FName("AtMine"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasOre"), true);
		RegisterAction(MakeAction(FName("Mine_Ore"), Pre, Eff, 10.f,
			FName("crafting"), { FName("mining") }, 8.f, true, FName("Mine")));
	}

	// 9. Chop_Wood: Pre={AtForest}, Eff={HasLumber}, Cost=10
	{
		TMap<FName, bool> Pre; Pre.Add(FName("AtForest"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasLumber"), true);
		RegisterAction(MakeAction(FName("Chop_Wood"), Pre, Eff, 10.f,
			FName("crafting"), { FName("chopping") }, 8.f, true, FName("Forest")));
	}

	// 10. Quarry_Stone: Pre={AtMine}, Eff={HasStone}, Cost=10
	{
		TMap<FName, bool> Pre; Pre.Add(FName("AtMine"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasStone"), true);
		RegisterAction(MakeAction(FName("Quarry_Stone"), Pre, Eff, 10.f,
			FName("crafting"), { FName("mining"), FName("quarry") }, 8.f, true, FName("Mine")));
	}

	// 11. Fish: Pre={AtWater}, Eff={HasFood}, Cost=8
	{
		TMap<FName, bool> Pre; Pre.Add(FName("AtWater"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasFood"), true);
		RegisterAction(MakeAction(FName("Fish"), Pre, Eff, 8.f,
			FName("crafting"), { FName("fishing") }, 10.f, true, FName("Water")));
	}

	// 12. Cook: Pre={HasFood}, Eff={HasCookedFood}, Cost=5
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasFood"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasCookedFood"), true);
		RegisterAction(MakeAction(FName("Cook"), Pre, Eff, 5.f,
			FName("crafting"), { FName("cooking") }, 6.f, true));
	}

	// 13. Eat: Pre={HasFood OR HasCookedFood}, Eff={IsFed}, Cost=2
	// We encode this as just requiring HasFood since cooked food implies food
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasFood"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("IsFed"), true);
		RegisterAction(MakeAction(FName("Eat"), Pre, Eff, 2.f,
			FName("misc"), { FName("eat") }, 3.f, false, NAME_None,
			FName("Hunger"), 40.f));
	}

	// 13b. Eat cooked food (cheaper, better)
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasCookedFood"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("IsFed"), true);
		RegisterAction(MakeAction(FName("EatCooked"), Pre, Eff, 1.f,
			FName("misc"), { FName("eat") }, 3.f, false, NAME_None,
			FName("Hunger"), 60.f));
	}

	// --- Crafting actions ---

	// 14. Smelt: Pre={HasOre, AtForge}, Eff={HasIngots}, Cost=8
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasOre"), true); Pre.Add(FName("AtForge"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasIngots"), true);
		RegisterAction(MakeAction(FName("Smelt"), Pre, Eff, 8.f,
			FName("crafting"), { FName("smithing"), FName("smelt") }, 10.f, true, FName("Forge")));
	}

	// 15. Craft_Weapon: Pre={HasIngots, AtForge}, Eff={HasWeapon}, Cost=15
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasIngots"), true); Pre.Add(FName("AtForge"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasWeapon"), true);
		RegisterAction(MakeAction(FName("Craft_Weapon"), Pre, Eff, 15.f,
			FName("crafting"), { FName("smithing"), FName("weapon") }, 12.f, true, FName("Forge")));
	}

	// 16. Craft_Armor: Pre={HasIngots, AtForge}, Eff={HasArmor}, Cost=15
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasIngots"), true); Pre.Add(FName("AtForge"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("HasArmor"), true);
		RegisterAction(MakeAction(FName("Craft_Armor"), Pre, Eff, 15.f,
			FName("crafting"), { FName("smithing"), FName("armor") }, 12.f, true, FName("Forge")));
	}

	// 17. Equip_Gear: Pre={HasWeapon OR HasArmor}, Eff={IsGeared}, Cost=2
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasWeapon"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("IsGeared"), true);
		RegisterAction(MakeAction(FName("Equip_Gear"), Pre, Eff, 2.f,
			FName("misc"), { FName("equip") }, 2.f, true));
	}

	// 18. Build: Pre={HasLumber, HasStone, AtBuildSite}, Eff={StructureBuilt}, Cost=20
	{
		TMap<FName, bool> Pre;
		Pre.Add(FName("HasLumber"), true);
		Pre.Add(FName("HasStone"), true);
		Pre.Add(FName("AtBuildSite"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("StructureBuilt"), true);
		RegisterAction(MakeAction(FName("Build"), Pre, Eff, 20.f,
			FName("crafting"), { FName("building"), FName("hammer") }, 15.f, true, FName("BuildSite")));
	}

	// 19. Rest: Pre={AtHome}, Eff={IsRested}, Cost=10
	{
		TMap<FName, bool> Pre; Pre.Add(FName("AtHome"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("IsRested"), true);
		RegisterAction(MakeAction(FName("Rest"), Pre, Eff, 10.f,
			FName("idle"), { FName("rest"), FName("sleep") }, 15.f, false, FName("Home"),
			FName("Energy"), 50.f));
	}

	// --- Combat actions ---

	// 20. Attack_Melee: Pre={IsGeared, TargetVisible, InCombat}, Cost=3
	{
		TMap<FName, bool> Pre;
		Pre.Add(FName("IsGeared"), true);
		Pre.Add(FName("TargetVisible"), true);
		Pre.Add(FName("InCombat"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Attack_Melee"), Pre, Eff, 3.f,
			FName("combat_melee"), { FName("attack") }, 0.f, false));
	}

	// 21. Attack_Ranged: Pre={HasWeapon, TargetVisible}, Cost=4
	{
		TMap<FName, bool> Pre;
		Pre.Add(FName("HasWeapon"), true);
		Pre.Add(FName("TargetVisible"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Attack_Ranged"), Pre, Eff, 4.f,
			FName("combat_ranged"), { FName("attack"), FName("ranged") }, 0.f, false));
	}

	// 22. Cast_Spell: Pre={TargetVisible, InCombat}, Cost=5
	{
		TMap<FName, bool> Pre;
		Pre.Add(FName("TargetVisible"), true);
		Pre.Add(FName("InCombat"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Cast_Spell"), Pre, Eff, 5.f,
			FName("magic"), { FName("cast") }, 0.f, false));
	}

	// 23. Block: Pre={InCombat}, Cost=2
	{
		TMap<FName, bool> Pre; Pre.Add(FName("InCombat"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Block"), Pre, Eff, 2.f,
			FName("combat_melee"), { FName("block") }, 0.f, false));
	}

	// 24. Dodge: Pre={InCombat}, Cost=2
	{
		TMap<FName, bool> Pre; Pre.Add(FName("InCombat"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Dodge"), Pre, Eff, 2.f,
			FName("movement"), { FName("dodge") }, 0.f, false));
	}

	// 25. Flee: Pre={InDanger}, Eff={AtHome}, Cost=3
	{
		TMap<FName, bool> Pre; Pre.Add(FName("InDanger"), true);
		TMap<FName, bool> Eff; Eff.Add(FName("AtHome"), true);
		RegisterAction(MakeAction(FName("Flee"), Pre, Eff, 3.f,
			FName("movement"), { FName("run"), FName("sprint") }, 0.f, false));
	}

	// 26. Hide: Pre={InDanger}, Cost=4
	{
		TMap<FName, bool> Pre; Pre.Add(FName("InDanger"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Hide"), Pre, Eff, 4.f,
			FName("misc"), { FName("crouch"), FName("hide") }, 10.f, false));
	}

	// 27. Peek: Pre=none, Eff={TargetVisible}, Cost=2
	{
		TMap<FName, bool> Pre;
		TMap<FName, bool> Eff; Eff.Add(FName("TargetVisible"), true);
		RegisterAction(MakeAction(FName("Peek"), Pre, Eff, 2.f,
			FName("misc"), { FName("peek"), FName("look") }, 3.f, true));
	}

	// 28. Socialize: Cost=3
	{
		TMap<FName, bool> Pre;
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Socialize"), Pre, Eff, 3.f,
			FName("emotes"), { FName("talk"), FName("wave") }, 5.f, true, NAME_None,
			FName("Social"), 30.f));
	}

	// 29. Trade: Pre={AtMarket}, Cost=5
	{
		TMap<FName, bool> Pre; Pre.Add(FName("AtMarket"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("Trade"), Pre, Eff, 5.f,
			FName("social"), { FName("trade"), FName("gesture") }, 8.f, true, FName("Market")));
	}

	// 30. DrinkPotion: Pre={HasPotion}, Cost=1
	{
		TMap<FName, bool> Pre; Pre.Add(FName("HasPotion"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("DrinkPotion"), Pre, Eff, 1.f,
			FName("misc"), { FName("drink") }, 2.f, false));
	}

	// 31. LootCorpse: Pre={TargetDead}, Cost=3
	{
		TMap<FName, bool> Pre; Pre.Add(FName("TargetDead"), true);
		TMap<FName, bool> Eff;
		RegisterAction(MakeAction(FName("LootCorpse"), Pre, Eff, 3.f,
			FName("misc"), { FName("loot"), FName("pickup") }, 4.f, true));
	}
}

// ---------------------------------------------------------------------------
// Register custom action
// ---------------------------------------------------------------------------

void UAoCNPCGoalPlanner::RegisterAction(const FNPCAction& Action)
{
	if (Action.IsValid())
	{
		ActionLibrary.Add(Action);
	}
}

// ---------------------------------------------------------------------------
// World state
// ---------------------------------------------------------------------------

TMap<FName, bool> UAoCNPCGoalPlanner::GetWorldState() const
{
	TMap<FName, bool> State;

	// --- Location flags (set by Brain based on proximity to known locations) ---
	// These are managed externally via ExternalWorldState
	// Default all location flags to false
	State.Add(FName("AtMine"), false);
	State.Add(FName("AtForge"), false);
	State.Add(FName("AtWater"), false);
	State.Add(FName("AtHome"), false);
	State.Add(FName("AtForest"), false);
	State.Add(FName("AtMarket"), false);
	State.Add(FName("AtBuildSite"), false);

	// --- Inventory flags ---
	if (Inventory)
	{
		State.Add(FName("HasOre"), Inventory->HasOre());
		State.Add(FName("HasIngots"), Inventory->HasIngots());
		State.Add(FName("HasLumber"), Inventory->HasLumber());
		State.Add(FName("HasStone"), Inventory->HasStone());
		State.Add(FName("HasFood"), Inventory->HasFood());
		State.Add(FName("HasCookedFood"), Inventory->HasCookedFood());
		State.Add(FName("HasWeapon"), Inventory->HasWeapon());
		State.Add(FName("HasArmor"), Inventory->HasArmor());
		State.Add(FName("HasPotion"), Inventory->HasPotion());
		State.Add(FName("HasFishingRod"), Inventory->HasTool(FName("FishingRod")));
		State.Add(FName("HasPickaxe"), Inventory->HasTool(FName("Pickaxe")));
		State.Add(FName("HasAxe"), Inventory->HasTool(FName("Axe")));
		State.Add(FName("HasHammer"), Inventory->HasTool(FName("Hammer")));
		State.Add(FName("IsGeared"), Inventory->IsSlotEquipped(EEquipSlot::MainHand));
	}
	else
	{
		State.Add(FName("HasOre"), false);
		State.Add(FName("HasIngots"), false);
		State.Add(FName("HasLumber"), false);
		State.Add(FName("HasStone"), false);
		State.Add(FName("HasFood"), false);
		State.Add(FName("HasCookedFood"), false);
		State.Add(FName("HasWeapon"), false);
		State.Add(FName("HasArmor"), false);
		State.Add(FName("HasPotion"), false);
		State.Add(FName("HasFishingRod"), false);
		State.Add(FName("HasPickaxe"), false);
		State.Add(FName("HasAxe"), false);
		State.Add(FName("HasHammer"), false);
		State.Add(FName("IsGeared"), false);
	}

	// --- Need flags ---
	if (NeedSystem)
	{
		State.Add(FName("IsFed"), NeedSystem->GetNeedValue(ENPCNeed::Hunger) > 60.f);
		State.Add(FName("IsRested"), NeedSystem->GetNeedValue(ENPCNeed::Energy) > 60.f);
	}
	else
	{
		State.Add(FName("IsFed"), true);
		State.Add(FName("IsRested"), true);
	}

	// --- Combat/threat flags (default, overridden by external state) ---
	State.Add(FName("InCombat"), false);
	State.Add(FName("InDanger"), false);
	State.Add(FName("TargetVisible"), false);
	State.Add(FName("TargetDead"), false);

	// Building state
	State.Add(FName("StructureBuilt"), false);

	// --- Overlay external state (from brain perception) ---
	for (const auto& Pair : ExternalWorldState)
	{
		State.Add(Pair.Key, Pair.Value);
	}

	return State;
}

// ---------------------------------------------------------------------------
// Goal conditions
// ---------------------------------------------------------------------------

TMap<FName, bool> UAoCNPCGoalPlanner::GetGoalConditions(ENPCGoal Goal) const
{
	TMap<FName, bool> Conditions;

	switch (Goal)
	{
	case ENPCGoal::SatisfyHunger:
		Conditions.Add(FName("IsFed"), true);
		break;

	case ENPCGoal::RestoreEnergy:
		Conditions.Add(FName("IsRested"), true);
		break;

	case ENPCGoal::SeekSafety:
		Conditions.Add(FName("AtHome"), true);
		break;

	case ENPCGoal::Fish:
		Conditions.Add(FName("HasFood"), true);
		break;

	case ENPCGoal::Cook:
		Conditions.Add(FName("HasCookedFood"), true);
		break;

	case ENPCGoal::Mine:
		Conditions.Add(FName("HasOre"), true);
		break;

	case ENPCGoal::Chop:
		Conditions.Add(FName("HasLumber"), true);
		break;

	case ENPCGoal::Smelt:
		Conditions.Add(FName("HasIngots"), true);
		break;

	case ENPCGoal::GatherResource:
		// Generic: get ore OR lumber
		Conditions.Add(FName("HasOre"), true);
		break;

	case ENPCGoal::CraftItem:
		Conditions.Add(FName("HasWeapon"), true);
		break;

	case ENPCGoal::BuildStructure:
		Conditions.Add(FName("StructureBuilt"), true);
		break;

	case ENPCGoal::EquipGear:
		Conditions.Add(FName("IsGeared"), true);
		break;

	case ENPCGoal::AttackTarget:
	case ENPCGoal::HuntPlayers:
		Conditions.Add(FName("TargetDead"), true);
		break;

	case ENPCGoal::FleeFromThreat:
		Conditions.Add(FName("AtHome"), true);
		break;

	case ENPCGoal::HideFromThreat:
		// Just need to not be visible — hide action handles this
		break;

	case ENPCGoal::PatrolArea:
	case ENPCGoal::GuardLocation:
	case ENPCGoal::InvestigateNoise:
		// These are continuous goals — no single completion condition
		Conditions.Add(FName("TargetVisible"), true);
		break;

	case ENPCGoal::Trade:
		Conditions.Add(FName("AtMarket"), true);
		break;

	case ENPCGoal::ExploreWorld:
		// Exploration never truly "completes" but we need a condition for planning
		Conditions.Add(FName("TargetVisible"), true);
		break;

	case ENPCGoal::ReturnHome:
		Conditions.Add(FName("AtHome"), true);
		break;

	case ENPCGoal::Socialize:
		// Social action is self-contained
		break;

	case ENPCGoal::Idle:
	case ENPCGoal::Dead:
	default:
		break;
	}

	return Conditions;
}

// ---------------------------------------------------------------------------
// A* Planning
// ---------------------------------------------------------------------------

bool UAoCNPCGoalPlanner::ArePreconditionsMet(const FNPCAction& Action, const TMap<FName, bool>& State) const
{
	for (const auto& Pre : Action.Preconditions)
	{
		const bool* StateVal = State.Find(Pre.Key);
		if (!StateVal || *StateVal != Pre.Value)
		{
			return false;
		}
	}
	return true;
}

TMap<FName, bool> UAoCNPCGoalPlanner::ApplyEffects(const TMap<FName, bool>& State, const FNPCAction& Action) const
{
	TMap<FName, bool> NewState = State;

	// If this is a MoveTo action, clear all other location flags
	const FString ActionStr = Action.ActionID.ToString();
	if (ActionStr.StartsWith(TEXT("MoveTo_")))
	{
		// Clear all At* flags
		for (auto& Pair : NewState)
		{
			if (Pair.Key.ToString().StartsWith(TEXT("At")))
			{
				Pair.Value = false;
			}
		}
	}

	// Apply effects
	for (const auto& Effect : Action.Effects)
	{
		NewState.Add(Effect.Key, Effect.Value);
	}

	return NewState;
}

float UAoCNPCGoalPlanner::CalculateHeuristic(const TMap<FName, bool>& State, const TMap<FName, bool>& GoalConditions) const
{
	float Unsatisfied = 0.f;
	for (const auto& Cond : GoalConditions)
	{
		const bool* StateVal = State.Find(Cond.Key);
		if (!StateVal || *StateVal != Cond.Value)
		{
			Unsatisfied += 1.f;
		}
	}
	return Unsatisfied * 5.f; // Each unsatisfied condition ≈ one cheap action
}

bool UAoCNPCGoalPlanner::IsGoalSatisfied(const TMap<FName, bool>& State, const TMap<FName, bool>& GoalConditions) const
{
	for (const auto& Cond : GoalConditions)
	{
		const bool* StateVal = State.Find(Cond.Key);
		if (!StateVal || *StateVal != Cond.Value)
		{
			return false;
		}
	}
	return true;
}

FNPCPlan UAoCNPCGoalPlanner::CreatePlan(ENPCGoal Goal)
{
	FNPCPlan Plan;
	Plan.Goal = Goal;

	const TMap<FName, bool> GoalConditions = GetGoalConditions(Goal);
	const TMap<FName, bool> StartState = GetWorldState();

	// If goal conditions are empty (continuous goals), create a single-action plan
	if (GoalConditions.Num() == 0)
	{
		switch (Goal)
		{
		case ENPCGoal::Socialize:
			{
				const FNPCAction* SocAction = ActionLibrary.FindByPredicate(
					[](const FNPCAction& A) { return A.ActionID == FName("Socialize"); });
				if (SocAction) Plan.Actions.Add(*SocAction);
			}
			break;
		case ENPCGoal::HideFromThreat:
			{
				const FNPCAction* HideAction = ActionLibrary.FindByPredicate(
					[](const FNPCAction& A) { return A.ActionID == FName("Hide"); });
				if (HideAction) Plan.Actions.Add(*HideAction);
			}
			break;
		case ENPCGoal::Idle:
		default:
			break;
		}
		return Plan;
	}

	// Check if goal is already satisfied
	if (IsGoalSatisfied(StartState, GoalConditions))
	{
		return Plan; // empty plan, goal already met
	}

	// Determine max depth
	int32 EffectiveMaxDepth = MaxPlanDepth;
	if (Personality)
	{
		EffectiveMaxDepth = FMath::Min(MaxPlanDepth, Personality->GetMaxPlanDepth());
	}

	// A* search
	TArray<FPlanNode> OpenList;
	int32 NodesExpanded = 0;

	// Start node
	FPlanNode StartNode;
	StartNode.State = StartState;
	StartNode.GCost = 0.f;
	StartNode.HCost = CalculateHeuristic(StartState, GoalConditions);
	OpenList.Add(StartNode);

	FNPCPlan BestPlan;
	float BestCost = TNumericLimits<float>::Max();

	while (OpenList.Num() > 0 && NodesExpanded < MaxSearchNodes)
	{
		// Find node with lowest F cost
		int32 BestIdx = 0;
		float BestF = OpenList[0].FCost();
		for (int32 i = 1; i < OpenList.Num(); ++i)
		{
			if (OpenList[i].FCost() < BestF)
			{
				BestF = OpenList[i].FCost();
				BestIdx = i;
			}
		}

		FPlanNode Current = OpenList[BestIdx];
		OpenList.RemoveAt(BestIdx);
		NodesExpanded++;

		// Check if goal is satisfied
		if (IsGoalSatisfied(Current.State, GoalConditions))
		{
			if (Current.GCost < BestCost)
			{
				BestCost = Current.GCost;
				BestPlan.Goal = Goal;
				BestPlan.Actions.Empty();
				for (int32 ActionIdx : Current.ActionSequence)
				{
					BestPlan.Actions.Add(ActionLibrary[ActionIdx]);
				}
				BestPlan.CurrentActionIndex = 0;
			}
			continue; // Keep searching for potentially better plans
		}

		// Don't expand beyond max depth
		if (Current.ActionSequence.Num() >= EffectiveMaxDepth)
		{
			continue;
		}

		// Try every action
		for (int32 i = 0; i < ActionLibrary.Num(); ++i)
		{
			const FNPCAction& Action = ActionLibrary[i];

			if (!ArePreconditionsMet(Action, Current.State))
			{
				continue;
			}

			// Avoid useless repetition: don't repeat the same action twice in a row
			// unless it's a gathering action (mining twice is fine)
			if (Current.ActionSequence.Num() > 0)
			{
				const int32 LastIdx = Current.ActionSequence.Last();
				if (LastIdx == i)
				{
					const FString ActionStr = Action.ActionID.ToString();
					// Allow repeating gathering actions
					if (!ActionStr.Contains(TEXT("Mine")) &&
					    !ActionStr.Contains(TEXT("Chop")) &&
					    !ActionStr.Contains(TEXT("Quarry")) &&
					    !ActionStr.Contains(TEXT("Fish")))
					{
						continue;
					}
				}
			}

			TMap<FName, bool> NewState = ApplyEffects(Current.State, Action);
			float NewGCost = Current.GCost + Action.Cost;

			// Skip if this path is already worse than our best
			if (NewGCost >= BestCost) continue;

			FPlanNode NewNode;
			NewNode.State = NewState;
			NewNode.ActionSequence = Current.ActionSequence;
			NewNode.ActionSequence.Add(i);
			NewNode.GCost = NewGCost;
			NewNode.HCost = CalculateHeuristic(NewState, GoalConditions);

			OpenList.Add(NewNode);
		}
	}

	if (BestPlan.Actions.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[GOAP] Plan created for goal %d with %d actions, cost %.1f"),
			static_cast<int32>(Goal), BestPlan.Actions.Num(), BestCost);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GOAP] No plan found for goal %d (expanded %d nodes)"),
			static_cast<int32>(Goal), NodesExpanded);
	}

	return BestPlan;
}

// ---------------------------------------------------------------------------
// Goal selection
// ---------------------------------------------------------------------------

ENPCGoal UAoCNPCGoalPlanner::SelectBestGoal() const
{
	struct FGoalScore
	{
		ENPCGoal Goal;
		float Score;
	};

	TArray<FGoalScore> Scores;

	// Helper lambda to score a goal
	auto AddGoal = [&](ENPCGoal Goal, float BaseScore)
	{
		float PersonalityMult = Personality ? Personality->GetGoalWeight(Goal) : 1.f;
		Scores.Add({ Goal, BaseScore * PersonalityMult });
	};

	// --- Survival needs (highest priority when critical) ---
	if (NeedSystem)
	{
		const float HungerUrgency = NeedSystem->GetNeedUrgency(ENPCNeed::Hunger);
		const float EnergyUrgency = NeedSystem->GetNeedUrgency(ENPCNeed::Energy);
		const float SafetyUrgency = NeedSystem->GetNeedUrgency(ENPCNeed::Safety);
		const float SocialUrgency = NeedSystem->GetNeedUrgency(ENPCNeed::Social);
		const float PurposeUrgency = NeedSystem->GetNeedUrgency(ENPCNeed::Purpose);
		const float WealthUrgency = NeedSystem->GetNeedUrgency(ENPCNeed::Wealth);

		// Critical needs get massive score boost
		const float CritMult = 10.f;

		if (NeedSystem->IsNeedCritical(ENPCNeed::Hunger))
			AddGoal(ENPCGoal::SatisfyHunger, HungerUrgency * CritMult);
		else
			AddGoal(ENPCGoal::SatisfyHunger, HungerUrgency * 3.f);

		if (NeedSystem->IsNeedCritical(ENPCNeed::Energy))
			AddGoal(ENPCGoal::RestoreEnergy, EnergyUrgency * CritMult);
		else
			AddGoal(ENPCGoal::RestoreEnergy, EnergyUrgency * 2.f);

		if (NeedSystem->IsNeedCritical(ENPCNeed::Safety))
			AddGoal(ENPCGoal::SeekSafety, SafetyUrgency * CritMult);

		// Social/purpose
		AddGoal(ENPCGoal::Socialize, SocialUrgency * 1.5f);
	}

	// --- Combat/danger goals ---
	const bool* bInCombat = ExternalWorldState.Find(FName("InCombat"));
	const bool* bInDanger = ExternalWorldState.Find(FName("InDanger"));
	const bool* bTargetVisible = ExternalWorldState.Find(FName("TargetVisible"));

	if (bInDanger && *bInDanger)
	{
		if (Personality && Personality->ShouldFlee(80.f))
		{
			AddGoal(ENPCGoal::FleeFromThreat, 8.f);
			AddGoal(ENPCGoal::HideFromThreat, 6.f);
		}
		else
		{
			AddGoal(ENPCGoal::AttackTarget, 7.f);
		}
	}

	if (bInCombat && *bInCombat)
	{
		AddGoal(ENPCGoal::AttackTarget, 8.f);
	}

	// --- Work/industry goals ---
	AddGoal(ENPCGoal::GatherResource, 2.f);
	AddGoal(ENPCGoal::CraftItem, 1.5f);
	AddGoal(ENPCGoal::BuildStructure, 1.5f);
	AddGoal(ENPCGoal::Mine, 2.f);
	AddGoal(ENPCGoal::Chop, 1.8f);
	AddGoal(ENPCGoal::Fish, 1.5f);
	AddGoal(ENPCGoal::Smelt, 1.3f);
	AddGoal(ENPCGoal::Cook, 1.2f);

	// --- Patrol/explore ---
	AddGoal(ENPCGoal::PatrolArea, 1.0f);
	AddGoal(ENPCGoal::ExploreWorld, 1.0f);
	AddGoal(ENPCGoal::Trade, 1.0f);

	// --- Hunting (aggressive NPCs) ---
	if (bTargetVisible && *bTargetVisible)
	{
		AddGoal(ENPCGoal::HuntPlayers, 5.f);
	}
	else
	{
		AddGoal(ENPCGoal::HuntPlayers, 1.5f);
	}

	// --- Fallback ---
	AddGoal(ENPCGoal::Idle, 0.1f);

	// Sort by score descending
	Scores.Sort([](const FGoalScore& A, const FGoalScore& B) { return A.Score > B.Score; });

	if (Scores.Num() > 0)
	{
		return Scores[0].Goal;
	}

	return ENPCGoal::Idle;
}

// ---------------------------------------------------------------------------
// Plan management
// ---------------------------------------------------------------------------

bool UAoCNPCGoalPlanner::CanInterruptCurrentPlan(ENPCGoal NewGoal) const
{
	if (!CurrentPlan.IsValid()) return true;

	// Critical survival needs always interrupt
	if (NewGoal == ENPCGoal::SatisfyHunger || NewGoal == ENPCGoal::RestoreEnergy || NewGoal == ENPCGoal::SeekSafety)
	{
		// Check if current action is interruptible
		const FNPCAction CurrentAction = CurrentPlan.GetCurrentAction();
		return CurrentAction.bInterruptibleByNeeds;
	}

	// Combat interrupts non-combat plans
	if (NewGoal == ENPCGoal::AttackTarget || NewGoal == ENPCGoal::FleeFromThreat)
	{
		return true;
	}

	return false;
}

void UAoCNPCGoalPlanner::ResumePlan()
{
	if (bHasInterruptedPlan && InterruptedPlan.IsValid())
	{
		CurrentPlan = InterruptedPlan;
		bHasInterruptedPlan = false;
		InterruptedPlan = FNPCPlan();

		UE_LOG(LogTemp, Log, TEXT("[GOAP] Resuming interrupted plan for goal %d at action %d"),
			static_cast<int32>(CurrentPlan.Goal), CurrentPlan.CurrentActionIndex);

		OnPlanChanged.Broadcast(CurrentPlan.Goal);
	}
}

bool UAoCNPCGoalPlanner::HasActivePlan() const
{
	return CurrentPlan.IsValid() && !CurrentPlan.IsComplete();
}

TArray<FNPCAction> UAoCNPCGoalPlanner::GetAvailableActions() const
{
	const TMap<FName, bool> State = GetWorldState();
	TArray<FNPCAction> Available;

	for (const FNPCAction& Action : ActionLibrary)
	{
		if (ArePreconditionsMet(Action, State))
		{
			Available.Add(Action);
		}
	}

	return Available;
}

void UAoCNPCGoalPlanner::AddGoal(const FString& GoalName, float Priority)
{
	// Stub: register a goal for GOAP planning
}

void UAoCNPCGoalPlanner::PushGoal(const FString& GoalName, float Priority)
{
	// Stub: push a high-priority goal
}

void UAoCNPCGoalPlanner::SetWorldState(const FString& Key, const FString& Value)
{
	// Stub: set world state for GOAP planner
}
