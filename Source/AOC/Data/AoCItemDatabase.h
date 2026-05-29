// AoCItemDatabase.h
// Architect of Creation - Item Database Subsystem
// Centralized item definition registry for all game items.
// GameInstanceSubsystem — lives as long as the game instance.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "AoCItemDatabase.generated.h"

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EAoCItemCategory : uint8
{
	RawOre			UMETA(DisplayName = "Raw Ore"),
	Lump			UMETA(DisplayName = "Lump"),
	Bar				UMETA(DisplayName = "Bar"),
	Ingot			UMETA(DisplayName = "Ingot"),
	Crop			UMETA(DisplayName = "Crop"),
	Seed			UMETA(DisplayName = "Seed"),
	Soil			UMETA(DisplayName = "Soil"),
	Wood			UMETA(DisplayName = "Wood"),
	Stone			UMETA(DisplayName = "Stone"),
	Tool			UMETA(DisplayName = "Tool"),
	Weapon			UMETA(DisplayName = "Weapon"),
	Armor			UMETA(DisplayName = "Armor"),
	Food			UMETA(DisplayName = "Food"),
	Material		UMETA(DisplayName = "Material"),
	Herb			UMETA(DisplayName = "Herb"),
	Misc			UMETA(DisplayName = "Misc")
};

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
	Common			UMETA(DisplayName = "Common"),
	Uncommon		UMETA(DisplayName = "Uncommon"),
	Rare			UMETA(DisplayName = "Rare"),
	Epic			UMETA(DisplayName = "Epic"),
	Legendary		UMETA(DisplayName = "Legendary")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FItemDefinition : public FTableRowBase
{
	GENERATED_BODY()

	/** Unique identifier for this item (e.g. "Copper_RawOre"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemID;

	/** Human-readable display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString DisplayName;

	/** Tooltip description. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString Description;

	/** Primary category for sorting and filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EAoCItemCategory Category = EAoCItemCategory::Misc;

	/** Rarity tier — affects name color and drop weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EItemRarity Rarity = EItemRarity::Common;

	/** Soft reference to 128x128 inventory icon texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Assets")
	FSoftObjectPath IconPath;

	/** Soft reference to 3D mesh for world / in-hand display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Assets")
	FSoftObjectPath MeshPath;

	/** Maximum units in one inventory stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stats", meta = (ClampMin = "1", ClampMax = "999"))
	int32 MaxStackSize = 1;

	/** Weight per unit in "stones" (LiF weight unit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stats", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Base vendor sell value in copper coins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stats", meta = (ClampMin = "0"))
	int32 BaseSellValue = 0;

	/** Progression tier 1-6.  Higher tier = later game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Stats", meta = (ClampMin = "1", ClampMax = "6"))
	uint8 Tier = 1;

	FItemDefinition()
		: ItemID(NAME_None)
		, DisplayName(TEXT(""))
		, Description(TEXT(""))
		, Category(EAoCItemCategory::Misc)
		, Rarity(EItemRarity::Common)
		, MaxStackSize(1)
		, Weight(1.0f)
		, BaseSellValue(0)
		, Tier(1)
	{}
};

// ─── Subsystem ───────────────────────────────────────────────────────────────

UCLASS()
class AOC_API UAoCItemDatabase : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Called automatically when the GameInstance creates subsystems. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Called when the GameInstance is torn down. */
	virtual void Deinitialize() override;

	// ── Queries ─────────────────────────────────────────────────────────────

	/** Retrieve an item definition by ID.  Returns nullptr if not found. */
	UFUNCTION(BlueprintCallable, Category = "ItemDatabase")
	const FItemDefinition* GetItem(FName ItemID) const;

	/** Return all items in a given category. */
	UFUNCTION(BlueprintCallable, Category = "ItemDatabase")
	TArray<FItemDefinition> GetItemsByCategory(EAoCItemCategory Cat) const;

	/** Return all items of a given progression tier (1-6). */
	UFUNCTION(BlueprintCallable, Category = "ItemDatabase")
	TArray<FItemDefinition> GetItemsByTier(uint8 Tier) const;

	/** Return all items matching a given rarity. */
	UFUNCTION(BlueprintCallable, Category = "ItemDatabase")
	TArray<FItemDefinition> GetItemsByRarity(EItemRarity Rarity) const;

	/** Total number of registered items. */
	UFUNCTION(BlueprintPure, Category = "ItemDatabase")
	int32 GetItemCount() const;

protected:

	/** Master registry: ItemID → definition. */
	UPROPERTY()
	TMap<FName, FItemDefinition> Items;

private:

	// ── Registration helpers (called from Initialize) ───────────────────────

	/** Register a single item definition. */
	void RegisterItem(const FItemDefinition& Def);

	/**
	 * Register all four metal forms (RawOre, Lump, Bar, Ingot) for a given metal.
	 * @param MetalName   PascalCase metal name, e.g. "Copper"
	 * @param Tier        Progression tier 1-6
	 * @param Rarity      Item rarity
	 */
	void RegisterMetal(const FString& MetalName, uint8 Tier, EItemRarity Rarity);

	/** Register a soil type. */
	void RegisterSoil(const FString& SoilName, const FString& Description, float Weight, int32 SellValue);

	/** Register a harvested crop item. */
	void RegisterCrop(const FString& CropName, const FString& Desc, uint8 Tier, EItemRarity Rarity, float Weight, int32 SellValue);

	/** Register a seed item. */
	void RegisterSeed(const FString& CropName, const FString& Desc, uint8 Tier, EItemRarity Rarity, int32 SellValue);

	// ── Batch population ────────────────────────────────────────────────────

	void PopulateMetals();
	void PopulateSoils();
	void PopulateCrops();
	void PopulateSeeds();
};
