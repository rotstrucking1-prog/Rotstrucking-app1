// AoCNPCSkillSystem.h
// NPC Skill System — mirrors the player skill system exactly
// Use-based leveling: swing sword → sword skill up
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCNPCSkillSystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAoCSkill, Log, All);

/** Skill categories */
UENUM(BlueprintType)
enum class ESkillCategory : uint8
{
	Weapon,
	Magic,
	Gathering,
	Crafting,
	Construction
};

/** All 48 skills */
UENUM(BlueprintType)
enum class ESkillID : uint8
{
	// Weapon (17)
	Sword,
	Greatsword,
	Dagger,
	Axe,
	GreatAxe,
	Hammer,
	GreatHammer,
	Mace,
	GreatMace,
	Scythe,
	Spear,
	Claws,
	Bow,
	Shield,
	Staff1H,
	Staff2H,
	Unarmed,

	// Magic (10)
	Arcana,
	Pyromancy,
	Cryomancy,
	Stormcalling,
	Tempest,
	Verdancy,
	Umbramancy,
	Radiance,
	Sangromancy,
	Dominion,

	// Gathering (7)
	Mining,
	Woodcutting,
	Fishing,
	Herbalism,
	Hunting,
	Farming,
	Skinning,

	// Crafting (11)
	WeaponSmithing,
	ArmorSmithing,
	BowCrafting,
	StaffCrafting,
	Jewelcrafting,
	Enchanting,
	Alchemy,
	Cooking,
	Brewing,
	Tailoring,
	Leatherworking,

	// Construction (3)
	Masonry,
	Carpentry,
	Architecture,

	MAX UMETA(Hidden)
};

/** Attribute IDs */
UENUM(BlueprintType)
enum class EAttributeID : uint8
{
	STR,
	DEX,
	AGI,
	INT,
	WIS,
	END,
	MAX UMETA(Hidden)
};

/** Personality archetype (referenced from AoCNPCPersonality) */
UENUM(BlueprintType)
enum class EPersonalityArchetype : uint8
{
	Villager,
	Guard,
	Bandit,
	Merchant,
	Mage,
	Hunter,
	Hermit
};

/** Per-skill data */
USTRUCT(BlueprintType)
struct FNPCSkillData
{
	GENERATED_BODY()
	friend class AAoCOracleCompanion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Skills")
	FName SkillName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Skills")
	float Level = 0.f; // 0.0 - 100.0

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Skills")
	float TotalXP = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Skills")
	bool bMasteryUnlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Skills")
	float MasteryLevel = 0.f; // 0.0 - 100.0

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|NPC|Skills")
	float LastUsedTime = 0.f;

	/** Tracks how many times this skill was used recently for diminishing returns */
	float RecentUseCount = 0.f;
	float RecentUseDecayTimer = 0.f;
};

/** Serializable snapshot for LOD transitions / save-load */
USTRUCT(BlueprintType)
struct FSkillSystemState
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<uint8, FNPCSkillData> Skills; // keyed by ESkillID cast to uint8

	UPROPERTY()
	TArray<float> Attributes; // indexed by EAttributeID

	UPROPERTY()
	float TotalCombatPower = 0.f;
};

/**
 * UAoCNPCSkillSystem — UActorComponent
 *
 * Full skill system with 48 skills, 6 attributes, use-based leveling,
 * mastery tiers, and archetype initialization.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCNPCSkillSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UAoCNPCSkillSystem();

	virtual void BeginPlay() override;

	// --- Core Skill Functions ---

	/** Gain XP in a skill from use. DifficultyModifier: >1 for hard tasks, <1 for easy. */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	void GainSkillXP(ESkillID Skill, float BaseXP, float DifficultyModifier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	float GetSkillLevel(ESkillID Skill) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	float GetAttribute(EAttributeID Attribute) const;

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	void SetSkillLevel(ESkillID Skill, float NewLevel);

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	void SetAttribute(EAttributeID Attribute, float NewValue);

	// --- Combat Calculations ---

	/** Get all skill levels — NOT a UFUNCTION because TMap<uint8,...> is not Blueprint-safe */
	void GetAllSkillLevels(TMap<uint8, FNPCSkillData>& OutSkills) const { OutSkills = Skills; }

	/** Overall combat rating based on top weapon + magic + attributes */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	float GetCombatPower() const;

	/** Damage output for a weapon skill considering skill + STR/DEX + weapon */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	float GetEffectiveDamage(ESkillID WeaponSkill) const;

	/** Defense rating considering armor skill + equipment + AGI */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	float GetEffectiveDefense() const;

	/** Spell power for a magic school considering skill + INT/WIS */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	float GetSpellPower(ESkillID MagicSchool) const;

	// --- Crafting ---

	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	bool CanCraft(FName RecipeName) const;

	// --- Queries ---

	/** Get top N skills by level */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	TArray<ESkillID> GetTopSkills(int32 Count) const;

	/** Get the highest weapon skill */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	ESkillID GetBestWeaponSkill() const;

	/** Get the highest magic skill */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	ESkillID GetBestMagicSkill() const;

	/** Check if a skill is a weapon skill */
	static bool IsWeaponSkill(ESkillID Skill);
	static bool IsMagicSkill(ESkillID Skill);
	static bool IsGatheringSkill(ESkillID Skill);
	static bool IsCraftingSkill(ESkillID Skill);
	static bool IsConstructionSkill(ESkillID Skill);
	static ESkillCategory GetSkillCategory(ESkillID Skill);
	static FName GetSkillName(ESkillID Skill);

	// --- Passive / Background ---

	/** Background NPCs gain skills slowly based on current activity */
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	void TickPassiveGains(float DeltaTime);

	/** Current activity skill the NPC is practicing (set by LifeBrain) */
	UPROPERTY(BlueprintReadWrite, Category = "AoC|NPC|Skills")
	ESkillID CurrentPracticeSkill = ESkillID::MAX;

	// --- Serialization ---
	FSkillSystemState SerializeState() const;
	void DeserializeState(const FSkillSystemState& State);

	// --- Initialization ---
	UFUNCTION(BlueprintCallable, Category = "AoC|NPC|Skills")
	void InitializeFromArchetype(EPersonalityArchetype Archetype);

	// --- XP formula helpers ---
	static float XPForLevel(float Level);
	static float LevelFromXP(float TotalXP);

private:
	/** All 48 skills */
	UPROPERTY()
	TMap<uint8, FNPCSkillData> Skills;

	/** 6 attributes */
	float Attributes[static_cast<uint8>(EAttributeID::MAX)];

	/** Apply attribute gains from skill usage */
	void ApplyAttributeGain(ESkillID Skill, float XPGained);

	/** Get diminishing returns multiplier for recent use */
	float GetDiminishingReturns(ESkillID Skill) const;

	/** Update diminishing return decay timers */
	void DecayRecentUse(float DeltaTime);

	/** XP formula constants */
	static constexpr float BaseXPPerLevel = 100.f;
	static constexpr float LevelExponent = 1.5f;
	static constexpr float AttributeGainRate = 0.1f; // 1/10th of skill rate
	static constexpr float DiminishingReturnHalfLife = 300.f; // seconds until DR resets by half
	static constexpr float PassiveGainRate = 0.02f; // XP per second for background sim

	/** Cached combat power — recalculated when skills change */
	mutable float CachedCombatPower = 0.f;
	mutable bool bCombatPowerDirty = true;

	/** Helper to init a single skill entry */
	void InitSkill(ESkillID ID, FName Name, float StartLevel = 0.f);

	/** Random stream for archetype variation */
	FRandomStream RandStream;

public:
	// --- Helper overloads for Oracle & cross-system access ---
	UFUNCTION(BlueprintPure, Category = "AoC|AI|Skills")
	int32 GetSkillUseCount(const FString& SkillName) const;

	/** String-keyed skill level map for simplified access */
	void GetAllSkillLevelsAsStrings(TMap<FString, float>& OutLevels) const
	{
		for (const auto& Pair : Skills)
		{
			OutLevels.Add(Pair.Value.SkillName.ToString(), Pair.Value.Level);
		}
	}

	/** Use count by enum ID */
	int32 GetSkillUseCountByID(ESkillID ID) const
	{
		const FNPCSkillData* Data = Skills.Find(static_cast<uint8>(ID));
		return Data ? FMath::RoundToInt(Data->RecentUseCount) : 0;
	}

	/** Attribute lookup by string name */
	float GetAttributeByName(const FString& Name) const
	{
		if (Name.Contains(TEXT("Str"))) return GetAttributeValue(EAttributeID::STR);
		if (Name.Contains(TEXT("Dex"))) return GetAttributeValue(EAttributeID::DEX);
		if (Name.Contains(TEXT("Agi"))) return GetAttributeValue(EAttributeID::AGI);
		if (Name.Contains(TEXT("Int"))) return GetAttributeValue(EAttributeID::INT);
		if (Name.Contains(TEXT("Wis"))) return GetAttributeValue(EAttributeID::WIS);
		if (Name.Contains(TEXT("End"))) return GetAttributeValue(EAttributeID::END);
		return 10.f;
	}

	float GetAttributeValue(EAttributeID Attribute) const;

};
