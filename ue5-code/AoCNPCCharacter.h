#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AoCNPCCharacter.generated.h"

UENUM(BlueprintType)
enum class ENPCState : uint8
{
    Idle,
    Patrol,
    Alert,
    Combat,
    Dead
};

UCLASS()
class AOC_API AAoCNPCCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAoCNPCCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // NPC Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    FString NPCName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    ENPCState CurrentState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float MaxHealth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float CurrentHealth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float AggroRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float AttackRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float PatrolRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float WalkSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    float RunSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    bool bIsHostile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
    bool bCanPatrol;

    // Animation references - set in Blueprint
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
    UAnimSequence* IdleAnim;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
    UAnimSequence* WalkAnim;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
    UAnimSequence* RunAnim;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
    UAnimSequence* AttackAnim;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
    UAnimSequence* DeathAnim;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Animations")
    UAnimSequence* AlertAnim;

    // Functions
    UFUNCTION(BlueprintCallable, Category = "NPC")
    void PlayNPCAnimation(UAnimSequence* Anim, bool bLoop = true);

    UFUNCTION(BlueprintCallable, Category = "NPC")
    void SetNPCState(ENPCState NewState);

    UFUNCTION(BlueprintCallable, Category = "NPC")
    float TakeDamageNPC(float DamageAmount);

private:
    FVector SpawnLocation;
    FVector PatrolTarget;
    float StateTimer;
    float IdleTime;
    UAnimSequence* CurrentAnim;

    void UpdateIdle(float DeltaTime);
    void UpdatePatrol(float DeltaTime);
    void UpdateAlert(float DeltaTime);
    void UpdateCombat(float DeltaTime);
    void PickNewPatrolPoint();
    AActor* FindNearestPlayer();
};
