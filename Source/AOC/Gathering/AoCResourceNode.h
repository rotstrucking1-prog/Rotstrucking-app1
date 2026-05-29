#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AoCResourceNode.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EResourceNodeType : uint8
{
	OreVein			UMETA(DisplayName = "Ore Vein"),
	Tree			UMETA(DisplayName = "Tree"),
	StoneDeposit	UMETA(DisplayName = "Stone Deposit"),
	HerbCluster		UMETA(DisplayName = "Herb Cluster")
};

UENUM(BlueprintType)
enum class EResourceNodeSize : uint8
{
	Small			UMETA(DisplayName = "Small"),
	Medium			UMETA(DisplayName = "Medium"),
	Large			UMETA(DisplayName = "Large"),
	Mega			UMETA(DisplayName = "Mega")
};

UENUM(BlueprintType)
enum class EResourceTier : uint8
{
	Tier1			UMETA(DisplayName = "Tier 1"),
	Tier2			UMETA(DisplayName = "Tier 2"),
	Tier3			UMETA(DisplayName = "Tier 3"),
	Tier4			UMETA(DisplayName = "Tier 4"),
	Tier5			UMETA(DisplayName = "Tier 5"),
	Tier6			UMETA(DisplayName = "Tier 6")
};

USTRUCT(BlueprintType)
struct FResourceYield
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	FName ResourceID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 MinYield = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	int32 MaxYield = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
	EResourceTier Tier = EResourceTier::Tier1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNodeDepleted, AAoCResourceNode*, DepletedNode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNodeHit, AAoCResourceNode*, HitNode, int32, RemainingHealth);

UCLASS()
class AOC_API AAoCResourceNode : public AActor
{
	GENERATED_BODY()

public:
	AAoCResourceNode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource|Mesh")
	UStaticMeshComponent* NodeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource|Mesh")
	UStaticMeshComponent* GlowMesh;

	// --- Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	EResourceNodeType NodeType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	EResourceNodeSize NodeSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	EResourceTier Tier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	FName ResourceID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	int32 MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Resource")
	int32 CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	int32 YieldPerHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource|Config")
	float RespawnTime;

	UPROPERTY(BlueprintReadOnly, Category = "Resource")
	bool bIsDepleted;

	UPROPERTY(BlueprintReadOnly, Category = "Resource")
	bool bIsMegaNode;

	FTimerHandle RespawnTimerHandle;

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category = "Resource|Events")
	FOnNodeDepleted OnNodeDepleted;

	UPROPERTY(BlueprintAssignable, Category = "Resource|Events")
	FOnNodeHit OnNodeHit;

	// --- Functions ---

	void InitializeNode();

	UFUNCTION(BlueprintCallable, Category = "Resource")
	FResourceYield HarvestHit(float SkillLevel);

	UFUNCTION(BlueprintCallable, Category = "Resource")
	bool IsDepleted() const;

	void OnDepleted();
	void Respawn();
	void SetupMegaNodeVisuals();
	float GetPulseIntensity() const;

private:
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMaterialInstance;
};
