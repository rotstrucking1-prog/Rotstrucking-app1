// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSkillComponent.generated.h"

/**
 * UAoCSkillComponent
 * Player skill component tracking skill levels for the player character.
 * Mirrors the NPC skill system but stores player-specific progression.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSkillComponent();

	/** Map of skill names to their current levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills")
	TMap<FName, int32> PlayerSkills;

	/** Get the level of a specific skill. Returns 0 if the skill is not found. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	int32 GetSkillLevel(FName SkillName) const;

	/** Set the level of a specific skill. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void SetSkillLevel(FName SkillName, int32 Level);

	/** Get the sum of all skill levels. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	int32 GetTotalLevel() const;
};
