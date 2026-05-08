#!/usr/bin/env python3
"""
ROAT PKZ DECIMATOR v4.0 — PVP Assist Tool
==========================================
Pure assist mode: YOU fight, bot executes combos faster than human hands.

HOTKEYS (only active when Roat Pkz window is focused):
  Q  = Magic switch (equip mage gear + open spellbook + pre-select Ice Barrage)
  W  = Melee switch (equip melee gear + combat tab)
  E  = Range switch (equip range gear)
  R  = SPEC COMBO (melee gear + spec weapon + spec bar + click target)
  F11 = LEARN MODE (screenshot inventory, click items to assign gear switches)
  F12 = Pause / Resume toggle

GUI: Small overlay with status + spec weapon dropdown
"""

import ctypes
import ctypes.wintypes
import json
import math
import os
import random
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox
from pathlib import Path

try:
    import cv2
    import numpy as np
    from PIL import ImageGrab, Image, ImageTk
    from pynput import keyboard
except ImportError as e:
    print(f"Missing module: {e}")
    print("Run: pip install opencv-python numpy pillow pynput")
    sys.exit(1)

# =============================================================================
# WIN32
# =============================================================================
user32 = ctypes.windll.user32
EnumWindows = user32.EnumWindows
EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)
GetWindowTextW = user32.GetWindowTextW
GetWindowTextLengthW = user32.GetWindowTextLengthW
IsWindowVisible = user32.IsWindowVisible
SetForegroundWindow = user32.SetForegroundWindow
GetWindowRect = user32.GetWindowRect
GetForegroundWindow = user32.GetForegroundWindow

def find_roat_window():
    result = []
    def callback(hwnd, _):
        if not IsWindowVisible(hwnd):
            return True
        length = GetWindowTextLengthW(hwnd)
        if length == 0:
            return True
        buf = ctypes.create_unicode_buffer(length + 1)
        GetWindowTextW(hwnd, buf, length + 1)
        title = buf.value.lower()
        if "decimator" in title or "assist" in title or "learn" in title:
            return True
        if "roat" in title or "pkz" in title:
            result.append((hwnd, buf.value))
        return True
    EnumWindows(EnumWindowsProc(callback), 0)
    return result[0] if result else None

def get_window_rect(hwnd):
    rect = ctypes.wintypes.RECT()
    GetWindowRect(hwnd, ctypes.byref(rect))
    return rect.left, rect.top, rect.right, rect.bottom

def focus_window(hwnd):
    try:
        SetForegroundWindow(hwnd)
        time.sleep(0.05)
    except:
        pass

def is_game_focused(hwnd):
    """Check if the Roat Pkz window is currently in the foreground."""
    if not hwnd:
        return False
    return GetForegroundWindow() == hwnd

# =============================================================================
# MOUSE
# =============================================================================

def bezier_point(t, p0, p1, p2, p3):
    u = 1 - t
    return (u**3*p0[0] + 3*u**2*t*p1[0] + 3*u*t**2*p2[0] + t**3*p3[0],
            u**3*p0[1] + 3*u**2*t*p1[1] + 3*u*t**2*p2[1] + t**3*p3[1])

def human_move(x, y, speed=0.08):
    pt = ctypes.wintypes.POINT()
    user32.GetCursorPos(ctypes.byref(pt))
    sx, sy = pt.x, pt.y
    dx, dy = x - sx, y - sy
    dist = math.hypot(dx, dy)
    if dist < 3:
        ctypes.windll.user32.SetCursorPos(x, y)
        return
    cp1 = (sx + dx*0.3 + random.randint(-20, 20), sy + dy*0.3 + random.randint(-20, 20))
    cp2 = (sx + dx*0.7 + random.randint(-15, 15), sy + dy*0.7 + random.randint(-15, 15))
    steps = max(12, int(dist / 8))
    for i in range(steps + 1):
        t = i / steps
        bx, by = bezier_point(t, (sx, sy), cp1, cp2, (x, y))
        ctypes.windll.user32.SetCursorPos(int(bx), int(by))
        time.sleep(speed / steps + random.uniform(0, 0.003))

def click(x, y, speed=0.08):
    human_move(x, y, speed)
    time.sleep(random.uniform(0.01, 0.03))
    ctypes.windll.user32.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(random.uniform(0.04, 0.08))
    ctypes.windll.user32.mouse_event(0x0004, 0, 0, 0, 0)

def fast_click(x, y):
    human_move(x, y, speed=0.04)
    time.sleep(random.uniform(0.005, 0.015))
    ctypes.windll.user32.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(random.uniform(0.02, 0.04))
    ctypes.windll.user32.mouse_event(0x0004, 0, 0, 0, 0)

# =============================================================================
# KEYBOARD
# =============================================================================
VK_MAP = {
    'f1': 0x70, 'f2': 0x71, 'f4': 0x73,
}

def press_key(vk_code):
    user32.keybd_event(vk_code, 0, 0, 0)
    time.sleep(random.uniform(0.03, 0.06))
    user32.keybd_event(vk_code, 0, 0x0002, 0)

def send_game_key(key_name):
    vk = VK_MAP.get(key_name.lower())
    if vk:
        press_key(vk)

# =============================================================================
# INVENTORY GRID — Standard RuneLite layout
# =============================================================================
INV_COLS = 4
INV_ROWS = 7
INV_SLOT_SIZE = 36    # approximate px per slot (including small gap)
INV_ICON_SIZE = 32    # actual icon region within slot

# Relative positions (% of game window)
INV_START_X_PCT = 0.765
INV_START_Y_PCT = 0.650
INV_END_X_PCT   = 0.980
INV_END_Y_PCT   = 0.980

# Spellbook: Ice Barrage position
BARRAGE_X_PCT = 0.885
BARRAGE_Y_PCT = 0.870

# Combat tab: spec bar
SPEC_BAR_X_PCT = 0.860
SPEC_BAR_Y_PCT = 0.955

# Viewport center (click target)
VP_CX_PCT = 0.45
VP_CY_PCT = 0.52

# =============================================================================
# CONFIG — Persistent gear assignment storage
# =============================================================================
CONFIG_DIR = Path(__file__).parent
CONFIG_FILE = CONFIG_DIR / "gear_config.json"
TEMPLATE_DIR = CONFIG_DIR / "templates"

def load_config():
    if CONFIG_FILE.exists():
        try:
            with open(CONFIG_FILE, 'r') as f:
                return json.load(f)
        except:
            pass
    return {"mage": [], "melee": [], "range": [], "spec_weapon": "ags"}

def save_config(cfg):
    with open(CONFIG_FILE, 'w') as f:
        json.dump(cfg, f, indent=2)

# =============================================================================
# TEMPLATE STORE — OpenCV template matching
# =============================================================================

class TemplateStore:
    def __init__(self):
        self.templates = {}
        TEMPLATE_DIR.mkdir(exist_ok=True)
        self.load_all()

    def save_template(self, name, image_bgr):
        self.templates[name] = image_bgr
        cv2.imwrite(str(TEMPLATE_DIR / f"{name}.png"), image_bgr)

    def load_all(self):
        if not TEMPLATE_DIR.exists():
            return
        for f in TEMPLATE_DIR.glob("*.png"):
            img = cv2.imread(str(f))
            if img is not None:
                self.templates[f.stem] = img

    def find_in_region(self, name, screenshot_bgr, threshold=0.75):
        templ = self.templates.get(name)
        if templ is None:
            return None
        result = cv2.matchTemplate(screenshot_bgr, templ, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, max_loc = cv2.minMaxLoc(result)
        if max_val >= threshold:
            th, tw = templ.shape[:2]
            return (max_loc[0] + tw // 2, max_loc[1] + th // 2, max_val)
        return None

# =============================================================================
# LEARN MODE — Interactive gear assignment
# =============================================================================

class LearnWindow:
    """Popup window that shows inventory screenshot and lets user assign items."""

    def __init__(self, parent_bot):
        self.bot = parent_bot
        self.assignments = {"mage": [], "melee": [], "range": []}
        self.slot_assignments = {}  # slot_index -> switch name
        self.inv_image_pil = None
        self.slot_rects = []  # list of (x, y, w, h) per slot in the image
        self.win = None

    def capture_and_show(self):
        """Capture inventory from game and show assignment window."""
        if not self.bot.hwnd:
            if not self.bot.detect_game():
                print("[LEARN] Game window not found!")
                return

        # Focus game briefly to ensure inventory is visible
        focus_window(self.bot.hwnd)
        # Press F2 to open inventory tab
        send_game_key('f2')
        time.sleep(0.4)

        # Screenshot inventory region
        self.bot.refresh_rect()
        l, t, r, b = self.bot.game_rect
        gw, gh = r - l, b - t

        inv_l = int(l + gw * INV_START_X_PCT)
        inv_t = int(t + gh * INV_START_Y_PCT)
        inv_r = int(l + gw * INV_END_X_PCT)
        inv_b = int(t + gh * INV_END_Y_PCT)

        img = ImageGrab.grab(bbox=(inv_l, inv_t, inv_r, inv_b))
        self.inv_image_pil = img
        iw, ih = img.size

        # Calculate slot positions within the captured image
        slot_w = iw / INV_COLS
        slot_h = ih / INV_ROWS
        self.slot_rects = []
        for row in range(INV_ROWS):
            for col in range(INV_COLS):
                sx = int(col * slot_w)
                sy = int(row * slot_h)
                sw = int(slot_w)
                sh = int(slot_h)
                self.slot_rects.append((sx, sy, sw, sh))

        # Show the assignment window
        self._show_window(iw, ih)

    def _show_window(self, iw, ih):
        self.win = tk.Toplevel(self.bot.root)
        self.win.title("LEARN MODE — Click items to assign gear switches")
        self.win.attributes('-topmost', True)

        # Scale up for visibility
        scale = max(1, min(3, 600 // max(iw, 1)))
        disp_w, disp_h = iw * scale, ih * scale

        # Instructions
        tk.Label(self.win, text="Click each item to assign it to a gear switch.",
                 font=("Consolas", 11, "bold"), fg="#e94560").pack(pady=5)
        tk.Label(self.win, text="Top 5 = Melee(W) | Next 4 = Mage(Q) | Last 4 = Range(E)",
                 font=("Consolas", 9), fg="#888888").pack()

        # Canvas with inventory image
        self.canvas = tk.Canvas(self.win, width=disp_w, height=disp_h,
                                bg='#333333', highlightthickness=0)
        self.canvas.pack(padx=10, pady=10)

        # Resize image for display
        resized = self.inv_image_pil.resize((disp_w, disp_h), Image.NEAREST)
        self.tk_image = ImageTk.PhotoImage(resized)
        self.canvas.create_image(0, 0, anchor=tk.NW, image=self.tk_image)

        # Draw slot grid
        slot_w_disp = disp_w / INV_COLS
        slot_h_disp = disp_h / INV_ROWS
        for i, (sx, sy, sw, sh) in enumerate(self.slot_rects):
            x1 = sx * scale
            y1 = sy * scale
            x2 = x1 + sw * scale
            y2 = y1 + sh * scale
            self.canvas.create_rectangle(x1, y1, x2, y2, outline='#555555', width=1)

        self.canvas.bind("<Button-1>", lambda e: self._on_click(e, scale))
        self.scale = scale

        # Auto-assign and save buttons
        btn_frame = tk.Frame(self.win)
        btn_frame.pack(pady=5)

        tk.Button(btn_frame, text="AUTO-ASSIGN (5/4/4)",
                  command=self._auto_assign,
                  font=("Consolas", 10, "bold"), bg="#4444aa", fg="white").pack(side=tk.LEFT, padx=5)

        tk.Button(btn_frame, text="SAVE & CLOSE",
                  command=self._save_and_close,
                  font=("Consolas", 10, "bold"), bg="#44aa44", fg="white").pack(side=tk.LEFT, padx=5)

        tk.Button(btn_frame, text="CLEAR ALL",
                  command=self._clear_all,
                  font=("Consolas", 10), bg="#aa4444", fg="white").pack(side=tk.LEFT, padx=5)

        # Status
        self.learn_status = tk.StringVar(value="Click items or use AUTO-ASSIGN")
        tk.Label(self.win, textvariable=self.learn_status,
                 font=("Consolas", 9), fg="#aaaaaa").pack(pady=3)

    def _on_click(self, event, scale):
        """Handle click on inventory slot — cycle through assignments."""
        col = int(event.x / (self.canvas.winfo_width() / INV_COLS))
        row = int(event.y / (self.canvas.winfo_height() / INV_ROWS))
        idx = row * INV_COLS + col
        if idx >= len(self.slot_rects):
            return

        # Cycle: unassigned -> melee(W) -> mage(Q) -> range(E) -> unassigned
        cycle = [None, "melee", "mage", "range"]
        current = self.slot_assignments.get(idx)
        next_idx = (cycle.index(current) + 1) % len(cycle) if current in cycle else 1
        new_val = cycle[next_idx]

        if new_val:
            self.slot_assignments[idx] = new_val
        elif idx in self.slot_assignments:
            del self.slot_assignments[idx]

        self._redraw_assignments()

    def _auto_assign(self):
        """Auto-assign: first 5 = melee, next 4 = mage, last 4 = range."""
        self.slot_assignments.clear()
        # Find non-empty slots by checking if slot has significant pixel variance
        occupied = self._find_occupied_slots()

        if len(occupied) < 13:
            self.learn_status.set(f"Found {len(occupied)} items — need at least 13")
            # Assign whatever we have in order
            for i, slot_idx in enumerate(occupied):
                if i < 5:
                    self.slot_assignments[slot_idx] = "melee"
                elif i < 9:
                    self.slot_assignments[slot_idx] = "mage"
                else:
                    self.slot_assignments[slot_idx] = "range"
        else:
            for i, slot_idx in enumerate(occupied[:13]):
                if i < 5:
                    self.slot_assignments[slot_idx] = "melee"
                elif i < 9:
                    self.slot_assignments[slot_idx] = "mage"
                else:
                    self.slot_assignments[slot_idx] = "range"

        self._redraw_assignments()
        counts = self._count_assignments()
        self.learn_status.set(f"Auto: W={counts['melee']} Q={counts['mage']} E={counts['range']}")

    def _find_occupied_slots(self):
        """Check which inventory slots have items (non-empty)."""
        img_np = np.array(self.inv_image_pil)
        occupied = []
        for i, (sx, sy, sw, sh) in enumerate(self.slot_rects):
            # Crop slot, check pixel variance
            pad = 4
            crop = img_np[sy+pad:sy+sh-pad, sx+pad:sx+sw-pad]
            if crop.size == 0:
                continue
            variance = np.var(crop)
            if variance > 200:  # non-empty slots have more color variation
                occupied.append(i)
        return occupied

    def _count_assignments(self):
        counts = {"melee": 0, "mage": 0, "range": 0}
        for switch in self.slot_assignments.values():
            if switch in counts:
                counts[switch] += 1
        return counts

    def _redraw_assignments(self):
        """Redraw colored overlays on assigned slots."""
        # Clear previous overlays
        self.canvas.delete("overlay")
        colors = {"melee": "#ff4444", "mage": "#4488ff", "range": "#44ff44"}
        labels = {"melee": "W", "mage": "Q", "range": "E"}

        disp_w = self.canvas.winfo_width()
        disp_h = self.canvas.winfo_height()
        slot_w_disp = disp_w / INV_COLS
        slot_h_disp = disp_h / INV_ROWS

        for idx, switch in self.slot_assignments.items():
            row = idx // INV_COLS
            col = idx % INV_COLS
            x1 = col * slot_w_disp
            y1 = row * slot_h_disp
            x2 = x1 + slot_w_disp
            y2 = y1 + slot_h_disp
            color = colors.get(switch, "#ffffff")
            self.canvas.create_rectangle(x1, y1, x2, y2, outline=color,
                                          width=3, tags="overlay")
            self.canvas.create_text(x1 + slot_w_disp/2, y1 + 8,
                                     text=labels.get(switch, "?"),
                                     fill=color, font=("Consolas", 10, "bold"),
                                     tags="overlay")

    def _clear_all(self):
        self.slot_assignments.clear()
        self._redraw_assignments()
        self.learn_status.set("Cleared — click items or AUTO-ASSIGN")

    def _save_and_close(self):
        """Crop templates from inventory and save config."""
        img_np = np.array(self.inv_image_pil)
        img_bgr = cv2.cvtColor(img_np, cv2.COLOR_RGB2BGR)

        config = load_config()
        config["mage"] = []
        config["melee"] = []
        config["range"] = []

        # Clear old templates
        for f in TEMPLATE_DIR.glob("*.png"):
            f.unlink()

        item_counter = 0
        for idx, switch in sorted(self.slot_assignments.items()):
            sx, sy, sw, sh = self.slot_rects[idx]
            # Crop icon with small padding
            pad = 2
            crop = img_bgr[sy+pad:sy+sh-pad, sx+pad:sx+sw-pad]
            if crop.size == 0:
                continue

            item_name = f"{switch}_{item_counter}"
            item_counter += 1

            # Save template
            self.bot.tpl_store.save_template(item_name, crop)
            config[switch].append(item_name)

        save_config(config)

        # Update bot presets
        self.bot._reload_presets(config)

        counts = self._count_assignments()
        print(f"[LEARN] Saved: W={counts['melee']} Q={counts['mage']} E={counts['range']}")
        print(f"[LEARN] Templates saved to {TEMPLATE_DIR}")

        self.win.destroy()
        self.bot.status = "ACTIVE"
        self.bot.last_action = f"Learned: W={counts['melee']} Q={counts['mage']} E={counts['range']}"

# =============================================================================
# DECIMATOR v4.0 — Main Bot
# =============================================================================

class DecimatorV4:
    def __init__(self):
        self.hwnd = None
        self.game_rect = None
        self.paused = False
        self.running = True
        self.tpl_store = TemplateStore()
        self.action_lock = threading.Lock()
        self.status = "READY"
        self.last_action = ""
        self.root = None

        # Load saved config
        cfg = load_config()
        self.mage_items = cfg.get("mage", [])
        self.melee_items = cfg.get("melee", [])
        self.range_items = cfg.get("range", [])
        self.spec_weapon = cfg.get("spec_weapon", "ags")

    def _reload_presets(self, cfg):
        self.mage_items = cfg.get("mage", [])
        self.melee_items = cfg.get("melee", [])
        self.range_items = cfg.get("range", [])
        self.tpl_store.load_all()

    # --- Window ---

    def detect_game(self):
        found = find_roat_window()
        if found:
            self.hwnd, title = found
            self.game_rect = get_window_rect(self.hwnd)
            return True
        return False

    def refresh_rect(self):
        if self.hwnd:
            self.game_rect = get_window_rect(self.hwnd)

    def game_xy(self, x_pct, y_pct):
        self.refresh_rect()
        l, t, r, b = self.game_rect
        return int(l + (r-l) * x_pct), int(t + (b-t) * y_pct)

    # --- Inventory ---

    def screenshot_inv(self):
        """Capture inventory region as BGR array + offset."""
        self.refresh_rect()
        l, t, r, b = self.game_rect
        gw, gh = r-l, b-t
        inv_l = int(l + gw * INV_START_X_PCT)
        inv_t = int(t + gh * INV_START_Y_PCT)
        inv_r = int(l + gw * INV_END_X_PCT)
        inv_b = int(t + gh * INV_END_Y_PCT)
        img = ImageGrab.grab(bbox=(inv_l, inv_t, inv_r, inv_b))
        return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR), inv_l, inv_t

    def equip_items(self, item_names):
        """Find and fast-click each item in inventory."""
        if not item_names:
            return
        inv_img, ox, oy = self.screenshot_inv()
        for name in item_names:
            match = self.tpl_store.find_in_region(name, inv_img)
            if match:
                cx, cy, conf = match
                fast_click(ox + cx, oy + cy)
                time.sleep(random.uniform(0.05, 0.09))
                # Re-screenshot (items shift when equipped)
                inv_img, ox, oy = self.screenshot_inv()

    # --- Actions ---

    def switch_mage(self):
        if self.paused or not self.hwnd:
            return
        with self.action_lock:
            self.status = "MAGE SWITCH"
            self.last_action = "Q - Mage"
            focus_window(self.hwnd)
            send_game_key('f2')
            time.sleep(random.uniform(0.10, 0.18))
            self.equip_items(self.mage_items)
            # Open spellbook + click Ice Barrage
            send_game_key('f4')
            time.sleep(random.uniform(0.15, 0.25))
            bx, by = self.game_xy(BARRAGE_X_PCT, BARRAGE_Y_PCT)
            fast_click(bx, by)
            self.status = "ACTIVE"

    def switch_melee(self):
        if self.paused or not self.hwnd:
            return
        with self.action_lock:
            self.status = "MELEE SWITCH"
            self.last_action = "W - Melee"
            focus_window(self.hwnd)
            send_game_key('f2')
            time.sleep(random.uniform(0.10, 0.18))
            self.equip_items(self.melee_items)
            send_game_key('f1')
            self.status = "ACTIVE"

    def switch_range(self):
        if self.paused or not self.hwnd:
            return
        with self.action_lock:
            self.status = "RANGE SWITCH"
            self.last_action = "E - Range"
            focus_window(self.hwnd)
            send_game_key('f2')
            time.sleep(random.uniform(0.10, 0.18))
            self.equip_items(self.range_items)
            self.status = "ACTIVE"

    def spec_combo(self):
        if self.paused or not self.hwnd:
            return
        with self.action_lock:
            self.status = "SPEC COMBO"
            self.last_action = "R - SPEC!"
            focus_window(self.hwnd)
            # 1. Open inventory
            send_game_key('f2')
            time.sleep(random.uniform(0.08, 0.14))
            # 2. Equip melee gear
            self.equip_items(self.melee_items)
            # 3. Equip spec weapon (look for it by template name)
            spec_tpl = f"spec_{self.spec_weapon}"
            if spec_tpl in self.tpl_store.templates:
                self.equip_items([spec_tpl])
            # 4. Combat tab + spec bar
            send_game_key('f1')
            time.sleep(random.uniform(0.10, 0.18))
            sx, sy = self.game_xy(SPEC_BAR_X_PCT, SPEC_BAR_Y_PCT)
            fast_click(sx, sy)
            time.sleep(random.uniform(0.05, 0.10))
            # 5. Click target
            tx, ty = self.game_xy(VP_CX_PCT, VP_CY_PCT)
            fast_click(tx, ty)
            self.status = "ACTIVE"

    def toggle_pause(self):
        self.paused = not self.paused
        self.status = "PAUSED" if self.paused else "ACTIVE"
        self.last_action = "F12 - " + ("Paused" if self.paused else "Resumed")

    def start_learn(self):
        """Launch Learn Mode window."""
        if self.root:
            self.status = "LEARNING"
            self.last_action = "F11 - Learn Mode"
            learn = LearnWindow(self)
            learn.capture_and_show()

    # --- Keyboard ---

    def on_key_press(self, key):
        try:
            # F11/F12 work always
            if key == keyboard.Key.f11:
                if self.root:
                    self.root.after(0, self.start_learn)
                return
            if key == keyboard.Key.f12:
                self.toggle_pause()
                return

            # Q/W/E/R only when game is focused
            if not is_game_focused(self.hwnd):
                return

            if hasattr(key, 'char') and key.char:
                ch = key.char.lower()
                if ch == 'q':
                    threading.Thread(target=self.switch_mage, daemon=True).start()
                elif ch == 'w':
                    threading.Thread(target=self.switch_melee, daemon=True).start()
                elif ch == 'e':
                    threading.Thread(target=self.switch_range, daemon=True).start()
                elif ch == 'r':
                    threading.Thread(target=self.spec_combo, daemon=True).start()
        except Exception as ex:
            print(f"[KEY ERR] {ex}")

    # --- GUI ---

    def build_gui(self):
        self.root = tk.Tk()
        self.root.title("Decimator v4")
        self.root.geometry("260x220+50+50")
        self.root.attributes('-topmost', True)
        self.root.configure(bg='#1a1a2e')
        self.root.resizable(False, False)

        tk.Label(self.root, text="DECIMATOR v4.0",
                 font=("Consolas", 14, "bold"), fg="#e94560",
                 bg="#1a1a2e").pack(pady=(8, 3))

        self.status_var = tk.StringVar(value="READY")
        self.status_label = tk.Label(self.root, textvariable=self.status_var,
                                      font=("Consolas", 16, "bold"), fg="#00ff00",
                                      bg="#1a1a2e")
        self.status_label.pack(pady=3)

        self.action_var = tk.StringVar(value="")
        tk.Label(self.root, textvariable=self.action_var,
                 font=("Consolas", 10), fg="#aaaaaa", bg="#1a1a2e").pack()

        # Spec weapon
        frame = tk.Frame(self.root, bg="#1a1a2e")
        frame.pack(pady=6)
        tk.Label(frame, text="Spec:", font=("Consolas", 10),
                 fg="#cccccc", bg="#1a1a2e").pack(side=tk.LEFT, padx=3)
        self.spec_var = tk.StringVar(value=self.spec_weapon)
        specs = ["ags", "dds", "dragon_mace", "gmaul", "vls", "swd", "dclaws"]
        self.spec_dd = ttk.Combobox(frame, textvariable=self.spec_var,
                                     values=specs, width=12, state="readonly")
        self.spec_dd.pack(side=tk.LEFT)
        self.spec_dd.bind("<<ComboboxSelected>>", self._on_spec)

        # Gear count display
        n_m = len(self.melee_items)
        n_q = len(self.mage_items)
        n_e = len(self.range_items)
        gear_text = f"Gear: W={n_m}  Q={n_q}  E={n_e}"
        if n_m == 0 and n_q == 0 and n_e == 0:
            gear_text = "No gear loaded — press F11 to learn"
        self.gear_var = tk.StringVar(value=gear_text)
        tk.Label(self.root, textvariable=self.gear_var,
                 font=("Consolas", 9), fg="#888888", bg="#1a1a2e").pack(pady=2)

        legend = "Q:Mage W:Melee E:Range R:Spec\nF11:Learn  F12:Pause"
        tk.Label(self.root, text=legend, font=("Consolas", 8),
                 fg="#555555", bg="#1a1a2e").pack(side=tk.BOTTOM, pady=4)

    def _on_spec(self, event=None):
        self.spec_weapon = self.spec_var.get()
        cfg = load_config()
        cfg["spec_weapon"] = self.spec_weapon
        save_config(cfg)
        self.last_action = f"Spec -> {self.spec_weapon.upper()}"

    def update_gui(self):
        if not self.running:
            return
        self.status_var.set(self.status)
        self.action_var.set(self.last_action)
        colors = {
            "ACTIVE": "#00ff00", "READY": "#00ff00", "PAUSED": "#ffaa00",
            "MAGE SWITCH": "#00ccff", "MELEE SWITCH": "#ff4444",
            "RANGE SWITCH": "#44ff44", "SPEC COMBO": "#ff00ff",
            "LEARNING": "#ffff00",
        }
        self.status_label.config(fg=colors.get(self.status, "#ffffff"))

        # Update gear count
        n_m = len(self.melee_items)
        n_q = len(self.mage_items)
        n_e = len(self.range_items)
        if n_m + n_q + n_e > 0:
            self.gear_var.set(f"Gear: W={n_m}  Q={n_q}  E={n_e}")
        else:
            self.gear_var.set("No gear loaded - press F11 to learn")

        self.root.after(150, self.update_gui)

    # --- Main ---

    def run(self):
        print("=" * 50)
        print("  ROAT PKZ DECIMATOR v4.0 — PVP ASSIST")
        print("=" * 50)
        print("[*] Looking for Roat Pkz window...")

        if self.detect_game():
            print(f"[+] Game window found: {self.game_rect}")
        else:
            print("[!] Game not found — will retry automatically")

        self.listener = keyboard.Listener(on_press=self.on_key_press)
        self.listener.daemon = True
        self.listener.start()
        print("[*] Keyboard listener active")
        print("[*] Q=Mage  W=Melee  E=Range  R=Spec")
        print("[*] F11=Learn Mode  F12=Pause/Resume")

        if len(self.tpl_store.templates) == 0:
            print("[!] No gear templates found — press F11 to enter Learn Mode")
        else:
            print(f"[+] Loaded {len(self.tpl_store.templates)} item templates")

        self.build_gui()
        self.root.after(150, self.update_gui)

        def check_game():
            if not self.running:
                return
            if not self.hwnd:
                if self.detect_game():
                    print("[+] Game window found!")
                    self.status = "ACTIVE"
            self.root.after(3000, check_game)
        self.root.after(1000, check_game)

        try:
            self.root.mainloop()
        except KeyboardInterrupt:
            pass
        finally:
            self.running = False
            self.listener.stop()

# =============================================================================
# ENTRY
# =============================================================================
def main():
    bot = DecimatorV4()
    bot.run()

if __name__ == "__main__":
    main()
