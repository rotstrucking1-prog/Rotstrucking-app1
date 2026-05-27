// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AoCInventoryWidget.generated.h"

class UUniformGridPanel;
class UTextBlock;
class UImage;

/** Equipment slot type for the paper doll. */
UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
	Head		UMETA(DisplayName = "Head"),
	Chest		UMETA(DisplayName = "Chest"),
	Legs		UMETA(DisplayName = "Legs"),
	Boots		UMETA(DisplayName = "Boots"),
	Gloves		UMETA(DisplayName = "Gloves"),
	Weapon		UMETA(DisplayName = "Weapon"),
	Shield		UMETA(DisplayName = "Shield"),
	Ring1		UMETA(DisplayName = "Ring 1"),
	Ring2		UMETA(DisplayName = "Ring 2"),
	Amulet		UMETA(DisplayName = "Amulet")
};

/**
 * FAoCInventorySlotData
 * Data for a single inventory slot display.
 */
USTRUCT(BlueprintType)
struct FAoCInventorySlotData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Inventory")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Inventory")
	int32 Quantity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Inventory")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Inventory")
	FString ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Inventory")
	FString TooltipText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Inventory")
	float Weight;

	FAoCInventorySlotData()
		: ItemID(NAME_None)
		, Quantity(0)
		, Weight(0.0f)
	{}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventorySlotDragDrop, int32, SourceSlot, int32, TargetSlot);

/**
 * UAoCInventoryWidget
 * Inventory panel with a 28-slot grid, equipment slots, and drag-and-drop support.
 */
UCLASS()
class AOC_API UAoCInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAoCInventoryWidget(const FObjectInitializer& ObjectInitializer);

	// ── Grid Panel ──

	/** Grid panel containing the 28 inventory slot widgets. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	// ── Equipment Slots (bound in Blueprint) ──

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Head;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Chest;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Legs;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Boots;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Gloves;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Weapon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Shield;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Ring1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Ring2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipSlot_Amulet;

	// ── Info Display ──

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WeightText;

	// ── Data ──

	/** Set the full inventory data. Rebuilds the grid display. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Inventory")
	void SetInventoryData(const TArray<FAoCInventorySlotData>& Items);

	/** Update a single slot. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Inventory")
	void UpdateSlot(int32 SlotIndex, const FAoCInventorySlotData& SlotData);

	/** Set the gold display. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Inventory")
	void SetGoldAmount(int64 Gold);

	/** Set the weight display. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Inventory")
	void SetWeight(float CurrentWeight, float MaxWeight);

	// ── Delegates ──

	UPROPERTY(BlueprintAssignable, Category = "AoC|UI|Inventory")
	FOnInventorySlotClicked OnSlotClicked;

	UPROPERTY(BlueprintAssignable, Category = "AoC|UI|Inventory")
	FOnInventorySlotDragDrop OnSlotDragDrop;

	// ── Drag and Drop ──

	/** Called from slot widgets when a drag is detected. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Inventory")
	void HandleSlotDragDetected(int32 SlotIndex);

	/** Called from slot widgets when a drop is received. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|Inventory")
	void HandleSlotDropReceived(int32 TargetSlotIndex);

protected:
	virtual void NativeConstruct() override;

private:
	/** Number of inventory slots. */
	static constexpr int32 InventorySlotCount = 28;

	/** Number of columns in the grid. */
	static constexpr int32 GridColumns = 4;

	/** Current inventory data cache. */
	TArray<FAoCInventorySlotData> CachedInventory;

	/** Slot being dragged (-1 = none). */
	int32 DragSourceSlot;
};
