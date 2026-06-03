// Source/AOC/AnimLab/AoCAnimationManager.cpp

#include "AoCAnimationManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StreamableManager.h"
#include "UObject/UObjectGlobals.h"

// ─── Constructor ────────────────────────────────────────────────────────────

UAoCAnimationManager::UAoCAnimationManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	bLoaded = false;
	AnimationBasePath = TEXT("/Game/AoC/Animations/Movement");
}

void UAoCAnimationManager::BeginPlay()
{
	Super::BeginPlay();

	if (!bLoaded)
	{
		LoadAllAnimations();
	}
}

// ─── Category detection ─────────────────────────────────────────────────────

FString UAoCAnimationManager::DetermineCategory(const FString& AssetName) const
{
	// Match based on common name patterns (case-insensitive)
	FString Upper = AssetName.ToUpper();

	// Order matters — check more specific patterns first
	if (Upper.Contains(TEXT("CAST")) || Upper.Contains(TEXT("SPELL")) || Upper.Contains(TEXT("MAGIC")))
		return TEXT("Cast");
	if (Upper.Contains(TEXT("ATTACK")) || Upper.Contains(TEXT("SLASH")) || Upper.Contains(TEXT("SWING")) || Upper.Contains(TEXT("STRIKE")))
		return TEXT("Attack");
	if (Upper.Contains(TEXT("DEATH")) || Upper.Contains(TEXT("DIE")) || Upper.Contains(TEXT("DYING")))
		return TEXT("Death");
	if (Upper.Contains(TEXT("HIT")) || Upper.Contains(TEXT("FLINCH")) || Upper.Contains(TEXT("REACT")) || Upper.Contains(TEXT("HURT")))
		return TEXT("Hit");
	if (Upper.Contains(TEXT("DODGE")) || Upper.Contains(TEXT("ROLL")) || Upper.Contains(TEXT("EVADE")))
		return TEXT("Dodge");
	if (Upper.Contains(TEXT("BLOCK")) || Upper.Contains(TEXT("SHIELD")) || Upper.Contains(TEXT("PARRY")))
		return TEXT("Block");
	if (Upper.Contains(TEXT("JUMP")) || Upper.Contains(TEXT("LEAP")))
		return TEXT("Jump");
	if (Upper.Contains(TEXT("CROUCH")) || Upper.Contains(TEXT("SNEAK")))
		return TEXT("Crouch");
	if (Upper.Contains(TEXT("RUN")) || Upper.Contains(TEXT("SPRINT")))
		return TEXT("Run");
	if (Upper.Contains(TEXT("WALK")) || Upper.Contains(TEXT("LOCOMOTION")))
		return TEXT("Walk");
	if (Upper.Contains(TEXT("IDLE")) || Upper.Contains(TEXT("STAND")) || Upper.Contains(TEXT("BREATHING")))
		return TEXT("Idle");
	if (Upper.Contains(TEXT("TURN")))
		return TEXT("Turn");
	if (Upper.Contains(TEXT("STRAFE")))
		return TEXT("Strafe");
	if (Upper.Contains(TEXT("FALL")) || Upper.Contains(TEXT("LAND")))
		return TEXT("Fall");
	if (Upper.Contains(TEXT("EMOTE")) || Upper.Contains(TEXT("GESTURE")) || Upper.Contains(TEXT("WAVE")))
		return TEXT("Emote");
	if (Upper.Contains(TEXT("INTERACT")) || Upper.Contains(TEXT("PICK")) || Upper.Contains(TEXT("USE")))
		return TEXT("Interact");

	// Default catch-all
	return TEXT("Misc");
}

// ─── Load all animations ────────────────────────────────────────────────────

void UAoCAnimationManager::LoadAllAnimations()
{
	if (bLoaded) return;

	AllAnimations.Empty();
	AnimationMap.Empty();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();

	// Scan for all UAnimSequence assets under the base path
	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssetsByPath(FName(*AnimationBasePath), AssetList, /*bRecursive=*/ true);

	int32 LoadedCount = 0;

	for (const FAssetData& Asset : AssetList)
	{
		// Filter to AnimSequence class
		if (Asset.AssetClassPath.GetAssetName() != TEXT("AnimSequence"))
		{
			continue;
		}

		UAnimSequence* Seq = Cast<UAnimSequence>(Asset.GetAsset());
		if (!Seq)
		{
			continue;
		}

		AllAnimations.Add(Seq);

		// Categorise
		FString Category = DetermineCategory(Asset.AssetName.ToString());
		AnimationMap.FindOrAdd(Category).Add(Seq);

		LoadedCount++;
	}

	bLoaded = true;

	UE_LOG(LogTemp, Log, TEXT("AoCAnimationManager: Loaded %d animations from %s"), LoadedCount, *AnimationBasePath);

	// Log category breakdown
	for (const auto& Pair : AnimationMap)
	{
		UE_LOG(LogTemp, Log, TEXT("  [%s]: %d animations"), *Pair.Key, Pair.Value.Num());
	}
}

// ─── Queries ────────────────────────────────────────────────────────────────

UAnimSequence* UAoCAnimationManager::GetAnimationForAction(FName ActionName)
{
	if (!bLoaded)
	{
		LoadAllAnimations();
	}

	FString Key = ActionName.ToString();
	TArray<UAnimSequence*>* Found = AnimationMap.Find(Key);
	if (Found && Found->Num() > 0)
	{
		return (*Found)[0];
	}

	UE_LOG(LogTemp, Warning, TEXT("AoCAnimationManager: No animation found for action '%s'"), *Key);
	return nullptr;
}

TArray<UAnimSequence*> UAoCAnimationManager::GetAllAnimations()
{
	if (!bLoaded)
	{
		LoadAllAnimations();
	}
	return AllAnimations;
}

const TMap<FString, TArray<UAnimSequence*>>& UAoCAnimationManager::GetAnimationCategories() const
{
	return AnimationMap;
}

int32 UAoCAnimationManager::GetAnimationCount() const
{
	return AllAnimations.Num();
}

// ─── Playback ───────────────────────────────────────────────────────────────

void UAoCAnimationManager::PlayAnimation(USkeletalMeshComponent* MeshComp, FName ActionName)
{
	if (!MeshComp) return;

	UAnimSequence* Seq = GetAnimationForAction(ActionName);
	if (!Seq)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot play animation — no sequence for '%s'"), *ActionName.ToString());
		return;
	}

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	if (!AnimInst)
	{
		// Fallback: play directly on mesh (no blend, immediate)
		MeshComp->PlayAnimation(Seq, false);
		UE_LOG(LogTemp, Log, TEXT("Playing animation (direct): %s"), *Seq->GetName());
		return;
	}

	// Create a dynamic montage from the sequence and play it
	UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		Seq,
		FName(TEXT("DefaultSlot")),
		0.25f, // blend in
		0.25f, // blend out
		1.0f,  // play rate
		1,     // loop count (1 = play once)
		-1.f   // blend out trigger time
	);

	if (Montage)
	{
		AnimInst->Montage_Play(Montage, 1.0f);
		UE_LOG(LogTemp, Log, TEXT("Playing animation (montage): %s"), *Seq->GetName());
	}
	else
	{
		// Fallback
		MeshComp->PlayAnimation(Seq, false);
		UE_LOG(LogTemp, Log, TEXT("Playing animation (fallback): %s"), *Seq->GetName());
	}
}
