#!/usr/bin/env python3
"""
ROAT PKZ PVP BOT v2.0 — Main NH Tribrid
External screen-reader bot with humanized mouse + auto-calibration
Designed to evade Roat Pkz anti-cheat (AHK detection, mouse pattern ML)

FEATURES:
- Auto-detects Roat Pkz window (no manual calibration needed)
- Standard RuneLite UI position math (works at any window position)
- Bezier curve mouse movement (beats mouse ML detection)
- Variable human-like reaction timing
- GUI control panel (start/stop/status)
- Full NH tribrid combat brain (eat > pray > spec > switch > attack)
- Combo eating (food + karambwan + brew on same tick)
- 1-tick gear switching (4-way switch in under 600ms)
- KO detection (specs at optimal HP threshold)
"""

import time
import math
import random
import sys
import os
import json
import threading
import ctypes
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

# Try to import tkinter for GUI
try:
    import tkinter as tk
    from tkinter import ttk
    HAS_TK = True
except ImportError:
    HAS_TK = False
    print("[WARN] tkinter not available — running in CLI mode")


# ============================================================
#  CONSTANTS — RuneLite Fixed Layout
# ============================================================
# RuneLite fixed mode game viewport
RL_VIEWPORT_W = 765
RL_VIEWPORT_H = 503

# Sidebar panel (inventory/prayer/equipment) — right of viewport
RL_SIDEBAR_X = 548       # relative to client area left
RL_SIDEBAR_Y = 205       # relative to client area top
RL_SIDEBAR_W = 196
RL_SIDEBAR_H = 260

# Inventory grid (4 cols x 7 rows) inside sidebar
INV_GRID_X = 563         # first slot center X (relative to client)
INV_GRID_Y = 213         # first slot center Y
INV_SLOT_W = 42          # horizontal spacing
INV_SLOT_H = 36          # vertical spacing
INV_COLS = 4
INV_ROWS = 7

# HP orb (top-left of game area)
HP_ORB_X = 52            # center relative to client
HP_ORB_Y = 78
HP_ORB_R = 14            # radius of the orb fill

# Prayer orb
PRAY_ORB_X = 52
PRAY_ORB_Y = 114
PRAY_ORB_R = 14

# Spec orb
SPEC_ORB_X = 52
SPEC_ORB_Y = 150

# Prayer icons (in prayer tab, relative to sidebar top-left)
# Standard RuneLite prayer book layout — protect prayers are row 5
# Protect from Magic = col 1, Protect from Range = col 2, Protect from Melee = col 3
PRAY_TAB_BASE_X = 554    # first prayer icon X
PRAY_TAB_BASE_Y = 213    # first prayer icon Y
PRAY_ICON_W = 37         # spacing between icons
PRAY_ICON_H = 35

# Protection prayer positions (row 4, 0-indexed — the 5th row in standard prayer book)
# Standard order: Prot Magic (col 1), Prot Range (col 2), Prot Melee (col 3)
PROT_MAGE_COL = 1
PROT_RANGE_COL = 2
PROT_MELEE_COL = 3
PROT_PRAY_ROW = 4

# Minimap position (for detecting if in combat area)
MINIMAP_X = 643
MINIMAP_Y = 83
MINIMAP_R = 72

# Opponent HP bar (above their head in viewport — typically center-ish)
# This appears dynamically; we detect it by color scanning
OPP_HP_BAR_H = 5         # height of the bar in pixels
OPP_HP_SCAN_Y_MIN = 30   # scan from this Y (relative to viewport)
OPP_HP_SCAN_Y_MAX = 250  # to this Y

# Chat area
CHAT_X = 7
CHAT_Y = 480
CHAT_W = 505
CHAT_H = 128

# F-key bindings (standard RuneLite)
FKEY_COMBAT = Key.f1
FKEY_STATS = Key.f2
FKEY_QUESTS = Key.f3
FKEY_INVENTORY = Key.f1   # Roat Pkz may differ — will adjust
FKEY_EQUIPMENT = Key.f4
FKEY_PRAYER = Key.f5
FKEY_SPELLBOOK = Key.f6

# Escape to close interfaces
KEY_ESCAPE = Key.esc


# ============================================================
#  ENUMS
# ============================================================
class CombatStyle(Enum):
    MELEE = "melee"
    RANGE = "range"
    MAGE = "mage"

class PrayerType(Enum):
    PROTECT_MELEE = "melee"
    PROTECT_RANGE = "range"
    PROTECT_MAGE = "mage"

class BotState(Enum):
    IDLE = "idle"
    SEARCHING = "searching"
    FIGHTING = "fighting"
    EATING = "eating"
    SWITCHING = "switching"
    SPECCING = "speccing"
    DEAD = "dead"
    BANKING = "banking"


# ============================================================
#  CONFIG — User-editable settings
# ============================================================
class Config:
    # Combat
    EAT_HP_PCT = 65             # eat when HP below this %
    COMBO_EAT_HP_PCT = 40       # combo eat below this %
    SPEC_OPP_HP_PCT = 35        # spec when opponent below this %
    MAX_HP = 99

    # Timing (anti-detection)
    TICK_MS = 600
    MIN_REACT_MS = 120
    MAX_REACT_MS = 280
    MOUSE_SPEED_MIN = 0.06      # seconds
    MOUSE_SPEED_MAX = 0.18

    # Style rotation
    STYLE_ROTATION = [CombatStyle.MAGE, CombatStyle.RANGE, CombatStyle.MELEE]

    # Inventory layout (slot indices 0-27)
    # Top rows = gear switches, bottom rows = food
    MELEE_SLOTS = [0, 1, 2, 3]       # helm, body, legs, weapon
    RANGE_SLOTS = [4, 5, 6, 7]
    MAGE_SLOTS = [8, 9, 10, 11]
    SPEC_WEAPON_SLOT = 12
    SHIELD_SLOT = 13
    BREW_SLOTS = [14, 15, 16]
    RESTORE_SLOTS = [17, 18]
    KARAMBWAN_SLOT = 19
    FOOD_SLOTS = [20, 21, 22, 23, 24, 25, 26, 27]

    # HP bar color ranges (HSV)
    HP_GREEN_LOW = np.array([35, 80, 80])
    HP_GREEN_HIGH = np.array([85, 255, 255])
    HP_RED_LOW = np.array([0, 80, 80])
    HP_RED_HIGH = np.array([10, 255, 255])

    # Opponent HP bar colors (the thin bar above their head)
    OPP_BAR_GREEN = np.array([0, 255, 0])    # bright green
    OPP_BAR_RED = np.array([255, 0, 0])       # bright red

    # Global hotkey to stop bot
    STOP_KEY = Key.f12

    # Auto-retaliate should be ON in-game


# ============================================================
#  WINDOW FINDER — Auto-detect Roat Pkz
# ============================================================
class WindowFinder:
    """Finds and tracks the Roat Pkz game window"""

    WINDOW_TITLES = ["Roat Pkz", "roat pkz", "RoatPkz"]

    def __init__(self):
        self.window = None
        self.client_x = 0
        self.client_y = 0
        self.client_w = 0
        self.client_h = 0

    def find(self) -> bool:
        """Find the Roat Pkz window"""
        for title_search in self.WINDOW_TITLES:
            windows = gw.getWindowsWithTitle(title_search)
            if windows:
                self.window = windows[0]
                self._update_rect()
                return True

        # Fallback: search all windows for partial match
        for w in gw.getAllWindows():
            if w.title and ("roat" in w.title.lower() or "pkz" in w.title.lower()):
                self.window = w
                self._update_rect()
                return True

        return False

    def _update_rect(self):
        """Update stored window position"""
        if self.window:
            self.client_x = self.window.left
            self.client_y = self.window.top
            self.client_w = self.window.width
            self.client_h = self.window.height

    def refresh(self):
        """Refresh window position (in case it moved)"""
        if self.window:
            try:
                self._update_rect()
            except Exception:
                self.find()

    def is_active(self) -> bool:
        """Check if Roat Pkz is the active window"""
        try:
            return self.window and self.window.isActive
        except Exception:
            return False

    def activate(self):
        """Bring Roat Pkz to front"""
        try:
            if self.window:
                self.window.activate()
                time.sleep(0.1)
        except Exception:
            pass

    def screen_pos(self, rel_x: int, rel_y: int) -> Tuple[int, int]:
        """Convert client-relative coordinates to screen coordinates"""
        # Account for title bar (~31px on Windows) and border (~8px)
        title_bar = 31
        border = 8
        return (
            self.client_x + border + rel_x,
            self.client_y + title_bar + rel_y
        )


# ============================================================
#  HUMANIZED MOUSE — Bezier curves + variable speed
# ============================================================
class HumanMouse:
    """Mouse controller that moves along randomized Bezier curves"""

    def __init__(self):
        self.mouse = MouseController()
        self.last_move_time = 0

    def get_pos(self) -> Tuple[int, int]:
        return self.mouse.position

    def _bezier(self, t: float, pts: List[Tuple[float, float]]) -> Tuple[float, float]:
        """Evaluate cubic+ Bezier curve at parameter t"""
        n = len(pts) - 1
        x = y = 0.0
        for i, (px, py) in enumerate(pts):
            c = math.comb(n, i) * (t ** i) * ((1 - t) ** (n - i))
            x += c * px
            y += c * py
        return x, y

    def _ctrl_points(self, start: Tuple[int, int], end: Tuple[int, int]) -> List[Tuple[float, float]]:
        """Generate randomized control points for natural curve"""
        sx, sy = float(start[0]), float(start[1])
        ex, ey = float(end[0]), float(end[1])
        dist = math.hypot(ex - sx, ey - sy)

        num_cp = 2 if dist < 200 else (3 if dist < 500 else 4)
        pts = [(sx, sy)]

        for i in range(num_cp):
            t = (i + 1) / (num_cp + 1)
            mx = sx + (ex - sx) * t
            my = sy + (ey - sy) * t
            off = dist * 0.12 * random.uniform(0.2, 1.0)
            angle = math.atan2(ey - sy, ex - sx) + math.pi / 2
            dx = math.cos(angle) * off * random.choice([-1, 1])
            dy = math.sin(angle) * off * random.choice([-1, 1])
            pts.append((mx + dx, my + dy))

        pts.append((ex, ey))
        return pts

    def _speed_profile(self, t: float) -> float:
        """Fast in middle, slow at start/end (like a real hand)"""
        return 0.3 + 0.7 * math.sin(t * math.pi)

    def move_to(self, x: int, y: int, speed: float = None):
        """Move mouse along humanized Bezier curve to (x, y)"""
        if speed is None:
            speed = random.uniform(Config.MOUSE_SPEED_MIN, Config.MOUSE_SPEED_MAX)

        start = self.get_pos()
        # Add +-2px jitter to final position
        end = (x + random.randint(-2, 2), y + random.randint(-2, 2))
        dist = math.hypot(end[0] - start[0], end[1] - start[1])

        if dist < 3:
            self.mouse.position = end
            return

        pts = self._ctrl_points(start, end)
        steps = max(8, int(dist / 3.5))

        px, py = start
        for i in range(1, steps + 1):
            t = i / steps
            bx, by = self._bezier(t, pts)
            sp = self._speed_profile(t)
            delay = (speed / steps) / max(sp, 0.1)

            nx = int(bx + random.gauss(0, 0.4))
            ny = int(by + random.gauss(0, 0.4))
            if (nx, ny) != (px, py):
                self.mouse.position = (nx, ny)
                px, py = nx, ny
            time.sleep(delay)

        self.mouse.position = end
        self.last_move_time = time.time()

    def click(self, x: int, y: int, button: str = "left"):
        """Move and click with human timing"""
        self.move_to(x, y)
        time.sleep(random.uniform(0.015, 0.045))
        btn = Button.left if button == "left" else Button.right
        self.mouse.press(btn)
        time.sleep(random.uniform(0.035, 0.08))
        self.mouse.release(btn)
        time.sleep(random.uniform(0.01, 0.03))

    def fast_click(self, x: int, y: int):
        """Faster click for urgent actions (eating, pray switches)"""
        self.move_to(x, y, speed=random.uniform(0.04, 0.09))
        time.sleep(random.uniform(0.008, 0.025))
        self.mouse.press(Button.left)
        time.sleep(random.uniform(0.02, 0.045))
        self.mouse.release(Button.left)

    def rapid_clicks(self, positions: List[Tuple[int, int]]):
        """Click multiple positions as fast as humanly possible (gear switches)"""
        for i, (x, y) in enumerate(positions):
            spd = random.uniform(0.03, 0.07) if i > 0 else random.uniform(0.04, 0.09)
            self.move_to(x, y, speed=spd)
            time.sleep(random.uniform(0.005, 0.02))
            self.mouse.press(Button.left)
            time.sleep(random.uniform(0.015, 0.04))
            self.mouse.release(Button.left)
            if i < len(positions) - 1:
                time.sleep(random.uniform(0.02, 0.055))


# ============================================================
#  SCREEN READER — Fast game state from pixels
# ============================================================
class ScreenReader:
    """Reads HP, prayer, inventory, opponent state from screen"""

    def __init__(self, wf: WindowFinder):
        self.sct = mss.mss()
        self.wf = wf

    def capture(self) -> np.ndarray:
        """Capture the game window region"""
        self.wf.refresh()
        mon = {
            "left": self.wf.client_x,
            "top": self.wf.client_y,
            "width": self.wf.client_w,
            "height": self.wf.client_h,
        }
        shot = self.sct.grab(mon)
        return np.array(shot)[:, :, :3]  # drop alpha

    def capture_region(self, rx: int, ry: int, rw: int, rh: int) -> np.ndarray:
        """Capture a specific region (client-relative coords)"""
        sx, sy = self.wf.screen_pos(rx, ry)
        mon = {"left": sx, "top": sy, "width": rw, "height": rh}
        shot = self.sct.grab(mon)
        return np.array(shot)[:, :, :3]

    def read_orb_pct(self, frame: np.ndarray, orb_x: int, orb_y: int, orb_r: int) -> float:
        """Read an orb's fill percentage by green/red ratio"""
        # Account for title bar offset in the captured frame
        title_bar = 31
        border = 8
        cy = orb_y + title_bar
        cx = orb_x + border

        if cy - orb_r < 0 or cx - orb_r < 0:
            return 1.0
        if cy + orb_r >= frame.shape[0] or cx + orb_r >= frame.shape[1]:
            return 1.0

        orb = frame[cy - orb_r:cy + orb_r, cx - orb_r:cx + orb_r]
        if orb.size == 0:
            return 1.0

        hsv = cv2.cvtColor(orb, cv2.COLOR_BGR2HSV)
        green = cv2.countNonZero(cv2.inRange(hsv, Config.HP_GREEN_LOW, Config.HP_GREEN_HIGH))
        red = cv2.countNonZero(cv2.inRange(hsv, Config.HP_RED_LOW, Config.HP_RED_HIGH))
        total = green + red
        return green / total if total > 10 else 1.0

    def read_my_hp(self, frame: np.ndarray) -> float:
        """Read player HP percentage from orb"""
        return self.read_orb_pct(frame, HP_ORB_X, HP_ORB_Y, HP_ORB_R)

    def read_my_prayer(self, frame: np.ndarray) -> float:
        """Read player prayer percentage from orb"""
        return self.read_orb_pct(frame, PRAY_ORB_X, PRAY_ORB_Y, PRAY_ORB_R)

    def find_opponent_hp_bar(self, frame: np.ndarray) -> Tuple[Optional[float], Optional[Tuple[int, int]]]:
        """
        Scan viewport for opponent HP bar (green+red horizontal bar above their head).
        Returns (hp_pct, center_pos) or (None, None) if not found.
        """
        title_bar = 31
        border = 8

        # Scan upper portion of viewport for horizontal green/red bars
        y_start = title_bar + OPP_HP_SCAN_Y_MIN
        y_end = min(title_bar + OPP_HP_SCAN_Y_MAX, frame.shape[0])
        x_start = border
        x_end = min(border + RL_VIEWPORT_W, frame.shape[1])

        viewport = frame[y_start:y_end, x_start:x_end]
        if viewport.size == 0:
            return None, None

        hsv = cv2.cvtColor(viewport, cv2.COLOR_BGR2HSV)

        # Look for bright green pixels (HP remaining)
        green_mask = cv2.inRange(hsv, np.array([35, 150, 150]), np.array([85, 255, 255]))
        # Look for bright red pixels (HP lost)
        red_mask = cv2.inRange(hsv, np.array([0, 150, 150]), np.array([10, 255, 255]))

        # Combine and find horizontal clusters
        combined = cv2.bitwise_or(green_mask, red_mask)

        # Find contours that are wide and thin (HP bar shape)
        contours, _ = cv2.findContours(combined, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        best_bar = None
        best_width = 0

        for cnt in contours:
            x, y, w, h = cv2.boundingRect(cnt)
            # HP bar is wide (~30px) and thin (~5px)
            if w > 20 and h < 12 and w > h * 3:
                if w > best_width:
                    best_width = w
                    best_bar = (x, y, w, h)

        if best_bar is None:
            return None, None

        bx, by, bw, bh = best_bar
        bar_region = viewport[by:by + bh, bx:bx + bw]
        bar_hsv = cv2.cvtColor(bar_region, cv2.COLOR_BGR2HSV)

        g = cv2.countNonZero(cv2.inRange(bar_hsv, np.array([35, 150, 150]), np.array([85, 255, 255])))
        r = cv2.countNonZero(cv2.inRange(bar_hsv, np.array([0, 150, 150]), np.array([10, 255, 255])))
        total = g + r
        hp_pct = g / total if total > 5 else 1.0

        # Center position of the bar (opponent is below it)
        center_x = x_start + bx + bw // 2
        center_y = y_start + by + bh + 20  # click below the bar (on the character)

        return hp_pct, (center_x, center_y)

    def inv_slot_screen(self, slot: int) -> Tuple[int, int]:
        """Get screen coordinates for inventory slot (0-27)"""
        row = slot // INV_COLS
        col = slot % INV_COLS
        rx = INV_GRID_X + col * INV_SLOT_W
        ry = INV_GRID_Y + row * INV_SLOT_H
        return self.wf.screen_pos(rx, ry)

    def is_slot_empty(self, frame: np.ndarray, slot: int) -> bool:
        """Check if inventory slot appears empty (dark brown)"""
        row = slot // INV_COLS
        col = slot % INV_COLS
        title_bar = 31
        border = 8
        cx = border + INV_GRID_X + col * INV_SLOT_W
        cy = title_bar + INV_GRID_Y + row * INV_SLOT_H

        if cy - 4 < 0 or cx - 4 < 0:
            return True
        if cy + 4 >= frame.shape[0] or cx + 4 >= frame.shape[1]:
            return True

        region = frame[cy - 4:cy + 4, cx - 4:cx + 4]
        if region.size == 0:
            return True

        avg = np.mean(region, axis=(0, 1))
        # Empty slot is dark brown (~BGR 41, 53, 62)
        return avg[0] < 85 and avg[1] < 75 and avg[2] < 70

    def prayer_icon_screen(self, row: int, col: int) -> Tuple[int, int]:
        """Get screen coordinates for a prayer icon"""
        rx = PRAY_TAB_BASE_X + col * PRAY_ICON_W
        ry = PRAY_TAB_BASE_Y + row * PRAY_ICON_H
        return self.wf.screen_pos(rx, ry)

    def detect_attack_style(self, frame: np.ndarray, opp_pos: Tuple[int, int]) -> Optional[CombatStyle]:
        """
        Detect opponent's attack style from projectile colors near the player.
        - Blue/white/cyan pixels = magic (ice barrage, spell splashes)
        - Brown/gray elongated = range (arrows/bolts)
        - No projectile + close range = melee
        """
        title_bar = 31
        border = 8
        # Scan area around player (center of viewport)
        pcx = border + RL_VIEWPORT_W // 2
        pcy = title_bar + RL_VIEWPORT_H // 2

        scan_r = 60  # pixel radius around player
        y1 = max(0, pcy - scan_r)
        y2 = min(frame.shape[0], pcy + scan_r)
        x1 = max(0, pcx - scan_r)
        x2 = min(frame.shape[1], pcx + scan_r)

        region = frame[y1:y2, x1:x2]
        if region.size == 0:
            return None

        hsv = cv2.cvtColor(region, cv2.COLOR_BGR2HSV)

        # Ice barrage / magic = cyan/blue pixels
        magic_mask = cv2.inRange(hsv, np.array([85, 80, 150]), np.array([130, 255, 255]))
        magic_px = cv2.countNonZero(magic_mask)

        # Range projectile = brown/gray
        # Actually hard to detect — for v1, use distance heuristic
        # If opponent is far and no magic = range; close = melee

        if magic_px > 50:
            return CombatStyle.MAGE

        # Distance check
        if opp_pos:
            dist = math.hypot(opp_pos[0] - pcx, opp_pos[1] - pcy)
            if dist > 80:
                return CombatStyle.RANGE
            else:
                return CombatStyle.MELEE

        return None


# ============================================================
#  PVP BRAIN — Combat decision engine
# ============================================================
class PVPBrain:
    """
    Every tick priority: Survive > Protect > Spec > Switch+Attack
    """

    def __init__(self, mouse: HumanMouse, screen: ScreenReader, kb: KeyboardController, wf: WindowFinder):
        self.mouse = mouse
        self.screen = screen
        self.kb = kb
        self.wf = wf
        self.state = BotState.IDLE
        self.style = CombatStyle.MELEE
        self.prayer = None
        self.style_idx = 0
        self.last_eat = 0
        self.last_switch = 0
        self.last_spec = 0
        self.ticks = 0
        self.running = False
        self.paused = False
        self.log_lines = []
        self.stats = {"kills": 0, "deaths": 0, "fights": 0, "food_eaten": 0}

    def log(self, msg: str):
        ts = time.strftime("%H:%M:%S")
        line = f"[{ts}] {msg}"
        print(line)
        self.log_lines.append(line)
        if len(self.log_lines) > 200:
            self.log_lines = self.log_lines[-100:]

    def human_delay(self, lo=None, hi=None):
        lo = lo or Config.MIN_REACT_MS
        hi = hi or Config.MAX_REACT_MS
        time.sleep(random.randint(lo, hi) / 1000.0)

    # --- SURVIVE ---
    def do_eat(self, hp_pct: float, frame: np.ndarray) -> bool:
        now = time.time()
        if now - self.last_eat < 0.55:
            return False

        if hp_pct > Config.EAT_HP_PCT / 100.0:
            return False

        combo = hp_pct <= Config.COMBO_EAT_HP_PCT / 100.0

        self.log(f"{'COMBO ' if combo else ''}EAT — HP {hp_pct*100:.0f}%")

        # Open inventory
        self.kb.press(FKEY_INVENTORY)
        self.kb.release(FKEY_INVENTORY)
        time.sleep(0.015)

        # Click food
        for slot in Config.FOOD_SLOTS:
            if not self.screen.is_slot_empty(frame, slot):
                x, y = self.screen.inv_slot_screen(slot)
                self.mouse.fast_click(x, y)
                self.stats["food_eaten"] += 1
                break

        if combo:
            time.sleep(0.015)
            # Karambwan (same-tick eat)
            if not self.screen.is_slot_empty(frame, Config.KARAMBWAN_SLOT):
                x, y = self.screen.inv_slot_screen(Config.KARAMBWAN_SLOT)
                self.mouse.fast_click(x, y)

            time.sleep(0.015)
            # Brew
            for slot in Config.BREW_SLOTS:
                if not self.screen.is_slot_empty(frame, slot):
                    x, y = self.screen.inv_slot_screen(slot)
                    self.mouse.fast_click(x, y)
                    break

        self.last_eat = now
        return True

    # --- PROTECT ---
    def do_pray(self, frame: np.ndarray, opp_style: Optional[CombatStyle]) -> bool:
        if opp_style is None:
            return False

        needed = {
            CombatStyle.MELEE: PrayerType.PROTECT_MELEE,
            CombatStyle.RANGE: PrayerType.PROTECT_RANGE,
            CombatStyle.MAGE: PrayerType.PROTECT_MAGE,
        }[opp_style]

        if self.prayer == needed:
            return False

        self.log(f"PRAY switch -> Protect {needed.value}")

        # Open prayer tab
        self.kb.press(FKEY_PRAYER)
        self.kb.release(FKEY_PRAYER)
        time.sleep(0.02)

        # Click correct protection prayer
        col = {
            PrayerType.PROTECT_MAGE: PROT_MAGE_COL,
            PrayerType.PROTECT_RANGE: PROT_RANGE_COL,
            PrayerType.PROTECT_MELEE: PROT_MELEE_COL,
        }[needed]

        px, py = self.screen.prayer_icon_screen(PROT_PRAY_ROW, col)
        self.mouse.fast_click(px, py)
        self.prayer = needed

        # Back to inventory
        time.sleep(0.015)
        self.kb.press(FKEY_INVENTORY)
        self.kb.release(FKEY_INVENTORY)
        return True

    # --- SPEC ---
    def do_spec(self, opp_hp: float, opp_pos: Tuple[int, int], frame: np.ndarray) -> bool:
        now = time.time()
        if now - self.last_spec < 1.5:
            return False
        if opp_hp > Config.SPEC_OPP_HP_PCT / 100.0:
            return False

        self.log(f"SPEC KO ATTEMPT — Opp at {opp_hp*100:.0f}%!")

        # Open inventory
        self.kb.press(FKEY_INVENTORY)
        self.kb.release(FKEY_INVENTORY)
        time.sleep(0.015)

        # Click spec weapon
        if not self.screen.is_slot_empty(frame, Config.SPEC_WEAPON_SLOT):
            sx, sy = self.screen.inv_slot_screen(Config.SPEC_WEAPON_SLOT)
            self.mouse.fast_click(sx, sy)
            time.sleep(0.04)

        # Click spec orb (or use spec bar in combat tab)
        spec_x, spec_y = self.wf.screen_pos(SPEC_ORB_X, SPEC_ORB_Y)
        self.mouse.fast_click(spec_x, spec_y)
        time.sleep(0.03)

        # Click opponent
        self.mouse.fast_click(opp_pos[0], opp_pos[1])

        self.last_spec = now
        return True

    # --- GEAR SWITCH ---
    def do_switch(self, style: CombatStyle, frame: np.ndarray):
        now = time.time()
        if now - self.last_switch < 0.35:
            return

        slots_map = {
            CombatStyle.MELEE: Config.MELEE_SLOTS,
            CombatStyle.RANGE: Config.RANGE_SLOTS,
            CombatStyle.MAGE: Config.MAGE_SLOTS,
        }
        slots = slots_map[style]

        self.log(f"SWITCH -> {style.value}")

        # Open inventory
        self.kb.press(FKEY_INVENTORY)
        self.kb.release(FKEY_INVENTORY)
        time.sleep(0.015)

        # Rapid-click all gear slots
        positions = []
        for slot in slots:
            if not self.screen.is_slot_empty(frame, slot):
                positions.append(self.screen.inv_slot_screen(slot))

        if positions:
            self.mouse.rapid_clicks(positions)

        self.style = style
        self.last_switch = now

    def next_style(self) -> CombatStyle:
        self.style_idx = (self.style_idx + 1) % len(Config.STYLE_ROTATION)
        return Config.STYLE_ROTATION[self.style_idx]

    # --- MAIN TICK ---
    def tick(self):
        if self.paused:
            time.sleep(0.1)
            return

        frame = self.screen.capture()

        # Find opponent
        opp_hp, opp_pos = self.screen.find_opponent_hp_bar(frame)

        if opp_hp is None:
            if self.state == BotState.FIGHTING:
                self.log("Fight over — opponent gone")
                self.state = BotState.IDLE
                self.ticks = 0
                self.prayer = None
            return

        if self.state != BotState.FIGHTING:
            self.log("FIGHT DETECTED!")
            self.state = BotState.FIGHTING
            self.stats["fights"] += 1
            self.ticks = 0

        self.ticks += 1
        my_hp = self.screen.read_my_hp(frame)

        if self.ticks % 5 == 0:
            self.log(f"T{self.ticks} HP:{my_hp*100:.0f}% OppHP:{opp_hp*100:.0f}% Style:{self.style.value}")

        # Human reaction delay
        self.human_delay()

        # PRIORITY 1: SURVIVE
        if self.do_eat(my_hp, frame):
            return

        # PRIORITY 2: PRAY
        opp_style = self.screen.detect_attack_style(frame, opp_pos)
        self.do_pray(frame, opp_style)

        # PRIORITY 3: SPEC
        if self.do_spec(opp_hp, opp_pos, frame):
            return

        # PRIORITY 4: SWITCH + ATTACK
        ns = self.next_style()
        self.do_switch(ns, frame)

        # Attack opponent
        time.sleep(random.uniform(0.04, 0.10))
        self.mouse.click(opp_pos[0], opp_pos[1])

    def run(self):
        self.running = True
        self.log("Bot ARMED — enter a fight to activate")

        while self.running:
            try:
                t0 = time.time()
                self.tick()
                elapsed = time.time() - t0
                remain = (Config.TICK_MS / 1000.0) - elapsed
                if remain > 0:
                    time.sleep(remain)
            except KeyboardInterrupt:
                self.log("STOPPED by user")
                break
            except Exception as e:
                self.log(f"ERROR: {e}")
                time.sleep(0.5)

        self.running = False

    def stop(self):
        self.running = False


# ============================================================
#  GUI CONTROL PANEL
# ============================================================
class BotGUI:
    """Simple tkinter control panel"""

    def __init__(self, brain: PVPBrain):
        self.brain = brain
        self.root = None
        self.bot_thread = None

    def build(self):
        self.root = tk.Tk()
        self.root.title("Roat Pkz PVP Bot v2.0")
        self.root.geometry("340x520")
        self.root.resizable(False, False)
        self.root.attributes("-topmost", True)
        self.root.configure(bg="#1a1a2e")

        style = ttk.Style()
        style.theme_use("clam")

        # Title
        tk.Label(
            self.root, text="ROAT PKZ PVP BOT", font=("Consolas", 16, "bold"),
            fg="#e94560", bg="#1a1a2e"
        ).pack(pady=(10, 2))
        tk.Label(
            self.root, text="Main NH Tribrid v2.0", font=("Consolas", 10),
            fg="#888", bg="#1a1a2e"
        ).pack()

        # Status
        self.status_var = tk.StringVar(value="IDLE")
        self.status_label = tk.Label(
            self.root, textvariable=self.status_var, font=("Consolas", 14, "bold"),
            fg="#00ff41", bg="#1a1a2e"
        )
        self.status_label.pack(pady=10)

        # Buttons frame
        btn_frame = tk.Frame(self.root, bg="#1a1a2e")
        btn_frame.pack(pady=5)

        self.start_btn = tk.Button(
            btn_frame, text="START", font=("Consolas", 12, "bold"),
            bg="#16213e", fg="#00ff41", width=12, command=self.start_bot
        )
        self.start_btn.grid(row=0, column=0, padx=5)

        self.stop_btn = tk.Button(
            btn_frame, text="STOP", font=("Consolas", 12, "bold"),
            bg="#16213e", fg="#e94560", width=12, command=self.stop_bot, state="disabled"
        )
        self.stop_btn.grid(row=0, column=1, padx=5)

        # Stats frame
        stats_frame = tk.LabelFrame(
            self.root, text="STATS", font=("Consolas", 10, "bold"),
            fg="#0f3460", bg="#1a1a2e", labelanchor="n"
        )
        stats_frame.pack(fill="x", padx=15, pady=10)

        self.fights_var = tk.StringVar(value="Fights: 0")
        self.food_var = tk.StringVar(value="Food eaten: 0")
        self.state_var = tk.StringVar(value="State: IDLE")

        for var in [self.fights_var, self.food_var, self.state_var]:
            tk.Label(stats_frame, textvariable=var, font=("Consolas", 10),
                     fg="#ccc", bg="#1a1a2e", anchor="w").pack(fill="x", padx=10, pady=2)

        # Settings frame
        set_frame = tk.LabelFrame(
            self.root, text="SETTINGS", font=("Consolas", 10, "bold"),
            fg="#0f3460", bg="#1a1a2e", labelanchor="n"
        )
        set_frame.pack(fill="x", padx=15, pady=5)

        # Eat threshold
        tk.Label(set_frame, text="Eat HP%:", font=("Consolas", 9),
                 fg="#ccc", bg="#1a1a2e").grid(row=0, column=0, padx=5, sticky="w")
        self.eat_var = tk.StringVar(value=str(Config.EAT_HP_PCT))
        tk.Entry(set_frame, textvariable=self.eat_var, width=5, font=("Consolas", 9)).grid(row=0, column=1)

        # Combo eat threshold
        tk.Label(set_frame, text="Combo%:", font=("Consolas", 9),
                 fg="#ccc", bg="#1a1a2e").grid(row=1, column=0, padx=5, sticky="w")
        self.combo_var = tk.StringVar(value=str(Config.COMBO_EAT_HP_PCT))
        tk.Entry(set_frame, textvariable=self.combo_var, width=5, font=("Consolas", 9)).grid(row=1, column=1)

        # Spec threshold
        tk.Label(set_frame, text="Spec Opp%:", font=("Consolas", 9),
                 fg="#ccc", bg="#1a1a2e").grid(row=2, column=0, padx=5, sticky="w")
        self.spec_var = tk.StringVar(value=str(Config.SPEC_OPP_HP_PCT))
        tk.Entry(set_frame, textvariable=self.spec_var, width=5, font=("Consolas", 9)).grid(row=2, column=1)

        # Apply button
        tk.Button(set_frame, text="Apply", font=("Consolas", 9),
                  bg="#16213e", fg="#00ff41", command=self.apply_settings).grid(row=3, column=0, columnspan=2, pady=5)

        # Log area
        log_frame = tk.LabelFrame(
            self.root, text="LOG", font=("Consolas", 10, "bold"),
            fg="#0f3460", bg="#1a1a2e", labelanchor="n"
        )
        log_frame.pack(fill="both", expand=True, padx=15, pady=5)

        self.log_text = tk.Text(
            log_frame, height=6, font=("Consolas", 8), bg="#0d0d1a", fg="#00ff41",
            insertbackground="#00ff41", wrap="word"
        )
        self.log_text.pack(fill="both", expand=True, padx=3, pady=3)

        # F12 label
        tk.Label(
            self.root, text="F12 = Emergency Stop", font=("Consolas", 8),
            fg="#666", bg="#1a1a2e"
        ).pack(pady=(0, 5))

        # Update loop
        self.update_gui()

    def apply_settings(self):
        try:
            Config.EAT_HP_PCT = int(self.eat_var.get())
            Config.COMBO_EAT_HP_PCT = int(self.combo_var.get())
            Config.SPEC_OPP_HP_PCT = int(self.spec_var.get())
            self.add_log("Settings applied")
        except ValueError:
            self.add_log("Invalid setting value")

    def add_log(self, msg: str):
        self.log_text.insert("end", msg + "\n")
        self.log_text.see("end")

    def start_bot(self):
        self.start_btn.config(state="disabled")
        self.stop_btn.config(state="normal")
        self.status_var.set("RUNNING")
        self.status_label.config(fg="#00ff41")
        self.add_log("Bot STARTED")

        self.bot_thread = threading.Thread(target=self.brain.run, daemon=True)
        self.bot_thread.start()

    def stop_bot(self):
        self.brain.stop()
        self.start_btn.config(state="normal")
        self.stop_btn.config(state="disabled")
        self.status_var.set("STOPPED")
        self.status_label.config(fg="#e94560")
        self.add_log("Bot STOPPED")

    def update_gui(self):
        # Update stats
        s = self.brain.stats
        self.fights_var.set(f"Fights: {s['fights']}")
        self.food_var.set(f"Food eaten: {s['food_eaten']}")
        self.state_var.set(f"State: {self.brain.state.value}")

        # Sync log
        while len(self.brain.log_lines) > 0:
            line = self.brain.log_lines.pop(0)
            self.add_log(line)

        self.root.after(250, self.update_gui)

    def run(self):
        self.build()
        self.root.mainloop()


# ============================================================
#  GLOBAL HOTKEY LISTENER (F12 = emergency stop)
# ============================================================
def setup_hotkey(brain: PVPBrain):
    def on_press(key):
        if key == Config.STOP_KEY:
            brain.stop()
            print("\n[F12] EMERGENCY STOP")
    listener = Listener(on_press=on_press)
    listener.daemon = True
    listener.start()


# ============================================================
#  MAIN
# ============================================================
def main():
    print("""
    ========================================
      ROAT PKZ PVP BOT v2.0
      Main NH Tribrid
      Press F12 to emergency stop
    ========================================
    """)

    # Find game window
    wf = WindowFinder()
    print("[INIT] Looking for Roat Pkz window...")

    retries = 0
    while not wf.find():
        retries += 1
        if retries > 30:
            print("[ERROR] Could not find Roat Pkz window after 30 seconds.")
            print("        Make sure Roat Pkz is open and you're logged in.")
            sys.exit(1)
        time.sleep(1)

    print(f"[INIT] Found: '{wf.window.title}' at ({wf.client_x}, {wf.client_y}) {wf.client_w}x{wf.client_h}")

    # Initialize components
    mouse = HumanMouse()
    screen = ScreenReader(wf)
    kb = KeyboardController()
    brain = PVPBrain(mouse, screen, kb, wf)

    # Setup emergency stop
    setup_hotkey(brain)

    # Launch GUI or CLI
    if HAS_TK:
        print("[INIT] Launching GUI control panel...")
        gui = BotGUI(brain)
        gui.run()
    else:
        print("[INIT] Running in CLI mode (no tkinter)")
        print("[READY] Bot armed — enter a fight to activate")
        print("[INFO] Press Ctrl+C or F12 to stop\n")
        try:
            brain.run()
        except KeyboardInterrupt:
            brain.stop()
        print("GG.")


if __name__ == "__main__":
    main()
