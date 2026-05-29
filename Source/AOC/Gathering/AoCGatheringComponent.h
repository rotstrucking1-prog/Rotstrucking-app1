#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCGatheringComponent.generated.h"

UENUM(BlueprintType)
enum class EGatheringType : uint8
{
	Mining			UMETA(DisplayName = "Mining"),
	Woodcutting		UMETA(DisplayName = "Woodcutting"),
	Quarrying		UMETA(DisplayName = "Quarrying"),
	Herbalism		UMETA(DisplayName = "Herbalism"),
	Sawing			UMETA(DisplayName = "Sawing")
};

UENUM(BlueprintType)
enum class EWoodProduct : uint8
{
	Log				UMETA(DisplayName = "Log"),
	Plank			UMETA(DisplayName = "Plank"),
	Beam			UMETA(DisplayName = "Beam"),
	Pole			UMETA(DisplayName = "Pole"),
	Firewood		UMETA(DisplayName = "Firewood")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGatherComplete, EGatheringType, GatherType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGatherHit, EGatheringType, GatherType, int32, HitCount);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCGatheringComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCGatheringComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering")
	TMap<EGatheringType, float> GatheringSkills;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering|Stamina")
	float CurrentStamina;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gathering|Stamina")
	float MaxStamina;

	UPROPERTY(BlueprintReadOnly, Category = "Gathering")
	bool bIsGathering;

	UPROPERTY(BlueprintReadOnly, Category = "Gathering")
	EGatheringType CurrentGatherType;

	FTimerHandle GatherTimerHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Gathering")
	AActor* CurrentTargetNode;

	UPROPERTY(BlueprintReadOnly, Category = "Gathering")
	float GatherProgress;

	UPROPERTY(BlueprintReadOnly, Category = "Gathering")
	int32 GatherHitCount;

	// --- Delegates ---

	UPROPERTY(BlueprintAssignable, Category = "Gathering|Events")
	FOnGatherComplete OnGatherComplete;

	UPROPERTY(BlueprintAssignable, Category = "Gathering|Events")
	FOnGatherHit OnGatherHit;

	// --- Functions ---

	UFUNCTION(BlueprintCallable, Category = "Gathering")
	bool StartGathering(AActor* ResourceNode, EGatheringType Type);

	UFUNCTION(BlueprintCallable, Category = "Gathering")
	void StopGathering();

	void OnGatherTick();

	float GetGatherInterval(EGatheringType Type) const;
	float GetStaminaCost(EGatheringType Type) const;
	float GetYieldMultiplier(EGatheringType Type) const;

	void GainSkillXP(EGatheringType Type, float Amount);

	bool HasRequiredTool(EGatheringType Type) const;

	FName GetGatherAnimation(EGatheringType Type) const;

	UFUNCTION(BlueprintCallable, Category = "Gathering")
	float GetSkill(EGatheringType Type) const;
};
