// AoCAILODManager.cpp
// AI LOD Manager — full implementation
// Spatial hash grid, stagger groups, materialization queue, budget tracking

#include "AoCAILODManager.h"
#include "AoCHumanoidNPCV2.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogAoCNPC);

// ---------------------------------------------------------------------------
// Constructor & Subsystem Lifecycle
// ---------------------------------------------------------------------------

UAoCAILODManager::UAoCAILODManager()
{
}

bool UAoCAILODManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create in all game worlds (not editor preview)
	UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAoCAILODManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAoCNPC, Log, TEXT("AILODManager: Initialized"));
}

void UAoCAILODManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Register a tick function via timer so we tick every frame
	if (UWorld* World = GetWorld())
	{
		FTimerHandle TickHandle;
		// We use a timer with 0 rate for per-frame ticking
		World->GetTimerManager().SetTimer(
			TickHandle,
			[this]()
			{
				if (UWorld* W = GetWorld())
				{
					Tick(W->GetDeltaSeconds());
				}
			},
			0.0f, // rate (0 = every frame via tick)
			true   // loop
		);
	}

	UE_LOG(LogAoCNPC, Log, TEXT("AILODManager: World BeginPlay — tick registered"));
}

void UAoCAILODManager::Deinitialize()
{
	NPCRecords.Empty();
	SpatialHash.Empty();
	NPCToRecordIndex.Empty();
	MaterializationQueue.Empty();
	PlayerPositions.Empty();

	Super::Deinitialize();
	UE_LOG(LogAoCNPC, Log, TEXT("AILODManager: Deinitialized"));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void UAoCAILODManager::RegisterNPC(AoCHumanoidNPCV2* NPC)
{
	if (!NPC) return;

	TWeakObjectPtr<AoCHumanoidNPCV2> WeakNPC(NPC);
	if (NPCToRecordIndex.Contains(WeakNPC))
	{
		return; // Already registered
	}

	FNPCLODRecord Record;
	Record.NPC = NPC;
	Record.StaggerGroup = static_cast<uint8>(NPCRecords.Num() % NumStaggerGroups);
	Record.CurrentTier = EAILODTier::Hibernated;
	Record.PreviousTier = EAILODTier::Hibernated;
	Record.bIsMaterialized = true; // Assume spawned with mesh

	int32 Index = NPCRecords.Add(Record);
	NPCToRecordIndex.Add(WeakNPC, Index);

	UE_LOG(LogAoCNPC, Verbose, TEXT("AILODManager: Registered NPC %s (index %d, stagger %d)"),
		*NPC->GetDisplayName(), Index, Record.StaggerGroup);
}

void UAoCAILODManager::UnregisterNPC(AoCHumanoidNPCV2* NPC)
{
	if (!NPC) return;

	TWeakObjectPtr<AoCHumanoidNPCV2> WeakNPC(NPC);
	int32* IndexPtr = NPCToRecordIndex.Find(WeakNPC);
	if (!IndexPtr) return;

	int32 Index = *IndexPtr;

	// Swap-remove for O(1)
	if (Index < NPCRecords.Num() - 1)
	{
		NPCRecords[Index] = NPCRecords.Last();

		// Update the swapped NPC's index mapping
		TWeakObjectPtr<AoCHumanoidNPCV2> SwappedNPC = NPCRecords[Index].NPC;
		if (SwappedNPC.IsValid())
		{
			NPCToRecordIndex[SwappedNPC] = Index;
		}
	}

	NPCRecords.Pop(false);
	NPCToRecordIndex.Remove(WeakNPC);

	UE_LOG(LogAoCNPC, Verbose, TEXT("AILODManager: Unregistered NPC %s"), *NPC->GetDisplayName());
}

// ---------------------------------------------------------------------------
// Spatial Hash
// ---------------------------------------------------------------------------

FIntVector UAoCAILODManager::WorldToCell(const FVector& WorldPos) const
{
	return FIntVector(
		FMath::FloorToInt(WorldPos.X / SpatialCellSize),
		FMath::FloorToInt(WorldPos.Y / SpatialCellSize),
		FMath::FloorToInt(WorldPos.Z / SpatialCellSize)
	);
}

void UAoCAILODManager::RebuildSpatialHash()
{
	// Clear but keep allocations
	for (auto& Pair : SpatialHash)
	{
		Pair.Value.Reset();
	}

	for (int32 i = 0; i < NPCRecords.Num(); ++i)
	{
		const FNPCLODRecord& Record = NPCRecords[i];
		if (!Record.NPC.IsValid()) continue;

		FVector Pos = Record.NPC->GetActorLocation();
		FIntVector Cell = WorldToCell(Pos);

		TArray<int32>& CellArray = SpatialHash.FindOrAdd(Cell);
		CellArray.Add(i);
	}
}

void UAoCAILODManager::UpdatePlayerPositions()
{
	PlayerPositions.Reset();

	UWorld* World = GetWorld();
	if (!World) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			PlayerPositions.Add(PC->GetPawn()->GetActorLocation());
		}
	}
}

float UAoCAILODManager::GetDistanceToNearestPlayer(const FVector& Location) const
{
	float MinDist = MAX_FLT;
	for (const FVector& PlayerPos : PlayerPositions)
	{
		float Dist = FVector::Dist(Location, PlayerPos);
		if (Dist < MinDist)
		{
			MinDist = Dist;
		}
	}
	return MinDist;
}

// ---------------------------------------------------------------------------
// Tier Calculation
// ---------------------------------------------------------------------------

EAILODTier UAoCAILODManager::CalculateTier(float Distance) const
{
	if (Distance <= FullTierMaxDistance)
	{
		return EAILODTier::Full;
	}
	else if (Distance <= ReducedTierMaxDistance)
	{
		return EAILODTier::Reduced;
	}
	else if (Distance <= BackgroundTierMaxDistance)
	{
		return EAILODTier::Background;
	}
	else
	{
		return EAILODTier::Hibernated;
	}
}

void UAoCAILODManager::UpdateNPCTiers()
{
	for (int32 i = 0; i < NPCRecords.Num(); ++i)
	{
		FNPCLODRecord& Record = NPCRecords[i];
		if (!Record.NPC.IsValid()) continue;

		Record.DistanceToNearestPlayer = GetDistanceToNearestPlayer(Record.NPC->GetActorLocation());
		EAILODTier NewTier = CalculateTier(Record.DistanceToNearestPlayer);

		// If we're over budget, forcibly downgrade Full→Reduced
		if (bBudgetExceeded && NewTier == EAILODTier::Full)
		{
			// Only downgrade NPCs at the edge of Full range
			if (Record.DistanceToNearestPlayer > FullTierMaxDistance * 0.7f)
			{
				NewTier = EAILODTier::Reduced;
			}
		}

		if (NewTier != Record.CurrentTier)
		{
			HandleTierTransition(Record, NewTier);
		}
	}
}

void UAoCAILODManager::HandleTierTransition(FNPCLODRecord& Record, EAILODTier NewTier)
{
	EAILODTier OldTier = Record.CurrentTier;
	Record.PreviousTier = OldTier;
	Record.CurrentTier = NewTier;

	AoCHumanoidNPCV2* NPC = Record.NPC.Get();
	if (!NPC) return;

	UE_LOG(LogAoCNPC, Verbose, TEXT("AILODManager: NPC %s tier %d → %d (dist: %.0f)"),
		*NPC->GetDisplayName(), static_cast<int32>(OldTier), static_cast<int32>(NewTier),
		Record.DistanceToNearestPlayer);

	// Handle materialization transitions
	bool bNeedsMesh = (NewTier == EAILODTier::Full || NewTier == EAILODTier::Reduced);
	bool bHadMesh = (OldTier == EAILODTier::Full || OldTier == EAILODTier::Reduced);

	if (bNeedsMesh && !bHadMesh)
	{
		// Queue materialization — don't do it immediately
		FMaterializationRequest Req;
		Req.NPC = NPC;
		Req.bMaterialize = true;
		MaterializationQueue.Add(Req);
	}
	else if (!bNeedsMesh && bHadMesh)
	{
		// Queue dematerialization
		FMaterializationRequest Req;
		Req.NPC = NPC;
		Req.bMaterialize = false;
		MaterializationQueue.Add(Req);
	}
}

// ---------------------------------------------------------------------------
// Stagger
// ---------------------------------------------------------------------------

uint8 UAoCAILODManager::GetCurrentStaggerGroup() const
{
	return static_cast<uint8>(FrameCounter % NumStaggerGroups);
}

// ---------------------------------------------------------------------------
// Budget
// ---------------------------------------------------------------------------

void UAoCAILODManager::ApplyBudgetThrottling()
{
	if (LastAIFrameTimeMs > AIBudgetMs)
	{
		if (!bBudgetExceeded)
		{
			bBudgetExceeded = true;
			UE_LOG(LogAoCNPC, Warning, TEXT("AILODManager: AI budget exceeded (%.2fms > %.2fms). Throttling."),
				LastAIFrameTimeMs, AIBudgetMs);
		}
	}
	else if (bBudgetExceeded && LastAIFrameTimeMs < AIBudgetMs * 0.7f)
	{
		// Only release throttle when well under budget
		bBudgetExceeded = false;
		UE_LOG(LogAoCNPC, Log, TEXT("AILODManager: AI budget recovered (%.2fms). Releasing throttle."),
			LastAIFrameTimeMs);
	}
}

// ---------------------------------------------------------------------------
// Materialization Queue
// ---------------------------------------------------------------------------

void UAoCAILODManager::ProcessMaterializationQueue()
{
	int32 Processed = 0;

	// Process dematerializations first (cheaper, frees resources)
	for (int32 i = MaterializationQueue.Num() - 1; i >= 0; --i)
	{
		FMaterializationRequest& Req = MaterializationQueue[i];
		if (!Req.bMaterialize && Req.NPC.IsValid())
		{
			DematerializeNPC(Req.NPC.Get());
			MaterializationQueue.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}

	// Process materializations with a budget
	for (int32 i = MaterializationQueue.Num() - 1; i >= 0 && Processed < MaxMaterializationsPerFrame; --i)
	{
		FMaterializationRequest& Req = MaterializationQueue[i];
		if (Req.bMaterialize && Req.NPC.IsValid())
		{
			MaterializeNPC(Req.NPC.Get());
			MaterializationQueue.RemoveAtSwap(i, EAllowShrinking::No);
			++Processed;
		}
		else if (!Req.NPC.IsValid())
		{
			MaterializationQueue.RemoveAtSwap(i, EAllowShrinking::No);
		}
	}
}

void UAoCAILODManager::MaterializeNPC(AoCHumanoidNPCV2* NPC)
{
	if (!NPC) return;

	TWeakObjectPtr<AoCHumanoidNPCV2> WeakNPC(NPC);
	int32* IndexPtr = NPCToRecordIndex.Find(WeakNPC);
	if (IndexPtr)
	{
		FNPCLODRecord& Record = NPCRecords[*IndexPtr];
		if (Record.bIsMaterialized) return; // Already materialized
		Record.bIsMaterialized = true;
	}

	NPC->OnMaterialize();

	UE_LOG(LogAoCNPC, Verbose, TEXT("AILODManager: Materialized NPC %s"), *NPC->GetDisplayName());
}

void UAoCAILODManager::DematerializeNPC(AoCHumanoidNPCV2* NPC)
{
	if (!NPC) return;

	TWeakObjectPtr<AoCHumanoidNPCV2> WeakNPC(NPC);
	int32* IndexPtr = NPCToRecordIndex.Find(WeakNPC);
	if (IndexPtr)
	{
		FNPCLODRecord& Record = NPCRecords[*IndexPtr];
		if (!Record.bIsMaterialized) return; // Already dematerialized
		Record.bIsMaterialized = false;
	}

	NPC->OnDematerialize();

	UE_LOG(LogAoCNPC, Verbose, TEXT("AILODManager: Dematerialized NPC %s"), *NPC->GetDisplayName());
}

// ---------------------------------------------------------------------------
// Main Tick
// ---------------------------------------------------------------------------

void UAoCAILODManager::Tick(float DeltaTime)
{
	double StartTime = FPlatformTime::Seconds();

	// Update player positions for distance checks
	UpdatePlayerPositions();

	// If no players, hibernate everything
	if (PlayerPositions.Num() == 0)
	{
		for (FNPCLODRecord& Record : NPCRecords)
		{
			if (Record.CurrentTier != EAILODTier::Hibernated)
			{
				HandleTierTransition(Record, EAILODTier::Hibernated);
			}
		}
		return;
	}

	// Rebuild spatial hash (fast — just index reassignment)
	RebuildSpatialHash();

	// Update LOD tiers for all NPCs
	UpdateNPCTiers();

	// Process materialization queue (limited per frame)
	ProcessMaterializationQueue();

	// Current stagger group
	uint8 ActiveStagger = GetCurrentStaggerGroup();
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Clean up dead weak pointers and tick NPCs based on tier
	for (int32 i = NPCRecords.Num() - 1; i >= 0; --i)
	{
		FNPCLODRecord& Record = NPCRecords[i];

		if (!Record.NPC.IsValid())
		{
			// Clean up stale record
			if (i < NPCRecords.Num() - 1)
			{
				TWeakObjectPtr<AoCHumanoidNPCV2> LastNPC = NPCRecords.Last().NPC;
				NPCRecords[i] = NPCRecords.Last();
				if (LastNPC.IsValid())
				{
					NPCToRecordIndex[LastNPC] = i;
				}
			}
			NPCRecords.Pop(false);
			continue;
		}

		AoCHumanoidNPCV2* NPC = Record.NPC.Get();

		switch (Record.CurrentTier)
		{
		case EAILODTier::Full:
		{
			// Full tier: only tick NPCs in the active stagger group
			if (Record.StaggerGroup == ActiveStagger)
			{
				Record.LastFullTick = CurrentTime;
				// The NPC's own MasterTick handles full simulation
				NPC->MasterTick(DeltaTime * NumStaggerGroups); // compensate for skipped frames
			}
			break;
		}
		case EAILODTier::Reduced:
		{
			// Reduced: perception every 0.5s, full tick at lower rate
			if (CurrentTime - Record.LastReducedPerceptionTick >= ReducedPerceptionInterval)
			{
				Record.LastReducedPerceptionTick = CurrentTime;
				NPC->MasterTick(CurrentTime - Record.LastFullTick);
				Record.LastFullTick = CurrentTime;
			}
			break;
		}
		case EAILODTier::Background:
		{
			// Background: data-only sim every 1s
			if (CurrentTime - Record.LastBackgroundTick >= BackgroundSimInterval)
			{
				Record.LastBackgroundTick = CurrentTime;
				NPC->MasterTick(BackgroundSimInterval);
			}
			break;
		}
		case EAILODTier::Hibernated:
		{
			// Hibernated: major decisions only every 30s
			if (CurrentTime - Record.LastHibernatedTick >= HibernatedDecisionInterval)
			{
				Record.LastHibernatedTick = CurrentTime;
				NPC->MasterTick(HibernatedDecisionInterval);
			}
			break;
		}
		}
	}

	// Advance frame counter for stagger group rotation
	++FrameCounter;

	// Measure AI frame time
	double EndTime = FPlatformTime::Seconds();
	LastAIFrameTimeMs = static_cast<float>((EndTime - StartTime) * 1000.0);

	// Check budget
	ApplyBudgetThrottling();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

EAILODTier UAoCAILODManager::GetLODTier(AoCHumanoidNPCV2* NPC) const
{
	if (!NPC) return EAILODTier::Hibernated;

	TWeakObjectPtr<AoCHumanoidNPCV2> WeakNPC(const_cast<AoCHumanoidNPCV2*>(NPC));
	const int32* IndexPtr = NPCToRecordIndex.Find(WeakNPC);
	if (IndexPtr && *IndexPtr < NPCRecords.Num())
	{
		return NPCRecords[*IndexPtr].CurrentTier;
	}
	return EAILODTier::Hibernated;
}

TArray<AoCHumanoidNPCV2*> UAoCAILODManager::GetNPCsInRadius(const FVector& Center, float Radius) const
{
	TArray<AoCHumanoidNPCV2*> Result;

	float RadiusSq = Radius * Radius;

	// Determine which cells to check
	int32 CellRadius = FMath::CeilToInt(Radius / SpatialCellSize);
	FIntVector CenterCell = const_cast<UAoCAILODManager*>(this)->WorldToCell(Center);

	for (int32 X = CenterCell.X - CellRadius; X <= CenterCell.X + CellRadius; ++X)
	{
		for (int32 Y = CenterCell.Y - CellRadius; Y <= CenterCell.Y + CellRadius; ++Y)
		{
			for (int32 Z = CenterCell.Z - CellRadius; Z <= CenterCell.Z + CellRadius; ++Z)
			{
				FIntVector Cell(X, Y, Z);
				const TArray<int32>* Indices = SpatialHash.Find(Cell);
				if (!Indices) continue;

				for (int32 Idx : *Indices)
				{
					if (Idx < NPCRecords.Num() && NPCRecords[Idx].NPC.IsValid())
					{
						float DistSq = FVector::DistSquared(NPCRecords[Idx].NPC->GetActorLocation(), Center);
						if (DistSq <= RadiusSq)
						{
							Result.Add(NPCRecords[Idx].NPC.Get());
						}
					}
				}
			}
		}
	}

	return Result;
}

AoCHumanoidNPCV2* UAoCAILODManager::GetNearestNPCTo(AActor* Actor) const
{
	if (!Actor) return nullptr;

	FVector Pos = Actor->GetActorLocation();
	float MinDistSq = MAX_FLT;
	AoCHumanoidNPCV2* Nearest = nullptr;

	for (const FNPCLODRecord& Record : NPCRecords)
	{
		if (!Record.NPC.IsValid()) continue;
		if (Record.NPC.Get() == Actor) continue; // Skip self

		float DistSq = FVector::DistSquared(Record.NPC->GetActorLocation(), Pos);
		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			Nearest = Record.NPC.Get();
		}
	}

	return Nearest;
}

int32 UAoCAILODManager::GetNPCCountInTier(EAILODTier Tier) const
{
	int32 Count = 0;
	for (const FNPCLODRecord& Record : NPCRecords)
	{
		if (Record.CurrentTier == Tier && Record.NPC.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void UAoCAILODManager::DebugDraw() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World) return;

	for (const FNPCLODRecord& Record : NPCRecords)
	{
		if (!Record.NPC.IsValid()) continue;

		FVector Pos = Record.NPC->GetActorLocation() + FVector(0.f, 0.f, 120.f);
		FColor Color;
		FString TierStr;

		switch (Record.CurrentTier)
		{
		case EAILODTier::Full:
			Color = FColor::Green;
			TierStr = TEXT("FULL");
			break;
		case EAILODTier::Reduced:
			Color = FColor::Yellow;
			TierStr = TEXT("REDUCED");
			break;
		case EAILODTier::Background:
			Color = FColor::Orange;
			TierStr = TEXT("BG");
			break;
		case EAILODTier::Hibernated:
			Color = FColor::Red;
			TierStr = TEXT("HIB");
			break;
		}

		DrawDebugString(World, Pos, FString::Printf(TEXT("%s [%s] %.0fm"),
			*Record.NPC->GetDisplayName(), *TierStr, Record.DistanceToNearestPlayer / 100.f),
			nullptr, Color, 0.f, true);

		DrawDebugSphere(World, Record.NPC->GetActorLocation(), 50.f, 8, Color, false, 0.f);
	}

	// Draw budget info
	FString BudgetStr = FString::Printf(TEXT("AI: %.2fms / %.2fms | NPCs: %d | Full: %d | Reduced: %d | BG: %d | Hib: %d"),
		LastAIFrameTimeMs, AIBudgetMs, NPCRecords.Num(),
		GetNPCCountInTier(EAILODTier::Full),
		GetNPCCountInTier(EAILODTier::Reduced),
		GetNPCCountInTier(EAILODTier::Background),
		GetNPCCountInTier(EAILODTier::Hibernated));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, bBudgetExceeded ? FColor::Red : FColor::Green, BudgetStr);
	}
#endif
}
