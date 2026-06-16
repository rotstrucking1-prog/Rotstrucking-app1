// AoCNPCLootBrain.cpp
// Intelligent post-kill looting — full implementation

#include "AoCNPCLootBrain.h"
#include "AoCHumanoidNPCV2.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAoCLoot);

// ---------------------------------------------------------------------------
// Constructor / BeginPlay
// ---------------------------------------------------------------------------

UAoCNPCLootBrain::UAoCNPCLootBrain()
{
	PrimaryComponentTick.bCanEverTick = false;
	LootRand.Initialize(FMath::Rand());
}

void UAoCNPCLootBrain::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AoCHumanoidNPCV2>(GetOwner());
	if (OwnerNPC.IsValid())
	{
		SkillSystem = OwnerNPC->Skills;
		Inventory = OwnerNPC->Inventory;
		PersonalityComp = OwnerNPC->Personality;
		Memory = OwnerNPC->Memory;
	}

	LootRand.Initialize(GetOwner() ? GetOwner()->GetUniqueID() : FMath::Rand());
}

// ---------------------------------------------------------------------------
// Begin Looting
// ---------------------------------------------------------------------------

void UAoCNPCLootBrain::BeginLooting(AActor* Corpse)
{
	if (!Corpse || bIsLooting) return;

	// Check if area is dangerous — if so, use quick loot
	if (IsAreaDangerous())
	{
		QuickLoot(Corpse);
		return;
	}

	bIsLooting = true;
	bQuickMode = false;
	CurrentCorpse = Corpse;
	CurrentLootIndex = 0;

	// Brief pause like a real player opening loot window (0.5-1.0s)
	LootTimeRemaining = BaseLootDuration + LootRand.FRandRange(0.3f, 0.8f);

	// Evaluate and prioritize all items on the corpse
	EvaluateCorpseLoot(Corpse);

	UE_LOG(LogAoCLoot, Log, TEXT("LootBrain: %s begins looting %s (%d items evaluated)"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		*Corpse->GetName(), PrioritizedLoot.Num());
}

void UAoCNPCLootBrain::QuickLoot(AActor* Corpse)
{
	if (!Corpse || bIsLooting) return;

	bIsLooting = true;
	bQuickMode = true;
	CurrentCorpse = Corpse;
	CurrentLootIndex = 0;
	LootTimeRemaining = QuickLootDuration;

	// Evaluate loot but only grab top 2-3 items + gold
	EvaluateCorpseLoot(Corpse);

	// In quick mode, limit to first 3 items (already sorted by priority)
	int32 QuickLimit = 3;
	if (PrioritizedLoot.Num() > QuickLimit)
	{
		// Keep gold items even beyond limit
		TArray<FLootScore> Filtered;
		int32 NonGoldCount = 0;
		for (const FLootScore& LS : PrioritizedLoot)
		{
			if (LS.Item.bIsGold)
			{
				Filtered.Add(LS);
			}
			else if (NonGoldCount < QuickLimit)
			{
				Filtered.Add(LS);
				++NonGoldCount;
			}
		}
		PrioritizedLoot = Filtered;
	}

	UE_LOG(LogAoCLoot, Log, TEXT("LootBrain: %s QUICK-LOOTING %s (%d items, dangerous area!)"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		*Corpse->GetName(), PrioritizedLoot.Num());
}

// ---------------------------------------------------------------------------
// Loot Tick
// ---------------------------------------------------------------------------

void UAoCNPCLootBrain::LootTick(float DeltaTime)
{
	if (!bIsLooting) return;

	LootTimeRemaining -= DeltaTime;

	// Check if corpse is still valid
	if (!CurrentCorpse.IsValid())
	{
		bIsLooting = false;
		OnLootingComplete();
		return;
	}

	// Loot items progressively (one item every ~0.5s like a player clicking)
	float ItemInterval = bQuickMode ? 0.3f : 0.5f;
	float ElapsedSinceStart = (bQuickMode ? QuickLootDuration : BaseLootDuration) - LootTimeRemaining;
	int32 ItemsToHaveTaken = FMath::FloorToInt(ElapsedSinceStart / ItemInterval);

	while (CurrentLootIndex < PrioritizedLoot.Num() && CurrentLootIndex < ItemsToHaveTaken)
	{
		const FLootScore& LS = PrioritizedLoot[CurrentLootIndex];

		if (LS.bCanUse || LS.Item.bIsGold)
		{
			TakeItem(LS.Item);
		}

		++CurrentLootIndex;
	}

	// Done looting when all items taken or time expired
	if (CurrentLootIndex >= PrioritizedLoot.Num() || LootTimeRemaining <= 0.f)
	{
		bIsLooting = false;
		OnLootingComplete();
	}
}

// ---------------------------------------------------------------------------
// Item Evaluation
// ---------------------------------------------------------------------------

void UAoCNPCLootBrain::EvaluateCorpseLoot(AActor* Corpse)
{
	PrioritizedLoot.Reset();

	if (!Corpse) return;

	// Get items from corpse's inventory
	// In the real system, this reads from the corpse actor's loot table/inventory
	// Here we demonstrate the evaluation pipeline with simulated corpse inventory

	AoCHumanoidNPCV2* CorpseNPC = Cast<AoCHumanoidNPCV2>(Corpse);
	if (!CorpseNPC) return;

	// Simulate getting items from corpse inventory
	// In production, this iterates the corpse's actual FNPCItemData array
	// For now, we create the evaluation framework

	// Example: evaluate each item the corpse had
	// TArray<FNPCItemData> CorpseItems = CorpseNPC->Inventory->GetAllItems();

	// Simulate with empty array — the real integration would populate this
	TArray<FNPCItemData> CorpseItems;

	// Always add gold entry
	{
		FNPCItemData GoldItem;
		GoldItem.Name = TEXT("Gold");
		GoldItem.bIsGold = true;
		GoldItem.GoldAmount = 10.f + LootRand.FRandRange(0.f, 50.f); // Random gold amount
		GoldItem.Weight = 0.1f;
		CorpseItems.Add(GoldItem);
	}

	for (const FNPCItemData& Item : CorpseItems)
	{
		FLootScore LS;
		LS.Item = Item;
		LS.bCanUse = CanUseItem(Item);
		LS.Score = EvaluateItem(Item);

		if (Item.Slot != EEquipSlot::None)
		{
			LS.ImprovementOverEquipped = CompareToEquipped(Item, Item.Slot);
			LS.bShouldEquip = LS.ImprovementOverEquipped > 0.f;
		}

		PrioritizedLoot.Add(LS);
	}

	// Sort by priority: GOLD ALWAYS FIRST, then equipment upgrades, then by score
	PrioritizedLoot.Sort([](const FLootScore& A, const FLootScore& B)
	{
		// Gold always first
		if (A.Item.bIsGold && !B.Item.bIsGold) return true;
		if (!A.Item.bIsGold && B.Item.bIsGold) return false;

		// Equipment upgrades next
		if (A.bShouldEquip && !B.bShouldEquip) return true;
		if (!A.bShouldEquip && B.bShouldEquip) return false;

		// Then by score
		return A.Score > B.Score;
	});
}

float UAoCNPCLootBrain::EvaluateItem(const FNPCItemData& Item) const
{
	float Score = 0.f;

	// Gold is always valuable
	if (Item.bIsGold)
	{
		return 1000.f + Item.GoldAmount; // Gold always top priority
	}

	// Can't use = much lower value (but might sell)
	float UsabilityMult = CanUseItem(Item) ? 1.0f : 0.3f;

	// Equipment value based on slot
	if (Item.Slot != EEquipSlot::None)
	{
		float ImprovementScore = CompareToEquipped(Item, Item.Slot);
		if (ImprovementScore > 0.f)
		{
			// Direct upgrade — very high value
			Score += 500.f + ImprovementScore * 10.f;
		}
		else
		{
			// Not an upgrade — value based on gold worth per weight
			Score += GetValueDensity(Item) * 2.f;
		}
	}

	// Consumable value (potions, food)
	if (Item.bIsConsumable)
	{
		// Potions are always useful
		Score += 200.f;
	}

	// Material value (crafting materials)
	if (Item.bIsMaterial)
	{
		Score += Item.Value * 0.5f;
	}

	// Base value
	Score += Item.Value * 0.2f;

	// Personality modifier
	Score *= GetPersonalityScoreMultiplier(Item);

	// Usability modifier
	Score *= UsabilityMult;

	// Weight penalty — heavy items less desirable when carrying a lot
	float CurrentWeight = GetCurrentWeight();
	float WeightRoom = MaxCarryWeight - CurrentWeight;
	if (Item.Weight > WeightRoom)
	{
		Score *= 0.1f; // Can't carry it without dropping something
	}
	else if (Item.Weight > WeightRoom * 0.5f)
	{
		Score *= 0.5f; // Taking up a lot of remaining capacity
	}

	return FMath::Max(Score, 0.f);
}

float UAoCNPCLootBrain::CompareToEquipped(const FNPCItemData& Item, EEquipSlot Slot) const
{
	// Get the currently equipped item in this slot
	// In full integration: Inventory->GetEquippedInSlot(Slot)
	// For now, use a simple comparison framework

	float NewItemPower = 0.f;
	float CurrentPower = 0.f;

	// Power calculation based on slot type
	switch (Slot)
	{
	case EEquipSlot::MainHand:
	case EEquipSlot::OffHand:
		NewItemPower = Item.BaseDamage * 2.f + Item.BonusSTR + Item.BonusDEX;
		// Current weapon baseline (estimated from skill level)
		if (SkillSystem)
		{
			CurrentPower = SkillSystem->GetSkillLevel(SkillSystem->GetBestWeaponSkill()) * 0.5f;
		}
		break;

	case EEquipSlot::Head:
	case EEquipSlot::Chest:
	case EEquipSlot::Legs:
	case EEquipSlot::Feet:
	case EEquipSlot::Hands:
	{
		NewItemPower = Item.BaseArmor * 2.f + Item.BonusEND + Item.BonusAGI;
		// Slot-specific armor weight: chest > legs > head > feet > hands
		float SlotWeight = 1.0f;
		if (Slot == EEquipSlot::Chest) SlotWeight = 1.5f;
		else if (Slot == EEquipSlot::Legs) SlotWeight = 1.2f;
		else if (Slot == EEquipSlot::Head) SlotWeight = 1.0f;
		else SlotWeight = 0.8f;
		NewItemPower *= SlotWeight;
		break;
	}

	case EEquipSlot::Ring1:
	case EEquipSlot::Ring2:
	case EEquipSlot::Amulet:
		NewItemPower = Item.BonusSTR + Item.BonusDEX + Item.BonusAGI +
			Item.BonusINT + Item.BonusWIS + Item.BonusEND;
		break;

	case EEquipSlot::Back:
	case EEquipSlot::Belt:
		NewItemPower = Item.BaseArmor + Item.BonusEND;
		break;

	case EEquipSlot::Ammo:
		NewItemPower = Item.BaseDamage;
		break;

	default:
		break;
	}

	// Personality-weighted comparison
	if (SkillSystem)
	{
		// Mage values INT/WIS gear more
		ESkillID BestMagic = SkillSystem->GetBestMagicSkill();
		if (BestMagic != ESkillID::MAX && SkillSystem->GetSkillLevel(BestMagic) > 30.f)
		{
			NewItemPower += (Item.BonusINT + Item.BonusWIS) * 3.f;
		}

		// Warrior values STR/END gear more
		ESkillID BestWeapon = SkillSystem->GetBestWeaponSkill();
		if (BestWeapon != ESkillID::MAX && UAoCNPCSkillSystem::IsWeaponSkill(BestWeapon))
		{
			if (BestWeapon == ESkillID::Greatsword || BestWeapon == ESkillID::GreatAxe ||
				BestWeapon == ESkillID::GreatHammer)
			{
				NewItemPower += (Item.BonusSTR + Item.BonusEND) * 3.f;
			}
			else if (BestWeapon == ESkillID::Dagger || BestWeapon == ESkillID::Bow)
			{
				NewItemPower += (Item.BonusDEX + Item.BonusAGI) * 3.f;
			}
		}
	}

	return NewItemPower - CurrentPower;
}

// ---------------------------------------------------------------------------
// Take Item
// ---------------------------------------------------------------------------

void UAoCNPCLootBrain::TakeItem(const FNPCItemData& Item)
{
	if (!OwnerNPC.IsValid()) return;

	// Check weight
	float CurrentWeight = GetCurrentWeight();
	if (CurrentWeight + Item.Weight > MaxCarryWeight && !Item.bIsGold)
	{
		// Try to make room
		if (!MakeRoomForItem(Item))
		{
			UE_LOG(LogAoCLoot, Verbose, TEXT("LootBrain: %s can't carry %s (too heavy)"),
				*OwnerNPC->GetDisplayName(), *Item.Name.ToString());
			return;
		}
	}

	// Gold always taken
	if (Item.bIsGold)
	{
		// Add gold to inventory
		// Inventory->AddGold(Item.GoldAmount);
		UE_LOG(LogAoCLoot, Log, TEXT("LootBrain: %s looted %.0f gold"),
			*OwnerNPC->GetDisplayName(), Item.GoldAmount);
		return;
	}

	// Add to inventory
	// Inventory->AddToBag(Item);

	// Check if we should equip it
	if (Item.Slot != EEquipSlot::None)
	{
		float Improvement = CompareToEquipped(Item, Item.Slot);
		if (Improvement > 0.f)
		{
			// Equip immediately!
			// Inventory->EquipItem(Item);
			UE_LOG(LogAoCLoot, Log, TEXT("LootBrain: %s equipped %s (+%.1f improvement)"),
				*OwnerNPC->GetDisplayName(), *Item.Name.ToString(), Improvement);

			// Update combat brain — new gear changes effectiveness
			if (OwnerNPC->CombatBrain)
			{
				OwnerNPC->CombatBrain->BuildAbilityPool();
				OwnerNPC->CombatBrain->BuildRotation();
			}
		}
		else
		{
			UE_LOG(LogAoCLoot, Verbose, TEXT("LootBrain: %s bagged %s (not an upgrade, value: %.0f)"),
				*OwnerNPC->GetDisplayName(), *Item.Name.ToString(), Item.Value);
		}
	}
	else
	{
		UE_LOG(LogAoCLoot, Verbose, TEXT("LootBrain: %s took %s"),
			*OwnerNPC->GetDisplayName(), *Item.Name.ToString());
	}
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

bool UAoCNPCLootBrain::CanUseItem(const FNPCItemData& Item) const
{
	// Gold and consumables are always usable
	if (Item.bIsGold || Item.bIsConsumable || Item.bIsMaterial) return true;

	// Check skill requirement
	if (!Item.RequiredSkill.IsNone() && SkillSystem)
	{
		// Find the skill by name
		for (uint8 i = 0; i < static_cast<uint8>(ESkillID::MAX); ++i)
		{
			if (UAoCNPCSkillSystem::GetSkillName(static_cast<ESkillID>(i)) == Item.RequiredSkill)
			{
				if (SkillSystem->GetSkillLevel(static_cast<ESkillID>(i)) < Item.RequiredLevel)
				{
					return false;
				}
				break;
			}
		}
	}

	return true;
}

float UAoCNPCLootBrain::GetValueDensity(const FNPCItemData& Item) const
{
	if (Item.Weight <= 0.01f) return Item.Value * 100.f; // Weightless items are great
	return Item.Value / Item.Weight;
}

bool UAoCNPCLootBrain::MakeRoomForItem(const FNPCItemData& NewItem)
{
	// Find the lowest-value-density item in our bag and drop it if the new item is better
	// This is a simplified version — full integration queries inventory
	float NewItemDensity = GetValueDensity(NewItem);

	// In production: iterate bag, find lowest value density item
	// If new item density > lowest bag item density, drop the bag item
	// For now, always attempt to make room
	return NewItemDensity > 1.0f; // Only try for items worth carrying
}

float UAoCNPCLootBrain::GetCurrentWeight() const
{
	// Query inventory for total weight
	// if (Inventory) return Inventory->GetTotalWeight();
	return 50.f; // Default estimate
}

bool UAoCNPCLootBrain::IsAreaDangerous() const
{
	if (!OwnerNPC.IsValid()) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	// Check for hostile actors within 2000 units
	float DangerRadius = 2000.f;
	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(World, AoCHumanoidNPCV2::StaticClass(), NearbyActors);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == OwnerNPC.Get()) continue;

		float Dist = FVector::Dist(Actor->GetActorLocation(), OwnerNPC->GetActorLocation());
		if (Dist > DangerRadius) continue;

		AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Actor);
		if (OtherNPC && OtherNPC->SocialBrain)
		{
			if (OtherNPC->SocialBrain->IsEnemy(OwnerNPC.Get()))
			{
				return true;
			}
		}
	}

	// Check for player threats (simplified — real implementation checks player faction)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			float Dist = FVector::Dist(PC->GetPawn()->GetActorLocation(), OwnerNPC->GetActorLocation());
			if (Dist < DangerRadius)
			{
				// Player nearby — assume danger (could be hostile)
				return OwnerNPC->bIsAggressive; // Only dangerous if we're aggressive (players hunt aggressive NPCs)
			}
		}
	}

	return false;
}

float UAoCNPCLootBrain::GetPersonalityScoreMultiplier(const FNPCItemData& Item) const
{
	// Different archetypes value items differently
	if (!OwnerNPC.IsValid()) return 1.0f;

	float Mult = 1.0f;

	switch (OwnerNPC->ArchetypePreset)
	{
	case EPersonalityArchetype::Bandit:
		// Bandits value everything highly (greedy) — prioritize gold-per-weight for selling
		Mult = 1.3f;
		if (Item.bIsGold) Mult = 2.0f;
		break;

	case EPersonalityArchetype::Mage:
		// Mages value INT gear and magic materials highly, ignore heavy armor
		if (Item.BonusINT > 0 || Item.BonusWIS > 0) Mult = 1.8f;
		else if (Item.BaseArmor > 30.f && Item.Weight > 10.f) Mult = 0.3f; // Heavy armor = bad for mage
		break;

	case EPersonalityArchetype::Guard:
		// Guards value armor and weapons
		if (Item.BaseArmor > 0) Mult = 1.5f;
		if (Item.BaseDamage > 0 && Item.Slot == EEquipSlot::MainHand) Mult = 1.5f;
		break;

	case EPersonalityArchetype::Hunter:
		// Hunters value ranged gear and survival items
		if (Item.bIsConsumable) Mult = 1.3f;
		if (Item.BonusDEX > 0 || Item.BonusAGI > 0) Mult = 1.5f;
		break;

	case EPersonalityArchetype::Merchant:
		// Merchants value items by resale potential
		Mult = GetValueDensity(Item) > 5.f ? 1.5f : 0.8f;
		break;

	case EPersonalityArchetype::Villager:
		// Villagers mainly take practical items
		if (Item.bIsConsumable || Item.bIsMaterial) Mult = 1.3f;
		break;

	case EPersonalityArchetype::Hermit:
		// Hermits value survival items, ignore luxury
		if (Item.bIsConsumable || Item.bIsMaterial) Mult = 1.5f;
		if (Item.Value > 100.f && Item.BaseDamage == 0.f && Item.BaseArmor == 0.f) Mult = 0.5f;
		break;
	}

	return Mult;
}

void UAoCNPCLootBrain::OnLootingComplete()
{
	UE_LOG(LogAoCLoot, Log, TEXT("LootBrain: %s finished looting (%d/%d items taken)"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		CurrentLootIndex, PrioritizedLoot.Num());

	// Update combat power (new gear)
	if (SkillSystem)
	{
		// Force recalculation by marking dirty
		SkillSystem->GainSkillXP(ESkillID::Unarmed, 0.f, 0.f); // Trigger dirty flag
	}

	// Remember this location as a "good loot spot"
	if (Memory && CurrentCorpse.IsValid())
	{
		// Memory->Remember(TEXT("LootSpot"), CurrentCorpse->GetActorLocation(), 0.f, nullptr);
	}

	// Notify life brain that looting is done
	if (OwnerNPC.IsValid() && OwnerNPC->LifeBrain)
	{
		// LifeBrain will pick the next activity
	}

	PrioritizedLoot.Reset();
	CurrentCorpse = nullptr;
	CurrentLootIndex = 0;
}
