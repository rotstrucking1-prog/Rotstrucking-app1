// Source/AOC/AnimLab/AoCAnimationManager.h
// Animation manager — scans, categorises, and plays animations from the animation library.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCAnimationManager.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
class UAnimMontage;

/**
 * Scans the animation library at /Game/AoC/Animations/Movement/ and categorises
 * animations by name pattern (Walk, Run, Idle, Attack, Cast, etc.).
 * Provides quick lookup and playback for any action name.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCAnimationManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCAnimationManager();

	// ── Loading ─────────────────────────────────────────────────────────

	/** Scan and load all animation assets from the animation directory */
	UFUNCTION(BlueprintCallable, Category = "AoC|Animation")
	void LoadAllAnimations();

	// ── Queries ─────────────────────────────────────────────────────────

	/**
	 * Get the best animation for an action name.
	 * Supported action names: Cast, Attack, Idle, Walk, Run, Jump, Death, Hit, etc.
	 * Returns the first animation in that category, or nullptr if none found.
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Animation")
	UAnimSequence* GetAnimationForAction(FName ActionName);

	/** Get every loaded animation */
	UFUNCTION(BlueprintCallable, Category = "AoC|Animation")
	TArray<UAnimSequence*> GetAllAnimations();

	/** Get the categorised animation map */
	const TMap<FString, TArray<UAnimSequence*>>& GetAnimationCategories() const;

	/** Total number of loaded animations */
	UFUNCTION(BlueprintCallable, Category = "AoC|Animation")
	int32 GetAnimationCount() const;

	// ── Playback ────────────────────────────────────────────────────────

	/**
	 * Play an animation by action name on a skeletal mesh.
	 * Creates a dynamic montage from the sequence and plays it.
	 */
	UFUNCTION(BlueprintCallable, Category = "AoC|Animation")
	void PlayAnimation(USkeletalMeshComponent* MeshComp, FName ActionName);

protected:
	virtual void BeginPlay() override;

private:
	/** All loaded animations, categorised by action type (not UPROPERTY — nested containers) */
	TMap<FString, TArray<UAnimSequence*>> AnimationMap;

	/** Flat list of every loaded sequence */
	UPROPERTY()
	TArray<UAnimSequence*> AllAnimations;

	/** Whether LoadAllAnimations has been called */
	bool bLoaded;

	/** Root content path to scan */
	FString AnimationBasePath;

	/** Categorise a single animation by its asset name */
	FString DetermineCategory(const FString& AssetName) const;
};
