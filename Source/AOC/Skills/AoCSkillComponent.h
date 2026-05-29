#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCSkillComponent.generated.h"

UENUM(BlueprintType)
enum class EAoCSkillType : uint8
{
    // Weapon Mastery (17)
    Sword, Greatsword, Dagger, Axe, GreatAxe,
    Hammer, GreatHammer, Mace, GreatMace, Scythe,
    Spear, Claws, Bow, Shield, Staff1H, Staff2H, Unarmed,
    // Magic Schools (10)
    Arcana, Pyromancy, Cryomancy, Stormcalling, Tempest,
    Verdancy, Umbramancy, Radiance, Sangromancy, Dominion,
    // Gathering (7)
    Mining, Woodcutting, Fishing, Herbalism, Hunting, Farming, Skinning,
    // Crafting (11)
    WeaponSmithing, ArmorSmithing, BowCrafting, StaffCrafting,
    Jewelcrafting, Enchanting, Alchemy, Cooking, Brewing, Tailoring, Leatherworking,
    // Construction (3)
    Masonry, Carpentry, Architecture,
    MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FAoCSkillData
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) float CurrentXP = 0.f;
    UPROPERTY(BlueprintReadOnly) float XPToNextLevel = 100.f;
    UPROPERTY(BlueprintReadOnly) bool bHasMastery = false;
    UPROPERTY(BlueprintReadOnly) int32 MasteryLevel = 0;
    
    // Darkfall-style: XP scales with level
    static float GetXPForLevel(int32 Lvl)
    {
        float Total = 0.f;
        for (int32 L = 1; L < Lvl; L++)
            Total += FMath::Floor(L + 300.f * FMath::Pow(2.f, L / 7.f)) / 4.f;
        return Total;
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillLevelUp, EAoCSkillType, Skill, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillXPGained, EAoCSkillType, Skill, float, XPAmount);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AOC_API UAoCSkillComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAoCSkillComponent();

    UPROPERTY(BlueprintAssignable) FOnSkillLevelUp OnSkillLevelUp;
    UPROPERTY(BlueprintAssignable) FOnSkillXPGained OnSkillXPGained;

    UFUNCTION(BlueprintCallable, Category = "Skills")
    void AddXP(EAoCSkillType Skill, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Skills")
    int32 GetSkillLevel(EAoCSkillType Skill) const;

    UFUNCTION(BlueprintCallable, Category = "Skills")
    float GetSkillXP(EAoCSkillType Skill) const;

    UFUNCTION(BlueprintCallable, Category = "Skills")
    float GetXPToNextLevel(EAoCSkillType Skill) const;

    UFUNCTION(BlueprintCallable, Category = "Skills")
    int32 GetTotalLevel() const;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Skills")
    TMap<EAoCSkillType, FAoCSkillData> Skills;

    virtual void BeginPlay() override;
};
