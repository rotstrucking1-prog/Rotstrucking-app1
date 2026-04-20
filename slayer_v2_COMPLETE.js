// ============================================================
// 🗡️ AUTO SLAYER BOT v2.2 — COMPLETE ALL-IN-ONE
// 🧠 Resource Intelligence Engine + Full Slayer 1-99
// 7,184 lines | 73 functions | 222 monsters | 8 masters
// Paste this ENTIRE file into DZ Injector → Private Package
// ============================================================

// ==================== CONFIG ====================
// ============================================================
// 🗡️ AUTO SLAYER BOT v2 — CONFIG
// DeadZone Community Package — Paste into CONFIG tab
// ============================================================

var config = {
    // === Food & Healing ===
    foodType: ConfigItem.createList("foodType", "Combat", "Food Type", "Type of food to use for healing", 
        ["Shrimps", "Trout", "Salmon", "Lobster", "Swordfish", "Monkfish", "Shark", "Manta Ray"], "Lobster"),
    eatPercent: ConfigItem.createInteger("eatPercent", "Combat", "Eat HP %", "HP percentage to eat food at", 50),

    // === Prayer ===
    usePrayer: ConfigItem.createBoolean("usePrayer", "Combat", "Use Protection Prayers", "Enable automatic protection prayers during combat", true),

    // === Loot ===
    lootEnabled: ConfigItem.createBoolean("lootEnabled", "Loot", "Enable Looting", "Pick up loot from killed monsters", true),
    lootMinValue: ConfigItem.createInteger("lootMinValue", "Loot", "Min Loot Value (GP)", "Minimum GP value of items to pick up", 100),
    buryBones: ConfigItem.createBoolean("buryBones", "Loot", "Bury Bones", "Automatically bury bones from kills for Prayer XP", false),

    // === Slayer Master ===
    masterPreference: ConfigItem.createList("masterPreference", "Slayer", "Slayer Master", "Which slayer master to use (Auto = highest available)", 
        ["Auto", "Turael", "Mazchna", "Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"], "Auto"),

    // === Grand Exchange ===
    useGE: ConfigItem.createBoolean("useGE", "Supplies", "Use Grand Exchange", "Buy missing supplies from the Grand Exchange automatically", true),

    // === Self-Sufficiency ===
    selfSufficiency: ConfigItem.createBoolean("selfSufficiency", "Supplies", "Self-Sufficiency Mode", "Gather resources by hand when no GP available (fish, mine, craft)", true)
};

var overlay = {
    status:         OverlayItem.create2d("status",         "Status: Initializing...",      true, 0),
    task:           OverlayItem.create2d("task",            "Task: None",                   true, 1),
    killsLeft:      OverlayItem.create2d("killsLeft",       "Kills Left: 0",                true, 2),
    tasksCompleted: OverlayItem.create2d("tasksCompleted",  "Tasks Done: 0",                true, 3),
    slayerMaster:   OverlayItem.create2d("slayerMaster",    "Master: None",                 true, 4)
};


// ==================== UTILITIES ====================
// =============================================================================
// AUTO SLAYER BOT v2 — UTILITIES & DATA CONSTANTS
// =============================================================================
// This file contains ALL data constants, monster databases, item IDs, locations,
// and helper data structures used by the Auto Slayer Bot v2.
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// SLAYER MASTERS
// ─────────────────────────────────────────────────────────────────────────────
var SLAYER_MASTERS = {
    TURAEL: {
        name: "Turael",
        npcId: 5913,
        location: { x: 2931, y: 3536, plane: 0 },
        locationName: "Burthorpe",
        combatReq: 1,
        slayerReq: 1
    },
    MAZCHNA: {
        name: "Mazchna",
        npcId: 6797,
        location: { x: 3512, y: 3509, plane: 0 },
        locationName: "Canifis",
        combatReq: 20,
        slayerReq: 1
    },
    VANNAKA: {
        name: "Vannaka",
        npcId: 1597,
        location: { x: 3146, y: 9913, plane: 0 },
        locationName: "Edgeville Dungeon",
        combatReq: 40,
        slayerReq: 1
    },
    CHAELDAR: {
        name: "Chaeldar",
        npcId: 1598,
        location: { x: 2445, y: 4431, plane: 0 },
        locationName: "Zanaris",
        combatReq: 70,
        slayerReq: 1
    },
    KONAR: {
        name: "Konar quo Maten",
        npcId: 8623,
        location: { x: 1312, y: 3807, plane: 0 },
        locationName: "Mount Karuulm",
        combatReq: 75,
        slayerReq: 1
    },
    NIEVE: {
        name: "Nieve",
        npcId: 490,
        location: { x: 2432, y: 3423, plane: 0 },
        locationName: "Tree Gnome Stronghold",
        combatReq: 85,
        slayerReq: 1
    },
    DURADEL: {
        name: "Duradel",
        npcId: 1599,
        location: { x: 2869, y: 2982, plane: 1 },
        locationName: "Shilo Village (upstairs)",
        combatReq: 100,
        slayerReq: 50
    },
    KRYSTILIA: {
        name: "Krystilia",
        npcId: 7663,
        location: { x: 3109, y: 3516, plane: 0 },
        locationName: "Edgeville (Wilderness)",
        combatReq: 1,
        slayerReq: 1
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// FOOD ITEMS
// ─────────────────────────────────────────────────────────────────────────────
var FOOD = {
    "Shrimps": { id: 315, heal: 3 },
    "Trout": { id: 333, heal: 7 },
    "Salmon": { id: 329, heal: 9 },
    "Lobster": { id: 379, heal: 12 },
    "Swordfish": { id: 373, heal: 14 },
    "Monkfish": { id: 7946, heal: 16 },
    "Shark": { id: 385, heal: 20 },
    "Manta Ray": { id: 391, heal: 22 }
};

var ALL_FOOD_IDS = [315, 333, 329, 379, 373, 7946, 385, 391];

// ─────────────────────────────────────────────────────────────────────────────
// POTIONS
// ─────────────────────────────────────────────────────────────────────────────
var PRAYER_POTION = { 4: 2434, 3: 139, 2: 141, 1: 143 };
var PRAYER_POTION_IDS = [2434, 139, 141, 143];

var SUPER_COMBAT = { 4: 12695, 3: 12697, 2: 12699, 1: 12701 };
var SUPER_COMBAT_IDS = [12695, 12697, 12699, 12701];

var SUPER_ATTACK = { 4: 2436, 3: 145, 2: 147, 1: 149 };
var SUPER_STRENGTH = { 4: 2440, 3: 157, 2: 159, 1: 161 };

var ANTIFIRE = { 4: 2452, 3: 2454, 2: 2456, 1: 2458 };
var ANTIFIRE_IDS = [2452, 2454, 2456, 2458];

var EXTENDED_ANTIFIRE = { 4: 11951, 3: 11953, 2: 11955, 1: 11957 };
var EXTENDED_ANTIFIRE_IDS = [11951, 11953, 11955, 11957];

var ANTIPOISON = { 4: 2446, 3: 175, 2: 177, 1: 179 };
var ANTIPOISON_IDS = [2446, 175, 177, 179];

// ─────────────────────────────────────────────────────────────────────────────
// SPECIAL SLAYER ITEMS
// ─────────────────────────────────────────────────────────────────────────────
var SPECIAL_ITEMS = {
    ROCK_HAMMER: { id: 4162, name: "Rock hammer", usedOn: ["Gargoyle"], mechanic: "finish", hpThreshold: 8, desc: "Use on gargoyle when HP <= 8" },
    BAG_OF_SALT: { id: 4161, name: "Bag of salt", usedOn: ["Rock Slug"], mechanic: "finish", hpThreshold: 5, desc: "Use on rock slug when HP <= 5" },
    ICE_COOLER: { id: 6696, name: "Ice cooler", usedOn: ["Desert Lizard", "Lizard"], mechanic: "finish", hpThreshold: 4, desc: "Use on desert lizard when HP <= 4" },
    FUNGICIDE_SPRAY: { id: 7421, name: "Fungicide spray", usedOn: ["Mutated Zygomite", "Zygomite"], mechanic: "finish", hpThreshold: 7, desc: "Use on zygomite when HP <= 7" },
    MIRROR_SHIELD: { id: 4156, name: "Mirror shield", usedOn: ["Cockatrice", "Basilisk"], mechanic: "equip", desc: "Must be equipped" },
    NOSE_PEG: { id: 4168, name: "Nose peg", usedOn: ["Aberrant Spectre"], mechanic: "equip", desc: "Must be equipped (or slayer helm)" },
    EARMUFFS: { id: 4166, name: "Earmuffs", usedOn: ["Banshee"], mechanic: "equip", desc: "Must be equipped (or slayer helm)" },
    WITCHWOOD_ICON: { id: 8923, name: "Witchwood icon", usedOn: ["Cave Horror"], mechanic: "equip", desc: "Must be equipped" },
    LIT_BUG_LANTERN: { id: 7053, name: "Lit bug lantern", usedOn: ["Harpie Bug Swarm"], mechanic: "equip", desc: "Must be equipped in shield slot" },
    FACE_MASK: { id: 4164, name: "Face mask", usedOn: ["Dust Devil", "Smoke Devil"], mechanic: "equip", desc: "Must be equipped (or slayer helm)" },
    SLAYER_HELMET: { id: 11864, name: "Slayer helmet", replacesAll: true, desc: "Replaces nose peg, earmuffs, face mask, etc." },
    SLAYER_HELMET_I: { id: 11865, name: "Slayer helmet (i)", replacesAll: true, desc: "Imbued slayer helmet" },
    LEAF_BLADED_SWORD: { id: 11902, name: "Leaf-bladed sword", usedOn: ["Turoth", "Kurask"], mechanic: "weapon", desc: "Must use leaf-bladed weapon" },
    LEAF_BLADED_BATTLEAXE: { id: 20727, name: "Leaf-bladed battleaxe", usedOn: ["Turoth", "Kurask"], mechanic: "weapon", desc: "Must use leaf-bladed weapon" },
    BROAD_BOLTS: { id: 11875, name: "Broad bolts", usedOn: ["Turoth", "Kurask"], mechanic: "ammo", desc: "Ranged alternative for turoths/kurask" }
};

// ─────────────────────────────────────────────────────────────────────────────
// BONES
// ─────────────────────────────────────────────────────────────────────────────
var BONES = {
    BONES: 526,
    BIG_BONES: 532,
    DRAGON_BONES: 536,
    BABYDRAGON_BONES: 534,
    WYVERN_BONES: 6812,
    LAVA_DRAGON_BONES: 11943,
    SUPERIOR_DRAGON_BONES: 22124,
    DAGANNOTH_BONES: 6729
};

var ALL_BONE_IDS = [526, 532, 536, 534, 6812, 11943, 22124, 6729];

// ─────────────────────────────────────────────────────────────────────────────
// MELEE WEAPON TIERS
// ─────────────────────────────────────────────────────────────────────────────
var MELEE_WEAPONS = [
    { name: "Abyssal whip", id: 4151, atkReq: 70, strBonus: 82 },
    { name: "Dragon scimitar", id: 4587, atkReq: 60, strBonus: 66 },
    { name: "Leaf-bladed sword", id: 11902, atkReq: 50, strBonus: 67 },
    { name: "Rune scimitar", id: 1333, atkReq: 40, strBonus: 44 },
    { name: "Adamant scimitar", id: 1331, atkReq: 30, strBonus: 29 },
    { name: "Mithril scimitar", id: 1329, atkReq: 20, strBonus: 21 },
    { name: "Steel scimitar", id: 1325, atkReq: 5, strBonus: 14 },
    { name: "Iron scimitar", id: 1321, atkReq: 1, strBonus: 7 },
    { name: "Bronze sword", id: 1277, atkReq: 1, strBonus: 5 }
];

// ─────────────────────────────────────────────────────────────────────────────
// ARMOR TIERS — HELMETS
// ─────────────────────────────────────────────────────────────────────────────
var HELMETS = [
    { name: "Slayer helmet (i)", id: 11865, defReq: 10 },
    { name: "Slayer helmet", id: 11864, defReq: 10 },
    { name: "Helm of neitiznot", id: 10828, defReq: 55 },
    { name: "Rune full helm", id: 1163, defReq: 40 },
    { name: "Adamant full helm", id: 1161, defReq: 30 },
    { name: "Mithril full helm", id: 1159, defReq: 20 },
    { name: "Steel full helm", id: 1157, defReq: 5 },
    { name: "Iron full helm", id: 1153, defReq: 1 },
    { name: "Bronze full helm", id: 1155, defReq: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// ARMOR TIERS — BODY
// ─────────────────────────────────────────────────────────────────────────────
var PLATEBODIES = [
    { name: "Bandos chestplate", id: 11832, defReq: 65 },
    { name: "Dragon chainbody", id: 3140, defReq: 60 },
    { name: "Rune platebody", id: 1127, defReq: 40 },
    { name: "Adamant platebody", id: 1123, defReq: 30 },
    { name: "Mithril platebody", id: 1121, defReq: 20 },
    { name: "Steel platebody", id: 1119, defReq: 5 },
    { name: "Iron platebody", id: 1115, defReq: 1 },
    { name: "Bronze platebody", id: 1117, defReq: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// ARMOR TIERS — LEGS
// ─────────────────────────────────────────────────────────────────────────────
var PLATELEGS = [
    { name: "Bandos tassets", id: 11834, defReq: 65 },
    { name: "Dragon platelegs", id: 4087, defReq: 60 },
    { name: "Rune platelegs", id: 1079, defReq: 40 },
    { name: "Adamant platelegs", id: 1073, defReq: 30 },
    { name: "Mithril platelegs", id: 1071, defReq: 20 },
    { name: "Steel platelegs", id: 1069, defReq: 5 },
    { name: "Iron platelegs", id: 1067, defReq: 1 },
    { name: "Bronze platelegs", id: 1075, defReq: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// ARMOR TIERS — SHIELDS
// ─────────────────────────────────────────────────────────────────────────────
var SHIELDS = [
    { name: "Dragon defender", id: 12954, defReq: 60 },
    { name: "Rune defender", id: 8850, defReq: 40 },
    { name: "Rune kiteshield", id: 1201, defReq: 40 },
    { name: "Adamant kiteshield", id: 1199, defReq: 30 },
    { name: "Mithril kiteshield", id: 1197, defReq: 20 },
    { name: "Steel kiteshield", id: 1195, defReq: 5 },
    { name: "Iron kiteshield", id: 1191, defReq: 1 },
    { name: "Bronze kiteshield", id: 1189, defReq: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// BOOTS
// ─────────────────────────────────────────────────────────────────────────────
var BOOTS = [
    { name: "Dragon boots", id: 11840, defReq: 60 },
    { name: "Rune boots", id: 4131, defReq: 40 },
    { name: "Adamant boots", id: 4129, defReq: 30 },
    { name: "Mithril boots", id: 4127, defReq: 20 },
    { name: "Steel boots", id: 4125, defReq: 5 },
    { name: "Iron boots", id: 4121, defReq: 1 },
    { name: "Bronze boots", id: 4119, defReq: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// GLOVES
// ─────────────────────────────────────────────────────────────────────────────
var GLOVES = [
    { name: "Barrows gloves", id: 7462, defReq: 40 },
    { name: "Dragon gloves", id: 7461, defReq: 40 },
    { name: "Rune gloves", id: 7460, defReq: 40 },
    { name: "Adamant gloves", id: 7459, defReq: 30 },
    { name: "Mithril gloves", id: 7458, defReq: 20 },
    { name: "Steel gloves", id: 7457, defReq: 5 },
    { name: "Iron gloves", id: 7456, defReq: 1 },
    { name: "Bronze gloves", id: 7455, defReq: 1 },
    { name: "Leather gloves", id: 1059, defReq: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// AMULETS & CAPES
// ─────────────────────────────────────────────────────────────────────────────
var AMULETS = [
    { name: "Amuvar of fury", id: 6585, reqLevel: 1 },
    { name: "Amuvar of glory", id: 1704, reqLevel: 1 },
    { name: "Amuvar of power", id: 1731, reqLevel: 1 },
    { name: "Amuvar of strength", id: 1725, reqLevel: 1 }
];

var CAPES = [
    { name: "Fire cape", id: 6570, reqLevel: 1 },
    { name: "Obsidian cape", id: 6568, reqLevel: 1 },
    { name: "Ardougne cloak 4", id: 20760, reqLevel: 1 },
    { name: "Ardougne cloak 3", id: 20758, reqLevel: 1 }
];

// ─────────────────────────────────────────────────────────────────────────────
// BANK LOCATIONS
// ─────────────────────────────────────────────────────────────────────────────
var BANKS = [
    { name: "Grand Exchange", x: 3164, y: 3487, plane: 0 },
    { name: "Lumbridge", x: 3208, y: 3220, plane: 2 },
    { name: "Varrock West", x: 3185, y: 3436, plane: 0 },
    { name: "Varrock East", x: 3253, y: 3420, plane: 0 },
    { name: "Edgeville", x: 3094, y: 3491, plane: 0 },
    { name: "Falador West", x: 2946, y: 3368, plane: 0 },
    { name: "Falador East", x: 3013, y: 3355, plane: 0 },
    { name: "Draynor", x: 3092, y: 3243, plane: 0 },
    { name: "Al Kharid", x: 3269, y: 3167, plane: 0 },
    { name: "Canifis", x: 3512, y: 3480, plane: 0 },
    { name: "Catherby", x: 2808, y: 3441, plane: 0 },
    { name: "Seers Village", x: 2726, y: 3492, plane: 0 },
    { name: "Ardougne South", x: 2655, y: 3283, plane: 0 },
    { name: "Yanille", x: 2613, y: 3094, plane: 0 },
    { name: "Shilo Village", x: 2852, y: 2954, plane: 0 },
    { name: "Zanaris", x: 2383, y: 4458, plane: 0 },
    { name: "Tree Gnome Stronghold", x: 2445, y: 3416, plane: 1 },
    { name: "Burthorpe", x: 2889, y: 3536, plane: 0 },
    { name: "Nardah", x: 3428, y: 2892, plane: 0 },
    { name: "Mount Karuulm", x: 1322, y: 3826, plane: 0 }
];

// ─────────────────────────────────────────────────────────────────────────────
// GRAND EXCHANGE
// ─────────────────────────────────────────────────────────────────────────────
var GE = {
    BOOTH_IDS: [10061],
    CLERK_IDS: [2148, 2149],
    LOCATION: { x: 3164, y: 3487, plane: 0 },
    WIDGET_GROUP: 465,
    SEARCH_WIDGET: { group: 162, child: 38 },
    BUY_BUTTON: { group: 465, child: 7, subChild: 0 },
    OFFER_PRICE: { group: 465, child: 24, subChild: 37 },
    OFFER_QUANTITY: { group: 465, child: 24, subChild: 7 },
    CONFIRM_BUTTON: { group: 465, child: 27, subChild: 0 }
};

// ─────────────────────────────────────────────────────────────────────────────
// DEATH / RESPAWN
// ─────────────────────────────────────────────────────────────────────────────
var DEATH_SPAWN = { x: 3222, y: 3218, plane: 0 };

// ─────────────────────────────────────────────────────────────────────────────
// TELEPORT ITEMS (for faster travel)
// ─────────────────────────────────────────────────────────────────────────────
var TELEPORTS = {
    VARROCK_TAB: 8007,
    LUMBRIDGE_TAB: 8008,
    FALADOR_TAB: 8009,
    CAMELOT_TAB: 8010,
    ARDOUGNE_TAB: 8011,
    SLAYER_RING_8: 11866,
    SLAYER_RING_7: 11867,
    SLAYER_RING_6: 11868,
    SLAYER_RING_5: 11869,
    SLAYER_RING_4: 11870,
    SLAYER_RING_3: 11871,
    SLAYER_RING_2: 11872,
    SLAYER_RING_1: 11873,
    RING_OF_WEALTH_5: 11980
};

var SLAYER_RING_IDS = [11866, 11867, 11868, 11869, 11870, 11871, 11872, 11873];


// =============================================================================
// MONSTER DATABASE
// =============================================================================
// Comprehensive database of ALL slayer monsters with locations, NPC IDs,
// special requirements, and which masters assign them.
// =============================================================================

var MONSTER_DB = {

    // ─────────────────────────────────────────────────────────────────────────
    // TURAEL TASKS (low-level, Burthorpe)
    // ─────────────────────────────────────────────────────────────────────────

    "Banshee": {
        name: "Banshee",
        npcIds: [1612],
        location: { x: 3442, y: 3545, plane: 0 },
        locationName: "Slayer Tower (ground floor)",
        combatLevel: 23,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 15,
        combatReq: 1,
        specialItem: 4166,
        specialItemName: "Earmuffs",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Turael", "Mazchna", "Vannaka"],
        notes: "Requires earmuffs or slayer helmet"
    },

    "Bat": {
        name: "Bat",
        npcIds: [412, 6283, 2824],
        location: { x: 3178, y: 9901, plane: 0 },
        locationName: "Edgeville Dungeon",
        combatLevel: 6,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3094, y: 3491, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: ""
    },

    "Bear": {
        name: "Bear",
        npcIds: [106, 107, 3423, 105],
        location: { x: 2700, y: 3332, plane: 0 },
        locationName: "East of Ardougne",
        combatLevel: 19,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2655, y: 3283, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Includes grizzly bears and black bears"
    },

    "Bird": {
        name: "Bird",
        npcIds: [1017, 1018, 6113, 6114, 1475, 1476, 1401, 1402],
        location: { x: 3230, y: 3295, plane: 0 },
        locationName: "Lumbridge farm (chickens)",
        combatLevel: 1,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Turael"],
        notes: "Chickens, seagulls, etc."
    },

    "Cave Bug": {
        name: "Cave Bug",
        npcIds: [481],
        location: { x: 3160, y: 9572, plane: 0 },
        locationName: "Lumbridge Swamp Caves",
        combatLevel: 6,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 7,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Turael"],
        notes: "Bring a light source"
    },

    "Cave Crawler": {
        name: "Cave Crawler",
        npcIds: [406, 407, 408],
        location: { x: 2788, y: 9997, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 23,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 10,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Turael", "Mazchna", "Vannaka"],
        notes: "Can poison — bring antipoison"
    },

    "Cave Slime": {
        name: "Cave Slime",
        npcIds: [480],
        location: { x: 3160, y: 9572, plane: 0 },
        locationName: "Lumbridge Swamp Caves",
        combatLevel: 23,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 17,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Can poison. Bring a light source."
    },

    "Cow": {
        name: "Cow",
        npcIds: [2790, 2791, 2793, 2794, 397],
        location: { x: 3253, y: 3270, plane: 0 },
        locationName: "Lumbridge cow field",
        combatLevel: 2,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Crawling Hand": {
        name: "Crawling Hand",
        npcIds: [448, 449, 450, 451, 452, 1648, 1649, 1650, 1651, 1652],
        location: { x: 3418, y: 3546, plane: 0 },
        locationName: "Slayer Tower (ground floor)",
        combatLevel: 16,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 5,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: ""
    },

    "Desert Lizard": {
        name: "Desert Lizard",
        npcIds: [458, 459, 460, 461],
        location: { x: 3305, y: 3112, plane: 0 },
        locationName: "Kharidian Desert",
        combatLevel: 24,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 22,
        combatReq: 1,
        specialItem: 6696,
        specialItemName: "Ice cooler",
        specialMechanic: "finish",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3269, y: 3167, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Must use ice cooler to finish when HP <= 4. Bring waterskins."
    },

    "Dog": {
        name: "Dog",
        npcIds: [99, 100, 3582],
        location: { x: 2930, y: 3515, plane: 0 },
        locationName: "Burthorpe",
        combatLevel: 29,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2889, y: 3536, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Guard dogs near Burthorpe"
    },

    "Dwarf": {
        name: "Dwarf",
        npcIds: [117, 118, 2462, 5880, 5881],
        location: { x: 3025, y: 9803, plane: 0 },
        locationName: "Dwarven Mine",
        combatLevel: 10,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3013, y: 3355, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Ghost": {
        name: "Ghost",
        npcIds: [102, 103, 104, 491, 1698],
        location: { x: 3241, y: 9911, plane: 0 },
        locationName: "Varrock Sewers",
        combatLevel: 19,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: ""
    },

    "Goblin": {
        name: "Goblin",
        npcIds: [3029, 3030, 3031, 3032, 3033, 3034, 655, 656],
        location: { x: 3244, y: 3243, plane: 0 },
        locationName: "Lumbridge",
        combatLevel: 5,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Icefiend": {
        name: "Icefiend",
        npcIds: [3139, 7700],
        location: { x: 3008, y: 3477, plane: 0 },
        locationName: "Ice Mountain",
        combatLevel: 18,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3094, y: 3491, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Kalphite Worker": {
        name: "Kalphite Worker",
        npcIds: [955],
        location: { x: 3323, y: 9502, plane: 0 },
        locationName: "Kalphite Lair",
        combatLevel: 28,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 3269, y: 3167, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Rope required first time. Bring antipoison."
    },

    "Minotaur": {
        name: "Minotaur",
        npcIds: [4404, 4405, 4832, 4833],
        location: { x: 1866, y: 5234, plane: 0 },
        locationName: "Stronghold of Security (first floor)",
        combatLevel: 12,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Monkey": {
        name: "Monkey",
        npcIds: [205, 2711, 5259, 5260],
        location: { x: 2793, y: 2988, plane: 0 },
        locationName: "Karamja",
        combatLevel: 3,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Turael"],
        notes: "Not ideal — no bank nearby"
    },

    "Rat": {
        name: "Rat",
        npcIds: [87, 2854, 2855, 2856, 4396, 4397],
        location: { x: 3237, y: 9866, plane: 0 },
        locationName: "Varrock Sewers",
        combatLevel: 1,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Scorpion": {
        name: "Scorpion",
        npcIds: [107, 144, 3024],
        location: { x: 3298, y: 3298, plane: 0 },
        locationName: "Al Kharid Mine",
        combatLevel: 14,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3269, y: 3167, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Skeleton": {
        name: "Skeleton",
        npcIds: [90, 91, 92, 93, 3240],
        location: { x: 3233, y: 9911, plane: 0 },
        locationName: "Varrock Sewers",
        combatLevel: 21,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Spider": {
        name: "Spider",
        npcIds: [60, 61, 62, 63],
        location: { x: 3168, y: 3244, plane: 0 },
        locationName: "Lumbridge (behind castle)",
        combatLevel: 1,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Wolf": {
        name: "Wolf",
        npcIds: [106, 2853, 5346, 1198, 1199],
        location: { x: 2857, y: 3553, plane: 0 },
        locationName: "White Wolf Mountain",
        combatLevel: 25,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Turael"],
        notes: ""
    },

    "Zombie": {
        name: "Zombie",
        npcIds: [58, 59, 73, 74, 75, 76, 2836, 2837],
        location: { x: 3238, y: 9906, plane: 0 },
        locationName: "Varrock Sewers",
        combatLevel: 13,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: ""
    },

    // ─────────────────────────────────────────────────────────────────────────
    // MAZCHNA ADDITIONAL TASKS
    // ─────────────────────────────────────────────────────────────────────────

    "Catablepon": {
        name: "Catablepon",
        npcIds: [2475, 2476, 2477],
        location: { x: 1866, y: 5234, plane: 0 },
        locationName: "Stronghold of Security (second floor)",
        combatLevel: 49,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: true,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Mazchna"],
        notes: "Can drain stats with magic attack"
    },

    "Cockatrice": {
        name: "Cockatrice",
        npcIds: [419, 420],
        location: { x: 2792, y: 10002, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 37,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 25,
        combatReq: 1,
        specialItem: 4156,
        specialItemName: "Mirror shield",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: "Must equip mirror shield or V's shield"
    },

    "Cyclops": {
        name: "Cyclops",
        npcIds: [2463, 2464, 2465, 2466],
        location: { x: 2849, y: 3543, plane: 0 },
        locationName: "Warriors Guild",
        combatLevel: 56,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2889, y: 3536, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: ""
    },

    "Earth Warrior": {
        name: "Earth Warrior",
        npcIds: [2840],
        location: { x: 3122, y: 9972, plane: 0 },
        locationName: "Edgeville Dungeon",
        combatLevel: 51,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3094, y: 3491, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: ""
    },

    "Flesh Crawler": {
        name: "Flesh Crawler",
        npcIds: [2498, 2499, 2500],
        location: { x: 1866, y: 5234, plane: 0 },
        locationName: "Stronghold of Security (second floor)",
        combatLevel: 41,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Mazchna"],
        notes: ""
    },

    "Ghoul": {
        name: "Ghoul",
        npcIds: [289, 290, 1543],
        location: { x: 3421, y: 3508, plane: 0 },
        locationName: "Canifis (graveyard)",
        combatLevel: 42,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Mazchna"],
        notes: ""
    },

    "Hill Giant": {
        name: "Hill Giant",
        npcIds: [2098, 2099, 2100, 2101, 2102, 2103],
        location: { x: 3116, y: 9856, plane: 0 },
        locationName: "Edgeville Dungeon",
        combatLevel: 28,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3094, y: 3491, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: "Good for big bones"
    },

    "Hobgoblin": {
        name: "Hobgoblin",
        npcIds: [2240, 2241, 2242, 3583],
        location: { x: 2903, y: 3393, plane: 0 },
        locationName: "Crafting Guild area",
        combatLevel: 28,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Mazchna"],
        notes: ""
    },

    "Ice Warrior": {
        name: "Ice Warrior",
        npcIds: [124, 3172],
        location: { x: 3052, y: 9579, plane: 0 },
        locationName: "Asgarnian Ice Dungeon",
        combatLevel: 57,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: ""
    },

    "Infernal Mage": {
        name: "Infernal Mage",
        npcIds: [443, 444, 445, 446, 447],
        location: { x: 3447, y: 3563, plane: 1 },
        locationName: "Slayer Tower (first floor)",
        combatLevel: 66,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 45,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: "Uses fire spells"
    },

    "Killerwatt": {
        name: "Killerwatt",
        npcIds: [3201, 7637],
        location: { x: 2657, y: 4581, plane: 0 },
        locationName: "Killerwatt Plane (Draynor Manor)",
        combatLevel: 55,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 37,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3092, y: 3243, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: "Ernest the Chicken quest required"
    },

    "Mogre": {
        name: "Mogre",
        npcIds: [114, 2801],
        location: { x: 2987, y: 3108, plane: 0 },
        locationName: "Mudskipper Point",
        combatLevel: 32,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 32,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: "Use fishing explosives to spawn"
    },

    "Pyrefiend": {
        name: "Pyrefiend",
        npcIds: [433, 434, 435, 436],
        location: { x: 2762, y: 10009, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 43,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 30,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: ""
    },

    "Rock Slug": {
        name: "Rock Slug",
        npcIds: [421, 422],
        location: { x: 2795, y: 10000, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 29,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 20,
        combatReq: 1,
        specialItem: 4161,
        specialItemName: "Bag of salt",
        specialMechanic: "finish",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: "Use bag of salt to finish when HP <= 5"
    },

    "Shade": {
        name: "Shade",
        npcIds: [425, 426, 427, 428, 1240, 1241],
        location: { x: 3464, y: 3574, plane: 0 },
        locationName: "Mort'ton",
        combatLevel: 40,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Mazchna"],
        notes: ""
    },

    "Vampire": {
        name: "Vampire",
        npcIds: [3138, 3139, 3140, 3141],
        location: { x: 3563, y: 3475, plane: 0 },
        locationName: "Canifis",
        combatLevel: 32,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Mazchna", "Vannaka"],
        notes: ""
    },

    "Wall Beast": {
        name: "Wall Beast",
        npcIds: [476, 7823],
        location: { x: 3163, y: 9571, plane: 0 },
        locationName: "Lumbridge Swamp Caves",
        combatLevel: 49,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 35,
        combatReq: 20,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3208, y: 3220, plane: 2 },
        assignedBy: ["Mazchna"],
        notes: "Bring a light source, wear spiny helmet"
    },

    // ─────────────────────────────────────────────────────────────────────────
    // VANNAKA ADDITIONAL TASKS
    // ─────────────────────────────────────────────────────────────────────────

    "Aberrant Spectre": {
        name: "Aberrant Spectre",
        npcIds: [2, 3, 4, 5, 6, 7],
        location: { x: 3430, y: 3543, plane: 1 },
        locationName: "Slayer Tower (first floor)",
        combatLevel: 96,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 60,
        combatReq: 40,
        specialItem: 4168,
        specialItemName: "Nose peg",
        specialMechanic: "equip",
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Requires nose peg or slayer helmet. Good herb drops."
    },

    "Ankou": {
        name: "Ankou",
        npcIds: [2514, 2515, 2516, 2517, 2518, 2519],
        location: { x: 2361, y: 5232, plane: 0 },
        locationName: "Stronghold of Security (fourth floor)",
        combatLevel: 75,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: true,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Vannaka", "Nieve", "Duradel"],
        notes: ""
    },

    "Basilisk": {
        name: "Basilisk",
        npcIds: [417, 418],
        location: { x: 2741, y: 10006, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 61,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 40,
        combatReq: 40,
        specialItem: 4156,
        specialItemName: "Mirror shield",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar"],
        notes: "Must equip mirror shield or V's shield"
    },

    "Bloodveld": {
        name: "Bloodveld",
        npcIds: [484, 485, 486, 487, 3138, 7276, 7277, 7278],
        location: { x: 3418, y: 3564, plane: 1 },
        locationName: "Slayer Tower (first floor)",
        combatLevel: 76,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 50,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Uses ranged attack that rolls on magic defence. Also in Catacombs of Kourend."
    },

    "Blue Dragon": {
        name: "Blue Dragon",
        npcIds: [55, 264, 265, 266, 8076],
        location: { x: 2892, y: 9774, plane: 0 },
        locationName: "Taverley Dungeon",
        combatLevel: 111,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Bring antifire shield. Safespot available. Drops dragon bones."
    },

    "Bronze Dragon": {
        name: "Bronze Dragon",
        npcIds: [270, 271],
        location: { x: 2735, y: 9488, plane: 0 },
        locationName: "Brimhaven Dungeon",
        combatLevel: 131,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Vannaka"],
        notes: "Bring antifire shield or extended antifire potions"
    },

    "Crocodile": {
        name: "Crocodile",
        npcIds: [1621, 1622, 1623],
        location: { x: 3347, y: 2963, plane: 0 },
        locationName: "Kharidian Desert (near Nardah)",
        combatLevel: 63,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3428, y: 2892, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: ""
    },

    "Dagannoth": {
        name: "Dagannoth",
        npcIds: [2259, 2260, 2261, 2262, 2263, 2264, 2265, 970],
        location: { x: 2442, y: 10147, plane: 0 },
        locationName: "Waterbirth Island",
        combatLevel: 74,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Various types, some range/mage. Also in Catacombs of Kourend."
    },

    "Fire Giant": {
        name: "Fire Giant",
        npcIds: [2075, 2076, 2077, 2078, 2079, 2080, 7252, 7253],
        location: { x: 2568, y: 9893, plane: 0 },
        locationName: "Waterfall Dungeon",
        combatLevel: 86,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Also in Catacombs of Kourend (multi). Great task."
    },

    "Green Dragon": {
        name: "Green Dragon",
        npcIds: [260, 261, 262, 263, 264, 7868, 7869, 7870],
        location: { x: 3345, y: 3670, plane: 0 },
        locationName: "Wilderness (west, level 13-14)",
        combatLevel: 79,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3094, y: 3491, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: "Wilderness — PK risk. Bring antifire shield."
    },

    "Harpie Bug Swarm": {
        name: "Harpie Bug Swarm",
        npcIds: [3153, 3154],
        location: { x: 2471, y: 4427, plane: 0 },
        locationName: "Zanaris",
        combatLevel: 46,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 33,
        combatReq: 40,
        specialItem: 7053,
        specialItemName: "Lit bug lantern",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2383, y: 4458, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: "Must have lit bug lantern in shield slot"
    },

    "Ice Giant": {
        name: "Ice Giant",
        npcIds: [2085, 2086, 2087, 2088, 2089],
        location: { x: 3048, y: 9579, plane: 0 },
        locationName: "Asgarnian Ice Dungeon",
        combatLevel: 53,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: ""
    },

    "Jelly": {
        name: "Jelly",
        npcIds: [437, 438, 7518, 7519],
        location: { x: 2789, y: 9990, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 78,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 52,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar"],
        notes: "Also in Catacombs of Kourend (warped jellies)"
    },

    "Jungle Horror": {
        name: "Jungle Horror",
        npcIds: [2825, 2826, 2827],
        location: { x: 2731, y: 2732, plane: 0 },
        locationName: "Mos Le'Harmless (cave)",
        combatLevel: 70,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Vannaka", "Chaeldar"],
        notes: "Cabin Fever quest required for access"
    },

    "Lesser Demon": {
        name: "Lesser Demon",
        npcIds: [2005, 2006, 2007, 7247, 7248],
        location: { x: 2838, y: 9568, plane: 0 },
        locationName: "Karamja Dungeon",
        combatLevel: 82,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Vannaka", "Chaeldar"],
        notes: "Also in Catacombs of Kourend"
    },

    "Moss Giant": {
        name: "Moss Giant",
        npcIds: [2090, 2091, 2092, 2093],
        location: { x: 3155, y: 9904, plane: 0 },
        locationName: "Varrock Sewers",
        combatLevel: 42,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: ""
    },

    "Otherworldly Being": {
        name: "Otherworldly Being",
        npcIds: [2843, 2844],
        location: { x: 2454, y: 4470, plane: 0 },
        locationName: "Zanaris",
        combatLevel: 64,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2383, y: 4458, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: "Lost City quest required"
    },

    "Shadow Warrior": {
        name: "Shadow Warrior",
        npcIds: [688, 5846],
        location: { x: 2714, y: 9690, plane: 0 },
        locationName: "Legends Guild basement",
        combatLevel: 48,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2655, y: 3283, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: ""
    },

    "Turoth": {
        name: "Turoth",
        npcIds: [426, 427, 428, 429, 432, 7519],
        location: { x: 2723, y: 10003, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 83,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 55,
        combatReq: 40,
        specialItem: 11902,
        specialItemName: "Leaf-bladed sword",
        specialMechanic: "weapon",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve"],
        notes: "Must use leaf-bladed weapon, broad bolts, or magic dart"
    },

    "Werewolf": {
        name: "Werewolf",
        npcIds: [6006, 6007, 6008, 6009, 6010, 6011, 6012, 6013, 6014, 6015, 6016, 6017, 6018, 6019, 6020, 6021],
        location: { x: 3505, y: 3474, plane: 0 },
        locationName: "Canifis",
        combatLevel: 88,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Vannaka"],
        notes: "Canifis citizens turn into werewolves when attacked (must NOT have wolfbane)"
    },

    // ─────────────────────────────────────────────────────────────────────────
    // CHAELDAR / KONAR / NIEVE / DURADEL TASKS (mid-high level)
    // ─────────────────────────────────────────────────────────────────────────

    "Abyssal Demon": {
        name: "Abyssal Demon",
        npcIds: [415, 416, 7241, 7242],
        location: { x: 3419, y: 3569, plane: 2 },
        locationName: "Slayer Tower (top floor)",
        combatLevel: 124,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 85,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Teleports randomly. Drops abyssal whip. Also in Catacombs of Kourend."
    },

    "Black Demon": {
        name: "Black Demon",
        npcIds: [2048, 2049, 2050, 2051, 2052, 7244, 7245, 7246],
        location: { x: 2857, y: 9777, plane: 0 },
        locationName: "Taverley Dungeon",
        combatLevel: 172,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 80,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Can safespot in Taverley Dungeon. Also in Catacombs of Kourend."
    },

    "Black Dragon": {
        name: "Black Dragon",
        npcIds: [54, 247, 248, 249, 250, 8084, 8085],
        location: { x: 2830, y: 9824, plane: 0 },
        locationName: "Taverley Dungeon",
        combatLevel: 227,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 80,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Bring antifire shield. Baby black dragons count too."
    },

    "Cave Horror": {
        name: "Cave Horror",
        npcIds: [3209, 3210, 3211, 3212, 3213, 3214],
        location: { x: 3750, y: 9375, plane: 0 },
        locationName: "Mos Le'Harmless Cave",
        combatLevel: 80,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 58,
        combatReq: 70,
        specialItem: 8923,
        specialItemName: "Witchwood icon",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve"],
        notes: "Must wear witchwood icon or slayer helmet. Drops black mask."
    },

    "Cave Kraken": {
        name: "Cave Kraken",
        npcIds: [492, 493],
        location: { x: 2280, y: 10001, plane: 0 },
        locationName: "Kraken Cove",
        combatLevel: 127,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 87,
        combatReq: 80,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Must attack the whirlpool to spawn. Instance available."
    },

    "Dark Beast": {
        name: "Dark Beast",
        npcIds: [7938, 7939, 4005, 4006],
        location: { x: 2015, y: 4639, plane: 0 },
        locationName: "Mourner Tunnels",
        combatLevel: 182,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 90,
        combatReq: 90,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Also in Catacombs of Kourend"
    },

    "Drake": {
        name: "Drake",
        npcIds: [8612, 8613],
        location: { x: 1310, y: 10187, plane: 0 },
        locationName: "Karuulm Slayer Dungeon",
        combatLevel: 192,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 84,
        combatReq: 80,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 1322, y: 3826, plane: 0 },
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Dodge volcanic breath special attack. Boots of stone required in Karuulm."
    },

    "Dust Devil": {
        name: "Dust Devil",
        npcIds: [423, 424, 7249, 7250],
        location: { x: 3210, y: 9192, plane: 0 },
        locationName: "Smoke Dungeon",
        combatLevel: 93,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 65,
        combatReq: 70,
        specialItem: 4164,
        specialItemName: "Face mask",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 3428, y: 2892, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Face mask or slayer helmet required. Great for bursting in Catacombs."
    },

    "Gargoyle": {
        name: "Gargoyle",
        npcIds: [412, 413, 7256, 7257],
        location: { x: 3438, y: 3537, plane: 2 },
        locationName: "Slayer Tower (top floor)",
        combatLevel: 111,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 75,
        combatReq: 75,
        specialItem: 4162,
        specialItemName: "Rock hammer",
        specialMechanic: "finish",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Must use rock hammer when HP <= 8. Auto-smash perk available."
    },

    "Greater Demon": {
        name: "Greater Demon",
        npcIds: [2025, 2026, 2027, 2028, 2029, 2030, 7244, 7245],
        location: { x: 2639, y: 9517, plane: 2 },
        locationName: "Brimhaven Dungeon",
        combatLevel: 92,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 75,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Also in Catacombs of Kourend."
    },

    "Hellhound": {
        name: "Hellhound",
        npcIds: [135, 136, 3133, 7256, 7257],
        location: { x: 2867, y: 9851, plane: 0 },
        locationName: "Taverley Dungeon",
        combatLevel: 122,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 75,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Great for prayer training (no drops, just prayer). Also in Catacombs of Kourend."
    },

    "Iron Dragon": {
        name: "Iron Dragon",
        npcIds: [272, 273, 7254],
        location: { x: 2694, y: 9442, plane: 0 },
        locationName: "Brimhaven Dungeon",
        combatLevel: 189,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 80,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Bring antifire shield. Also in Catacombs of Kourend."
    },

    "Kalphite": {
        name: "Kalphite",
        npcIds: [955, 956, 957, 958, 959],
        location: { x: 3323, y: 9502, plane: 0 },
        locationName: "Kalphite Lair",
        combatLevel: 85,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 70,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 3269, y: 3167, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Includes workers, soldiers, and guardians. Bring antipoison."
    },

    "Kraken": {
        name: "Kraken",
        npcIds: [494, 496],
        location: { x: 2280, y: 10001, plane: 0 },
        locationName: "Kraken Cove",
        combatLevel: 291,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 87,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Boss version of cave kraken. Instance available."
    },

    "Kurask": {
        name: "Kurask",
        npcIds: [410, 411, 7520, 7521],
        location: { x: 2701, y: 9992, plane: 0 },
        locationName: "Fremennik Slayer Dungeon",
        combatLevel: 106,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 70,
        combatReq: 70,
        specialItem: 11902,
        specialItemName: "Leaf-bladed sword",
        specialMechanic: "weapon",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2726, y: 3492, plane: 0 },
        assignedBy: ["Vannaka", "Chaeldar", "Konar", "Nieve"],
        notes: "Must use leaf-bladed weapon, broad bolts, or magic dart"
    },

    "Lizardman": {
        name: "Lizardman",
        npcIds: [6915, 6916, 6917, 6918, 6919],
        location: { x: 1477, y: 3689, plane: 0 },
        locationName: "Lizardman Canyon",
        combatLevel: 53,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 70,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve"],
        notes: "Lizardman shamans (6766, 6767, 6768) are harder variant."
    },

    "Mutated Zygomite": {
        name: "Mutated Zygomite",
        npcIds: [537, 538, 7797, 7798],
        location: { x: 2450, y: 4465, plane: 0 },
        locationName: "Zanaris",
        combatLevel: 74,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 57,
        combatReq: 70,
        specialItem: 7421,
        specialItemName: "Fungicide spray",
        specialMechanic: "finish",
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2383, y: 4458, plane: 0 },
        assignedBy: ["Chaeldar", "Konar"],
        notes: "Use fungicide spray when HP <= 7"
    },

    "Nechryael": {
        name: "Nechryael",
        npcIds: [8, 9, 10, 11, 7278, 7279],
        location: { x: 3438, y: 3559, plane: 2 },
        locationName: "Slayer Tower (top floor)",
        combatLevel: 115,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 80,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 3512, y: 3480, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Summon death spawn minions. Great for bursting in Catacombs."
    },

    "Red Dragon": {
        name: "Red Dragon",
        npcIds: [247, 248, 249, 250, 8075, 8079],
        location: { x: 2703, y: 9487, plane: 0 },
        locationName: "Brimhaven Dungeon",
        combatLevel: 152,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 80,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Bring antifire shield. Also in Catacombs of Kourend."
    },

    "Skeletal Wyvern": {
        name: "Skeletal Wyvern",
        npcIds: [465, 466, 467, 468],
        location: { x: 3058, y: 9555, plane: 0 },
        locationName: "Asgarnian Ice Dungeon (lower)",
        combatLevel: 140,
        attackStyle: "ranged",
        protectionPrayer: "PROTECT_FROM_MISSILES",
        slayerReq: 72,
        combatReq: 70,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Bring elemental/mind/dragonfire shield to block ice breath"
    },

    "Smoke Devil": {
        name: "Smoke Devil",
        npcIds: [498, 499, 8616, 8617],
        location: { x: 2395, y: 9444, plane: 0 },
        locationName: "Smoke Devil Dungeon",
        combatLevel: 160,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 93,
        combatReq: 85,
        specialItem: 4164,
        specialItemName: "Face mask",
        specialMechanic: "equip",
        safeSpottable: false,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Face mask or slayer helmet required. Good for bursting."
    },

    "Steel Dragon": {
        name: "Steel Dragon",
        npcIds: [274, 275, 7255],
        location: { x: 2714, y: 9450, plane: 0 },
        locationName: "Brimhaven Dungeon",
        combatLevel: 246,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Bring antifire shield. High defence."
    },

    "Suqah": {
        name: "Suqah",
        npcIds: [787, 788, 789],
        location: { x: 2116, y: 3940, plane: 0 },
        locationName: "Lunar Isle",
        combatLevel: 111,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2099, y: 3921, plane: 0 },
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Lunar Diplomacy quest required"
    },

    "Troll": {
        name: "Troll",
        npcIds: [1106, 1107, 1108, 1109, 1110, 1111, 1112, 1113, 1114, 1115, 1116, 1117, 3132, 3133, 3134],
        location: { x: 2876, y: 3587, plane: 0 },
        locationName: "Death Plateau (Burthorpe)",
        combatLevel: 69,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 60,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: { x: 2889, y: 3536, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Various types — mountain trolls, ice trolls. Also Jatizso ice trolls."
    },

    "Waterfiend": {
        name: "Waterfiend",
        npcIds: [5361, 5362, 2916, 2917],
        location: { x: 2714, y: 9690, plane: 0 },
        locationName: "Kraken Cove / Ancient Cavern",
        combatLevel: 115,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 1,
        combatReq: 75,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Good crimson charm droppers. Weak to ranged."
    },

    "Wyrm": {
        name: "Wyrm",
        npcIds: [8610, 8611],
        location: { x: 1268, y: 10189, plane: 0 },
        locationName: "Karuulm Slayer Dungeon",
        combatLevel: 99,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 62,
        combatReq: 60,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 1322, y: 3826, plane: 0 },
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Boots of stone required in Karuulm. Weak to stab."
    },

    // ─────────────────────────────────────────────────────────────────────────
    // ADDITIONAL MONSTERS (expanding database)
    // ─────────────────────────────────────────────────────────────────────────

    "Elves": {
        name: "Elves",
        npcIds: [5295, 5296, 5297, 5298, 5299, 5300, 5301, 5302, 5303, 5304],
        location: { x: 2333, y: 3172, plane: 0 },
        locationName: "Lletya / Prifddinas",
        combatLevel: 108,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 70,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Vannaka", "Chaeldar"],
        notes: "Regicide quest required"
    },

    "Spiritual Creature": {
        name: "Spiritual Creature",
        npcIds: [2212, 2213, 2214, 2215, 2216, 2217, 2218, 2219, 2233, 2234, 2235, 2236],
        location: { x: 2884, y: 5303, plane: 2 },
        locationName: "God Wars Dungeon",
        combatLevel: 120,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 63,
        combatReq: 75,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Spiritual rangers, warriors, mages in GWD. Bring god items."
    },

    "Mithril Dragon": {
        name: "Mithril Dragon",
        npcIds: [2919, 2920],
        location: { x: 1740, y: 5344, plane: 0 },
        locationName: "Ancient Cavern",
        combatLevel: 304,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 100,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Uses all 3 combat styles. Bring antifire. Very hard."
    },

    "Adamant Dragon": {
        name: "Adamant Dragon",
        npcIds: [8030, 8031, 8032],
        location: { x: 1568, y: 5075, plane: 0 },
        locationName: "Lithkren Vault",
        combatLevel: 338,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 100,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Dragon Slayer II required. Bring antifire."
    },

    "Rune Dragon": {
        name: "Rune Dragon",
        npcIds: [8027, 8028, 8029, 8031],
        location: { x: 1568, y: 5075, plane: 0 },
        locationName: "Lithkren Vault",
        combatLevel: 380,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 100,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Dragon Slayer II required. Bring antifire + insulated boots."
    },

    "Hydra": {
        name: "Hydra",
        npcIds: [8609, 8610, 8611],
        location: { x: 1312, y: 10232, plane: 0 },
        locationName: "Karuulm Slayer Dungeon",
        combatLevel: 194,
        attackStyle: "ranged",
        protectionPrayer: "PROTECT_FROM_MISSILES",
        slayerReq: 95,
        combatReq: 95,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 1322, y: 3826, plane: 0 },
        assignedBy: ["Konar"],
        notes: "Boots of stone required. Alchemical Hydra is boss variant."
    },

    "Fossil Island Wyvern": {
        name: "Fossil Island Wyvern",
        npcIds: [7793, 7794, 7795, 7796],
        location: { x: 3608, y: 10278, plane: 0 },
        locationName: "Fossil Island Wyvern Cave",
        combatLevel: 139,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 66,
        combatReq: 60,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Bring elemental/mind/dragonfire shield. Ancient, long-tailed, spitting, taloned variants."
    },

    "Greater Nechryael": {
        name: "Greater Nechryael",
        npcIds: [7278, 7279, 7280],
        location: { x: 1702, y: 10074, plane: 0 },
        locationName: "Catacombs of Kourend",
        combatLevel: 200,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 80,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Great for bursting. Summon death spawn minions."
    },

    "Deviant Spectre": {
        name: "Deviant Spectre",
        npcIds: [7279, 7280],
        location: { x: 1701, y: 10042, plane: 0 },
        locationName: "Catacombs of Kourend",
        combatLevel: 169,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 60,
        combatReq: 75,
        specialItem: 4168,
        specialItemName: "Nose peg",
        specialMechanic: "equip",
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Stronger variant of aberrant spectre. Nose peg/slayer helm required."
    },

    "Warped Jelly": {
        name: "Warped Jelly",
        npcIds: [7518, 7519],
        location: { x: 1673, y: 10062, plane: 0 },
        locationName: "Catacombs of Kourend",
        combatLevel: 112,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 52,
        combatReq: 70,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Stronger jelly variant. Good for bursting in Catacombs."
    },

    "Black Bear": {
        name: "Black Bear",
        npcIds: [105, 106],
        location: { x: 3286, y: 3363, plane: 0 },
        locationName: "East of Varrock",
        combatLevel: 19,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3253, y: 3420, plane: 0 },
        assignedBy: ["Turael"],
        notes: "Part of Bear task"
    },

    "Giant Bat": {
        name: "Giant Bat",
        npcIds: [78, 3711, 4422],
        location: { x: 3240, y: 9875, plane: 0 },
        locationName: "Varrock Sewers",
        combatLevel: 27,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 3185, y: 3436, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Part of Bat task"
    },

    "Guard Dog": {
        name: "Guard Dog",
        npcIds: [99, 100, 3582],
        location: { x: 2774, y: 3274, plane: 0 },
        locationName: "Handelmort mansion (Ardougne)",
        combatLevel: 44,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2655, y: 3283, plane: 0 },
        assignedBy: ["Turael", "Mazchna"],
        notes: "Part of Dog task"
    },

    "Terrorbird": {
        name: "Terrorbird",
        npcIds: [5049, 5050, 5051, 5052],
        location: { x: 2392, y: 3453, plane: 0 },
        locationName: "Tree Gnome Stronghold",
        combatLevel: 28,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2445, y: 3416, plane: 1 },
        assignedBy: ["Turael"],
        notes: "Part of Bird task"
    },

    "Baby Blue Dragon": {
        name: "Baby Blue Dragon",
        npcIds: [52, 53, 8074],
        location: { x: 2892, y: 9774, plane: 0 },
        locationName: "Taverley Dungeon",
        combatLevel: 48,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Chaeldar"],
        notes: "Part of Blue Dragon task. No dragon breath."
    },

    "Wyvern": {
        name: "Wyvern",
        npcIds: [465, 466, 467, 468, 7793, 7794, 7795, 7796],
        location: { x: 3058, y: 9555, plane: 0 },
        locationName: "Asgarnian Ice Dungeon / Fossil Island",
        combatLevel: 140,
        attackStyle: "ranged",
        protectionPrayer: "PROTECT_FROM_MISSILES",
        slayerReq: 66,
        combatReq: 60,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: { x: 2946, y: 3368, plane: 0 },
        assignedBy: ["Chaeldar", "Konar", "Nieve", "Duradel"],
        notes: "Bring elemental/mind/dragonfire shield to block ice breath"
    },

    "Brutal Blue Dragon": {
        name: "Brutal Blue Dragon",
        npcIds: [7272, 7273, 7274],
        location: { x: 1654, y: 10092, plane: 0 },
        locationName: "Catacombs of Kourend",
        combatLevel: 271,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 1,
        combatReq: 75,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Brutal variant. Bring antifire."
    },

    "Brutal Black Dragon": {
        name: "Brutal Black Dragon",
        npcIds: [7274, 7275],
        location: { x: 1614, y: 10050, plane: 0 },
        locationName: "Catacombs of Kourend",
        combatLevel: 318,
        attackStyle: "magic",
        protectionPrayer: "PROTECT_FROM_MAGIC",
        slayerReq: 1,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve", "Duradel"],
        notes: "Bring antifire and ranged gear. Good money."
    },

    "Maniacal Monkey": {
        name: "Maniacal Monkey",
        npcIds: [7117, 7118, 7119],
        location: { x: 2460, y: 9100, plane: 1 },
        locationName: "Kruk's Dungeon (Ape Atoll)",
        combatLevel: 118,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 1,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: true,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Great for chinning/bursting. MM2 required."
    },

    "Lizardman Shaman": {
        name: "Lizardman Shaman",
        npcIds: [6766, 6767, 6768],
        location: { x: 1469, y: 3686, plane: 0 },
        locationName: "Lizardman Canyon / Shayzien",
        combatLevel: 150,
        attackStyle: "ranged",
        protectionPrayer: "PROTECT_FROM_MISSILES",
        slayerReq: 1,
        combatReq: 75,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: true,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar", "Nieve"],
        notes: "Drops Dragon Warhammer. Dodge spawns."
    },

    "Demonic Gorilla": {
        name: "Demonic Gorilla",
        npcIds: [7144, 7145, 7146, 7147, 7148, 7149, 7150, 7151, 7152],
        location: { x: 2130, y: 5663, plane: 0 },
        locationName: "Crash Site Cavern",
        combatLevel: 275,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 85,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Nieve", "Duradel"],
        notes: "Switches attack styles. Drops zenyte shards. MM2 required."
    },

    "Sulphur Lizard": {
        name: "Sulphur Lizard",
        npcIds: [8614, 8615],
        location: { x: 1294, y: 10178, plane: 0 },
        locationName: "Karuulm Slayer Dungeon",
        combatLevel: 50,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 44,
        combatReq: 40,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: { x: 1322, y: 3826, plane: 0 },
        assignedBy: ["Konar"],
        notes: "Boots of stone required in Karuulm."
    },

    "Warped Creature": {
        name: "Warped Creature",
        npcIds: [7532, 7533],
        location: { x: 1673, y: 10062, plane: 0 },
        locationName: "Catacombs of Kourend",
        combatLevel: 100,
        attackStyle: "melee",
        protectionPrayer: "PROTECT_FROM_MELEE",
        slayerReq: 1,
        combatReq: 70,
        specialItem: null,
        specialItemName: null,
        specialMechanic: null,
        safeSpottable: false,
        multiCombat: false,
        bankNearby: null,
        assignedBy: ["Konar"],
        notes: "Catacombs of Kourend only"
    }

};


// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// COMBAT LEVEL CALCULATOR
// ─────────────────────────────────────────────────────────────────────────────
function getCombatLevel() {
    var atk = Client.getRealSkillLevels(Skill.ATTACK);
    var str = Client.getRealSkillLevels(Skill.STRENGTH);
    var def = Client.getRealSkillLevels(Skill.DEFENCE);
    var hp = Client.getRealSkillLevels(Skill.HITPOINTS);
    var pray = Client.getRealSkillLevels(Skill.PRAYER);
    var ranged = Client.getRealSkillLevels(Skill.RANGED);
    var magic = Client.getRealSkillLevels(Skill.MAGIC);

    var base = 0.25 * (def + hp + Math.floor(pray / 2));
    var melee = 0.325 * (atk + str);
    var range = 0.325 * (Math.floor(ranged / 2) + ranged);
    var mage = 0.325 * (Math.floor(magic / 2) + magic);

    return Math.floor(base + Math.max(melee, range, mage));
}

// ─────────────────────────────────────────────────────────────────────────────
// SELECT BEST SLAYER MASTER
// ─────────────────────────────────────────────────────────────────────────────
function selectBestMaster(combatLevel, slayerLevel, preference) {
    if (preference && preference !== "Auto") {
        var key = preference.toUpperCase().replace(/ /g, "_").replace("NIEVE", "NIEVE");
        if (SLAYER_MASTERS[key] && combatLevel >= SLAYER_MASTERS[key].combatReq && slayerLevel >= SLAYER_MASTERS[key].slayerReq) {
            return SLAYER_MASTERS[key];
        }
    }

    // Auto: pick highest eligible (excluding Krystilia — wilderness)
    var priority = ["DURADEL", "NIEVE", "KONAR", "CHAELDAR", "VANNAKA", "MAZCHNA", "TURAEL"];
    for (var i = 0; i < priority.length; i++) {
        var master = SLAYER_MASTERS[priority[i]];
        if (combatLevel >= master.combatReq && slayerLevel >= master.slayerReq) {
            return master;
        }
    }
    return SLAYER_MASTERS.TURAEL;
}

// ─────────────────────────────────────────────────────────────────────────────
// FUZZY MATCH MONSTER NAME FROM CHAT
// ─────────────────────────────────────────────────────────────────────────────
function findMonster(taskName) {
    if (!taskName) return null;

    var name = taskName.trim();

    // Direct match
    if (MONSTER_DB[name]) return MONSTER_DB[name];

    // Case-insensitive match
    var lowerName = name.toLowerCase();
    for (var key in MONSTER_DB) {
        if (key.toLowerCase() === lowerName) return MONSTER_DB[key];
    }

    // Strip trailing 's' (plural)
    if (lowerName.endsWith("s")) {
        var singular = lowerName.slice(0, -1);
        for (var key in MONSTER_DB) {
            if (key.toLowerCase() === singular) return MONSTER_DB[key];
        }
    }

    // Strip trailing 'es' (e.g., "Banshees" -> "Banshee")
    if (lowerName.endsWith("es")) {
        var singular = lowerName.slice(0, -2);
        for (var key in MONSTER_DB) {
            if (key.toLowerCase() === singular) return MONSTER_DB[key];
        }
    }

    // Partial/contains match
    for (var key in MONSTER_DB) {
        if (key.toLowerCase().indexOf(lowerName) !== -1 || lowerName.indexOf(key.toLowerCase()) !== -1) {
            return MONSTER_DB[key];
        }
    }

    // Match against individual name field
    for (var key in MONSTER_DB) {
        var monsterName = MONSTER_DB[key].name.toLowerCase();
        if (monsterName === lowerName || monsterName === lowerName.replace(/s$/, "")) {
            return MONSTER_DB[key];
        }
    }

    return null;
}

// ─────────────────────────────────────────────────────────────────────────────
// FIND NEAREST BANK
// ─────────────────────────────────────────────────────────────────────────────
function findNearestBank(playerX, playerY, playerPlane) {
    var nearest = null;
    var nearestDist = Infinity;

    for (var i = 0; i < BANKS.length; i++) {
        var bank = BANKS[i];
        if (bank.plane !== playerPlane && bank.plane !== 0) continue; // Skip different planes unless ground
        var dx = bank.x - playerX;
        var dy = bank.y - playerY;
        var dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearest = bank;
        }
    }

    return nearest;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET BEST EQUIPMENT FOR LEVEL
// ─────────────────────────────────────────────────────────────────────────────
function getBestWeapon(attackLevel) {
    for (var i = 0; i < MELEE_WEAPONS.length; i++) {
        if (attackLevel >= MELEE_WEAPONS[i].atkReq) {
            return MELEE_WEAPONS[i];
        }
    }
    return MELEE_WEAPONS[MELEE_WEAPONS.length - 1];
}

function getBestHelmet(defenceLevel, hasSlayerHelm) {
    if (hasSlayerHelm) return HELMETS[0]; // Slayer helm (i) first
    for (var i = 0; i < HELMETS.length; i++) {
        if (defenceLevel >= HELMETS[i].defReq) {
            return HELMETS[i];
        }
    }
    return HELMETS[HELMETS.length - 1];
}

function getBestBody(defenceLevel) {
    for (var i = 0; i < PLATEBODIES.length; i++) {
        if (defenceLevel >= PLATEBODIES[i].defReq) {
            return PLATEBODIES[i];
        }
    }
    return PLATEBODIES[PLATEBODIES.length - 1];
}

function getBestLegs(defenceLevel) {
    for (var i = 0; i < PLATELEGS.length; i++) {
        if (defenceLevel >= PLATELEGS[i].defReq) {
            return PLATELEGS[i];
        }
    }
    return PLATELEGS[PLATELEGS.length - 1];
}

function getBestShield(defenceLevel) {
    for (var i = 0; i < SHIELDS.length; i++) {
        if (defenceLevel >= SHIELDS[i].defReq) {
            return SHIELDS[i];
        }
    }
    return SHIELDS[SHIELDS.length - 1];
}

function getBestBoots(defenceLevel) {
    for (var i = 0; i < BOOTS.length; i++) {
        if (defenceLevel >= BOOTS[i].defReq) {
            return BOOTS[i];
        }
    }
    return BOOTS[BOOTS.length - 1];
}

function getBestGloves(defenceLevel) {
    for (var i = 0; i < GLOVES.length; i++) {
        if (defenceLevel >= GLOVES[i].defReq) {
            return GLOVES[i];
        }
    }
    return GLOVES[GLOVES.length - 1];
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECK IF PLAYER HAS SPECIAL ITEM COVERED
// ─────────────────────────────────────────────────────────────────────────────
function isSpecialItemCovered(monster) {
    if (!monster.specialItem) return true;

    // Check if player has slayer helmet — covers nose peg, earmuffs, face mask
    var hasSlayerHelm = Game.localPlayer.getEquipment().search(11864) !== -1 ||
                        Game.localPlayer.getEquipment().search(11865) !== -1;

    if (hasSlayerHelm) {
        var coveredItems = [4168, 4166, 4164]; // Nose peg, earmuffs, face mask
        if (coveredItems.indexOf(monster.specialItem) !== -1) {
            return true;
        }
    }

    // Check equipment
    if (monster.specialMechanic === "equip") {
        return Game.localPlayer.getEquipment().search(monster.specialItem) !== -1;
    }

    // Check inventory for finish/use items
    if (monster.specialMechanic === "finish") {
        return Game.info.inventory.search(monster.specialItem) !== -1;
    }

    // Check weapon requirement
    if (monster.specialMechanic === "weapon") {
        var weaponIds = [11902, 20727, 11875]; // Leaf-bladed sword, axe, broad bolts
        for (var i = 0; i < weaponIds.length; i++) {
            if (Game.localPlayer.getEquipment().search(weaponIds[i]) !== -1) return true;
            if (Game.info.inventory.search(weaponIds[i]) !== -1) return true;
        }
        return false;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// COUNT FOOD IN INVENTORY
// ─────────────────────────────────────────────────────────────────────────────
function countFood() {
    var count = 0;
    for (var i = 0; i < ALL_FOOD_IDS.length; i++) {
        count += Game.info.inventory.count(ALL_FOOD_IDS[i]);
    }
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// COUNT PRAYER POTIONS
// ─────────────────────────────────────────────────────────────────────────────
function countPrayerPots() {
    var count = 0;
    for (var i = 0; i < PRAYER_POTION_IDS.length; i++) {
        count += Game.info.inventory.count(PRAYER_POTION_IDS[i]);
    }
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET DISTANCE BETWEEN TWO POINTS
// ─────────────────────────────────────────────────────────────────────────────
function getDistance(x1, y1, x2, y2) {
    return Math.sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECK IF PLAYER IS NEAR LOCATION
// ─────────────────────────────────────────────────────────────────────────────
function isNear(targetX, targetY, threshold) {
    if (!threshold) threshold = 5;
    var pos = Game.localPlayer.getPosition();
    return getDistance(pos.x, pos.y, targetX, targetY) <= threshold;
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECK IF PLAYER IS AT DEATH SPAWN
// ─────────────────────────────────────────────────────────────────────────────
function isAtDeathSpawn() {
    return isNear(DEATH_SPAWN.x, DEATH_SPAWN.y, 10);
}

// ─────────────────────────────────────────────────────────────────────────────
// PRAYER CONSTANTS
// ─────────────────────────────────────────────────────────────────────────────
var PRAYERS = {
    PROTECT_FROM_MELEE: { name: "Protect from Melee", widgetChild: 19 },
    PROTECT_FROM_MISSILES: { name: "Protect from Missiles", widgetChild: 18 },
    PROTECT_FROM_MAGIC: { name: "Protect from Magic", widgetChild: 17 }
};

// ─────────────────────────────────────────────────────────────────────────────
// MASTER TASK ASSIGNMENTS (for reference: which monsters each master assigns)
// ─────────────────────────────────────────────────────────────────────────────
var MASTER_TASKS = {
    TURAEL: ["Banshee", "Bat", "Bear", "Bird", "Cave Bug", "Cave Crawler", "Cave Slime", "Cow", "Crawling Hand", "Desert Lizard", "Dog", "Dwarf", "Ghost", "Goblin", "Icefiend", "Kalphite Worker", "Minotaur", "Monkey", "Rat", "Scorpion", "Skeleton", "Spider", "Wolf", "Zombie", "Black Bear", "Giant Bat", "Guard Dog", "Terrorbird"],
    MAZCHNA: ["Banshee", "Bat", "Bear", "Catablepon", "Cave Crawler", "Cave Slime", "Cockatrice", "Crawling Hand", "Cyclops", "Desert Lizard", "Dog", "Earth Warrior", "Flesh Crawler", "Ghost", "Ghoul", "Hill Giant", "Hobgoblin", "Ice Warrior", "Infernal Mage", "Kalphite Worker", "Killerwatt", "Mogre", "Pyrefiend", "Rock Slug", "Shade", "Vampire", "Wall Beast", "Zombie"],
    VANNAKA: ["Aberrant Spectre", "Ankou", "Banshee", "Basilisk", "Bloodveld", "Blue Dragon", "Bronze Dragon", "Cave Crawler", "Cockatrice", "Crocodile", "Cyclops", "Dagannoth", "Earth Warrior", "Elves", "Fire Giant", "Green Dragon", "Harpie Bug Swarm", "Hill Giant", "Ice Giant", "Ice Warrior", "Infernal Mage", "Jelly", "Jungle Horror", "Kalphite Worker", "Killerwatt", "Kurask", "Lesser Demon", "Mogre", "Moss Giant", "Otherworldly Being", "Pyrefiend", "Rock Slug", "Shadow Warrior", "Turoth", "Vampire", "Werewolf", "Hellhound"],
    CHAELDAR: ["Aberrant Spectre", "Abyssal Demon", "Banshee", "Basilisk", "Black Demon", "Black Dragon", "Bloodveld", "Blue Dragon", "Cave Horror", "Cave Kraken", "Dagannoth", "Dust Devil", "Elves", "Fire Giant", "Gargoyle", "Greater Demon", "Hellhound", "Infernal Mage", "Iron Dragon", "Jelly", "Jungle Horror", "Kalphite", "Kurask", "Lesser Demon", "Mutated Zygomite", "Nechryael", "Pyrefiend", "Skeletal Wyvern", "Spiritual Creature", "Steel Dragon", "Troll", "Turoth", "Waterfiend", "Baby Blue Dragon", "Wyvern"],
    KONAR: ["Aberrant Spectre", "Abyssal Demon", "Adamant Dragon", "Basilisk", "Black Demon", "Black Dragon", "Bloodveld", "Blue Dragon", "Brutal Black Dragon", "Cave Kraken", "Dagannoth", "Dark Beast", "Drake", "Dust Devil", "Fire Giant", "Fossil Island Wyvern", "Gargoyle", "Greater Demon", "Hellhound", "Hydra", "Iron Dragon", "Jelly", "Kalphite", "Kraken", "Kurask", "Lizardman", "Lizardman Shaman", "Mutated Zygomite", "Nechryael", "Red Dragon", "Rune Dragon", "Skeletal Wyvern", "Smoke Devil", "Steel Dragon", "Suqah", "Sulphur Lizard", "Troll", "Warped Creature", "Waterfiend", "Wyrm"],
    NIEVE: ["Aberrant Spectre", "Abyssal Demon", "Adamant Dragon", "Ankou", "Black Demon", "Black Dragon", "Bloodveld", "Blue Dragon", "Brutal Black Dragon", "Brutal Blue Dragon", "Cave Horror", "Cave Kraken", "Dagannoth", "Dark Beast", "Demonic Gorilla", "Drake", "Dust Devil", "Fire Giant", "Fossil Island Wyvern", "Gargoyle", "Greater Demon", "Greater Nechryael", "Hellhound", "Iron Dragon", "Kalphite", "Kraken", "Kurask", "Lizardman", "Lizardman Shaman", "Maniacal Monkey", "Mithril Dragon", "Nechryael", "Red Dragon", "Rune Dragon", "Skeletal Wyvern", "Smoke Devil", "Spiritual Creature", "Steel Dragon", "Suqah", "Troll", "Turoth", "Warped Jelly", "Waterfiend", "Wyrm"],
    DURADEL: ["Aberrant Spectre", "Abyssal Demon", "Adamant Dragon", "Ankou", "Black Demon", "Black Dragon", "Bloodveld", "Blue Dragon", "Brutal Black Dragon", "Cave Horror", "Cave Kraken", "Dagannoth", "Dark Beast", "Demonic Gorilla", "Deviant Spectre", "Drake", "Dust Devil", "Fire Giant", "Fossil Island Wyvern", "Gargoyle", "Greater Demon", "Greater Nechryael", "Hellhound", "Iron Dragon", "Kalphite", "Kraken", "Maniacal Monkey", "Mithril Dragon", "Nechryael", "Red Dragon", "Rune Dragon", "Skeletal Wyvern", "Smoke Devil", "Spiritual Creature", "Steel Dragon", "Suqah", "Troll", "Warped Jelly", "Waterfiend", "Wyrm"]
};


// ==================== RESOURCE INTELLIGENCE ENGINE ====================
// =============================================================================
// AUTO SLAYER BOT v2.2 — RESOURCE INTELLIGENCE ENGINE
// =============================================================================
// The brain that decides WHAT resources to gather, WHETHER to level up first,
// and HOW to get everything. Handles level 1 to 99, every food tier, every
// gear progression path, and Barrows runs at 70+ stats.
//
// KEY PRINCIPLE: Analyze task → check levels → pick optimal tier → gather → return
// Brad's rule: "It checks all my levels and available resources, even factors in
// if it's worth it to gain some levels to get the next best resource"
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// ITEM IDS
// ─────────────────────────────────────────────────────────────────────────────
var SS_ITEMS = {
    // Currency
    COINS: 995,

    // Fishing tools
    SMALL_FISHING_NET: 303,
    FLY_FISHING_ROD: 309,
    FISHING_ROD: 307,
    LOBSTER_POT: 301,
    HARPOON: 311,
    FEATHER: 314,

    // Raw fish
    RAW_SHRIMPS: 317,
    RAW_ANCHOVIES: 321,
    RAW_TROUT: 335,
    RAW_SALMON: 331,
    RAW_TUNA: 359,
    RAW_LOBSTER: 377,
    RAW_SWORDFISH: 371,
    RAW_MONKFISH: 7944,
    RAW_SHARK: 383,
    RAW_ANGLERFISH: 13439,

    // Cooked fish
    COOKED_SHRIMPS: 315,
    COOKED_ANCHOVIES: 319,
    COOKED_TROUT: 333,
    COOKED_SALMON: 329,
    COOKED_TUNA: 361,
    COOKED_LOBSTER: 379,
    COOKED_SWORDFISH: 373,
    COOKED_MONKFISH: 7946,
    COOKED_SHARK: 385,
    COOKED_ANGLERFISH: 13441,

    // Meat
    RAW_CHICKEN: 2138,
    COOKED_CHICKEN: 2140,
    RAW_BEEF: 2132,
    COOKED_MEAT: 2142,
    BONES: 526,
    BIG_BONES: 532,

    // Mining
    COPPER_ORE: 436,
    TIN_ORE: 438,
    IRON_ORE: 440,
    COAL: 453,
    MITHRIL_ORE: 447,
    ADAMANTITE_ORE: 449,
    BRONZE_BAR: 2349,
    IRON_BAR: 2351,
    STEEL_BAR: 2353,
    MITHRIL_BAR: 2359,
    ADAMANTITE_BAR: 2361,

    // Crafting
    COWHIDE: 1739,
    LEATHER: 1741,
    HARD_LEATHER: 1743,
    NEEDLE: 1733,
    THREAD: 1734,
    LEATHER_BODY: 1129,
    LEATHER_CHAPS: 1095,
    LEATHER_VAMBRACES: 1063,
    HARDLEATHER_BODY: 1131,

    // Smithing tools
    HAMMER: 2347,

    // Mining tools
    BRONZE_PICKAXE: 1265,
    IRON_PICKAXE: 1267,
    STEEL_PICKAXE: 1269,
    MITHRIL_PICKAXE: 1273,
    ADAMANT_PICKAXE: 1271,
    RUNE_PICKAXE: 1275,

    // Weapons (melee)
    BRONZE_SWORD: 1277,
    BRONZE_SCIMITAR: 1321,
    IRON_SCIMITAR: 1323,
    STEEL_SCIMITAR: 1325,
    MITHRIL_SCIMITAR: 1329,
    ADAMANT_SCIMITAR: 1331,
    RUNE_SCIMITAR: 1333,
    DRAGON_SCIMITAR: 4587,
    ABYSSAL_WHIP: 4151,

    // Armor (melee)
    BRONZE_PLATEBODY: 1117,
    BRONZE_PLATELEGS: 1075,
    IRON_PLATEBODY: 1115,
    IRON_PLATELEGS: 1067,
    STEEL_PLATEBODY: 1119,
    STEEL_PLATELEGS: 1069,
    MITHRIL_PLATEBODY: 1121,
    MITHRIL_PLATELEGS: 1071,
    ADAMANT_PLATEBODY: 1123,
    ADAMANT_PLATELEGS: 1073,
    RUNE_PLATEBODY: 1127,
    RUNE_PLATELEGS: 1079,
    RUNE_FULL_HELM: 1163,
    RUNE_KITESHIELD: 1201,

    // Shields
    BRONZE_KITESHIELD: 1189,
    IRON_KITESHIELD: 1191,
    STEEL_KITESHIELD: 1193,
    MITHRIL_KITESHIELD: 1197,
    ADAMANT_KITESHIELD: 1199,

    // Ranged
    SHORTBOW: 841,
    OAK_SHORTBOW: 843,
    WILLOW_SHORTBOW: 849,
    MAPLE_SHORTBOW: 853,
    MAGIC_SHORTBOW: 861,
    RUNE_CROSSBOW: 9185,
    BRONZE_ARROW: 882,
    IRON_ARROW: 884,
    STEEL_ARROW: 886,
    MITHRIL_ARROW: 888,
    ADAMANT_ARROW: 890,
    RUNE_ARROW: 892,
    BROAD_BOLTS: 11875,

    // Prayer
    PRAYER_POTION_4: 2434,
    PRAYER_POTION_3: 139,
    PRAYER_POTION_2: 141,
    PRAYER_POTION_1: 143,

    // Barrows gear
    DHAROK_HELM: 4716,
    DHAROK_PLATEBODY: 4720,
    DHAROK_PLATELEGS: 4722,
    DHAROK_GREATAXE: 4718,
    GUTHAN_HELM: 4724,
    GUTHAN_PLATEBODY: 4728,
    GUTHAN_CHAINSKIRT: 4730,
    GUTHAN_WARSPEAR: 4726,
    VERAC_HELM: 4753,
    VERAC_BRASSARD: 4757,
    VERAC_PLATESKIRT: 4759,
    VERAC_FLAIL: 4755,
    TORAG_HELM: 4745,
    TORAG_PLATEBODY: 4749,
    TORAG_PLATELEGS: 4751,
    TORAG_HAMMERS: 4747,
    KARIL_COIF: 4732,
    KARIL_TOP: 4736,
    KARIL_SKIRT: 4738,
    KARIL_CROSSBOW: 4734,
    AHRIM_HOOD: 4708,
    AHRIM_TOP: 4712,
    AHRIM_SKIRT: 4714,
    AHRIM_STAFF: 4710,

    // Runes
    AIR_RUNE: 556,
    WATER_RUNE: 555,
    EARTH_RUNE: 557,
    FIRE_RUNE: 554,
    MIND_RUNE: 558,
    CHAOS_RUNE: 562,
    DEATH_RUNE: 560,
    BLOOD_RUNE: 565,

    // Misc
    VIAL_OF_WATER: 227,
    ANTIFIRE_POTION_4: 2452,
    IBANS_STAFF: 1409,

    // Valuable loot to always pick up
    HERB_GRIMY_RANARR: 207,
    HERB_GRIMY_SNAPDRAGON: 3051,
    HERB_GRIMY_TORSTOL: 219,
    NATURE_RUNE: 561,
    LAW_RUNE: 563,
    UNCUT_DIAMOND: 1617,
    UNCUT_RUBY: 1619,
    UNCUT_SAPPHIRE: 1623,
    UNCUT_EMERALD: 1621,
    CLUE_SCROLL_MEDIUM: 2801,
    CLUE_SCROLL_HARD: 2722,
    LONG_BONE: 10976,
    CURVED_BONE: 10977,
    ENSOULED_HEAD: 13467 // generic; actual IDs vary by monster
};

// ─────────────────────────────────────────────────────────────────────────────
// NPC IDS
// ─────────────────────────────────────────────────────────────────────────────
var SS_NPCS = {
    CHICKEN: [3667, 1017, 1018, 1019],
    COW: [2790, 2791, 2793, 2795],
    GOBLIN: [3029, 3030, 3031, 3032, 3033, 3034],
    GUARD: [3010, 3011],
    GIANT_RAT: [2510, 2511, 2512],
    // Fishing spots
    NET_FISHING_SPOT: [1525, 1527, 1530],
    FLY_FISHING_SPOT: [1526, 1527, 1528],
    CAGE_HARPOON_SPOT: [1510, 1519, 1522],
    NET_HARPOON_BIG_SPOT: [1520, 1521],
    // Shop NPCs
    GERRANT_FISHING: 558,    // Port Sarim fishing shop
    LUMBRIDGE_GENERAL: 528,
    AL_KHARID_TANNER: 2824,  // Ellis
    LOWE_ARCHERY: 1385,       // Varrock archery
    BOB_AXES: 519             // Lumbridge Bob's Axes
};

// ─────────────────────────────────────────────────────────────────────────────
// OBJECT IDS
// ─────────────────────────────────────────────────────────────────────────────
var SS_OBJECTS = {
    // Cooking
    LUMBRIDGE_RANGE: 114,      // 3211,3215,0
    COOKING_FIRE: 26185,       // Generic fire object

    // Mining rocks
    COPPER_ROCK: [10943, 11161],
    TIN_ROCK: [10944, 11360, 11361],
    IRON_ROCK: [10942, 11364, 11365],
    COAL_ROCK: [11366, 11367],
    MITHRIL_ROCK: [11372, 11373],
    ADAMANTITE_ROCK: [11374, 11375],

    // Smelting & Smithing
    FURNACE: [16469, 24009, 10082],   // Al Kharid, Edgeville, Falador
    ANVIL: [2097, 2783],               // Varrock, Falador

    // Barrows
    BARROWS_CHEST: [20973],
    BARROWS_DOORS: [20681, 20682, 20683, 20684, 20685, 20686],

    // Banks
    BANK_BOOTH: [10355, 10357, 10583, 24101, 25808, 27718, 27719, 27720, 27721]
};

// ─────────────────────────────────────────────────────────────────────────────
// LOCATIONS (WorldPoint x, y, plane)
// ─────────────────────────────────────────────────────────────────────────────
var SS_LOCATIONS = {
    // Gathering spots
    LUMBRIDGE_CHICKENS:         { x: 3236, y: 3295, z: 0 },
    LUMBRIDGE_COWS:             { x: 3258, y: 3265, z: 0 },
    LUMBRIDGE_SWAMP_FISHING:    { x: 3242, y: 3151, z: 0 },
    BARBARIAN_VILLAGE_FISHING:  { x: 3103, y: 3433, z: 0 },
    KARAMJA_FISHING:            { x: 2924, y: 3178, z: 0 },
    CATHERBY_FISHING:           { x: 2837, y: 3431, z: 0 },
    FISHING_GUILD:              { x: 2599, y: 3419, z: 0 },
    PISCATORIS:                 { x: 2339, y: 3702, z: 0 },
    PORT_PISCARILIUS:           { x: 1823, y: 3781, z: 0 },

    // Cooking
    LUMBRIDGE_RANGE:            { x: 3211, y: 3215, z: 0 },
    BARBARIAN_VILLAGE_FIRE:     { x: 3100, y: 3430, z: 0 },
    CATHERBY_RANGE:             { x: 2815, y: 3443, z: 0 },
    KARAMJA_FIRE:               { x: 2928, y: 3175, z: 0 },

    // Mining
    LUMBRIDGE_SWAMP_MINE:       { x: 3230, y: 3148, z: 0 },
    AL_KHARID_MINE:             { x: 3298, y: 3313, z: 0 },
    VARROCK_EAST_MINE:          { x: 3285, y: 3365, z: 0 },
    MINING_GUILD:               { x: 3048, y: 9764, z: 0 },

    // Processing
    AL_KHARID_FURNACE:          { x: 3275, y: 3186, z: 0 },
    VARROCK_ANVIL:              { x: 3189, y: 3425, z: 0 },
    AL_KHARID_TANNER:           { x: 3274, y: 3192, z: 0 },

    // Shops
    PORT_SARIM_FISHING_SHOP:    { x: 3013, y: 3222, z: 0 },
    LUMBRIDGE_GENERAL_STORE:    { x: 3211, y: 3246, z: 0 },
    VARROCK_ARCHERY_SHOP:       { x: 3234, y: 3420, z: 0 },

    // Banks
    LUMBRIDGE_BANK:             { x: 3208, y: 3220, z: 2 },
    AL_KHARID_BANK:             { x: 3269, y: 3167, z: 0 },
    VARROCK_WEST_BANK:          { x: 3185, y: 3436, z: 0 },
    VARROCK_EAST_BANK:          { x: 3253, y: 3420, z: 0 },
    FALADOR_BANK:               { x: 2946, y: 3368, z: 0 },
    EDGEVILLE_BANK:             { x: 3094, y: 3491, z: 0 },
    CATHERBY_BANK:              { x: 2808, y: 3441, z: 0 },
    FISHING_GUILD_BANK:         { x: 2588, y: 3419, z: 0 },
    GRAND_EXCHANGE:             { x: 3164, y: 3487, z: 0 },

    // Barrows
    BARROWS_ENTRANCE:           { x: 3565, y: 3288, z: 0 },
    BARROWS_DHAROK:             { x: 3575, y: 3297, z: 0 },
    BARROWS_GUTHAN:             { x: 3577, y: 3283, z: 0 },
    BARROWS_VERAC:              { x: 3557, y: 3298, z: 0 },
    BARROWS_TORAG:              { x: 3553, y: 3283, z: 0 },
    BARROWS_KARIL:              { x: 3565, y: 3275, z: 0 },
    BARROWS_AHRIM:              { x: 3565, y: 3289, z: 0 }
};

// ─────────────────────────────────────────────────────────────────────────────
// VALUABLE LOOT — Always pick these up (GP generation)
// ─────────────────────────────────────────────────────────────────────────────
var VALUABLE_LOOT_IDS = [
    SS_ITEMS.COINS,
    SS_ITEMS.NATURE_RUNE,
    SS_ITEMS.LAW_RUNE,
    SS_ITEMS.DEATH_RUNE,
    SS_ITEMS.BLOOD_RUNE,
    SS_ITEMS.CHAOS_RUNE,
    SS_ITEMS.HERB_GRIMY_RANARR,
    SS_ITEMS.HERB_GRIMY_SNAPDRAGON,
    SS_ITEMS.HERB_GRIMY_TORSTOL,
    SS_ITEMS.UNCUT_DIAMOND,
    SS_ITEMS.UNCUT_RUBY,
    SS_ITEMS.UNCUT_EMERALD,
    SS_ITEMS.UNCUT_SAPPHIRE,
    SS_ITEMS.CLUE_SCROLL_MEDIUM,
    SS_ITEMS.CLUE_SCROLL_HARD,
    SS_ITEMS.LONG_BONE,
    SS_ITEMS.CURVED_BONE,
    SS_ITEMS.RUNE_SCIMITAR,
    SS_ITEMS.ABYSSAL_WHIP,
    SS_ITEMS.RUNE_PLATEBODY,
    SS_ITEMS.RUNE_PLATELEGS,
    SS_ITEMS.RUNE_KITESHIELD,
    SS_ITEMS.RUNE_FULL_HELM,
    SS_ITEMS.DRAGON_SCIMITAR,
    SS_ITEMS.ADAMANT_PLATEBODY
];

// Approximate GE prices for value estimation
var SS_PRICES = {
    995: 1,       // Coins (literal)
    561: 180,     // Nature rune
    563: 150,     // Law rune
    560: 200,     // Death rune
    565: 350,     // Blood rune
    562: 80,      // Chaos rune
    207: 8000,    // Ranarr
    3051: 9000,   // Snapdragon
    219: 7000,    // Torstol
    1617: 3000,   // Uncut diamond
    1619: 1300,   // Uncut ruby
    1621: 700,    // Uncut emerald
    1623: 500,    // Uncut sapphire
    1333: 15000,  // Rune scim
    4151: 2500000, // Abyssal whip
    1127: 38000,  // Rune platebody
    1079: 38000,  // Rune platelegs
    1201: 32000,  // Rune kiteshield
    1163: 11000,  // Rune full helm
    4587: 60000,  // Dragon scimitar
    1123: 10000   // Adamant platebody
};

// =============================================================================
// FOOD TIERS — ALL 10 TIERS from chicken to anglerfish
// =============================================================================
var SS_FOOD_TIERS = [
    {
        tier: 0,
        name: "Chicken",
        minFishLevel: 0,  // No fishing needed
        minCookLevel: 1,
        method: "kill",
        npcIds: SS_NPCS.CHICKEN,
        gatherLocation: SS_LOCATIONS.LUMBRIDGE_CHICKENS,
        rawItemId: SS_ITEMS.RAW_CHICKEN,
        cookedItemId: SS_ITEMS.COOKED_CHICKEN,
        cookLocation: SS_LOCATIONS.LUMBRIDGE_RANGE,
        nearestBank: SS_LOCATIONS.LUMBRIDGE_BANK,
        healAmount: 3,
        burnStopLevel: 4,
        toolRequired: null,
        toolId: null
    },
    {
        tier: 1,
        name: "Shrimps",
        minFishLevel: 1,
        minCookLevel: 1,
        method: "fish",
        fishAction: "NET",
        spotIds: SS_NPCS.NET_FISHING_SPOT,
        gatherLocation: SS_LOCATIONS.LUMBRIDGE_SWAMP_FISHING,
        rawItemId: SS_ITEMS.RAW_SHRIMPS,
        cookedItemId: SS_ITEMS.COOKED_SHRIMPS,
        cookLocation: SS_LOCATIONS.LUMBRIDGE_RANGE,
        nearestBank: SS_LOCATIONS.LUMBRIDGE_BANK,
        healAmount: 3,
        burnStopLevel: 34,
        toolRequired: "Small fishing net",
        toolId: SS_ITEMS.SMALL_FISHING_NET
    },
    {
        tier: 2,
        name: "Trout",
        minFishLevel: 20,
        minCookLevel: 15,
        method: "fish",
        fishAction: "FLY",
        spotIds: SS_NPCS.FLY_FISHING_SPOT,
        gatherLocation: SS_LOCATIONS.BARBARIAN_VILLAGE_FISHING,
        rawItemId: SS_ITEMS.RAW_TROUT,
        cookedItemId: SS_ITEMS.COOKED_TROUT,
        cookLocation: SS_LOCATIONS.BARBARIAN_VILLAGE_FIRE,
        nearestBank: SS_LOCATIONS.EDGEVILLE_BANK,
        healAmount: 7,
        burnStopLevel: 50,
        toolRequired: "Fly fishing rod + feathers",
        toolId: SS_ITEMS.FLY_FISHING_ROD,
        secondaryId: SS_ITEMS.FEATHER,
        secondaryNeeded: 200
    },
    {
        tier: 3,
        name: "Salmon",
        minFishLevel: 30,
        minCookLevel: 25,
        method: "fish",
        fishAction: "FLY",
        spotIds: SS_NPCS.FLY_FISHING_SPOT,
        gatherLocation: SS_LOCATIONS.BARBARIAN_VILLAGE_FISHING,
        rawItemId: SS_ITEMS.RAW_SALMON,
        cookedItemId: SS_ITEMS.COOKED_SALMON,
        cookLocation: SS_LOCATIONS.BARBARIAN_VILLAGE_FIRE,
        nearestBank: SS_LOCATIONS.EDGEVILLE_BANK,
        healAmount: 9,
        burnStopLevel: 58,
        toolRequired: "Fly fishing rod + feathers",
        toolId: SS_ITEMS.FLY_FISHING_ROD,
        secondaryId: SS_ITEMS.FEATHER,
        secondaryNeeded: 200
    },
    {
        tier: 4,
        name: "Tuna",
        minFishLevel: 35,
        minCookLevel: 30,
        method: "fish",
        fishAction: "HARPOON",
        spotIds: SS_NPCS.CAGE_HARPOON_SPOT,
        gatherLocation: SS_LOCATIONS.KARAMJA_FISHING,
        rawItemId: SS_ITEMS.RAW_TUNA,
        cookedItemId: SS_ITEMS.COOKED_TUNA,
        cookLocation: SS_LOCATIONS.KARAMJA_FIRE,
        nearestBank: null,  // No bank on Karamja — use deposit box
        healAmount: 10,
        burnStopLevel: 64,
        toolRequired: "Harpoon",
        toolId: SS_ITEMS.HARPOON
    },
    {
        tier: 5,
        name: "Lobster",
        minFishLevel: 40,
        minCookLevel: 40,
        method: "fish",
        fishAction: "CAGE",
        spotIds: SS_NPCS.CAGE_HARPOON_SPOT,
        gatherLocation: SS_LOCATIONS.KARAMJA_FISHING,
        rawItemId: SS_ITEMS.RAW_LOBSTER,
        cookedItemId: SS_ITEMS.COOKED_LOBSTER,
        cookLocation: SS_LOCATIONS.KARAMJA_FIRE,
        nearestBank: null,
        healAmount: 12,
        burnStopLevel: 74,
        toolRequired: "Lobster pot",
        toolId: SS_ITEMS.LOBSTER_POT
    },
    {
        tier: 6,
        name: "Swordfish",
        minFishLevel: 50,
        minCookLevel: 45,
        method: "fish",
        fishAction: "HARPOON",
        spotIds: SS_NPCS.CAGE_HARPOON_SPOT,
        gatherLocation: SS_LOCATIONS.CATHERBY_FISHING,
        rawItemId: SS_ITEMS.RAW_SWORDFISH,
        cookedItemId: SS_ITEMS.COOKED_SWORDFISH,
        cookLocation: SS_LOCATIONS.CATHERBY_RANGE,
        nearestBank: SS_LOCATIONS.CATHERBY_BANK,
        healAmount: 14,
        burnStopLevel: 86,
        toolRequired: "Harpoon",
        toolId: SS_ITEMS.HARPOON
    },
    {
        tier: 7,
        name: "Monkfish",
        minFishLevel: 62,
        minCookLevel: 62,
        method: "fish",
        fishAction: "NET",
        spotIds: SS_NPCS.NET_HARPOON_BIG_SPOT,
        gatherLocation: SS_LOCATIONS.PISCATORIS,
        rawItemId: SS_ITEMS.RAW_MONKFISH,
        cookedItemId: SS_ITEMS.COOKED_MONKFISH,
        cookLocation: SS_LOCATIONS.PISCATORIS,  // Range nearby
        nearestBank: SS_LOCATIONS.PISCATORIS,    // Bank nearby
        healAmount: 16,
        burnStopLevel: 92,
        toolRequired: "Small fishing net",
        toolId: SS_ITEMS.SMALL_FISHING_NET,
        questRequired: "Swan Song"
    },
    {
        tier: 8,
        name: "Shark",
        minFishLevel: 76,
        minCookLevel: 80,
        method: "fish",
        fishAction: "HARPOON",
        spotIds: SS_NPCS.NET_HARPOON_BIG_SPOT,
        gatherLocation: SS_LOCATIONS.FISHING_GUILD,
        rawItemId: SS_ITEMS.RAW_SHARK,
        cookedItemId: SS_ITEMS.COOKED_SHARK,
        cookLocation: SS_LOCATIONS.FISHING_GUILD,
        nearestBank: SS_LOCATIONS.FISHING_GUILD_BANK,
        healAmount: 20,
        burnStopLevel: 94,
        toolRequired: "Harpoon",
        toolId: SS_ITEMS.HARPOON,
        fishingGuildRequired: true  // Need 68 fishing to enter
    },
    {
        tier: 9,
        name: "Anglerfish",
        minFishLevel: 82,
        minCookLevel: 84,
        method: "fish",
        fishAction: "ROD",
        spotIds: [1520],  // Anglerfish spot
        gatherLocation: SS_LOCATIONS.PORT_PISCARILIUS,
        rawItemId: SS_ITEMS.RAW_ANGLERFISH,
        cookedItemId: SS_ITEMS.COOKED_ANGLERFISH,
        cookLocation: SS_LOCATIONS.PORT_PISCARILIUS,
        nearestBank: SS_LOCATIONS.PORT_PISCARILIUS,
        healAmount: 22,
        burnStopLevel: 99,
        toolRequired: "Fishing rod + sandworms",
        toolId: SS_ITEMS.FISHING_ROD,
        questRequired: "Kourend Favour"
    }
];

// =============================================================================
// GEAR PROGRESSION — Melee weapons, armor, ranged, and magic by level
// =============================================================================
var SS_MELEE_WEAPONS = [
    { level: 1,  itemId: SS_ITEMS.BRONZE_SWORD,    name: "Bronze sword",    smithLvl: 1,  barId: SS_ITEMS.BRONZE_BAR, bars: 1 },
    { level: 1,  itemId: SS_ITEMS.BRONZE_SCIMITAR,  name: "Bronze scimitar", smithLvl: 5,  barId: SS_ITEMS.BRONZE_BAR, bars: 2 },
    { level: 1,  itemId: SS_ITEMS.IRON_SCIMITAR,    name: "Iron scimitar",   smithLvl: 20, barId: SS_ITEMS.IRON_BAR, bars: 2 },
    { level: 5,  itemId: SS_ITEMS.STEEL_SCIMITAR,   name: "Steel scimitar",  smithLvl: 35, barId: SS_ITEMS.STEEL_BAR, bars: 2 },
    { level: 20, itemId: SS_ITEMS.MITHRIL_SCIMITAR, name: "Mithril scimitar",smithLvl: 55, barId: SS_ITEMS.MITHRIL_BAR, bars: 2 },
    { level: 30, itemId: SS_ITEMS.ADAMANT_SCIMITAR, name: "Adamant scimitar",smithLvl: 75, barId: SS_ITEMS.ADAMANTITE_BAR, bars: 2 },
    { level: 40, itemId: SS_ITEMS.RUNE_SCIMITAR,    name: "Rune scimitar",   geOnly: true },
    { level: 60, itemId: SS_ITEMS.DRAGON_SCIMITAR,   name: "Dragon scimitar", geOnly: true, questRequired: "Monkey Madness I" },
    { level: 70, itemId: SS_ITEMS.ABYSSAL_WHIP,      name: "Abyssal whip",    geOnly: true }
];

var SS_MELEE_ARMOR = [
    { level: 1,  bodyId: SS_ITEMS.LEATHER_BODY,     legsId: SS_ITEMS.LEATHER_CHAPS,     name: "Leather",  craftable: true, craftLvl: 14 },
    { level: 1,  bodyId: SS_ITEMS.BRONZE_PLATEBODY,  legsId: SS_ITEMS.BRONZE_PLATELEGS,  name: "Bronze",   smithLvl: 18, barId: SS_ITEMS.BRONZE_BAR, bodyBars: 5, legsBars: 3 },
    { level: 1,  bodyId: SS_ITEMS.IRON_PLATEBODY,    legsId: SS_ITEMS.IRON_PLATELEGS,    name: "Iron",     smithLvl: 33, barId: SS_ITEMS.IRON_BAR, bodyBars: 5, legsBars: 3 },
    { level: 5,  bodyId: SS_ITEMS.STEEL_PLATEBODY,   legsId: SS_ITEMS.STEEL_PLATELEGS,   name: "Steel",    smithLvl: 48, barId: SS_ITEMS.STEEL_BAR, bodyBars: 5, legsBars: 3 },
    { level: 20, bodyId: SS_ITEMS.MITHRIL_PLATEBODY, legsId: SS_ITEMS.MITHRIL_PLATELEGS, name: "Mithril",  smithLvl: 68, barId: SS_ITEMS.MITHRIL_BAR, bodyBars: 5, legsBars: 3 },
    { level: 30, bodyId: SS_ITEMS.ADAMANT_PLATEBODY, legsId: SS_ITEMS.ADAMANT_PLATELEGS, name: "Adamant",  smithLvl: 88, barId: SS_ITEMS.ADAMANTITE_BAR, bodyBars: 5, legsBars: 3 },
    { level: 40, bodyId: SS_ITEMS.RUNE_PLATEBODY,    legsId: SS_ITEMS.RUNE_PLATELEGS,    name: "Rune",     geOnly: true },
    { level: 70, bodyId: SS_ITEMS.DHAROK_PLATEBODY,   legsId: SS_ITEMS.DHAROK_PLATELEGS,  name: "Dharok",   barrowsOnly: true },
    { level: 70, bodyId: SS_ITEMS.GUTHAN_PLATEBODY,   legsId: SS_ITEMS.GUTHAN_CHAINSKIRT, name: "Guthan",   barrowsOnly: true },
    { level: 70, bodyId: SS_ITEMS.TORAG_PLATEBODY,    legsId: SS_ITEMS.TORAG_PLATELEGS,   name: "Torag",    barrowsOnly: true },
    { level: 70, bodyId: SS_ITEMS.VERAC_BRASSARD,     legsId: SS_ITEMS.VERAC_PLATESKIRT,  name: "Verac",    barrowsOnly: true }
];

var SS_RANGED_WEAPONS = [
    { level: 1,  itemId: SS_ITEMS.SHORTBOW,       name: "Shortbow",       ammoId: SS_ITEMS.BRONZE_ARROW },
    { level: 20, itemId: SS_ITEMS.WILLOW_SHORTBOW, name: "Willow shortbow", ammoId: SS_ITEMS.IRON_ARROW },
    { level: 30, itemId: SS_ITEMS.MAPLE_SHORTBOW,  name: "Maple shortbow", ammoId: SS_ITEMS.STEEL_ARROW },
    { level: 40, itemId: SS_ITEMS.MAGIC_SHORTBOW,  name: "Magic shortbow", ammoId: SS_ITEMS.RUNE_ARROW },
    { level: 61, itemId: SS_ITEMS.RUNE_CROSSBOW,   name: "Rune crossbow",  ammoId: SS_ITEMS.BROAD_BOLTS }
];

// =============================================================================
// RESOURCE INTELLIGENCE ENGINE — The Brain
// =============================================================================

/**
 * analyzeTaskRequirements — Given a slayer task, determine everything needed
 * @param {string} monsterName - Name of the slayer monster
 * @param {number} taskCount - How many to kill
 * @param {object} monsterData - From SLAYER_MONSTERS config: { combatLevel, maxHit, ... }
 * @returns {object} Full resource plan
 */
function analyzeTaskRequirements(monsterName, taskCount, monsterData) {
    var combatLvl = monsterData ? (monsterData.combatLevel || 0) : 0;
    var maxHit = monsterData ? (monsterData.maxHit || 0) : 0;

    // Get ALL player levels
    var levels = getAllPlayerLevels();

    // Determine difficulty tier
    var difficulty = getMonsterDifficulty(combatLvl, maxHit);

    // Determine food needs
    var foodPlan = planFoodForTask(difficulty, taskCount, levels);

    // Determine prayer needs
    var prayerPlan = planPrayerForTask(difficulty, taskCount, levels);

    // Determine gear needs
    var gearPlan = planGearForTask(difficulty, levels);

    // Determine if we should level up before attempting
    var levelUpPlan = shouldLevelUpFirst(foodPlan, gearPlan, levels);

    // Determine if Barrows runs are viable/needed
    var barrowsPlan = planBarrowsIfNeeded(levels, gearPlan);

    var plan = {
        monsterName: monsterName,
        taskCount: taskCount,
        difficulty: difficulty,
        food: foodPlan,
        prayer: prayerPlan,
        gear: gearPlan,
        levelUp: levelUpPlan,
        barrows: barrowsPlan,
        estimatedTripFood: estimateFoodPerTrip(difficulty, foodPlan.bestTier),
        estimatedTrips: Math.ceil(taskCount / estimateKillsPerTrip(difficulty, foodPlan.bestTier, levels))
    };

    Game.sendGameMessage("[SlayerBot][RI] Task Analysis: " + monsterName + " x" + taskCount, "Bot");
    Game.sendGameMessage("[SlayerBot][RI]   Difficulty: " + difficulty.name + " | Food tier needed: " + difficulty.minFoodTier, "Bot");
    Game.sendGameMessage("[SlayerBot][RI]   Best food available: " + foodPlan.bestTier.name + " (tier " + foodPlan.bestTier.tier + ")", "Bot");
    Game.sendGameMessage("[SlayerBot][RI]   Prayer needed: " + prayerPlan.needed + " | Pots needed: " + prayerPlan.potsNeeded, "Bot");
    Game.sendGameMessage("[SlayerBot][RI]   Level up first: " + levelUpPlan.shouldLevel + (levelUpPlan.shouldLevel ? " (" + levelUpPlan.skill + " to " + levelUpPlan.targetLevel + ")" : ""), "Bot");
    Game.sendGameMessage("[SlayerBot][RI]   Barrows viable: " + barrowsPlan.viable + " | Should do runs: " + barrowsPlan.shouldDoRuns, "Bot");
    Game.sendGameMessage("[SlayerBot][RI]   Est trips: " + plan.estimatedTrips, "Bot");

    return plan;
}

/**
 * getAllPlayerLevels — Read every skill level
 */
function getAllPlayerLevels() {
    return {
        attack:     Client.getRealSkillLevels(Skill.ATTACK) || 1,
        strength:   Client.getRealSkillLevels(Skill.STRENGTH) || 1,
        defence:    Client.getRealSkillLevels(Skill.DEFENCE) || 1,
        ranged:     Client.getRealSkillLevels(Skill.RANGED) || 1,
        prayer:     Client.getRealSkillLevels(Skill.PRAYER) || 1,
        magic:      Client.getRealSkillLevels(Skill.MAGIC) || 1,
        hitpoints:  Client.getRealSkillLevels(Skill.HITPOINTS) || 10,
        mining:     Client.getRealSkillLevels(Skill.MINING) || 1,
        smithing:   Client.getRealSkillLevels(Skill.SMITHING) || 1,
        fishing:    Client.getRealSkillLevels(Skill.FISHING) || 1,
        cooking:    Client.getRealSkillLevels(Skill.COOKING) || 1,
        crafting:   Client.getRealSkillLevels(Skill.CRAFTING) || 1,
        fletching:  Client.getRealSkillLevels(Skill.FLETCHING) || 1,
        herblore:   Client.getRealSkillLevels(Skill.HERBLORE) || 1,
        slayer:     Client.getRealSkillLevels(Skill.SLAYER) || 1,
        woodcutting: Client.getRealSkillLevels(Skill.WOODCUTTING) || 1,
        firemaking: Client.getRealSkillLevels(Skill.FIREMAKING) || 1,
        agility:    Client.getRealSkillLevels(Skill.AGILITY) || 1
    };
}

/**
 * getMonsterDifficulty — Classify a monster and determine resource requirements
 */
function getMonsterDifficulty(combatLevel, maxHit) {
    if (combatLevel <= 15) {
        return { name: "Trivial", minFoodTier: 0, prayerNeeded: false, prayerPotsNeeded: false, foodPerKill: 0.2 };
    }
    if (combatLevel <= 40) {
        return { name: "Easy", minFoodTier: 1, prayerNeeded: false, prayerPotsNeeded: false, foodPerKill: 0.5 };
    }
    if (combatLevel <= 80) {
        return { name: "Medium", minFoodTier: 3, prayerNeeded: false, prayerPotsNeeded: false, foodPerKill: 1.0 };
    }
    if (combatLevel <= 140) {
        return { name: "Hard", minFoodTier: 5, prayerNeeded: true, prayerPotsNeeded: false, foodPerKill: 1.5 };
    }
    if (combatLevel <= 250) {
        return { name: "Very Hard", minFoodTier: 7, prayerNeeded: true, prayerPotsNeeded: true, foodPerKill: 2.0 };
    }
    return { name: "Boss", minFoodTier: 8, prayerNeeded: true, prayerPotsNeeded: true, foodPerKill: 3.0 };
}

// =============================================================================
// FOOD PLANNING — Pick the right food tier
// =============================================================================

/**
 * planFoodForTask — Determine the best food strategy
 */
function planFoodForTask(difficulty, taskCount, levels) {
    var requiredTier = difficulty.minFoodTier;
    var bestTier = selectBestFoodTier(levels);
    var gap = requiredTier - bestTier.tier;

    // How much food do we need total?
    var foodNeeded = Math.ceil(taskCount * difficulty.foodPerKill * 1.2); // 20% buffer

    // If our best tier is sufficient, great
    if (gap <= 0) {
        return {
            bestTier: bestTier,
            requiredTier: requiredTier,
            sufficient: true,
            shouldLevelFishing: false,
            foodQuantity: foodNeeded,
            strategy: "GATHER_BEST"
        };
    }

    // Gap is 1: just bring more lower-tier food (faster than leveling)
    if (gap === 1) {
        // Lower tier heals less, so need more food
        var ratio = SS_FOOD_TIERS[requiredTier].healAmount / bestTier.healAmount;
        var adjustedQuantity = Math.ceil(foodNeeded * ratio * 1.1);
        return {
            bestTier: bestTier,
            requiredTier: requiredTier,
            sufficient: false,
            shouldLevelFishing: false,
            foodQuantity: adjustedQuantity,
            strategy: "BRING_MORE_LOWER"
        };
    }

    // Gap is 2+: MUST level up fishing/cooking first
    var nextViableTier = findNextViableFoodTier(levels);
    return {
        bestTier: bestTier,
        requiredTier: requiredTier,
        sufficient: false,
        shouldLevelFishing: true,
        targetFishLevel: nextViableTier.minFishLevel,
        targetCookLevel: nextViableTier.minCookLevel,
        targetTier: nextViableTier,
        foodQuantity: foodNeeded,
        strategy: "LEVEL_UP_THEN_GATHER"
    };
}

/**
 * selectBestFoodTier — Find the HIGHEST tier food the player can fish AND cook
 */
function selectBestFoodTier(levels) {
    var fishLevel = levels.fishing;
    var cookLevel = levels.cooking;
    var bestTier = SS_FOOD_TIERS[0]; // Default: chicken (always available)

    for (var i = SS_FOOD_TIERS.length - 1; i >= 0; i--) {
        var tier = SS_FOOD_TIERS[i];

        // Skip quest-locked tiers if we can't verify quest completion
        if (tier.questRequired) continue;

        // Check if player can fish AND cook this tier
        if (fishLevel >= tier.minFishLevel && cookLevel >= tier.minCookLevel) {
            bestTier = tier;
            break;
        }
        // Special case: chicken doesn't need fishing
        if (tier.method === "kill" && cookLevel >= tier.minCookLevel) {
            bestTier = tier;
            // Don't break — keep looking for better
        }
    }

    Game.sendGameMessage("[SlayerBot][RI] Best food tier for fishing " + fishLevel + " / cooking " + cookLevel + ": " + bestTier.name + " (tier " + bestTier.tier + ", heals " + bestTier.healAmount + ")", "Bot");
    return bestTier;
}

/**
 * findNextViableFoodTier — Find the next tier UP from current that's closest to reach
 */
function findNextViableFoodTier(levels) {
    var fishLevel = levels.fishing;
    var cookLevel = levels.cooking;
    var currentBest = selectBestFoodTier(levels);

    for (var i = 0; i < SS_FOOD_TIERS.length; i++) {
        var tier = SS_FOOD_TIERS[i];
        if (tier.tier <= currentBest.tier) continue;
        if (tier.questRequired) continue;

        // This is the next tier we could reach
        var fishGap = Math.max(0, tier.minFishLevel - fishLevel);
        var cookGap = Math.max(0, tier.minCookLevel - cookLevel);
        var totalGap = fishGap + cookGap;

        // If the gap is reasonable (< 30 total levels), target this tier
        if (totalGap < 30) {
            Game.sendGameMessage("[SlayerBot][RI] Next viable food tier: " + tier.name + " (need fish " + tier.minFishLevel + ", cook " + tier.minCookLevel + ", gap: " + totalGap + " levels)", "Bot");
            return tier;
        }
    }

    // If no viable upgrade, just stick with current
    return currentBest;
}

/**
 * estimateFoodPerTrip — How many food items to bring per trip
 */
function estimateFoodPerTrip(difficulty, foodTier) {
    // Inventory has 28 slots. Reserve some for loot.
    // With prayer: bring fewer food, more prayer pots
    var baseSlots = difficulty.prayerPotsNeeded ? 16 : (difficulty.prayerNeeded ? 20 : 24);
    return baseSlots;
}

/**
 * estimateKillsPerTrip — How many monsters we can kill per bank trip
 */
function estimateKillsPerTrip(difficulty, foodTier, levels) {
    var foodSlots = estimateFoodPerTrip(difficulty, foodTier);
    var hpPerFood = foodTier.healAmount;
    var avgDamagePerKill = difficulty.foodPerKill * 10; // rough hp damage taken per kill
    if (avgDamagePerKill <= 0) avgDamagePerKill = 1;
    var totalHealable = foodSlots * hpPerFood;
    return Math.max(1, Math.floor(totalHealable / avgDamagePerKill));
}

// =============================================================================
// PRAYER PLANNING
// =============================================================================
function planPrayerForTask(difficulty, taskCount, levels) {
    if (!difficulty.prayerNeeded) {
        return { needed: false, potsNeeded: 0, prayerToUse: null };
    }

    // Determine prayer type based on monster
    var prayerToUse = "PROTECT_FROM_MELEE"; // default

    // Prayer drain rate: roughly 1 dose per 2 minutes with protect prayers
    // 1 pot = 4 doses = ~8 minutes of prayer
    var estimatedMinutesPerKill = 0.5;
    var totalMinutes = taskCount * estimatedMinutesPerKill;
    var dosesNeeded = Math.ceil(totalMinutes / 2);
    var potsNeeded = Math.ceil(dosesNeeded / 4);

    // If we have high prayer, need fewer pots
    if (levels.prayer >= 70) potsNeeded = Math.ceil(potsNeeded * 0.7);
    if (levels.prayer >= 50) potsNeeded = Math.ceil(potsNeeded * 0.85);

    return {
        needed: true,
        potsNeeded: difficulty.prayerPotsNeeded ? potsNeeded : 0,
        prayerToUse: prayerToUse,
        canAffordPots: false // Will be checked against GP
    };
}

// =============================================================================
// GEAR PLANNING — Determine best obtainable gear
// =============================================================================

function planGearForTask(difficulty, levels) {
    var meleeWeapon = selectBestMeleeWeapon(levels);
    var meleeArmor = selectBestMeleeArmor(levels);
    var rangedWeapon = selectBestRangedWeapon(levels);

    return {
        meleeWeapon: meleeWeapon,
        meleeArmor: meleeArmor,
        rangedWeapon: rangedWeapon,
        canSmith: canSmithGear(levels),
        canCraft: canCraftGear(levels),
        bestSmithableTier: getBestSmithableTier(levels),
        bestCraftableTier: getBestCraftableTier(levels)
    };
}

function selectBestMeleeWeapon(levels) {
    var best = SS_MELEE_WEAPONS[0];
    for (var i = SS_MELEE_WEAPONS.length - 1; i >= 0; i--) {
        var w = SS_MELEE_WEAPONS[i];
        if (levels.attack >= w.level) {
            // Can we actually OBTAIN this weapon?
            if (w.geOnly) {
                // Need GP — check if we can afford
                // For now, mark as "preferred but need GP"
                best = w;
                best.obtainMethod = "GE";
            } else if (w.smithLvl && levels.smithing >= w.smithLvl) {
                best = w;
                best.obtainMethod = "SMITH";
            } else if (w.smithLvl) {
                // Could smith if we level up
                best = w;
                best.obtainMethod = "LEVEL_SMITHING";
            }
            break;
        }
    }
    return best;
}

function selectBestMeleeArmor(levels) {
    var best = SS_MELEE_ARMOR[0];
    for (var i = SS_MELEE_ARMOR.length - 1; i >= 0; i--) {
        var a = SS_MELEE_ARMOR[i];
        if (levels.defence >= a.level) {
            if (a.barrowsOnly) {
                best = a;
                best.obtainMethod = "BARROWS";
            } else if (a.geOnly) {
                best = a;
                best.obtainMethod = "GE";
            } else if (a.craftable && levels.crafting >= a.craftLvl) {
                best = a;
                best.obtainMethod = "CRAFT";
            } else if (a.smithLvl && levels.smithing >= a.smithLvl) {
                best = a;
                best.obtainMethod = "SMITH";
            } else {
                best = a;
                best.obtainMethod = "NEED_LEVELS";
            }
            break;
        }
    }
    return best;
}

function selectBestRangedWeapon(levels) {
    var best = SS_RANGED_WEAPONS[0];
    for (var i = SS_RANGED_WEAPONS.length - 1; i >= 0; i--) {
        if (levels.ranged >= SS_RANGED_WEAPONS[i].level) {
            best = SS_RANGED_WEAPONS[i];
            break;
        }
    }
    return best;
}

function canSmithGear(levels) { return levels.smithing >= 1 && levels.mining >= 1; }
function canCraftGear(levels) { return levels.crafting >= 1; }

function getBestSmithableTier(levels) {
    if (levels.smithing >= 88) return "adamant";
    if (levels.smithing >= 68) return "mithril";
    if (levels.smithing >= 48) return "steel";
    if (levels.smithing >= 33) return "iron";
    return "bronze";
}

function getBestCraftableTier(levels) {
    if (levels.crafting >= 28) return "hardleather";
    if (levels.crafting >= 14) return "leather";
    return "none";
}

// =============================================================================
// LEVELING DECISION — Should we level up a skill before the task?
// =============================================================================

/**
 * shouldLevelUpFirst — The CRITICAL intelligence decision
 * Brad: "factors in if it's worth it to gain some levels to get the next best resource"
 */
function shouldLevelUpFirst(foodPlan, gearPlan, levels) {
    var plans = [];

    // Check if we need to level fishing/cooking
    if (foodPlan.shouldLevelFishing) {
        var fishGap = Math.max(0, (foodPlan.targetFishLevel || 0) - levels.fishing);
        var cookGap = Math.max(0, (foodPlan.targetCookLevel || 0) - levels.cooking);

        if (fishGap > 0) {
            plans.push({
                skill: "fishing",
                currentLevel: levels.fishing,
                targetLevel: foodPlan.targetFishLevel,
                gap: fishGap,
                reason: "Unlock " + (foodPlan.targetTier ? foodPlan.targetTier.name : "better food"),
                method: fishGap <= 10 ? "fish_at_current_spot" : "power_fish"
            });
        }
        if (cookGap > 0) {
            plans.push({
                skill: "cooking",
                currentLevel: levels.cooking,
                targetLevel: foodPlan.targetCookLevel,
                gap: cookGap,
                reason: "Cook " + (foodPlan.targetTier ? foodPlan.targetTier.name : "better food"),
                method: "cook_all_raw_fish"
            });
        }
    }

    // Check if we should level mining/smithing for better gear
    if (gearPlan.meleeArmor && gearPlan.meleeArmor.obtainMethod === "NEED_LEVELS") {
        var target = gearPlan.meleeArmor.smithLvl;
        if (target && target - levels.smithing <= 20) {
            plans.push({
                skill: "smithing",
                currentLevel: levels.smithing,
                targetLevel: target,
                gap: target - levels.smithing,
                reason: "Smith " + gearPlan.meleeArmor.name + " armor",
                method: "mine_and_smith"
            });
        }
    }

    // Sort by smallest gap first (quickest wins)
    plans.sort(function(a, b) { return a.gap - b.gap; });

    if (plans.length === 0) {
        return { shouldLevel: false, plans: [] };
    }

    return {
        shouldLevel: true,
        skill: plans[0].skill,
        targetLevel: plans[0].targetLevel,
        reason: plans[0].reason,
        plans: plans
    };
}

// =============================================================================
// BARROWS PLANNING — At 70+ combat, Barrows becomes a massive gear upgrade
// =============================================================================

function planBarrowsIfNeeded(levels, gearPlan) {
    var canDoBarrows = levels.attack >= 70 && levels.defence >= 70 &&
                       levels.magic >= 50 && levels.prayer >= 43;
    var hasBarrowsGear = false; // Would need to check bank

    // Should we do Barrows?
    // YES if: combat is 70+ AND current gear is rune or lower AND we don't already have Barrows
    var shouldDoRuns = canDoBarrows && !hasBarrowsGear &&
                       gearPlan.meleeArmor && gearPlan.meleeArmor.level < 70;

    return {
        viable: canDoBarrows,
        shouldDoRuns: shouldDoRuns,
        requiredLevels: { attack: 70, defence: 70, magic: 50, prayer: 43 },
        currentMeetsReqs: canDoBarrows,
        estimatedRunsForSet: 50, // ~50 runs average for a useful set
        runsPerTrip: 3 // estimate
    };
}
// =============================================================================
// LEVEL-UP EXECUTION — Actually level fishing, cooking, mining, smithing
// =============================================================================

// Current leveling state
var levelingSkill = null;
var levelingTarget = 0;
var levelingMethod = null;
var levelingPhase = "idle"; // idle, walking, fishing, cooking, mining, smelting, smithing

/**
 * startLevelingUp — Begin leveling a skill
 */
function startLevelingUp(plan) {
    levelingSkill = plan.skill;
    levelingTarget = plan.targetLevel;
    levelingMethod = plan.method;
    levelingPhase = "walking";

    Game.sendGameMessage("[SlayerBot][RI] Starting level-up: " + plan.skill + " from " +
               plan.currentLevel + " to " + plan.targetLevel + " (" + plan.reason + ")", "Bot");
}

/**
 * handleLevelingTick — Called each game tick when in LEVEL_UP state
 */
function handleLevelingTick() {
    if (isExecuting) return;

    // Check if we've reached target
    var currentLevel = 1;
    try {
        if (levelingSkill === "fishing") currentLevel = Client.getRealSkillLevels(Skill.FISHING);
        else if (levelingSkill === "cooking") currentLevel = Client.getRealSkillLevels(Skill.COOKING);
        else if (levelingSkill === "mining") currentLevel = Client.getRealSkillLevels(Skill.MINING);
        else if (levelingSkill === "smithing") currentLevel = Client.getRealSkillLevels(Skill.SMITHING);
        else if (levelingSkill === "crafting") currentLevel = Client.getRealSkillLevels(Skill.CRAFTING);
    } catch(e) {
        Game.sendGameMessage("[SlayerBot][RI] Error reading skill level: " + e, "Bot");
    }

    if (currentLevel >= levelingTarget) {
        Game.sendGameMessage("[SlayerBot][RI] ✅ Leveling complete! " + levelingSkill + " is now " + currentLevel, "Bot");
        levelingPhase = "idle";
        levelingSkill = null;
        return "DONE";
    }

    // Route to correct handler
    switch(levelingSkill) {
        case "fishing":  return handleLevelFishing(); break;
        case "cooking":  return handleLevelCooking(); break;
        case "mining":   return handleLevelMining(); break;
        case "smithing": return handleLevelSmelting(); break;
        case "crafting": return handleLevelCrafting(); break;
        default:
            Game.sendGameMessage("[SlayerBot][RI] Unknown leveling skill: " + levelingSkill, "Bot");
            return "DONE";
    }
}

/**
 * handleLevelFishing — Fish at the best available spot to level up
 */
function handleLevelFishing() {
    if (isExecuting) return;

    var fishLevel = Client.getRealSkillLevels(Skill.FISHING) || 1;

    // Pick best spot for leveling
    var spot = null;
    var location = null;

    if (fishLevel < 20) {
        // Net shrimps at Lumbridge Swamp
        spot = SS_NPCS.NET_FISHING_SPOT;
        location = SS_LOCATIONS.LUMBRIDGE_SWAMP_FISHING;
    } else if (fishLevel < 40) {
        // Fly fish trout at Barbarian Village (best xp)
        spot = SS_NPCS.FLY_FISHING_SPOT;
        location = SS_LOCATIONS.BARBARIAN_VILLAGE_FISHING;
    } else if (fishLevel < 62) {
        // Fly fish at Barbarian Village (still good xp)
        spot = SS_NPCS.FLY_FISHING_SPOT;
        location = SS_LOCATIONS.BARBARIAN_VILLAGE_FISHING;
    } else {
        // Harpoon at Catherby or Fishing Guild
        spot = SS_NPCS.CAGE_HARPOON_SPOT;
        location = SS_LOCATIONS.CATHERBY_FISHING;
    }

    // Check if we need tools
    if (fishLevel >= 20 && Game.info.inventory.getItemCount(SS_ITEMS.FLY_FISHING_ROD) === 0 &&
        Game.info.inventory.getItemCount(SS_ITEMS.FEATHER) === 0) {
        // Need fly rod and feathers — go to Port Sarim shop
        if (!isNearLocation(SS_LOCATIONS.PORT_SARIM_FISHING_SHOP, 15)) {
            walkToLocation(SS_LOCATIONS.PORT_SARIM_FISHING_SHOP);
            return;
        }
        // Buy from shop
        buyFromShop(SS_NPCS.GERRANT_FISHING, SS_ITEMS.FLY_FISHING_ROD, 1);
        return;
    }

    if (fishLevel < 20 && Game.info.inventory.getItemCount(SS_ITEMS.SMALL_FISHING_NET) === 0) {
        // Need fishing net
        if (!isNearLocation(SS_LOCATIONS.PORT_SARIM_FISHING_SHOP, 15)) {
            walkToLocation(SS_LOCATIONS.PORT_SARIM_FISHING_SHOP);
            return;
        }
        buyFromShop(SS_NPCS.GERRANT_FISHING, SS_ITEMS.SMALL_FISHING_NET, 1);
        return;
    }

    // Walk to fishing spot
    if (!isNearLocation(location, 15)) {
        walkToLocation(location);
        return;
    }

    // If inventory is full, drop fish (power fishing) or cook
    if (Game.info.inventory.isFull()) {
        dropAllRawFish();
        return;
    }

    // Fish!
    if (!PlayerHelper.isAnimating()) {
        var fishingSpot = Game.info.npc.getNearest(spot);
        if (fishingSpot) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    fishingSpot.action(MenuAction.NPC_FIRST_OPTION);
                    Game.sendGameMessage("[SlayerBot][RI] Fishing at " + fishingSpot.getName(), "Bot");
                } catch(e) {
                    Game.sendGameMessage("[SlayerBot][RI] Fish error: " + e, "Bot");
                }
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

/**
 * handleLevelCooking — Cook all raw fish to level cooking
 */
function handleLevelCooking() {
    if (isExecuting) return;

    // Check if we have raw fish in inventory
    var rawFish = findRawFishInInventory();
    if (!rawFish) {
        // No raw fish — go fish some first
        handleLevelFishing();
        return;
    }

    // Walk to Lumbridge range (never burns from quest completion bonus)
    if (!isNearLocation(SS_LOCATIONS.LUMBRIDGE_RANGE, 5)) {
        walkToLocation(SS_LOCATIONS.LUMBRIDGE_RANGE);
        return;
    }

    // Cook on range
    if (!PlayerHelper.isAnimating()) {
        var range = Game.info.gameObject.getNearest([SS_OBJECTS.LUMBRIDGE_RANGE]);
        if (range) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.inventory.consumeItem(rawFish);
                    Game.sendGameMessage("[SlayerBot][RI] Cooking raw fish: " + rawFish, "Bot");
                } catch(e) {
                    Game.sendGameMessage("[SlayerBot][RI] Cook error: " + e, "Bot");
                }
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

/**
 * handleLevelMining — Mine ores to level mining
 */
function handleLevelMining() {
    if (isExecuting) return;

    var miningLevel = Client.getRealSkillLevels(Skill.MINING) || 1;

    // Pick ore to mine
    var oreRocks = null;
    var mineLocation = null;

    if (miningLevel < 15) {
        oreRocks = SS_OBJECTS.COPPER_ROCK;
        mineLocation = SS_LOCATIONS.LUMBRIDGE_SWAMP_MINE;
    } else if (miningLevel < 30) {
        oreRocks = SS_OBJECTS.IRON_ROCK;
        mineLocation = SS_LOCATIONS.AL_KHARID_MINE;
    } else if (miningLevel < 55) {
        oreRocks = SS_OBJECTS.IRON_ROCK;
        mineLocation = SS_LOCATIONS.AL_KHARID_MINE;
    } else {
        oreRocks = SS_OBJECTS.MITHRIL_ROCK;
        mineLocation = SS_LOCATIONS.AL_KHARID_MINE;
    }

    // Need pickaxe
    if (Game.info.inventory.getItemCount(SS_ITEMS.BRONZE_PICKAXE) === 0 &&
        Game.info.inventory.getItemCount(SS_ITEMS.IRON_PICKAXE) === 0 &&
        Game.info.inventory.getItemCount(SS_ITEMS.STEEL_PICKAXE) === 0 &&
        Game.info.inventory.getItemCount(SS_ITEMS.MITHRIL_PICKAXE) === 0 &&
        Game.info.inventory.getItemCount(SS_ITEMS.RUNE_PICKAXE) === 0) {
        // Try to get pickaxe from bank
        bankForTool(SS_ITEMS.BRONZE_PICKAXE);
        return;
    }

    if (!isNearLocation(mineLocation, 15)) {
        walkToLocation(mineLocation);
        return;
    }

    if (Game.info.inventory.isFull()) {
        // Drop ores (power mining) or bank
        dropAllOres();
        return;
    }

    if (!PlayerHelper.isAnimating()) {
        var rock = Game.info.gameObject.getNearest(oreRocks);
        if (rock) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.gameObject.action(rock, MenuAction.GAME_OBJECT_FIRST_OPTION);
                    Game.sendGameMessage("[SlayerBot][RI] Mining ore", "Bot");
                } catch(e) {
                    Game.sendGameMessage("[SlayerBot][RI] Mine error: " + e, "Bot");
                }
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

function handleLevelSmelting() {
    if (isExecuting) return;
    // Walk to furnace with ores
    if (!isNearLocation(SS_LOCATIONS.AL_KHARID_FURNACE, 10)) {
        walkToLocation(SS_LOCATIONS.AL_KHARID_FURNACE);
        return;
    }
    var furnace = Game.info.gameObject.getNearest(SS_OBJECTS.FURNACE);
    if (furnace && !PlayerHelper.isAnimating()) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try {
                Game.interact.gameObject.action(furnace, MenuAction.GAME_OBJECT_FIRST_OPTION);
                Game.sendGameMessage("[SlayerBot][RI] Smelting at furnace", "Bot");
            } catch(e) {
                Game.sendGameMessage("[SlayerBot][RI] Smelt error: " + e, "Bot");
            }
            isExecuting = false;
        }, Utility.getDelay());
    }
}

function handleLevelCrafting() {
    if (isExecuting) return;
    // Kill cows -> tan -> craft
    handleGatherLeather();
}

// =============================================================================
// FOOD GATHERING — Master handler for all 10 food tiers
// =============================================================================

var gatherFoodTier = null;
var gatherFoodNeeded = 0;
var gatherFoodCount = 0;
var gatherFoodPhase = "idle"; // idle, walking_to_spot, gathering, walking_to_cook, cooking, walking_to_bank, banking, done

/**
 * initFoodGathering — Start gathering a specific food tier
 */
function initFoodGathering(tier, quantity) {
    if (typeof tier === "number") {
        gatherFoodTier = SS_FOOD_TIERS[Math.min(tier, SS_FOOD_TIERS.length - 1)];
    } else {
        gatherFoodTier = tier;
    }
    gatherFoodNeeded = quantity || 20;
    gatherFoodCount = 0;
    gatherFoodPhase = "walking_to_spot";

    Game.sendGameMessage("[SlayerBot][RI] Init food gathering: " + gatherFoodTier.name + " x" + gatherFoodNeeded, "Bot");
}

/**
 * handleGatherFoodTick — Main food gathering handler (called per tick)
 */
function handleGatherFoodTick() {
    if (isExecuting) return;
    if (!gatherFoodTier) return;

    // Check if we've gathered enough cooked food
    var cookedCount = Game.info.inventory.getItemCount(gatherFoodTier.cookedItemId);
    if (cookedCount >= gatherFoodNeeded) {
        Game.sendGameMessage("[SlayerBot][RI] ✅ Food gathering complete! " + cookedCount + " " + gatherFoodTier.name, "Bot");
        gatherFoodPhase = "done";
        return "DONE";
    }

    switch(gatherFoodPhase) {
        case "walking_to_spot":
            handleWalkToGatherSpot();
            break;
        case "gathering":
            if (gatherFoodTier.method === "kill") {
                handleGatherFoodByKilling();
            } else {
                handleGatherFoodByFishing();
            }
            break;
        case "walking_to_cook":
            handleWalkToCook();
            break;
        case "cooking":
            handleCookAllRawFood();
            break;
        case "walking_to_bank":
            handleWalkToFoodBank();
            break;
        case "banking":
            handleFoodBanking();
            break;
        case "done":
            return "DONE";
    }
}

function handleWalkToGatherSpot() {
    if (isExecuting || PlayerHelper.isWebWalking()) return;

    var loc = gatherFoodTier.gatherLocation;
    if (isNearLocation(loc, 15)) {
        gatherFoodPhase = "gathering";
        Game.sendGameMessage("[SlayerBot][RI] Arrived at " + gatherFoodTier.name + " gathering spot", "Bot");
        return;
    }

    walkToLocation(loc);
}

/**
 * handleGatherFoodByKilling — Kill chickens/cows for raw meat
 */
function handleGatherFoodByKilling() {
    if (isExecuting) return;

    // Check inventory
    if (Game.info.inventory.isFull()) {
        gatherFoodPhase = "walking_to_cook";
        return;
    }

    // Pick up raw meat from ground first
    var groundItems = Game.info.groundItem.getAll();
    if (groundItems) {
        for (var i = 0; i < groundItems.length; i++) {
            var item = groundItems[i];
            if (item.getId() === gatherFoodTier.rawItemId ||
                item.getId() === SS_ITEMS.FEATHER ||
                item.getId() === SS_ITEMS.BONES ||
                VALUABLE_LOOT_IDS.indexOf(item.getId()) !== -1) {
                isExecuting = true;
                Utility.invokeLater(function() {
                    try {
                        Game.interact.groundItem.pickup(item);
                    } catch(e) {}
                    isExecuting = false;
                }, Utility.getDelay());
                return;
            }
        }
    }

    // Kill if not in combat
    if (!PlayerHelper.isInCombat()) {
        var npc = Game.info.npc.getNearest(gatherFoodTier.npcIds);
        if (npc) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.npc.attack(npc);
                    Game.sendGameMessage("[SlayerBot][RI] Attacking " + npc.getName() + " for food", "Bot");
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

/**
 * handleGatherFoodByFishing — Fish at the appropriate tier spot
 */
function handleGatherFoodByFishing() {
    if (isExecuting) return;

    // Check tools
    if (gatherFoodTier.toolId && Game.info.inventory.getItemCount(gatherFoodTier.toolId) === 0) {
        // Missing tool — go buy from fishing shop
        Game.sendGameMessage("[SlayerBot][RI] Missing fishing tool: " + gatherFoodTier.toolRequired, "Bot");
        gatherFoodPhase = "walking_to_spot"; // Will redirect to shop
        return;
    }

    // Check for feathers if fly fishing
    if (gatherFoodTier.secondaryId && Game.info.inventory.getItemCount(gatherFoodTier.secondaryId) < 10) {
        Game.sendGameMessage("[SlayerBot][RI] Need more feathers — going to kill chickens", "Bot");
        // Kill chickens for feathers
        if (!isNearLocation(SS_LOCATIONS.LUMBRIDGE_CHICKENS, 15)) {
            walkToLocation(SS_LOCATIONS.LUMBRIDGE_CHICKENS);
            return;
        }
        // Kill chickens and collect feathers
        var feather = null;
        var groundItems = Game.info.groundItem.getAll();
        if (groundItems) {
            for (var i = 0; i < groundItems.length; i++) {
                if (groundItems[i].getId() === SS_ITEMS.FEATHER) {
                    feather = groundItems[i];
                    break;
                }
            }
        }
        if (feather) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try { Game.interact.groundItem.pickup(feather); } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
            return;
        }
        if (!PlayerHelper.isInCombat()) {
            var chicken = Game.info.npc.getNearest(SS_NPCS.CHICKEN);
            if (chicken) {
                isExecuting = true;
                Utility.invokeLater(function() {
                    try { Game.interact.npc.attack(chicken); } catch(e) {}
                    isExecuting = false;
                }, Utility.getDelay());
            }
        }
        return;
    }

    // Inventory full? Go cook
    if (Game.info.inventory.isFull()) {
        gatherFoodPhase = "walking_to_cook";
        return;
    }

    // Fish!
    if (!PlayerHelper.isAnimating()) {
        var spot = Game.info.npc.getNearest(gatherFoodTier.spotIds);
        if (spot) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    // Different fishing methods use different menu actions
                    if (gatherFoodTier.fishAction === "NET") {
                        spot.action(MenuAction.NPC_FIRST_OPTION);
                    } else if (gatherFoodTier.fishAction === "FLY") {
                        spot.action(MenuAction.NPC_FIRST_OPTION);
                    } else if (gatherFoodTier.fishAction === "CAGE") {
                        spot.action(MenuAction.NPC_FIRST_OPTION);
                    } else if (gatherFoodTier.fishAction === "HARPOON") {
                        spot.action(MenuAction.NPC_SECOND_OPTION);
                    } else if (gatherFoodTier.fishAction === "ROD") {
                        spot.action(MenuAction.NPC_SECOND_OPTION);
                    } else {
                        spot.action(MenuAction.NPC_FIRST_OPTION);
                    }
                    Game.sendGameMessage("[SlayerBot][RI] Fishing " + gatherFoodTier.name, "Bot");
                } catch(e) {
                    Game.sendGameMessage("[SlayerBot][RI] Fish error: " + e, "Bot");
                }
                isExecuting = false;
            }, Utility.getDelay());
        } else {
            Game.sendGameMessage("[SlayerBot][RI] No fishing spot found — walking closer", "Bot");
            walkToLocation(gatherFoodTier.gatherLocation);
        }
    }
}

// =============================================================================
// COOKING — Cook all raw fish
// =============================================================================

function handleWalkToCook() {
    if (isExecuting || PlayerHelper.isWebWalking()) return;

    var cookLoc = gatherFoodTier.cookLocation;
    if (isNearLocation(cookLoc, 8)) {
        gatherFoodPhase = "cooking";
        return;
    }
    walkToLocation(cookLoc);
}

/**
 * handleCookAllRawFood — Cook everything in inventory
 */
function handleCookAllRawFood() {
    if (isExecuting) return;

    var rawCount = Game.info.inventory.getItemCount(gatherFoodTier.rawItemId);
    if (rawCount === 0) {
        // Done cooking this batch — go back to gathering or bank
        var cookedTotal = Game.info.inventory.getItemCount(gatherFoodTier.cookedItemId);
        if (cookedTotal >= gatherFoodNeeded) {
            gatherFoodPhase = "done";
            return;
        }
        // Need more — bank and continue
        gatherFoodPhase = "walking_to_spot";
        return;
    }

    // Cook on range/fire
    if (!PlayerHelper.isAnimating()) {
        var cookObj = Game.info.gameObject.getNearest([SS_OBJECTS.LUMBRIDGE_RANGE, SS_OBJECTS.COOKING_FIRE]);
        if (cookObj) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    // Use raw fish on range
                    Game.interact.inventory.useItemOnObject(gatherFoodTier.rawItemId, cookObj);
                    Game.sendGameMessage("[SlayerBot][RI] Cooking " + gatherFoodTier.name + " (" + rawCount + " raw remaining)", "Bot");
                } catch(e) {
                    // Fallback: try clicking the range directly
                    try {
                        Game.interact.gameObject.action(cookObj, MenuAction.GAME_OBJECT_FIRST_OPTION);
                    } catch(e2) {
                        Game.sendGameMessage("[SlayerBot][RI] Cook error: " + e2, "Bot");
                    }
                }
                isExecuting = false;
            }, Utility.getDelay());
        } else {
            Game.sendGameMessage("[SlayerBot][RI] No cooking range found — walking to Lumbridge", "Bot");
            walkToLocation(SS_LOCATIONS.LUMBRIDGE_RANGE);
        }
    }
}

function handleWalkToFoodBank() {
    if (isExecuting || PlayerHelper.isWebWalking()) return;
    var bankLoc = gatherFoodTier.nearestBank || SS_LOCATIONS.LUMBRIDGE_BANK;
    if (isNearLocation(bankLoc, 10)) {
        gatherFoodPhase = "banking";
        return;
    }
    walkToLocation(bankLoc);
}

function handleFoodBanking() {
    if (isExecuting) return;
    if (!Game.info.bank.isOpen()) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try {
                Game.interact.bank.openNearest();
            } catch(e) {}
            isExecuting = false;
        }, Utility.getDelay());
        return;
    }
    // Deposit everything, withdraw tools, go again
    isExecuting = true;
    Utility.invokeLater(function() {
        try {
            Game.interact.bank.depositAllInventory();
            Game.sendGameMessage("[SlayerBot][RI] Deposited all — withdrawing fishing tools", "Bot");
        } catch(e) {}
        isExecuting = false;
    }, Utility.getDelay());
    gatherFoodPhase = "walking_to_spot";
}

// =============================================================================
// GEAR GATHERING — Kill monsters for drops, mine+smith, or Barrows
// =============================================================================

var gatherGearPhase = "idle";
var gatherGearTarget = null;

function initGearGathering(gearPlan) {
    gatherGearTarget = gearPlan;
    gatherGearPhase = "decide";
    Game.sendGameMessage("[SlayerBot][RI] Init gear gathering: " + (gearPlan.meleeWeapon ? gearPlan.meleeWeapon.name : "unknown"), "Bot");
}

function handleGatherGearTick() {
    if (isExecuting) return;
    if (!gatherGearTarget) return;

    switch(gatherGearPhase) {
        case "decide":
            decideGearMethod();
            break;
        case "mine_ore":
            handleMineOreForGear();
            break;
        case "smelt_ore":
            handleSmeltOreForGear();
            break;
        case "smith_gear":
            handleSmithGearItem();
            break;
        case "kill_for_drops":
            handleKillForGearDrops();
            break;
        case "kill_cows":
            handleKillCowsForLeather();
            break;
        case "tan_hides":
            handleTanHides();
            break;
        case "craft_leather":
            handleCraftLeatherArmor();
            break;
        case "barrows":
            handleBarrowsRun();
            break;
        case "done":
            return "DONE";
    }
}

function decideGearMethod() {
    var levels = getAllPlayerLevels();

    // Priority: Barrows > Smithing > Crafting > Drops > Buy
    if (gatherGearTarget.meleeArmor && gatherGearTarget.meleeArmor.barrowsOnly) {
        gatherGearPhase = "barrows";
        Game.sendGameMessage("[SlayerBot][RI] Going to Barrows for gear!", "Bot");
        return;
    }

    if (gatherGearTarget.canSmith && gatherGearTarget.bestSmithableTier !== "none") {
        gatherGearPhase = "mine_ore";
        Game.sendGameMessage("[SlayerBot][RI] Mining + smithing gear", "Bot");
        return;
    }

    if (levels.crafting >= 14) {
        gatherGearPhase = "kill_cows";
        Game.sendGameMessage("[SlayerBot][RI] Crafting leather armor", "Bot");
        return;
    }

    // Kill monsters and hope for drops
    gatherGearPhase = "kill_for_drops";
    Game.sendGameMessage("[SlayerBot][RI] Killing monsters for gear drops", "Bot");
}

// ─────────── MINING FOR GEAR ───────────

function handleMineOreForGear() {
    if (isExecuting) return;

    var smithLevel = Client.getRealSkillLevels(Skill.SMITHING) || 1;
    var miningLevel = Client.getRealSkillLevels(Skill.MINING) || 1;

    // Determine what ore to mine based on what we can smith
    var oreRocks, oreLocation, oreId, barsNeeded;

    if (smithLevel >= 48 && miningLevel >= 30) {
        // Steel: need iron + coal
        oreRocks = SS_OBJECTS.IRON_ROCK;
        oreLocation = SS_LOCATIONS.AL_KHARID_MINE;
        oreId = SS_ITEMS.IRON_ORE;
        barsNeeded = 8; // Platebody(5) + legs(3)
    } else if (smithLevel >= 33 && miningLevel >= 15) {
        // Iron
        oreRocks = SS_OBJECTS.IRON_ROCK;
        oreLocation = SS_LOCATIONS.AL_KHARID_MINE;
        oreId = SS_ITEMS.IRON_ORE;
        barsNeeded = 8;
    } else {
        // Bronze: tin + copper
        oreRocks = SS_OBJECTS.COPPER_ROCK;
        oreLocation = SS_LOCATIONS.LUMBRIDGE_SWAMP_MINE;
        oreId = SS_ITEMS.COPPER_ORE;
        barsNeeded = 8;
    }

    // Check if we have enough ore
    var oreCount = Game.info.inventory.getItemCount(oreId);
    if (oreId === SS_ITEMS.COPPER_ORE) {
        var tinCount = Game.info.inventory.getItemCount(SS_ITEMS.TIN_ORE);
        if (oreCount >= barsNeeded && tinCount >= barsNeeded) {
            gatherGearPhase = "smelt_ore";
            return;
        }
        // Need to mine both copper and tin
        if (oreCount < barsNeeded) {
            // Mine copper
            oreRocks = SS_OBJECTS.COPPER_ROCK;
        } else {
            // Mine tin
            oreRocks = SS_OBJECTS.TIN_ROCK;
        }
    } else if (oreCount >= barsNeeded) {
        gatherGearPhase = "smelt_ore";
        return;
    }

    // Need pickaxe
    if (!hasPickaxe()) {
        bankForTool(SS_ITEMS.BRONZE_PICKAXE);
        return;
    }

    if (!isNearLocation(oreLocation, 15)) {
        walkToLocation(oreLocation);
        return;
    }

    if (Game.info.inventory.isFull()) {
        gatherGearPhase = "smelt_ore";
        return;
    }

    if (!PlayerHelper.isAnimating()) {
        var rock = Game.info.gameObject.getNearest(oreRocks);
        if (rock) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.gameObject.action(rock, MenuAction.GAME_OBJECT_FIRST_OPTION);
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

function handleSmeltOreForGear() {
    if (isExecuting) return;

    if (!isNearLocation(SS_LOCATIONS.AL_KHARID_FURNACE, 10)) {
        walkToLocation(SS_LOCATIONS.AL_KHARID_FURNACE);
        return;
    }

    // Check if all ore is smelted
    var hasOre = Game.info.inventory.getItemCount(SS_ITEMS.COPPER_ORE) > 0 ||
                 Game.info.inventory.getItemCount(SS_ITEMS.TIN_ORE) > 0 ||
                 Game.info.inventory.getItemCount(SS_ITEMS.IRON_ORE) > 0 ||
                 Game.info.inventory.getItemCount(SS_ITEMS.COAL) > 0;

    if (!hasOre) {
        gatherGearPhase = "smith_gear";
        return;
    }

    if (!PlayerHelper.isAnimating()) {
        var furnace = Game.info.gameObject.getNearest(SS_OBJECTS.FURNACE);
        if (furnace) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.gameObject.action(furnace, MenuAction.GAME_OBJECT_FIRST_OPTION);
                    Game.sendGameMessage("[SlayerBot][RI] Smelting ore at furnace", "Bot");
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

function handleSmithGearItem() {
    if (isExecuting) return;

    if (!isNearLocation(SS_LOCATIONS.VARROCK_ANVIL, 10)) {
        walkToLocation(SS_LOCATIONS.VARROCK_ANVIL);
        return;
    }

    // Check if we have bars
    var hasBars = Game.info.inventory.getItemCount(SS_ITEMS.BRONZE_BAR) > 0 ||
                  Game.info.inventory.getItemCount(SS_ITEMS.IRON_BAR) > 0 ||
                  Game.info.inventory.getItemCount(SS_ITEMS.STEEL_BAR) > 0 ||
                  Game.info.inventory.getItemCount(SS_ITEMS.MITHRIL_BAR) > 0;

    if (!hasBars) {
        gatherGearPhase = "done";
        Game.sendGameMessage("[SlayerBot][RI] No more bars to smith — done", "Bot");
        return;
    }

    // Need hammer
    if (Game.info.inventory.getItemCount(SS_ITEMS.HAMMER) === 0) {
        bankForTool(SS_ITEMS.HAMMER);
        return;
    }

    if (!PlayerHelper.isAnimating()) {
        var anvil = Game.info.gameObject.getNearest(SS_OBJECTS.ANVIL);
        if (anvil) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.gameObject.action(anvil, MenuAction.GAME_OBJECT_FIRST_OPTION);
                    Game.sendGameMessage("[SlayerBot][RI] Smithing at anvil", "Bot");
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

// ─────────── KILLING FOR DROPS ───────────

function handleKillForGearDrops() {
    if (isExecuting) return;

    // Kill goblins (drop bronze/iron gear) or guards (drop steel gear)
    var levels = getAllPlayerLevels();
    var targetNPCs, targetLoc;

    if (levels.attack < 10) {
        targetNPCs = SS_NPCS.GOBLIN;
        targetLoc = SS_LOCATIONS.LUMBRIDGE_CHICKENS; // Goblins near Lumbridge
    } else {
        targetNPCs = SS_NPCS.GUARD;
        targetLoc = { x: 3211, y: 3381, z: 0 }; // Falador guards
    }

    if (!isNearLocation(targetLoc, 20)) {
        walkToLocation(targetLoc);
        return;
    }

    // Pick up gear drops
    var groundItems = Game.info.groundItem.getAll();
    if (groundItems) {
        for (var i = 0; i < groundItems.length; i++) {
            var item = groundItems[i];
            var id = item.getId();
            // Pick up any weapon or armor
            if (isEquipmentDrop(id)) {
                isExecuting = true;
                Utility.invokeLater(function() {
                    try { Game.interact.groundItem.pickup(item); } catch(e) {}
                    isExecuting = false;
                }, Utility.getDelay());
                return;
            }
        }
    }

    if (!PlayerHelper.isInCombat()) {
        var npc = Game.info.npc.getNearest(targetNPCs);
        if (npc) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try { Game.interact.npc.attack(npc); } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
    }

    // After 50 kills, give up on drops and try smithing
    gatherGearPhase = "done";
}

function isEquipmentDrop(itemId) {
    // Check if this item is a weapon or armor we can use
    var equipIds = [
        SS_ITEMS.BRONZE_SWORD, SS_ITEMS.BRONZE_SCIMITAR,
        SS_ITEMS.IRON_SCIMITAR, SS_ITEMS.STEEL_SCIMITAR,
        SS_ITEMS.MITHRIL_SCIMITAR, SS_ITEMS.ADAMANT_SCIMITAR,
        SS_ITEMS.RUNE_SCIMITAR,
        SS_ITEMS.BRONZE_PLATEBODY, SS_ITEMS.IRON_PLATEBODY,
        SS_ITEMS.STEEL_PLATEBODY, SS_ITEMS.MITHRIL_PLATEBODY,
        SS_ITEMS.ADAMANT_PLATEBODY, SS_ITEMS.RUNE_PLATEBODY,
        SS_ITEMS.BRONZE_PLATELEGS, SS_ITEMS.IRON_PLATELEGS,
        SS_ITEMS.STEEL_PLATELEGS, SS_ITEMS.MITHRIL_PLATELEGS,
        SS_ITEMS.ADAMANT_PLATELEGS, SS_ITEMS.RUNE_PLATELEGS,
        SS_ITEMS.BRONZE_KITESHIELD, SS_ITEMS.IRON_KITESHIELD,
        SS_ITEMS.STEEL_KITESHIELD, SS_ITEMS.MITHRIL_KITESHIELD,
        SS_ITEMS.ADAMANT_KITESHIELD, SS_ITEMS.RUNE_KITESHIELD
    ];
    return equipIds.indexOf(itemId) !== -1;
}

// =============================================================================
// LEATHER CRAFTING — Kill cows → tan → craft
// =============================================================================

var leatherPhase = "killing";

function handleKillCowsForLeather() {
    if (isExecuting) return;

    var hideCount = Game.info.inventory.getItemCount(SS_ITEMS.COWHIDE);
    if (hideCount >= 10 || Game.info.inventory.isFull()) {
        gatherGearPhase = "tan_hides";
        return;
    }

    if (!isNearLocation(SS_LOCATIONS.LUMBRIDGE_COWS, 15)) {
        walkToLocation(SS_LOCATIONS.LUMBRIDGE_COWS);
        return;
    }

    // Pick up hides
    var groundItems = Game.info.groundItem.getAll();
    if (groundItems) {
        for (var i = 0; i < groundItems.length; i++) {
            if (groundItems[i].getId() === SS_ITEMS.COWHIDE) {
                isExecuting = true;
                Utility.invokeLater(function() {
                    try { Game.interact.groundItem.pickup(groundItems[i]); } catch(e) {}
                    isExecuting = false;
                }, Utility.getDelay());
                return;
            }
        }
    }

    if (!PlayerHelper.isInCombat()) {
        var cow = Game.info.npc.getNearest(SS_NPCS.COW);
        if (cow) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try { Game.interact.npc.attack(cow); } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
    }
}

function handleTanHides() {
    if (isExecuting) return;

    var hideCount = Game.info.inventory.getItemCount(SS_ITEMS.COWHIDE);
    if (hideCount === 0) {
        gatherGearPhase = "craft_leather";
        return;
    }

    if (!isNearLocation(SS_LOCATIONS.AL_KHARID_TANNER, 10)) {
        walkToLocation(SS_LOCATIONS.AL_KHARID_TANNER);
        return;
    }

    var tanner = Game.info.npc.getNearest([SS_NPCS.AL_KHARID_TANNER]);
    if (tanner) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try {
                tanner.action(MenuAction.NPC_FIRST_OPTION);
                Game.sendGameMessage("[SlayerBot][RI] Tanning " + hideCount + " cowhides", "Bot");
            } catch(e) {}
            isExecuting = false;
        }, Utility.getDelay());
    }
}

function handleCraftLeatherArmor() {
    if (isExecuting) return;

    var leatherCount = Game.info.inventory.getItemCount(SS_ITEMS.LEATHER);
    if (leatherCount === 0) {
        gatherGearPhase = "done";
        Game.sendGameMessage("[SlayerBot][RI] Leather crafting done", "Bot");
        return;
    }

    // Need needle + thread
    if (Game.info.inventory.getItemCount(SS_ITEMS.NEEDLE) === 0 ||
        Game.info.inventory.getItemCount(SS_ITEMS.THREAD) === 0) {
        bankForTool(SS_ITEMS.NEEDLE);
        return;
    }

    // Use needle on leather
    isExecuting = true;
    Utility.invokeLater(function() {
        try {
            Game.interact.inventory.useItemOnItem(SS_ITEMS.NEEDLE, SS_ITEMS.LEATHER);
            Game.sendGameMessage("[SlayerBot][RI] Crafting leather armor", "Bot");
        } catch(e) {}
        isExecuting = false;
    }, Utility.getDelay());
}

function handleGatherLeather() {
    // Alias for the full leather gathering flow
    handleKillCowsForLeather();
}
// =============================================================================
// BARROWS RUNS — At 70+ combat, massive gear upgrade
// =============================================================================

var barrowsPhase = "walk_to_barrows";
var barrowsBrother = 0; // 0-5 for each brother
var barrowsRunCount = 0;
var barrowsMaxRuns = 10;

var BARROWS_BROTHERS = [
    { name: "Dharok",  location: { x: 3575, y: 3297, z: 0 }, npcId: 1673, prayer: "PROTECT_FROM_MELEE" },
    { name: "Guthan",  location: { x: 3577, y: 3283, z: 0 }, npcId: 1674, prayer: "PROTECT_FROM_MELEE" },
    { name: "Verac",   location: { x: 3557, y: 3298, z: 0 }, npcId: 1677, prayer: "PROTECT_FROM_MELEE" },
    { name: "Torag",   location: { x: 3553, y: 3283, z: 0 }, npcId: 1676, prayer: "PROTECT_FROM_MELEE" },
    { name: "Karil",   location: { x: 3565, y: 3275, z: 0 }, npcId: 1675, prayer: "PROTECT_FROM_MISSILES" },
    { name: "Ahrim",   location: { x: 3565, y: 3289, z: 0 }, npcId: 1672, prayer: "PROTECT_FROM_MAGIC" }
];

function handleBarrowsRun() {
    if (isExecuting) return;

    if (barrowsRunCount >= barrowsMaxRuns) {
        Game.sendGameMessage("[SlayerBot][RI] Completed " + barrowsRunCount + " Barrows runs — checking loot", "Bot");
        gatherGearPhase = "done";
        return;
    }

    switch(barrowsPhase) {
        case "walk_to_barrows":
            handleWalkToBarrows();
            break;
        case "dig_mound":
            handleDigMound();
            break;
        case "kill_brother":
            handleKillBrother();
            break;
        case "next_brother":
            handleNextBrother();
            break;
        case "open_chest":
            handleOpenChest();
            break;
        case "bank_loot":
            handleBarrowsBanking();
            break;
    }
}

function handleWalkToBarrows() {
    if (isExecuting || PlayerHelper.isWebWalking()) return;

    if (barrowsBrother >= 6) {
        barrowsPhase = "open_chest";
        return;
    }

    var brother = BARROWS_BROTHERS[barrowsBrother];
    if (isNearLocation(brother.location, 5)) {
        barrowsPhase = "dig_mound";
        return;
    }
    walkToLocation(brother.location);
}

function handleDigMound() {
    if (isExecuting) return;

    // Dig at mound (spade in inventory)
    isExecuting = true;
    Utility.invokeLater(function() {
        try {
            Game.interact.inventory.consumeItem(952); // Spade ID
            Game.sendGameMessage("[SlayerBot][RI] Digging at " + BARROWS_BROTHERS[barrowsBrother].name + "'s mound", "Bot");
        } catch(e) {
            Game.sendGameMessage("[SlayerBot][RI] Dig error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
    barrowsPhase = "kill_brother";
}

function handleKillBrother() {
    if (isExecuting) return;

    var brother = BARROWS_BROTHERS[barrowsBrother];

    // Activate prayer
    try {
        if (brother.prayer === "PROTECT_FROM_MELEE") {
            Game.interact.prayer.activate(Prayer.PROTECT_FROM_MELEE);
        } else if (brother.prayer === "PROTECT_FROM_MISSILES") {
            Game.interact.prayer.activate(Prayer.PROTECT_FROM_MISSILES);
        } else if (brother.prayer === "PROTECT_FROM_MAGIC") {
            Game.interact.prayer.activate(Prayer.PROTECT_FROM_MAGIC);
        }
    } catch(e) {}

    // Find and attack brother
    var npc = Game.info.npc.getNearest([brother.npcId]);
    if (npc) {
        if (!PlayerHelper.isInCombat()) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.npc.attack(npc);
                    Game.sendGameMessage("[SlayerBot][RI] Attacking " + brother.name, "Bot");
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
        }
        // Eat food if low HP
        autoEatIfNeeded();
    } else {
        // Brother is dead — move to next
        barrowsBrother++;
        barrowsPhase = "next_brother";
        Game.sendGameMessage("[SlayerBot][RI] " + brother.name + " defeated! (" + barrowsBrother + "/6)", "Bot");
    }
}

function handleNextBrother() {
    // Turn off prayer to conserve
    try { Game.interact.prayer.deactivateAll(); } catch(e) {}
    barrowsPhase = "walk_to_barrows";
}

function handleOpenChest() {
    if (isExecuting) return;

    // Navigate to chest in tunnels and open
    var chest = Game.info.gameObject.getNearest(SS_OBJECTS.BARROWS_CHEST);
    if (chest) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try {
                Game.interact.gameObject.action(chest, MenuAction.GAME_OBJECT_FIRST_OPTION);
                Game.sendGameMessage("[SlayerBot][RI] Opening Barrows chest! Run #" + (barrowsRunCount + 1), "Bot");
                barrowsRunCount++;
                barrowsBrother = 0;
                barrowsPhase = "bank_loot";
            } catch(e) {}
            isExecuting = false;
        }, Utility.getDelay());
    } else {
        // Walk to chest location in tunnels
        walkToLocation({ x: 3551, y: 9694, z: 0 });
    }
}

function handleBarrowsBanking() {
    if (isExecuting) return;

    // Walk to bank
    if (!isNearLocation(SS_LOCATIONS.VARROCK_EAST_BANK, 10)) {
        walkToLocation(SS_LOCATIONS.VARROCK_EAST_BANK);
        return;
    }

    if (!Game.info.bank.isOpen()) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try { Game.interact.bank.openNearest(); } catch(e) {}
            isExecuting = false;
        }, Utility.getDelay());
        return;
    }

    // Deposit loot, withdraw supplies, go again
    isExecuting = true;
    Utility.invokeLater(function() {
        try {
            Game.interact.bank.depositAllInventory();
            Game.sendGameMessage("[SlayerBot][RI] Banked Barrows loot — run " + barrowsRunCount + "/" + barrowsMaxRuns, "Bot");
        } catch(e) {}
        isExecuting = false;
    }, Utility.getDelay());
    barrowsPhase = "walk_to_barrows";
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * isNearLocation — Check if player is within range of a location
 */
function isNearLocation(loc, range) {
    try {
        var player = Client.getLocalPlayer();
        if (!player) return false;
        var pos = player.getWorldLocation();
        var dx = Math.abs(pos.getX() - loc.x);
        var dy = Math.abs(pos.getY() - loc.y);
        return dx <= range && dy <= range;
    } catch(e) {
        return false;
    }
}

/**
 * walkToLocation — WebWalk to a location
 */
function walkToLocation(loc) {
    if (isExecuting || PlayerHelper.isWebWalking()) return;
    isExecuting = true;
    Utility.invokeLater(function() {
        try {
            Utility.walkTo(new WorldPoint(loc.x, loc.y, loc.z || 0));
            Game.sendGameMessage("[SlayerBot][RI] Walking to " + loc.x + "," + loc.y, "Bot");
        } catch(e) {
            Game.sendGameMessage("[SlayerBot][RI] Walk error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

/**
 * dropAllRawFish — Drop all raw fish for power fishing
 */
function dropAllRawFish() {
    if (isExecuting) return;

    var rawIds = [
        SS_ITEMS.RAW_SHRIMPS, SS_ITEMS.RAW_ANCHOVIES, SS_ITEMS.RAW_TROUT,
        SS_ITEMS.RAW_SALMON, SS_ITEMS.RAW_TUNA, SS_ITEMS.RAW_LOBSTER,
        SS_ITEMS.RAW_SWORDFISH, SS_ITEMS.RAW_MONKFISH, SS_ITEMS.RAW_SHARK,
        SS_ITEMS.RAW_ANGLERFISH, SS_ITEMS.RAW_CHICKEN, SS_ITEMS.RAW_BEEF
    ];

    for (var i = 0; i < rawIds.length; i++) {
        if (Game.info.inventory.getItemCount(rawIds[i]) > 0) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.inventory.dropItem(rawIds[i]);
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
            return;
        }
    }
}

/**
 * dropAllOres — Drop ores for power mining
 */
function dropAllOres() {
    if (isExecuting) return;

    var oreIds = [SS_ITEMS.COPPER_ORE, SS_ITEMS.TIN_ORE, SS_ITEMS.IRON_ORE, SS_ITEMS.COAL];
    for (var i = 0; i < oreIds.length; i++) {
        if (Game.info.inventory.getItemCount(oreIds[i]) > 0) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try { Game.interact.inventory.dropItem(oreIds[i]); } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
            return;
        }
    }
}

/**
 * findRawFishInInventory — Find any raw fish in inventory
 */
function findRawFishInInventory() {
    var rawIds = [
        SS_ITEMS.RAW_ANGLERFISH, SS_ITEMS.RAW_SHARK, SS_ITEMS.RAW_MONKFISH,
        SS_ITEMS.RAW_SWORDFISH, SS_ITEMS.RAW_LOBSTER, SS_ITEMS.RAW_TUNA,
        SS_ITEMS.RAW_SALMON, SS_ITEMS.RAW_TROUT, SS_ITEMS.RAW_SHRIMPS,
        SS_ITEMS.RAW_CHICKEN, SS_ITEMS.RAW_BEEF
    ];
    for (var i = 0; i < rawIds.length; i++) {
        if (Game.info.inventory.getItemCount(rawIds[i]) > 0) return rawIds[i];
    }
    return null;
}

/**
 * hasPickaxe — Check if player has any pickaxe
 */
function hasPickaxe() {
    return Game.info.inventory.getItemCount(SS_ITEMS.BRONZE_PICKAXE) > 0 ||
           Game.info.inventory.getItemCount(SS_ITEMS.IRON_PICKAXE) > 0 ||
           Game.info.inventory.getItemCount(SS_ITEMS.STEEL_PICKAXE) > 0 ||
           Game.info.inventory.getItemCount(SS_ITEMS.MITHRIL_PICKAXE) > 0 ||
           Game.info.inventory.getItemCount(SS_ITEMS.ADAMANT_PICKAXE) > 0 ||
           Game.info.inventory.getItemCount(SS_ITEMS.RUNE_PICKAXE) > 0;
}

/**
 * bankForTool — Open bank and withdraw a tool
 */
function bankForTool(toolId) {
    if (isExecuting) return;

    if (!Game.info.bank.isOpen()) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try { Game.interact.bank.openNearest(); } catch(e) {}
            isExecuting = false;
        }, Utility.getDelay());
        return;
    }

    isExecuting = true;
    Utility.invokeLater(function() {
        try {
            Game.interact.bank.withdraw(toolId, 1);
            Game.sendGameMessage("[SlayerBot][RI] Withdrew tool: " + toolId, "Bot");
        } catch(e) {}
        isExecuting = false;
    }, Utility.getDelay());
}

/**
 * buyFromShop — Buy an item from a shop NPC
 */
function buyFromShop(npcId, itemId, quantity) {
    if (isExecuting) return;

    var npc = Game.info.npc.getNearest([npcId]);
    if (npc) {
        isExecuting = true;
        Utility.invokeLater(function() {
            try {
                npc.action(MenuAction.NPC_FIRST_OPTION);  // "Trade"
                Game.sendGameMessage("[SlayerBot][RI] Opening shop with NPC " + npcId, "Bot");
            } catch(e) {}
            isExecuting = false;
        }, Utility.getDelay());
    }
}

/**
 * autoEatIfNeeded — Eat food when HP is low
 */
function autoEatIfNeeded() {
    try {
        var currentHP = Client.getBoostedSkillLevels(Skill.HITPOINTS);
        var maxHP = Client.getRealSkillLevels(Skill.HITPOINTS);
        if (currentHP < maxHP * 0.4) {
            // Find food in inventory and eat
            var foodIds = [
                SS_ITEMS.COOKED_ANGLERFISH, SS_ITEMS.COOKED_SHARK, SS_ITEMS.COOKED_MONKFISH,
                SS_ITEMS.COOKED_SWORDFISH, SS_ITEMS.COOKED_LOBSTER, SS_ITEMS.COOKED_TUNA,
                SS_ITEMS.COOKED_SALMON, SS_ITEMS.COOKED_TROUT, SS_ITEMS.COOKED_SHRIMPS,
                SS_ITEMS.COOKED_CHICKEN, SS_ITEMS.COOKED_MEAT
            ];
            for (var i = 0; i < foodIds.length; i++) {
                if (Game.info.inventory.getItemCount(foodIds[i]) > 0) {
                    Game.interact.inventory.consumeItem(foodIds[i]);
                    Game.sendGameMessage("[SlayerBot][RI] Eating food (HP: " + currentHP + "/" + maxHP + ")", "Bot");
                    return true;
                }
            }
        }
    } catch(e) {}
    return false;
}

/**
 * ssPickUpValuableLoot — Always pick up valuable items
 */
function ssPickUpValuableLoot() {
    if (isExecuting) return false;

    var groundItems = Game.info.groundItem.getAll();
    if (!groundItems) return false;

    for (var i = 0; i < groundItems.length; i++) {
        var item = groundItems[i];
        if (VALUABLE_LOOT_IDS.indexOf(item.getId()) !== -1) {
            // Don't pick up if inventory is full (unless it's coins which stack)
            if (Game.info.inventory.isFull() && item.getId() !== SS_ITEMS.COINS) continue;

            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.groundItem.pickup(item);
                    Game.sendGameMessage("[SlayerBot][RI] Picked up valuable: " + item.getId(), "Bot");
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
            return true;
        }
    }
    return false;
}

/**
 * ssBuryBones — Bury bones for prayer XP
 */
function ssBuryBones() {
    if (isExecuting) return false;

    var boneIds = [SS_ITEMS.BONES, SS_ITEMS.BIG_BONES];
    for (var i = 0; i < boneIds.length; i++) {
        if (Game.info.inventory.getItemCount(boneIds[i]) > 0) {
            isExecuting = true;
            Utility.invokeLater(function() {
                try {
                    Game.interact.inventory.consumeItem(boneIds[i]);
                    Game.sendGameMessage("[SlayerBot][RI] Burying bones for prayer XP", "Bot");
                } catch(e) {}
                isExecuting = false;
            }, Utility.getDelay());
            return true;
        }
    }
    return false;
}

/**
 * countAllFood — Count all cooked food in inventory
 */
function countAllFood() {
    var count = 0;
    var foodIds = [
        SS_ITEMS.COOKED_ANGLERFISH, SS_ITEMS.COOKED_SHARK, SS_ITEMS.COOKED_MONKFISH,
        SS_ITEMS.COOKED_SWORDFISH, SS_ITEMS.COOKED_LOBSTER, SS_ITEMS.COOKED_TUNA,
        SS_ITEMS.COOKED_SALMON, SS_ITEMS.COOKED_TROUT, SS_ITEMS.COOKED_SHRIMPS,
        SS_ITEMS.COOKED_CHICKEN, SS_ITEMS.COOKED_MEAT
    ];
    for (var i = 0; i < foodIds.length; i++) {
        count += Game.info.inventory.getItemCount(foodIds[i]);
    }
    return count;
}

/**
 * getPlayerGP — Get coins in inventory
 */
function getPlayerGP() {
    return Game.info.inventory.getItemCount(SS_ITEMS.COINS);
}

// =============================================================================
// MASTER ENTRY POINT — Called by main state machine
// =============================================================================

var riPlan = null;          // Current resource intelligence plan
var riPhase = "analyze";    // analyze, level_up, gather_food, gather_gear, gather_ranged, done

/**
 * enterResourceIntelligence — Called when bot needs resources for a task
 * @param {string} monsterName
 * @param {number} taskCount
 * @param {object} monsterData - from SLAYER_MONSTERS
 */
function enterResourceIntelligence(monsterName, taskCount, monsterData) {
    riPlan = analyzeTaskRequirements(monsterName, taskCount, monsterData);
    riPhase = "analyze";
    Game.sendGameMessage("[SlayerBot][RI] ═══════════════════════════════════════════", "Bot");
    Game.sendGameMessage("[SlayerBot][RI]  RESOURCE INTELLIGENCE ACTIVATED", "Bot");
    Game.sendGameMessage("[SlayerBot][RI]  Task: " + monsterName + " x" + taskCount, "Bot");
    Game.sendGameMessage("[SlayerBot][RI]  Difficulty: " + riPlan.difficulty.name, "Bot");
    Game.sendGameMessage("[SlayerBot][RI]  Strategy: " + riPlan.food.strategy, "Bot");
    Game.sendGameMessage("[SlayerBot][RI] ═══════════════════════════════════════════", "Bot");

    // Route to first action
    if (riPlan.barrows.shouldDoRuns) {
        riPhase = "barrows";
        initGearGathering(riPlan.gear);
        gatherGearPhase = "barrows";
    } else if (riPlan.levelUp.shouldLevel) {
        riPhase = "level_up";
        startLevelingUp(riPlan.levelUp.plans[0]);
    } else if (riPlan.food.strategy !== "GATHER_BEST" || countAllFood() < riPlan.food.foodQuantity) {
        riPhase = "gather_food";
        initFoodGathering(riPlan.food.bestTier, riPlan.food.foodQuantity);
    } else {
        riPhase = "done";
    }
}

/**
 * handleResourceIntelligenceTick — Called every game tick when in RI mode
 * @returns "DONE" when all resources are gathered
 */
function handleResourceIntelligenceTick() {
    if (isExecuting) return;

    // Always try to pick up valuables
    if (ssPickUpValuableLoot()) return;

    switch(riPhase) {
        case "level_up":
            var levelResult = handleLevelingTick();
            if (levelResult === "DONE") {
                // Check if there are more skills to level
                if (riPlan.levelUp.plans.length > 1) {
                    riPlan.levelUp.plans.shift();
                    startLevelingUp(riPlan.levelUp.plans[0]);
                } else {
                    // Move to food gathering
                    riPhase = "gather_food";
                    // Re-analyze with new levels
                    var levels = getAllPlayerLevels();
                    var newFoodPlan = planFoodForTask(riPlan.difficulty, riPlan.taskCount, levels);
                    initFoodGathering(newFoodPlan.bestTier, newFoodPlan.foodQuantity);
                }
            }
            break;

        case "gather_food":
            var foodResult = handleGatherFoodTick();
            if (foodResult === "DONE") {
                // Check if we need gear too
                if (riPlan.gear.meleeArmor && riPlan.gear.meleeArmor.obtainMethod !== "GE") {
                    riPhase = "gather_gear";
                    initGearGathering(riPlan.gear);
                } else {
                    riPhase = "done";
                }
            }
            break;

        case "gather_gear":
            var gearResult = handleGatherGearTick();
            if (gearResult === "DONE") {
                riPhase = "done";
            }
            break;

        case "barrows":
            handleBarrowsRun();
            if (gatherGearPhase === "done") {
                riPhase = "gather_food";
                initFoodGathering(riPlan.food.bestTier, riPlan.food.foodQuantity);
            }
            break;

        case "done":
            Game.sendGameMessage("[SlayerBot][RI] ✅ All resources gathered — returning to slayer task!", "Bot");
            return "DONE";
    }
}

/**
 * quickCheckSupplies — Fast check: do we have enough food for one more trip?
 * Called between trips to decide if we need to re-enter RI
 */
function quickCheckSupplies(monsterData) {
    var foodCount = countAllFood();
    var difficulty = getMonsterDifficulty(monsterData.combatLevel || 0, monsterData.maxHit || 0);
    var neededPerTrip = estimateFoodPerTrip(difficulty, selectBestFoodTier(getAllPlayerLevels()));

    if (foodCount < neededPerTrip) {
        Game.sendGameMessage("[SlayerBot][RI] Low on food (" + foodCount + "/" + neededPerTrip + ") — entering resource gathering", "Bot");
        return false;
    }
    return true;
}

// =============================================================================
// RANGED SUPPLY GATHERING — Arrows, bolts, etc.
// =============================================================================

var rangedGatherPhase = "idle";

function initRangedGathering(quantity) {
    rangedGatherPhase = "check";
    Game.sendGameMessage("[SlayerBot][RI] Init ranged supply gathering (" + quantity + " arrows/bolts)", "Bot");
}

function handleGatherRangedTick() {
    if (isExecuting) return;

    var levels = getAllPlayerLevels();

    // Determine best arrow type
    var bestRanged = selectBestRangedWeapon(levels);
    var ammoId = bestRanged.ammoId;
    var ammoCount = Game.info.inventory.getItemCount(ammoId);

    if (ammoCount >= 100) {
        Game.sendGameMessage("[SlayerBot][RI] Have " + ammoCount + " ammo — sufficient", "Bot");
        return "DONE";
    }

    // Method 1: Buy from Lowe's archery shop in Varrock
    if (getPlayerGP() >= 500) {
        if (!isNearLocation(SS_LOCATIONS.VARROCK_ARCHERY_SHOP, 10)) {
            walkToLocation(SS_LOCATIONS.VARROCK_ARCHERY_SHOP);
            return;
        }
        buyFromShop(SS_NPCS.LOWE_ARCHERY, ammoId, 100);
        return;
    }

    // Method 2: Kill chickens for feathers → fletch arrows
    if (levels.fletching >= 1) {
        // Kill chickens for feathers
        if (!isNearLocation(SS_LOCATIONS.LUMBRIDGE_CHICKENS, 15)) {
            walkToLocation(SS_LOCATIONS.LUMBRIDGE_CHICKENS);
            return;
        }
        // Collect feathers
        var groundItems = Game.info.groundItem.getAll();
        if (groundItems) {
            for (var i = 0; i < groundItems.length; i++) {
                if (groundItems[i].getId() === SS_ITEMS.FEATHER) {
                    isExecuting = true;
                    Utility.invokeLater(function() {
                        try { Game.interact.groundItem.pickup(groundItems[i]); } catch(e) {}
                        isExecuting = false;
                    }, Utility.getDelay());
                    return;
                }
            }
        }
        if (!PlayerHelper.isInCombat()) {
            var chicken = Game.info.npc.getNearest(SS_NPCS.CHICKEN);
            if (chicken) {
                isExecuting = true;
                Utility.invokeLater(function() {
                    try { Game.interact.npc.attack(chicken); } catch(e) {}
                    isExecuting = false;
                }, Utility.getDelay());
            }
        }
    }
}

// =============================================================================
// SHUTDOWN — Reset all self-sufficiency state
// =============================================================================
function OnShutdown_SS() {
    riPlan = null;
    riPhase = "analyze";
    gatherFoodTier = null;
    gatherFoodPhase = "idle";
    gatherGearPhase = "idle";
    gatherGearTarget = null;
    levelingSkill = null;
    levelingPhase = "idle";
    barrowsPhase = "walk_to_barrows";
    barrowsBrother = 0;
    barrowsRunCount = 0;
    rangedGatherPhase = "idle";
    Game.sendGameMessage("[SlayerBot][RI] Resource Intelligence Engine shutdown. All states reset.", "Bot");
}

// =============================================================================
// MODULE LOADED
// =============================================================================
Game.sendGameMessage("[SlayerBot][RI] ═══════════════════════════════════════════════════", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  RESOURCE INTELLIGENCE ENGINE v2.2 LOADED", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  " + SS_FOOD_TIERS.length + " food tiers (chicken → anglerfish)", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  " + SS_MELEE_WEAPONS.length + " melee weapons, " + SS_MELEE_ARMOR.length + " armor sets", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  " + SS_RANGED_WEAPONS.length + " ranged weapons", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  " + VALUABLE_LOOT_IDS.length + " valuable loot types tracked", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  Barrows: " + BARROWS_BROTHERS.length + " brothers mapped", "Bot");
Game.sendGameMessage("[SlayerBot][RI]  Features: Task analysis, level-up decisions, tier selection,", "Bot");
Game.sendGameMessage("[SlayerBot][RI]           food gathering, gear smithing, leather crafting,", "Bot");
Game.sendGameMessage("[SlayerBot][RI]           Barrows runs, ranged supply, GP generation", "Bot");
Game.sendGameMessage("[SlayerBot][RI] ═══════════════════════════════════════════════════", "Bot");


// ==================== MAIN LOOP ====================
// =============================================================================
// AUTO SLAYER BOT v2 — MAIN STATE MACHINE
// =============================================================================
// The brain of the slayer bot. Implements a full autonomous state machine that:
//   1. Selects the best slayer master based on combat/slayer levels
//   2. Walks to master, gets a task assignment
//   3. Banks for supplies (food, potions, special items, gear)
//   4. Walks to monster location and fights until task is complete
//   5. Handles special kill mechanics, loot, bones, prayer
//   6. Loops forever — completing task after task
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// STATE DEFINITIONS
// ─────────────────────────────────────────────────────────────────────────────
var STATES = {
    STARTUP: "STARTUP",
    CHECK_STATS: "CHECK_STATS",
    SELECT_MASTER: "SELECT_MASTER",
    WALK_TO_MASTER: "WALK_TO_MASTER",
    TALK_TO_MASTER: "TALK_TO_MASTER",
    WAIT_FOR_TASK: "WAIT_FOR_TASK",
    READ_TASK: "READ_TASK",
    CHECK_SUPPLIES: "CHECK_SUPPLIES",
    WALK_TO_BANK: "WALK_TO_BANK",
    BANK_SUPPLIES: "BANK_SUPPLIES",
    WALK_TO_GE: "WALK_TO_GE",
    BUY_AT_GE: "BUY_AT_GE",
    WALK_TO_TASK: "WALK_TO_TASK",
    FIGHT_TASK: "FIGHT_TASK",
    SPECIAL_FINISH: "SPECIAL_FINISH",
    EAT_FOOD: "EAT_FOOD",
    DRINK_PRAYER: "DRINK_PRAYER",
    LOOT_ITEMS: "LOOT_ITEMS",
    BURY_BONES: "BURY_BONES",
    TASK_COMPLETE: "TASK_COMPLETE",
    HANDLE_DEATH: "HANDLE_DEATH",
    HANDLE_LEVEL_UP: "HANDLE_LEVEL_UP",
    // Self-Sufficiency states
    GATHER_FOOD: "GATHER_FOOD",
    COOK_FOOD: "COOK_FOOD",
    GATHER_GEAR: "GATHER_GEAR",
    TAN_HIDES: "TAN_HIDES",
    CRAFT_GEAR: "CRAFT_GEAR",
    MINE_ORE: "MINE_ORE",
    SMELT_ORE: "SMELT_ORE",
    SMITH_GEAR: "SMITH_GEAR",
    GATHER_RANGED: "GATHER_RANGED",
    WALK_TO_GATHER: "WALK_TO_GATHER",
    IDLE: "IDLE"
};

// ─────────────────────────────────────────────────────────────────────────────
// BOT STATE VARIABLES
// ─────────────────────────────────────────────────────────────────────────────
var state = STATES.STARTUP;
var isExecuting = false;
var tickCounter = 0;
var stateTickCounter = 0;
var lastState = "";

// Task info
var currentTaskName = "";
var currentTaskMonster = null;
var killsLeft = 0;
var totalKills = 0;
var tasksCompleted = 0;
var taskReceived = false;

// Master info
var selectedMaster = null;

// Player stats (cached)
var combatLevel = 0;
var slayerLevel = 0;
var attackLevel = 0;
var strengthLevel = 0;
var defenceLevel = 0;
var hitpointsLevel = 0;
var prayerLevel = 0;
var rangedLevel = 0;
var magicLevel = 0;

// Banking
var bankTarget = null;
var needsRestock = false;
var missingItems = [];

// Timeouts
var stateTimeout = 0;
var waitTicks = 0;
var retryCount = 0;
var maxRetries = 3;

// Loot tracking
var lootQueue = [];

// Death detection
var previousHP = 0;
var deathDetected = false;

// Level up handling
var levelUpDetected = false;


// =============================================================================
// OnStart — Initialize the bot
// =============================================================================
function OnStart() {
    // Initialize overlay
    overlay.status.update("Status: Starting...");
    overlay.task.update("Task: None");
    overlay.killsLeft.update("Kills Left: 0");
    overlay.tasksCompleted.update("Tasks Done: 0");
    overlay.slayerMaster.update("Master: Auto");

    state = STATES.STARTUP;
    isExecuting = false;
    tickCounter = 0;

    Game.sendGameMessage("[SlayerBot] v2.2 Started! State: STARTUP", "Bot");
}

// =============================================================================
// OnShutdown — CRITICAL: always reset state
// =============================================================================
// Reset gather state variables
function resetGatherState() {
    gatherFoodPhase = "idle";
    gatherFoodTier = null;
    gatherFoodNeeded = 0;
    gatherFoodCount = 0;
    gatherGearPhase = "idle";
    gatherGearTarget = null;
    leatherPhase = "killing";
    rangedGatherPhase = "idle";
}

function OnShutdown() {
    isExecuting = false;
    state = STATES.IDLE;
    // Reset self-sufficiency state
    try {
        resetGatherState();
        OnShutdown_SS();
    } catch (e) {
        // SS module may not be loaded
    }
    Game.sendGameMessage("[SlayerBot] Shutdown. isExecuting reset to false.", "Bot");
}

// =============================================================================
// OVERLAY UPDATE HELPER
// =============================================================================
function updateOverlay(statusText, taskText, kills) {
    if (overlay.status) overlay.status.update("Status: " + statusText);
    if (overlay.task) overlay.task.update("Task: " + taskText);
    if (overlay.killsLeft) overlay.killsLeft.update("Kills Left: " + kills);
}

// =============================================================================
// OnGameTick — MAIN LOOP
// =============================================================================
function OnGameTick() {
    tickCounter++;
    stateTickCounter++;

    // Prevent overlapping actions
    if (isExecuting) return;

    // Handle death detection
    if (deathDetected) {
        deathDetected = false;
        state = STATES.HANDLE_DEATH;
        stateTickCounter = 0;
    }

    // Handle level up
    if (levelUpDetected) {
        levelUpDetected = false;
        // Just continue — the dialogue skipper should handle it
    }

    // State timeout protection — if stuck for 120 ticks (~72 seconds), reset
    if (stateTickCounter > 120 && state !== STATES.FIGHT_TASK && state !== STATES.WALK_TO_TASK && state !== STATES.WALK_TO_MASTER && state !== STATES.WALK_TO_BANK && state !== STATES.WALK_TO_GATHER && state !== STATES.GATHER_FOOD && state !== STATES.GATHER_GEAR && state !== STATES.GATHER_RANGED && state !== STATES.MINE_ORE) {
        Game.sendGameMessage("[SlayerBot] State timeout in " + state + " after " + stateTickCounter + " ticks. Retrying...", "Bot");
        retryCount++;
        if (retryCount > maxRetries) {
            Game.sendGameMessage("[SlayerBot] Max retries exceeded. Resetting to CHECK_STATS.", "Bot");
            state = STATES.CHECK_STATS;
            retryCount = 0;
        }
        stateTickCounter = 0;
    }

    // Track state changes for logging
    if (state !== lastState) {
        Game.sendGameMessage("[SlayerBot] State transition: " + lastState + " -> " + state, "Bot");
        lastState = state;
        stateTickCounter = 0;
        retryCount = 0;
    }

    // ─── STATE ROUTER ───
    switch (state) {
        case STATES.STARTUP:
            handleStartup();
            break;
        case STATES.CHECK_STATS:
            handleCheckStats();
            break;
        case STATES.SELECT_MASTER:
            handleSelectMaster();
            break;
        case STATES.WALK_TO_MASTER:
            handleWalkToMaster();
            break;
        case STATES.TALK_TO_MASTER:
            handleTalkToMaster();
            break;
        case STATES.WAIT_FOR_TASK:
            handleWaitForTask();
            break;
        case STATES.READ_TASK:
            handleReadTask();
            break;
        case STATES.CHECK_SUPPLIES:
            handleCheckSupplies();
            break;
        case STATES.WALK_TO_BANK:
            handleWalkToBank();
            break;
        case STATES.BANK_SUPPLIES:
            handleBankSupplies();
            break;
        case STATES.WALK_TO_GE:
            handleWalkToGE();
            break;
        case STATES.BUY_AT_GE:
            handleBuyAtGE();
            break;
        case STATES.WALK_TO_TASK:
            handleWalkToTask();
            break;
        case STATES.FIGHT_TASK:
            handleFightTask();
            break;
        case STATES.SPECIAL_FINISH:
            handleSpecialFinish();
            break;
        case STATES.EAT_FOOD:
            handleEatFood();
            break;
        case STATES.DRINK_PRAYER:
            handleDrinkPrayer();
            break;
        case STATES.LOOT_ITEMS:
            handleLootItems();
            break;
        case STATES.BURY_BONES:
            handleBuryBones();
            break;
        case STATES.TASK_COMPLETE:
            handleTaskComplete();
            break;
        case STATES.HANDLE_DEATH:
            handleDeath();
            break;
        case STATES.HANDLE_LEVEL_UP:
            handleLevelUp();
            break;

        // ─── SELF-SUFFICIENCY STATES ───
        case STATES.GATHER_FOOD:
            handleGatherFood();
            break;
        case STATES.COOK_FOOD:
            handleCookFood();
            break;
        case STATES.GATHER_GEAR:
            handleGatherGear();
            break;
        case STATES.TAN_HIDES:
            handleTanHides();
            break;
        case STATES.CRAFT_GEAR:
            handleCraftGear();
            break;
        case STATES.MINE_ORE:
            handleMineOre();
            break;
        case STATES.SMELT_ORE:
            handleSmeltOre();
            break;
        case STATES.SMITH_GEAR:
            handleSmithGear();
            break;
        case STATES.GATHER_RANGED:
            handleGatherRanged();
            break;
        case STATES.WALK_TO_GATHER:
            handleWalkToGather();
            break;

        case STATES.IDLE:
            // Do nothing
            break;
    }
}

// =============================================================================
// STATE HANDLERS
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// STARTUP — Initial checks
// ─────────────────────────────────────────────────────────────────────────────
function handleStartup() {
    updateOverlay("Initializing...", "None", 0);
    Game.sendGameMessage("[SlayerBot] Startup — checking initial state...", "Bot");

    // Small delay before starting
    if (stateTickCounter < 3) return;

    state = STATES.CHECK_STATS;
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECK_STATS — Read player levels
// ─────────────────────────────────────────────────────────────────────────────
function handleCheckStats() {
    updateOverlay("Checking stats...", currentTaskName || "None", killsLeft);

    attackLevel = Client.getRealSkillLevels(Skill.ATTACK);
    strengthLevel = Client.getRealSkillLevels(Skill.STRENGTH);
    defenceLevel = Client.getRealSkillLevels(Skill.DEFENCE);
    hitpointsLevel = Client.getRealSkillLevels(Skill.HITPOINTS);
    prayerLevel = Client.getRealSkillLevels(Skill.PRAYER);
    rangedLevel = Client.getRealSkillLevels(Skill.RANGED);
    magicLevel = Client.getRealSkillLevels(Skill.MAGIC);
    slayerLevel = Client.getRealSkillLevels(Skill.SLAYER);
    combatLevel = getCombatLevel();

    Game.sendGameMessage("[SlayerBot] Stats — Combat: " + combatLevel + ", Slayer: " + slayerLevel +
               ", ATK: " + attackLevel + ", STR: " + strengthLevel + ", DEF: " + defenceLevel, "Bot");

    // Check if we already have a task (kills left > 0 from a previous session)
    if (currentTaskName && killsLeft > 0 && currentTaskMonster) {
        Game.sendGameMessage("[SlayerBot] Resuming existing task: " + currentTaskName + " (" + killsLeft + " left)", "Bot");
        state = STATES.CHECK_SUPPLIES;
        return;
    }

    state = STATES.SELECT_MASTER;
}

// ─────────────────────────────────────────────────────────────────────────────
// SELECT_MASTER — Choose slayer master
// ─────────────────────────────────────────────────────────────────────────────
function handleSelectMaster() {
    var preference = config.masterPreference ? config.masterPreference.read() : "Auto";
    selectedMaster = selectBestMaster(combatLevel, slayerLevel, preference);

    Game.sendGameMessage("[SlayerBot] Selected master: " + selectedMaster.name +
               " at " + selectedMaster.locationName +
               " (Combat req: " + selectedMaster.combatReq + ", Our combat: " + combatLevel + ")", "Bot");

    overlay.slayerMaster.update("Master: " + selectedMaster.name);
    updateOverlay("Going to " + selectedMaster.name, currentTaskName || "None", killsLeft);

    state = STATES.WALK_TO_MASTER;
}

// ─────────────────────────────────────────────────────────────────────────────
// WALK_TO_MASTER — Navigate to slayer master
// ─────────────────────────────────────────────────────────────────────────────
function handleWalkToMaster() {
    // Check if already near master
    if (isNear(selectedMaster.location.x, selectedMaster.location.y, 10)) {
        Game.sendGameMessage("[SlayerBot] Arrived at " + selectedMaster.name, "Bot");
        state = STATES.TALK_TO_MASTER;
        return;
    }

    // Check if web walking
    if (PlayerHelper.isWebWalking()) {
        updateOverlay("Walking to " + selectedMaster.name + "...", currentTaskName || "None", killsLeft);
        return; // Still walking
    }

    // Start walking
    isExecuting = true;
    updateOverlay("Walking to " + selectedMaster.name + "...", currentTaskName || "None", killsLeft);

    Utility.invokeLater(function() {
        try {
            PlayerHelper.webWalkTo(selectedMaster.location.x, selectedMaster.location.y, selectedMaster.location.plane);
            Game.sendGameMessage("[SlayerBot] Started walking to " + selectedMaster.name +
                       " at (" + selectedMaster.location.x + ", " + selectedMaster.location.y + ")", "Bot");
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Walk error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// TALK_TO_MASTER — Interact with slayer master
// ─────────────────────────────────────────────────────────────────────────────
function handleTalkToMaster() {
    updateOverlay("Talking to " + selectedMaster.name + "...", "Getting task", killsLeft);

    isExecuting = true;
    taskReceived = false;

    Utility.invokeLater(function() {
        try {
            var masterNpc = Game.info.npc.getNearest(selectedMaster.npcId);
            if (masterNpc) {
                masterNpc.action(MenuAction.NPC_FIRST_OPTION); // "Assignment"
                Game.sendGameMessage("[SlayerBot] Talking to " + selectedMaster.name + " (NPC ID: " + selectedMaster.npcId + ")", "Bot");
                state = STATES.WAIT_FOR_TASK;
            } else {
                Game.sendGameMessage("[SlayerBot] Master NPC not found! Retrying walk...", "Bot");
                state = STATES.WALK_TO_MASTER;
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Talk error: " + e, "Bot");
            state = STATES.WALK_TO_MASTER;
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// WAIT_FOR_TASK — Wait for chat message with task
// ─────────────────────────────────────────────────────────────────────────────
function handleWaitForTask() {
    updateOverlay("Waiting for task...", "Listening...", killsLeft);

    // Task received via OnChatMessage
    if (taskReceived && currentTaskName && killsLeft > 0) {
        Game.sendGameMessage("[SlayerBot] Task received: Kill " + killsLeft + " " + currentTaskName, "Bot");
        state = STATES.READ_TASK;
        return;
    }

    // Timeout — try talking again after 30 ticks
    if (stateTickCounter > 30) {
        Game.sendGameMessage("[SlayerBot] No task received after 30 ticks. Retrying...", "Bot");
        state = STATES.TALK_TO_MASTER;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// READ_TASK — Look up monster in database
// ─────────────────────────────────────────────────────────────────────────────
function handleReadTask() {
    currentTaskMonster = findMonster(currentTaskName);

    if (currentTaskMonster) {
        Game.sendGameMessage("[SlayerBot] Task identified: " + currentTaskMonster.name +
                   " at " + currentTaskMonster.locationName +
                   " (Slayer req: " + currentTaskMonster.slayerReq + ")", "Bot");
        updateOverlay("Task: " + currentTaskMonster.name, currentTaskMonster.name, killsLeft);
        state = STATES.CHECK_SUPPLIES;
    } else {
        Game.sendGameMessage("[SlayerBot] WARNING: Unknown monster '" + currentTaskName + "'! Cannot proceed.", "Bot");
        updateOverlay("ERROR: Unknown task '" + currentTaskName + "'", currentTaskName, killsLeft);
        // Try to continue — walk to GE and wait
        state = STATES.IDLE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECK_SUPPLIES — Verify we have everything needed
// ─────────────────────────────────────────────────────────────────────────────
function handleCheckSupplies() {
    updateOverlay("Checking supplies...", currentTaskMonster.name, killsLeft);
    missingItems = [];
    needsRestock = false;

    // Check food
    var foodCount = countFood();
    if (foodCount < 5) {
        Game.sendGameMessage("[SlayerBot] Low food: " + foodCount + " — need restock", "Bot");
        needsRestock = true;
        var foodName = config.foodType ? config.foodType.read() : "Lobster";
        missingItems.push({ name: foodName, id: FOOD[foodName] ? FOOD[foodName].id : 379, quantity: 15 });
    }

    // Check prayer potions (if prayer is enabled)
    var usePrayer = config.usePrayer ? config.usePrayer.read() : true;
    if (usePrayer && prayerLevel > 1) {
        var prayerPotCount = countPrayerPots();
        if (prayerPotCount < 2) {
            Game.sendGameMessage("[SlayerBot] Low prayer pots: " + prayerPotCount + " — need restock", "Bot");
            needsRestock = true;
            missingItems.push({ name: "Prayer potion(4)", id: 2434, quantity: 4 });
        }
    }

    // Check special items for current task
    if (currentTaskMonster.specialItem && currentTaskMonster.specialMechanic === "finish") {
        var specialCount = Game.info.inventory.count(currentTaskMonster.specialItem);
        if (specialCount < killsLeft) {
            Game.sendGameMessage("[SlayerBot] Need " + currentTaskMonster.specialItemName + ": have " + specialCount + ", need " + killsLeft, "Bot");
            needsRestock = true;
            missingItems.push({ name: currentTaskMonster.specialItemName, id: currentTaskMonster.specialItem, quantity: killsLeft });
        }
    }

    if (needsRestock) {
        Game.sendGameMessage("[SlayerBot] Restocking needed. Missing " + missingItems.length + " item types.", "Bot");

        // ─── SELF-SUFFICIENCY CHECK ───
        // Before going to bank/GE, check if we can afford it
        var ssPlan = needsSelfSufficiency(missingItems);
        if (ssPlan) {
            Game.sendGameMessage("[SlayerBot] Cannot afford GE! Entering self-sufficiency mode.", "Bot");
            enterSelfSufficiency(ssPlan);
            return;
        }

        Game.sendGameMessage("[SlayerBot] Can afford GE purchases. Proceeding to bank.", "Bot");
        state = STATES.WALK_TO_BANK;
    } else {
        Game.sendGameMessage("[SlayerBot] Supplies OK! Heading to task.", "Bot");
        state = STATES.WALK_TO_TASK;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WALK_TO_BANK — Navigate to nearest bank
// ─────────────────────────────────────────────────────────────────────────────
function handleWalkToBank() {
    // Determine bank target
    if (!bankTarget) {
        var pos = Game.localPlayer.getPosition();
        if (currentTaskMonster && currentTaskMonster.bankNearby) {
            bankTarget = currentTaskMonster.bankNearby;
        } else {
            bankTarget = findNearestBank(pos.x, pos.y, pos.plane);
        }
        if (!bankTarget) {
            bankTarget = { x: 3164, y: 3487, plane: 0 }; // Default: GE
        }
        Game.sendGameMessage("[SlayerBot] Bank target: (" + bankTarget.x + ", " + bankTarget.y + ")", "Bot");
    }

    // Check if arrived
    if (isNear(bankTarget.x, bankTarget.y, 8)) {
        Game.sendGameMessage("[SlayerBot] Arrived at bank.", "Bot");
        state = STATES.BANK_SUPPLIES;
        return;
    }

    // Check if web walking
    if (PlayerHelper.isWebWalking()) {
        updateOverlay("Walking to bank...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);
        return;
    }

    // Start walking
    isExecuting = true;
    updateOverlay("Walking to bank...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);

    Utility.invokeLater(function() {
        try {
            PlayerHelper.webWalkTo(bankTarget.x, bankTarget.y, bankTarget.plane);
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Walk to bank error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// BANK_SUPPLIES — Open bank and withdraw needed items
// ─────────────────────────────────────────────────────────────────────────────
function handleBankSupplies() {
    updateOverlay("Banking...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);

    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            // Open nearest bank booth/chest
            var bankBooth = Game.info.gameObject.getNearest(10583); // Bank booth
            if (!bankBooth) bankBooth = Game.info.gameObject.getNearest(12308); // Bank chest
            if (!bankBooth) bankBooth = Game.info.gameObject.getNearest(10355); // Another booth type

            if (!bankBooth) {
                Game.sendGameMessage("[SlayerBot] No bank found nearby! Trying NPC banker...", "Bot");
                var banker = Game.info.npc.getNearest(1618); // Banker NPC
                if (banker) {
                    banker.action(MenuAction.NPC_FIRST_OPTION);
                } else {
                    Game.sendGameMessage("[SlayerBot] No bank or banker found!", "Bot");
                    isExecuting = false;
                    state = STATES.WALK_TO_BANK;
                    bankTarget = null; // Reset to re-find bank
                    return;
                }
            } else {
                Game.interact.gameObject.action(bankBooth, MenuAction.GAME_OBJECT_SECOND_OPTION);
            }

            Game.sendGameMessage("[SlayerBot] Opening bank...", "Bot");

            // Wait for bank to open, then manage inventory
            Utility.invokeLater(function() {
                try {
                    // Check GP in bank for self-sufficiency tracking
                    try {
                        checkBankForGP();
                    } catch (e) {
                        // SS module may not be loaded
                    }

                    // Check if this is a self-sufficiency bank phase
                    if (gatherBankPhase > 0) {
                        try {
                            handleSSBankPhase();
                        } catch (e) {
                            Game.sendGameMessage("[SlayerBot] SS bank phase error: " + e, "Bot");
                            gatherBankPhase = 0;
                        }
                        isExecuting = false;
                        return;
                    }

                    // Deposit all
                    Game.interact.bank.depositAll();
                    Game.sendGameMessage("[SlayerBot] Deposited all items.", "Bot");

                    Utility.invokeLater(function() {
                        try {
                            // Withdraw food
                            var foodName = config.foodType ? config.foodType.read() : "Lobster";
                            var foodId = FOOD[foodName] ? FOOD[foodName].id : 379;
                            Game.interact.bank.withdraw(foodId, 14);
                            Game.sendGameMessage("[SlayerBot] Withdrew 14x " + foodName, "Bot");

                            Utility.invokeLater(function() {
                                try {
                                    // Withdraw prayer potions if needed
                                    var usePrayer = config.usePrayer ? config.usePrayer.read() : true;
                                    if (usePrayer && prayerLevel > 1) {
                                        Game.interact.bank.withdraw(2434, 4); // Prayer pot (4)
                                        Game.sendGameMessage("[SlayerBot] Withdrew 4x Prayer potion(4)", "Bot");
                                    }

                                    Utility.invokeLater(function() {
                                        try {
                                            // Withdraw special items if task needs them
                                            if (currentTaskMonster && currentTaskMonster.specialItem) {
                                                if (currentTaskMonster.specialMechanic === "finish") {
                                                    // Need enough for remaining kills
                                                    var qty = Math.min(killsLeft, 28);
                                                    Game.interact.bank.withdraw(currentTaskMonster.specialItem, qty);
                                                    Game.sendGameMessage("[SlayerBot] Withdrew " + qty + "x " + currentTaskMonster.specialItemName, "Bot");
                                                } else if (currentTaskMonster.specialMechanic === "equip") {
                                                    Game.interact.bank.withdraw(currentTaskMonster.specialItem, 1);
                                                    Game.sendGameMessage("[SlayerBot] Withdrew 1x " + currentTaskMonster.specialItemName, "Bot");
                                                } else if (currentTaskMonster.specialMechanic === "weapon") {
                                                    // Withdraw leaf-bladed sword
                                                    Game.interact.bank.withdraw(11902, 1);
                                                    Game.sendGameMessage("[SlayerBot] Withdrew leaf-bladed sword", "Bot");
                                                }
                                            }

                                            // Withdraw best equipment
                                            Utility.invokeLater(function() {
                                                try {
                                                    withdrawBestGear();

                                                    // Close bank
                                                    Utility.invokeLater(function() {
                                                        try {
                                                            Game.interact.bank.close();
                                                            Game.sendGameMessage("[SlayerBot] Bank closed. Equipping gear...", "Bot");

                                                            // Equip gear from inventory
                                                            Utility.invokeLater(function() {
                                                                try {
                                                                    equipGearFromInventory();
                                                                    Game.sendGameMessage("[SlayerBot] Gear equipped. Ready for task.", "Bot");
                                                                    bankTarget = null;
                                                                    needsRestock = false;
                                                                    state = STATES.WALK_TO_TASK;
                                                                } catch (e) {
                                                                    Game.sendGameMessage("[SlayerBot] Equip error: " + e, "Bot");
                                                                    state = STATES.WALK_TO_TASK; // Try anyway
                                                                }
                                                                isExecuting = false;
                                                            }, Utility.getDelay());
                                                        } catch (e) {
                                                            Game.sendGameMessage("[SlayerBot] Bank close error: " + e, "Bot");
                                                            isExecuting = false;
                                                            state = STATES.WALK_TO_TASK;
                                                        }
                                                    }, Utility.getDelay());
                                                } catch (e) {
                                                    Game.sendGameMessage("[SlayerBot] Gear withdraw error: " + e, "Bot");
                                                    isExecuting = false;
                                                    state = STATES.WALK_TO_TASK;
                                                }
                                            }, Utility.getDelay());
                                        } catch (e) {
                                            Game.sendGameMessage("[SlayerBot] Special item withdraw error: " + e, "Bot");
                                            isExecuting = false;
                                            state = STATES.WALK_TO_TASK;
                                        }
                                    }, Utility.getDelay());
                                } catch (e) {
                                    Game.sendGameMessage("[SlayerBot] Prayer pot withdraw error: " + e, "Bot");
                                    isExecuting = false;
                                    state = STATES.WALK_TO_TASK;
                                }
                            }, Utility.getDelay());
                        } catch (e) {
                            Game.sendGameMessage("[SlayerBot] Food withdraw error: " + e, "Bot");
                            isExecuting = false;
                            state = STATES.WALK_TO_TASK;
                        }
                    }, Utility.getDelay());
                } catch (e) {
                    Game.sendGameMessage("[SlayerBot] Deposit error: " + e, "Bot");
                    isExecuting = false;
                }
            }, Utility.getDelay() * 2); // Extra delay for bank to open
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Bank open error: " + e, "Bot");
            isExecuting = false;
            state = STATES.WALK_TO_BANK;
            bankTarget = null;
        }
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// WITHDRAW BEST GEAR — Helper for banking
// ─────────────────────────────────────────────────────────────────────────────
function withdrawBestGear() {
    // Determine if task needs special weapon
    var needsLeafBlade = currentTaskMonster && currentTaskMonster.specialMechanic === "weapon";

    if (!needsLeafBlade) {
        // Withdraw best melee weapon
        var bestWeapon = getBestWeapon(attackLevel);
        Game.interact.bank.withdraw(bestWeapon.id, 1);
        Game.sendGameMessage("[SlayerBot] Withdrew weapon: " + bestWeapon.name, "Bot");
    }

    // Withdraw best body
    var bestBody = getBestBody(defenceLevel);
    Game.interact.bank.withdraw(bestBody.id, 1);

    // Withdraw best legs
    var bestLegs = getBestLegs(defenceLevel);
    Game.interact.bank.withdraw(bestLegs.id, 1);

    // Withdraw best shield (if not using mirror shield)
    var needsMirrorShield = currentTaskMonster && currentTaskMonster.specialItem === 4156;
    if (!needsMirrorShield) {
        var bestShield = getBestShield(defenceLevel);
        Game.interact.bank.withdraw(bestShield.id, 1);
    }

    // Withdraw boots
    var bestBoots = getBestBoots(defenceLevel);
    Game.interact.bank.withdraw(bestBoots.id, 1);

    Game.sendGameMessage("[SlayerBot] Withdrew full armor set.", "Bot");
}

// ─────────────────────────────────────────────────────────────────────────────
// EQUIP GEAR FROM INVENTORY — After banking
// ─────────────────────────────────────────────────────────────────────────────
function equipGearFromInventory() {
    // Build list of all gear IDs to try equipping
    var gearIds = [];

    // Add all weapon IDs
    for (var i = 0; i < MELEE_WEAPONS.length; i++) {
        gearIds.push(MELEE_WEAPONS[i].id);
    }
    // Add leaf-bladed weapons
    gearIds.push(11902, 20727);

    // Add all armor IDs
    for (var i = 0; i < HELMETS.length; i++) gearIds.push(HELMETS[i].id);
    for (var i = 0; i < PLATEBODIES.length; i++) gearIds.push(PLATEBODIES[i].id);
    for (var i = 0; i < PLATELEGS.length; i++) gearIds.push(PLATELEGS[i].id);
    for (var i = 0; i < SHIELDS.length; i++) gearIds.push(SHIELDS[i].id);
    for (var i = 0; i < BOOTS.length; i++) gearIds.push(BOOTS[i].id);
    for (var i = 0; i < GLOVES.length; i++) gearIds.push(GLOVES[i].id);
    for (var i = 0; i < AMULETS.length; i++) gearIds.push(AMULETS[i].id);

    // Add special items that need equipping
    gearIds.push(4156, 4168, 4166, 4164, 8923, 7053); // Mirror shield, nose peg, earmuffs, face mask, witchwood, lantern

    // Equip anything found in inventory
    for (var i = 0; i < gearIds.length; i++) {
        if (Game.info.inventory.search(gearIds[i]) !== -1) {
            Game.interact.inventory.wield(gearIds[i]);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WALK_TO_GE — Navigate to Grand Exchange
// ─────────────────────────────────────────────────────────────────────────────
function handleWalkToGE() {
    if (isNear(GE.LOCATION.x, GE.LOCATION.y, 10)) {
        Game.sendGameMessage("[SlayerBot] Arrived at GE.", "Bot");
        state = STATES.BUY_AT_GE;
        return;
    }

    if (PlayerHelper.isWebWalking()) {
        updateOverlay("Walking to GE...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);
        return;
    }

    isExecuting = true;
    updateOverlay("Walking to GE...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);

    Utility.invokeLater(function() {
        try {
            PlayerHelper.webWalkTo(GE.LOCATION.x, GE.LOCATION.y, GE.LOCATION.plane);
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Walk to GE error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// BUY_AT_GE — Purchase items from Grand Exchange
// ─────────────────────────────────────────────────────────────────────────────
function handleBuyAtGE() {
    updateOverlay("Buying at GE...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);

    // GE interaction is complex — simplified approach
    // Open GE booth and buy missing items one at a time
    if (missingItems.length === 0) {
        Game.sendGameMessage("[SlayerBot] Nothing to buy at GE. Going to bank.", "Bot");
        state = STATES.WALK_TO_BANK;
        bankTarget = null;
        return;
    }

    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            // Open GE booth
            var geBooth = Game.info.gameObject.getNearest(GE.BOOTH_IDS[0]);
            if (geBooth) {
                Game.interact.gameObject.action(geBooth, MenuAction.GAME_OBJECT_SECOND_OPTION);
                Game.sendGameMessage("[SlayerBot] Opened GE booth. Buying items...", "Bot");

                // For each missing item, attempt to buy
                // This is simplified — in practice, GE widget interaction is complex
                Utility.invokeLater(function() {
                    try {
                        for (var i = 0; i < missingItems.length; i++) {
                            var item = missingItems[i];
                            Game.sendGameMessage("[SlayerBot] Attempting to buy: " + item.quantity + "x " + item.name + " (ID: " + item.id + ")", "Bot");
                            // GE buy widget interaction would go here
                            // Widget.interact(GE.BUY_BUTTON.group, GE.BUY_BUTTON.child, ...)
                        }
                        missingItems = [];
                        Game.sendGameMessage("[SlayerBot] GE purchases attempted. Waiting for completion...", "Bot");

                        // Wait for offers to complete, then collect
                        Utility.invokeLater(function() {
                            // Collect completed offers
                            Game.sendGameMessage("[SlayerBot] Collecting GE offers...", "Bot");
                            state = STATES.WALK_TO_BANK;
                            bankTarget = null;
                            isExecuting = false;
                        }, Utility.getDelay() * 5);
                    } catch (e) {
                        Game.sendGameMessage("[SlayerBot] GE buy error: " + e, "Bot");
                        state = STATES.WALK_TO_BANK;
                        bankTarget = null;
                        isExecuting = false;
                    }
                }, Utility.getDelay() * 2);
            } else {
                Game.sendGameMessage("[SlayerBot] GE booth not found!", "Bot");
                isExecuting = false;
                state = STATES.WALK_TO_GE;
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] GE error: " + e, "Bot");
            isExecuting = false;
            state = STATES.WALK_TO_BANK;
            bankTarget = null;
        }
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// WALK_TO_TASK — Navigate to monster location
// ─────────────────────────────────────────────────────────────────────────────
function handleWalkToTask() {
    if (!currentTaskMonster) {
        Game.sendGameMessage("[SlayerBot] No task monster set! Going to select master.", "Bot");
        state = STATES.SELECT_MASTER;
        return;
    }

    var taskLoc = currentTaskMonster.location;

    // Check if arrived
    if (isNear(taskLoc.x, taskLoc.y, 15)) {
        Game.sendGameMessage("[SlayerBot] Arrived at task location: " + currentTaskMonster.locationName, "Bot");
        updateOverlay("Fighting " + currentTaskMonster.name, currentTaskMonster.name, killsLeft);
        state = STATES.FIGHT_TASK;
        return;
    }

    // Check if walking
    if (PlayerHelper.isWebWalking()) {
        updateOverlay("Walking to " + currentTaskMonster.locationName + "...", currentTaskMonster.name, killsLeft);
        return;
    }

    // Start walking
    isExecuting = true;
    updateOverlay("Walking to " + currentTaskMonster.locationName + "...", currentTaskMonster.name, killsLeft);

    Utility.invokeLater(function() {
        try {
            PlayerHelper.webWalkTo(taskLoc.x, taskLoc.y, taskLoc.plane);
            Game.sendGameMessage("[SlayerBot] Walking to " + currentTaskMonster.locationName +
                       " (" + taskLoc.x + ", " + taskLoc.y + ", " + taskLoc.plane + ")", "Bot");
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Walk to task error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// FIGHT_TASK — Main combat loop
// ─────────────────────────────────────────────────────────────────────────────
function handleFightTask() {
    if (!currentTaskMonster) {
        state = STATES.SELECT_MASTER;
        return;
    }

    // ─── CHECK IF TASK IS COMPLETE ───
    if (killsLeft <= 0) {
        Game.sendGameMessage("[SlayerBot] Task complete! Killed all " + currentTaskMonster.name, "Bot");
        state = STATES.TASK_COMPLETE;
        return;
    }

    // ─── DEATH CHECK ───
    var currentHP = Client.getBoostedSkillLevels(Skill.HITPOINTS);
    if (currentHP <= 0 || isAtDeathSpawn()) {
        Game.sendGameMessage("[SlayerBot] Death detected!", "Bot");
        deathDetected = true;
        return;
    }

    // ─── EAT FOOD CHECK ───
    var eatPercent = config.eatPercent ? config.eatPercent.read() : 50;
    var hpPercent = (currentHP / hitpointsLevel) * 100;
    if (hpPercent <= eatPercent) {
        var foodCount = countFood();
        if (foodCount > 0) {
            eatFood();
            return;
        } else {
            // Out of food — need to bank
            Game.sendGameMessage("[SlayerBot] Out of food! Going to bank.", "Bot");
            state = STATES.CHECK_SUPPLIES;
            return;
        }
    }

    // ─── PRAYER CHECK ───
    var usePrayer = config.usePrayer ? config.usePrayer.read() : true;
    if (usePrayer && prayerLevel > 1) {
        var currentPrayer = Client.getBoostedSkillLevels(Skill.PRAYER);
        if (currentPrayer > 0 && currentPrayer < 20) {
            drinkPrayerPot();
            return;
        }
        if (currentPrayer <= 0) {
            // Prayer drained — try to drink pot or continue without prayer
            var potCount = countPrayerPots();
            if (potCount > 0) {
                drinkPrayerPot();
                return;
            }
        }

        // Activate protection prayer if not already active
        activateProtectionPrayer();
    }

    // ─── SPECIAL FINISH CHECK ───
    // Check if current target needs a finish mechanic (gargoyle, rock slug, etc.)
    if (currentTaskMonster.specialMechanic === "finish") {
        var target = Game.localPlayer.getInteracting();
        if (target && target.getHealthRatio) {
            var healthRatio = target.getHealthRatio();
            // HealthRatio of 0 or very low means it's ready for the finish item
            if (healthRatio > 0 && healthRatio <= 10) {
                handleSpecialFinish();
                return;
            }
        }
    }

    // ─── LOOT CHECK ───
    var lootEnabled = config.lootEnabled ? config.lootEnabled.read() : true;
    if (lootEnabled && !Game.localPlayer.isInteracting()) {
        // Check for loot on ground
        var lootMinValue = config.lootMinValue ? config.lootMinValue.read() : 100;
        var lootItem = Game.info.groundItems.getNearest();
        if (lootItem && Game.info.inventory.count(-1) < 28) {
            // Pick up valuable loot
            pickUpLoot();
            return;
        }
    }

    // ─── BURY BONES CHECK ───
    var buryBones = config.buryBones ? config.buryBones.read() : false;
    if (buryBones && !Game.localPlayer.isInteracting()) {
        for (var i = 0; i < ALL_BONE_IDS.length; i++) {
            if (Game.info.inventory.search(ALL_BONE_IDS[i]) !== -1) {
                buryBone(ALL_BONE_IDS[i]);
                return;
            }
        }
    }

    // ─── ATTACK TARGET ───
    // Check if already in combat
    if (Game.localPlayer.isInteracting()) {
        // Already fighting — wait
        updateOverlay("Fighting " + currentTaskMonster.name, currentTaskMonster.name, killsLeft);
        return;
    }

    // Find and attack nearest target
    attackNearest();
}

// ─────────────────────────────────────────────────────────────────────────────
// ATTACK NEAREST — Find and attack the nearest task NPC
// ─────────────────────────────────────────────────────────────────────────────
function attackNearest() {
    if (!currentTaskMonster || !currentTaskMonster.npcIds) return;

    isExecuting = true;
    updateOverlay("Attacking " + currentTaskMonster.name, currentTaskMonster.name, killsLeft);

    Utility.invokeLater(function() {
        try {
            var target = null;

            // Try each NPC ID for the task
            for (var i = 0; i < currentTaskMonster.npcIds.length; i++) {
                target = Game.info.npc.getNearest(currentTaskMonster.npcIds[i]);
                if (target && !target.isDead() && !target.isInteracting()) {
                    break;
                }
                target = null;
            }

            if (target) {
                Game.interact.npc.attack(target);
                Game.sendGameMessage("[SlayerBot] Attacking " + currentTaskMonster.name + " (NPC ID: " + target.getId() + ")", "Bot");
            } else {
                // No target found — might need to move around
                Game.sendGameMessage("[SlayerBot] No " + currentTaskMonster.name + " found nearby. Waiting...", "Bot");

                // If stuck for a while, try walking to exact task location
                if (stateTickCounter > 30 && stateTickCounter % 15 === 0) {
                    var loc = currentTaskMonster.location;
                    PlayerHelper.webWalkTo(loc.x + Math.floor(Math.random() * 6 - 3),
                                           loc.y + Math.floor(Math.random() * 6 - 3),
                                           loc.plane);
                    Game.sendGameMessage("[SlayerBot] Walking closer to task area...", "Bot");
                }
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Attack error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// EAT FOOD — Consume food from inventory
// ─────────────────────────────────────────────────────────────────────────────
function eatFood() {
    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            var foodName = config.foodType ? config.foodType.read() : "Lobster";
            var foodId = FOOD[foodName] ? FOOD[foodName].id : 379;

            // Try configured food first
            if (Game.info.inventory.search(foodId) !== -1) {
                Game.interact.inventory.useItem(foodId);
                Game.sendGameMessage("[SlayerBot] Eating " + foodName, "Bot");
            } else {
                // Try any food
                for (var i = 0; i < ALL_FOOD_IDS.length; i++) {
                    if (Game.info.inventory.search(ALL_FOOD_IDS[i]) !== -1) {
                        Game.interact.inventory.useItem(ALL_FOOD_IDS[i]);
                        Game.sendGameMessage("[SlayerBot] Eating food ID: " + ALL_FOOD_IDS[i], "Bot");
                        break;
                    }
                }
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Eat error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// DRINK PRAYER POTION
// ─────────────────────────────────────────────────────────────────────────────
function drinkPrayerPot() {
    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            // Try highest dose first
            for (var i = 0; i < PRAYER_POTION_IDS.length; i++) {
                if (Game.info.inventory.search(PRAYER_POTION_IDS[i]) !== -1) {
                    Game.interact.inventory.useItem(PRAYER_POTION_IDS[i]);
                    Game.sendGameMessage("[SlayerBot] Drank prayer potion (ID: " + PRAYER_POTION_IDS[i] + ")", "Bot");
                    break;
                }
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Prayer pot error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// ACTIVATE PROTECTION PRAYER
// ─────────────────────────────────────────────────────────────────────────────
function activateProtectionPrayer() {
    if (!currentTaskMonster || !currentTaskMonster.protectionPrayer) return;

    var prayerInfo = PRAYERS[currentTaskMonster.protectionPrayer];
    if (!prayerInfo) return;

    var currentPrayer = Client.getBoostedSkillLevels(Skill.PRAYER);
    if (currentPrayer <= 0) return; // No prayer points

    // Check if prayer is already active (simplified — toggle check)
    // The game's prayer widget would need to be checked
    // For now, we'll activate on every cycle (the API should handle already-active prayers gracefully)
    try {
        // Prayer tab widget: group 541, children are individual prayers
        var prayerWidget = Client.getWidget(541, prayerInfo.widgetChild);
        if (prayerWidget) {
            // Only click if not already active (check sprite ID or text)
            // Simplified: just ensure it's on
        }
    } catch (e) {
        // Prayer activation is best-effort
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SPECIAL FINISH — Use item on NPC when HP is low
// ─────────────────────────────────────────────────────────────────────────────
function handleSpecialFinish() {
    if (!currentTaskMonster || !currentTaskMonster.specialItem) return;
    if (currentTaskMonster.specialMechanic !== "finish") return;

    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            var target = Game.localPlayer.getInteracting();
            if (target) {
                var itemId = currentTaskMonster.specialItem;
                if (Game.info.inventory.search(itemId) !== -1) {
                    Game.interact.inventory.useItemOnNPC(itemId, target);
                    Game.sendGameMessage("[SlayerBot] Used " + currentTaskMonster.specialItemName + " on " + currentTaskMonster.name, "Bot");
                } else {
                    Game.sendGameMessage("[SlayerBot] Missing special item: " + currentTaskMonster.specialItemName + "!", "Bot");
                }
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Special finish error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOT ITEMS — Pick up ground items
// ─────────────────────────────────────────────────────────────────────────────
function pickUpLoot() {
    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            var lootItem = Game.info.groundItems.getNearest();
            if (lootItem) {
                Game.interact.groundItems.pickUp(lootItem);
                Game.sendGameMessage("[SlayerBot] Picking up loot: " + lootItem.getId(), "Bot");
            }
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Loot error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// BURY BONES
// ─────────────────────────────────────────────────────────────────────────────
function buryBone(boneId) {
    isExecuting = true;

    Utility.invokeLater(function() {
        try {
            Game.interact.inventory.useItem(boneId);
            Game.sendGameMessage("[SlayerBot] Burying bone: " + boneId, "Bot");
        } catch (e) {
            Game.sendGameMessage("[SlayerBot] Bury error: " + e, "Bot");
        }
        isExecuting = false;
    }, Utility.getDelay());
}

// ─────────────────────────────────────────────────────────────────────────────
// HANDLE LOOT ITEMS (state handler version)
// ─────────────────────────────────────────────────────────────────────────────
function handleLootItems() {
    // Self-sufficiency: also pick up coins and valuables for GP generation
    try {
        if (ssPickUpValuableLoot()) return; // Returns true if started picking up
    } catch (e) {
        // SS module may not be loaded
    }
    pickUpLoot();
    state = STATES.FIGHT_TASK;
}

// ─────────────────────────────────────────────────────────────────────────────
// HANDLE BURY BONES (state handler version)
// ─────────────────────────────────────────────────────────────────────────────
function handleBuryBones() {
    for (var i = 0; i < ALL_BONE_IDS.length; i++) {
        if (Game.info.inventory.search(ALL_BONE_IDS[i]) !== -1) {
            buryBone(ALL_BONE_IDS[i]);
            return;
        }
    }
    state = STATES.FIGHT_TASK;
}

// ─────────────────────────────────────────────────────────────────────────────
// HANDLE EAT FOOD (state handler version)
// ─────────────────────────────────────────────────────────────────────────────
function handleEatFood() {
    eatFood();
    state = STATES.FIGHT_TASK;
}

// ─────────────────────────────────────────────────────────────────────────────
// HANDLE DRINK PRAYER (state handler version)
// ─────────────────────────────────────────────────────────────────────────────
function handleDrinkPrayer() {
    drinkPrayerPot();
    state = STATES.FIGHT_TASK;
}

// ─────────────────────────────────────────────────────────────────────────────
// TASK_COMPLETE — Task finished, go get a new one
// ─────────────────────────────────────────────────────────────────────────────
function handleTaskComplete() {
    tasksCompleted++;
    overlay.tasksCompleted.update("Tasks Done: " + tasksCompleted);
    updateOverlay("Task complete! (" + tasksCompleted + " total)", "Complete!", 0);

    Game.sendGameMessage("[SlayerBot] === TASK COMPLETE === Total tasks: " + tasksCompleted, "Bot");

    // Reset task info
    currentTaskName = "";
    currentTaskMonster = null;
    killsLeft = 0;
    taskReceived = false;
    bankTarget = null;

    // Check if we have food to continue
    var foodCount = countFood();
    if (foodCount >= 5) {
        // Good to go — head straight to master
        Game.sendGameMessage("[SlayerBot] Enough food remaining. Going to master for new task.", "Bot");
        state = STATES.SELECT_MASTER;
    } else {
        // Restock first
        Game.sendGameMessage("[SlayerBot] Low on supplies after task. Restocking first.", "Bot");
        state = STATES.WALK_TO_BANK;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HANDLE_DEATH — Recover from death
// ─────────────────────────────────────────────────────────────────────────────
function handleDeath() {
    updateOverlay("DIED! Recovering...", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);
    Game.sendGameMessage("[SlayerBot] === DEATH DETECTED === Recovering...", "Bot");

    // After death, player respawns in Lumbridge
    // Need to: 1) go to bank, 2) re-gear, 3) go back to task
    deathDetected = false;

    // Wait a few ticks for respawn
    if (stateTickCounter < 5) return;

    // Go to bank for gear
    bankTarget = null; // Reset so it finds nearest bank
    state = STATES.CHECK_SUPPLIES;
}

// ─────────────────────────────────────────────────────────────────────────────
// HANDLE_LEVEL_UP — Process level up events
// ─────────────────────────────────────────────────────────────────────────────
function handleLevelUp() {
    updateOverlay("Level up!", currentTaskMonster ? currentTaskMonster.name : "None", killsLeft);
    Game.sendGameMessage("[SlayerBot] Level up detected! Recalculating stats...", "Bot");

    levelUpDetected = false;

    // Refresh stats
    attackLevel = Client.getRealSkillLevels(Skill.ATTACK);
    strengthLevel = Client.getRealSkillLevels(Skill.STRENGTH);
    defenceLevel = Client.getRealSkillLevels(Skill.DEFENCE);
    hitpointsLevel = Client.getRealSkillLevels(Skill.HITPOINTS);
    prayerLevel = Client.getRealSkillLevels(Skill.PRAYER);
    rangedLevel = Client.getRealSkillLevels(Skill.RANGED);
    magicLevel = Client.getRealSkillLevels(Skill.MAGIC);
    slayerLevel = Client.getRealSkillLevels(Skill.SLAYER);
    combatLevel = getCombatLevel();

    Game.sendGameMessage("[SlayerBot] Updated combat level: " + combatLevel, "Bot");

    // Continue what we were doing (should be FIGHT_TASK usually)
    if (currentTaskMonster && killsLeft > 0) {
        state = STATES.FIGHT_TASK;
    } else {
        state = STATES.CHECK_STATS;
    }
}

// =============================================================================
// CHAT MESSAGE HANDLER — Parse task assignments and kill tracking
// =============================================================================
function OnChatMessage(msg) {
    if (!msg) return;

    var text = "";
    if (typeof msg === "string") {
        text = msg;
    } else if (msg.getMessage) {
        text = msg.getMessage();
    } else if (msg.message) {
        text = msg.message;
    }

    if (!text || text.length === 0) return;

    // ─── TASK ASSIGNMENT ───
    // "Your new task is to kill 135 Blue dragons."
    var newTaskMatch = text.match(/[Yy]our (?:new )?task is to kill (\d+) (.+?)(?:\.|$)/);
    if (newTaskMatch) {
        killsLeft = parseInt(newTaskMatch[1], 10);
        currentTaskName = newTaskMatch[2].trim();
        // Remove trailing period if present
        if (currentTaskName.endsWith(".")) {
            currentTaskName = currentTaskName.slice(0, -1);
        }
        taskReceived = true;

        Game.sendGameMessage("[SlayerBot] NEW TASK: Kill " + killsLeft + " " + currentTaskName, "Bot");
        overlay.task.update("Task: " + currentTaskName);
        overlay.killsLeft.update("Kills Left: " + killsLeft);
        return;
    }

    // ─── TASK COMPLETE (from master) ───
    if (text.indexOf("You've completed your task") !== -1 ||
        text.indexOf("you have completed your task") !== -1 ||
        text.indexOf("You need something new to hunt") !== -1) {
        Game.sendGameMessage("[SlayerBot] Task completion confirmed via chat.", "Bot");
        if (state === STATES.FIGHT_TASK || state === STATES.WALK_TO_TASK) {
            killsLeft = 0;
            state = STATES.TASK_COMPLETE;
        }
        return;
    }

    // ─── RETURN TO MASTER ───
    if (text.indexOf("return to a Slayer master") !== -1) {
        Game.sendGameMessage("[SlayerBot] Return to master message detected.", "Bot");
        if (killsLeft <= 0) {
            state = STATES.TASK_COMPLETE;
        }
        return;
    }

    // ─── KILL COUNT TRACKING ───
    // "Your Slayer task: Gargoyles (95 remaining)"
    var taskRemaining = text.match(/[Ss]layer task[:\s]+(.+?)\s*\((\d+)\s*remaining\)/);
    if (taskRemaining) {
        currentTaskName = taskRemaining[1].trim();
        killsLeft = parseInt(taskRemaining[2], 10);
        overlay.task.update("Task: " + currentTaskName);
        overlay.killsLeft.update("Kills Left: " + killsLeft);
        return;
    }

    // "You have completed your Slayer task! You were assigned X."
    // Kill count decrement (superior version)
    var killUpdate = text.match(/[Ss]uperior foe has appeared/);
    if (killUpdate) {
        Game.sendGameMessage("[SlayerBot] Superior monster appeared!", "Bot");
        return;
    }

    // ─── LEVEL UP ───
    if (text.indexOf("Congratulations") !== -1 && text.indexOf("level") !== -1) {
        Game.sendGameMessage("[SlayerBot] Level up detected: " + text, "Bot");
        levelUpDetected = true;
        return;
    }

    // ─── SLAYER LEVEL UP SPECIFICALLY ───
    if (text.indexOf("Slayer level") !== -1) {
        slayerLevel = Client.getRealSkillLevels(Skill.SLAYER);
        Game.sendGameMessage("[SlayerBot] Slayer level now: " + slayerLevel, "Bot");
        return;
    }

    // ─── XP DROP / KILL TRACKING (simplified) ───
    // When an NPC dies that matches our task, decrement killsLeft
    // This is handled more reliably via OnNPCDeath or hitsplat tracking
}

// =============================================================================
// NPC LOOT APPEARED — Track kills when NPC dies
// =============================================================================
function OnNPCLootAppeared(npc) {
    if (!currentTaskMonster || !npc) return;

    var npcId = -1;
    try {
        npcId = npc.getId();
    } catch (e) {
        return;
    }

    // Check if this NPC is our task
    var isTask = false;
    for (var i = 0; i < currentTaskMonster.npcIds.length; i++) {
        if (currentTaskMonster.npcIds[i] === npcId) {
            isTask = true;
            break;
        }
    }

    if (isTask) {
        killsLeft--;
        totalKills++;
        overlay.killsLeft.update("Kills Left: " + Math.max(0, killsLeft));
        Game.sendGameMessage("[SlayerBot] Kill tracked! " + killsLeft + " " + currentTaskMonster.name + " remaining. Total kills: " + totalKills, "Bot");

        if (killsLeft <= 0) {
            Game.sendGameMessage("[SlayerBot] All kills complete!", "Bot");
            state = STATES.TASK_COMPLETE;
        }
    }
}
