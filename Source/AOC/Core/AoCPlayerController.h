// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AoCPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UAoCHUDWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToggleInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToggleSpellbook);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseGame);

/**
 * AAoCPlayerController
 * Player controller with Enhanced Input setup and HUD widget management.
 */
UCLASS()
class ARCHITECTOFCREATION_API AAoCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAoCPlayerController();

	// ---- Input Mapping ----

	/** Default input mapping context for the player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Input action for toggling inventory. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_ToggleInventory;

	/** Input action for toggling the spellbook. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_ToggleSpellbook;

	/** Input action for pausing / opening settings menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|Input")
	TObjectPtr<UInputAction> IA_Pause;

	// ---- HUD ----

	/** HUD widget class to create on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AoC|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/** Runtime reference to the instantiated HUD widget. */
	UPROPERTY(BlueprintReadOnly, Category = "AoC|UI")
	TObjectPtr<UUserWidget> HUDWidgetInstance;

	/** Get the HUD widget instance (may be null if not yet created). */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI")
	UUserWidget* GetHUDWidget() const { return HUDWidgetInstance; }

	// ---- Delegates ----

	UPROPERTY(BlueprintAssignable, Category = "AoC|Input")
	FOnToggleInventory OnToggleInventory;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Input")
	FOnToggleSpellbook OnToggleSpellbook;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Input")
	FOnPauseGame OnPauseGame;

	// ---- Inventory / Spellbook State ----

	UFUNCTION(BlueprintCallable, Category = "AoC|UI")
	void ToggleInventoryPanel();

	UFUNCTION(BlueprintCallable, Category = "AoC|UI")
	void ToggleSpellbookPanel();

	UFUNCTION(BlueprintCallable, Category = "AoC|UI")
	void TogglePauseMenu();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandleToggleInventory();
	void HandleToggleSpellbook();
	void HandlePause();

	bool bInventoryOpen;
	bool bSpellbookOpen;
	bool bPauseOpen;
};
