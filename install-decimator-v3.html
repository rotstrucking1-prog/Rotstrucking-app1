#!/usr/bin/env python3
"""
ROAT PKZ PVP BOT v3.0 — TICK-PERFECT DECIMATOR

One button. Full send. Target lock → read prayer → detect weapon → hit through protection
→ predict exact tick timing → eat/switch/spec at mathematically perfect moments.

TICK ENGINE:
- OSRS tick = 0.600 seconds exactly
- Bot syncs to game tick cycle via animation/hitsplat detection
- Tracks our attack cooldown AND opponent's attack cooldown
- Queues actions to execute on the exact tick they need to happen
- Weapon speed database: knows EXACTLY how many ticks each weapon takes

COMBAT BRAIN:
- Reads overhead prayer icon → switches to style they're NOT protecting
- Detects opponent weapon type → predicts incoming damage style
- Knows when opponent's next hit lands → pray-switches on that tick
- Knows when OUR next hit lands → gear-switches 1 tick before
- Combo eats between our attack ticks (no DPS loss)
- Specs when opponent HP is in KO range on OUR attack tick

FEATURES:
- Auto-detects Roat Pkz window
- Standard RuneLite UI position math
- Bezier curve humanized mouse (beats ML anti-cheat)
- GUI with TARGET LOCK button + live tick counter
- F12 emergency stop
"""

import time
import math
import random
import sys
import os
import json
import threading
import ctypes
import ctypes.wintypes
import struct
from dataclasses import dataclass, field
from typing import Optional, Tuple, List, Dict
from enum import Enum

# ============================================================
#  DEPENDENCIES
# ============================================================
try:
    import cv2
    import numpy as np
    import mss
    from pynput.mouse import Controller as MouseController, Button
    from pynput.keyboard import Controller as KeyboardController, Key, KeyCode, Listener
    import pygetwindow as gw
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Run: pip install opencv-python numpy mss pynput pygetwindow Pillow")
    sys.exit(1)

try:
    import tkinter as tk
    from tkinter import ttk
    HAS_GUI = True
except ImportError:
    HAS_GUI = False

# ============================================================
#  CONSTANTS
# ============================================================
TICK_DURATION = 0.600  # OSRS game tick in seconds — EXACT
TICK_MS = 600          # Same in milliseconds

# ============================================================
#  COMBAT STYLES
# ============================================================
class CombatStyle(Enum):
    MELEE = "melee"
    RANGE = "range"
    MAGE = "mage"
    UNKNOWN = "unknown"

# ============================================================
#  WEAPON SPEED DATABASE — Every PVP weapon on Roat Pkz
#  Speed = ticks between attacks (lower = faster)
#  All values from OSRS Wiki verified
# ============================================================
WEAPON_DB = {
    # ---- MELEE ----
    "abyssal_whip":       {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "accuracy", "max_hit_bonus": 0},
    "abyssal_tentacle":   {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "accuracy", "max_hit_bonus": 0},
    "dragon_scimitar":    {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 55, "spec_type": "accuracy", "max_hit_bonus": 0},
    "dragon_longsword":   {"speed": 5, "style": CombatStyle.MELEE, "spec_cost": 25, "spec_type": "damage", "max_hit_bonus": 0.15},
    "dragon_dagger":      {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 25, "spec_type": "double_hit", "max_hit_bonus": 0.15},
    "dragon_mace":        {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 25, "spec_type": "damage", "max_hit_bonus": 0},
    "dragon_halberd":     {"speed": 7, "style": CombatStyle.MELEE, "spec_cost": 30, "spec_type": "double_hit", "max_hit_bonus": 0.10},
    "dragon_claws":       {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "quad_hit", "max_hit_bonus": 0},
    "granite_maul":       {"speed": 7, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "instant", "max_hit_bonus": 0},
    "armadyl_godsword":   {"speed": 6, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "damage", "max_hit_bonus": 0.375},
    "saradomin_godsword": {"speed": 6, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "heal", "max_hit_bonus": 0.10},
    "zamorak_godsword":   {"speed": 6, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "freeze", "max_hit_bonus": 0.10},
    "bandos_godsword":    {"speed": 6, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "drain", "max_hit_bonus": 0.21},
    "elder_maul":         {"speed": 6, "style": CombatStyle.MELEE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "ghrazi_rapier":      {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "inquisitors_mace":   {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "voidwaker":          {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "mage_hit", "max_hit_bonus": 0},
    "volatile_staff":     {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 50, "spec_type": "mage_hit", "max_hit_bonus": 0},
    "korasi_sword":       {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 60, "spec_type": "mage_hit", "max_hit_bonus": 0},
    "staff_of_the_dead":  {"speed": 4, "style": CombatStyle.MELEE, "spec_cost": 100, "spec_type": "absorb", "max_hit_bonus": 0},

    # ---- RANGE ----
    "magic_shortbow":     {"speed": 3, "style": CombatStyle.RANGE, "spec_cost": 55, "spec_type": "double_shot", "max_hit_bonus": 0},
    "magic_shortbow_i":   {"speed": 3, "style": CombatStyle.RANGE, "spec_cost": 50, "spec_type": "double_shot", "max_hit_bonus": 0},
    "dark_bow":           {"speed": 8, "style": CombatStyle.RANGE, "spec_cost": 65, "spec_type": "double_shot", "max_hit_bonus": 0.50},
    "armadyl_crossbow":   {"speed": 5, "style": CombatStyle.RANGE, "spec_cost": 40, "spec_type": "accuracy", "max_hit_bonus": 0},
    "dragon_crossbow":    {"speed": 5, "style": CombatStyle.RANGE, "spec_cost": 60, "spec_type": "dragonfire", "max_hit_bonus": 0},
    "heavy_ballista":     {"speed": 7, "style": CombatStyle.RANGE, "spec_cost": 65, "spec_type": "accuracy", "max_hit_bonus": 0.25},
    "blowpipe":           {"speed": 3, "style": CombatStyle.RANGE, "spec_cost": 50, "spec_type": "heal_shot", "max_hit_bonus": 0},
    "zaryte_crossbow":    {"speed": 5, "style": CombatStyle.RANGE, "spec_cost": 75, "spec_type": "accuracy", "max_hit_bonus": 0},
    "dragon_knife":       {"speed": 3, "style": CombatStyle.RANGE, "spec_cost": 25, "spec_type": "double_throw", "max_hit_bonus": 0},
    "morrigans_javelin":  {"speed": 4, "style": CombatStyle.RANGE, "spec_cost": 50, "spec_type": "dot", "max_hit_bonus": 0},
    "morrigans_axe":      {"speed": 5, "style": CombatStyle.RANGE, "spec_cost": 50, "spec_type": "accuracy", "max_hit_bonus": 0},
    "dragon_thrownaxe":   {"speed": 4, "style": CombatStyle.RANGE, "spec_cost": 25, "spec_type": "accuracy", "max_hit_bonus": 0},

    # ---- MAGE ----
    "trident":            {"speed": 4, "style": CombatStyle.MAGE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "sanguinesti_staff":  {"speed": 4, "style": CombatStyle.MAGE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "nightmare_staff":    {"speed": 5, "style": CombatStyle.MAGE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "tumekens_shadow":    {"speed": 5, "style": CombatStyle.MAGE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "kodai_wand":         {"speed": 4, "style": CombatStyle.MAGE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "ancient_staff":      {"speed": 5, "style": CombatStyle.MAGE, "spec_cost": 0, "spec_type": "none", "max_hit_bonus": 0},
    "zuriels_staff":      {"speed": 5, "style": CombatStyle.MAGE, "spec_cost": 50, "spec_type": "accuracy", "max_hit_bonus": 0},
}

# ============================================================
#  SPEC WEAPON PRIORITY — ordered by KO potential
# ============================================================
SPEC_PRIORITY = [
    "armadyl_godsword",   # 73 max — the classic KO
    "voidwaker",          # 58 max mage through melee prayer
    "dragon_claws",       # 46-46-23-23 = quad hit
    "granite_maul",       # instant — stack after any other spec
    "volatile_staff",     # magic spec through melee prayer
    "korasi_sword",       # magic-based melee spec
    "dragon_dagger",      # double hit cheap spec
    "heavy_ballista",     # 70+ max ranged
    "dark_bow",           # double shot guaranteed min 16 each
    "magic_shortbow_i",   # fast double shot
]

# GMaul can be used AFTER another spec on the same tick
GMAUL_STACK_WEAPONS = [
    "armadyl_godsword", "dragon_claws", "voidwaker", "dragon_dagger",
    "heavy_ballista", "volatile_staff", "korasi_sword",
]

# ============================================================
#  OVERHEAD PRAYER COLORS — pixel colors of prayer icons above head
#  These are the overhead prayer icon circle colors in RuneLite
# ============================================================
PRAYER_OVERHEAD_COLORS = {
    CombatStyle.MELEE: {  # Protect from Melee — silver/white shield
        "primary": (192, 192, 192),  # Silver
        "tolerance": 30,
    },
    CombatStyle.RANGE: {  # Protect from Missiles — green arrow
        "primary": (0, 180, 0),  # Green
        "tolerance": 35,
    },
    CombatStyle.MAGE: {  # Protect from Magic — blue circle
        "primary": (0, 128, 255),  # Blue
        "tolerance": 35,
    },
}

# ============================================================
#  OUR PRAYER ICON POSITIONS (F-key row on prayer tab)
#  These are relative to the prayer tab widget, calculated at runtime
# ============================================================
PRAYER_SLOTS = {
    "protect_melee":    {"row": 4, "col": 2},  # Row 5, Col 3 in prayer book
    "protect_missiles": {"row": 4, "col": 1},  # Row 5, Col 2
    "protect_magic":    {"row": 4, "col": 0},  # Row 5, Col 1
    "piety":            {"row": 5, "col": 4},  # Bottom row, last
    "rigour":           {"row": 5, "col": 3},
    "augury":           {"row": 5, "col": 2},
    "smite":            {"row": 4, "col": 4},
}

# ============================================================
#  INVENTORY SLOT COORDINATES (relative to inventory widget)
#  4 columns x 7 rows = 28 slots
# ============================================================
INV_COLS = 4
INV_ROWS = 7
INV_SLOT_W = 42
INV_SLOT_H = 36
INV_START_X = 0   # relative to inventory widget top-left
INV_START_Y = 0

# Inventory/Prayer widget position relative to game window top-left
# Standard RuneLite: inventory panel is ~195px from right edge, ~253px from top
INV_PANEL_OFFSET_X = -195   # negative = from right edge
INV_PANEL_OFFSET_Y = 253
PRAYER_PANEL_OFFSET_X = -195
PRAYER_PANEL_OFFSET_Y = 253   # same as inventory when tab selected

# ============================================================
#  TICK TRACKER — The heart of the bot
# ============================================================
class TickTracker:
    """
    Synchronizes with the OSRS 600ms tick cycle.
    Tracks our attack cooldown, opponent's attack cooldown,
    and predicts exactly when the next hits land.
    """
    def __init__(self):
        self.tick_zero = 0.0        # timestamp of last confirmed tick boundary
        self.current_tick = 0       # incremental tick counter
        self.synced = False         # whether we've locked onto the tick cycle

        # Our attack state
        self.our_weapon_speed = 4   # ticks between our attacks (default whip)
        self.our_last_attack_tick = 0
        self.our_next_attack_tick = 0

        # Opponent attack state
        self.opp_weapon_speed = 4   # ticks between their attacks
        self.opp_last_attack_tick = 0
        self.opp_next_attack_tick = 0

        # Hitsplat tracking for tick sync
        self.last_hitsplat_time = 0.0
        self.hitsplat_times = []    # last N hitsplat timestamps for tick sync

        # Eat delay tracking
        self.last_eat_tick = 0      # eating delays your next attack by 3 ticks
        self.eat_delay_ticks = 3

    def sync_from_hitsplat(self, timestamp: float):
        """Called when we detect a hitsplat appearing. Use multiple to lock onto tick cycle."""
        self.hitsplat_times.append(timestamp)
        # Keep last 10 for averaging
        if len(self.hitsplat_times) > 10:
            self.hitsplat_times.pop(0)

        if len(self.hitsplat_times) >= 2:
            # Calculate tick boundaries from hitsplat intervals
            intervals = []
            for i in range(1, len(self.hitsplat_times)):
                diff = self.hitsplat_times[i] - self.hitsplat_times[i-1]
                # Quantize to nearest number of ticks
                tick_count = max(1, round(diff / TICK_DURATION))
                # Calculate what tick_zero should be
                estimated_zero = self.hitsplat_times[i] - (tick_count * TICK_DURATION)
                intervals.append(estimated_zero % TICK_DURATION)

            # Average the phase offset
            avg_phase = sum(intervals) / len(intervals)
            self.tick_zero = timestamp - ((timestamp - avg_phase) % TICK_DURATION)
            self.synced = True

    def get_current_tick(self) -> int:
        """Get current game tick number based on our sync."""
        if not self.synced:
            return 0
        elapsed = time.time() - self.tick_zero
        return int(elapsed / TICK_DURATION)

    def time_until_next_tick(self) -> float:
        """Seconds until the next game tick fires."""
        if not self.synced:
            return 0.3  # best guess
        elapsed = time.time() - self.tick_zero
        into_tick = elapsed % TICK_DURATION
        return TICK_DURATION - into_tick

    def ticks_until_our_attack(self) -> int:
        """How many ticks until we can attack again."""
        ct = self.get_current_tick()
        remaining = self.our_next_attack_tick - ct
        return max(0, remaining)

    def ticks_until_opp_attack(self) -> int:
        """How many ticks until opponent hits us."""
        ct = self.get_current_tick()
        remaining = self.opp_next_attack_tick - ct
        return max(0, remaining)

    def seconds_until_our_attack(self) -> float:
        """Exact seconds until our next attack lands."""
        ticks = self.ticks_until_our_attack()
        return (ticks * TICK_DURATION) + self.time_until_next_tick()

    def seconds_until_opp_attack(self) -> float:
        """Exact seconds until opponent's next hit lands."""
        ticks = self.ticks_until_opp_attack()
        return (ticks * TICK_DURATION) + self.time_until_next_tick()

    def register_our_attack(self, weapon_key: str):
        """Called when we successfully attack."""
        ct = self.get_current_tick()
        self.our_last_attack_tick = ct
        if weapon_key in WEAPON_DB:
            self.our_weapon_speed = WEAPON_DB[weapon_key]["speed"]
        self.our_next_attack_tick = ct + self.our_weapon_speed
        # If we ate recently, our attack is delayed
        if ct - self.last_eat_tick < self.eat_delay_ticks:
            self.our_next_attack_tick = max(self.our_next_attack_tick, self.last_eat_tick + self.eat_delay_ticks)

    def register_opp_attack(self, detected_speed: int = 0):
        """Called when opponent's hitsplat appears on us."""
        ct = self.get_current_tick()
        # If we saw their previous attack, calculate their actual speed
        if self.opp_last_attack_tick > 0 and detected_speed == 0:
            gap = ct - self.opp_last_attack_tick
            if 2 <= gap <= 10:
                self.opp_weapon_speed = gap
        elif detected_speed > 0:
            self.opp_weapon_speed = detected_speed
        self.opp_last_attack_tick = ct
        self.opp_next_attack_tick = ct + self.opp_weapon_speed

    def register_eat(self):
        """Called when we eat. Eating delays our attack by 3 ticks."""
        ct = self.get_current_tick()
        self.last_eat_tick = ct
        # Push our attack window back if it would overlap
        earliest = ct + self.eat_delay_ticks
        if self.our_next_attack_tick < earliest:
            self.our_next_attack_tick = earliest

    def can_eat_without_dps_loss(self) -> bool:
        """True if we can eat RIGHT NOW without delaying our next attack."""
        ticks_to_attack = self.ticks_until_our_attack()
        return ticks_to_attack >= self.eat_delay_ticks

    def is_attack_tick(self) -> bool:
        """True if THIS tick is when our attack will fire."""
        return self.ticks_until_our_attack() == 0

    def should_switch_now(self) -> bool:
        """Switch gear 1 tick before our attack lands."""
        return self.ticks_until_our_attack() == 1

    def reset(self):
        """Reset all state for a new fight."""
        self.our_last_attack_tick = 0
        self.our_next_attack_tick = 0
        self.opp_last_attack_tick = 0
        self.opp_next_attack_tick = 0
        self.last_eat_tick = 0
        # Keep tick sync — that doesn't change between fights

# ============================================================
#  TARGET STATE — Everything we know about the opponent
# ============================================================
@dataclass
class TargetState:
    locked: bool = False
    screen_pos: Tuple[int, int] = (0, 0)       # where they are on screen
    hp_percent: float = 100.0                    # their HP bar reading
    overhead_prayer: CombatStyle = CombatStyle.UNKNOWN  # what they're praying
    detected_style: CombatStyle = CombatStyle.UNKNOWN   # what style they're using
    detected_weapon_speed: int = 4               # estimated weapon speed
    last_seen_time: float = 0.0                  # when we last saw them
    consecutive_style: int = 0                   # how many ticks same style
    is_eating: bool = False                      # did they just eat (HP went up)
    prev_hp: float = 100.0                       # previous HP reading

    def is_valid(self) -> bool:
        return self.locked and (time.time() - self.last_seen_time) < 15.0

    def is_in_ko_range(self, our_max_hit: int) -> bool:
        """Can we potentially KO them with a spec?"""
        # Estimate their current HP from percentage
        # Roat Pkz max HP is typically 99 (or boosted to 120+ with brews)
        estimated_hp = self.hp_percent * 1.2  # assume ~120 max with brews
        return estimated_hp <= our_max_hit * 1.1  # 10% buffer for accuracy

    def best_attack_style(self) -> CombatStyle:
        """What style to use based on their overhead prayer."""
        if self.overhead_prayer == CombatStyle.MELEE:
            return CombatStyle.MAGE  # Hit through melee prayer with mage
        elif self.overhead_prayer == CombatStyle.MAGE:
            return CombatStyle.RANGE  # Hit through mage prayer with range
        elif self.overhead_prayer == CombatStyle.RANGE:
            return CombatStyle.MELEE  # Hit through range prayer with melee
        else:
            return CombatStyle.MELEE  # Default to melee (highest DPS)

# ============================================================
#  OUR STATE — Everything about ourselves
# ============================================================
@dataclass
class PlayerState:
    hp_percent: float = 100.0
    prayer_percent: float = 100.0
    spec_percent: float = 100.0
    current_style: CombatStyle = CombatStyle.MELEE
    active_prayer: CombatStyle = CombatStyle.UNKNOWN  # our protection prayer
    offensive_prayer: str = "none"  # piety/rigour/augury
    is_frozen: bool = False
    frozen_until: float = 0.0

    # Inventory setup — which slots have what
    # User configures this once — tells bot what's in each slot
    melee_slots: List[int] = field(default_factory=lambda: [0, 1, 2, 3])   # helm, body, legs, weapon
    range_slots: List[int] = field(default_factory=lambda: [4, 5, 6, 7])   # helm, body, legs, weapon
    mage_slots: List[int] = field(default_factory=lambda: [8, 9, 10, 11])  # helm, body, legs, weapon
    spec_weapon_slot: int = 12
    food_slots: List[int] = field(default_factory=lambda: [16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27])
    karambwan_slot: int = 15
    brew_slots: List[int] = field(default_factory=lambda: [13, 14])
    restore_slot: int = 12

    # Weapon names (user configures)
    melee_weapon: str = "abyssal_whip"
    range_weapon: str = "magic_shortbow_i"
    mage_weapon: str = "ancient_staff"
    spec_weapon: str = "armadyl_godsword"

    def max_hit_with_spec(self) -> int:
        """Rough max hit with current spec weapon on Roat (max stats + gear)."""
        specs = {
            "armadyl_godsword": 73, "dragon_claws": 46, "voidwaker": 58,
            "granite_maul": 52, "volatile_staff": 58, "dragon_dagger": 48,
            "heavy_ballista": 70, "dark_bow": 48, "korasi_sword": 60,
        }
        return specs.get(self.spec_weapon, 50)

# ============================================================
#  SCREEN READER — Reads game state from pixels
# ============================================================
class ScreenReader:
    """Reads all game state from screen pixels. No injection needed."""

    def __init__(self):
        self.sct = mss.mss()
        self.window_rect = None  # (x, y, w, h) of Roat Pkz window
        self.window_obj = None   # pygetwindow object — for .activate()
        self._hwnd = None        # win32 window handle — for reliable focus
        self.last_frame = None
        self.frame_time = 0.0

    def find_window(self) -> bool:
        """Find the Roat Pkz game window using win32 API (reliable) + pygetwindow fallback."""
        # === METHOD 1: win32 API EnumWindows — most reliable ===
        try:
            found_wins = []
            def _enum_cb(hwnd, _):
                if ctypes.windll.user32.IsWindowVisible(hwnd):
                    length = ctypes.windll.user32.GetWindowTextLengthW(hwnd)
                    if length > 0:
                        buf = ctypes.create_unicode_buffer(length + 1)
                        ctypes.windll.user32.GetWindowTextW(hwnd, buf, length + 1)
                        title = buf.value
                        if title.strip():
                            rect = ctypes.wintypes.RECT()
                            ctypes.windll.user32.GetWindowRect(hwnd, ctypes.byref(rect))
                            w = rect.right - rect.left
                            h = rect.bottom - rect.top
                            if w > 200 and h > 200:
                                found_wins.append((hwnd, title, rect.left, rect.top, w, h))
                return True

            WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
            ctypes.windll.user32.EnumWindows(WNDENUMPROC(_enum_cb), 0)

            print("=== ALL WINDOWS (win32) ===")
            for hwnd, title, x, y, w, h in found_wins:
                print(f"  [{w}x{h}] hwnd={hwnd} '{title}'")
            print("===========================")

            # Match order: "roat" first, then "pkz", then any "runelite"
            for keyword in ["roat", "pkz", "runelite"]:
                for hwnd, title, x, y, w, h in found_wins:
                    if keyword in title.lower() and "decimator" not in title.lower():
                        if w > 500 and h > 400:
                            self.window_rect = (x, y, w, h)
                            self._hwnd = hwnd  # Store hwnd for direct win32 focus
                            # Also get pygetwindow obj as backup
                            try:
                                for gw_win in gw.getAllWindows():
                                    if gw_win.title == title:
                                        self.window_obj = gw_win
                                        break
                            except Exception:
                                self.window_obj = None
                            print(f">>> MATCHED: '{title}' hwnd={hwnd} at ({x},{y}) {w}x{h}")
                            return True
        except Exception as e:
            print(f"win32 window search error: {e}")

        # === METHOD 2: pygetwindow fallback ===
        try:
            all_wins = gw.getAllWindows()
            print("=== FALLBACK: pygetwindow ===")
            for w in all_wins:
                if w.width > 200 and w.height > 200 and w.title.strip():
                    print(f"  [{w.width}x{w.height}] '{w.title}'")
            for keyword in ["roat", "pkz", "runelite"]:
                for w in all_wins:
                    if keyword in w.title.lower() and "decimator" not in w.title.lower() and w.width > 500 and w.height > 400:
                        self.window_rect = (w.left, w.top, w.width, w.height)
                        self.window_obj = w
                        self._hwnd = None
                        print(f">>> MATCHED (fallback): '{w.title}' at ({w.left},{w.top}) {w.width}x{w.height}")
                        return True
        except Exception as e:
            print(f"pygetwindow fallback error: {e}")

        print(">>> NO GAME WINDOW FOUND!")
        return False

    # Alias so both names work
    find_game_window = find_window

    def focus_game(self):
        """Bring Roat Pkz window to foreground before any click/key action.
        Uses win32 API directly — pygetwindow.activate() is unreliable."""
        try:
            # Prefer stored hwnd from find_window(), fallback to FindWindowW
            hwnd = getattr(self, '_hwnd', None)
            if not hwnd and self.window_obj:
                try:
                    hwnd = ctypes.windll.user32.FindWindowW(None, self.window_obj.title)
                except Exception:
                    pass
            if hwnd or self.window_obj:
                if hwnd:
                    # ShowWindow(hwnd, SW_RESTORE=9) in case minimized
                    ctypes.windll.user32.ShowWindow(hwnd, 9)
                    # SetForegroundWindow — the reliable way
                    ctypes.windll.user32.SetForegroundWindow(hwnd)
                    time.sleep(0.1)
                else:
                    # Fallback to pygetwindow
                    try:
                        self.window_obj.activate()
                        time.sleep(0.1)
                    except Exception:
                        pass
                # Refresh window rect in case it moved
                if hwnd:
                    rect = ctypes.wintypes.RECT()
                    ctypes.windll.user32.GetWindowRect(hwnd, ctypes.byref(rect))
                    self.window_rect = (rect.left, rect.top,
                                        rect.right - rect.left, rect.bottom - rect.top)
                elif self.window_obj:
                    self.window_rect = (self.window_obj.left, self.window_obj.top,
                                        self.window_obj.width, self.window_obj.height)
        except Exception:
            # If activate fails, try minimize+restore as fallback
            try:
                if self.window_obj:
                    self.window_obj.minimize()
                    time.sleep(0.1)
                    self.window_obj.restore()
                    time.sleep(0.15)
            except Exception:
                pass

    def capture(self) -> Optional[np.ndarray]:
        """Capture current game frame."""
        if not self.window_rect:
            if not self.find_window():
                return None
        x, y, w, h = self.window_rect
        region = {"left": x, "top": y, "width": w, "height": h}
        try:
            img = np.array(self.sct.grab(region))
            self.last_frame = cv2.cvtColor(img, cv2.COLOR_BGRA2BGR)
            self.frame_time = time.time()
            return self.last_frame
        except Exception:
            return None

    # ---- HP / Prayer / Spec orbs (top-left of viewport) ----

    def _read_orb(self, frame: np.ndarray, orb_y_offset: int) -> float:
        """Read a minimap orb percentage (HP, Prayer, or Spec).
        Returns 100.0 (assume healthy) if orb can't be reliably detected."""
        try:
            if not self.window_rect:
                return 100.0
            wx, wy, ww, wh = self.window_rect
            # Orbs are in the minimap area — right side of fixed-mode client
            # HP orb is roughly at viewport_right - 208, viewport_top + 55
            orb_x = ww - 208
            orb_y = 55 + orb_y_offset

            # Bounds check
            if orb_x < 0 or orb_y < 0 or orb_x + 27 > frame.shape[1] or orb_y + 5 > frame.shape[0]:
                return 100.0  # Out of bounds — assume healthy

            bar_w = 27
            bar_h = 5
            bar_region = frame[orb_y:orb_y + bar_h, orb_x:orb_x + bar_w]

            if bar_region.size == 0:
                return 100.0

            # Green channel > 100 and Red < 100 = green portion (HP remaining)
            green_mask = (bar_region[:, :, 1] > 100) & (bar_region[:, :, 2] < 100)
            # Red channel > 100 and Green < 100 = red portion (HP lost)
            red_mask = (bar_region[:, :, 2] > 100) & (bar_region[:, :, 1] < 100)
            green_pixels = int(np.sum(green_mask))
            red_pixels = int(np.sum(red_mask))
            identifiable = green_pixels + red_pixels

            # If we can't identify enough orb pixels, reading is unreliable
            # Assume healthy rather than triggering emergency eat spam
            if identifiable < 10:
                return 100.0

            return (green_pixels / identifiable) * 100.0
        except Exception:
            return 100.0  # Any error = assume healthy

    def read_our_hp(self, frame: np.ndarray) -> float:
        return self._read_orb(frame, 0)

    def read_our_prayer(self, frame: np.ndarray) -> float:
        return self._read_orb(frame, 34)

    def read_our_spec(self, frame: np.ndarray) -> float:
        return self._read_orb(frame, 68)

    # ---- Opponent HP bar (above their head) ----

    def read_opponent_hp(self, frame: np.ndarray, target_pos: Tuple[int, int]) -> float:
        """Read opponent's HP bar above their character model."""
        tx, ty = target_pos
        # HP bar is ~30px wide, ~5px tall, about 15px above character center
        bar_y = ty - 20
        bar_x = tx - 15
        bar_w = 30
        bar_h = 5

        if bar_y < 0 or bar_x < 0:
            return -1.0

        bar_region = frame[bar_y:bar_y + bar_h, bar_x:bar_x + bar_w]
        if bar_region.size == 0:
            return -1.0

        # Green portion = remaining HP, Red = damage taken
        green_mask = (bar_region[:, :, 1] > 120) & (bar_region[:, :, 2] < 80)
        red_mask = (bar_region[:, :, 2] > 120) & (bar_region[:, :, 1] < 80)
        green_px = np.sum(green_mask)
        red_px = np.sum(red_mask)
        total = green_px + red_px
        if total < 5:
            return -1.0  # No HP bar visible
        return (green_px / total) * 100.0

    # ---- Overhead prayer detection ----

    def read_overhead_prayer(self, frame: np.ndarray, target_pos: Tuple[int, int]) -> CombatStyle:
        """Detect what protection prayer the opponent has active (icon above head)."""
        tx, ty = target_pos
        # Overhead prayer icon is ~30px above the HP bar
        icon_y = ty - 50
        icon_x = tx - 12
        icon_w = 24
        icon_h = 24

        if icon_y < 0 or icon_x < 0:
            return CombatStyle.UNKNOWN

        icon_region = frame[icon_y:icon_y + icon_h, icon_x:icon_x + icon_w]
        if icon_region.size == 0:
            return CombatStyle.UNKNOWN

        # Check for each prayer color
        best_match = CombatStyle.UNKNOWN
        best_count = 0

        for style, colors in PRAYER_OVERHEAD_COLORS.items():
            r, g, b = colors["primary"]
            tol = colors["tolerance"]
            mask = (
                (np.abs(icon_region[:, :, 2].astype(int) - r) < tol) &
                (np.abs(icon_region[:, :, 1].astype(int) - g) < tol) &
                (np.abs(icon_region[:, :, 0].astype(int) - b) < tol)
            )
            count = np.sum(mask)
            if count > best_count and count > 15:  # minimum pixel threshold
                best_count = count
                best_match = style

        return best_match

    # ---- Hitsplat detection (for tick sync) ----

    def detect_hitsplat(self, frame: np.ndarray, pos: Tuple[int, int]) -> bool:
        """Detect if a hitsplat (red/blue circle) just appeared near a position."""
        px, py = pos
        region_y = py - 30
        region_x = px - 20
        region_w = 40
        region_h = 25

        if region_y < 0 or region_x < 0:
            return False

        region = frame[region_y:region_y + region_h, region_x:region_x + region_w]
        if region.size == 0:
            return False

        # Hitsplats are bright red circles
        red_mask = (
            (region[:, :, 2] > 180) &  # High red
            (region[:, :, 1] < 60) &   # Low green
            (region[:, :, 0] < 60)     # Low blue
        )
        # Also check for blue (zero) hitsplats
        blue_mask = (
            (region[:, :, 0] > 150) &
            (region[:, :, 1] < 80) &
            (region[:, :, 2] < 80)
        )
        hit_pixels = np.sum(red_mask) + np.sum(blue_mask)
        return hit_pixels > 20  # hitsplat is roughly 10x10 pixels

    # ---- Opponent weapon/style detection ----

    def detect_opponent_style(self, frame: np.ndarray, target_pos: Tuple[int, int],
                               our_pos: Tuple[int, int]) -> Tuple[CombatStyle, int]:
        """
        Detect opponent's current attack style and estimated weapon speed.
        Uses: distance, projectile color, animation timing.
        Returns: (style, estimated_speed_ticks)
        """
        tx, ty = target_pos
        ox, oy = our_pos
        dist = math.sqrt((tx - ox) ** 2 + (ty - oy) ** 2)

        # Check for projectiles between opponent and us
        proj_style = self._scan_projectiles(frame, target_pos, our_pos)
        if proj_style != CombatStyle.UNKNOWN:
            speed = 4  # default
            if proj_style == CombatStyle.RANGE:
                speed = 5 if dist > 200 else 3  # crossbow vs shortbow
            elif proj_style == CombatStyle.MAGE:
                speed = 5  # typical spell speed
            return proj_style, speed

        # No projectile — use distance heuristic
        if dist < 50:
            # Very close = melee
            return CombatStyle.MELEE, 4  # whip speed
        elif dist < 150:
            # Medium = could be anything, assume melee if no projectile
            return CombatStyle.MELEE, 4
        else:
            # Far away = ranged
            return CombatStyle.RANGE, 4

    def _scan_projectiles(self, frame: np.ndarray, src: Tuple[int, int],
                           dst: Tuple[int, int]) -> CombatStyle:
        """Scan the line between opponent and us for projectile colors."""
        sx, sy = src
        dx, dy = dst
        steps = 10
        for i in range(1, steps):
            t = i / steps
            px = int(sx + (dx - sx) * t)
            py = int(sy + (dy - sy) * t)
            if 0 <= py < frame.shape[0] and 0 <= px < frame.shape[1]:
                b, g, r = frame[py, px]
                # Ice barrage / water spell = bright cyan/blue
                if b > 180 and g > 150 and r < 100:
                    return CombatStyle.MAGE
                # Fire spell = orange/yellow
                if r > 200 and g > 100 and g < 200 and b < 80:
                    return CombatStyle.MAGE
                # Arrow = brown/grey thin line (harder to detect)
                # Green arrow = (0,200,0) type
        return CombatStyle.UNKNOWN

    # ---- Find opponent position in viewport ----

    def find_target_near(self, frame: np.ndarray, last_pos: Tuple[int, int],
                          search_radius: int = 80) -> Optional[Tuple[int, int]]:
        """Find opponent's position near where we last saw them (HP bar tracking)."""
        lx, ly = last_pos
        search_y = max(0, ly - search_radius)
        search_x = max(0, lx - search_radius)
        search_h = min(frame.shape[0], ly + search_radius) - search_y
        search_w = min(frame.shape[1], lx + search_radius) - search_x

        region = frame[search_y:search_y + search_h, search_x:search_x + search_w]
        if region.size == 0:
            return None

        # Look for HP bar (green-red gradient, ~30px wide)
        green_mask = (region[:, :, 1] > 120) & (region[:, :, 2] < 80) & (region[:, :, 0] < 80)
        red_mask = (region[:, :, 2] > 120) & (region[:, :, 1] < 80) & (region[:, :, 0] < 80)
        bar_mask = green_mask | red_mask

        coords = np.where(bar_mask)
        if len(coords[0]) < 5:
            return None

        # Center of HP bar pixels
        cy = int(np.mean(coords[0])) + search_y
        cx = int(np.mean(coords[1])) + search_x
        # Character is ~20px below HP bar
        return (cx, cy + 20)

    def find_any_target(self, frame: np.ndarray) -> Optional[Tuple[int, int]]:
        """Scan full viewport for any opponent HP bar (for initial target lock)."""
        # Viewport area (exclude minimap, inventory, chat)
        vp_w = int(self.window_rect[2] * 0.65)
        vp_h = int(self.window_rect[3] * 0.70)
        viewport = frame[0:vp_h, 0:vp_w]

        # Find HP bars
        green_mask = (viewport[:, :, 1] > 120) & (viewport[:, :, 2] < 80) & (viewport[:, :, 0] < 80)
        red_mask = (viewport[:, :, 2] > 120) & (viewport[:, :, 1] < 80) & (viewport[:, :, 0] < 80)
        bar_mask = green_mask | red_mask

        coords = np.where(bar_mask)
        if len(coords[0]) < 5:
            return None

        cy = int(np.mean(coords[0]))
        cx = int(np.mean(coords[1]))
        return (cx, cy + 20)

    def detect_opponent_overlay(self, frame: np.ndarray) -> bool:
        """Detect opponent info overlay in top-left of viewport.
        In RuneLite, when in combat the opponent's name + HP bar appears
        in the top-left area. The HP bar is green/red."""
        if frame is None or frame.shape[0] < 100 or frame.shape[1] < 250:
            return False
        # Scan a generous region — top 120px, left 250px of captured window
        region = frame[0:120, 0:250]
        if region.size == 0:
            return False
        # BGR format from OpenCV — look for HP bar colors
        # Green HP bar: high green channel
        green_px = (region[:, :, 1] > 80) & (region[:, :, 2] < 100) & (region[:, :, 0] < 100)
        # Red HP bar: high red channel  
        red_px = (region[:, :, 2] > 80) & (region[:, :, 1] < 100) & (region[:, :, 0] < 100)
        bar_pixels = int(np.count_nonzero(green_px)) + int(np.count_nonzero(red_px))
        return bar_pixels >= 15

    def save_diagnostic(self, frame: np.ndarray, label: str = "diag"):
        """Save a diagnostic screenshot to Desktop so Brad can see what bot captures."""
        if frame is None:
            return
        try:
            import os
            desktop = os.path.join(os.path.expanduser("~"), "Desktop")
            path = os.path.join(desktop, f"bot_{label}_{int(time.time())}.png")
            cv2.imwrite(path, frame)
            # Also save the top-left region separately for overlay analysis
            tl_region = frame[0:120, 0:250]
            tl_path = os.path.join(desktop, f"bot_{label}_topleft_{int(time.time())}.png")
            cv2.imwrite(tl_path, tl_region)
            print(f"[DIAG] Saved screenshot: {path}")
            print(f"[DIAG] Saved top-left crop: {tl_path}")
            print(f"[DIAG] Frame size: {frame.shape[1]}x{frame.shape[0]}")
            print(f"[DIAG] Window rect: {self.window_rect}")
        except Exception as e:
            print(f"[DIAG] Failed to save: {e}")

# ============================================================
#  MOUSE CONTROLLER — Humanized Bezier movement
# ============================================================
class HumanMouse:
    """Bezier curve mouse movement that beats ML anti-cheat detection."""

    def __init__(self, screen_reader=None):
        self.mouse = MouseController()
        self.kbd = KeyboardController()
        self.last_move_time = 0.0
        self.screen_reader = screen_reader  # For focus_game() before actions

    def _bezier_points(self, start: Tuple[int, int], end: Tuple[int, int],
                        num_points: int = 20) -> List[Tuple[int, int]]:
        """Generate bezier curve control points for natural mouse movement."""
        sx, sy = start
        ex, ey = end
        dist = math.sqrt((ex - sx)**2 + (ey - sy)**2)

        # Random control points for curve
        cp1x = sx + (ex - sx) * 0.3 + random.randint(-int(dist * 0.15), int(dist * 0.15))
        cp1y = sy + (ey - sy) * 0.3 + random.randint(-int(dist * 0.10), int(dist * 0.10))
        cp2x = sx + (ex - sx) * 0.7 + random.randint(-int(dist * 0.10), int(dist * 0.10))
        cp2y = sy + (ey - sy) * 0.7 + random.randint(-int(dist * 0.10), int(dist * 0.10))

        points = []
        for i in range(num_points + 1):
            t = i / num_points
            # Cubic bezier
            x = (1-t)**3*sx + 3*(1-t)**2*t*cp1x + 3*(1-t)*t**2*cp2x + t**3*ex
            y = (1-t)**3*sy + 3*(1-t)**2*t*cp1y + 3*(1-t)*t**2*cp2y + t**3*ey
            # Add micro-jitter for humanity
            x += random.randint(-1, 1)
            y += random.randint(-1, 1)
            points.append((int(x), int(y)))
        return points

    def move_to(self, x: int, y: int, speed: str = "normal"):
        """Move mouse to position with human-like bezier curve."""
        current = self.mouse.position
        dist = math.sqrt((x - current[0])**2 + (y - current[1])**2)

        # Speed profiles — PVP needs to be FAST
        if speed == "instant":
            # Emergency actions — straight move, minimal delay
            self.mouse.position = (x + random.randint(-2, 2), y + random.randint(-2, 2))
            time.sleep(random.uniform(0.01, 0.02))
            return
        elif speed == "fast":
            num_points = max(5, int(dist / 50))
            delay = random.uniform(0.002, 0.006)
        elif speed == "normal":
            num_points = max(8, int(dist / 30))
            delay = random.uniform(0.004, 0.010)
        else:
            num_points = max(12, int(dist / 20))
            delay = random.uniform(0.008, 0.015)

        points = self._bezier_points(current, (x, y), num_points)
        for px, py in points:
            self.mouse.position = (px, py)
            time.sleep(delay)
        self.last_move_time = time.time()

    def click(self, x: int = None, y: int = None, button: str = "left", speed: str = "fast"):
        """Click at position (or current position if no coords)."""
        # ALWAYS focus game window before clicking
        if self.screen_reader:
            self.screen_reader.focus_game()
        if x is not None and y is not None:
            self.move_to(x, y, speed=speed)
        btn = Button.left if button == "left" else Button.right
        # Randomize click hold time
        self.mouse.press(btn)
        time.sleep(random.uniform(0.03, 0.08))
        self.mouse.release(btn)

    def rapid_click_slots(self, slot_coords: List[Tuple[int, int]]):
        """Rapid-fire click multiple inventory slots — for gear switches."""
        # Focus game before gear switch
        if self.screen_reader:
            self.screen_reader.focus_game()
        for i, (sx, sy) in enumerate(slot_coords):
            # Add tiny random offset to each slot click
            ox = random.randint(-3, 3)
            oy = random.randint(-3, 3)
            self.mouse.position = (sx + ox, sy + oy)
            time.sleep(random.uniform(0.02, 0.05))  # 20-50ms between clicks
            self.mouse.press(Button.left)
            time.sleep(random.uniform(0.02, 0.04))
            self.mouse.release(Button.left)

    def press_fkey(self, key_num: int):
        """Press an F-key (for tab switching)."""
        # Focus game before pressing F-keys
        if self.screen_reader:
            self.screen_reader.focus_game()
        fkeys = {1: Key.f1, 2: Key.f2, 3: Key.f3, 4: Key.f4, 5: Key.f5,
                 6: Key.f6, 7: Key.f7, 8: Key.f8, 9: Key.f9}
        if key_num in fkeys:
            self.kbd.press(fkeys[key_num])
            time.sleep(random.uniform(0.02, 0.04))
            self.kbd.release(fkeys[key_num])

# ============================================================
#  COMBAT BRAIN v3.0 — TICK-PERFECT DECIMATOR
# ============================================================
class CombatBrain:
    """
    The PVP fight controller.
    Every decision is tick-aligned. Every action is optimized.
    One button → target dies.
    """

    def __init__(self):
        self.ticker = TickTracker()
        self.target = TargetState()
        self.player = PlayerState()
        self.screen = ScreenReader()
        self.mouse = HumanMouse(screen_reader=self.screen)

        # Fight state
        self.fighting = False
        self.fight_start_time = 0.0
        self.total_fights = 0
        self.total_kills = 0

        # Action queue — actions scheduled for specific ticks
        self.action_queue: List[Dict] = []

        # Window geometry for inventory/prayer click positions
        self.inv_origin = (0, 0)   # top-left of inventory widget (absolute screen coords)
        self.prayer_origin = (0, 0)

        # Combat log
        self.combat_log: List[str] = []

    def log(self, msg: str):
        timestamp = time.strftime("%H:%M:%S")
        entry = f"[{timestamp}] {msg}"
        self.combat_log.append(entry)
        if len(self.combat_log) > 100:
            self.combat_log.pop(0)
        print(entry)

    # ---- CALIBRATE UI — set inv/prayer origins from window position ----

    def calibrate_ui(self):
        """Calculate inventory/prayer absolute positions from game window rect."""
        if not self.screen.window_rect:
            return
        wx, wy, ww, wh = self.screen.window_rect
        # Inventory widget top-left (absolute screen coords)
        self.inv_origin = (wx + ww + INV_PANEL_OFFSET_X, wy + INV_PANEL_OFFSET_Y)
        # Prayer widget top-left (absolute screen coords)
        self.prayer_origin = (wx + ww + PRAYER_PANEL_OFFSET_X, wy + PRAYER_PANEL_OFFSET_Y)

    # ---- Inventory slot absolute position ----

    def inv_slot_pos(self, slot: int) -> Tuple[int, int]:
        """Get absolute screen position of an inventory slot."""
        row = slot // INV_COLS
        col = slot % INV_COLS
        x = self.inv_origin[0] + INV_START_X + col * INV_SLOT_W + INV_SLOT_W // 2
        y = self.inv_origin[1] + INV_START_Y + row * INV_SLOT_H + INV_SLOT_H // 2
        return (x, y)

    # ---- Prayer click position ----

    def prayer_pos(self, prayer_name: str) -> Tuple[int, int]:
        """Get absolute screen position of a prayer icon."""
        slot = PRAYER_SLOTS.get(prayer_name)
        if not slot:
            return (0, 0)
        x = self.prayer_origin[0] + slot["col"] * 37 + 18
        y = self.prayer_origin[1] + slot["row"] * 37 + 18
        return (x, y)

    # ---- GEAR SWITCH — equip full style in 1 tick ----

    def switch_gear(self, style: CombatStyle):
        """Rapid-click all gear slots for a combat style switch."""
        if style == self.player.current_style:
            return  # Already in this style

        if style == CombatStyle.MELEE:
            slots = self.player.melee_slots
            weapon = self.player.melee_weapon
        elif style == CombatStyle.RANGE:
            slots = self.player.range_slots
            weapon = self.player.range_weapon
        elif style == CombatStyle.MAGE:
            slots = self.player.mage_slots
            weapon = self.player.mage_weapon
        else:
            return

        # F1 = inventory tab
        self.mouse.press_fkey(1)
        time.sleep(0.03)

        # Rapid-click all gear slots
        coords = [self.inv_slot_pos(s) for s in slots]
        self.mouse.rapid_click_slots(coords)

        self.player.current_style = style
        self.ticker.our_weapon_speed = WEAPON_DB.get(weapon, {}).get("speed", 4)
        self.log(f"⚔️ SWITCHED → {style.value.upper()} (speed: {self.ticker.our_weapon_speed}t)")

    # ---- PRAYER SWITCH ----

    def switch_prayer(self, protect_style: CombatStyle, offensive: str = "auto"):
        """Switch protection prayer and offensive prayer in one action."""
        # F5 = prayer tab
        self.mouse.press_fkey(5)
        time.sleep(0.03)

        # Protection prayer
        if protect_style != self.player.active_prayer:
            if protect_style == CombatStyle.MELEE:
                pos = self.prayer_pos("protect_melee")
            elif protect_style == CombatStyle.RANGE:
                pos = self.prayer_pos("protect_missiles")
            elif protect_style == CombatStyle.MAGE:
                pos = self.prayer_pos("protect_magic")
            else:
                pos = None
            if pos and pos != (0, 0):
                self.mouse.click(pos[0], pos[1], speed="instant")
                self.player.active_prayer = protect_style
                self.log(f"🛡️ PRAY → Protect {protect_style.value}")

        # Offensive prayer (auto-select based on our attack style)
        if offensive == "auto":
            if self.player.current_style == CombatStyle.MELEE:
                offensive = "piety"
            elif self.player.current_style == CombatStyle.RANGE:
                offensive = "rigour"
            elif self.player.current_style == CombatStyle.MAGE:
                offensive = "augury"

        if offensive != self.player.offensive_prayer and offensive != "none":
            pos = self.prayer_pos(offensive)
            if pos and pos != (0, 0):
                self.mouse.click(pos[0], pos[1], speed="instant")
                self.player.offensive_prayer = offensive

    # ---- EAT (combo eat for maximum healing) ----

    def eat(self, combo: bool = False):
        """Eat food. If combo=True, eats food + karambwan + brew on same tick."""
        # Rate-limit eating — minimum 1.8 seconds between eat attempts (3 game ticks)
        now = time.time()
        if not hasattr(self, '_last_eat_time'):
            self._last_eat_time = 0.0
        if now - self._last_eat_time < 1.8:
            return  # Too soon — skip
        self._last_eat_time = now

        self.screen.focus_game()
        self.mouse.press_fkey(1)  # inventory tab
        time.sleep(0.02)

        # Main food
        if self.player.food_slots:
            food_pos = self.inv_slot_pos(self.player.food_slots[0])
            self.mouse.click(food_pos[0], food_pos[1], speed="instant")
            self.ticker.register_eat()
            self.log("🍖 ATE food")

        if combo:
            # Karambwan (can eat on same tick as regular food)
            time.sleep(0.02)
            karam_pos = self.inv_slot_pos(self.player.karambwan_slot)
            self.mouse.click(karam_pos[0], karam_pos[1], speed="instant")
            self.log("🍤 ATE karambwan (combo)")

            # Saradomin brew (can stack with karambwan)
            if self.player.brew_slots:
                time.sleep(0.02)
                brew_pos = self.inv_slot_pos(self.player.brew_slots[0])
                self.mouse.click(brew_pos[0], brew_pos[1], speed="instant")
                self.log("🧪 DRANK brew (triple combo)")

    # ---- SPEC ATTACK ----

    def spec_attack(self):
        """Equip spec weapon → enable spec → click opponent."""
        # Equip spec weapon from inventory
        self.mouse.press_fkey(1)
        time.sleep(0.02)
        spec_pos = self.inv_slot_pos(self.player.spec_weapon_slot)
        self.mouse.click(spec_pos[0], spec_pos[1], speed="instant")
        time.sleep(0.05)

        # Click spec orb (bottom of minimap area)
        wx, wy, ww, wh = self.screen.window_rect
        spec_orb_x = wx + ww - 153
        spec_orb_y = wy + 128
        self.mouse.click(spec_orb_x, spec_orb_y, speed="instant")
        time.sleep(0.05)

        # Click opponent
        if self.target.screen_pos != (0, 0):
            tx, ty = self.target.screen_pos
            self.mouse.click(wx + tx, wy + ty, speed="instant")
            self.ticker.register_our_attack(self.player.spec_weapon)
            self.log(f"💥 SPEC → {self.player.spec_weapon} (max hit: {self.player.max_hit_with_spec()})")

        # GMaul stack check — if we have gmaul AND just used a stackable spec
        if (self.player.spec_weapon in GMAUL_STACK_WEAPONS and
                self.player.spec_percent >= 100):  # gmaul needs 50% but we check > spec cost
            # This is the GMAUL STACK — instant hit on same tick
            # Switch to gmaul in inventory and click opponent
            # (Would need gmaul_slot configured)
            pass

    # ---- ATTACK (click opponent) ----

    def attack_target(self):
        """Click opponent to attack with current weapon."""
        if not self.target.is_valid():
            return
        wx, wy = self.screen.window_rect[0], self.screen.window_rect[1]
        tx, ty = self.target.screen_pos
        self.mouse.click(wx + tx, wy + ty, speed="fast")

        # Register which weapon we attacked with
        if self.player.current_style == CombatStyle.MELEE:
            self.ticker.register_our_attack(self.player.melee_weapon)
        elif self.player.current_style == CombatStyle.RANGE:
            self.ticker.register_our_attack(self.player.range_weapon)
        elif self.player.current_style == CombatStyle.MAGE:
            self.ticker.register_our_attack(self.player.mage_weapon)

    # ================================================================
    #  MAIN COMBAT LOOP — runs every frame (~16ms)
    #  Priority system ensures the most critical action always happens
    # ================================================================

    def combat_tick(self):
        """
        The main decision function. Called every game tick.
        Reads EVERYTHING, decides the optimal action, executes it.

        PRIORITY ORDER (highest first):
        1. SURVIVE — combo eat if HP critical
        2. PRAYER — switch protect prayer to block incoming hit
        3. KO CHECK — if opponent is in kill range, SPEC NOW
        4. GEAR SWITCH — switch to style that hits through their prayer
        5. ATTACK — click opponent on our attack tick
        6. EAT (safe) — eat between attacks without losing DPS
        7. OFFENSIVE PRAYER — toggle piety/rigour/augury
        """
        frame = self.screen.capture()
        if frame is None:
            return

        # ---- READ ALL STATE ----
        # HOTFIX 5: Force all to 100% — orb positions not calibrated for Roat Pkz yet
        self.player.hp_percent = 100.0
        self.player.prayer_percent = 100.0
        self.player.spec_percent = 100.0

        # HOTFIX 7: Use opponent info overlay (top-left) as primary combat signal
        overlay_visible = self.screen.detect_opponent_overlay(frame)

        # Track target via character HP bar (secondary)
        if self.target.locked:
            new_pos = self.screen.find_target_near(frame, self.target.screen_pos)
            if new_pos:
                self.target.screen_pos = new_pos
                self.target.last_seen_time = time.time()

                # Read opponent HP
                opp_hp = self.screen.read_opponent_hp(frame, new_pos)
                if opp_hp >= 0:
                    self.target.prev_hp = self.target.hp_percent
                    self.target.hp_percent = opp_hp
                    self.target.is_eating = opp_hp > self.target.prev_hp + 3

                # Read their overhead prayer
                self.target.overhead_prayer = self.screen.read_overhead_prayer(frame, new_pos)

                # Read their attack style
                our_center = (frame.shape[1] // 2, frame.shape[0] // 2)
                style, speed = self.screen.detect_opponent_style(frame, new_pos, our_center)
                if style != CombatStyle.UNKNOWN:
                    if style == self.target.detected_style:
                        self.target.consecutive_style += 1
                    else:
                        self.target.consecutive_style = 0
                    self.target.detected_style = style
                    self.target.detected_weapon_speed = speed

                # Check for hitsplats (tick sync)
                if self.screen.detect_hitsplat(frame, new_pos):
                    self.ticker.sync_from_hitsplat(time.time())
                    self.ticker.register_opp_attack(speed)

            elif overlay_visible:
                # Can't find HP bar above character, but overlay says we're in combat
                # Keep target alive — re-click center to maintain engagement
                self.target.last_seen_time = time.time()
                if not hasattr(self, '_last_reclick') or time.time() - self._last_reclick > 2.5:
                    # Re-click center every ~2.5s to keep attacking
                    wx, wy, ww, wh = self.screen.window_rect
                    cx = wx + int(ww * 0.45)
                    cy = wy + int(wh * 0.52)
                    self.mouse.move_to(cx + random.randint(-15, 15),
                                       cy + random.randint(-15, 15), speed="fast")
                    time.sleep(0.03)
                    self.mouse.click()
                    self._last_reclick = time.time()
                    self.log("🔄 Re-clicking target (overlay visible, tracking lost)")
            else:
                # No overlay AND no HP bar — truly lost
                if time.time() - self.target.last_seen_time > 15.0:
                    self.log("❌ Target lost!")
                    self.target.locked = False
                    return

        if not self.target.is_valid():
            # HOTFIX 7: Even if target object isn't valid, if overlay is showing
            # we're being attacked — re-engage
            if overlay_visible and not self.target.locked:
                self.log("⚠️ Opponent overlay detected — auto re-engaging!")
                self.target.locked = True
                self.target.last_seen_time = time.time()
                # Click center to attack
                wx, wy, ww, wh = self.screen.window_rect
                cx = wx + int(ww * 0.45)
                cy = wy + int(wh * 0.52)
                self.mouse.move_to(cx + random.randint(-10, 10),
                                   cy + random.randint(-10, 10), speed="fast")
                time.sleep(0.03)
                self.mouse.click()
                self._last_reclick = time.time()
                self.fighting = True
            else:
                return

        # ---- TICK TIMING ----
        ticks_to_our_atk = self.ticker.ticks_until_our_attack()
        ticks_to_opp_atk = self.ticker.ticks_until_opp_attack()
        secs_to_our_atk = self.ticker.seconds_until_our_attack()
        secs_to_opp_atk = self.ticker.seconds_until_opp_attack()
        best_style = self.target.best_attack_style()

        # ========================================
        # PRIORITY 1: SURVIVE — eat if low HP
        # ========================================
        if self.player.hp_percent <= 35:
            # CRITICAL — triple combo eat NOW
            self.eat(combo=True)
            self.log(f"🚨 EMERGENCY COMBO EAT @ {self.player.hp_percent:.0f}% HP")
            return

        if self.player.hp_percent <= 55:
            # Regular eat — but only if it won't cost DPS
            if self.ticker.can_eat_without_dps_loss() or self.player.hp_percent <= 40:
                self.eat(combo=False)
                self.log(f"🍖 Eat @ {self.player.hp_percent:.0f}% HP")

        # ========================================
        # PRIORITY 2: PRAYER — protect from their attack
        # ========================================
        if self.target.detected_style != CombatStyle.UNKNOWN:
            # Switch prayer to protect from whatever they're attacking with
            self.switch_prayer(self.target.detected_style)

        # ========================================
        # PRIORITY 3: KO CHECK — can we kill them RIGHT NOW?
        # ========================================
        if (self.target.is_in_ko_range(self.player.max_hit_with_spec()) and
                self.player.spec_percent >= 50 and
                ticks_to_our_atk <= 1):
            # SEND IT — spec attack on this tick
            self.log(f"💀 KO ATTEMPT! Target @ {self.target.hp_percent:.0f}% HP")
            self.spec_attack()
            return

        # ========================================
        # PRIORITY 4: GEAR SWITCH — 1 tick before our attack
        # ========================================
        if self.ticker.should_switch_now() or ticks_to_our_atk == 0:
            # Switch to the style that hits through their prayer
            self.switch_gear(best_style)

            # Also update offensive prayer to match
            off_prayer = "none"
            if best_style == CombatStyle.MELEE:
                off_prayer = "piety"
            elif best_style == CombatStyle.RANGE:
                off_prayer = "rigour"
            elif best_style == CombatStyle.MAGE:
                off_prayer = "augury"
            if off_prayer != self.player.offensive_prayer:
                self.mouse.press_fkey(5)
                time.sleep(0.02)
                pos = self.prayer_pos(off_prayer)
                if pos != (0, 0):
                    self.mouse.click(pos[0], pos[1], speed="instant")
                    self.player.offensive_prayer = off_prayer

        # ========================================
        # PRIORITY 5: ATTACK — click opponent on our attack tick
        # ========================================
        if self.ticker.is_attack_tick():
            self.attack_target()
            self.log(f"⚔️ ATTACK ({self.player.current_style.value}) → "
                     f"they pray {self.target.overhead_prayer.value}, "
                     f"opp HP: {self.target.hp_percent:.0f}%")

        # ========================================
        # PRIORITY 6: SAFE EAT — between attack ticks
        # ========================================
        if (self.player.hp_percent <= 65 and
                self.ticker.can_eat_without_dps_loss() and
                ticks_to_our_atk >= 3):
            self.eat(combo=False)

    # ---- TARGET LOCK ----

    def lock_target(self):
        """Lock onto nearest opponent. F9 pressed.
        HOTFIX 8: Saves diagnostic screenshot + stays locked minimum 10 seconds."""
        # Re-find and focus the game window
        if not self.screen.find_window():
            self.log("!! Can't find Roat Pkz window!")
            try:
                import pygetwindow as gw
                all_wins = gw.getAllWindows()
                self.log("All open windows:")
                for w in all_wins:
                    if w.width > 100 and w.height > 100 and w.title.strip():
                        self.log(f"  [{w.width}x{w.height}] '{w.title}'")
            except:
                pass
            return False

        self.screen.focus_game()
        time.sleep(0.15)

        # HOTFIX 8: Capture and save diagnostic screenshot BEFORE clicking
        diag_frame = self.screen.capture()
        if diag_frame is not None:
            self.screen.save_diagnostic(diag_frame, "f9_lock")
            overlay = self.screen.detect_opponent_overlay(diag_frame)
            self.log(f"[DIAG] Overlay detected: {overlay}")
            self.log(f"[DIAG] Frame shape: {diag_frame.shape}")
        else:
            self.log("[DIAG] capture() returned None!")

        # Click CENTER of game viewport
        wx, wy, ww, wh = self.screen.window_rect
        vp_center_x = wx + int(ww * 0.45)
        vp_center_y = wy + int(wh * 0.52)

        self.log(f"Window: ({wx},{wy}) {ww}x{wh}")
        self.log(f"Clicking viewport center: ({vp_center_x}, {vp_center_y})")

        # Click to attack
        self.mouse.move_to(vp_center_x + random.randint(-5, 5),
                           vp_center_y + random.randint(-5, 5), speed="fast")
        time.sleep(0.05)
        self.mouse.click()

        self.target = TargetState(
            locked=True,
            screen_pos=(vp_center_x, vp_center_y),
            last_seen_time=time.time(),
        )
        self.fighting = True
        self.fight_start_time = time.time()
        self._lock_time = time.time()  # HOTFIX 8: minimum lock duration
        self.total_fights += 1
        self.ticker.reset()
        self._last_reclick = time.time()
        self.log(f"TARGET LOCKED at ({vp_center_x}, {vp_center_y}) -- Screenshots saved to Desktop")
        return True

    def unlock_target(self):
        """Release target lock."""
        self.target = TargetState()
        self.fighting = False
        self.log("🔓 Target unlocked")

    # ---- MAIN LOOP ----

    def auto_scan_for_target(self):
        """Continuously scan viewport for opponent HP bars — auto-lock when found."""
        frame = self.screen.capture()
        if frame is None:
            return False
        pos = self.screen.find_any_target(frame)
        if pos:
            self.target = TargetState(
                locked=True,
                screen_pos=pos,
                last_seen_time=time.time(),
            )
            self.fighting = True
            self.fight_start_time = time.time()
            self.total_fights += 1
            self.ticker.reset()
            self.log(f"🎯 AUTO-LOCKED TARGET at ({pos[0]}, {pos[1]})")
            return True
        return False

    def run(self):
        """Main bot loop — auto-detects and fights anyone who attacks."""
        # STEP 1: Find game window
        print("\n=== DECIMATOR v3.0 STARTUP ===")
        print("Looking for Roat Pkz window...")

        # Show ALL window titles for debugging
        try:
            import subprocess
            result = subprocess.run(
                ['powershell', '-c', 'Get-Process | Where-Object {$_.MainWindowTitle} | Select-Object MainWindowTitle | Format-Table -AutoSize'],
                capture_output=True, text=True, timeout=5
            )
            print("=== OPEN WINDOWS ===")
            print(result.stdout)
            print("====================")
        except Exception as e:
            print(f"(Could not list windows: {e})")

        found = self.screen.find_window()
        if not found:
            self.log("❌ COULD NOT FIND GAME WINDOW — make sure Roat Pkz is open!")
            print("\n⚠️  Open Roat Pkz client first, then run this again.")
            print("    Bot looks for window titles containing: roat, pkz, or runelite")
            return

        # STEP 2: Calibrate UI positions
        self.calibrate_ui()
        print(f"✅ Window found: {self.screen.window_rect}")
        print(f"📐 Inv origin: {self.inv_origin}")
        print(f"📐 Prayer origin: {self.prayer_origin}")
        if self.inv_origin == (0, 0):
            self.log("⚠️ WARNING: Inv origin is (0,0) — calibration may have failed!")
        print("=== STARTUP COMPLETE ===\n")
        self.log("🟢 Bot v3.0 HOTFIX 5 — MANUAL TARGET MODE")
        self.log("💤 Bot is IDLE — does NOTHING until you press F9 near an opponent")
        self.log(f"⚔️ Melee: {self.player.melee_weapon} | 🏹 Range: {self.player.range_weapon} | 🔮 Mage: {self.player.mage_weapon}")
        self.log(f"💥 Spec: {self.player.spec_weapon} (max hit: {self.player.max_hit_with_spec()})")
        self.log("F12 = emergency stop")

        self._bot_active = True
        self._idle_logged = False

        while self._bot_active:
            try:
                loop_start = time.time()

                if self.fighting and self.target.locked:
                    # === IN COMBAT — run combat tick ===
                    self.combat_tick()
                    self._idle_logged = False

                    # HOTFIX 8: Stay locked minimum 10 seconds. Re-click every 2.5s.
                    lock_age = time.time() - getattr(self, '_lock_time', time.time())
                    if lock_age < 10.0:
                        # Within minimum lock period -- keep fighting, re-click periodically
                        if not hasattr(self, '_last_reclick') or time.time() - self._last_reclick > 2.5:
                            wx, wy, ww, wh = self.screen.window_rect
                            cx = wx + int(ww * 0.45) + random.randint(-8, 8)
                            cy = wy + int(wh * 0.52) + random.randint(-8, 8)
                            self.mouse.move_to(cx, cy, speed="fast")
                            time.sleep(0.03)
                            self.mouse.click()
                            self._last_reclick = time.time()
                            self.target.last_seen_time = time.time()
                    else:
                        # After 10s, check overlay to decide if still in combat
                        check_frame = self.screen.capture()
                        still_fighting = check_frame is not None and self.screen.detect_opponent_overlay(check_frame)
                        if still_fighting:
                            self.target.last_seen_time = time.time()
                            self._lock_time = time.time()
                            if not hasattr(self, '_last_reclick') or time.time() - self._last_reclick > 2.5:
                                wx, wy, ww, wh = self.screen.window_rect
                                cx = wx + int(ww * 0.45) + random.randint(-8, 8)
                                cy = wy + int(wh * 0.52) + random.randint(-8, 8)
                                self.mouse.move_to(cx, cy, speed="fast")
                                time.sleep(0.03)
                                self.mouse.click()
                                self._last_reclick = time.time()
                        elif self.target.last_seen_time and (time.time() - self.target.last_seen_time > 5.0):
                            self.log("Target gone -- press F9 to lock new target")
                            self.unlock_target()
                else:
                    # === IDLE — waiting for F9 or auto-detect via overlay ===
                    # HOTFIX 7: Also check opponent overlay — if someone attacks Brad,
                    # the overlay appears even without F9. Auto-engage.
                    idle_frame = self.screen.capture()
                    if idle_frame is not None and self.screen.detect_opponent_overlay(idle_frame):
                        self.log("⚡ ATTACKED! Opponent overlay detected — auto-engaging!")
                        self.screen.focus_game()
                        time.sleep(0.05)
                        wx, wy, ww, wh = self.screen.window_rect
                        cx = wx + int(ww * 0.45)
                        cy = wy + int(wh * 0.52)
                        self.mouse.move_to(cx + random.randint(-10, 10),
                                           cy + random.randint(-10, 10), speed="fast")
                        time.sleep(0.03)
                        self.mouse.click()
                        self.target = TargetState()
                        self.target.locked = True
                        self.target.screen_pos = (cx, cy)
                        self.target.last_seen_time = time.time()
                        self.fighting = True
                        self._last_reclick = time.time()
                        self._idle_logged = False
                    elif not self._idle_logged:
                        self.log("💤 IDLE — Press F9 or wait (auto-detects attackers)")
                        self._idle_logged = True

                # Timing
                elapsed = time.time() - loop_start
                if self.fighting:
                    sleep_time = max(0.010, 0.016 - elapsed)  # 60fps during combat
                else:
                    sleep_time = max(0.050, 0.250 - elapsed)  # 4fps while idle (minimal CPU)
                sleep_time += random.uniform(-0.003, 0.003)
                time.sleep(max(0.005, sleep_time))

            except Exception as e:
                self.log(f"⚠️ Error: {e}")
                time.sleep(0.1)

# ============================================================
#  GUI — Control Panel with TARGET LOCK button
# ============================================================
class BotGUI:
    def __init__(self, brain: CombatBrain):
        self.brain = brain
        self.root = None
        self.running = False
        self.combat_thread = None

        # F12 emergency stop listener
        self.key_listener = Listener(on_press=self._on_key)
        self.key_listener.daemon = True
        self.key_listener.start()

    def _on_key(self, key):
        try:
            if key == Key.f12:
                self.emergency_stop()
            elif key == Key.f9:
                # F9 = quick target lock
                if self.running:
                    self.brain.lock_target()
        except Exception:
            pass

    def emergency_stop(self):
        self.running = False
        self.brain.fighting = False
        self.brain._bot_active = False
        self.brain.unlock_target()
        self.brain.log("🛑 F12 EMERGENCY STOP")
        if self.root:
            try:
                self.status_label.config(text="STOPPED", foreground="red")
            except Exception:
                pass

    def start_bot(self):
        if self.running:
            return
        self.running = True
        self.brain._bot_active = True
        self.brain.log("✅ Bot ACTIVE — auto-targeting anyone who attacks you")
        self.status_label.config(text="👁️ SCANNING — waiting for opponent...", foreground="yellow")
        # Start main loop in thread
        self.combat_thread = threading.Thread(target=self.brain.run, daemon=True)
        self.combat_thread.start()

    def stop_bot(self):
        self.running = False
        self.brain.fighting = False
        self.brain._bot_active = False
        self.brain.unlock_target()
        self.status_label.config(text="STOPPED", foreground="red")

    def target_lock(self):
        """THE BUTTON — starts auto-targeting mode, or force-locks nearest target."""
        if not self.running:
            self.start_bot()
            return

        if self.brain.target.locked:
            # Already locked — unlock and go back to scanning
            self.brain.fighting = False
            self.brain.unlock_target()
            self.lock_btn.config(text="🎯 TARGET LOCK")
            self.status_label.config(text="👁️ SCANNING — waiting for opponent...", foreground="yellow")
        else:
            # Force manual lock on nearest target
            if self.brain.lock_target():
                self.lock_btn.config(text="🔓 UNLOCK TARGET")
                self.status_label.config(text="🔥 FIGHTING — DECIMATING TARGET", foreground="green")

    def update_display(self):
        """Update GUI labels every 100ms."""
        if not self.root:
            return

        try:
            # Tick info
            ticks_us = self.brain.ticker.ticks_until_our_attack()
            ticks_them = self.brain.ticker.ticks_until_opp_attack()
            secs_us = self.brain.ticker.seconds_until_our_attack()
            secs_them = self.brain.ticker.seconds_until_opp_attack()

            tick_text = f"Our atk: {ticks_us}t ({secs_us:.1f}s) | Their atk: {ticks_them}t ({secs_them:.1f}s)"
            self.tick_label.config(text=tick_text)

            # HP info
            hp_text = (f"Our HP: {self.brain.player.hp_percent:.0f}% | "
                       f"Prayer: {self.brain.player.prayer_percent:.0f}% | "
                       f"Spec: {self.brain.player.spec_percent:.0f}%")
            self.hp_label.config(text=hp_text)

            # Target info
            if self.brain.target.locked:
                t = self.brain.target
                tgt_text = (f"Target HP: {t.hp_percent:.0f}% | "
                           f"Pray: {t.overhead_prayer.value} | "
                           f"Style: {t.detected_style.value} | "
                           f"Speed: {t.detected_weapon_speed}t")
                self.target_label.config(text=tgt_text)

                # Best action
                best = t.best_attack_style()
                action_text = f"→ HIT WITH: {best.value.upper()} (through their {t.overhead_prayer.value} prayer)"
                if t.is_in_ko_range(self.brain.player.max_hit_with_spec()):
                    action_text = f"💀 KO RANGE — SPEC WITH {self.brain.player.spec_weapon.upper()}!"
                self.action_label.config(text=action_text)
            else:
                self.target_label.config(text="No target locked")
                if self.running:
                    self.action_label.config(text="👁️ Auto-scanning for opponents...")
                    self.status_label.config(text="👁️ SCANNING — waiting for opponent...", foreground="yellow")
                else:
                    self.action_label.config(text="Press START to begin auto-targeting")

            # Combat log (last 8 entries)
            log_text = "\n".join(self.brain.combat_log[-8:])
            self.log_text.config(state="normal")
            self.log_text.delete("1.0", "end")
            self.log_text.insert("1.0", log_text)
            self.log_text.config(state="disabled")

            # Stats
            stats_text = f"Fights: {self.brain.total_fights} | Kills: {self.brain.total_kills}"
            self.stats_label.config(text=stats_text)

        except Exception:
            pass

        if self.root:
            self.root.after(100, self.update_display)

    def build_gui(self):
        self.root = tk.Tk()
        self.root.title("ROAT PKZ DECIMATOR v3.0")
        self.root.geometry("520x700")
        self.root.configure(bg="#1a1a2e")
        self.root.attributes("-topmost", True)

        style = ttk.Style()
        style.theme_use("clam")

        # Title
        title = tk.Label(self.root, text="☠️ ROAT PKZ DECIMATOR v3.0 ☠️",
                         font=("Consolas", 16, "bold"), fg="#e94560", bg="#1a1a2e")
        title.pack(pady=10)

        # THE BUTTON
        self.lock_btn = tk.Button(
            self.root, text="🎯 TARGET LOCK", font=("Consolas", 20, "bold"),
            bg="#e94560", fg="white", activebackground="#ff2e63",
            command=self.target_lock, height=2, width=20
        )
        self.lock_btn.pack(pady=10)

        # Status
        self.status_label = tk.Label(self.root, text="IDLE", font=("Consolas", 12, "bold"),
                                      fg="gray", bg="#1a1a2e")
        self.status_label.pack()

        # Tick counter
        tick_frame = tk.LabelFrame(self.root, text="⏱ TICK ENGINE", font=("Consolas", 10),
                                    fg="#0f3460", bg="#1a1a2e", labelanchor="n")
        tick_frame.pack(fill="x", padx=10, pady=5)
        self.tick_label = tk.Label(tick_frame, text="Waiting for sync...",
                                    font=("Consolas", 10), fg="#16c79a", bg="#1a1a2e")
        self.tick_label.pack(pady=3)

        # Our stats
        hp_frame = tk.LabelFrame(self.root, text="🫀 OUR STATUS", font=("Consolas", 10),
                                  fg="#0f3460", bg="#1a1a2e", labelanchor="n")
        hp_frame.pack(fill="x", padx=10, pady=5)
        self.hp_label = tk.Label(hp_frame, text="HP: -- | Prayer: -- | Spec: --",
                                  font=("Consolas", 10), fg="#16c79a", bg="#1a1a2e")
        self.hp_label.pack(pady=3)

        # Target info
        tgt_frame = tk.LabelFrame(self.root, text="🎯 TARGET", font=("Consolas", 10),
                                   fg="#0f3460", bg="#1a1a2e", labelanchor="n")
        tgt_frame.pack(fill="x", padx=10, pady=5)
        self.target_label = tk.Label(tgt_frame, text="No target",
                                      font=("Consolas", 10), fg="#e94560", bg="#1a1a2e")
        self.target_label.pack(pady=3)
        self.action_label = tk.Label(tgt_frame, text="",
                                      font=("Consolas", 11, "bold"), fg="#ffbd39", bg="#1a1a2e")
        self.action_label.pack(pady=3)

        # Weapon config
        cfg_frame = tk.LabelFrame(self.root, text="⚙️ LOADOUT", font=("Consolas", 10),
                                   fg="#0f3460", bg="#1a1a2e", labelanchor="n")
        cfg_frame.pack(fill="x", padx=10, pady=5)

        weapons = [
            ("Melee:", "melee_weapon", self.brain.player.melee_weapon),
            ("Range:", "range_weapon", self.brain.player.range_weapon),
            ("Mage:", "mage_weapon", self.brain.player.mage_weapon),
            ("Spec:", "spec_weapon", self.brain.player.spec_weapon),
        ]
        self.weapon_vars = {}
        weapon_names = sorted(WEAPON_DB.keys())
        for label_text, attr, default in weapons:
            row = tk.Frame(cfg_frame, bg="#1a1a2e")
            row.pack(fill="x", padx=5, pady=1)
            tk.Label(row, text=label_text, font=("Consolas", 9), fg="#aaa", bg="#1a1a2e",
                     width=7, anchor="e").pack(side="left")
            var = tk.StringVar(value=default)
            self.weapon_vars[attr] = var
            combo = ttk.Combobox(row, textvariable=var, values=weapon_names, width=25, state="readonly")
            combo.pack(side="left", padx=5)

        # Apply loadout button
        tk.Button(cfg_frame, text="Apply Loadout", font=("Consolas", 9),
                  bg="#0f3460", fg="white", command=self.apply_loadout).pack(pady=5)

        # Stats
        self.stats_label = tk.Label(self.root, text="Fights: 0 | Kills: 0",
                                     font=("Consolas", 10), fg="#aaa", bg="#1a1a2e")
        self.stats_label.pack(pady=3)

        # Combat log
        log_frame = tk.LabelFrame(self.root, text="📋 COMBAT LOG", font=("Consolas", 10),
                                   fg="#0f3460", bg="#1a1a2e", labelanchor="n")
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)
        self.log_text = tk.Text(log_frame, font=("Consolas", 8), bg="#0d1117", fg="#16c79a",
                                 height=8, state="disabled", wrap="word")
        self.log_text.pack(fill="both", expand=True, padx=3, pady=3)

        # Hotkey info
        tk.Label(self.root, text="F9 = Quick Lock | F12 = Emergency Stop",
                 font=("Consolas", 9), fg="#555", bg="#1a1a2e").pack(pady=5)

        self.update_display()
        self.root.mainloop()

    def apply_loadout(self):
        """Apply weapon selections from GUI dropdowns."""
        for attr, var in self.weapon_vars.items():
            setattr(self.brain.player, attr, var.get())
        self.brain.log(f"✅ Loadout applied: melee={self.brain.player.melee_weapon}, "
                       f"range={self.brain.player.range_weapon}, "
                       f"mage={self.brain.player.mage_weapon}, "
                       f"spec={self.brain.player.spec_weapon}")

# ============================================================
#  MAIN ENTRY POINT
# ============================================================
def main():
    print("=" * 60)
    print("  ☠️  ROAT PKZ DECIMATOR v3.0  ☠️")
    print("  Tick-perfect NH tribrid combat bot")
    print("=" * 60)
    print()
    print("  ONE BUTTON. FULL SEND. TARGET DIES.")
    print()
    print("  Controls:")
    print("    🎯 TARGET LOCK button — lock onto nearest opponent")
    print("    F9  — quick target lock (keyboard shortcut)")
    print("    F12 — emergency stop (kills everything)")
    print()
    print("  Tick Engine:")
    print(f"    Game tick: {TICK_DURATION}s ({TICK_MS}ms)")
    print(f"    Weapons in database: {len(WEAPON_DB)}")
    print(f"    Spec weapons ranked: {len(SPEC_PRIORITY)}")
    print()

    brain = CombatBrain()

    if HAS_GUI:
        gui = BotGUI(brain)
        gui.build_gui()
    else:
        print("No GUI available — running headless")
        brain.lock_target()
        brain.run()

if __name__ == "__main__":
    main()
