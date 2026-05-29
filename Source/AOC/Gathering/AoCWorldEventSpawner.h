#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCResourceNode.h"
#include "AoCWorldEventSpawner.generated.h"

USTRUCT(BlueprintType)
struct FMegaNodeSpawnConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent")
	EResourceNodeType NodeType = EResourceNodeType::OreVein;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent")
	EResourceTier Tier = EResourceTier::Tier1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent")
	FName ResourceID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent")
	TArray<FVector> PossibleLocations;
};

UCLASS()
class AOC_API AAoCWorldEventSpawner : public AActor
{
	GENERATED_BODY()

public:
	AAoCWorldEventSpawner();

	virtual void BeginPlay() override;

	// --- Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent|Config")
	TArray<FMegaNodeSpawnConfig> SpawnConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent|Config")
	float MinSpawnInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent|Config")
	float MaxSpawnInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldEvent|Config")
	int32 MaxActiveMegaNodes;

	UPROPERTY(BlueprintReadOnly, Category = "WorldEvent")
	TArray<AAoCResourceNode*> ActiveMegaNodes;

	FTimerHandle SpawnTimerHandle;

	// --- Functions ---

	void TrySpawnMegaNode();

	UFUNCTION()
	void OnMegaNodeDepleted(AAoCResourceNode* Node);

	FVector PickRandomLocation(const FMegaNodeSpawnConfig& Config) const;

	void ScheduleNextSpawn();
};
