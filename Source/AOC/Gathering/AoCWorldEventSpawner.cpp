#include "AoCWorldEventSpawner.h"
#include "AoCResourceNode.h"
#include "TimerManager.h"
#include "Engine/World.h"

AAoCWorldEventSpawner::AAoCWorldEventSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	MinSpawnInterval = 600.0f;
	MaxSpawnInterval = 1800.0f;
	MaxActiveMegaNodes = 3;
}

void AAoCWorldEventSpawner::BeginPlay()
{
	Super::BeginPlay();

	// Schedule the first mega node spawn
	ScheduleNextSpawn();
}

void AAoCWorldEventSpawner::TrySpawnMegaNode()
{
	// Check if we've hit the max active mega nodes
	if (ActiveMegaNodes.Num() >= MaxActiveMegaNodes)
	{
		ScheduleNextSpawn();
		return;
	}

	// Need at least one spawn config
	if (SpawnConfigs.Num() == 0)
	{
		ScheduleNextSpawn();
		return;
	}

	// Pick a random spawn config
	const int32 ConfigIndex = FMath::RandRange(0, SpawnConfigs.Num() - 1);
	const FMegaNodeSpawnConfig& Config = SpawnConfigs[ConfigIndex];

	// Pick a random location from the config
	const FVector SpawnLocation = PickRandomLocation(Config);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawn the mega resource node
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AAoCResourceNode* MegaNode = World->SpawnActor<AAoCResourceNode>(
		AAoCResourceNode::StaticClass(),
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (MegaNode)
	{
		// Configure as mega node
		MegaNode->NodeType = Config.NodeType;
		MegaNode->NodeSize = EResourceNodeSize::Mega;
		MegaNode->Tier = Config.Tier;
		MegaNode->ResourceID = Config.ResourceID;
		MegaNode->DisplayName = Config.DisplayName;

		// Re-initialize with mega settings
		MegaNode->InitializeNode();

		// Bind depletion delegate
		MegaNode->OnNodeDepleted.AddDynamic(this, &AAoCWorldEventSpawner::OnMegaNodeDepleted);

		// Track the active mega node
		ActiveMegaNodes.Add(MegaNode);

		UE_LOG(LogTemp, Log, TEXT("WorldEventSpawner: Spawned Mega Node '%s' at %s"),
			*Config.DisplayName, *SpawnLocation.ToString());
	}

	// Schedule the next spawn
	ScheduleNextSpawn();
}

void AAoCWorldEventSpawner::OnMegaNodeDepleted(AAoCResourceNode* Node)
{
	if (!Node)
	{
		return;
	}

	// Remove from active list
	ActiveMegaNodes.Remove(Node);

	UE_LOG(LogTemp, Log, TEXT("WorldEventSpawner: Mega Node '%s' depleted. Destroying in 30s."),
		*Node->DisplayName);

	// Destroy the node after a 30-second delay
	if (UWorld* World = GetWorld())
	{
		FTimerHandle DestroyTimerHandle;
		TWeakObjectPtr<AAoCResourceNode> WeakNode = Node;
		World->GetTimerManager().SetTimer(
			DestroyTimerHandle,
			[WeakNode]()
			{
				if (WeakNode.IsValid())
				{
					WeakNode->Destroy();
				}
			},
			30.0f,
			false
		);
	}

	// Schedule next spawn
	ScheduleNextSpawn();
}

FVector AAoCWorldEventSpawner::PickRandomLocation(const FMegaNodeSpawnConfig& Config) const
{
	if (Config.PossibleLocations.Num() == 0)
	{
		// Fallback to spawner's own location
		return GetActorLocation();
	}

	const int32 LocationIndex = FMath::RandRange(0, Config.PossibleLocations.Num() - 1);
	return Config.PossibleLocations[LocationIndex];
}

void AAoCWorldEventSpawner::ScheduleNextSpawn()
{
	if (UWorld* World = GetWorld())
	{
		const float Delay = FMath::RandRange(MinSpawnInterval, MaxSpawnInterval);
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AAoCWorldEventSpawner::TrySpawnMegaNode,
			Delay,
			false
		);

		UE_LOG(LogTemp, Log, TEXT("WorldEventSpawner: Next mega node spawn in %.0f seconds."), Delay);
	}
}
