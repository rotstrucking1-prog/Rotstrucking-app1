// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AoCQuestData.generated.h"

/** Quest progress state. */
UENUM(BlueprintType)
enum class EQuestStatus : uint8
{
	NotStarted		UMETA(DisplayName = "Not Started"),
	InProgress		UMETA(DisplayName = "In Progress"),
	Completed		UMETA(DisplayName = "Completed"),
	Failed			UMETA(DisplayName = "Failed")
};

/** Type of quest objective. */
UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
	Kill		UMETA(DisplayName = "Kill"),
	Collect		UMETA(DisplayName = "Collect"),
	Explore		UMETA(DisplayName = "Explore"),
	Talk		UMETA(DisplayName = "Talk"),
	Craft		UMETA(DisplayName = "Craft")
};

/**
 * FAoCQuestObjective
 * A single objective within a quest.
 */
USTRUCT(BlueprintType)
struct ARCHITECTOFCREATION_API FAoCQuestObjective
{
	GENERATED_BODY()

	/** Type of objective (Kill, Collect, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	EQuestObjectiveType Type;

	/** Target identifier (creature ID, item ID, location tag, NPC name, recipe ID). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	FName TargetID;

	/** Number required to complete this objective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest", meta = (ClampMin = "1"))
	int32 RequiredCount;

	/** Current progress toward this objective. */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|Quest")
	int32 CurrentCount;

	/** Human-readable description of this objective. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	FString Description;

	/** Returns true when CurrentCount >= RequiredCount. */
	bool IsComplete() const { return CurrentCount >= RequiredCount; }

	FAoCQuestObjective()
		: Type(EQuestObjectiveType::Kill)
		, TargetID(NAME_None)
		, RequiredCount(1)
		, CurrentCount(0)
	{}
};

/**
 * FAoCQuestRewardItem
 * An item granted as a quest reward.
 */
USTRUCT(BlueprintType)
struct ARCHITECTOFCREATION_API FAoCQuestRewardItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest", meta = (ClampMin = "1"))
	int32 Quantity;

	FAoCQuestRewardItem()
		: ItemID(NAME_None)
		, Quantity(1)
	{}
};

/**
 * FAoCQuestData
 * Full data definition for a single quest.
 */
USTRUCT(BlueprintType)
struct ARCHITECTOFCREATION_API FAoCQuestData
{
	GENERATED_BODY()

	/** Unique quest identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	FName QuestID;

	/** Display title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	FString Title;

	/** Long description / lore text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	FString Description;

	/** Current status. */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|Quest")
	EQuestStatus Status;

	/** Objectives to complete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	TArray<FAoCQuestObjective> Objectives;

	/** XP reward on completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest", meta = (ClampMin = "0"))
	int32 XPReward;

	/** Item rewards on completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	TArray<FAoCQuestRewardItem> ItemRewards;

	/** Quest IDs that must be completed before this quest can be accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|Quest")
	TArray<FName> Prerequisites;

	/** Returns true if all objectives are complete. */
	bool AreAllObjectivesComplete() const
	{
		for (const FAoCQuestObjective& Obj : Objectives)
		{
			if (!Obj.IsComplete())
			{
				return false;
			}
		}
		return true;
	}

	FAoCQuestData()
		: QuestID(NAME_None)
		, Status(EQuestStatus::NotStarted)
		, XPReward(0)
	{}
};
