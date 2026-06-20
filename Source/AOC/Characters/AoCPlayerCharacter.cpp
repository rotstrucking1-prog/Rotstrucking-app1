// Copyright Architect of Creation. All Rights Reserved.

#include "AoCPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AAoCPlayerCharacter::AAoCPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Spring arm for third person camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 400.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bDoCollisionTest = true;

	// Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// Inventory
	InventoryComponent = CreateDefaultSubobject<UAoCInventoryComponent>(TEXT("InventoryComponent"));

	// Spellcasting
	SpellCastingComponent = CreateDefaultSubobject<UAoCSpellCastingComponent>(TEXT("SpellCastingComponent"));

	// Movement defaults
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	SprintSpeedMultiplier = 1.5f;
	SprintStaminaDrainPerSecond = 10.0f;
	bIsSprinting = false;
	BaseWalkSpeed = 600.0f;
	InteractTraceDistance = 300.0f;
}

void AAoCPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	// Setup enhanced input mapping context for local player
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (PlayerMappingContext)
			{
				Subsystem->AddMappingContext(PlayerMappingContext, 1);
			}
		}
	}
}

void AAoCPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickSprint(DeltaTime);
}

void AAoCPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (IA_Move)
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AAoCPlayerCharacter::HandleMove);
	}
	if (IA_Look)
	{
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AAoCPlayerCharacter::HandleLook);
	}
	if (IA_Jump)
	{
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &AAoCPlayerCharacter::HandleJump);
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AAoCPlayerCharacter::HandleStopJumping);
	}
	if (IA_Sprint)
	{
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AAoCPlayerCharacter::HandleSprintStart);
		EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AAoCPlayerCharacter::HandleSprintStop);
	}
	if (IA_Attack)
	{
		EnhancedInput->BindAction(IA_Attack, ETriggerEvent::Started, this, &AAoCPlayerCharacter::HandleAttack);
	}
	if (IA_CastSpell)
	{
		EnhancedInput->BindAction(IA_CastSpell, ETriggerEvent::Started, this, &AAoCPlayerCharacter::HandleCastSpell);
	}
	if (IA_Interact)
	{
		EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &AAoCPlayerCharacter::HandleInteract);
	}
	if (IA_ToggleInventory)
	{
		EnhancedInput->BindAction(IA_ToggleInventory, ETriggerEvent::Started, this, &AAoCPlayerCharacter::HandleToggleInventory);
	}
}

void AAoCPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	const FVector2D MoveVector = Value.Get<FVector2D>();

	if (GetController())
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, MoveVector.Y);
		AddMovementInput(RightDir, MoveVector.X);
	}
}

void AAoCPlayerCharacter::HandleLook(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	if (GetController())
	{
		AddControllerYawInput(LookVector.X);
		AddControllerPitchInput(LookVector.Y);
	}
}

void AAoCPlayerCharacter::HandleJump()
{
	Jump();
}

void AAoCPlayerCharacter::HandleStopJumping()
{
	StopJumping();
}

void AAoCPlayerCharacter::HandleSprintStart()
{
	if (GetStamina() > 0.0f)
	{
		bIsSprinting = true;
		GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * SprintSpeedMultiplier;
	}
}

void AAoCPlayerCharacter::HandleSprintStop()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AAoCPlayerCharacter::HandleAttack()
{
	// Trigger primary weapon attack via GAS
	// This would activate a gameplay ability tagged with "Ability.Attack"
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Attack")));
		AbilitySystemComponent->TryActivateAbilitiesByTag(TagContainer);
	}
}

void AAoCPlayerCharacter::HandleCastSpell()
{
	// Trigger currently selected spell slot (slot 0 default via button press)
	if (SpellCastingComponent)
	{
		// The SpellCastingComponent handles mana check, cooldowns, cast time
		// Default to slot 0; specific slot selection handled by number keys in widget
	}
}

void AAoCPlayerCharacter::HandleInteract()
{
	PerformInteract();
}

void AAoCPlayerCharacter::HandleToggleInventory()
{
	// Delegate to PlayerController
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		// The PlayerController handles UI toggling
	}
}

void AAoCPlayerCharacter::PerformInteract()
{
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + (Camera->GetForwardVector() * InteractTraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	if (bHit && HitResult.GetActor())
	{
		OnInteract.Broadcast(HitResult.GetActor());

		UE_LOG(LogTemp, Log, TEXT("AoCPlayerCharacter: Interacted with %s"), *HitResult.GetActor()->GetName());
	}
}

void AAoCPlayerCharacter::TickSprint(float DeltaTime)
{
	if (bIsSprinting && IsAlive())
	{
		float CurrentStamina = GetStamina();
		float Drain = SprintStaminaDrainPerSecond * DeltaTime;

		if (CurrentStamina <= Drain)
		{
			// Out of stamina, stop sprinting
			HandleSprintStop();
		}
		else if (AttributeSet)
		{
			// Drain stamina — in a full GAS pipeline this would use a GameplayEffect.
			// Direct set for simplicity; production code should use a GE.
			AttributeSet->SetStamina(CurrentStamina - Drain);
		}
	}
}
