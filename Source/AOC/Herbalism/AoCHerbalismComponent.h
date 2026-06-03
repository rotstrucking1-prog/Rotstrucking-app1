#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCHerbalismComponent.generated.h"

UENUM(BlueprintType)
enum class EHerbTier : uint8
{
	Common			UMETA(DisplayName = "Common"),
	Uncommon		UMETA(DisplayName = "Uncommon"),
	Rare			UMETA(DisplayName = "Rare"),
	Epic			UMETA(DisplayName = "Epic"),
	Legendary		UMETA(DisplayName = "Legendary")
};

USTRUCT(BlueprintType)
struct FHerbData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	FName HerbID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	EHerbTier Tier = EHerbTier::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	FString ActiveCompound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	float GatherTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	bool bDiscovered = false;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCHerbalismComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCHerbalismComponent();

	virtual void BeginPlay() override;

	// --- Properties ---

	UPROPERTY(BlueprintReadOnly, Category = "Herbalism")
	TMap<FName, FHerbData> HerbDatabase;

	UPROPERTY(BlueprintReadOnly, Category = "Herbalism")
	TSet<FName> DiscoveredHerbs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalism")
	float HerbalismSkill;

	UPROPERTY(BlueprintReadOnly, Category = "Herbalism")
	bool bIsGathering;

	FTimerHandle GatherTimer;

	UPROPERTY(BlueprintReadOnly, Category = "Herbalism")
	AActor* CurrentHerbNode;

	// --- Functions ---

	void InitHerbDatabase();

	UFUNCTION(BlueprintCallable, Category = "Herbalism")
	bool StartGatherHerb(AActor* HerbNode, FName HerbID);

	void OnGatherComplete();

	UFUNCTION(BlueprintCallable, Category = "Herbalism")
	bool IsHerbDiscovered(FName HerbID) const;

	void DiscoverHerb(FName HerbID);

	UFUNCTION(BlueprintCallable, Category = "Herbalism")
	TArray<FHerbData> GetDiscoveredHerbs() const;

	float GetGatherTimeModifier() const;

private:
	/** The HerbID currently being gathered */
	FName CurrentGatherHerbID;

	void AddHerb(FName ID, const FString& Name, EHerbTier HerbTier, const FString& Desc, const FString& Compound, float Time);
};
