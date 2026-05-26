// AoCNPCBrain.cpp — Goal-based NPC AI component implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCNPCBrain.h"
#include "AoCAnimationLab.h"
#include "AoCSmartAnimPlayer.h"
#include "AoCAnimationEntry.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

UAoCNPCBrain::UAoCNPCBrain()
{
	PrimaryComponentTick.bCanEverTick = false; // purely timer-driven
	WeaponTags = { TEXT("unarmed") };
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UAoCNPCBrain::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystem
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		AnimLab = GI->GetSubsystem<UAoCAnimationLab>();
	}

	// Cache sibling component
	AnimPlayer = GetOwner()->FindComponentByClass<UAoCSmartAnimPlayer>();

	if (!AnimLab)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCBrain] AnimationLab subsystem not found!"));
	}
	if (!AnimPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPCBrain] SmartAnimPlayer not found on %s"), *GetOwner()->GetName());
	}

	// --- Start perception timer (every 0.5s) ---
	GetWorld()->GetTimerManager().SetTimer(
		PerceptionTimerHandle, this, &UAoCNPCBrain::RunPerception,
		0.5f, true);

	// --- Start goal selection timer (every 1.0s) ---
	GetWorld()->GetTimerManager().SetTimer(
		GoalSelectionTimerHandle, this, &UAoCNPCBrain::SelectGoal,
		1.0f, true);

	// --- Start goal execution timer (every 0.5s) ---
	GetWorld()->GetTimerManager().SetTimer(
		GoalExecutionTimerHandle, this, &UAoCNPCBrain::ExecuteGoal,
		0.5f, true);

	UE_LOG(LogTemp, Log, TEXT("[NPCBrain] %s brain initialized."), *GetOwner()->GetName());
}

void UAoCNPCBrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		FTimerManager& TM = GetWorld()->GetTimerManager();
		TM.ClearTimer(PerceptionTimerHandle);
		TM.ClearTimer(GoalSelectionTimerHandle);
		TM.ClearTimer(GoalExecutionTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Perception
// ---------------------------------------------------------------------------

void UAoCNPCBrain::RunPerception()
{
	if (CurrentGoal == EAoCNPCGoal::Dead) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector MyLoc = GetOwner()->GetActorLocation();

	// Find player
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	PlayerRef = PlayerPawn;

	if (PlayerPawn)
	{
		DistanceToPlayer = FVector::Dist(MyLoc, PlayerPawn->GetActorLocation());
	}
	else
	{
		DistanceToPlayer = 99999.f;
	}

	// Simple sphere detection for all pawns in sight radius
	DetectedActors.Empty();
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, APawn::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		if (Actor == GetOwner()) continue;
		float Dist = FVector::Dist(MyLoc, Actor->GetActorLocation());
		if (Dist <= SightRadius)
		{
			DetectedActors.Add(Actor);
		}
	}

	ThreatLevel = CalculateThreatLevel();
}

float UAoCNPCBrain::CalculateThreatLevel() const
{
	float Threat = 0.f;

	// Player proximity is the primary threat driver
	if (DistanceToPlayer < AlertRadius)
	{
		// Closer = higher threat, max 100 at distance 0
		Threat += (1.f - (DistanceToPlayer / AlertRadius)) * 60.f;
	}

	// Additional actors add some threat
	for (const TObjectPtr<AActor>& Det : DetectedActors)
	{
		if (Det == GetOwner()) continue;
		float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Det->GetActorLocation());
		if (Dist < AlertRadius)
		{
			Threat += 10.f;
		}
	}

	// Low health increases perceived threat
	if (HealthRatio < 0.5f)
	{
		Threat += (1.f - HealthRatio) * 20.f;
	}

	return FMath::Clamp(Threat, 0.f, 100.f);
}

// ---------------------------------------------------------------------------
// Goal selection (runs every 1s)
// ---------------------------------------------------------------------------

void UAoCNPCBrain::SelectGoal()
{
	if (CurrentGoal == EAoCNPCGoal::Dead) return;

	// Priority-ordered goal selection
	if (HealthRatio <= 0.f)
	{
		SetGoal(EAoCNPCGoal::Dead);
	}
	else if (HealthRatio < FleeHealthPercent)
	{
		SetGoal(EAoCNPCGoal::Flee);
	}
	else if (ThreatLevel > HighThreatThreshold)
	{
		SetGoal(EAoCNPCGoal::Combat);
	}
	else if (ThreatLevel > MediumThreatThreshold)
	{
		SetGoal(EAoCNPCGoal::Alert);
	}
	else if (bHasInvestigateTarget)
	{
		SetGoal(EAoCNPCGoal::Investigate);
	}
	else if (PatrolPoints.Num() > 0)
	{
		SetGoal(EAoCNPCGoal::Patrol);
	}
	else
	{
		SetGoal(EAoCNPCGoal::Idle);
	}
}

void UAoCNPCBrain::SetGoal(EAoCNPCGoal NewGoal)
{
	if (NewGoal == CurrentGoal) return;

	EAoCNPCGoal OldGoal = CurrentGoal;
	CurrentGoal = NewGoal;
	OnGoalChanged.Broadcast(OldGoal, NewGoal);

	UE_LOG(LogTemp, Log, TEXT("[NPCBrain] %s goal: %d → %d"),
		*GetOwner()->GetName(), (int32)OldGoal, (int32)NewGoal);
}

// ---------------------------------------------------------------------------
// Goal execution (runs every 0.5s)
// ---------------------------------------------------------------------------

void UAoCNPCBrain::ExecuteGoal()
{
	switch (CurrentGoal)
	{
	case EAoCNPCGoal::Idle:        ExecuteIdle();        break;
	case EAoCNPCGoal::Patrol:      ExecutePatrol();      break;
	case EAoCNPCGoal::Investigate: ExecuteInvestigate(); break;
	case EAoCNPCGoal::Alert:       ExecuteAlert();       break;
	case EAoCNPCGoal::Combat:      ExecuteCombat();      break;
	case EAoCNPCGoal::Flee:        ExecuteFlee();        break;
	case EAoCNPCGoal::Hide:        ExecuteHide();        break;
	case EAoCNPCGoal::Social:      ExecuteSocial();      break;
	case EAoCNPCGoal::Dead:        ExecuteDead();        break;
	}
}

// ---------------------------------------------------------------------------
// Goal-specific execution
// ---------------------------------------------------------------------------

void UAoCNPCBrain::ExecuteIdle()
{
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		// Pick a random idle or social animation
		const bool bSocial = FMath::RandBool();
		const FString Cat = bSocial ? TEXT("social") : TEXT("idle");
		PlayFromLab(Cat, TEXT("standing"), { TEXT("idle"), TEXT("relaxed") });
	}
}

void UAoCNPCBrain::ExecutePatrol()
{
	if (PatrolPoints.Num() == 0) return;

	const FVector Target = PatrolPoints[CurrentPatrolIndex];
	const FVector MyLoc = GetOwner()->GetActorLocation();
	const float Dist = FVector::Dist2D(MyLoc, Target);

	if (Dist < 150.f)
	{
		// Reached waypoint — pause and look around
		CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();

		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("idle"), TEXT("standing"), { TEXT("look"), TEXT("around") });
		}
	}
	else
	{
		// Walk toward waypoint
		MoveToward(Target, 200.f);

		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("movement"), TEXT("standing"), { TEXT("walk") });
		}
	}
}

void UAoCNPCBrain::ExecuteInvestigate()
{
	if (!bHasInvestigateTarget)
	{
		return;
	}

	const FVector MyLoc = GetOwner()->GetActorLocation();
	const float Dist = FVector::Dist2D(MyLoc, InvestigateLocation);

	if (Dist < 200.f)
	{
		// Arrived at investigation point — look around then clear
		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("idle"), TEXT("standing"), { TEXT("look"), TEXT("alert") });
		}
		bHasInvestigateTarget = false;
	}
	else
	{
		MoveToward(InvestigateLocation, 250.f);
		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			PlayFromLab(TEXT("movement"), TEXT("standing"), { TEXT("walk"), TEXT("cautious") });
		}
	}
}

void UAoCNPCBrain::ExecuteAlert()
{
	// Face the player and play alert idle
	if (PlayerRef)
	{
		FVector Dir = GetDirectionTo(PlayerRef);
		GetOwner()->SetActorRotation(Dir.Rotation());
	}

	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("idle"), TEXT("standing"), { TEXT("alert"), TEXT("combat"), TEXT("ready") });
	}
}

void UAoCNPCBrain::ExecuteCombat()
{
	if (!PlayerRef) return;

	const float Dist = DistanceToPlayer;

	// If far, run toward player
	if (Dist > 300.f)
	{
		MoveToward(PlayerRef->GetActorLocation(), 600.f);
		if (AnimPlayer && !AnimPlayer->IsPlaying())
		{
			TArray<FString> Tags = WeaponTags;
			Tags.Add(TEXT("run"));
			PlayFromLab(TEXT("movement"), TEXT("standing"), Tags);
		}
		return;
	}

	// Face the player
	FVector Dir = GetDirectionTo(PlayerRef);
	GetOwner()->SetActorRotation(Dir.Rotation());

	// Attack if not currently animating
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		// Decide action: attack, dodge, block, taunt
		const int32 Roll = FMath::RandRange(0, 9);

		if (Roll < 6) // 60% attack
		{
			TArray<FString> Tags = WeaponTags;
			Tags.Add(TEXT("attack"));

			// Variety: don't repeat last attack
			if (AttackVarietyCounter > 2)
			{
				Tags.Add(TEXT("combo"));
				AttackVarietyCounter = 0;
			}
			else
			{
				AttackVarietyCounter++;
			}

			if (AnimLab)
			{
				FAoCAnimationEntry Entry = AnimLab->FindBestMatch(TEXT("combat_melee"), TEXT("standing"), Tags);
				// Avoid repeating the exact same attack
				if (Entry.EntryName == LastAttackName && AnimLab->QueryByCategory(TEXT("combat_melee")).Num() > 1)
				{
					Entry = AnimLab->GetRandomFromCategory(TEXT("combat_melee"));
				}
				LastAttackName = Entry.EntryName;
				if (AnimPlayer) AnimPlayer->PlayEntry(Entry);
			}
		}
		else if (Roll < 8) // 20% block/dodge
		{
			TArray<FString> Tags = WeaponTags;
			Tags.Add(TEXT("block"));
			Tags.Add(TEXT("dodge"));
			PlayFromLab(TEXT("combat_melee"), TEXT("standing"), Tags);
		}
		else // 20% taunt or combat idle
		{
			PlayFromLab(TEXT("social"), TEXT("standing"), { TEXT("taunt"), TEXT("aggressive") });
		}
	}
}

void UAoCNPCBrain::ExecuteFlee()
{
	if (!PlayerRef) return;

	// Run AWAY from player
	const FVector MyLoc = GetOwner()->GetActorLocation();
	const FVector PlayerLoc = PlayerRef->GetActorLocation();
	const FVector FleeDir = (MyLoc - PlayerLoc).GetSafeNormal();
	const FVector FleeTarget = MyLoc + FleeDir * 1000.f;

	MoveToward(FleeTarget, 600.f);

	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("movement"), TEXT("standing"), { TEXT("run"), TEXT("sprint"), TEXT("fast") });
	}
}

void UAoCNPCBrain::ExecuteHide()
{
	// Play crouching/hiding animation
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("stealth"), TEXT("standing"), { TEXT("crouch"), TEXT("hide"), TEXT("cover") });
	}
}

void UAoCNPCBrain::ExecuteSocial()
{
	if (AnimPlayer && !AnimPlayer->IsPlaying())
	{
		PlayFromLab(TEXT("social"), TEXT("standing"), { TEXT("talk"), TEXT("gesture"), TEXT("wave") });
	}
}

void UAoCNPCBrain::ExecuteDead()
{
	// Play death once — if AnimPlayer is already playing a death anim, don't restart
	if (AnimPlayer && AnimPlayer->IsPlaying()) return;

	PlayFromLab(TEXT("death"), TEXT("standing"), { TEXT("death"), TEXT("dying") });
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UAoCNPCBrain::ForceGoal(EAoCNPCGoal NewGoal)
{
	SetGoal(NewGoal);
}

void UAoCNPCBrain::NotifyHit(const FVector& HitDirection, float Damage)
{
	if (CurrentGoal == EAoCNPCGoal::Dead) return;

	// Play hit-react animation
	if (AnimPlayer && AnimLab)
	{
		TArray<FString> Tags = { TEXT("react"), TEXT("hit") };

		// Determine direction tag
		const FVector Forward = GetOwner()->GetActorForwardVector();
		const float Dot = FVector::DotProduct(Forward, HitDirection.GetSafeNormal());
		if (Dot > 0.5f)      Tags.Add(TEXT("front"));
		else if (Dot < -0.5f) Tags.Add(TEXT("back"));
		else
		{
			const FVector Right = GetOwner()->GetActorRightVector();
			const float RightDot = FVector::DotProduct(Right, HitDirection.GetSafeNormal());
			Tags.Add(RightDot > 0.f ? TEXT("right") : TEXT("left"));
		}

		FAoCAnimationEntry HitReact = AnimLab->FindBestMatch(TEXT("combat_melee"), TEXT("standing"), Tags);
		AnimPlayer->InterruptWith(HitReact);
	}

	// Immediately switch to combat if not already
	if (CurrentGoal != EAoCNPCGoal::Combat && CurrentGoal != EAoCNPCGoal::Dead)
	{
		SetGoal(EAoCNPCGoal::Combat);
	}
}

void UAoCNPCBrain::NotifyNoise(const FVector& NoiseLocation, float Loudness)
{
	if (CurrentGoal == EAoCNPCGoal::Dead || CurrentGoal == EAoCNPCGoal::Combat) return;

	float Dist = FVector::Dist(GetOwner()->GetActorLocation(), NoiseLocation);
	if (Dist <= HearingRadius * Loudness)
	{
		InvestigateLocation = NoiseLocation;
		bHasInvestigateTarget = true;
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UAoCNPCBrain::PlayFromLab(const FString& Category, const FString& BodyState, const TArray<FString>& Tags)
{
	if (!AnimLab || !AnimPlayer) return;

	FAoCAnimationEntry Entry = AnimLab->FindBestMatch(Category, BodyState, Tags);
	AnimPlayer->PlayEntry(Entry);
}

void UAoCNPCBrain::MoveToward(const FVector& Location, float Speed)
{
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char) return;

	UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
	if (!CMC) return;

	const FVector MyLoc = Char->GetActorLocation();
	FVector Dir = (Location - MyLoc).GetSafeNormal2D();

	// Face movement direction
	if (!Dir.IsNearlyZero())
	{
		Char->SetActorRotation(Dir.Rotation());
	}

	CMC->MaxWalkSpeed = Speed;
	Char->AddMovementInput(Dir, 1.0f);
}

FVector UAoCNPCBrain::GetDirectionTo(const AActor* Target) const
{
	if (!Target) return FVector::ForwardVector;
	return (Target->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
}
