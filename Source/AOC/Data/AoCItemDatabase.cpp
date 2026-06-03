// AoCItemDatabase.cpp
// Architect of Creation - Item Database Subsystem Implementation

#include "AoCItemDatabase.h"

// ─────────────────────────────────────────────────────────────────────────────
// Subsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UAoCItemDatabase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PopulateMetals();
	PopulateSoils();
	PopulateCrops();
	PopulateSeeds();

	UE_LOG(LogTemp, Log, TEXT("AoCItemDatabase: Initialized with %d items."), Items.Num());
}

void UAoCItemDatabase::Deinitialize()
{
	Items.Empty();
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

const FItemDefinition* UAoCItemDatabase::GetItem(FName ItemID) const
{
	return Items.Find(ItemID);
}

bool UAoCItemDatabase::GetItemCopy(FName ItemID, FItemDefinition& OutItem) const
{
	const FItemDefinition* Found = Items.Find(ItemID);
	if (Found)
	{
		OutItem = *Found;
		return true;
	}
	return false;
}

TArray<FItemDefinition> UAoCItemDatabase::GetItemsByCategory(EAoCItemCategory Cat) const
{
	TArray<FItemDefinition> Result;
	for (const auto& Pair : Items)
	{
		if (Pair.Value.Category == Cat)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FItemDefinition> UAoCItemDatabase::GetItemsByTier(uint8 InTier) const
{
	TArray<FItemDefinition> Result;
	for (const auto& Pair : Items)
	{
		if (Pair.Value.Tier == InTier)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FItemDefinition> UAoCItemDatabase::GetItemsByRarity(EItemRarity InRarity) const
{
	TArray<FItemDefinition> Result;
	for (const auto& Pair : Items)
	{
		if (Pair.Value.Rarity == InRarity)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

int32 UAoCItemDatabase::GetItemCount() const
{
	return Items.Num();
}

// ─────────────────────────────────────────────────────────────────────────────
// Registration helpers
// ─────────────────────────────────────────────────────────────────────────────

void UAoCItemDatabase::RegisterItem(const FItemDefinition& Def)
{
	Items.Add(Def.ItemID, Def);
}

void UAoCItemDatabase::RegisterMetal(const FString& MetalName, uint8 InTier, EItemRarity InRarity)
{
	// Sell-value multiplier table by tier
	// T1=1x, T2=2x, T3=5x, T4=10x, T5=25x, T6=50x
	static const int32 TierMultiplier[] = { 0, 1, 2, 5, 10, 25, 50 };
	const int32 Mult = TierMultiplier[FMath::Clamp((int32)InTier, 1, 6)];

	// --- Raw Ore ---
	{
		FItemDefinition D;
		D.ItemID      = FName(*(MetalName + TEXT("_RawOre")));
		D.DisplayName = MetalName + TEXT(" Raw Ore");
		D.Description = FString::Printf(TEXT("Unprocessed %s ore, freshly mined."), *MetalName);
		D.Category    = EAoCItemCategory::RawOre;
		D.Rarity      = InRarity;
		D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Ore/%s_RawOre_Icon"), *MetalName));
		D.MeshPath    = FSoftObjectPath(TEXT("/Game/Items/Meshes/Ore/SM_RawOre_Generic"));
		D.MaxStackSize = 50;
		D.Weight      = 1.0f;
		D.BaseSellValue = 1 * Mult;
		D.Tier        = InTier;
		RegisterItem(D);
	}

	// --- Lump ---
	{
		FItemDefinition D;
		D.ItemID      = FName(*(MetalName + TEXT("_Lump")));
		D.DisplayName = MetalName + TEXT(" Lump");
		D.Description = FString::Printf(TEXT("A small refined lump of %s."), *MetalName);
		D.Category    = EAoCItemCategory::Lump;
		D.Rarity      = InRarity;
		D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Lump/%s_Lump_Icon"), *MetalName));
		D.MeshPath    = FSoftObjectPath(TEXT("/Game/Items/Meshes/Lump/SM_Lump_Generic"));
		D.MaxStackSize = 100;
		D.Weight      = 1.0f;
		D.BaseSellValue = 5 * Mult;
		D.Tier        = InTier;
		RegisterItem(D);
	}

	// --- Bar ---
	{
		FItemDefinition D;
		D.ItemID      = FName(*(MetalName + TEXT("_Bar")));
		D.DisplayName = MetalName + TEXT(" Bar");
		D.Description = FString::Printf(TEXT("A rectangular bar of refined %s. Equivalent to 4 ore."), *MetalName);
		D.Category    = EAoCItemCategory::Bar;
		D.Rarity      = InRarity;
		D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Bar/%s_Bar_Icon"), *MetalName));
		D.MeshPath    = FSoftObjectPath(TEXT("/Game/Items/Meshes/Bar/SM_Bar_Generic"));
		D.MaxStackSize = 25;
		D.Weight      = 4.0f;
		D.BaseSellValue = 20 * Mult;
		D.Tier        = InTier;
		RegisterItem(D);
	}

	// --- Ingot ---
	{
		FItemDefinition D;
		D.ItemID      = FName(*(MetalName + TEXT("_Ingot")));
		D.DisplayName = MetalName + TEXT(" Ingot");
		D.Description = FString::Printf(TEXT("A large ingot of %s. Equivalent to 20 ore."), *MetalName);
		D.Category    = EAoCItemCategory::Ingot;
		D.Rarity      = InRarity;
		D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Ingot/%s_Ingot_Icon"), *MetalName));
		D.MeshPath    = FSoftObjectPath(TEXT("/Game/Items/Meshes/Ingot/SM_Ingot_Generic"));
		D.MaxStackSize = 10;
		D.Weight      = 20.0f;
		D.BaseSellValue = 100 * Mult;
		D.Tier        = InTier;
		RegisterItem(D);
	}
}

void UAoCItemDatabase::RegisterSoil(const FString& SoilName, const FString& Desc, float InWeight, int32 SellValue)
{
	FItemDefinition D;
	D.ItemID      = FName(*SoilName);
	D.DisplayName = SoilName;
	D.Description = Desc;
	D.Category    = EAoCItemCategory::Soil;
	D.Rarity      = EItemRarity::Common;
	D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Soil/%s_Icon"), *SoilName));
	D.MeshPath    = FSoftObjectPath(TEXT("/Game/Items/Meshes/Soil/SM_SoilPile_Generic"));
	D.MaxStackSize = 50;
	D.Weight      = InWeight;
	D.BaseSellValue = SellValue;
	D.Tier        = 1;
	RegisterItem(D);
}

void UAoCItemDatabase::RegisterCrop(const FString& CropName, const FString& Desc, uint8 InTier, EItemRarity InRarity, float InWeight, int32 SellValue)
{
	FItemDefinition D;
	D.ItemID      = FName(*CropName);
	D.DisplayName = CropName;
	D.Description = Desc;
	D.Category    = EAoCItemCategory::Crop;
	D.Rarity      = InRarity;
	D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Crops/%s_Icon"), *CropName));
	D.MeshPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Meshes/Crops/SM_%s"), *CropName));
	D.MaxStackSize = 50;
	D.Weight      = InWeight;
	D.BaseSellValue = SellValue;
	D.Tier        = InTier;
	RegisterItem(D);
}

void UAoCItemDatabase::RegisterSeed(const FString& CropName, const FString& Desc, uint8 InTier, EItemRarity InRarity, int32 SellValue)
{
	FItemDefinition D;
	D.ItemID      = FName(*(CropName + TEXT("_Seeds")));
	D.DisplayName = CropName + TEXT(" Seeds");
	D.Description = Desc;
	D.Category    = EAoCItemCategory::Seed;
	D.Rarity      = InRarity;
	D.IconPath    = FSoftObjectPath(FString::Printf(TEXT("/Game/Items/Icons/Seeds/%s_Seeds_Icon"), *CropName));
	D.MeshPath    = FSoftObjectPath(TEXT("/Game/Items/Meshes/Seeds/SM_SeedBag_Generic"));
	D.MaxStackSize = 100;
	D.Weight      = 0.1f;
	D.BaseSellValue = SellValue;
	D.Tier        = InTier;
	RegisterItem(D);
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch population — Metals (22 metals × 4 forms = 88 items)
// ─────────────────────────────────────────────────────────────────────────────

void UAoCItemDatabase::PopulateMetals()
{
	// Tier 1 — Common (bloomery-accessible starter metals)
	RegisterMetal(TEXT("Copper"),      1, EItemRarity::Common);
	RegisterMetal(TEXT("Tin"),         1, EItemRarity::Common);

	// Tier 2 — Common (bloomery w/ bellows)
	RegisterMetal(TEXT("Iron"),        2, EItemRarity::Common);
	RegisterMetal(TEXT("Zinc"),        2, EItemRarity::Common);
	RegisterMetal(TEXT("Lead"),        2, EItemRarity::Common);

	// Tier 3 — Uncommon (precious / mid-game)
	RegisterMetal(TEXT("Nickel"),      3, EItemRarity::Uncommon);
	RegisterMetal(TEXT("Silver"),      3, EItemRarity::Uncommon);
	RegisterMetal(TEXT("Gold"),        3, EItemRarity::Uncommon);

	// Tier 4 — Rare (blast-furnace metals)
	RegisterMetal(TEXT("Chromium"),    4, EItemRarity::Rare);
	RegisterMetal(TEXT("Cobalt"),      4, EItemRarity::Rare);
	RegisterMetal(TEXT("Manganese"),   4, EItemRarity::Rare);
	RegisterMetal(TEXT("Molybdenum"),  4, EItemRarity::Rare);

	// Tier 5 — Epic (crucible-furnace / deep veins)
	RegisterMetal(TEXT("Titanium"),    5, EItemRarity::Epic);
	RegisterMetal(TEXT("Tungsten"),    5, EItemRarity::Epic);
	RegisterMetal(TEXT("Vanadium"),    5, EItemRarity::Epic);
	RegisterMetal(TEXT("Platinum"),    5, EItemRarity::Epic);
	RegisterMetal(TEXT("Palladium"),   5, EItemRarity::Epic);

	// Tier 6 — Legendary (arcane-forge / deepest veins)
	RegisterMetal(TEXT("Rhodium"),     6, EItemRarity::Legendary);
	RegisterMetal(TEXT("Iridium"),     6, EItemRarity::Legendary);
	RegisterMetal(TEXT("Niobium"),     6, EItemRarity::Legendary);
	RegisterMetal(TEXT("Tantalum"),    6, EItemRarity::Legendary);
	RegisterMetal(TEXT("Osmium"),      6, EItemRarity::Legendary);
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch population — Soils (8 types)
// ─────────────────────────────────────────────────────────────────────────────

void UAoCItemDatabase::PopulateSoils()
{
	RegisterSoil(TEXT("Soil"),
		TEXT("Regular brown dirt. Poor fertility for farming."),
		2.0f, 1);

	RegisterSoil(TEXT("FertileSoil"),
		TEXT("Dark, nutrient-rich earth. Best for farming — quality determines crop quality."),
		2.0f, 3);

	RegisterSoil(TEXT("ForestSoil"),
		TEXT("Woodland floor soil with decomposed leaf litter. +20% yield bonus."),
		2.0f, 5);

	RegisterSoil(TEXT("Clay"),
		TEXT("Dense orange-brown clay. Used for pottery, bricks, and kiln construction."),
		3.0f, 2);

	RegisterSoil(TEXT("Sand"),
		TEXT("Fine tan sand. Used for glass-making and mortar."),
		2.5f, 2);

	RegisterSoil(TEXT("RockChunks"),
		TEXT("Broken grey rock fragments. Used for construction and crude walls."),
		4.0f, 1);

	RegisterSoil(TEXT("SwampSoil"),
		TEXT("Dark, waterlogged muck. Yields fertile soil when terraformed (half quantity)."),
		2.5f, 2);

	RegisterSoil(TEXT("GrassClod"),
		TEXT("A clod of earth with living grass on top. Can regrow grass when placed."),
		2.0f, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch population — Crops (25 harvested produce items)
// ─────────────────────────────────────────────────────────────────────────────

void UAoCItemDatabase::PopulateCrops()
{
	// ── Vegetables ──────────────────────────────────────────────────────────

	RegisterCrop(TEXT("Cabbage"),
		TEXT("A head of fresh cabbage. Used in cooking and as animal feed."),
		2, EItemRarity::Common, 0.5f, 4);

	RegisterCrop(TEXT("Carrot"),
		TEXT("A crisp orange carrot. Used in cooking and to tame horses."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Onion"),
		TEXT("A pungent onion bulb. Essential cooking ingredient."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Peas"),
		TEXT("A handful of green peas. Cooking ingredient and animal feed."),
		1, EItemRarity::Common, 0.2f, 2);

	RegisterCrop(TEXT("Potato"),
		TEXT("A hearty potato tuber. Staple food, used to tame cattle."),
		2, EItemRarity::Common, 0.4f, 4);

	RegisterCrop(TEXT("Turnip"),
		TEXT("A white and purple root vegetable. Used in stews and as animal feed."),
		1, EItemRarity::Common, 0.4f, 3);

	RegisterCrop(TEXT("Garlic"),
		TEXT("A cluster of garlic cloves. Used in cooking and alchemy potions."),
		2, EItemRarity::Uncommon, 0.2f, 6);

	RegisterCrop(TEXT("Leek"),
		TEXT("A tall leek stalk with white base. Used in soups and stews."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Beetroot"),
		TEXT("A dark red root vegetable. Used in cooking and as red dye in alchemy."),
		1, EItemRarity::Common, 0.4f, 3);

	RegisterCrop(TEXT("Parsnip"),
		TEXT("A cream-colored root. Excellent in roasts and soups."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Pumpkin"),
		TEXT("A large orange gourd. Used in pies, soup, and decoration."),
		2, EItemRarity::Common, 2.0f, 8);

	// ── Grains ──────────────────────────────────────────────────────────────

	RegisterCrop(TEXT("Wheat"),
		TEXT("Golden wheat stalks. Milled into flour for cooking, also animal feed."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Barley"),
		TEXT("Barley grain. Essential for brewing beer and ale."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Oat"),
		TEXT("Oat grain. Used in cooking and as horse feed."),
		1, EItemRarity::Common, 0.3f, 3);

	RegisterCrop(TEXT("Rye"),
		TEXT("Dark rye grain. Used for dark bread and brewing."),
		1, EItemRarity::Common, 0.3f, 3);

	// ── Industrial Crops ────────────────────────────────────────────────────

	RegisterCrop(TEXT("Flax"),
		TEXT("Flax stems. Processed into linen cloth and rope."),
		1, EItemRarity::Common, 0.2f, 4);

	RegisterCrop(TEXT("Grapes"),
		TEXT("A cluster of ripe grapes. Used for wine-making."),
		2, EItemRarity::Common, 0.3f, 5);

	RegisterCrop(TEXT("Hemp"),
		TEXT("Tall hemp stalks. Used for rope, cloth, and alchemy."),
		1, EItemRarity::Common, 0.3f, 4);

	RegisterCrop(TEXT("Hops"),
		TEXT("Green hop cones. Essential ingredient for brewing beer."),
		2, EItemRarity::Uncommon, 0.2f, 6);

	RegisterCrop(TEXT("Cotton"),
		TEXT("White fluffy cotton bolls. Used for cotton cloth and bandages."),
		2, EItemRarity::Common, 0.2f, 5);

	RegisterCrop(TEXT("Sunflower"),
		TEXT("A large sunflower head with seeds. Used for cooking oil and alchemy."),
		3, EItemRarity::Uncommon, 0.5f, 10);

	// ── Trees / Fruit ───────────────────────────────────────────────────────

	RegisterCrop(TEXT("Apple"),
		TEXT("A crisp apple. Used in cooking, brewing cider, and as food."),
		2, EItemRarity::Common, 0.2f, 4);

	RegisterCrop(TEXT("Mulberry"),
		TEXT("Mulberry leaves and silkworm cocoons. Used in tailoring for silk."),
		1, EItemRarity::Common, 0.1f, 5);

	// ── Special Crops ───────────────────────────────────────────────────────

	RegisterCrop(TEXT("Rice"),
		TEXT("Husked rice grain. Staple food and ingredient for brewing sake. Requires paddy."),
		3, EItemRarity::Uncommon, 0.3f, 8);

	RegisterCrop(TEXT("Lavender"),
		TEXT("Bundles of fragrant lavender. Used in alchemy potions, oils, and flavoring."),
		2, EItemRarity::Uncommon, 0.1f, 7);
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch population — Seeds (25 seed items, one per crop)
// ─────────────────────────────────────────────────────────────────────────────

void UAoCItemDatabase::PopulateSeeds()
{
	// ── Vegetables ──────────────────────────────────────────────────────────

	RegisterSeed(TEXT("Cabbage"),
		TEXT("Small cabbage seeds. Plant in fertile soil. Requires Farming 60."),
		2, EItemRarity::Common, 2);

	RegisterSeed(TEXT("Carrot"),
		TEXT("Tiny carrot seeds. Plant in fertile soil. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Onion"),
		TEXT("Onion seed sets. Plant in fertile soil. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Peas"),
		TEXT("Dried pea seeds. Plant in fertile soil. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Potato"),
		TEXT("Seed potatoes — small tubers for planting. Requires Farming 60."),
		2, EItemRarity::Common, 2);

	RegisterSeed(TEXT("Turnip"),
		TEXT("Turnip seeds. A hardy root crop. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Garlic"),
		TEXT("Garlic cloves for planting. Requires Farming 60."),
		2, EItemRarity::Uncommon, 3);

	RegisterSeed(TEXT("Leek"),
		TEXT("Leek seeds. Plant in fertile soil. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Beetroot"),
		TEXT("Beetroot seed clusters. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Parsnip"),
		TEXT("Parsnip seeds. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Pumpkin"),
		TEXT("Large pumpkin seeds. Requires Farming 60."),
		2, EItemRarity::Common, 2);

	// ── Grains ──────────────────────────────────────────────────────────────

	RegisterSeed(TEXT("Wheat"),
		TEXT("Wheat grain for sowing. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Barley"),
		TEXT("Barley grain for sowing. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Oat"),
		TEXT("Oat grain for sowing. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Rye"),
		TEXT("Rye grain for sowing. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	// ── Industrial ──────────────────────────────────────────────────────────

	RegisterSeed(TEXT("Flax"),
		TEXT("Flax seeds. Grows into linen-producing stems. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Grapes"),
		TEXT("Grape vine cuttings. Requires Farming 60."),
		2, EItemRarity::Common, 2);

	RegisterSeed(TEXT("Hemp"),
		TEXT("Hemp seeds. Industrial crop for rope and cloth. Requires Farming 30."),
		1, EItemRarity::Common, 1);

	RegisterSeed(TEXT("Hops"),
		TEXT("Hop rhizomes for planting. Essential for brewing. Requires Farming 60."),
		2, EItemRarity::Uncommon, 3);

	RegisterSeed(TEXT("Cotton"),
		TEXT("Cotton seeds. Grows into fluffy bolls. Requires Farming 60."),
		2, EItemRarity::Common, 2);

	RegisterSeed(TEXT("Sunflower"),
		TEXT("Sunflower seeds. Grows tall with oil-rich head. Requires Farming 90."),
		3, EItemRarity::Uncommon, 5);

	// ── Trees / Fruit ───────────────────────────────────────────────────────

	RegisterSeed(TEXT("Apple"),
		TEXT("Apple tree sapling. Takes longest to grow. Requires Farming 60."),
		2, EItemRarity::Common, 2);

	RegisterSeed(TEXT("Mulberry"),
		TEXT("Mulberry tree sapling. Produces silkworm cocoons."),
		1, EItemRarity::Common, 2);

	// ── Special ─────────────────────────────────────────────────────────────

	RegisterSeed(TEXT("Rice"),
		TEXT("Rice seedlings. Requires paddy (waterlogged tile). Requires Farming 90."),
		3, EItemRarity::Uncommon, 4);

	RegisterSeed(TEXT("Lavender"),
		TEXT("Lavender cuttings. Fragrant alchemy crop. Requires Farming 60."),
		2, EItemRarity::Uncommon, 3);
}
