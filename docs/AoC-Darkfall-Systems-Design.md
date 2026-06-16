# Architect of Creation — Darkfall-Style Systems Design Document
## Version 1.0 — Complete Systems Bible

*Based on extensive study of Darkfall Online, Darkfall: Unholy Wars, and Darkfall: Rise of Agon*
*Adapted for AoC's dark medieval crimson theme*

---

# TABLE OF CONTENTS
1. [UI System — Action Mode / GUI Mode](#1-ui-system)
2. [Moveable & Resizable Panels](#2-moveable-panels)
3. [Hotbar System — 10×10 Grid with Column Switching](#3-hotbar-system)
4. [Keybind System — Full Customizable Bindings](#4-keybind-system)
5. [Autocast System — Hold/Instant/Toggle/Cycling](#5-autocast-system)
6. [Skill System — Use-Based Progression + NPC Trainers](#6-skill-system)
7. [Death & Grave System — Full Loot PvP](#7-death-grave-system)
8. [Bindstone & Recall System](#8-bindstone-recall)
9. [Party & Clan System with Enhanced Loot](#9-party-clan-system)
10. [Chat System](#10-chat-system)
11. [Complete Keybind Map](#11-keybind-map)

---

# 1. UI SYSTEM — ACTION MODE / GUI MODE

## Core Concept (from Darkfall)
Darkfall uses a **dual-mode UI system**:
- **Action Mode** (default): First-person combat. Mouse controls camera. WASD movement. HUD elements visible but non-interactive. This is where you fight, explore, and play.
- **GUI Mode** (ESC toggle): Cursor appears. All panels become interactive. You can click buttons, drag items, open windows, reorganize UI. Movement still works but camera is unlocked.

## AoC Implementation

### Action Mode (Default Gameplay)
- Mouse locked to center → controls camera rotation (FPS-style)
- Left Mouse Button = Attack (melee swing / spell cast / bow shot depending on equipped)
- Right Mouse Button = Parry / Block
- All HUD elements visible but NOT clickable
- Hotbar responds to number keys 1-0
- HP/MP/Stamina/XP bars visible
- Minimap visible with crimson ring + compass
- Target frame visible (when targeting)
- Chat visible (can type with Enter without leaving Action Mode)
- Party frames visible (if in party)

### GUI Mode (ESC Toggle)
- Mouse cursor appears and unlocks
- ALL panels become interactive (clickable, draggable, resizable)
- Can open Inventory, Spellbook, Character Sheet, Crafting, Clan, etc.
- Can drag spells/items to hotbar
- Can right-click player names in chat for context menu
- Can reorganize, move, resize ANY panel
- Movement still works (WASD) but camera doesn't follow mouse
- Press ESC again to return to Action Mode

### UI Edit Mode (Within GUI Mode)
- All panels get a crimson highlight border showing they're draggable
- Drag any panel by its title bar to reposition
- Drag corner/edge handles to resize
- Right-click panel → opacity slider (0-100%)
- Right-click panel → "Lock" toggle (sticky — visible in Action Mode)
- Right-click panel → "Reset Position" to default layout
- Positions saved to local config file (persist across sessions)
- `/gui_persist_save` and `/gui_persist_load` commands for saving/loading layouts

---

# 2. MOVEABLE & RESIZABLE PANELS

## Panel List (ALL moveable)
| Panel | Default Position | Sticky (Action Mode visible) | Default Size |
|-------|-----------------|------|-------------|
| HP/MP/Stamina Bars | Top-left | ✅ Always | 220×80 |
| XP Bar | Bottom of screen | ✅ Always | Full width×12 |
| Minimap | Top-right | ✅ Always | 200×200 |
| Target Frame | Top-center-right | ✅ When targeting | 200×60 |
| Hotbar (active column) | Bottom-center | ✅ Always | 400×48 |
| Chat Panel | Bottom-left | ✅ Always | 400×200 |
| Party Frames | Left side | ✅ When in party | 150×(40×members) |
| Inventory | Center | ❌ GUI only | 400×500 |
| Spellbook | Center-right | ❌ GUI only | 500×450 |
| Character Sheet | Center-left | ❌ GUI only | 350×500 |
| Crafting Panel | Center | ❌ GUI only | 450×400 |
| Clan Panel | Center | ❌ GUI only | 400×350 |
| Quest Log | Right | ❌ GUI only | 350×400 |
| World Map | Fullscreen overlay | ❌ GUI only | 80% screen |
| Dialogue Box | Bottom-center | ✅ When active | 500×200 |
| Loot Window | Center | ✅ When looting | 250×300 |
| Pause Menu | Center | ❌ GUI only | 300×400 |
| Settings | Center | ❌ GUI only | 500×400 |

## Drag & Drop Architecture
Each panel is a `UCanvasPanel` child with:
- A **title bar** (UBorder with dark bg + crimson text) — drag handle
- A **resize grip** (bottom-right corner, 16×16 crimson triangle)
- An **opacity control** (right-click context)
- A **lock icon** (pin/unpin for Action Mode visibility)
- A **close button** (X in top-right, crimson)

Positions stored as `FVector2D` in a `TMap<FString, FPanelLayout>` saved to GameUserSettings.ini.

---

# 3. HOTBAR SYSTEM — 10×10 GRID WITH COLUMN SWITCHING

## How Darkfall Hotbars Work
- **10 slots** visible at a time (keys 1-0)
- **10 columns** (CTRL+1 through CTRL+0 switches active column)
- Total: **100 hotbar slots** across 10 columns
- The **active column** number shown on the left side of the hotbar
- Any combination can go in a slot:
  - Spells (from any of the 10 magic schools)
  - Weapon skills (Power Attack, Whirlwind, etc.)
  - Items (potions, food, mount figurines)
  - Equipment (weapons, shields — equipping on press)
  - Recall skills (Bindstone Recall, House Recall)
  - General skills (Rest, Sprint, Gank, Revive)

## AoC Hotbar Implementation
- 10 visible slots, horizontal, bottom-center
- Keys 1-0 activate the slot
- CTRL+1 through CTRL+0 switches to column 1-10
- Current column indicator: small number badge (crimson) on left
- Each slot shows: icon + keybind number + cooldown sweep + quantity (for stacked items)
- **Drag-to-populate**: In GUI mode, drag from Spellbook/Inventory/Skills to hotbar
- **Right-click slot** in GUI mode: Clear slot / Set autocast mode
- **Visual feedback**: Active/selected spell has crimson glow border
- **Weapon swap**: Placing weapons in different slots = pressing that key equips the weapon
  - This is how Darkfall handles weapon switching — weapons on hotbar keys!

## Column Presets (Common Darkfall Setups)
- Column 1: Default combat (sword + shield + basic spells)
- Column 2: Archery (bow + arrows + utility)
- Column 3: Magic (staff + offensive spells)
- Column 4: Healing (staff + heal spells + buffs)
- Column 5-10: Custom loadouts

---

# 4. KEYBIND SYSTEM — FULL CUSTOMIZABLE BINDINGS

## Architecture
Every action in the game has a **bindable action name** (e.g., `Combat_Attack`, `Spell_HealSelf`, `UI_Inventory_Toggle`).

Players can bind:
- **Keyboard keys** (any key including F1-F12, numpad, etc.)
- **Mouse buttons** (LMB, RMB, MMB, Mouse4, Mouse5)
- **Modifier combos** (CTRL+key, SHIFT+key, ALT+key, CTRL+SHIFT+key)
- **Multiple binds per action** (primary + secondary keybind)

### Bind Categories (from Darkfall)
| Category | Actions |
|----------|---------|
| **Movement** | Forward, Backward, Strafe Left/Right, Jump, Crouch, Sprint, Toggle Walk |
| **Combat** | Attack (LMB), Parry (RMB), Toggle Attack Style (H), Sheathe Weapon (R) |
| **Hotbar** | Slot 1-0, Column Switch (CTRL+1-0) |
| **UI Toggles** | Inventory (I), Spellbook (B), Character (C), Crafting (K), Map (M), Quest (J), Clan (G), Social (O), Settings |
| **GUI** | Toggle GUI Mode (ESC), Chat (Enter) |
| **Interaction** | Interact (F), Alt-Interact (G), Loot/Gank (F on grave), Revive (G on downed player) |
| **Camera** | Zoom In/Out, Toggle First/Third Person (scroll wheel) |
| **Communication** | Local Chat, Clan Chat, Party Chat, Whisper, Trade Chat |
| **Social** | Create Party, Leave Party, Invite Player |
| **Recall** | Bindstone Recall, House Recall, Capital Recall |

### /bind Command System
Players can type `/bind KEY ACTION` in chat:
```
/bind O UI_Inventory_Toggle
/bind BUTTON_1 Combat_Parry
/bind SHIFT+F Interaction_AltUse
/bind CTRL+1 Hotbar_Column_1
```

---

# 5. AUTOCAST SYSTEM — HOLD / INSTANT / TOGGLE / CYCLING

## Cast Modes (Per Hotbar Slot)
Each hotbar slot can be configured (right-click → Cast Mode) to one of:

### 1. Press-to-Cast (Default)
- Press key once → spell/skill fires once
- Most basic mode
- Used for: single-target attacks, heals, utility spells

### 2. Hold-to-Cast (Continuous)
- Hold key down → spell fires repeatedly on cooldown
- Release key → stops casting
- Used for: sustained damage (Mana Missile, Arrow Volley), self-buffs on repeat
- Critical for combat — Darkfall players HOLD their heal key while dodging

### 3. Toggle Autocast
- Press once → spell auto-fires on cooldown continuously
- Press again → stops
- Visual indicator: pulsing crimson border on slot
- Used for: grinding/farming, sustained healing

### 4. Instant-on-Press (No Queue)
- Fires immediately with zero queue delay
- Used for: emergency heals, interrupts, reactive parries
- Bypasses any animation queue

### 5. Cycling Mode (Hotbar Column)
- Pressing the hotbar key cycles through a SET of spells in sequence
- Each press fires the NEXT spell in the cycle
- Wraps around to first spell
- Used for: Melee combo chains (Slash → Power Attack → Cleave), Spell rotations
- Visual: current position dot indicator under the slot icon

## Weapon Swap + Autocast Interaction
- In Darkfall, players bind weapons to hotbar and rapidly swap + cast:
  - Key 1 = Sword (equips sword) → LMB attack
  - Key 2 = Staff (equips staff) → auto-selects last used spell
  - Key 3 = Heal Self → casts while staff equipped
  - Key 1 again → back to sword instantly
- This creates the signature Darkfall "weapon juggling" combat feel
- AoC preserves this: equipping a weapon via hotbar is INSTANT, no animation delay

---

# 6. SKILL SYSTEM — USE-BASED PROGRESSION + NPC TRAINERS

## Core Philosophy (Darkfall + Morrowind + Life is Feudal)
**Everything levels through use.** No XP pools, no level-ups with skill points. You get better at what you DO.

### How Use-Based Leveling Works
- **Swing a sword** → Sword skill gains XP (0.1-0.5 per hit, scaled by target difficulty)
- **Cast a Pyromancy spell** → Pyromancy school gains XP
- **Mine an ore node** → Mining skill gains XP
- **Craft a weapon** → Weapon Smithing gains XP
- **Take damage** → Endurance attribute gains XP (passively)
- **Cast spells** → Intelligence attribute gains XP (passively)
- **Run/dodge** → Agility attribute gains XP (passively)

### Skill Progression Curve
```
Level 1-25:   Fast (100 XP per level) — "Learning the basics"
Level 26-50:  Medium (250 XP per level) — "Getting proficient"
Level 51-75:  Slow (500 XP per level) — "Mastering the craft"
Level 76-100: Very Slow (1000 XP per level) — "Legendary dedication"
```

### Attribute Growth (Passive, from Darkfall)
| Attribute | Grows From |
|-----------|------------|
| **Strength** | Melee attacks, Mining, Carrying heavy loads |
| **Dexterity** | Ranged attacks, Bow use, Crafting precision tasks |
| **Agility** | Running, Dodging, Swimming, Taking hits while moving |
| **Intelligence** | Casting ANY magic spell, Enchanting, Alchemy |
| **Wisdom** | Healing spells, Buff spells, Meditation |
| **Endurance** | Taking damage, Blocking, Sprinting, Heavy armor wear |

### NPC Trainers — Buyable Skills (from Darkfall)
In cities, specialized NPCs sell advanced skills:

| NPC Type | Sells | Found In |
|----------|-------|----------|
| **Fighter** | Weapon mastery skills (Power Attack, Whirlwind, etc.) | All cities |
| **Arcanist** | Advanced magic schools, spell upgrades, House Recall | Capital cities |
| **Blacksmith** | Advanced crafting skills, smelting recipes | Capital cities |
| **Herbalist** | Alchemy recipes, potion knowledge | Some cities |
| **Scout** | Stealth, tracking, archery specials | Frontier towns |
| **Sage** | Enchanting recipes, scroll crafting | Mage towers |

### Skill Purchase Requirements
- **Gold cost** (scales with skill tier)
- **Prerequisite skill level** (e.g., "Requires Sword 50" to buy "Power Attack II")
- **May require reputation** with a faction
- Purchased skills still need to be LEVELED through use to reach full power

### AoC Complete Skill List (48 Skills + 17 Mastery Tiers)

#### 🗡️ WEAPON MASTERY (17 weapons, each 1-100 + Mastery tier at 100)
Sword, Greatsword, Dagger, Axe, Great Axe, Hammer, Great Hammer, Mace, Great Mace, Scythe, Spear, Claws, Bow, Shield, 1H Staff, 2H Staff, Unarmed

Each weapon has:
- **Base skill** (1-100, levels through hitting things)
- **Mastery tier** (unlocks at base 100, adds 1-50 mastery with special abilities)
- **Sub-skills** bought from Fighter NPC:
  - Level 25: Basic special (e.g., "Sword Lunge")
  - Level 50: Intermediate special (e.g., "Whirlwind Slash")
  - Level 75: Advanced special (e.g., "Blade Storm")
  - Level 100 (Mastery): Ultimate special (e.g., "Sovereign's Strike")

#### 🔮 MAGIC SCHOOLS (10 schools, each 1-100)
Arcana, Pyromancy, Cryomancy, Stormcalling, Tempest, Verdancy, Umbramancy, Radiance, Sangromancy, Dominion

Each school has:
- 16 spells (unlocked progressively as school level rises)
- Spells unlock at levels: 1, 5, 10, 15, 20, 30, 40, 50, 60, 70, 75, 80, 85, 90, 95, 100
- Each individual spell ALSO levels through use (1-100) improving damage/duration/efficiency

#### ⛏️ GATHERING (7 skills, each 1-100)
Mining, Woodcutting, Fishing, Herbalism, Hunting, Farming, Skinning

- Higher level = access to higher-tier nodes
- Higher level = faster gathering speed
- Higher level = chance for rare materials
- Attributes gained: STR (Mining, Woodcutting), DEX (Herbalism, Fishing), END (Farming, Skinning)

#### 🔨 CRAFTING (11 skills, each 1-100)
Weapon Smithing, Armor Smithing, Bow Crafting, Staff Crafting, Jewelcrafting, Enchanting, Alchemy, Cooking, Brewing, Tailoring, Leatherworking

- Leveled by crafting items
- Higher level = access to higher-tier recipes
- Sub-skills purchased from Blacksmith NPC:
  - Level 25: Intermediate recipes
  - Level 50: Advanced recipes
  - Level 75: Master recipes
  - Level 100: Legendary recipes (unique to that crafter)

#### 🏰 CONSTRUCTION (3 skills, Life is Feudal style)
Masonry (stone structures), Carpentry (wood structures), Architecture (planning, blueprints, large builds)

- Masonry + Carpentry = build individual pieces
- Architecture = required for planning large structures (keeps, walls, bridges)
- Higher Architecture = larger/more complex structures allowed

---

# 7. DEATH & GRAVE SYSTEM — FULL LOOT PVP

## Death Mechanics (from Darkfall, themed for AoC)

### When You Die:
1. Character falls to "Death's Door" state (ragdoll, can look around)
2. A **gravestone** spawns at your death location
3. **ALL inventory items** go into the gravestone (full loot)
4. You can be **Ganked** (finished off) or **Revived** by other players
5. If ganked or no revive within 60 seconds → respawn at bindstone
6. Respawn with: Low HP, Low Mana, NO items (naked + starter weapon)

### The Gravestone
- Visual: Dark stone marker with crimson runes (AoC themed, NOT Darkfall's tombstone)
- Shows player name + time of death
- **Persists for 2 hours** then despawns (items lost forever)
- **Death location shown on YOUR map** with skull icon + timer
- **ANYONE can loot** the gravestone (press F to open loot window)
- Looting is a **drag-and-drop** process (takes time, can be interrupted)
  - This is crucial for PvP — you can't quickly vacuum-loot; you must choose carefully
  - Each item must be individually dragged from grave → your inventory
  - Being hit while looting cancels the action

### Death's Door Interactions
- **Gank** (F key while standing over downed player): Finish them off, they respawn
- **Revive** (G key): Resurrect them with ~25% HP/MP. They keep their items (no grave spawns)
- Both Gank and Revive are **General Skills** every player has from the start
- Revive has a ~10 second channel (can be interrupted)
- Dying in water = instant gank (no revive possible)

### AoC Additions to Darkfall's System
- **Grave Protection Timer**: First 30 seconds after death, only YOUR party members can loot your grave (prevents instant ninja-looting by nearby enemies)
- **Equipment Binding**: Certain rare/quest items can be "Soulbound" — these stay with you on death and do NOT go to the grave
- **Death Penalty**: Small skill XP loss on death (~0.5% of current level in all skills) — makes death meaningful but not devastating

---

# 8. BINDSTONE & RECALL SYSTEM

## Bindstones
- Magical structures found in cities and player holdings
- Press F while looking at one → "Bind Your Soul" (2-second channel)
- You can only be bound to ONE bindstone at a time
- On death → respawn here
- Visual: Tall dark obelisk with pulsing crimson runes (AoC themed)

## Recall Skills
Three recall skills, all are channeled (interruptible):

### 1. Bindstone Recall
- Teleports you to your current bindstone
- **Channel time**: 120 seconds (2 minutes)
- **Cooldown**: 60 minutes
- **Interrupted by**: Taking damage, Moving, Using any skill
- **Cost**: Free
- Default bound stone = your starting city

### 2. House Recall
- Teleports you to a house you own or are a tenant of
- **Channel time**: 120 seconds
- **Cooldown**: 60 minutes
- **Purchase**: Must buy from Arcanist NPC (500 gold)
- Requires owning or being a tenant at a house

### 3. Capital Recall (AoC: "Realm Recall")
- Teleports to the capital city of your race
- **Channel time**: 90 seconds
- **Cooldown**: Scales with alignment (better alignment = shorter)
- **Cost**: Scales with alignment (25-500 gold)
- New characters get 10 free uses
- Only available to players with positive alignment (blue status)

### Runestones (Rare Consumable Teleport)
- Found as rare drops from monsters
- Can be "marked" at any outdoor location using Teleport Anchor skill
- Single-use: channels teleport to marked location, then consumed
- Extremely valuable PvP/PvE tool
- Cannot be marked inside dungeons

---

# 9. PARTY & CLAN SYSTEM WITH ENHANCED LOOT

## Party System

### Creating & Managing
- Right-click own status window → "Create Party"
- Or: `/partycreate` command
- Invite: Right-click player name in chat or `/invite FirstName LastName`
- Leave: `/partyleave`
- Max party size: **8 players**

### Party UI (from Darkfall)
- Small status windows for each party member (HP/MP bars + name)
- Party members shown on minimap (green dots + names)
- Party members shown on world map
- Status windows can be locked (visible in Action Mode) or unlocked (GUI only)
- Party leader has star icon

### AoC Party Loot System (ENHANCED from Darkfall)

In Darkfall, loot is free-for-all even in parties. AoC improves this:

#### Loot Modes (Party Leader sets):
1. **Free Loot** (Darkfall default): Anyone can loot anything. First come, first served.
2. **Round Robin**: Loot rights rotate through party members per kill.
3. **Need/Greed**: Rare+ items trigger a Need/Greed/Pass roll for all party members.
4. **Master Looter**: Party leader distributes all loot.

#### AoC Loot Boost System (Brad's request)
**"Everyone gets a fair slice, boosted per player that joins"**

- When a party kills a mob, the loot table rolls ONCE per mob (base)
- **Loot quality/quantity** is boosted by party size:
  - Solo: Base loot (100%)
  - 2 players: 130% loot quantity
  - 3 players: 155% loot quantity
  - 4 players: 175% loot quantity
  - 5 players: 190% loot quantity
  - 6 players: 200% loot quantity
  - 7 players: 208% loot quantity
  - 8 players: 215% loot quantity
- Each party member gets their OWN loot roll from the boosted table
- Gold is split evenly among nearby party members
- This means: more players = more total loot, everyone benefits
- XP is NOT penalized — full XP to all party members who participated
- "Participated" = dealt damage to the mob within last 30 seconds

## Clan System

### Structure
- **Clan creation**: Costs 2000 gold, requires 1 player minimum
- **Clan ranks** (customizable names, preset permissions):
  - **Overlord** (Supreme General): Full control. Only 1 per clan.
  - **Warlord** (General): Can invite, kick, set MOTD, access vault
  - **Commander** (Captain): Can invite, access vault limited
  - **Soldier** (Member): Basic member, no vault access by default
  - **Initiate** (Recruit): New member, limited permissions
- Leaving a clan takes **15 minutes** (prevents combat-logging clan swaps)

### Clan Features
- **Clan Chat** channel (all members)
- **Clan Vault** (shared bank, access by rank)
- **Clan MOTD** (Message of the Day)
- **Clan Banner/Emblem** (customizable colors + symbol)
- **Clan Tax**: Overlord can set % tax on member gold gains (goes to clan vault)
- **War declarations**: Clan can declare war on another clan (kills don't affect alignment)
- **Alliance system**: Up to 3 clans can form an alliance

### Clan Holdings (Cities/Territory)
- Clans can conquer and own NPC cities by siege
- **Siege requires**: 5+ premium members (hamlet) or 10+ (city)
- Siege stone must be placed within 1001m of target holding
- Holding structures: Bindstone (limited slots), Banks, Crafting stations
- **Levy Collector**: Generates income from territory (harvesting, monster kills, etc.)

---

# 10. CHAT SYSTEM

## Chat Channels (Tabs)
| Channel | Command | Color | Range |
|---------|---------|-------|-------|
| **Local** | (default) | White | 50m radius |
| **Shout** | /shout | Yellow | 200m radius |
| **Clan** | /clan or /c | Green | All clan members |
| **Party** | /party or /p | Cyan | Party members |
| **Whisper** | /tell Name | Purple | Direct to player |
| **Trade** | /trade | Orange | Global |
| **System** | (auto) | Crimson | System messages |
| **Combat** | (auto) | Red | Damage/skill numbers |

## Chat Features
- Tab-based filtering (click tabs to show/hide channels)
- Right-click player name → context menu (Invite, Whisper, Ignore, View Profile)
- `/ignore "FirstName LastName" reason` — blocks messages
- Chat is available in BOTH Action and GUI modes
- Press Enter to start typing (even in Action Mode)
- Customizable font size and opacity
- Chat window moveable/resizable like all panels

---

# 11. COMPLETE KEYBIND MAP — DEFAULT LAYOUT

## Movement
| Key | Action |
|-----|--------|
| W | Move Forward |
| S | Move Backward |
| A | Strafe Left |
| D | Strafe Right |
| Space | Jump |
| Shift (hold) | Sprint |
| C (hold) | Crouch |
| Z | Toggle Walk/Run |

## Combat
| Key | Action |
|-----|--------|
| LMB | Attack / Cast Selected Spell |
| RMB | Parry / Block |
| H | Toggle Melee Attack Style (Horizontal/Vertical) |
| R | Sheathe/Unsheathe Weapon |
| Q | Quick Swap (last two weapons) |
| Tab | Cycle Nearest Target |
| Shift+Tab | Cycle Friendly Target |
| F | Interact / Loot / Bind / Gank |
| G | Alt-Interact / Trade / Revive |

## Hotbar
| Key | Action |
|-----|--------|
| 1-0 | Hotbar Slot 1-10 |
| CTRL+1-0 | Switch Hotbar Column 1-10 |
| Mouse Scroll | Cycle hotbar columns |

## UI Toggles (GUI Mode)
| Key | Action |
|-----|--------|
| ESC | Toggle Action/GUI Mode |
| I | Inventory |
| B | Spellbook |
| P | Character Sheet (Stats/Skills) |
| K | Crafting |
| J | Quest Log |
| M | World Map |
| G | Clan Panel |
| O | Social / Friends |
| N | Settings |
| L | Chat Log |
| Enter | Open Chat Input |

## Recall
| Key | Action |
|-----|--------|
| (unbound) | Bindstone Recall |
| (unbound) | House Recall |
| (unbound) | Realm Recall |
*These are meant to be placed on hotbar slots or manually bound*

---

# C++ IMPLEMENTATION NOTES

## AoCRuntimeUI v3 Architecture

The entire UI system lives in one massive UUserWidget subclass: `UAoCRuntimeUI`

### Key Technical Decisions:
1. **All absolute positioning** — no anchors (anchors were buggy in v2). Every panel uses `SetPosition()` and `SetSize()` on `UCanvasPanelSlot`.
2. **Drag system**: Override `NativeOnMouseButtonDown`, `NativeOnMouseMove`, `NativeOnMouseButtonUp` to implement panel dragging.
3. **Dual mode**: Track `bGUIMode` bool. When false, input goes to game. When true, input goes to UI.
4. **Hotbar columns**: `TArray<TArray<FHotbarSlotData>> HotbarColumns` — 10 arrays of 10 slots each.
5. **Keybinds**: `TMap<FKey, FBindableAction> KeyBindings` — fully remappable.
6. **Autocast**: Each hotbar slot has an `ECastMode` enum (Press, Hold, Toggle, Cycling).
7. **Panel positions saved**: `TMap<FString, FPanelLayoutData> PanelLayouts` serialized to SaveGame.
8. **Party frames**: Dynamic — created/destroyed as party members join/leave.
9. **Chat**: ScrollBox with colored text entries, tab filtering.

### File Structure:
```
Source/AOC/UI/
├── AoCRuntimeUI.h          — Master header (all declarations)
├── AoCRuntimeUI.cpp         — Master implementation (all 15+ panels)
├── AoCHotbarSystem.h/cpp    — Hotbar logic, column switching, autocast
├── AoCKeybindSystem.h/cpp   — Keybind management, save/load
├── AoCDragSystem.h/cpp      — Panel drag & resize logic
├── AoCChatSystem.h/cpp      — Chat channels, commands, filtering
├── AoCPartyUI.h/cpp         — Party frames, loot window
├── AoCClanUI.h/cpp          — Clan management panel
```

But since Brad wants **ONE JUICY FILE**, we combine everything into AoCRuntimeUI.h + AoCRuntimeUI.cpp.

---

# END OF DESIGN DOCUMENT
*This document is the complete reference for all Darkfall-inspired systems in Architect of Creation.*
*Every system described here will be implemented in AoCRuntimeUI v3.*
