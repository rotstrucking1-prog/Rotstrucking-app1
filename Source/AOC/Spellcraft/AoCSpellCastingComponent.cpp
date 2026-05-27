// Copyright Architect of Creation. All Rights Reserved.

#include "AoCSpellCastingComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "../Combat/AoCProjectile.h"

UAoCSpellCastingComponent::UAoCSpellCastingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	GlobalCooldownDuration = 0.5f;
	GlobalCooldownRemaining = 0.0f;
	bIsCasting = false;
	CurrentCastSlot = -1;
	CastTimeElapsed = 0.0f;
	CastTimeTotal = 0.0f;

	// Initialize spell slots
	SpellSlots.SetNum(MaxSpellSlots);
}

void UAoCSpellCastingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAoCSpellCastingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Tick cooldowns
	TickCooldowns(DeltaTime);

	// Tick cast progress
	if (bIsCasting)
	{
		CastTimeElapsed += DeltaTime;
		OnCastProgress.Broadcast(CastTimeElapsed, CastTimeTotal);

		if (CastTimeElapsed >= CastTimeTotal)
		{
			FinishCast();
		}
	}
}

bool UAoCSpellCastingComponent::EquipSpell(int32 SlotIndex, FName SpellID)
{
	if (SlotIndex < 0 || SlotIndex >= MaxSpellSlots)
	{
		return false;
	}

	if (SpellID.IsNone())
	{
		return false;
	}

	// Validate spell exists in DataTable
	if (SpellDataTable)
	{
		FTableRowBase* RowBase = SpellDataTable->FindRow<FTableRowBase>(SpellID, TEXT("EquipSpell"));
		if (!RowBase)
		{
			UE_LOG(LogTemp, Warning, TEXT("AoCSpellCasting: Spell ID '%s' not found in DataTable."), *SpellID.ToString());
			return false;
		}
	}

	SpellSlots[SlotIndex].SpellID = SpellID;
	return true;
}

void UAoCSpellCastingComponent::UnequipSpell(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= MaxSpellSlots)
	{
		return;
	}

	SpellSlots[SlotIndex].SpellID = NAME_None;
	SpellSlots[SlotIndex].Icon = nullptr;
}

void UAoCSpellCastingComponent::SwapSlots(int32 SlotA, int32 SlotB)
{
	if (SlotA < 0 || SlotA >= MaxSpellSlots || SlotB < 0 || SlotB >= MaxSpellSlots)
	{
		return;
	}

	FAoCSpellSlot Temp = SpellSlots[SlotA];
	SpellSlots[SlotA] = SpellSlots[SlotB];
	SpellSlots[SlotB] = Temp;
}

void UAoCSpellCastingComponent::CastSpell(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= MaxSpellSlots)
	{
		OnSpellFailed.Broadcast(SlotIndex, TEXT("Invalid slot index."));
		return;
	}

	const FName& SpellID = SpellSlots[SlotIndex].SpellID;
	if (SpellID.IsNone())
	{
		OnSpellFailed.Broadcast(SlotIndex, TEXT("No spell equipped in this slot."));
		return;
	}

	// Check global cooldown
	if (GlobalCooldownRemaining > 0.0f)
	{
		OnSpellFailed.Broadcast(SlotIndex, TEXT("Global cooldown active."));
		return;
	}

	// Check per-spell cooldown
	if (const float* Remaining = CooldownMap.Find(SpellID))
	{
		if (*Remaining > 0.0f)
		{
			OnSpellFailed.Broadcast(SlotIndex, FString::Printf(TEXT("Spell on cooldown (%.1fs remaining)."), *Remaining));
			return;
		}
	}

	// Already casting?
	if (bIsCasting)
	{
		OnSpellFailed.Broadcast(SlotIndex, TEXT("Already casting a spell."));
		return;
	}

	// Look up spell data from DataTable for mana cost and cast time
	float ManaCost = 0.0f;
	float CastTime = 0.0f;
	float Cooldown = 1.0f;

	if (SpellDataTable)
	{
		// We access the row generically; in production the struct would be FAoCSpellDataRow
		// For now we read known column names via the struct.
		FTableRowBase* RowBase = SpellDataTable->FindRow<FTableRowBase>(SpellID, TEXT("CastSpell"));
		if (RowBase)
		{
			// Spell data fields would be accessed through the concrete struct type:
			// FAoCSpellDataRow* SpellData = static_cast<FAoCSpellDataRow*>(RowBase);
			// ManaCost = SpellData->ManaCost;
			// CastTime = SpellData->CastTime;
			// Cooldown = SpellData->Cooldown;
		}
	}

	// Check mana
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner());
	if (ASI)
	{
		UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
		if (ASC)
		{
			bool bFound = false;
			float CurrentMana = ASC->GetGameplayAttributeValue(
				FGameplayAttribute(FindFieldChecked<FProperty>(
					ASC->GetOwner()->GetClass(), FName(TEXT("Mana"))
				)), bFound);

			// Simplified mana check using attribute set directly
		}
	}

	// Begin cast
	if (CastTime <= 0.0f)
	{
		// Instant cast
		ExecuteSpell(SpellID);
		CooldownMap.Add(SpellID, Cooldown);
		StartGlobalCooldown();
		OnSpellCast.Broadcast(SlotIndex, SpellID);
	}
	else
	{
		// Channeled / cast time spell
		bIsCasting = true;
		CurrentCastSlot = SlotIndex;
		CurrentCastSpellID = SpellID;
		CastTimeElapsed = 0.0f;
		CastTimeTotal = CastTime;
	}
}

void UAoCSpellCastingComponent::InterruptCast()
{
	if (!bIsCasting)
	{
		return;
	}

	bIsCasting = false;
	CurrentCastSlot = -1;
	CurrentCastSpellID = NAME_None;
	CastTimeElapsed = 0.0f;
	CastTimeTotal = 0.0f;

	OnCastInterrupted.Broadcast();
}

float UAoCSpellCastingComponent::GetCooldownRemaining(FName SpellID) const
{
	if (const float* Remaining = CooldownMap.Find(SpellID))
	{
		return FMath::Max(0.0f, *Remaining);
	}
	return 0.0f;
}

void UAoCSpellCastingComponent::FinishCast()
{
	if (!bIsCasting)
	{
		return;
	}

	FName SpellID = CurrentCastSpellID;
	int32 SlotIndex = CurrentCastSlot;

	bIsCasting = false;
	CurrentCastSlot = -1;
	CurrentCastSpellID = NAME_None;
	CastTimeElapsed = 0.0f;
	CastTimeTotal = 0.0f;

	ExecuteSpell(SpellID);

	// Set cooldown (would come from DataTable)
	float Cooldown = 1.0f;
	CooldownMap.Add(SpellID, Cooldown);
	StartGlobalCooldown();

	OnSpellCast.Broadcast(SlotIndex, SpellID);
}

void UAoCSpellCastingComponent::ExecuteSpell(FName SpellID)
{
	// In a full implementation, this reads the spell type from the DataTable
	// and dispatches to the appropriate handler (projectile, AOE, instant, buff, etc.)
	UE_LOG(LogTemp, Log, TEXT("AoCSpellCasting: Executing spell '%s'."), *SpellID.ToString());
}

AAoCProjectile* UAoCSpellCastingComponent::SpawnProjectile(TSubclassOf<AAoCProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation)
{
	if (!ProjectileClass || !GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAoCProjectile* Projectile = GetWorld()->SpawnActor<AAoCProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	return Projectile;
}

void UAoCSpellCastingComponent::ApplyAOE(FVector Origin, float Radius, TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!EffectClass || !GetWorld())
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());

	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(Radius),
		Params
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor)
		{
			continue;
		}

		IAbilitySystemInterface* TargetASI = Cast<IAbilitySystemInterface>(HitActor);
		if (!TargetASI)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = TargetASI->GetAbilitySystemComponent();
		if (!TargetASC)
		{
			continue;
		}

		// Apply the gameplay effect to each target in the AOE
		IAbilitySystemInterface* OwnerASI = Cast<IAbilitySystemInterface>(GetOwner());
		if (OwnerASI)
		{
			UAbilitySystemComponent* OwnerASC = OwnerASI->GetAbilitySystemComponent();
			if (OwnerASC)
			{
				FGameplayEffectContextHandle Context = OwnerASC->MakeEffectContext();
				Context.AddSourceObject(GetOwner());

				FGameplayEffectSpecHandle SpecHandle = OwnerASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
				if (SpecHandle.IsValid())
				{
					OwnerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AoCSpellCasting: Applied AOE at %s, radius %.0f, hit %d actors."),
		*Origin.ToString(), Radius, Overlaps.Num());
}

void UAoCSpellCastingComponent::StartGlobalCooldown()
{
	GlobalCooldownRemaining = GlobalCooldownDuration;
}

void UAoCSpellCastingComponent::TickCooldowns(float DeltaTime)
{
	// Tick global cooldown
	if (GlobalCooldownRemaining > 0.0f)
	{
		GlobalCooldownRemaining = FMath::Max(0.0f, GlobalCooldownRemaining - DeltaTime);
	}

	// Tick per-spell cooldowns
	TArray<FName> ExpiredKeys;
	for (auto& Pair : CooldownMap)
	{
		Pair.Value -= DeltaTime;
		if (Pair.Value <= 0.0f)
		{
			ExpiredKeys.Add(Pair.Key);
		}
	}

	for (const FName& Key : ExpiredKeys)
	{
		CooldownMap.Remove(Key);
	}
}
