#include "AoCPlayerPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Mining/AoCMiningComponent.h"
#include "Mining/AoCProspectingComponent.h"
#include "Smelting/AoCSmeltingComponent.h"
#include "Terraforming/AoCTerraformingComponent.h"
#include "Gathering/AoCGatheringComponent.h"
#include "Herbalism/AoCHerbalismComponent.h"
#include "Inventory/AoCInventoryComponent.h"
#include "Skills/AoCSkillComponent.h"

AAoCPlayerPawn::AAoCPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.2f;

	// ── Gameplay Components ─────────────────────────────────────────────────

	MiningComponent = CreateDefaultSubobject<UAoCMiningComponent>(TEXT("MiningComponent"));

	ProspectingComponent = CreateDefaultSubobject<UAoCProspectingComponent>(TEXT("ProspectingComponent"));

	SmeltingComponent = CreateDefaultSubobject<UAoCSmeltingComponent>(TEXT("SmeltingComponent"));

	TerraformingComponent = CreateDefaultSubobject<UAoCTerraformingComponent>(TEXT("TerraformingComponent"));

	GatheringComponent = CreateDefaultSubobject<UAoCGatheringComponent>(TEXT("GatheringComponent"));

	HerbalismComponent = CreateDefaultSubobject<UAoCHerbalismComponent>(TEXT("HerbalismComponent"));

	InventoryComponent = CreateDefaultSubobject<UAoCInventoryComponent>(TEXT("InventoryComponent"));

	SkillComponent = CreateDefaultSubobject<UAoCSkillComponent>(TEXT("SkillComponent"));
}

void AAoCPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

void AAoCPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	const FRotator CamRot = Controller->GetControlRotation();
	const FRotator YawOnly(0.0f, CamRot.Yaw, 0.0f);

	const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	if (PC->IsInputKeyDown(EKeys::W)) AddMovementInput(Forward, 1.0f);
	if (PC->IsInputKeyDown(EKeys::S)) AddMovementInput(Forward, -1.0f);
	if (PC->IsInputKeyDown(EKeys::A)) AddMovementInput(Right, -1.0f);
	if (PC->IsInputKeyDown(EKeys::D)) AddMovementInput(Right, 1.0f);

	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar)) Jump();

	float MouseX, MouseY;
	PC->GetInputMouseDelta(MouseX, MouseY);
	AddControllerYawInput(MouseX);
	AddControllerPitchInput(MouseY * -1.0f);
}
