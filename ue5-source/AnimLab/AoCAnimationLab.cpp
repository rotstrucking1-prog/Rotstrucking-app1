// AoCAnimationLab.cpp — Animation Laboratory subsystem implementation
// Architect of Creation (AOC) — UE5 5.7

#include "AoCAnimationLab.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Math/UnrealMathUtility.h"

// ---------------------------------------------------------------------------
// Subsystem lifecycle
// ---------------------------------------------------------------------------

void UAoCAnimationLab::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Build a bare-bones idle fallback in case the catalog is empty
	IdleFallback.EntryName = FName(TEXT("_IdleFallback"));
	IdleFallback.AnimAsset = FSoftObjectPath(TEXT("/Game/AoC/Animations/Idle/Idle"));
	IdleFallback.Category  = TEXT("idle");
	IdleFallback.Subcategory = TEXT("idle_default");
	IdleFallback.Duration  = 3.0f;
	IdleFallback.BodyStateStart = TEXT("standing");
	IdleFallback.BodyStateEnd   = TEXT("standing");
	IdleFallback.bLooping  = true;
	IdleFallback.Priority  = 1;

	LoadCatalog();

	UE_LOG(LogTemp, Log, TEXT("[AoCAnimationLab] Initialized with %d entries."), AllEntries.Num());
}

void UAoCAnimationLab::Deinitialize()
{
	AllEntries.Empty();
	CategoryIndex.Empty();
	TagIndex.Empty();
	NameIndex.Empty();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Catalog loading
// ---------------------------------------------------------------------------

FString UAoCAnimationLab::GetCatalogFilePath() const
{
	// Look next to the project file: {ProjectDir}/AoC_GameData/animation_catalog.json
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("AoC_GameData"), TEXT("animation_catalog.json"));
}

bool UAoCAnimationLab::LoadCatalog()
{
	const FString FilePath = GetCatalogFilePath();

	FString JsonRaw;
	if (!FFileHelper::LoadFileToString(JsonRaw, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AoCAnimationLab] Could not load catalog from: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonRaw);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[AoCAnimationLab] Failed to parse JSON catalog."));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EntriesArray = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("entries"), EntriesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[AoCAnimationLab] JSON catalog has no 'entries' array."));
		return false;
	}

	AllEntries.Empty(EntriesArray->Num());

	for (const TSharedPtr<FJsonValue>& Val : *EntriesArray)
	{
		const TSharedPtr<FJsonObject>* EntryObj = nullptr;
		if (!Val->TryGetObject(EntryObj)) continue;

		FAoCAnimationEntry Entry;
		if (ParseEntry(*EntryObj, Entry))
		{
			AllEntries.Add(MoveTemp(Entry));
		}
	}

	RebuildIndices();

	// Try to pick a real idle fallback from loaded data
	const TArray<int32>* IdleIndices = CategoryIndex.Find(TEXT("idle"));
	if (IdleIndices && IdleIndices->Num() > 0)
	{
		IdleFallback = AllEntries[(*IdleIndices)[0]];
	}

	UE_LOG(LogTemp, Log, TEXT("[AoCAnimationLab] Loaded %d entries from catalog."), AllEntries.Num());
	return true;
}

bool UAoCAnimationLab::ParseEntry(const TSharedPtr<FJsonObject>& JsonObj, FAoCAnimationEntry& OutEntry) const
{
	FString NameStr;
	if (!JsonObj->TryGetStringField(TEXT("name"), NameStr)) return false;
	OutEntry.EntryName = FName(*NameStr);

	FString AssetStr;
	if (JsonObj->TryGetStringField(TEXT("asset"), AssetStr))
	{
		OutEntry.AnimAsset = FSoftObjectPath(AssetStr);
	}

	JsonObj->TryGetStringField(TEXT("category"),      OutEntry.Category);
	JsonObj->TryGetStringField(TEXT("subcategory"),    OutEntry.Subcategory);
	JsonObj->TryGetNumberField(TEXT("duration"),       OutEntry.Duration);
	JsonObj->TryGetStringField(TEXT("bodyStateStart"), OutEntry.BodyStateStart);
	JsonObj->TryGetStringField(TEXT("bodyStateEnd"),   OutEntry.BodyStateEnd);
	JsonObj->TryGetBoolField(TEXT("looping"),          OutEntry.bLooping);
	JsonObj->TryGetBoolField(TEXT("hasRootMotion"),    OutEntry.bHasRootMotion);
	JsonObj->TryGetStringField(TEXT("movementType"),   OutEntry.MovementType);

	double BlendIn = 0.25, BlendOut = 0.25;
	if (JsonObj->TryGetNumberField(TEXT("blendIn"), BlendIn))   OutEntry.BlendInTime  = static_cast<float>(BlendIn);
	if (JsonObj->TryGetNumberField(TEXT("blendOut"), BlendOut)) OutEntry.BlendOutTime = static_cast<float>(BlendOut);

	int32 Prio = 5;
	if (JsonObj->TryGetNumberField(TEXT("priority"), BlendIn)) OutEntry.Priority = static_cast<int32>(BlendIn);

	const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
	if (JsonObj->TryGetArrayField(TEXT("tags"), TagsArray))
	{
		for (const auto& TagVal : *TagsArray)
		{
			FString Tag;
			if (TagVal->TryGetString(Tag))
			{
				OutEntry.Tags.Add(Tag.ToLower());
			}
		}
	}

	return OutEntry.IsValid();
}

void UAoCAnimationLab::RebuildIndices()
{
	CategoryIndex.Empty();
	TagIndex.Empty();
	NameIndex.Empty();

	for (int32 i = 0; i < AllEntries.Num(); ++i)
	{
		const FAoCAnimationEntry& E = AllEntries[i];
		CategoryIndex.FindOrAdd(E.Category.ToLower()).Add(i);
		NameIndex.Add(E.EntryName, i);

		for (const FString& Tag : E.Tags)
		{
			TagIndex.FindOrAdd(Tag.ToLower()).Add(i);
		}
	}
}

// ---------------------------------------------------------------------------
// Query functions
// ---------------------------------------------------------------------------

TArray<FAoCAnimationEntry> UAoCAnimationLab::QueryByCategory(const FString& Category) const
{
	TArray<FAoCAnimationEntry> Results;
	const FString Key = Category.ToLower();
	if (const TArray<int32>* Indices = CategoryIndex.Find(Key))
	{
		Results.Reserve(Indices->Num());
		for (int32 Idx : *Indices)
		{
			Results.Add(AllEntries[Idx]);
		}
	}
	return Results;
}

TArray<FAoCAnimationEntry> UAoCAnimationLab::QueryByTags(
	const TArray<FString>& RequiredTags,
	const TArray<FString>& OptionalTags) const
{
	// Collect candidate indices that have ALL required tags
	TSet<int32> Candidates;
	bool bFirstRequired = true;

	for (const FString& ReqTag : RequiredTags)
	{
		const FString Key = ReqTag.ToLower();
		const TArray<int32>* Indices = TagIndex.Find(Key);
		if (!Indices)
		{
			// A required tag has zero matches → no results
			return TArray<FAoCAnimationEntry>();
		}

		TSet<int32> TagSet;
		for (int32 Idx : *Indices) TagSet.Add(Idx);

		if (bFirstRequired)
		{
			Candidates = TagSet;
			bFirstRequired = false;
		}
		else
		{
			Candidates = Candidates.Intersect(TagSet);
		}
	}

	// If no required tags were given, start with all entries
	if (bFirstRequired)
	{
		for (int32 i = 0; i < AllEntries.Num(); ++i) Candidates.Add(i);
	}

	// Score candidates by optional tags
	struct FScoredEntry
	{
		int32 Index;
		int32 Score;
	};

	TArray<FScoredEntry> Scored;
	Scored.Reserve(Candidates.Num());

	for (int32 Idx : Candidates)
	{
		int32 Score = AllEntries[Idx].Priority;
		for (const FString& OptTag : OptionalTags)
		{
			const FString OptKey = OptTag.ToLower();
			for (const FString& EntryTag : AllEntries[Idx].Tags)
			{
				if (EntryTag == OptKey) { Score += 1; break; }
			}
		}
		Scored.Add({ Idx, Score });
	}

	// Sort descending by score
	Scored.Sort([](const FScoredEntry& A, const FScoredEntry& B) { return A.Score > B.Score; });

	TArray<FAoCAnimationEntry> Results;
	Results.Reserve(Scored.Num());
	for (const FScoredEntry& S : Scored)
	{
		Results.Add(AllEntries[S.Index]);
	}
	return Results;
}

FAoCAnimationEntry UAoCAnimationLab::FindBestMatch(
	const FString& Category,
	const FString& BodyState,
	const TArray<FString>& Tags) const
{
	const FString CatKey = Category.ToLower();
	const FString StateKey = BodyState.ToLower();

	const TArray<int32>* CatIndices = CategoryIndex.Find(CatKey);
	if (!CatIndices || CatIndices->Num() == 0)
	{
		return IdleFallback;
	}

	int32 BestIndex = -1;
	int32 BestScore = -1;

	for (int32 Idx : *CatIndices)
	{
		const FAoCAnimationEntry& E = AllEntries[Idx];
		int32 Score = E.Priority;

		// Bonus for matching body state
		if (E.BodyStateStart.ToLower() == StateKey)
		{
			Score += 5;
		}

		// Score by matching tags
		for (const FString& WantedTag : Tags)
		{
			const FString WantedKey = WantedTag.ToLower();
			for (const FString& EntryTag : E.Tags)
			{
				if (EntryTag == WantedKey) { Score += 2; break; }
			}
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Idx;
		}
	}

	return (BestIndex >= 0) ? AllEntries[BestIndex] : IdleFallback;
}

TArray<FAoCAnimationEntry> UAoCAnimationLab::BuildSequence(const TArray<FString>& ActionGoals) const
{
	TArray<FAoCAnimationEntry> Sequence;
	if (ActionGoals.Num() == 0) return Sequence;

	FString CurrentBodyState = TEXT("standing");

	for (const FString& Goal : ActionGoals)
	{
		// Goal can be a category name or a tag; we try both
		const FString GoalKey = Goal.ToLower();

		// First try to find by category with current body state
		TArray<FString> GoalTags = { GoalKey };
		FAoCAnimationEntry Best = FindBestMatch(GoalKey, CurrentBodyState, GoalTags);

		// If fallback was returned (no match in that category), try as tag search
		if (Best.EntryName == IdleFallback.EntryName)
		{
			TArray<FString> Required = { GoalKey };
			TArray<FString> Optional = { CurrentBodyState };
			TArray<FAoCAnimationEntry> TagResults = QueryByTags(Required, Optional);

			if (TagResults.Num() > 0)
			{
				// Pick the one whose BodyStateStart best matches
				bool bFound = false;
				for (const FAoCAnimationEntry& Candidate : TagResults)
				{
					if (Candidate.BodyStateStart.ToLower() == CurrentBodyState.ToLower())
					{
						Best = Candidate;
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					Best = TagResults[0];
				}
			}
		}

		Sequence.Add(Best);
		CurrentBodyState = Best.BodyStateEnd;
	}

	return Sequence;
}

FAoCAnimationEntry UAoCAnimationLab::GetRandomFromCategory(const FString& Category) const
{
	const FString Key = Category.ToLower();
	const TArray<int32>* Indices = CategoryIndex.Find(Key);
	if (!Indices || Indices->Num() == 0)
	{
		return IdleFallback;
	}
	const int32 RandIdx = FMath::RandRange(0, Indices->Num() - 1);
	return AllEntries[(*Indices)[RandIdx]];
}

float UAoCAnimationLab::GetAnimDuration(FName EntryName) const
{
	const int32* Idx = NameIndex.Find(EntryName);
	if (!Idx) return -1.0f;
	return AllEntries[*Idx].Duration;
}

TArray<FString> UAoCAnimationLab::GetAllCategories() const
{
	TArray<FString> Cats;
	CategoryIndex.GetKeys(Cats);
	return Cats;
}

TArray<FString> UAoCAnimationLab::GetAllTags() const
{
	TArray<FString> AllTagKeys;
	TagIndex.GetKeys(AllTagKeys);
	return AllTagKeys;
}

FAoCAnimationEntry UAoCAnimationLab::GetEntryByName(FName EntryName) const
{
	const int32* Idx = NameIndex.Find(EntryName);
	if (Idx)
	{
		return AllEntries[*Idx];
	}
	return IdleFallback;
}
