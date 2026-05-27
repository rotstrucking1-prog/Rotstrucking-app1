// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AoCAbilitySystemComponent.generated.h"

/**
 * UAoCAbilitySystemComponent
 * Custom Ability System Component for Architect of Creation.
 * Extends UAbilitySystemComponent with game-specific functionality.
 */
UCLASS()
class AOC_API UAoCAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UAoCAbilitySystemComponent();
};
