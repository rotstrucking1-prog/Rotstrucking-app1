// Copyright Architect of Creation. All Rights Reserved.

#include "AoCSpellBarWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

UAoCSpellBarWidget::UAoCSpellBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SlotData.SetNum(SpellSlotCount);
}

void UAoCSpellBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HideCastBar();

	if (GlobalCooldownBar)
	{
		GlobalCooldownBar->SetPercent(0.0f);
		GlobalCooldownBar->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UAoCSpellBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Tick down cooldowns for visual updates
	for (int32 i = 0; i < SpellSlotCount; ++i)
	{
		FAoCSpellSlotWidgetData& SpellSlot = SlotData[i];
		if (SpellSlot.CooldownRemaining > 0.0f)
		{
			SpellSlot.CooldownRemaining = FMath::Max(0.0f, SpellSlot.CooldownRemaining - InDeltaTime);

			// Update the slot widget's cooldown overlay
			if (SpellSlotContainer)
			{
				UWidget* SlotWidget = SpellSlotContainer->GetChildAt(i);
				if (SlotWidget)
				{
					// Update cooldown overlay opacity or sweep angle
				}
			}
		}
	}
}

void UAoCSpellBarWidget::SetSpell(int32 SlotIndex, FName SpellID, UTexture2D* Icon, const FString& KeybindLabel)
{
	if (SlotIndex < 0 || SlotIndex >= SpellSlotCount)
	{
		return;
	}

	FAoCSpellSlotWidgetData& SpellSlot = SlotData[SlotIndex];
	SpellSlot.SpellID = SpellID;
	SpellSlot.KeybindLabel = KeybindLabel;
	SpellSlot.CooldownRemaining = 0.0f;
	SpellSlot.CooldownTotal = 0.0f;

	if (Icon)
	{
		SpellSlot.Icon = Icon;
	}

	// Update the visual slot widget in the container
	if (SpellSlotContainer)
	{
		UWidget* SlotWidget = SpellSlotContainer->GetChildAt(SlotIndex);
		if (SlotWidget)
		{
			// Cast to spell slot widget and update icon, keybind label
		}
	}
}

void UAoCSpellBarWidget::ClearSpell(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= SpellSlotCount)
	{
		return;
	}

	SlotData[SlotIndex] = FAoCSpellSlotWidgetData();

	if (SpellSlotContainer)
	{
		UWidget* SlotWidget = SpellSlotContainer->GetChildAt(SlotIndex);
		if (SlotWidget)
		{
			// Clear icon and keybind label
		}
	}
}

void UAoCSpellBarWidget::UpdateCooldown(int32 SlotIndex, float RemainingTime, float TotalCooldown)
{
	if (SlotIndex < 0 || SlotIndex >= SpellSlotCount)
	{
		return;
	}

	SlotData[SlotIndex].CooldownRemaining = RemainingTime;
	SlotData[SlotIndex].CooldownTotal = TotalCooldown;
}

void UAoCSpellBarWidget::UpdateGlobalCooldown(float RemainingTime, float TotalCooldown)
{
	if (GlobalCooldownBar)
	{
		if (TotalCooldown > 0.0f && RemainingTime > 0.0f)
		{
			float Percent = RemainingTime / TotalCooldown;
			GlobalCooldownBar->SetPercent(Percent);
			GlobalCooldownBar->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			GlobalCooldownBar->SetPercent(0.0f);
			GlobalCooldownBar->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UAoCSpellBarWidget::UpdateCastBar(const FString& SpellName, float ElapsedTime, float TotalCastTime)
{
	if (CastBar)
	{
		float Percent = (TotalCastTime > 0.0f) ? (ElapsedTime / TotalCastTime) : 0.0f;
		CastBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
		CastBar->SetVisibility(ESlateVisibility::Visible);
	}

	if (CastBarSpellName)
	{
		CastBarSpellName->SetText(FText::FromString(SpellName));
		CastBarSpellName->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAoCSpellBarWidget::HideCastBar()
{
	if (CastBar)
	{
		CastBar->SetPercent(0.0f);
		CastBar->SetVisibility(ESlateVisibility::Hidden);
	}

	if (CastBarSpellName)
	{
		CastBarSpellName->SetVisibility(ESlateVisibility::Hidden);
	}
}
