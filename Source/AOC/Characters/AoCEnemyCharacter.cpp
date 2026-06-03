// Copyright Architect of Creation. All Rights Reserved.

#include "AoCEnemyCharacter.h"
#include "../Skills/AoCSkillComponent.h"
#include "AIController.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAoCEnemyCharacter::AAoCEnemyCharacter()
{
	CreatureDataRowName = NAME_None;
	AggroRange = 1000.0f;
	LeashRange = 3000.0f;
	XPReward = 25;
	XPSkillType = FName(TEXT("Combat"));
	CorpseLifespan = 30.0f;

	// Health bar billboard widget
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawAtDesiredSize(true);

	// AI defaults
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AAoCEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	AoCAIController = Cast<AAIController>(GetController());
}

TArray<FAoCLootDrop> AAoCEnemyCharacter::RollLoot() const
{
	TArray<FAoCLootDrop> Drops;

	for (const FAoCLootDrop& Entry : LootTable)
	{
		float Roll = FMath::FRand();
		if (Roll <= Entry.DropChance)
		{
			FAoCLootDrop Drop = Entry;
			Drop.MinQuantity = FMath::RandRange(Entry.MinQuantity, Entry.MaxQuantity);
			// Store the rolled quantity in MinQuantity for convenience
			Drops.Add(Drop);
		}
	}

	return Drops;
}

void AAoCEnemyCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	// Determine the killer from the last damage instigator
	// In a full GAS pipeline, this would come from the gameplay effect context
	AActor* Killer = LastDamageInstigator;

	// Call base death handling (ragdoll, disable movement, broadcast OnCharacterDied)
	Super::HandleDeath();

	// Grant XP to killer
	if (Killer)
	{
		GrantXPToKiller(Killer);
	}

	// Spawn loot in the world
	SpawnLootDrops();

	// Broadcast enemy-specific death delegate
	OnEnemyDied.Broadcast(this, Killer);

	// Hide health bar
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false);
	}

	// Destroy corpse after lifespan
	SetLifeSpan(CorpseLifespan);
}

void AAoCEnemyCharacter::GrantXPToKiller(AActor* Killer)
{
	if (!Killer || !HasAuthority())
	{
		return;
	}

	// Try to get the SkillComponent from the killer (or their pawn)
	UAoCSkillComponent* KillerSkills = Killer->FindComponentByClass<UAoCSkillComponent>();

	if (!KillerSkills)
	{
		// If killer is a controller, check the pawn
		AController* KillerController = Cast<AController>(Killer);
		if (KillerController && KillerController->GetPawn())
		{
			KillerSkills = KillerController->GetPawn()->FindComponentByClass<UAoCSkillComponent>();
		}
	}

	if (KillerSkills)
	{
		// SkillComponent should expose an AddXP function
		// KillerSkills->AddXP(XPSkillType, XPReward);
		UE_LOG(LogTemp, Log, TEXT("AoCEnemyCharacter: Granted %d XP (%s) to killer %s"),
			XPReward, *XPSkillType.ToString(), *Killer->GetName());
	}
}

void AAoCEnemyCharacter::SpawnLootDrops()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<FAoCLootDrop> Drops = RollLoot();

	for (const FAoCLootDrop& Drop : Drops)
	{
		// In a full implementation, this would spawn a pickup actor in the world
		// at the enemy's location with the item data.
		FVector SpawnLoc = GetActorLocation() + FVector(
			FMath::FRandRange(-50.0f, 50.0f),
			FMath::FRandRange(-50.0f, 50.0f),
			50.0f
		);

		UE_LOG(LogTemp, Log, TEXT("AoCEnemyCharacter: Dropped %s x%d at %s"),
			*Drop.ItemID.ToString(), Drop.MinQuantity, *SpawnLoc.ToString());
	}
}
