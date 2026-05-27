// AoCNPCInventory.cpp — NPC inventory and equipment implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCInventory.h"

UAoCNPCInventory::UAoCNPCInventory()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

int32 UAoCNPCInventory::FindItemIndex(FName ItemID) const
{
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].ItemID == ItemID)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

// ---------------------------------------------------------------------------
// Inventory operations
// ---------------------------------------------------------------------------

void UAoCNPCInventory::AddItem(const FNPCItem& Item)
{
	if (Item.ItemID.IsNone()) return;

	// Stack with existing item of same ID
	const int32 Idx = FindItemIndex(Item.ItemID);
	if (Idx != INDEX_NONE)
	{
		Items[Idx].Quantity += Item.Quantity;
	}
	else
	{
		Items.Add(Item);
	}
}

bool UAoCNPCInventory::RemoveItem(FName ItemID, int32 Count)
{
	const int32 Idx = FindItemIndex(ItemID);
	if (Idx == INDEX_NONE) return false;

	if (Items[Idx].Quantity <= Count)
	{
		Items.RemoveAt(Idx);
	}
	else
	{
		Items[Idx].Quantity -= Count;
	}
	return true;
}

bool UAoCNPCInventory::HasItem(FName ItemID, int32 Count) const
{
	const int32 Idx = FindItemIndex(ItemID);
	return (Idx != INDEX_NONE) && (Items[Idx].Quantity >= Count);
}

int32 UAoCNPCInventory::GetItemCount(FName ItemID) const
{
	const int32 Idx = FindItemIndex(ItemID);
	return (Idx != INDEX_NONE) ? Items[Idx].Quantity : 0;
}

bool UAoCNPCInventory::HasItemOfCategory(EItemCategory Category) const
{
	for (const FNPCItem& Item : Items)
	{
		if (Item.Category == Category && Item.Quantity > 0)
		{
			return true;
		}
	}
	return false;
}

TArray<FNPCItem> UAoCNPCInventory::GetItemsByCategory(EItemCategory Category) const
{
	TArray<FNPCItem> Result;
	for (const FNPCItem& Item : Items)
	{
		if (Item.Category == Category && Item.Quantity > 0)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Equipment
// ---------------------------------------------------------------------------

bool UAoCNPCInventory::EquipItem(FName ItemID, EEquipSlot Slot)
{
	const int32 Idx = FindItemIndex(ItemID);
	if (Idx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCInventory] Cannot equip %s — not in inventory."), *ItemID.ToString());
		return false;
	}

	const FNPCItem& Item = Items[Idx];

	// Only weapons, armor, and tools can be equipped
	if (Item.Category != EItemCategory::Weapon &&
	    Item.Category != EItemCategory::Armor &&
	    Item.Category != EItemCategory::Tool)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCInventory] Cannot equip %s — wrong category."), *ItemID.ToString());
		return false;
	}

	// Unequip current item in slot first
	UnequipSlot(Slot);

	// Equip
	EquippedItems.Add(Slot, Item);
	OnEquipmentChanged.Broadcast(Slot, ItemID);

	UE_LOG(LogTemp, Log, TEXT("[NPCInventory] Equipped %s in slot %d"), *ItemID.ToString(), static_cast<int32>(Slot));
	return true;
}

void UAoCNPCInventory::UnequipSlot(EEquipSlot Slot)
{
	if (EquippedItems.Contains(Slot))
	{
		const FName RemovedID = EquippedItems[Slot].ItemID;
		EquippedItems.Remove(Slot);
		OnEquipmentChanged.Broadcast(Slot, NAME_None);
	}
}

bool UAoCNPCInventory::IsSlotEquipped(EEquipSlot Slot) const
{
	return EquippedItems.Contains(Slot);
}

FNPCItem UAoCNPCInventory::GetEquippedWeapon() const
{
	const FNPCItem* Weapon = EquippedItems.Find(EEquipSlot::MainHand);
	return Weapon ? *Weapon : FNPCItem();
}

float UAoCNPCInventory::GetTotalArmor() const
{
	float TotalArmor = 0.f;

	for (const auto& Pair : EquippedItems)
	{
		if (Pair.Key == EEquipSlot::MainHand || Pair.Key == EEquipSlot::OffHand)
		{
			// Weapons don't contribute armor (unless they have an Armor stat like shields)
		}
		TotalArmor += Pair.Value.GetStat(FName("Armor"));
	}

	return TotalArmor;
}

FNPCItem UAoCNPCInventory::GetBestFood() const
{
	FNPCItem Best;
	float BestHealing = -1.f;

	for (const FNPCItem& Item : Items)
	{
		if (Item.Category == EItemCategory::Food && Item.Quantity > 0)
		{
			const float Healing = Item.GetStat(FName("Healing"));
			if (Healing > BestHealing)
			{
				BestHealing = Healing;
				Best = Item;
			}
		}
	}

	return Best;
}

FNPCItem UAoCNPCInventory::GetBestWeapon() const
{
	FNPCItem Best;
	float BestDamage = -1.f;

	for (const FNPCItem& Item : Items)
	{
		if (Item.Category == EItemCategory::Weapon && Item.Quantity > 0)
		{
			const float Damage = Item.GetStat(FName("Damage"));
			if (Damage > BestDamage)
			{
				BestDamage = Damage;
				Best = Item;
			}
		}
	}

	return Best;
}

FNPCItem UAoCNPCInventory::GetBestPotion() const
{
	FNPCItem Best;
	float BestValue = -1.f;

	for (const FNPCItem& Item : Items)
	{
		if (Item.Category == EItemCategory::Potion && Item.Quantity > 0)
		{
			const float Healing = Item.GetStat(FName("Healing"));
			if (Healing > BestValue)
			{
				BestValue = Healing;
				Best = Item;
			}
		}
	}

	return Best;
}

float UAoCNPCInventory::ConsumeItem(FName ItemID)
{
	const int32 Idx = FindItemIndex(ItemID);
	if (Idx == INDEX_NONE) return 0.f;

	const FNPCItem& Item = Items[Idx];
	float EffectAmount = 0.f;

	if (Item.Category == EItemCategory::Food)
	{
		EffectAmount = Item.GetStat(FName("Healing"));
		if (EffectAmount <= 0.f) EffectAmount = 20.f; // Default food healing
	}
	else if (Item.Category == EItemCategory::Potion)
	{
		EffectAmount = Item.GetStat(FName("Healing"));
		if (EffectAmount <= 0.f) EffectAmount = 30.f; // Default potion healing
	}

	// Remove one from stack
	RemoveItem(ItemID, 1);

	UE_LOG(LogTemp, Log, TEXT("[NPCInventory] Consumed %s for %.0f effect."), *ItemID.ToString(), EffectAmount);
	return EffectAmount;
}

// ---------------------------------------------------------------------------
// Weight
// ---------------------------------------------------------------------------

float UAoCNPCInventory::GetWeightLoad() const
{
	float Total = 0.f;
	for (const FNPCItem& Item : Items)
	{
		Total += Item.Weight * Item.Quantity;
	}
	return Total;
}

bool UAoCNPCInventory::IsOverEncumbered() const
{
	return GetWeightLoad() > MaxWeight;
}

// ---------------------------------------------------------------------------
// GOAP world state queries
// ---------------------------------------------------------------------------

bool UAoCNPCInventory::HasFood() const
{
	return HasItemOfCategory(EItemCategory::Food);
}

bool UAoCNPCInventory::HasCookedFood() const
{
	for (const FNPCItem& Item : Items)
	{
		if (Item.Category == EItemCategory::Food && Item.Quantity > 0)
		{
			// Items tagged with "Cooked" stat or with "Cooked" in name
			if (Item.Stats.Contains(FName("Cooked")) || Item.DisplayName.Contains(TEXT("Cooked")))
			{
				return true;
			}
		}
	}
	return false;
}

bool UAoCNPCInventory::HasOre() const
{
	return HasItem(FName("Ore")) || HasItem(FName("IronOre")) || HasItem(FName("CopperOre")) || HasItem(FName("GoldOre"));
}

bool UAoCNPCInventory::HasIngots() const
{
	return HasItem(FName("Ingot")) || HasItem(FName("IronIngot")) || HasItem(FName("CopperIngot")) || HasItem(FName("GoldIngot")) || HasItem(FName("SteelIngot"));
}

bool UAoCNPCInventory::HasLumber() const
{
	return HasItem(FName("Lumber")) || HasItem(FName("Wood")) || HasItem(FName("Plank"));
}

bool UAoCNPCInventory::HasStone() const
{
	return HasItem(FName("Stone")) || HasItem(FName("Granite")) || HasItem(FName("CutStone"));
}

bool UAoCNPCInventory::HasWeapon() const
{
	return HasItemOfCategory(EItemCategory::Weapon);
}

bool UAoCNPCInventory::HasArmor() const
{
	return HasItemOfCategory(EItemCategory::Armor);
}

bool UAoCNPCInventory::HasPotion() const
{
	return HasItemOfCategory(EItemCategory::Potion);
}

bool UAoCNPCInventory::HasTool(FName ToolID) const
{
	return HasItem(ToolID);
}
