// AoCAnimationEntry.h — Animation catalog entry struct
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "AoCAnimationEntry.generated.h"

/**
 * A single entry in the Animation Laboratory catalog.
 * Describes one animation asset and its metadata for intelligent querying.
 */
USTRUCT(BlueprintType)
struct AOC_API FAoCAnimationEntry
{
	GENERATED_BODY()

	/** Unique identifier for this entry (e.g. "Standing_Melee_Attack_Downward") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName EntryName;

	/** Soft reference to the animation asset: /Game/AoC/Animations/{Folder}/{AnimName} */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FSoftObjectPath AnimAsset;

	/** High-level category: movement, combat_melee, combat_ranged, magic, idle, death, social, stealth, crafting, dance, creature, misc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FString Category;

	/** More specific sub-category (e.g. walk, run, sprint, attack_slash, idle_breathing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FString Subcategory;

	/** Duration in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float Duration = 2.0f;

	/** Body state this animation expects to start from (standing, crouching, prone, sitting, jumping, swimming) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FString BodyStateStart = TEXT("standing");

	/** Body state the character is in after this animation completes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FString BodyStateEnd = TEXT("standing");

	/** Searchable keyword tags (e.g. "aggressive", "two_handed", "sword", "fast") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FString> Tags;

	/** Whether this animation should loop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bLooping = false;

	/** Whether this animation has root motion baked in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bHasRootMotion = false;

	/** Movement direction: stationary, forward, backward, left, right, up, down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FString MovementType = TEXT("stationary");

	/** Blend-in time in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float BlendInTime = 0.25f;

	/** Blend-out time in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float BlendOutTime = 0.25f;

	/** Priority for selection — higher values are preferred when multiple entries match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	int32 Priority = 5;

	/** Returns true if this entry has valid data */
	bool IsValid() const
	{
		return !EntryName.IsNone() && !Category.IsEmpty();
	}
};
