#include "AoCNPCCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

AAoCNPCCharacter::AAoCNPCCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Defaults
    NPCName = TEXT("NPC");
    CurrentState = ENPCState::Idle;
    MaxHealth = 100.f;
    CurrentHealth = 100.f;
    AggroRadius = 1500.f;
    AttackRange = 200.f;
    PatrolRadius = 800.f;
    WalkSpeed = 150.f;
    RunSpeed = 400.f;
    bIsHostile = true;
    bCanPatrol = true;
    StateTimer = 0.f;
    IdleTime = 0.f;
    CurrentAnim = nullptr;

    // Movement defaults
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

    // Don't use controller rotation
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;
}

void AAoCNPCCharacter::BeginPlay()
{
    Super::BeginPlay();
    SpawnLocation = GetActorLocation();
    CurrentHealth = MaxHealth;

    // Start with idle
    SetNPCState(ENPCState::Idle);
}

void AAoCNPCCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentState == ENPCState::Dead) return;

    StateTimer += DeltaTime;

    switch (CurrentState)
    {
    case ENPCState::Idle:
        UpdateIdle(DeltaTime);
        break;
    case ENPCState::Patrol:
        UpdatePatrol(DeltaTime);
        break;
    case ENPCState::Alert:
        UpdateAlert(DeltaTime);
        break;
    case ENPCState::Combat:
        UpdateCombat(DeltaTime);
        break;
    default:
        break;
    }
}

void AAoCNPCCharacter::UpdateIdle(float DeltaTime)
{
    IdleTime += DeltaTime;

    // Check for nearby players (aggro)
    if (bIsHostile)
    {
        AActor* Player = FindNearestPlayer();
        if (Player)
        {
            float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
            if (Dist < AggroRadius)
            {
                SetNPCState(ENPCState::Alert);
                return;
            }
        }
    }

    // After idling for a while, start patrol
    if (bCanPatrol && IdleTime > FMath::RandRange(5.f, 15.f))
    {
        SetNPCState(ENPCState::Patrol);
    }
}

void AAoCNPCCharacter::UpdatePatrol(float DeltaTime)
{
    // Check for aggro
    if (bIsHostile)
    {
        AActor* Player = FindNearestPlayer();
        if (Player)
        {
            float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
            if (Dist < AggroRadius)
            {
                SetNPCState(ENPCState::Alert);
                return;
            }
        }
    }

    // Move toward patrol target
    FVector ToTarget = PatrolTarget - GetActorLocation();
    ToTarget.Z = 0;
    float DistToTarget = ToTarget.Size();

    if (DistToTarget < 100.f)
    {
        // Reached patrol point, go back to idle
        SetNPCState(ENPCState::Idle);
        return;
    }

    // Walk toward target
    FVector Direction = ToTarget.GetSafeNormal();
    AddMovementInput(Direction, 1.0f);

    // Timeout - if stuck for too long, go idle
    if (StateTimer > 20.f)
    {
        SetNPCState(ENPCState::Idle);
    }
}

void AAoCNPCCharacter::UpdateAlert(float DeltaTime)
{
    AActor* Player = FindNearestPlayer();
    if (!Player)
    {
        SetNPCState(ENPCState::Idle);
        return;
    }

    float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());

    // Lost aggro
    if (Dist > AggroRadius * 1.5f)
    {
        SetNPCState(ENPCState::Idle);
        return;
    }

    // Close enough to attack
    if (Dist < AttackRange)
    {
        SetNPCState(ENPCState::Combat);
        return;
    }

    // Chase player
    GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
    FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
    ToPlayer.Z = 0;
    AddMovementInput(ToPlayer.GetSafeNormal(), 1.0f);

    // Play run anim
    if (RunAnim && CurrentAnim != RunAnim)
    {
        PlayNPCAnimation(RunAnim);
    }
}

void AAoCNPCCharacter::UpdateCombat(float DeltaTime)
{
    AActor* Player = FindNearestPlayer();
    if (!Player)
    {
        SetNPCState(ENPCState::Idle);
        return;
    }

    float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());

    // Player ran away
    if (Dist > AttackRange * 2.f)
    {
        SetNPCState(ENPCState::Alert);
        return;
    }

    // Face the player
    FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
    ToPlayer.Z = 0;
    FRotator LookRot = ToPlayer.Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookRot, GetWorld()->GetDeltaSeconds(), 5.f));

    // Attack on timer
    if (StateTimer > 2.f)
    {
        if (AttackAnim)
        {
            PlayNPCAnimation(AttackAnim, false);
        }
        StateTimer = 0.f;
    }
}

void AAoCNPCCharacter::PlayNPCAnimation(UAnimSequence* Anim, bool bLoop)
{
    if (!Anim) return;
    CurrentAnim = Anim;
    
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (MeshComp)
    {
        MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        MeshComp->PlayAnimation(Anim, bLoop);
    }
}

void AAoCNPCCharacter::SetNPCState(ENPCState NewState)
{
    CurrentState = NewState;
    StateTimer = 0.f;

    switch (NewState)
    {
    case ENPCState::Idle:
        IdleTime = 0.f;
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
        if (IdleAnim)
            PlayNPCAnimation(IdleAnim);
        break;

    case ENPCState::Patrol:
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
        PickNewPatrolPoint();
        if (WalkAnim)
            PlayNPCAnimation(WalkAnim);
        break;

    case ENPCState::Alert:
        GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
        if (AlertAnim)
            PlayNPCAnimation(AlertAnim);
        else if (RunAnim)
            PlayNPCAnimation(RunAnim);
        break;

    case ENPCState::Combat:
        GetCharacterMovement()->MaxWalkSpeed = 0.f;
        break;

    case ENPCState::Dead:
        GetCharacterMovement()->DisableMovement();
        if (DeathAnim)
            PlayNPCAnimation(DeathAnim, false);
        break;
    }
}

float AAoCNPCCharacter::TakeDamageNPC(float DamageAmount)
{
    if (CurrentState == ENPCState::Dead) return 0.f;

    CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);

    if (CurrentHealth <= 0.f)
    {
        SetNPCState(ENPCState::Dead);
    }
    else if (CurrentState == ENPCState::Idle || CurrentState == ENPCState::Patrol)
    {
        // Getting hit triggers alert
        SetNPCState(ENPCState::Alert);
    }

    return CurrentHealth;
}

void AAoCNPCCharacter::PickNewPatrolPoint()
{
    // Random point within patrol radius of spawn
    float Angle = FMath::FRandRange(0.f, 360.f);
    float Distance = FMath::FRandRange(200.f, PatrolRadius);
    PatrolTarget = SpawnLocation + FVector(
        FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance,
        FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance,
        0.f
    );
}

AActor* AAoCNPCCharacter::FindNearestPlayer()
{
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    return PlayerPawn;
}
