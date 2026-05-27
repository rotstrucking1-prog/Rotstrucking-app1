// AoCNPCCharacter.cpp — NPC Character with V2 brain implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCCharacter.h"
#include "AoCNPCBrainV2.h"
#include "AoCNPCNeedSystem.h"
#include "AoCNPCMemory.h"
#include "AoCNPCInventory.h"
#include "AoCNPCGoalPlanner.h"
#include "AoCNPCPersonality.h"
#include "../AnimLab/AoCSmartAnimPlayer.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"

AAoCNPCCharacter::AAoCNPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create all AI sub-components
	NeedSystem = CreateDefaultSubobject<UAoCNPCNeedSystem>(TEXT("NeedSystem"));
	Memory = CreateDefaultSubobject<UAoCNPCMemory>(TEXT("Memory"));
	Inventory = CreateDefaultSubobject<UAoCNPCInventory>(TEXT("Inventory"));
	GoalPlanner = CreateDefaultSubobject<UAoCNPCGoalPlanner>(TEXT("GoalPlanner"));
	Personality = CreateDefaultSubobject<UAoCNPCPersonality>(TEXT("Personality"));

	// Create smart animation player
	AnimPlayer = CreateDefaultSubobject<UAoCSmartAnimPlayer>(TEXT("AnimPlayer"));

	// Create the V2 brain (master controller)
	NPCBrain = CreateDefaultSubobject<UAoCNPCBrainV2>(TEXT("NPCBrain"));

	// Sensible defaults for character movement
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = 300.f;
		CMC->bOrientRotationToMovement = true;
		CMC->RotationRate = FRotator(0.f, 360.f, 0.f);
	}

	// Default weapon tags
	WeaponTags = { TEXT("unarmed") };
}

void AAoCNPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Record spawn location
	SpawnLocation = GetActorLocation();
	SpawnRotation = GetActorRotation();

	// --- Configure Personality ---
	if (Personality)
	{
		Personality->SetArchetype(NPCArchetype);
		// Add slight randomization for variety
		Personality->RandomizeTraits(0.1f);
	}

	// --- Configure Memory with initial known locations ---
	if (Memory)
	{
		// Always know home
		Memory->RememberLocation(FName("Home"), SpawnLocation);

		// Add all editor-configured known locations
		for (const auto& Pair : InitialKnownLocations)
		{
			Memory->RememberLocation(Pair.Key, Pair.Value);
		}
	}

	// --- Configure Brain ---
	if (NPCBrain)
	{
		NPCBrain->PatrolPoints = PatrolPoints;
		NPCBrain->WeaponTags = WeaponTags;
		NPCBrain->KnownSpells = KnownSpells;
		NPCBrain->MagicSchools = MagicSchools;
		NPCBrain->SetHealthRatio(GetHealthPercent());

		// Aggressive NPCs get a tighter perception radius
		if (bIsAggressive)
		{
			NPCBrain->PerceptionRadius = AggroRadius;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[NPCCharacter] %s spawned as %s (%s)"),
		*NPCName, *UEnum::GetValueAsString(NPCArchetype), *Faction);
}

// ---------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------

float AAoCNPCCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead) return 0.f;

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	Health = FMath::Clamp(Health - ActualDamage, 0.f, MaxHealth);

	// Update aggro
	if (DamageCauser)
	{
		AddAggro(DamageCauser, ActualDamage);
	}

	// Inform brain
	if (NPCBrain)
	{
		NPCBrain->SetHealthRatio(GetHealthPercent());

		// Calculate hit direction
		FVector HitDir = FVector::ForwardVector;
		if (DamageCauser)
		{
			HitDir = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
		}
		NPCBrain->NotifyHit(HitDir, ActualDamage);
	}

	// Check death
	if (Health <= 0.f)
	{
		Die();
	}

	return ActualDamage;
}

// ---------------------------------------------------------------------------
// Death & Respawn
// ---------------------------------------------------------------------------

void AAoCNPCCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;

	UE_LOG(LogTemp, Log, TEXT("[NPC] %s has died."), *NPCName);

	// Tell brain we're dead
	if (NPCBrain)
	{
		NPCBrain->SetHealthRatio(0.f);
		NPCBrain->ForceGoal(ENPCGoal::Dead);
	}

	// Disable collision
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Disable movement
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->DisableMovement();
	}

	// Start respawn timer
	if (RespawnTime > 0.f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			RespawnTimerHandle, this, &AAoCNPCCharacter::Respawn,
			RespawnTime, false);
	}
}

void AAoCNPCCharacter::Respawn()
{
	UE_LOG(LogTemp, Log, TEXT("[NPC] %s is respawning."), *NPCName);

	bIsDead = false;
	Health = MaxHealth;
	Mana = MaxMana;
	AggroTable.Empty();

	// Restore position
	SetActorLocation(SpawnLocation);
	SetActorRotation(SpawnRotation);

	// Re-enable collision
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Re-enable movement
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_Walking);
	}

	// Reset brain
	if (NPCBrain)
	{
		NPCBrain->SetHealthRatio(1.f);
		NPCBrain->ForceGoal(ENPCGoal::Idle);
	}

	// Reset needs to healthy defaults
	if (NeedSystem)
	{
		NeedSystem->SetNeedValue(ENPCNeed::Hunger, 80.f);
		NeedSystem->SetNeedValue(ENPCNeed::Energy, 90.f);
		NeedSystem->SetNeedValue(ENPCNeed::Safety, 100.f);
		NeedSystem->SetNeedValue(ENPCNeed::Purpose, 60.f);
		NeedSystem->SetNeedValue(ENPCNeed::Social, 50.f);
	}

	// Clear inventory (fresh start on respawn)
	if (Inventory)
	{
		Inventory->Items.Empty();
		Inventory->EquippedItems.Empty();
	}

	// Re-apply personality (in case it was modified during life)
	if (Personality)
	{
		Personality->SetArchetype(NPCArchetype);
		Personality->RandomizeTraits(0.1f);
	}
}

// ---------------------------------------------------------------------------
// Aggro
// ---------------------------------------------------------------------------

void AAoCNPCCharacter::AddAggro(AActor* Source, float Amount)
{
	if (!Source) return;
	float& Existing = AggroTable.FindOrAdd(Source);
	Existing += Amount;
}

AActor* AAoCNPCCharacter::GetTopAggroTarget() const
{
	AActor* TopTarget = nullptr;
	float TopAggro = -1.f;

	for (const auto& Pair : AggroTable)
	{
		if (Pair.Value > TopAggro)
		{
			TopAggro = Pair.Value;
			TopTarget = Pair.Key;
		}
	}

	return TopTarget;
}
