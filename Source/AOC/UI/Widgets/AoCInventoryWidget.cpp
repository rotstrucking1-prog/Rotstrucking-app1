// Copyright Architect of Creation. All Rights Reserved.

#include "AoCInventoryWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

UAoCInventoryWidget::UAoCInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DragSourceSlot = -1;
	CachedInventory.SetNum(InventorySlotCount);
}

void UAoCInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize gold and weight displays
	SetGoldAmount(0);
	SetWeight(0.0f, 100.0f);
}

void UAoCInventoryWidget::SetInventoryData(const TArray<FAoCInventorySlotData>& Items)
{
	CachedInventory = Items;

	// Ensure we have exactly InventorySlotCount entries
	CachedInventory.SetNum(InventorySlotCount);

	// Rebuild grid display
	// In a full implementation, this would iterate through the grid slots
	// and update each slot widget's icon, quantity, and tooltip.
	// The grid slots are set up in the Blueprint widget with the InventoryGrid panel.
	for (int32 i = 0; i < InventorySlotCount; ++i)
	{
		UpdateSlot(i, CachedInventory[i]);
	}
}

void UAoCInventoryWidget::UpdateSlot(int32 SlotIndex, const FAoCInventorySlotData& SlotData)
{
	if (SlotIndex < 0 || SlotIndex >= InventorySlotCount)
	{
		return;
	}

	if (SlotIndex < CachedInventory.Num())
	{
		CachedInventory[SlotIndex] = SlotData;
	}

	// In a full implementation, update the visual slot widget in the grid.
	// The grid's child widgets would be custom UUserWidget subclasses
	// representing individual slots with an icon, quantity label, and tooltip.
	if (InventoryGrid)
	{
		UWidget* SlotWidget = InventoryGrid->GetChildAt(SlotIndex);
		if (SlotWidget)
		{
			// Cast to slot widget type and update its display
			// SlotWidget->SetIcon(SlotData.Icon);
			// SlotWidget->SetQuantity(SlotData.Quantity);
		}
	}
}

void UAoCInventoryWidget::SetGoldAmount(int64 Gold)
{
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("%lld GP"), Gold)));
	}
}

void UAoCInventoryWidget::SetWeight(float CurrentWeight, float MaxWeight)
{
	if (WeightText)
	{
		WeightText->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.1f kg"), CurrentWeight, MaxWeight)));
	}
}

void UAoCInventoryWidget::HandleSlotDragDetected(int32 SlotIndex)
{
	if (SlotIndex >= 0 && SlotIndex < InventorySlotCount)
	{
		DragSourceSlot = SlotIndex;
	}
}

void UAoCInventoryWidget::HandleSlotDropReceived(int32 TargetSlotIndex)
{
	if (DragSourceSlot >= 0 && DragSourceSlot < InventorySlotCount &&
		TargetSlotIndex >= 0 && TargetSlotIndex < InventorySlotCount &&
		DragSourceSlot != TargetSlotIndex)
	{
		OnSlotDragDrop.Broadcast(DragSourceSlot, TargetSlotIndex);

		// Swap cached data locally for immediate visual feedback
		FAoCInventorySlotData Temp = CachedInventory[DragSourceSlot];
		CachedInventory[DragSourceSlot] = CachedInventory[TargetSlotIndex];
		CachedInventory[TargetSlotIndex] = Temp;

		UpdateSlot(DragSourceSlot, CachedInventory[DragSourceSlot]);
		UpdateSlot(TargetSlotIndex, CachedInventory[TargetSlotIndex]);
	}

	DragSourceSlot = -1;
}
