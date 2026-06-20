// AoCNPCInventory.h — NPC inventory and equipment system
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCInventory.generated.h"

/** Item categories */
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	Weapon      UMETA(DisplayName = "Weapon"),
	Armor       UMETA(DisplayName = "Armor"),
	Material    UMETA(DisplayName = "Material"),
	Food        UMETA(DisplayName = "Food"),
	Potion      UMETA(DisplayName = "Potion"),
	Tool        UMETA(DisplayName = "Tool"),
	CraftedGood UMETA(DisplayName = "Crafted Good")
};

/** Equipment slots */
UENUM(BlueprintType)
enum class EEquipSlot : uint8
{
	Head       UMETA(DisplayName = "Head"),
	Chest      UMETA(DisplayName = "Chest"),
	Legs       UMETA(DisplayName = "Legs"),
	Feet       UMETA(DisplayName = "Feet"),
	Hands      UMETA(DisplayName = "Hands"),
	Shoulders  UMETA(DisplayName = "Shoulders"),
	Back       UMETA(DisplayName = "Back"),
	MainHand   UMETA(DisplayName = "Main Hand"),
	OffHand    UMETA(DisplayName = "Off Hand"),
	Ring1      UMETA(DisplayName = "Ring 1"),
	Ring2      UMETA(DisplayName = "Ring 2"),
	Neck       UMETA(DisplayName = "Neck"),
	Waist      UMETA(DisplayName = "Waist"),
	None       UMETA(DisplayName = "None"),
	MAX        UMETA(Hidden),
	Ammo,
	Amulet,
	Belt,
};

/** An item in the NPC's inventory */
USTRUCT(BlueprintType)
struct FNPCItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	EItemCategory Category = EItemCategory::Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	TMap<FName, float> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	float Weight = 1.f;

	/** Slot this item can be equipped to (only for weapons/armor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	EEquipSlot PreferredSlot = EEquipSlot::MAX;

	bool IsValid() const { return !ItemID.IsNone(); }

	float GetStat(FName StatName) const
	{
		const float* Val = Stats.Find(StatName);
		return Val ? *Val : 0.f;
	}
};

/** Broadcast when equipment changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipmentChanged, EEquipSlot, Slot, FName, ItemID);

/**
 * UAoCNPCInventory
 *
 * Manages an NPC's carried items and equipped gear.
 * Used by the goal planner to check preconditions (HasOre, HasFood, etc.)
 * and by combat to determine weapon/armor stats.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCInventory : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCInventory();

	// --- Inventory --------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	TArray<FNPCItem> Items;

	// --- Equipment (13 slots) ---------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|AI|Inventory|Equipment")
	TMap<EEquipSlot, FNPCItem> EquippedItems;

	// --- Carry capacity ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|AI|Inventory")
	float MaxWeight = 100.f;

	// --- Delegates --------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "AoC|AI|Inventory")
	FOnEquipmentChanged OnEquipmentChanged;

	// --- Inventory operations ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	void AddItem(const FNPCItem& Item);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	bool RemoveItem(FName ItemID, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasItem(FName ItemID, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	int32 GetItemCount(FName ItemID) const;

	/** Check if we have any item of a given category. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasItemOfCategory(EItemCategory Category) const;

	/** Get all items of a specific category. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	TArray<FNPCItem> GetItemsByCategory(EItemCategory Category) const;

	// --- Equipment operations ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	bool EquipItem(FName ItemID, EEquipSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	void UnequipSlot(EEquipSlot Slot);

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool IsSlotEquipped(EEquipSlot Slot) const;

	/** Get the currently equipped weapon (MainHand). Returns nullptr-equivalent item if none. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	FNPCItem GetEquippedWeapon() const;

	/** Sum all equipped armor stats. */
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	float GetTotalArmor() const;

	/** Find the best food in inventory (highest Healing stat). */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	FNPCItem GetBestFood() const;

	/** Find the best weapon in inventory (highest Damage stat). */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	FNPCItem GetBestWeapon() const;

	/** Find the best potion in inventory. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	FNPCItem GetBestPotion() const;

	/** Consume a food/potion item (removes 1 from inventory). Returns the healing/effect amount. */
	UFUNCTION(BlueprintCallable, Category = "AoC|AI|Inventory")
	float ConsumeItem(FName ItemID);

	// --- Weight -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	float GetWeightLoad() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	float GetMaxWeight() const { return MaxWeight; }

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool IsOverEncumbered() const;

	// --- Query helpers for GOAP world state -------------------------------------

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasFood() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasCookedFood() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasOre() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasIngots() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasLumber() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasStone() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasWeapon() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasArmor() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasPotion() const;

	UFUNCTION(BlueprintPure, Category = "AoC|AI|Inventory")
	bool HasTool(FName ToolID) const;

private:
	/** Find the index of an item by ID, returns INDEX_NONE if not found. */
	int32 FindItemIndex(FName ItemID) const;
};
