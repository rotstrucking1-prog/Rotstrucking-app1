// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AoCCharacterBase.h"
#include "AoCPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAoCInventoryComponent;
class UAoCSpellCastingComponent;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteract, AActor*, InteractedActor);

/**
 * AAoCPlayerCharacter
 * Player-controlled character with camera, inventory, spellcasting, and Enhanced Input.
 */
UCLASS()
class AOC_API AAoCPlayerCharacter : public AAoCCharacterBase
{
	GENERATED_BODY()

public:
	AAoCPlayerCharacter();

	// ── Components ──

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Components")
	TObjectPtr<UAoCInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Components")
	TObjectPtr<UAoCSpellCastingComponent> SpellCastingComponent;

	// ── Input Actions ──

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_CastSpell;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Interact;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_ToggleInventory;

	// ── Sprint Settings ──

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Movement")
	float SprintSpeedMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Movement")
	float SprintStaminaDrainPerSecond;

	UPROPERTY(BlueprintReadOnly, Category = "AoC|Movement")
	bool bIsSprinting;

	/** Normal walk speed, cached at BeginPlay. */
	UPROPERTY(BlueprintReadOnly, Category = "AoC|Movement")
	float BaseWalkSpeed;

	// ── Interact ──

	/** Maximum distance for interaction line trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AoC|Interact")
	float InteractTraceDistance;

	/** Perform a line trace forward and interact with the first valid actor. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Interact")
	void PerformInteract();

	UPROPERTY(BlueprintAssignable, Category = "AoC|Interact")
	FOnInteract OnInteract;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	// Input handlers
	void HandleMove(const struct FInputActionValue& Value);
	void HandleLook(const struct FInputActionValue& Value);
	void HandleJump();
	void HandleStopJumping();
	void HandleSprintStart();
	void HandleSprintStop();
	void HandleAttack();
	void HandleCastSpell();
	void HandleInteract();
	void HandleToggleInventory();

	/** Drain stamina while sprinting. */
	void TickSprint(float DeltaTime);
};
