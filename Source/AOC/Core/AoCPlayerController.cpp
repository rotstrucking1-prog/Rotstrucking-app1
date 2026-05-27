// Copyright Architect of Creation. All Rights Reserved.

#include "AoCPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"

AAoCPlayerController::AAoCPlayerController()
{
	bInventoryOpen = false;
	bSpellbookOpen = false;
	bPauseOpen = false;
}

void AAoCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add the default mapping context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Create HUD widget for local player
	if (IsLocalController() && HUDWidgetClass)
	{
		HUDWidgetInstance = CreateWidget<UUserWidget>(this, HUDWidgetClass);
		if (HUDWidgetInstance)
		{
			HUDWidgetInstance->AddToViewport();
		}
	}
}

void AAoCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_ToggleInventory)
		{
			EnhancedInput->BindAction(IA_ToggleInventory, ETriggerEvent::Started, this, &AAoCPlayerController::HandleToggleInventory);
		}
		if (IA_ToggleSpellbook)
		{
			EnhancedInput->BindAction(IA_ToggleSpellbook, ETriggerEvent::Started, this, &AAoCPlayerController::HandleToggleSpellbook);
		}
		if (IA_Pause)
		{
			EnhancedInput->BindAction(IA_Pause, ETriggerEvent::Started, this, &AAoCPlayerController::HandlePause);
		}
	}
}

void AAoCPlayerController::HandleToggleInventory()
{
	ToggleInventoryPanel();
}

void AAoCPlayerController::HandleToggleSpellbook()
{
	ToggleSpellbookPanel();
}

void AAoCPlayerController::HandlePause()
{
	TogglePauseMenu();
}

void AAoCPlayerController::ToggleInventoryPanel()
{
	bInventoryOpen = !bInventoryOpen;
	OnToggleInventory.Broadcast();

	if (bInventoryOpen)
	{
		SetInputMode(FInputModeGameAndUI());
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void AAoCPlayerController::ToggleSpellbookPanel()
{
	bSpellbookOpen = !bSpellbookOpen;
	OnToggleSpellbook.Broadcast();

	if (bSpellbookOpen)
	{
		SetInputMode(FInputModeGameAndUI());
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void AAoCPlayerController::TogglePauseMenu()
{
	bPauseOpen = !bPauseOpen;
	OnPauseGame.Broadcast();

	if (bPauseOpen)
	{
		SetInputMode(FInputModeUIOnly());
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}
