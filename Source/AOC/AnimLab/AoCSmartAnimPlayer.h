// AoCSmartAnimPlayer.h — Smart animation playback component
// Architect of Creation (AOC) — UE5 5.7

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCAnimationEntry.h"
#include "AoCSmartAnimPlayer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAoCAnimStarted, const FAoCAnimationEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAoCAnimFinished, const FAoCAnimationEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAoCSequenceComplete);

/**
 * UAoCSmartAnimPlayer
 *
 * Attaches to any character/actor with a USkeletalMeshComponent and drives
 * animation playback through the Animation Laboratory catalog entries.
 * Supports single plays, chained sequences, interrupts, and priority gating.
 */
UCLASS(ClassGroup = (AoC), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCSmartAnimPlayer : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCSmartAnimPlayer();

	// --- Playback API ----------------------------------------------------------

	/** Play a single animation entry on the owning character's skeletal mesh. */
	UFUNCTION(BlueprintCallable, Category = "SmartAnimPlayer")
	void PlayEntry(const FAoCAnimationEntry& Entry);

	/** Play a chain of entries in order, auto-advancing when each finishes. */
	UFUNCTION(BlueprintCallable, Category = "SmartAnimPlayer")
	void PlaySequence(const TArray<FAoCAnimationEntry>& Sequence);

	/**
	 * Interrupt the current animation (if the new one has equal or higher priority)
	 * and immediately begin playing the given entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartAnimPlayer")
	bool InterruptWith(const FAoCAnimationEntry& Entry);

	/** Stop whatever is currently playing and clear the queue. */
	UFUNCTION(BlueprintCallable, Category = "SmartAnimPlayer")
	void StopAll();

	// --- State queries ---------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SmartAnimPlayer")
	bool IsPlaying() const { return bIsPlaying; }

	UFUNCTION(BlueprintPure, Category = "SmartAnimPlayer")
	FAoCAnimationEntry GetCurrentEntry() const { return CurrentEntry; }

	UFUNCTION(BlueprintPure, Category = "SmartAnimPlayer")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "SmartAnimPlayer")
	int32 GetCurrentPriority() const { return bIsPlaying ? CurrentEntry.Priority : -1; }

	// --- Delegates -------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SmartAnimPlayer")
	FOnAoCAnimStarted OnAnimStarted;

	UPROPERTY(BlueprintAssignable, Category = "SmartAnimPlayer")
	FOnAoCAnimFinished OnAnimFinished;

	UPROPERTY(BlueprintAssignable, Category = "SmartAnimPlayer")
	FOnAoCSequenceComplete OnSequenceComplete;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Cached pointer to the owning actor's skeletal mesh */
	UPROPERTY()
	TObjectPtr<class USkeletalMeshComponent> CachedMesh;

	/** Currently-playing entry */
	FAoCAnimationEntry CurrentEntry;

	/** Queued sequence (index 0 is the NEXT one to play) */
	TArray<FAoCAnimationEntry> SequenceQueue;

	/** Whether something is actively playing */
	bool bIsPlaying = false;

	/** World time when the current anim started */
	float PlayStartTime = 0.f;

	/** Timer handle for auto-advancement */
	FTimerHandle AdvanceTimerHandle;

	/** Resolve and cache the skeletal mesh component */
	class USkeletalMeshComponent* GetMesh() const;

	/** Internal: apply an animation to the mesh */
	void ApplyAnimation(const FAoCAnimationEntry& Entry);

	/** Called by the timer when the current animation's duration elapses */
	void OnCurrentAnimElapsed();

	/** Advance to the next entry in the sequence queue, or signal completion */
	void AdvanceSequence();
};
