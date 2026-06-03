// Source/AOC/Spellcraft/AoCSpellData.h
// Spell database: enums, structs, and static spell library for all 10 magic schools (160 spells)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AoCSpellData.generated.h"

/**
 * The 10 Magic Schools of Architect of Creation.
 */
UENUM(BlueprintType)
enum class EAoCMagicSchool : uint8
{
	Arcana        UMETA(DisplayName = "Arcana"),
	Pyromancy     UMETA(DisplayName = "Pyromancy"),
	Cryomancy     UMETA(DisplayName = "Cryomancy"),
	Stormcalling  UMETA(DisplayName = "Stormcalling"),
	Necromancy    UMETA(DisplayName = "Necromancy"),
	Verdancy      UMETA(DisplayName = "Verdancy"),
	Umbramancy    UMETA(DisplayName = "Umbramancy"),
	Radiance      UMETA(DisplayName = "Radiance"),
	Sangromancy   UMETA(DisplayName = "Sangromancy"),
	Dominion      UMETA(DisplayName = "Dominion"),
	MAX           UMETA(Hidden)
};

/**
 * Spell delivery/behavior types.
 */
UENUM(BlueprintType)
enum class EAoCSpellType : uint8
{
	Projectile  UMETA(DisplayName = "Projectile"),
	AOE         UMETA(DisplayName = "AOE"),
	Instant     UMETA(DisplayName = "Instant"),
	Buff        UMETA(DisplayName = "Buff"),
	DOT         UMETA(DisplayName = "DOT"),
	Channeled   UMETA(DisplayName = "Channeled"),
	Beam        UMETA(DisplayName = "Beam"),
	Shield      UMETA(DisplayName = "Shield"),
	Summon      UMETA(DisplayName = "Summon"),
	Self        UMETA(DisplayName = "Self"),
	MAX         UMETA(Hidden)
};

/**
 * Complete spell definition — contains every property needed to cast and display a spell.
 */
USTRUCT(BlueprintType)
struct AOC_API FAoCSpellInfo
{
	GENERATED_BODY()

	/** Unique identifier for this spell */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
	FName SpellID;

	/** Human-readable name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
	FString DisplayName;

	/** Flavor/tooltip text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
	FString Description;

	/** Which magic school this spell belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
	EAoCMagicSchool School;

	/** How the spell is delivered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell")
	EAoCSpellType Type;

	/** Base damage (or healing amount for heals) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float BaseDamage;

	/** Mana cost to cast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float ManaCost;

	/** Time in seconds to cast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float CastTime;

	/** Cooldown in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float Cooldown;

	/** Maximum range in Unreal units (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float Range;

	/** AOE / effect radius in Unreal units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float Radius;

	/** Duration in seconds for buffs, DOTs, channeled spells */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float Duration;

	/** Speed of projectile in Unreal units/sec */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	float ProjectileSpeed;

	/** Number of projectiles (for multi-missile spells) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Stats")
	int32 NumberOfProjectiles;

	/** Progression rank within the school (1 = first unlocked, 16 = ultimate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Progression")
	int32 SchoolLevel;

	/** True if this spell hits an area */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Flags")
	bool bIsAOE;

	/** True if this spell is channeled over its duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Flags")
	bool bIsChanneled;

	/** True if a target must be selected before casting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spell|Flags")
	bool bRequiresTarget;

	FAoCSpellInfo()
		: School(EAoCMagicSchool::Arcana)
		, Type(EAoCSpellType::Projectile)
		, BaseDamage(0.f)
		, ManaCost(0.f)
		, CastTime(0.f)
		, Cooldown(0.f)
		, Range(0.f)
		, Radius(0.f)
		, Duration(0.f)
		, ProjectileSpeed(3000.f)
		, NumberOfProjectiles(1)
		, SchoolLevel(1)
		, bIsAOE(false)
		, bIsChanneled(false)
		, bRequiresTarget(true)
	{
	}
};

/**
 * Visual theme for a magic school — colors, glow, particle parameters.
 */
USTRUCT(BlueprintType)
struct AOC_API FAoCSchoolVisuals
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor PrimaryColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor SecondaryColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float GlowIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float ParticleScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FVector ProjectileMeshScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float ImpactDecalSize;

	FAoCSchoolVisuals()
		: PrimaryColor(FLinearColor::White)
		, SecondaryColor(FLinearColor::White)
		, GlowIntensity(5.f)
		, ParticleScale(1.f)
		, ProjectileMeshScale(FVector(0.3f))
		, ImpactDecalSize(100.f)
	{
	}
};

/**
 * Static spell database — authoritative source for all 160 spell definitions.
 * Access via static helper functions; no instance required.
 */
UCLASS(BlueprintType)
class AOC_API UAoCSpellDatabase : public UObject
{
	GENERATED_BODY()

public:

	/** Get every spell in the game */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spells")
	static TArray<FAoCSpellInfo> GetAllSpells();

	/** Get all spells belonging to a specific magic school */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spells")
	static TArray<FAoCSpellInfo> GetSpellsBySchool(EAoCMagicSchool School);

	/** Find a single spell by its unique ID. Returns nullptr if not found. */
	static FAoCSpellInfo* FindSpell(FName SpellID);

	/** Get the colour / glow theme for a school */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spells")
	static FAoCSchoolVisuals GetSchoolVisuals(EAoCMagicSchool School);

	/** Human-readable school name */
	UFUNCTION(BlueprintCallable, Category = "AoC|Spells")
	static FString GetSchoolName(EAoCMagicSchool School);

private:
	/** Lazily initialised spell table */
	static TArray<FAoCSpellInfo>& GetSpellTable();
	static bool bInitialised;
	static TArray<FAoCSpellInfo> SpellTable;

	/** Build the full spell table (called once) */
	static void InitialiseSpells();
};
