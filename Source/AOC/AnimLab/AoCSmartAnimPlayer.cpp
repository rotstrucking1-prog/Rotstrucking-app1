// AoCSmartAnimPlayer.cpp — Smart animation playback component
// Architect of Creation (AOC) — UE5 5.7

#include "AoCSmartAnimPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Animation/AnimationAsset.h"

UAoCSmartAnimPlayer::UAoCSmartAnimPlayer()
{
	PrimaryComponentTick.bCanEverTick = false; // timer-driven, not tick-driven
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UAoCSmartAnimPlayer::BeginPlay()
{
	Super::BeginPlay();
	CachedMesh = GetMesh();
	if (!CachedMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SmartAnimPlayer] No SkeletalMeshComponent found on %s"),
			*GetOwner()->GetName());
	}
}

void UAoCSmartAnimPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AdvanceTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

USkeletalMeshComponent* UAoCSmartAnimPlayer::GetMesh() const
{
	if (CachedMesh) return CachedMesh;

	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	// Prefer the character mesh if owner is a character
	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		return Char->GetMesh();
	}

	// Fall back to first skeletal mesh on actor
	return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

// ---------------------------------------------------------------------------
// Playback API
// ---------------------------------------------------------------------------

void UAoCSmartAnimPlayer::PlayEntry(const FAoCAnimationEntry& Entry)
{
	SequenceQueue.Empty();
	ApplyAnimation(Entry);
}

void UAoCSmartAnimPlayer::PlaySequence(const TArray<FAoCAnimationEntry>& Sequence)
{
	if (Sequence.Num() == 0) return;

	// Queue everything after the first entry
	SequenceQueue.Empty(Sequence.Num() - 1);
	for (int32 i = 1; i < Sequence.Num(); ++i)
	{
		SequenceQueue.Add(Sequence[i]);
	}

	// Start the first one
	ApplyAnimation(Sequence[0]);
}

bool UAoCSmartAnimPlayer::InterruptWith(const FAoCAnimationEntry& Entry)
{
	// Priority gating — can only interrupt if new priority >= current
	if (bIsPlaying && Entry.Priority < CurrentEntry.Priority)
	{
		return false;
	}

	SequenceQueue.Empty();
	ApplyAnimation(Entry);
	return true;
}

void UAoCSmartAnimPlayer::StopAll()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AdvanceTimerHandle);
	}

	SequenceQueue.Empty();

	if (bIsPlaying)
	{
		FAoCAnimationEntry Finished = CurrentEntry;
		bIsPlaying = false;
		CurrentEntry = FAoCAnimationEntry();

		// Stop the mesh animation
		USkeletalMeshComponent* Mesh = GetMesh();
		if (Mesh)
		{
			Mesh->Stop();
		}

		OnAnimFinished.Broadcast(Finished);
	}
}

float UAoCSmartAnimPlayer::GetRemainingTime() const
{
	if (!bIsPlaying || !GetWorld()) return 0.f;
	const float Elapsed = GetWorld()->GetTimeSeconds() - PlayStartTime;
	return FMath::Max(0.f, CurrentEntry.Duration - Elapsed);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UAoCSmartAnimPlayer::ApplyAnimation(const FAoCAnimationEntry& Entry)
{
	// Clear any pending timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AdvanceTimerHandle);
	}

	// Fire finished delegate for the outgoing anim (if any)
	if (bIsPlaying)
	{
		OnAnimFinished.Broadcast(CurrentEntry);
	}

	CurrentEntry = Entry;
	bIsPlaying = true;
	PlayStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Resolve and play the animation asset
	USkeletalMeshComponent* Mesh = GetMesh();
	if (Mesh)
	{
		UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(Entry.AnimAsset.TryLoad());
		if (AnimAsset)
		{
			Mesh->PlayAnimation(AnimAsset, Entry.bLooping);
			UE_LOG(LogTemp, Verbose, TEXT("[SmartAnimPlayer] Playing: %s (%.1fs, loop=%d)"),
				*Entry.EntryName.ToString(), Entry.Duration, Entry.bLooping ? 1 : 0);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[SmartAnimPlayer] Could not load anim asset: %s"),
				*Entry.AnimAsset.GetAssetPathString());
		}
	}

	OnAnimStarted.Broadcast(CurrentEntry);

	// Set timer for auto-advancement (even for looping, so sequences can advance)
	if (GetWorld() && Entry.Duration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			AdvanceTimerHandle,
			this,
			&UAoCSmartAnimPlayer::OnCurrentAnimElapsed,
			Entry.Duration,
			false // not looping timer
		);
	}
}

void UAoCSmartAnimPlayer::OnCurrentAnimElapsed()
{
	if (!bIsPlaying) return;

	if (SequenceQueue.Num() > 0)
	{
		AdvanceSequence();
	}
	else
	{
		// Single play or end of sequence
		FAoCAnimationEntry Finished = CurrentEntry;
		bIsPlaying = false;
		CurrentEntry = FAoCAnimationEntry();
		OnAnimFinished.Broadcast(Finished);
		OnSequenceComplete.Broadcast();
	}
}

void UAoCSmartAnimPlayer::AdvanceSequence()
{
	if (SequenceQueue.Num() == 0)
	{
		OnSequenceComplete.Broadcast();
		return;
	}

	FAoCAnimationEntry Next = SequenceQueue[0];
	SequenceQueue.RemoveAt(0);
	ApplyAnimation(Next);
}
