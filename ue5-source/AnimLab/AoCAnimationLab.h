// AoCAnimationLab.h — Animation Laboratory subsystem
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AoCAnimationEntry.h"
#include "AoCAnimationLab.generated.h"

/**
 * UAoCAnimationLab
 *
 * Game Instance Subsystem that loads the animation catalog JSON and provides
 * intelligent query functions for the NPC AI brain and any other system that
 * needs to find the right animation for a situation.
 *
 * Catalog file: AoC_GameData/animation_catalog.json  (next to the .uproject)
 */
UCLASS()
class AOC_API UAoCAnimationLab : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// --- Subsystem lifecycle ---------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// --- Catalog loading -------------------------------------------------------

	/** Load (or reload) the animation catalog from disk. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	bool LoadCatalog();

	/** Number of entries currently loaded */
	UFUNCTION(BlueprintPure, Category = "AnimationLab")
	int32 GetEntryCount() const { return AllEntries.Num(); }

	// --- Query functions -------------------------------------------------------

	/** Return every entry in a given category. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	TArray<FAoCAnimationEntry> QueryByCategory(const FString& Category) const;

	/**
	 * Tag-based scored query.
	 * Each RequiredTag must be present; OptionalTags add +1 to the score.
	 * Results are sorted descending by score then by Priority.
	 */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	TArray<FAoCAnimationEntry> QueryByTags(
		const TArray<FString>& RequiredTags,
		const TArray<FString>& OptionalTags) const;

	/**
	 * Find the single best animation for a given situation.
	 * Matches Category first, then BodyStateStart, then scores by Tags.
	 * Falls back to any idle animation if nothing matches.
	 */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	FAoCAnimationEntry FindBestMatch(
		const FString& Category,
		const FString& BodyState,
		const TArray<FString>& Tags) const;

	/**
	 * Build a sequence of animations that chain together.
	 * Each ActionGoal is a category or tag hint; the function finds an entry
	 * whose BodyStateStart matches the previous entry's BodyStateEnd.
	 */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	TArray<FAoCAnimationEntry> BuildSequence(const TArray<FString>& ActionGoals) const;

	/** Return a random entry from the given category. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	FAoCAnimationEntry GetRandomFromCategory(const FString& Category) const;

	/** Get the duration of a specific entry by name. Returns -1 if not found. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	float GetAnimDuration(FName EntryName) const;

	/** Get all distinct categories in the catalog. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	TArray<FString> GetAllCategories() const;

	/** Get all unique tags across every entry. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	TArray<FString> GetAllTags() const;

	/** Lookup a single entry by name. Returns an invalid entry if not found. */
	UFUNCTION(BlueprintCallable, Category = "AnimationLab")
	FAoCAnimationEntry GetEntryByName(FName EntryName) const;

private:

	/** Full flat list of all entries */
	UPROPERTY()
	TArray<FAoCAnimationEntry> AllEntries;

	/** Index: Category → array of indices into AllEntries */
	TMap<FString, TArray<int32>> CategoryIndex;

	/** Index: Tag → array of indices into AllEntries */
	TMap<FString, TArray<int32>> TagIndex;

	/** Index: EntryName → index into AllEntries */
	TMap<FName, int32> NameIndex;

	/** Cached idle fallback entry */
	FAoCAnimationEntry IdleFallback;

	/** Rebuild all index maps from AllEntries */
	void RebuildIndices();

	/** Parse a single JSON entry object into an FAoCAnimationEntry */
	bool ParseEntry(const TSharedPtr<class FJsonObject>& JsonObj, FAoCAnimationEntry& OutEntry) const;

	/** Resolve the catalog file path on disk */
	FString GetCatalogFilePath() const;
};
