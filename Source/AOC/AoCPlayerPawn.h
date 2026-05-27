#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AoCPlayerPawn.generated.h"

struct FInputActionValue;

UENUM()
enum class EAoCMoveState : uint8
{
	Idle,
	Walking,
	Running,
	Jumping
};

UCLASS()
class AOC_API AAoCPlayerPawn : public ACharacter
{
	GENERATED_BODY()

public:
	AAoCPlayerPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* FollowCamera;

	// Current movement state (readable in Blueprints)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	EAoCMoveState CurrentMoveState;

private:
	// Input handlers
	void MoveTriggered(const FInputActionValue& Value);
	void LookTriggered(const FInputActionValue& Value);
	void JumpTriggered(const FInputActionValue& Value);

	// Animation assets (loaded at runtime)
	UPROPERTY()
	class UAnimSequence* IdleAnimation;
	UPROPERTY()
	class UAnimSequence* WalkAnimation;
	UPROPERTY()
	class UAnimSequence* RunAnimation;
	UPROPERTY()
	class UAnimSequence* JumpAnimation;
};
