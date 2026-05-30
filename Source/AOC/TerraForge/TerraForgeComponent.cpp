// TerraForgeComponent.cpp
// TerraForge — Player component implementation.
// Handles all terraforming interaction logic.

#include "TerraForgeComponent.h"
#include "TerraForgeSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

// ============================================================================
// CONSTRUCTION & LIFECYCLE
// ============================================================================

UTerraForgeComponent::UTerraForgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f; // 20Hz tick for target updates
}

void UTerraForgeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystem reference
	UWorld* World = GetWorld();
	if (World)
	{
		TerraForgeSubsystem = World->GetSubsystem<UTerraForgeSubsystem>();
		if (TerraForgeSubsystem)
		{
			UE_LOG(LogTemp, Log, TEXT("TerraForge Component: Connected to subsystem."));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TerraForge Component: Subsystem not found!"));
		}
	}

	// Initialize gem drop table
	GemDropTable.Initialize();
}

void UTerraForgeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Tick subsystem (it's not auto-ticked as a UWorldSubsystem)
	if (TerraForgeSubsystem)
	{
		TerraForgeSubsystem->TickSubsystem(DeltaTime);
	}

	// Update cooldown
	if (ActionCooldown > 0.0f)
	{
		ActionCooldown -= DeltaTime;
		if (ActionCooldown < 0.0f) ActionCooldown = 0.0f;
	}

	// Update target
	UpdateTarget();

	// Check structural integrity at target
	CheckStructuralIntegrity();
}

// ============================================================================
// TARGET TRACKING
// ============================================================================

void UTerraForgeComponent::UpdateTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	ACharacter* Character = Cast<ACharacter>(Owner);
	if (!Character) return;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return;

	// Raycast from camera center
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector TraceStart = CamLoc;
	const FVector TraceEnd = CamLoc + CamRot.Vector() * MaxActionRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);
	Params.bTraceComplex = true;

	bTargetValid = GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

	if (bTargetValid)
	{
		TargetPosition = Hit.Location;
		TargetNormal = Hit.Normal;

		// Get surface material at target
		if (TerraForgeSubsystem)
		{
			TargetMaterial = TerraForgeSubsystem->GetSurfaceMaterialAt(TargetPosition);
			TargetStructuralState = TerraForgeSubsystem->GetStructuralState(TargetPosition);
		}
	}
}

// ============================================================================
// CONTEXT MENU
// ============================================================================

void UTerraForgeComponent::OpenContextMenu()
{
	if (!bTargetValid) return;

	bContextMenuOpen = true;

	// Set flatten reference height to player's current standing height
	AActor* Owner = GetOwner();
	if (Owner)
	{
		FlattenReferenceHeight = Owner->GetActorLocation().Z;
	}

	OnContextMenuRequested.Broadcast();
}

void UTerraForgeComponent::CloseContextMenu()
{
	bContextMenuOpen = false;
}

TArray<ETerraAction> UTerraForgeComponent::GetAvailableActions() const
{
	TArray<ETerraAction> Actions;

	// Observe is always available with shovel
	if (EquippedTool == ETerraToolType::Shovel || EquippedTool == ETerraToolType::Pickaxe)
	{
		Actions.Add(ETerraAction::Observe);
	}

	// Shovel actions
	if (EquippedTool == ETerraToolType::Shovel)
	{
		Actions.Add(ETerraAction::LowerGround);

		if (CarriedDirtUnits > 0)
		{
			Actions.Add(ETerraAction::RaiseGround);
		}

		if (TerraformingSkill >= FlattenMinSkill)
		{
			Actions.Add(ETerraAction::Flatten);
		}

		if (TerraformingSkill >= SlopeMinSkill)
		{
			Actions.Add(ETerraAction::FlattenSlope);
		}

		Actions.Add(ETerraAction::Prospect);
	}

	// Pickaxe actions
	if (EquippedTool == ETerraToolType::Pickaxe)
	{
		// Can dig rock/ore with pickaxe
		if (TargetMaterial >= EGeoMaterial::Sandstone || TargetMaterial == EGeoMaterial::OreVein)
		{
			Actions.Add(ETerraAction::DigTunnel);
		}
		Actions.Add(ETerraAction::Prospect);
	}

	// Hammer actions
	if (EquippedTool == ETerraToolType::Hammer)
	{
		Actions.Add(ETerraAction::PlaceSupport);
	}

	return Actions;
}

FText UTerraForgeComponent::GetActionDisplayName(ETerraAction Action)
{
	switch (Action)
	{
	case ETerraAction::Observe:      return FText::FromString(TEXT("Observe Terrain"));
	case ETerraAction::LowerGround:  return FText::FromString(TEXT("Lower Ground Level"));
	case ETerraAction::RaiseGround:  return FText::FromString(TEXT("Raise Ground Level"));
	case ETerraAction::Flatten:      return FText::FromString(TEXT("Flatten Ground"));
	case ETerraAction::FlattenSlope: return FText::FromString(TEXT("Flatten Slope"));
	case ETerraAction::DigTunnel:    return FText::FromString(TEXT("Dig Tunnel"));
	case ETerraAction::PlaceSupport: return FText::FromString(TEXT("Place Mine Support"));
	case ETerraAction::Prospect:     return FText::FromString(TEXT("Prospect Area"));
	default:                          return FText::FromString(TEXT("Unknown"));
	}
}

ETerraToolType UTerraForgeComponent::GetRequiredTool(ETerraAction Action)
{
	switch (Action)
	{
	case ETerraAction::Observe:      return ETerraToolType::Shovel;
	case ETerraAction::LowerGround:  return ETerraToolType::Shovel;
	case ETerraAction::RaiseGround:  return ETerraToolType::Shovel;
	case ETerraAction::Flatten:      return ETerraToolType::Shovel;
	case ETerraAction::FlattenSlope: return ETerraToolType::Shovel;
	case ETerraAction::DigTunnel:    return ETerraToolType::Pickaxe;
	case ETerraAction::PlaceSupport: return ETerraToolType::Hammer;
	case ETerraAction::Prospect:     return ETerraToolType::None; // Any tool
	default:                          return ETerraToolType::None;
	}
}

// ============================================================================
// ACTION EXECUTION
// ============================================================================

bool UTerraForgeComponent::ExecuteAction(ETerraAction Action)
{
	if (!TerraForgeSubsystem) return false;
	if (!bTargetValid) return false;
	if (ActionCooldown > 0.0f) return false;

	// Validate tool
	if (!HasCorrectTool(Action))
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: Wrong tool for action %d. Need %d, have %d."),
			(int32)Action, (int32)GetRequiredTool(Action), (int32)EquippedTool);
		OnTerraformAction.Broadcast(Action, false);
		return false;
	}

	bool bSuccess = false;

	switch (Action)
	{
	case ETerraAction::Observe:
		ToggleObserveMode();
		bSuccess = true;
		break;

	case ETerraAction::LowerGround:
	{
		EGeoMaterial DugMat = PerformDig();
		bSuccess = (DugMat != EGeoMaterial::Air);
		if (bSuccess)
		{
			AddToInventory(DugMat);
			ActionCooldown = GetDigTime();

			// --- Gem drop chance on rock/stone layers ---
			if (DugMat >= EGeoMaterial::Granite && DugMat <= EGeoMaterial::Basalt)
			{
				float DepthBelowSurface = TerraForgeSubsystem->GetElevationAt(TargetPosition) - TargetPosition.Z;
				DepthBelowSurface /= 100.0f; // Convert cm → meters

				FGemDropResult GemResult = GemDropTable.RollGemDrop(DepthBelowSurface, MiningSkill);
				if (GemResult.bDropped)
				{
					OnGemFound(GemResult);
				}
			}
		}
		break;
	}

	case ETerraAction::RaiseGround:
		bSuccess = PerformRaise();
		if (bSuccess)
		{
			ActionCooldown = 0.8f;
		}
		break;

	case ETerraAction::Flatten:
		bSuccess = PerformFlatten();
		if (bSuccess)
		{
			ActionCooldown = 1.0f;
		}
		break;

	case ETerraAction::FlattenSlope:
		// Slope uses the same flatten logic but with the target's current height
		bSuccess = TerraForgeSubsystem->FlattenTerrain(TargetPosition, TargetPosition.Z);
		if (bSuccess)
		{
			ActionCooldown = 1.2f;
		}
		break;

	case ETerraAction::DigTunnel:
	{
		// Dig in the direction the player is facing
		AActor* Owner = GetOwner();
		FVector DigDir = FVector::ForwardVector;
		if (Owner)
		{
			ACharacter* Char = Cast<ACharacter>(Owner);
			if (Char)
			{
				APlayerController* PC = Cast<APlayerController>(Char->GetController());
				if (PC)
				{
					FRotator ViewRot;
					FVector ViewLoc;
					PC->GetPlayerViewPoint(ViewLoc, ViewRot);
					DigDir = ViewRot.Vector();
				}
			}
		}
		bSuccess = TerraForgeSubsystem->DigTunnel(TargetPosition, DigDir, 1.0f);
		if (bSuccess)
		{
			ActionCooldown = GetDigTime() * 2.0f; // Tunneling takes longer
		}
		break;
	}

	case ETerraAction::PlaceSupport:
		bSuccess = TerraForgeSubsystem->PlaceSupport(TargetPosition);
		if (bSuccess)
		{
			ActionCooldown = 2.0f;
		}
		break;

	case ETerraAction::Prospect:
		StartProspecting();
		bSuccess = true;
		ActionCooldown = 3.0f;
		break;

	default:
		break;
	}

	OnTerraformAction.Broadcast(Action, bSuccess);
	return bSuccess;
}

bool UTerraForgeComponent::ExecuteActionBySlot(int32 Slot)
{
	TArray<ETerraAction> Available = GetAvailableActions();

	if (Slot < 1 || Slot > Available.Num())
	{
		return false;
	}

	return ExecuteAction(Available[Slot - 1]);
}

// ============================================================================
// OBSERVE MODE
// ============================================================================

void UTerraForgeComponent::ToggleObserveMode()
{
	bObserveModeActive = !bObserveModeActive;
	OnObserveModeChanged.Broadcast(bObserveModeActive);

	if (bObserveModeActive)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Observe mode ENABLED"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Observe mode DISABLED"));
	}
}

TArray<FObserveTile> UTerraForgeComponent::GetObserveGrid() const
{
	if (!TerraForgeSubsystem)
	{
		return TArray<FObserveTile>();
	}

	return TerraForgeSubsystem->GetObserveGrid(TargetPosition);
}

// ============================================================================
// PROSPECTING
// ============================================================================

void UTerraForgeComponent::StartProspecting()
{
	if (!TerraForgeSubsystem) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	const FVector ProspectPos = Owner->GetActorLocation();
	LastProspectResults = TerraForgeSubsystem->Prospect(
		ProspectPos, ProspectingRadius, ProspectingSkill);

	OnProspectingComplete.Broadcast(LastProspectResults);

	if (LastProspectResults.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Prospecting found %d deposits."), LastProspectResults.Num());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("TerraForge: Prospecting found nothing in range."));
	}
}

// ============================================================================
// DIRT INVENTORY
// ============================================================================

float UTerraForgeComponent::GetDirtWeight() const
{
	if (!TerraForgeSubsystem || CarriedDirtUnits == 0)
		return 0.0f;

	const FGeoMaterialProperties* Props = TerraForgeSubsystem->MaterialTable.Find(CarriedMaterialType);
	if (Props)
	{
		return CarriedDirtUnits * Props->WeightPerVoxel;
	}
	return CarriedDirtUnits * 50.0f; // Default weight
}

bool UTerraForgeComponent::IsOverburdened() const
{
	return GetDirtWeight() > 200.0f; // 200 kg overburdened threshold
}

void UTerraForgeComponent::AddToInventory(EGeoMaterial Material, int32 Units)
{
	if (CarriedDirtUnits >= MaxDirtCapacity)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: Dirt inventory full (%d/%d)."), CarriedDirtUnits, MaxDirtCapacity);
		return;
	}

	// If carrying different material, drop the old one (simplified — could stack)
	if (CarriedDirtUnits > 0 && CarriedMaterialType != Material)
	{
		// For now, replace. A full system would stack different materials.
		CarriedMaterialType = Material;
		CarriedDirtUnits = Units;
	}
	else
	{
		CarriedMaterialType = Material;
		CarriedDirtUnits = FMath::Min(CarriedDirtUnits + Units, MaxDirtCapacity);
	}
}

bool UTerraForgeComponent::RemoveFromInventory(int32 Units)
{
	if (CarriedDirtUnits < Units)
		return false;

	CarriedDirtUnits -= Units;
	if (CarriedDirtUnits == 0)
	{
		CarriedMaterialType = EGeoMaterial::Air;
	}
	return true;
}

// ============================================================================
// TOOL VALIDATION
// ============================================================================

bool UTerraForgeComponent::HasCorrectTool(ETerraAction Action) const
{
	const ETerraToolType Required = GetRequiredTool(Action);
	if (Required == ETerraToolType::None) return true;
	return EquippedTool == Required;
}

// ============================================================================
// INTERNAL OPERATIONS
// ============================================================================

float UTerraForgeComponent::GetDigTime() const
{
	if (!TerraForgeSubsystem) return 1.0f;

	const FGeoMaterialProperties* Props = TerraForgeSubsystem->MaterialTable.Find(TargetMaterial);
	if (Props)
	{
		// Scale by skill level (higher skill = faster)
		const float SkillMultiplier = 1.0f - (TerraformingSkill * 0.005f); // 0-50% faster at skill 100
		return Props->DigTime * FMath::Max(0.5f, SkillMultiplier);
	}
	return 1.0f;
}

EGeoMaterial UTerraForgeComponent::PerformDig()
{
	if (!TerraForgeSubsystem) return EGeoMaterial::Air;

	// Check if we can dig this material with our tool
	const FGeoMaterialProperties* Props = TerraForgeSubsystem->MaterialTable.Find(TargetMaterial);
	if (Props && EquippedTool < Props->RequiredTool)
	{
		UE_LOG(LogTemp, Warning, TEXT("TerraForge: Cannot dig %s with %s — need %s."),
			*Props->DisplayName.ToString(),
			*UEnum::GetValueAsString(EquippedTool),
			*UEnum::GetValueAsString(Props->RequiredTool));
		return EGeoMaterial::Air;
	}

	return TerraForgeSubsystem->LowerTerrain(TargetPosition);
}

bool UTerraForgeComponent::PerformRaise()
{
	if (!TerraForgeSubsystem) return false;
	if (CarriedDirtUnits <= 0) return false;

	if (TerraForgeSubsystem->RaiseTerrain(TargetPosition, CarriedMaterialType))
	{
		RemoveFromInventory(1);
		return true;
	}
	return false;
}

bool UTerraForgeComponent::PerformFlatten()
{
	if (!TerraForgeSubsystem) return false;
	if (TerraformingSkill < FlattenMinSkill) return false;

	return TerraForgeSubsystem->FlattenTerrain(TargetPosition, FlattenReferenceHeight);
}

void UTerraForgeComponent::CheckStructuralIntegrity()
{
	if (TargetStructuralState != PreviousStructuralState)
	{
		if (TargetStructuralState >= EStructuralState::Warning)
		{
			OnStructuralWarning.Broadcast(TargetStructuralState);
		}
		PreviousStructuralState = TargetStructuralState;
	}
}

void UTerraForgeComponent::OnGemFound(const FGemDropResult& GemResult)
{
	// Get gem properties for logging and spawning
	const FGemProperties* Props = GemDropTable.Gems.Find(GemResult.GemType);
	if (!Props) return;

	// Log the find
	UE_LOG(LogTemp, Log, TEXT("TerraForge: 💎 GEM FOUND! %s (Quality: %.0f, Value: %d)"),
		*Props->DisplayName.ToString(), GemResult.Quality, GemResult.Value);

	// Spawn a world-visible gem actor at the dig position
	UWorld* World = GetWorld();
	if (World)
	{
		// Spawn a small static mesh actor for the gem on the ground
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* GemActor = World->SpawnActor<AActor>(AActor::StaticClass(), TargetPosition, FRotator::ZeroRotator, SpawnParams);
		if (GemActor)
		{
			// Add mesh component
			UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(GemActor, TEXT("GemMesh"));
			if (MeshComp)
			{
				// Load gem mesh
				UStaticMesh* GemMesh = LoadObject<UStaticMesh>(nullptr, *Props->WorldMeshPath);
				if (GemMesh)
				{
					MeshComp->SetStaticMesh(GemMesh);
				}
				else
				{
					// Fallback: use a small sphere primitive
					UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr,
						TEXT("/Engine/BasicShapes/Sphere.Sphere"));
					if (SphereMesh)
					{
						MeshComp->SetStaticMesh(SphereMesh);
						MeshComp->SetWorldScale3D(FVector(0.08f)); // Tiny gem-sized
					}
				}

				// Apply gem color tint via dynamic material
				MeshComp->RegisterComponent();
				GemActor->SetRootComponent(MeshComp);

				UMaterialInstanceDynamic* DynMat = MeshComp->CreateDynamicMaterialInstance(0);
				if (DynMat)
				{
					DynMat->SetVectorParameterValue(TEXT("BaseColor"), Props->Color);
					DynMat->SetScalarParameterValue(TEXT("Metallic"), 0.8f);
					DynMat->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
				}

				// Tag the actor so it can be picked up
				GemActor->Tags.Add(TEXT("GemDrop"));
				GemActor->Tags.Add(*FString::Printf(TEXT("Gem_%s"), *Props->DisplayName.ToString()));
				GemActor->Tags.Add(*FString::Printf(TEXT("Quality_%.0f"), GemResult.Quality));
				GemActor->Tags.Add(*FString::Printf(TEXT("Value_%d"), GemResult.Value));

				// Auto-destroy after 5 minutes if not picked up
				GemActor->SetLifeSpan(300.0f);
			}
		}
	}

	// Notify UI — flash gem discovery popup
	OnGemFound_Event.Broadcast(GemResult);

	// TODO: Add gem to player inventory system when inventory is wired up
	// For now it spawns in world and can be picked up via interaction
}
