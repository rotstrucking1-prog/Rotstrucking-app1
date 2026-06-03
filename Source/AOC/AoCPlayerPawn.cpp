// AoCPlayerPawn.cpp
// Architect of Creation — Player Pawn Implementation
// Full interaction system with rebindable keys, raycast targeting,
// animation playback for every action, and on-screen HUD.

#include "AoCPlayerPawn.h"

// Engine
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"

// Game Components
#include "Mining/AoCMiningComponent.h"
#include "Mining/AoCProspectingComponent.h"
#include "Mining/AoCVoxelTypes.h"
#include "Mining/AoCOreVein.h"
#include "Smelting/AoCSmeltingComponent.h"
#include "Smelting/AoCFurnaceActor.h"
#include "Terraforming/AoCTerraformingComponent.h"
#include "Gathering/AoCGatheringComponent.h"
#include "Gathering/AoCResourceNode.h"
#include "Herbalism/AoCHerbalismComponent.h"
#include "Inventory/AoCInventoryComponent.h"
#include "Skills/AoCSkillComponent.h"
#include "TerraForge/TerraForgeComponent.h"
#include "UI/Widgets/AoCHUDWidget.h"
#include "Blueprint/UserWidget.h"

// ============================================================================
// CONSTRUCTION & LIFECYCLE
// ============================================================================

AAoCPlayerPawn::AAoCPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Camera rotation follows controller, character body doesn't
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// ── Camera Boom ─────────────────────────────────────────────────────────
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength       = 600.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest       = false;
	CameraBoom->SocketOffset           = FVector(0.0f, 60.0f, 0.0f);
	CameraBoom->TargetOffset           = FVector(0.0f, 0.0f, 100.0f);
	CameraBoom->bInheritPitch          = true;
	CameraBoom->bInheritYaw            = true;
	CameraBoom->bInheritRoll           = false;

	// ── Follow Camera ───────────────────────────────────────────────────────
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->bUsePawnControlRotation = false;

	// ── Character Movement ──────────────────────────────────────────────────
	GetCharacterMovement()->MaxWalkSpeed            = 600.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate             = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity            = 600.0f;
	GetCharacterMovement()->AirControl               = 0.2f;

	// ── Gameplay Components ─────────────────────────────────────────────────
	MiningComponent       = CreateDefaultSubobject<UAoCMiningComponent>(TEXT("MiningComponent"));
	ProspectingComponent  = CreateDefaultSubobject<UAoCProspectingComponent>(TEXT("ProspectingComponent"));
	SmeltingComponent     = CreateDefaultSubobject<UAoCSmeltingComponent>(TEXT("SmeltingComponent"));
	TerraformingComponent = CreateDefaultSubobject<UAoCTerraformingComponent>(TEXT("TerraformingComponent"));
	GatheringComponent    = CreateDefaultSubobject<UAoCGatheringComponent>(TEXT("GatheringComponent"));
	HerbalismComponent    = CreateDefaultSubobject<UAoCHerbalismComponent>(TEXT("HerbalismComponent"));
	InventoryComponent    = CreateDefaultSubobject<UAoCInventoryComponent>(TEXT("InventoryComponent"));
	SkillComponent        = CreateDefaultSubobject<UAoCSkillComponent>(TEXT("SkillComponent"));
	TerraForgeComp        = CreateDefaultSubobject<UTerraForgeComponent>(TEXT("TerraForgeComp"));

	// ── Default state ───────────────────────────────────────────────────────
	InteractionRange    = 800.0f;
	CurrentTarget       = EInteractionTarget::Nothing;
	CurrentMode         = EInteractionMode::Default;
	TargetActor         = nullptr;
	TargetLocation      = FVector::ZeroVector;
	TargetNormal        = FVector::UpVector;
	ActionCooldown      = 0.0f;
	NotificationTimer   = 0.0f;
	bShowingInventory   = false;
	bShowingSkills      = false;
	bPlayingActionAnim  = false;

	// ── Null animation pointers ─────────────────────────────────────────────
	AnimIdle          = nullptr;
	AnimMiningSwing   = nullptr;
	AnimDigging       = nullptr;
	AnimGatherHerb    = nullptr;
	AnimPickupItem    = nullptr;
	AnimProspecting   = nullptr;
	AnimObserveGround = nullptr;
	AnimSmelting      = nullptr;
	AnimWalk          = nullptr;
	AnimRun           = nullptr;
}

void AAoCPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Lock cursor for gameplay
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	// Load all animation assets
	LoadAllAnimations();

	// Play idle animation on PeasantMan mesh
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh && AnimIdle)
	{
		SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkelMesh->PlayAnimation(AnimIdle, true);
	}

	// Initialize rebindable key bindings
	SetupDefaultKeyBindings();
	LoadKeyBindings();

	// Create and display the HUD widget
	CreateHUDWidget();
}

void AAoCPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Update timers
	if (ActionCooldown > 0.0f)    ActionCooldown    -= DeltaTime;
	if (NotificationTimer > 0.0f) NotificationTimer -= DeltaTime;

	// Core per-frame systems
	UpdateInteractionTarget();
	UpdateLookAtTooltip();
	ProcessMovement(PC);
	ProcessCamera(PC);
	ProcessInteractionInput(PC);
	DisplayHUD();
}

// ============================================================================
// ANIMATION SYSTEM — Load, play, and auto-return to idle
// ============================================================================

UAnimSequence* AAoCPlayerPawn::TryLoadAnim(const TCHAR* Path)
{
	UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, Path);
	if (Anim)
	{
		UE_LOG(LogTemp, Log, TEXT("AoC Anim loaded: %s"), Path);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AoC Anim NOT found: %s"), Path);
	}
	return Anim;
}

void AAoCPlayerPawn::LoadAllAnimations()
{
	// Idle — PeasantMan default idle
	AnimIdle = TryLoadAnim(
		TEXT("/Game/AoC/Characters/PeasantMan/SK_PeasantMan_Anim.SK_PeasantMan_Anim"));

	// Mining — overhead downward strike (pickaxe swing)
	AnimMiningSwing = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Movement/Standing_Melee_Attack_Downward.Standing_Melee_Attack_Downward"));

	// Digging / Terraforming — kneeling dig and plant
	AnimDigging = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Combat_Melee/Kneeling_Digging_And_Planting_Seeds.Kneeling_Digging_And_Planting_Seeds"));

	// Gathering herbs — pick fruit from bush
	AnimGatherHerb = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Crafting/Pick_Fruit.Pick_Fruit"));

	// Picking up items from ground
	AnimPickupItem = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Crafting/Picking_Up_Object.Picking_Up_Object"));

	// Prospecting — crouch and examine ground
	AnimProspecting = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Movement/Standing_To_Kneeling_Down.Standing_To_Kneeling_Down"));

	// Observe ground — look around carefully
	AnimObserveGround = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Idle/Look_Around.Look_Around"));

	// Smelting — working at furnace (bartending motion works for stirring/pouring)
	AnimSmelting = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Misc/Bartending.Bartending"));

	// Walk cycle
	AnimWalk = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Movement/Standard_Walk.Standard_Walk"));

	// Run cycle
	AnimRun = TryLoadAnim(
		TEXT("/Game/AoC/Animations/Movement/Standing_Run_Forward.Standing_Run_Forward"));

	// Fallback: if specific anims failed, try alternatives
	if (!AnimMiningSwing)
	{
		AnimMiningSwing = TryLoadAnim(
			TEXT("/Game/AoC/Animations/Combat_Melee/Swinging.Swinging"));
	}
	if (!AnimDigging)
	{
		AnimDigging = TryLoadAnim(
			TEXT("/Game/AoC/Animations/Crafting/Dig_And_Plant_Seeds.Dig_And_Plant_Seeds"));
	}
	if (!AnimGatherHerb)
	{
		AnimGatherHerb = TryLoadAnim(
			TEXT("/Game/AoC/Animations/Crafting/Picking_Fruit_At_Eye_Level.Picking_Fruit_At_Eye_Level"));
	}
	if (!AnimProspecting)
	{
		AnimProspecting = TryLoadAnim(
			TEXT("/Game/AoC/Animations/Movement/Crouch_Idle_01.Crouch_Idle_01"));
	}

	const int32 Loaded = (AnimIdle ? 1 : 0) + (AnimMiningSwing ? 1 : 0) +
		(AnimDigging ? 1 : 0) + (AnimGatherHerb ? 1 : 0) + (AnimPickupItem ? 1 : 0) +
		(AnimProspecting ? 1 : 0) + (AnimObserveGround ? 1 : 0) + (AnimSmelting ? 1 : 0) +
		(AnimWalk ? 1 : 0) + (AnimRun ? 1 : 0);
	UE_LOG(LogTemp, Log, TEXT("AoC: Loaded %d/10 animations"), Loaded);
}

void AAoCPlayerPawn::PlayActionAnimation(UAnimSequence* Anim, float OverrideDuration)
{
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (!SkelMesh || !Anim) return;

	// Cancel any pending return-to-idle timer
	GetWorldTimerManager().ClearTimer(AnimReturnTimerHandle);

	bPlayingActionAnim = true;
	SkelMesh->PlayAnimation(Anim, false); // false = one-shot, not looping

	// Calculate how long to wait before returning to idle
	const float Duration = (OverrideDuration > 0.0f) ? OverrideDuration : Anim->GetPlayLength();

	// Set timer to auto-return to idle after animation finishes
	GetWorldTimerManager().SetTimer(
		AnimReturnTimerHandle,
		this,
		&AAoCPlayerPawn::ReturnToIdle,
		Duration,
		false // not looping
	);

	UE_LOG(LogTemp, Verbose, TEXT("AoC: Playing anim %s (%.2fs)"),
		*Anim->GetName(), Duration);
}

void AAoCPlayerPawn::ReturnToIdle()
{
	bPlayingActionAnim = false;

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh && AnimIdle)
	{
		SkelMesh->PlayAnimation(AnimIdle, true); // loop idle
	}
}

// ============================================================================
// KEY BINDING SYSTEM — All keys rebindable, saved to config
// ============================================================================

void AAoCPlayerPawn::SetupDefaultKeyBindings()
{
	KeyBindings.Empty();

	// Movement
	KeyBindings.Add(FName("MoveForward"), EKeys::W);
	KeyBindings.Add(FName("MoveBack"),    EKeys::S);
	KeyBindings.Add(FName("MoveLeft"),    EKeys::A);
	KeyBindings.Add(FName("MoveRight"),   EKeys::D);
	KeyBindings.Add(FName("Jump"),        EKeys::SpaceBar);
	KeyBindings.Add(FName("Sprint"),      EKeys::LeftShift);

	// Interaction
	KeyBindings.Add(FName("Interact"),      EKeys::E);
	KeyBindings.Add(FName("ObserveGround"), EKeys::G);
	KeyBindings.Add(FName("Terraform"),     EKeys::T);
	KeyBindings.Add(FName("MiningMenu"),    EKeys::M);
	KeyBindings.Add(FName("Prospect"),      EKeys::R);
	KeyBindings.Add(FName("Inventory"),     EKeys::I);
	KeyBindings.Add(FName("Skills"),        EKeys::Tab);
	KeyBindings.Add(FName("Cancel"),        EKeys::Escape);

	// Action slots (sub-menus: terraform 1-5, mining 1-3)
	KeyBindings.Add(FName("Slot1"), EKeys::One);
	KeyBindings.Add(FName("Slot2"), EKeys::Two);
	KeyBindings.Add(FName("Slot3"), EKeys::Three);
	KeyBindings.Add(FName("Slot4"), EKeys::Four);
	KeyBindings.Add(FName("Slot5"), EKeys::Five);
	KeyBindings.Add(FName("Slot6"), EKeys::Six);
}

void AAoCPlayerPawn::LoadKeyBindings()
{
	FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("AoCKeyBindings.ini");
	if (!FPaths::FileExists(ConfigPath)) return;

	for (auto& Pair : KeyBindings)
	{
		FString KeyStr;
		if (GConfig->GetString(TEXT("KeyBindings"), *Pair.Key.ToString(), KeyStr, ConfigPath))
		{
			FKey LoadedKey(*KeyStr);
			if (LoadedKey.IsValid())
			{
				Pair.Value = LoadedKey;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AoC: Key bindings loaded from %s"), *ConfigPath);
}

void AAoCPlayerPawn::SaveKeyBindings()
{
	FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("AoCKeyBindings.ini");
	for (const auto& Pair : KeyBindings)
	{
		GConfig->SetString(TEXT("KeyBindings"), *Pair.Key.ToString(),
			*Pair.Value.GetFName().ToString(), ConfigPath);
	}
	GConfig->Flush(false, ConfigPath);
	UE_LOG(LogTemp, Log, TEXT("AoC: Key bindings saved to %s"), *ConfigPath);
}

void AAoCPlayerPawn::RebindKey(FName ActionName, FKey NewKey)
{
	if (KeyBindings.Contains(ActionName))
	{
		KeyBindings[ActionName] = NewKey;
		SaveKeyBindings();
		ShowNotification(FString::Printf(TEXT("%s rebound to %s"),
			*ActionName.ToString(), *NewKey.GetDisplayName().ToString()), FColor::Cyan);
	}
}

FKey AAoCPlayerPawn::GetKeyForAction(FName ActionName) const
{
	const FKey* Key = KeyBindings.Find(ActionName);
	return Key ? *Key : EKeys::Invalid;
}

void AAoCPlayerPawn::ResetKeysToDefaults()
{
	SetupDefaultKeyBindings();
	SaveKeyBindings();
	ShowNotification(TEXT("All keys reset to defaults"), FColor::Cyan);
}

TArray<FName> AAoCPlayerPawn::GetAllActionNames() const
{
	TArray<FName> Names;
	KeyBindings.GetKeys(Names);
	return Names;
}

// ============================================================================
// INPUT PROCESSING
// ============================================================================

void AAoCPlayerPawn::ProcessMovement(APlayerController* PC)
{
	// Don't move while playing an action animation
	if (bPlayingActionAnim) return;

	const FRotator CamRot  = Controller->GetControlRotation();
	const FRotator YawOnly(0.0f, CamRot.Yaw, 0.0f);
	const FVector Forward  = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right    = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

	bool bMoving = false;
	if (PC->IsInputKeyDown(KeyBindings.FindRef(FName("MoveForward")))) { AddMovementInput(Forward,  1.0f); bMoving = true; }
	if (PC->IsInputKeyDown(KeyBindings.FindRef(FName("MoveBack"))))    { AddMovementInput(Forward, -1.0f); bMoving = true; }
	if (PC->IsInputKeyDown(KeyBindings.FindRef(FName("MoveLeft"))))    { AddMovementInput(Right,   -1.0f); bMoving = true; }
	if (PC->IsInputKeyDown(KeyBindings.FindRef(FName("MoveRight"))))   { AddMovementInput(Right,    1.0f); bMoving = true; }

	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Jump")))) Jump();

	// Sprint toggle
	const bool bSprinting = PC->IsInputKeyDown(KeyBindings.FindRef(FName("Sprint")));
	GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 900.0f : 600.0f;

	// Movement animation switching (only when not playing an action anim)
	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh && !bPlayingActionAnim)
	{
		if (bMoving && bSprinting && AnimRun)
		{
			// Switch to run animation if not already playing it
			if (SkelMesh->GetSingleNodeInstance() && SkelMesh->GetSingleNodeInstance()->GetCurrentAsset() != AnimRun)
			{
				SkelMesh->PlayAnimation(AnimRun, true);
			}
		}
		else if (bMoving && AnimWalk)
		{
			if (SkelMesh->GetSingleNodeInstance() && SkelMesh->GetSingleNodeInstance()->GetCurrentAsset() != AnimWalk)
			{
				SkelMesh->PlayAnimation(AnimWalk, true);
			}
		}
		else if (!bMoving && AnimIdle)
		{
			if (SkelMesh->GetSingleNodeInstance() && SkelMesh->GetSingleNodeInstance()->GetCurrentAsset() != AnimIdle)
			{
				SkelMesh->PlayAnimation(AnimIdle, true);
			}
		}
	}
}

void AAoCPlayerPawn::ProcessCamera(APlayerController* PC)
{
	float MouseX, MouseY;
	PC->GetInputMouseDelta(MouseX, MouseY);
	AddControllerYawInput(MouseX * 2.0f);

	// Clamp pitch so camera stays between -60° (looking down) and +20° (slightly up)
	FRotator CtrlRot = PC->GetControlRotation();
	float NewPitch = CtrlRot.Pitch + (MouseY * -2.0f);
	// Normalize pitch to -180..180 range for clamping
	if (NewPitch > 180.0f) NewPitch -= 360.0f;
	NewPitch = FMath::Clamp(NewPitch, -60.0f, 20.0f);
	CtrlRot.Pitch = NewPitch;
	PC->SetControlRotation(CtrlRot);
}

void AAoCPlayerPawn::ProcessInteractionInput(APlayerController* PC)
{
	// Don't accept new interaction input while an action animation is playing
	if (bPlayingActionAnim) return;

	// Cancel / Escape — always available, exits any mode
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Cancel"))))
	{
		HandleCancel();
		return;
	}

	// Sub-menu mode: number keys select actions, or toggle key to exit
	if (CurrentMode == EInteractionMode::TerraformMenu ||
		CurrentMode == EInteractionMode::MiningMenu)
	{
		// Allow pressing the SAME mode key again to toggle OFF (T exits terraform, M exits mining)
		if (CurrentMode == EInteractionMode::TerraformMenu &&
			PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Terraform"))))
		{
			HandleCancel();
			return;
		}
		if (CurrentMode == EInteractionMode::MiningMenu &&
			PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("MiningMenu"))))
		{
			HandleCancel();
			return;
		}

		for (int32 i = 1; i <= 6; ++i)
		{
			FName SlotName = FName(*FString::Printf(TEXT("Slot%d"), i));
			if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(SlotName)))
			{
				HandleActionSlot(i);
				return;
			}
		}
		if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Cancel"))))
		{
			HandleCancel();
		}
		return; // Don't process other keys while in menu
	}

	// Default mode — all interaction keys
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Interact"))))      HandleInteract();
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("ObserveGround")))) HandleObserveGround();
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Terraform"))))     HandleTerraformMenu();
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("MiningMenu"))))    HandleMiningMenu();
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Prospect"))))      HandleProspect();
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Inventory"))))     HandleInventoryToggle();
	if (PC->WasInputKeyJustPressed(KeyBindings.FindRef(FName("Skills"))))        HandleSkillsToggle();
}

// ============================================================================
// INTERACTION DETECTION — Raycast from camera center
// ============================================================================

void AAoCPlayerPawn::UpdateInteractionTarget()
{
	FHitResult Hit = PerformInteractionTrace();

	if (Hit.bBlockingHit && Hit.GetActor())
	{
		TargetActor       = Hit.GetActor();
		TargetLocation    = Hit.ImpactPoint;
		TargetNormal      = Hit.ImpactNormal;
		CurrentTarget     = ClassifyActor(TargetActor.Get());
		TargetDisplayName = BuildTargetName(TargetActor.Get(), CurrentTarget);
	}
	else if (Hit.bBlockingHit)
	{
		// Hit landscape/BSP geometry with no actor
		TargetActor       = nullptr;
		TargetLocation    = Hit.ImpactPoint;
		TargetNormal      = Hit.ImpactNormal;
		CurrentTarget     = EInteractionTarget::Ground;
		TargetDisplayName = TEXT("Ground");
	}
	else
	{
		TargetActor       = nullptr;
		TargetLocation    = FVector::ZeroVector;
		TargetNormal      = FVector::UpVector;
		CurrentTarget     = EInteractionTarget::Nothing;
		TargetDisplayName = TEXT("");
	}

	// Visual crosshair indicator at hit point
	if (Hit.bBlockingHit)
	{
		DrawDebugPoint(GetWorld(), TargetLocation, 8.0f, FColor::Yellow, false, -1.0f, 0);
	}
}

FHitResult AAoCPlayerPawn::PerformInteractionTrace()
{
	FHitResult Hit;
	if (!FollowCamera) return Hit;

	const FVector Start = FollowCamera->GetComponentLocation();
	const FVector End   = Start + FollowCamera->GetForwardVector() * InteractionRange;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;

	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return Hit;
}

EInteractionTarget AAoCPlayerPawn::ClassifyActor(AActor* Actor)
{
	if (!Actor) return EInteractionTarget::Ground;

	// Check actor tags first (most reliable)
	if (Actor->Tags.Contains(FName("OreVein")))    return EInteractionTarget::OreVein;
	if (Actor->Tags.Contains(FName("Furnace")))     return EInteractionTarget::Furnace;
	if (Actor->Tags.Contains(FName("HerbBush")))    return EInteractionTarget::HerbBush;
	if (Actor->Tags.Contains(FName("ResourceNode")))return EInteractionTarget::ResourceNode;
	if (Actor->Tags.Contains(FName("NPC")))         return EInteractionTarget::NPC;
	if (Actor->Tags.Contains(FName("Creature")))    return EInteractionTarget::Creature;

	// Fallback: check class name
	const FString ClassName = Actor->GetClass()->GetName();

	if (ClassName.Contains(TEXT("OreVein")))      return EInteractionTarget::OreVein;
	if (ClassName.Contains(TEXT("FurnaceActor"))) return EInteractionTarget::Furnace;
	if (ClassName.Contains(TEXT("ResourceNode"))) return EInteractionTarget::HerbBush;
	if (ClassName.Contains(TEXT("HerbBush")))     return EInteractionTarget::HerbBush;
	if (ClassName.Contains(TEXT("Landscape")))    return EInteractionTarget::Ground;

	// Check actor name (for actors placed without custom classes)
	const FString ActorName = Actor->GetActorNameOrLabel();
	if (ActorName.Contains(TEXT("Ore")) || ActorName.Contains(TEXT("Vein")))
		return EInteractionTarget::OreVein;
	if (ActorName.Contains(TEXT("Furnace")) || ActorName.Contains(TEXT("Bloomery")) ||
		ActorName.Contains(TEXT("Forge")) || ActorName.Contains(TEXT("Crucible")))
		return EInteractionTarget::Furnace;
	if (ActorName.Contains(TEXT("Herb")) || ActorName.Contains(TEXT("Bush")))
		return EInteractionTarget::HerbBush;

	// Default: if it's a static mesh we hit, treat as ground
	return EInteractionTarget::Ground;
}

FString AAoCPlayerPawn::BuildTargetName(AActor* Actor, EInteractionTarget Type)
{
	if (!Actor) return TEXT("Ground");

	switch (Type)
	{
	case EInteractionTarget::OreVein:
	{
		// Try to read ore_material property (set on placed ore vein actors)
		FString OreName;
		FProperty* Prop = Actor->GetClass()->FindPropertyByName(FName("ore_material"));
		if (!Prop) Prop = Actor->GetClass()->FindPropertyByName(FName("OreMaterial"));
		if (Prop)
		{
			Prop->ExportTextItem_Direct(OreName, Prop->ContainerPtrToValuePtr<void>(Actor), nullptr, Actor, 0);
		}
		if (OreName.IsEmpty())
		{
			OreName = Actor->GetActorNameOrLabel();
		}
		return FString::Printf(TEXT("Ore Vein: %s"), *OreName);
	}

	case EInteractionTarget::Furnace:
	{
		return FString::Printf(TEXT("Furnace: %s"), *Actor->GetActorNameOrLabel());
	}

	case EInteractionTarget::HerbBush:
	{
		return FString::Printf(TEXT("Herb: %s"), *Actor->GetActorNameOrLabel());
	}

	case EInteractionTarget::NPC:
		return FString::Printf(TEXT("NPC: %s"), *Actor->GetActorNameOrLabel());

	case EInteractionTarget::Creature:
		return FString::Printf(TEXT("Creature: %s"), *Actor->GetActorNameOrLabel());

	default:
		return TEXT("Ground");
	}
}

// ============================================================================
// ACTION HANDLERS — Context-sensitive interaction with animations
// ============================================================================

void AAoCPlayerPawn::HandleInteract()
{
	if (ActionCooldown > 0.0f) return;

	switch (CurrentTarget)
	{
	// ── Mine ore from a vein ────────────────────────────────────────────────
	case EInteractionTarget::OreVein:
	{
		ActionCooldown = 1.5f;
		PlayActionAnimation(AnimMiningSwing);

		if (MiningComponent)
		{
			FVector OreLocation = (TargetActor != nullptr) ? TargetActor->GetActorLocation() : GetActorLocation();
			FMiningResult MResult = MiningComponent->MineOre(OreLocation);
			if (MResult.bSuccess)
			{
				ShowNotification(FString::Printf(TEXT("Mined %d ore (Q%d)"), MResult.Quantity, MResult.Quality), FColor::Green, 3.0f);
			}
			else
			{
				ShowNotification(FString::Printf(TEXT("Mining %s..."), *TargetDisplayName), FColor::Green);
			}
		}
		else
		{
			ShowNotification(TEXT("Mining component not ready"), FColor::Red);
		}
		break;
	}

	// ── Interact with a furnace ─────────────────────────────────────────────
	case EInteractionTarget::Furnace:
	{
		ActionCooldown = 0.5f;
		PlayActionAnimation(AnimSmelting);

		if (SmeltingComponent)
		{
			ShowNotification(FString::Printf(TEXT("Using %s..."), *TargetDisplayName), FColor::Orange);
		}
		else
		{
			ShowNotification(TEXT("Smelting component not ready"), FColor::Red);
		}
		break;
	}

	// ── Gather herb from a bush ─────────────────────────────────────────────
	case EInteractionTarget::HerbBush:
	{
		ActionCooldown = 1.0f;
		PlayActionAnimation(AnimGatherHerb);

		if (HerbalismComponent)
		{
			ShowNotification(FString::Printf(TEXT("Gathering %s..."), *TargetDisplayName), FColor::Green);
		}
		else if (GatheringComponent)
		{
			GatheringComponent->StartGathering(TargetActor.Get(), EGatheringType::Herbalism);
			ShowNotification(FString::Printf(TEXT("Gathering %s..."), *TargetDisplayName), FColor::Green);
		}
		else
		{
			ShowNotification(TEXT("Gathering component not ready"), FColor::Red);
		}
		break;
	}

	// ── Resource node ───────────────────────────────────────────────────────
	case EInteractionTarget::ResourceNode:
	{
		ActionCooldown = 1.0f;
		PlayActionAnimation(AnimPickupItem);

		if (GatheringComponent)
		{
			GatheringComponent->StartGathering(TargetActor.Get(), EGatheringType::Mining);
			ShowNotification(FString::Printf(TEXT("Gathering %s..."), *TargetDisplayName), FColor::Green);
		}
		break;
	}

	// ── Looking at ground — hint at available actions ────────────────────────
	case EInteractionTarget::Ground:
	{
		const FString TKey = KeyBindings.FindRef(FName("Terraform")).GetDisplayName().ToString();
		const FString GKey = KeyBindings.FindRef(FName("ObserveGround")).GetDisplayName().ToString();
		ShowNotification(FString::Printf(TEXT("Press [%s] for Terraform  |  [%s] to Observe Ground"),
			*TKey, *GKey), FColor(180, 180, 180));
		break;
	}

	default:
		ShowNotification(TEXT("Nothing to interact with"), FColor(150, 150, 150));
		break;
	}
}

void AAoCPlayerPawn::HandleObserveGround()
{
	PlayActionAnimation(AnimObserveGround, 3.0f);

	// ── TerraForge observe mode (shows 11x11 grid overlay) ──
	if (TerraForgeComp)
	{
		TerraForgeComp->ExecuteAction(ETerraAction::Observe);
	}

	FString Info = TEXT("=== GROUND OBSERVATION ===\n");
	Info += FString::Printf(TEXT("Location: (%.0f, %.0f, %.0f)\n"),
		TargetLocation.X, TargetLocation.Y, TargetLocation.Z);

	const float SlopeAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(TargetNormal, FVector::UpVector)));
	Info += FString::Printf(TEXT("Slope: %.1f degrees\n"), SlopeAngle);

	// Determine what's on the surface
	if (CurrentTarget == EInteractionTarget::OreVein)
	{
		Info += FString::Printf(TEXT("Surface: %s"), *TargetDisplayName);
	}
	else
	{
		Info += TEXT("Surface: Terrain\n");
		const FString RKey = KeyBindings.FindRef(FName("Prospect")).GetDisplayName().ToString();
		Info += FString::Printf(TEXT("Press [%s] to Prospect for underground ores"), *RKey);
	}

	ShowNotification(Info, FColor::Cyan, 5.0f);
}

void AAoCPlayerPawn::HandleTerraformMenu()
{
	if (CurrentMode == EInteractionMode::TerraformMenu)
	{
		CurrentMode = EInteractionMode::Default;
		if (TerraForgeComp) TerraForgeComp->CloseContextMenu();
		ShowNotification(TEXT("Terraform mode OFF"), FColor(180, 180, 180), 1.0f);
	}
	else
	{
		CurrentMode = EInteractionMode::TerraformMenu;
		if (TerraForgeComp) TerraForgeComp->OpenContextMenu();
		ShowNotification(TEXT("TERRAFORM MODE\n[1] Lower  [2] Raise  [3] Flatten\n[4] Slope  [5] Tunnel  [6] Support"), FColor::Yellow, 5.0f);
	}
}

void AAoCPlayerPawn::HandleMiningMenu()
{
	if (CurrentMode == EInteractionMode::MiningMenu)
	{
		CurrentMode = EInteractionMode::Default;
		if (TerraForgeComp) TerraForgeComp->CloseContextMenu();
		ShowNotification(TEXT("Mining mode OFF"), FColor(180, 180, 180), 1.0f);
	}
	else
	{
		CurrentMode = EInteractionMode::MiningMenu;
		if (TerraForgeComp) TerraForgeComp->OpenContextMenu();
		ShowNotification(TEXT("MINING MODE\n[1] Tunnel Fwd  [2] Tunnel Down  [3] Tunnel Up\n[4] Place Support"), FColor::Orange, 5.0f);
	}
}

void AAoCPlayerPawn::HandleProspect()
{
	if (ActionCooldown > 0.0f) return;
	ActionCooldown = 3.0f;

	PlayActionAnimation(AnimProspecting, 3.0f);

	// ── TerraForge prospecting (deep geological scan) ──
	if (TerraForgeComp)
	{
		TerraForgeComp->ExecuteAction(ETerraAction::Prospect);
		ShowNotification(TEXT("Prospecting... scanning geological layers..."), FColor::Yellow, 4.0f);
		return;
	}

	// ── Fallback: old prospecting component ──
	if (ProspectingComponent)
	{
		ProspectingComponent->StartProspect(GetActorLocation());
		ShowNotification(TEXT("Prospecting... listening for vibrations..."), FColor::Yellow, 4.0f);
	}
	else
	{
		ShowNotification(TEXT("Prospecting component not ready"), FColor::Red);
	}
}

void AAoCPlayerPawn::HandleInventoryToggle()
{
	bShowingInventory = !bShowingInventory;
	if (GameHUD)
	{
		GameHUD->ToggleInventory();
	}
}

void AAoCPlayerPawn::HandleSkillsToggle()
{
	bShowingSkills = !bShowingSkills;
	if (GameHUD)
	{
		GameHUD->ToggleSkills();
	}
}

void AAoCPlayerPawn::HandleCancel()
{
	// Exit any active mode
	if (CurrentMode != EInteractionMode::Default)
	{
		CurrentMode = EInteractionMode::Default;
		if (TerraForgeComp) TerraForgeComp->CloseContextMenu();
		ShowNotification(TEXT("Cancelled"), FColor(180, 180, 180), 1.0f);
	}

	// Cancel action animation
	if (bPlayingActionAnim)
	{
		GetWorldTimerManager().ClearTimer(AnimReturnTimerHandle);
		ReturnToIdle();
	}
}

void AAoCPlayerPawn::HandleActionSlot(int32 Slot)
{
	if (CurrentMode == EInteractionMode::TerraformMenu)
	{
		ExecuteTerraformAction(Slot);
	}
	else if (CurrentMode == EInteractionMode::MiningMenu)
	{
		ExecuteMiningAction(Slot);
	}
}

// ============================================================================
// ACTION EXECUTION — Terraform & Mining sub-actions with animations
// ============================================================================

void AAoCPlayerPawn::ExecuteTerraformAction(int32 ActionIndex)
{
	if (ActionCooldown > 0.0f) return;

	// ── Route through TerraForge (unified engine) ──
	if (TerraForgeComp)
	{
		// Map action slot to ETerraAction
		ETerraAction TFAction;
		FString ActionName;
		switch (ActionIndex)
		{
		case 1: TFAction = ETerraAction::LowerGround;  ActionName = TEXT("Lower Ground");  break;
		case 2: TFAction = ETerraAction::RaiseGround;   ActionName = TEXT("Raise Ground");   break;
		case 3: TFAction = ETerraAction::Flatten;        ActionName = TEXT("Flatten");        break;
		case 4: TFAction = ETerraAction::FlattenSlope;   ActionName = TEXT("Flatten Slope");  break;
		case 5: TFAction = ETerraAction::DigTunnel;      ActionName = TEXT("Dig Tunnel");     break;
		case 6: TFAction = ETerraAction::PlaceSupport;   ActionName = TEXT("Place Support");  break;
		default:
			ShowNotification(TEXT("Invalid terraform action"), FColor::Red);
			return;
		}

		// Play digging animation for all terraform actions
		PlayActionAnimation(AnimDigging);

		const bool bSuccess = TerraForgeComp->ExecuteAction(TFAction);
		if (bSuccess)
		{
			ShowNotification(FString::Printf(TEXT("Terraforming: %s"), *ActionName), FColor::Green);
		}
		else
		{
			ShowNotification(FString::Printf(TEXT("Cannot %s here"), *ActionName), FColor::Red);
		}
		ActionCooldown = 1.0f;
		return;
	}

	// ── Fallback: old terraforming component ──
	ActionCooldown = 1.0f;

	ETerraformAction TAction;
	FString ActionName;
	switch (ActionIndex)
	{
	case 1: TAction = ETerraformAction::RaiseGround;        ActionName = TEXT("Raise");      break;
	case 2: TAction = ETerraformAction::LowerGround;        ActionName = TEXT("Lower");      break;
	case 3: TAction = ETerraformAction::Flatten;            ActionName = TEXT("Flatten");    break;
	case 4: TAction = ETerraformAction::FlattenSlopeUp;     ActionName = TEXT("Slope Up");   break;
	case 5: TAction = ETerraformAction::FlattenSlopeDown;   ActionName = TEXT("Slope Down"); break;
	default:
		ShowNotification(TEXT("Invalid terraform action"), FColor::Red);
		return;
	}

	PlayActionAnimation(AnimDigging);

	if (TerraformingComponent)
	{
		TerraformingComponent->StartTerraform(TAction, GetActorLocation());
		ShowNotification(FString::Printf(TEXT("Terraforming: %s"), *ActionName), FColor::Green);
	}
	else
	{
		ShowNotification(TEXT("Terraforming component not ready"), FColor::Red);
	}
}

void AAoCPlayerPawn::ExecuteMiningAction(int32 ActionIndex)
{
	if (ActionCooldown > 0.0f) return;
	ActionCooldown = 1.5f;

	FString ActionName;
	switch (ActionIndex)
	{
	case 1: ActionName = TEXT("Tunnel Forward"); break;
	case 2: ActionName = TEXT("Tunnel Down");    break;
	case 3: ActionName = TEXT("Tunnel Up");      break;
	case 4: ActionName = TEXT("Place Support");  break;
	default:
		ShowNotification(TEXT("Invalid mining action"), FColor::Red);
		return;
	}

	// Play mining swing animation for all tunnel actions
	PlayActionAnimation(AnimMiningSwing);

	// ── Route through TerraForge (unified engine) ──
	if (TerraForgeComp)
	{
		bool bSuccess = false;
		if (ActionIndex == 4)
		{
			bSuccess = TerraForgeComp->ExecuteAction(ETerraAction::PlaceSupport);
		}
		else
		{
			bSuccess = TerraForgeComp->ExecuteAction(ETerraAction::DigTunnel);
		}

		if (bSuccess)
		{
			ShowNotification(FString::Printf(TEXT("Mining: %s"), *ActionName), FColor::Green);
		}
		else
		{
			ShowNotification(FString::Printf(TEXT("Cannot %s here"), *ActionName), FColor::Red);
		}
		return;
	}

	// ── Fallback: old mining component ──
	if (MiningComponent)
	{
		if (ActionIndex == 1) MiningComponent->TunnelForward();
		else if (ActionIndex == 2) MiningComponent->TunnelDown();
		else if (ActionIndex == 3) MiningComponent->TunnelUp();

		ShowNotification(FString::Printf(TEXT("Mining: %s"), *ActionName), FColor::Green);
	}
	else
	{
		ShowNotification(TEXT("Mining component not ready"), FColor::Red);
	}
}

// ============================================================================
// HUD WIDGET — Create and manage the AoC HUD
// ============================================================================

void AAoCPlayerPawn::CreateHUDWidget()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Find the HUD widget class
	UClass* HUDClass = LoadObject<UClass>(nullptr, TEXT("/Script/AOC.AoCHUDWidget"));
	if (!HUDClass)
	{
		// Try StaticLoadClass as fallback
		HUDClass = UAoCHUDWidget::StaticClass();
	}

	if (HUDClass)
	{
		GameHUD = CreateWidget<UAoCHUDWidget>(PC, HUDClass);
		if (GameHUD)
		{
			GameHUD->AddToViewport(0);
			UE_LOG(LogTemp, Log, TEXT("AoC HUD Widget created and added to viewport."));
		}
	}
}

// ============================================================================
// LOOK-AT TOOLTIP — Show clean description when looking at world objects
// ============================================================================

void AAoCPlayerPawn::UpdateLookAtTooltip()
{
	if (!GameHUD) return;

	// Use the existing raycast hit to determine what we're looking at
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const float LookAtRange = 3000.0f; // 30 meters — much further than interaction range
	FVector TraceEnd = CamLoc + CamRot.Vector() * LookAtRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params);

	if (bHit && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();

		// Don't update tooltip if it's the same actor
		if (LastLookAtActor.IsValid() && LastLookAtActor.Get() == HitActor)
		{
			return; // Same target, tooltip already showing
		}

		LastLookAtActor = HitActor;

		FString DisplayName;
		FString Description;

		// ── Ore Veins ──────────────────────────────────────────────────────
		if (HitActor->ActorHasTag(TEXT("OreVein")))
		{
			// Get ore type from property
			FString OreType;
			FProperty* OreProp = HitActor->GetClass()->FindPropertyByName(FName(TEXT("OreMaterial")));
			if (OreProp)
			{
				const UEnum* OreEnum = FindObject<UEnum>(nullptr, TEXT("/Script/AOC.EAoCOreMaterial"), true);
				if (OreEnum)
				{
					const void* ValuePtr = OreProp->ContainerPtrToValuePtr<void>(HitActor);
					uint8 EnumVal = *static_cast<const uint8*>(ValuePtr);
					OreType = OreEnum->GetDisplayNameTextByValue(EnumVal).ToString();
					OreType.RemoveFromStart(TEXT("ORE_"));
					// Title case
					if (OreType.Len() > 0)
					{
						OreType = OreType.Left(1).ToUpper() + OreType.Mid(1).ToLower();
					}
				}
			}

			if (OreType.IsEmpty()) OreType = TEXT("Unknown");
			DisplayName = FString::Printf(TEXT("%s Ore Vein"), *OreType);

			// Read quality and remaining ore via property reflection
			float Quality = 0.f;
			int32 RemainingOre = 0;
			FProperty* QualProp = HitActor->GetClass()->FindPropertyByName(FName(TEXT("Quality")));
			FProperty* RemProp = HitActor->GetClass()->FindPropertyByName(FName(TEXT("RemainingOre")));
			if (QualProp)
			{
				const void* QPtr = QualProp->ContainerPtrToValuePtr<void>(HitActor);
				Quality = *static_cast<const float*>(QPtr);
			}
			if (RemProp)
			{
				const void* RPtr = RemProp->ContainerPtrToValuePtr<void>(HitActor);
				RemainingOre = *static_cast<const int32*>(RPtr);
			}

			Description = FString::Printf(TEXT("Quality: %.0f  |  Ore: %d"), Quality, RemainingOre);
		}
		// ── Furnaces ───────────────────────────────────────────────────────
		else if (HitActor->ActorHasTag(TEXT("Furnace")))
		{
			DisplayName = HitActor->GetActorNameOrLabel();
			if (DisplayName.IsEmpty()) DisplayName = TEXT("Furnace");
			Description = TEXT("Press [E] to Smelt");
		}
		// ── Herb Bushes ────────────────────────────────────────────────────
		else if (HitActor->ActorHasTag(TEXT("HerbBush")))
		{
			DisplayName = HitActor->GetActorNameOrLabel();
			if (DisplayName.IsEmpty()) DisplayName = TEXT("Herb Bush");
			Description = TEXT("Press [E] to Gather");
		}
		// ── Resource Nodes (wood, stone, etc.) ─────────────────────────────
		else if (HitActor->ActorHasTag(TEXT("ResourceNode")))
		{
			DisplayName = HitActor->GetActorNameOrLabel();
			if (DisplayName.IsEmpty()) DisplayName = TEXT("Resource");
			Description = TEXT("Press [E] to Gather");
		}
		// ── Trees ──────────────────────────────────────────────────────────
		else if (HitActor->ActorHasTag(TEXT("Tree")) ||
		         HitActor->GetActorNameOrLabel().Contains(TEXT("Tree")))
		{
			DisplayName = HitActor->GetActorNameOrLabel();
			if (DisplayName.IsEmpty()) DisplayName = TEXT("Tree");
		}
		// ── NPCs & Creatures ───────────────────────────────────────────────
		else if (HitActor->ActorHasTag(TEXT("NPC")) ||
		         HitActor->ActorHasTag(TEXT("Creature")))
		{
			DisplayName = HitActor->GetActorNameOrLabel();
			if (DisplayName.IsEmpty()) DisplayName = TEXT("Character");
		}
		// ── Generic named actors (with a DisplayName tag) ──────────────────
		else
		{
			// Check for "DisplayName:Something" tag convention
			for (const FName& Tag : HitActor->Tags)
			{
				FString TagStr = Tag.ToString();
				if (TagStr.StartsWith(TEXT("DisplayName:")))
				{
					DisplayName = TagStr.Mid(12); // After "DisplayName:"
					break;
				}
			}
		}

		// Only show tooltip if we have something to display
		if (!DisplayName.IsEmpty())
		{
			GameHUD->ShowLookAtTooltip(DisplayName, Description);
		}
		else
		{
			GameHUD->HideLookAtTooltip();
			LastLookAtActor.Reset();
		}
	}
	else
	{
		// Not looking at anything — hide tooltip
		if (LastLookAtActor.IsValid())
		{
			LastLookAtActor.Reset();
			GameHUD->HideLookAtTooltip();
		}
	}
}

// ============================================================================
// HUD DISPLAY — Context-sensitive on-screen info (debug overlay)
// ============================================================================

void AAoCPlayerPawn::DisplayHUD()
{
	// Handled by UAoCHUDWidget — NativeTick pulls all state from pawn automatically.
	// Old AddOnScreenDebugMessage system removed.
}

void AAoCPlayerPawn::ShowNotification(const FString& Message, FColor Color, float Duration)
{
	UE_LOG(LogTemp, Log, TEXT("AoC: %s"), *Message);

	// Route to proper HUD widget
	if (GameHUD)
	{
		GameHUD->ShowNotification(Message, FLinearColor(Color));
	}

	// Keep old tracking as fallback
	NotificationText  = Message;
	NotificationColor = Color;
	NotificationTimer = Duration;
}
