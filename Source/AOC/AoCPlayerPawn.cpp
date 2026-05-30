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
	CameraBoom->TargetArmLength       = 450.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest       = true;
	CameraBoom->SocketOffset           = FVector(0.0f, 50.0f, 80.0f);

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
	AddControllerYawInput(MouseX);
	AddControllerPitchInput(MouseY * -1.0f);
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

	// Sub-menu mode: number keys select actions
	if (CurrentMode == EInteractionMode::TerraformMenu ||
		CurrentMode == EInteractionMode::MiningMenu)
	{
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
			MiningComponent->MineOre(TargetActor.Get(), 80);
			ShowNotification(FString::Printf(TEXT("Mining %s..."), *TargetDisplayName), FColor::Green);
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
			GatheringComponent->StartGathering(TargetActor.Get());
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
			GatheringComponent->StartGathering(TargetActor.Get());
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
		ShowNotification(TEXT("Terraform mode OFF"), FColor(180, 180, 180), 1.0f);
	}
	else
	{
		CurrentMode = EInteractionMode::TerraformMenu;
		ShowNotification(TEXT("TERRAFORM MODE - select action with number keys"), FColor::Yellow);
	}
}

void AAoCPlayerPawn::HandleMiningMenu()
{
	if (CurrentMode == EInteractionMode::MiningMenu)
	{
		CurrentMode = EInteractionMode::Default;
		ShowNotification(TEXT("Mining mode OFF"), FColor(180, 180, 180), 1.0f);
	}
	else
	{
		CurrentMode = EInteractionMode::MiningMenu;
		ShowNotification(TEXT("MINING MODE - select tunnel direction"), FColor::Orange);
	}
}

void AAoCPlayerPawn::HandleProspect()
{
	if (ActionCooldown > 0.0f) return;
	ActionCooldown = 3.0f;

	PlayActionAnimation(AnimProspecting, 3.0f);

	if (ProspectingComponent)
	{
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
	if (bShowingInventory && InventoryComponent)
	{
		FString Msg = TEXT("=== INVENTORY ===\n");
		ShowNotification(Msg, FColor::White, 5.0f);
	}
	else
	{
		ShowNotification(TEXT("Inventory closed"), FColor(180, 180, 180), 1.0f);
	}
}

void AAoCPlayerPawn::HandleSkillsToggle()
{
	bShowingSkills = !bShowingSkills;
	if (bShowingSkills && SkillComponent)
	{
		ShowNotification(TEXT("=== SKILLS ==="), FColor::Cyan, 5.0f);
	}
	else
	{
		ShowNotification(TEXT("Skills closed"), FColor(180, 180, 180), 1.0f);
	}
}

void AAoCPlayerPawn::HandleCancel()
{
	// Exit any active mode
	if (CurrentMode != EInteractionMode::Default)
	{
		CurrentMode = EInteractionMode::Default;
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
	ActionCooldown = 1.0f;

	FString ActionName;
	switch (ActionIndex)
	{
	case 1: ActionName = TEXT("Raise");      break;
	case 2: ActionName = TEXT("Lower");      break;
	case 3: ActionName = TEXT("Flatten");    break;
	case 4: ActionName = TEXT("Slope Up");   break;
	case 5: ActionName = TEXT("Slope Down"); break;
	default:
		ShowNotification(TEXT("Invalid terraform action"), FColor::Red);
		return;
	}

	// Play digging animation for all terraform actions
	PlayActionAnimation(AnimDigging);

	if (TerraformingComponent)
	{
		TerraformingComponent->TerraformAction(TargetLocation, ActionName, TEXT("Dirt"));
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
	default:
		ShowNotification(TEXT("Invalid mining action"), FColor::Red);
		return;
	}

	// Play mining swing animation for all tunnel actions
	PlayActionAnimation(AnimMiningSwing);

	if (MiningComponent)
	{
		// Get the direction based on camera forward
		const FVector Dir = FollowCamera ? FollowCamera->GetForwardVector() : GetActorForwardVector();

		if (ActionIndex == 1) MiningComponent->TunnelForward(GetActorLocation(), Dir);
		else if (ActionIndex == 2) MiningComponent->TunnelDown(GetActorLocation(), Dir);
		else if (ActionIndex == 3) MiningComponent->TunnelUp(GetActorLocation(), Dir);

		ShowNotification(FString::Printf(TEXT("Mining: %s"), *ActionName), FColor::Green);
	}
	else
	{
		ShowNotification(TEXT("Mining component not ready"), FColor::Red);
	}
}

// ============================================================================
// HUD DISPLAY — Context-sensitive on-screen info
// ============================================================================

void AAoCPlayerPawn::DisplayHUD()
{
	if (!GEngine) return;

	// ── Line 1: Target info (what you're looking at) ────────────────────────
	if (CurrentTarget != EInteractionTarget::Nothing)
	{
		FColor TargetColor = FColor::White;
		switch (CurrentTarget)
		{
		case EInteractionTarget::OreVein:      TargetColor = FColor(255, 200, 50);  break;
		case EInteractionTarget::Furnace:      TargetColor = FColor(255, 120, 30);  break;
		case EInteractionTarget::HerbBush:     TargetColor = FColor(80, 220, 80);   break;
		case EInteractionTarget::ResourceNode: TargetColor = FColor(100, 200, 255); break;
		case EInteractionTarget::Ground:       TargetColor = FColor(180, 160, 120); break;
		case EInteractionTarget::NPC:          TargetColor = FColor(200, 200, 255); break;
		case EInteractionTarget::Creature:     TargetColor = FColor(255, 100, 100); break;
		default: break;
		}
		GEngine->AddOnScreenDebugMessage(1, 0.0f, TargetColor,
			FString::Printf(TEXT(">> %s"), *TargetDisplayName));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor(60, 60, 60), TEXT(""));
	}

	// ── Line 2: Available actions (context-sensitive) ───────────────────────
	const FString EKey = KeyBindings.FindRef(FName("Interact")).GetDisplayName().ToString();
	const FString GKey = KeyBindings.FindRef(FName("ObserveGround")).GetDisplayName().ToString();
	const FString TKey = KeyBindings.FindRef(FName("Terraform")).GetDisplayName().ToString();
	const FString MKey = KeyBindings.FindRef(FName("MiningMenu")).GetDisplayName().ToString();
	const FString RKey = KeyBindings.FindRef(FName("Prospect")).GetDisplayName().ToString();
	const FString IKey = KeyBindings.FindRef(FName("Inventory")).GetDisplayName().ToString();

	FString Actions;
	if (bPlayingActionAnim)
	{
		Actions = TEXT("... working ...");
	}
	else
	{
		switch (CurrentTarget)
		{
		case EInteractionTarget::OreVein:
			Actions = FString::Printf(TEXT("[%s] Mine  [%s] Prospect  [%s] Terraform  [%s] Tunnel"),
				*EKey, *RKey, *TKey, *MKey);
			break;
		case EInteractionTarget::Furnace:
			Actions = FString::Printf(TEXT("[%s] Open Furnace"), *EKey);
			break;
		case EInteractionTarget::HerbBush:
			Actions = FString::Printf(TEXT("[%s] Gather Herb  [%s] Observe"), *EKey, *GKey);
			break;
		case EInteractionTarget::ResourceNode:
			Actions = FString::Printf(TEXT("[%s] Gather Resource"), *EKey);
			break;
		case EInteractionTarget::Ground:
			Actions = FString::Printf(TEXT("[%s] Observe  [%s] Terraform  [%s] Prospect  [%s] Tunnel"),
				*GKey, *TKey, *RKey, *MKey);
			break;
		default:
			Actions = FString::Printf(TEXT("[%s] Prospect  [%s] Terraform  [%s] Tunnel  [%s] Inventory"),
				*RKey, *TKey, *MKey, *IKey);
			break;
		}
	}
	GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor(180, 180, 180), Actions);

	// ── Line 3: Mode-specific sub-menu ──────────────────────────────────────
	if (CurrentMode == EInteractionMode::TerraformMenu)
	{
		GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::Yellow,
			TEXT("TERRAFORM:  [1] Raise  [2] Lower  [3] Flatten  [4] Slope Up  [5] Slope Down  [Esc] Cancel"));
	}
	else if (CurrentMode == EInteractionMode::MiningMenu)
	{
		GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::Orange,
			TEXT("MINING:  [1] Tunnel Forward  [2] Tunnel Down  [3] Tunnel Up  [Esc] Cancel"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor(10, 10, 10), TEXT(""));
	}

	// ── Line 4: Notification (timed) ────────────────────────────────────────
	if (NotificationTimer > 0.0f)
	{
		GEngine->AddOnScreenDebugMessage(4, 0.0f, NotificationColor, NotificationText);
	}
}

void AAoCPlayerPawn::ShowNotification(const FString& Message, FColor Color, float Duration)
{
	NotificationText  = Message;
	NotificationColor = Color;
	NotificationTimer = Duration;

	UE_LOG(LogTemp, Log, TEXT("AoC: %s"), *Message);
}
