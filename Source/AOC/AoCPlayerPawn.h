// AoCPlayerPawn.h
// Architect of Creation - Player Pawn
// Third-person character with camera boom, WASD movement, and component slots
// for all gameplay systems (mining, smelting, prospecting, terraforming, etc.).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AoCPlayerPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAoCMiningComponent;
class UAoCProspectingComponent;
class UAoCSmeltingComponent;
class UAoCTerraformingComponent;
class UAoCGatheringComponent;
class UAoCHerbalismComponent;
class UAoCInventoryComponent;
class UAoCSkillComponent;
class UAoCContextMenuManager;

/**
 * Main player character for Architect of Creation.
 *
 * Uses a third-person camera boom with controller rotation.
 * Movement is polled each tick via IsInputKeyDown (WASD + SpaceBar).
 * Gameplay components are attached as sub-objects and accessible via getter functions.
 */
UCLASS()
class AOC_API AAoCPlayerPawn : public ACharacter
{
	GENERATED_BODY()

public:
	AAoCPlayerPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ── Camera ──────────────────────────────────────────────────────────────

	/** Camera boom positioning the camera behind the character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	/** Follow camera attached to the boom. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;

	// ── Gameplay Components ─────────────────────────────────────────────────

	/** Mining: dig ore from voxel world, extract resources. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCMiningComponent* MiningComponent;

	/** Prospecting: detect nearby ore veins underground. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCProspectingComponent* ProspectingComponent;

	/** Smelting: operate furnaces, smelt ore into ingots. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCSmeltingComponent* SmeltingComponent;

	/** Terraforming: raise, lower, flatten, slope terrain with shovel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCTerraformingComponent* TerraformingComponent;

	/** Gathering: collect resources from world nodes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCGatheringComponent* GatheringComponent;

	/** Herbalism: identify and gather wild herbs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCHerbalismComponent* HerbalismComponent;

	/** Inventory: carry items, manage equipment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCInventoryComponent* InventoryComponent;

	/** Skills: track skill levels, XP, and progression. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UAoCSkillComponent* SkillComponent;
};
