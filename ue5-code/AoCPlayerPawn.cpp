#include "AoCPlayerPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"

AAoCPlayerPawn::AAoCPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->JumpZVelocity = 500.f;

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement to rotate toward movement direction
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Setup camera boom (SpringArm) - raised to shoulder height
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;

	// Setup follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	// Setup mesh transform for Mixamo FBX orientation
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(-90.f, 0.f, 270.f));

	// Init state
	CurrentMoveState = EAoCMoveState::Idle;
	IdleAnimation = nullptr;
	WalkAnimation = nullptr;
	RunAnimation = nullptr;
	JumpAnimation = nullptr;
}

void AAoCPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Load animations from content
	IdleAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AoC/Animations/Movement/Idle.Idle"));
	WalkAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AoC/Animations/Movement/Walk.Walk"));
	RunAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AoC/Animations/Movement/Run.Run"));
	JumpAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/AoC/Animations/Movement/Jump.Jump"));

	// Set animation mode to single-node (direct playback, no ABP needed)
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	// Start with idle animation
	if (IdleAnimation)
	{
		GetMesh()->PlayAnimation(IdleAnimation, true);
	}

	// Add Input Mapping Contexts
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
			if (IMC)
			{
				Subsystem->AddMappingContext(IMC, 0);
			}
			UInputMappingContext* IMC_Mouse = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
			if (IMC_Mouse)
			{
				Subsystem->AddMappingContext(IMC_Mouse, 1);
			}
		}
	}
}

void AAoCPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Determine movement state from velocity
	float Speed = GetVelocity().Size();
	bool bFalling = GetCharacterMovement()->IsFalling();

	EAoCMoveState NewState;
	if (bFalling)
		NewState = EAoCMoveState::Jumping;
	else if (Speed > 300.f)
		NewState = EAoCMoveState::Running;
	else if (Speed > 10.f)
		NewState = EAoCMoveState::Walking;
	else
		NewState = EAoCMoveState::Idle;

	// Only switch animation when state changes
	if (NewState != CurrentMoveState)
	{
		CurrentMoveState = NewState;
		UAnimSequence* Anim = nullptr;
		bool bLoop = true;

		switch (CurrentMoveState)
		{
			case EAoCMoveState::Idle:    Anim = IdleAnimation; break;
			case EAoCMoveState::Walking: Anim = WalkAnimation; break;
			case EAoCMoveState::Running: Anim = RunAnimation; break;
			case EAoCMoveState::Jumping: Anim = JumpAnimation; bLoop = false; break;
		}

		if (Anim)
		{
			GetMesh()->PlayAnimation(Anim, bLoop);
		}
	}
}

void AAoCPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UInputAction* MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
		UInputAction* LookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
		UInputAction* MouseLookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
		UInputAction* JumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));

		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAoCPlayerPawn::MoveTriggered);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAoCPlayerPawn::LookTriggered);
		}
		if (MouseLookAction)
		{
			EnhancedInput->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AAoCPlayerPawn::LookTriggered);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AAoCPlayerPawn::JumpTriggered);
		}
	}
}

void AAoCPlayerPawn::MoveTriggered(const FInputActionValue& Value)
{
	FVector2D MoveInput = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, MoveInput.Y);
		AddMovementInput(RightDir, MoveInput.X);
	}
}

void AAoCPlayerPawn::LookTriggered(const FInputActionValue& Value)
{
	FVector2D LookInput = Value.Get<FVector2D>();
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AAoCPlayerPawn::JumpTriggered(const FInputActionValue& Value)
{
	Jump();
}
