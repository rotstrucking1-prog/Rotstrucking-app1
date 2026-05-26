// AoCNPCCharacter.cpp — NPC Character implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCCharacter.h"
#include "AoCNPCBrain.h"
#include "AoCSmartAnimPlayer.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"

AAoCNPCCharacter::AAoCNPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create brain component
	NPCBrain = CreateDefaultSubobject<UAoCNPCBrain>(TEXT("NPCBrain"));

	// Create smart animation player
	AnimPlayer = CreateDefaultSubobject<UAoCSmartAnimPlayer>(TEXT("AnimPlayer"));

	// Sensible defaults for character movement
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = 300.f;
		CMC->bOrientRotationToMovement = true;
		CMC->RotationRate = FRotator(0.f, 360.f, 0.f);
	}
}

void AAoCNPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Record spawn location
	SpawnLocation = GetActorLocation();
	SpawnRotation = GetActorRotation();

	// Pass config to brain
	if (NPCBrain)
	{
		NPCBrain->PatrolPoints = PatrolPoints;
		NPCBrain->SetHealthRatio(GetHealthPercent());

		// Aggressive NPCs get a tighter alert radius
		if (bIsAggressive)
		{
			NPCBrain->SightRadius = AggroRadius;
			NPCBrain->AlertRadius = AggroRadius * 0.8f;
		}
	}
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
		NPCBrain->ForceGoal(EAoCNPCGoal::Dead);
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
		NPCBrain->ForceGoal(EAoCNPCGoal::Idle);
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
