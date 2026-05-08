#!/usr/bin/env python3
"""
ROAT PKZ DECIMATOR v4.1 — PVP Assist Tool (Color Marker Edition)
================================================================
Detects RuneLite inventory tag colors to find gear pieces.
Blue=Q(Mage) Red=W(Melee) Green=E(Range) Yellow=R(Spec)

HOTKEYS:
  Q = Magic switch (equip blue items + F4 spellbook + click Ice Barrage)
  W = Melee switch (equip red items + F1 combat tab)
  E = Range switch (equip green items + F2 inventory tab)
  R = Spec combo (equip yellow spec weapon + F1 combat tab + click spec bar + click target)
  F12 = Pause / Resume

CALIBRATE: python bot.py --calibrate
  Points mouse to: inventory corners, Ice Barrage spell, spec bar

REQUIREMENTS: pip install pyautogui pillow pynput
"""

import pyautogui
import time
import sys
import os
import ctypes
import json
import threading
from PIL import Image, ImageGrab
from pynput import keyboard

# ─── SPEED ───
pyautogui.PAUSE = 0
pyautogui.FAILSAFE = False

# ─── COLOR DEFINITIONS ───
COLOR_RANGES = {
    "blue":   {"r": (0, 120),   "g": (0, 120),   "b": (140, 255)},
    "red":    {"r": (140, 255), "g": (0, 100),    "b": (0, 100)},
    "green":  {"r": (0, 120),   "g": (140, 255),  "b": (0, 120)},
    "yellow": {"r": (180, 255), "g": (180, 255),  "b": (0, 100)},
}

# ─── INVENTORY GRID CONSTANTS ───
INV_COLS = 4
INV_ROWS = 7
INV_SLOTS = 28

# Default positions relative to game window (standard RuneLite 765x503)
INV_RELATIVE = {
    "x_start": 563,
    "y_start": 213,
    "slot_w": 42,
    "slot_h": 36,
    "pad_x": 6,
    "pad_y": 4,
}

# Special click positions relative to game window
SPEC_BAR_RELATIVE = {"x": 643, "y": 423}
ICE_BARRAGE_RELATIVE = {"x": 687, "y": 375}
# Target click = center of viewport (where opponent is)
TARGET_RELATIVE = {"x": 345, "y": 268}

# ─── STATE ───
paused = False
running = True
game_hwnd = None
game_rect = None
switch_lock = threading.Lock()

# ─── WIN32 HELPERS ───
if sys.platform == "win32":
    import ctypes.wintypes
    user32 = ctypes.windll.user32
    EnumWindows = user32.EnumWindows
    EnumWindowsProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)
    GetWindowTextW = user32.GetWindowTextW
    GetWindowTextLengthW = user32.GetWindowTextLengthW
    IsWindowVisible = user32.IsWindowVisible
    SetForegroundWindow = user32.SetForegroundWindow
    GetForegroundWindow = user32.GetForegroundWindow
    GetWindowRect = user32.GetWindowRect

def find_game_window():
    """Find Roat Pkz window — excludes our own windows."""
    global game_hwnd, game_rect
    results = []
    def callback(hwnd, lParam):
        if IsWindowVisible(hwnd):
            length = GetWindowTextLengthW(hwnd)
            if length > 0:
                buf = ctypes.create_unicode_buffer(length + 1)
                GetWindowTextW(hwnd, buf, length + 1)
                title = buf.value.lower()
                if ("roat" in title or "pkz" in title) and "decimator" not in title:
                    results.append(hwnd)
        return True
    EnumWindows(EnumWindowsProc(callback), 0)
    if results:
        game_hwnd = results[0]
        rect = ctypes.wintypes.RECT()
        GetWindowRect(game_hwnd, ctypes.byref(rect))
        game_rect = (rect.left, rect.top, rect.right, rect.bottom)
        return True
    return False

def is_game_focused():
    if game_hwnd is None:
        return False
    return GetForegroundWindow() == game_hwnd

def focus_game():
    if game_hwnd:
        try:
            SetForegroundWindow(game_hwnd)
            time.sleep(0.05)
        except:
            pass

# ─── INVENTORY SCANNING ───

def get_slot_center(slot_index):
    """Get screen coordinates for CENTER of inventory slot 0-27."""
    if game_rect is None:
        return None
    col = slot_index % INV_COLS
    row = slot_index // INV_COLS
    rel_x = INV_RELATIVE["x_start"] + col * (INV_RELATIVE["slot_w"] + INV_RELATIVE["pad_x"]) + INV_RELATIVE["slot_w"] // 2
    rel_y = INV_RELATIVE["y_start"] + row * (INV_RELATIVE["slot_h"] + INV_RELATIVE["pad_y"]) + INV_RELATIVE["slot_h"] // 2
    return (game_rect[0] + rel_x, game_rect[1] + rel_y)

def get_slot_region(slot_index):
    """Get screen region (left, top, right, bottom) for a slot — used for color sampling."""
    if game_rect is None:
        return None
    col = slot_index % INV_COLS
    row = slot_index // INV_COLS
    x = game_rect[0] + INV_RELATIVE["x_start"] + col * (INV_RELATIVE["slot_w"] + INV_RELATIVE["pad_x"])
    y = game_rect[1] + INV_RELATIVE["y_start"] + row * (INV_RELATIVE["slot_h"] + INV_RELATIVE["pad_y"])
    return (x, y, x + INV_RELATIVE["slot_w"], y + INV_RELATIVE["slot_h"])

def detect_slot_color(img, slot_index):
    """Check what color marker tag is on this slot.
    Samples the center 60% of the slot to avoid edge noise.
    Returns 'blue','red','green','yellow' or None.
    """
    col = slot_index % INV_COLS
    row = slot_index // INV_COLS
    sx = INV_RELATIVE["x_start"] + col * (INV_RELATIVE["slot_w"] + INV_RELATIVE["pad_x"])
    sy = INV_RELATIVE["y_start"] + row * (INV_RELATIVE["slot_h"] + INV_RELATIVE["pad_y"])
    # Sample center 60%
    margin_x = int(INV_RELATIVE["slot_w"] * 0.2)
    margin_y = int(INV_RELATIVE["slot_h"] * 0.2)
    sample_x1 = sx + margin_x
    sample_y1 = sy + margin_y
    sample_x2 = sx + INV_RELATIVE["slot_w"] - margin_x
    sample_y2 = sy + INV_RELATIVE["slot_h"] - margin_y

    if sample_x2 <= sample_x1 or sample_y2 <= sample_y1:
        return None

    color_counts = {"blue": 0, "red": 0, "green": 0, "yellow": 0}
    total_sampled = 0

    for py in range(sample_y1, sample_y2, 2):
        for px in range(sample_x1, sample_x2, 2):
            try:
                r, g, b = img.getpixel((px, py))[:3]
            except:
                continue
            total_sampled += 1
            for color_name, ranges in COLOR_RANGES.items():
                if (ranges["r"][0] <= r <= ranges["r"][1] and
                    ranges["g"][0] <= g <= ranges["g"][1] and
                    ranges["b"][0] <= b <= ranges["b"][1]):
                    color_counts[color_name] += 1
                    break

    if total_sampled == 0:
        return None

    # Need at least 15% of pixels matching to count as tagged
    threshold = total_sampled * 0.15
    best = max(color_counts, key=color_counts.get)
    if color_counts[best] >= threshold:
        return best
    return None

def scan_inventory():
    """Screenshot game area and detect color-tagged slots.
    Returns dict: {'blue': [slot_indices], 'red': [...], ...}
    """
    if game_rect is None:
        find_game_window()
    if game_rect is None:
        return {}

    # Refresh window position
    rect = ctypes.wintypes.RECT()
    GetWindowRect(game_hwnd, ctypes.byref(rect))
    game_rect_local = (rect.left, rect.top, rect.right, rect.bottom)

    # Screenshot just the game window
    try:
        img = ImageGrab.grab(bbox=game_rect_local)
    except Exception as e:
        print(f"  Screenshot error: {e}")
        return {}

    result = {"blue": [], "red": [], "green": [], "yellow": []}
    for slot in range(INV_SLOTS):
        color = detect_slot_color(img, slot)
        if color:
            result[color].append(slot)

    return result

# ─── CLICKING ───

def fast_click(x, y):
    """Instant move + click — no humanization needed for PVP speed."""
    ctypes.windll.user32.SetCursorPos(int(x), int(y))
    time.sleep(0.008)
    pyautogui.click()

def click_slots(slot_list):
    """Click a list of inventory slots rapidly."""
    for slot in slot_list:
        center = get_slot_center(slot)
        if center:
            fast_click(center[0], center[1])
            time.sleep(0.035)  # ~35ms between clicks

def click_relative(rel_x, rel_y):
    """Click a position relative to game window."""
    if game_rect is None:
        return
    screen_x = game_rect[0] + rel_x
    screen_y = game_rect[1] + rel_y
    fast_click(screen_x, screen_y)

# ─── SWITCH FUNCTIONS ───

def do_mage_switch():
    """Q — Equip all blue items + open spellbook + pre-select Ice Barrage."""
    if not switch_lock.acquire(blocking=False):
        return
    try:
        if not is_game_focused():
            return
        print("[Q] MAGE SWITCH")
        find_game_window()  # refresh position

        inv = scan_inventory()
        blue_slots = inv.get("blue", [])

        if blue_slots:
            focus_game()
            click_slots(blue_slots)

        # Open spellbook (F4) + click Ice Barrage
        time.sleep(0.02)
        pyautogui.press("F4")
        time.sleep(0.08)
        click_relative(ICE_BARRAGE_RELATIVE["x"], ICE_BARRAGE_RELATIVE["y"])
        print(f"  -> Equipped {len(blue_slots)} mage items + Ice Barrage ready")
    finally:
        switch_lock.release()

def do_melee_switch():
    """W — Equip all red items + open combat tab."""
    if not switch_lock.acquire(blocking=False):
        return
    try:
        if not is_game_focused():
            return
        print("[W] MELEE SWITCH")
        find_game_window()

        inv = scan_inventory()
        red_slots = inv.get("red", [])

        if red_slots:
            focus_game()
            click_slots(red_slots)

        # Open combat tab (F1)
        time.sleep(0.02)
        pyautogui.press("F1")
        print(f"  -> Equipped {len(red_slots)} melee items")
    finally:
        switch_lock.release()

def do_range_switch():
    """E — Equip all green items + open inventory tab."""
    if not switch_lock.acquire(blocking=False):
        return
    try:
        if not is_game_focused():
            return
        print("[E] RANGE SWITCH")
        find_game_window()

        inv = scan_inventory()
        green_slots = inv.get("green", [])

        if green_slots:
            focus_game()
            click_slots(green_slots)

        # Open inventory tab (F2) so we can see gear
        time.sleep(0.02)
        pyautogui.press("F2")
        print(f"  -> Equipped {len(green_slots)} range items")
    finally:
        switch_lock.release()

def do_spec_combo():
    """R — Full spec KO combo:
    1. Equip yellow (spec weapon)
    2. Open combat tab (F1)
    3. Click spec bar
    4. Click target (center of viewport)
    """
    if not switch_lock.acquire(blocking=False):
        return
    try:
        if not is_game_focused():
            return
        print("[R] SPEC COMBO")
        find_game_window()

        inv = scan_inventory()
        yellow_slots = inv.get("yellow", [])

        focus_game()

        # Step 1: Equip spec weapon
        if yellow_slots:
            click_slots(yellow_slots)
            time.sleep(0.02)

        # Step 2: Open combat tab
        pyautogui.press("F1")
        time.sleep(0.06)

        # Step 3: Click spec bar
        click_relative(SPEC_BAR_RELATIVE["x"], SPEC_BAR_RELATIVE["y"])
        time.sleep(0.04)

        # Step 4: Click spec bar again (toggle on if needed)
        click_relative(SPEC_BAR_RELATIVE["x"], SPEC_BAR_RELATIVE["y"])
        time.sleep(0.04)

        # Step 5: Click target (center of game viewport)
        click_relative(TARGET_RELATIVE["x"], TARGET_RELATIVE["y"])
        print(f"  -> Spec weapon equipped + spec bar + attack!")
    finally:
        switch_lock.release()

# ─── KEYBOARD LISTENER ───

def on_key_press(key):
    global paused, running
    try:
        if key == keyboard.Key.f12:
            paused = not paused
            state = "PAUSED" if paused else "ACTIVE"
            print(f"\n[F12] {state}")
            return

        if paused:
            return

        if hasattr(key, 'char') and key.char:
            k = key.char.lower()
            if k == 'q':
                threading.Thread(target=do_mage_switch, daemon=True).start()
            elif k == 'w':
                threading.Thread(target=do_melee_switch, daemon=True).start()
            elif k == 'e':
                threading.Thread(target=do_range_switch, daemon=True).start()
            elif k == 'r':
                threading.Thread(target=do_spec_combo, daemon=True).start()
    except:
        pass

# ─── CALIBRATION MODE ───

def calibrate():
    """Interactive calibration — point mouse to key positions.
    Saves to calibration.json for persistent config.
    """
    print("\n" + "=" * 50)
    print("  DECIMATOR v4.1 CALIBRATION")
    print("=" * 50)
    print("\nMake sure Roat Pkz is open with INVENTORY tab visible.")
    print("You'll point your mouse to 4 positions.\n")

    if not find_game_window():
        print("ERROR: Can't find Roat Pkz window!")
        print("Make sure the game is open and visible.")
        return

    print(f"Found game window at {game_rect}")
    cal = {}

    # 1. Inventory top-left
    input("\n1. Move mouse to TOP-LEFT corner of FIRST inventory slot (slot 1), press ENTER: ")
    mx, my = pyautogui.position()
    cal["inv_x_start"] = mx - game_rect[0]
    cal["inv_y_start"] = my - game_rect[1]
    print(f"   Saved: ({cal['inv_x_start']}, {cal['inv_y_start']})")

    # 2. Inventory bottom-right
    input("\n2. Move mouse to BOTTOM-RIGHT corner of LAST inventory slot (slot 28), press ENTER: ")
    mx, my = pyautogui.position()
    br_x = mx - game_rect[0]
    br_y = my - game_rect[1]
    total_w = br_x - cal["inv_x_start"]
    total_h = br_y - cal["inv_y_start"]
    cal["slot_w"] = total_w // INV_COLS
    cal["slot_h"] = total_h // INV_ROWS
    print(f"   Slot size: {cal['slot_w']}x{cal['slot_h']}")

    # 3. Ice Barrage
    print("\n3. Now open your SPELLBOOK (F4) and point to ICE BARRAGE.")
    input("   Move mouse to ICE BARRAGE spell, press ENTER: ")
    mx, my = pyautogui.position()
    cal["ice_barrage_x"] = mx - game_rect[0]
    cal["ice_barrage_y"] = my - game_rect[1]
    print(f"   Saved Ice Barrage at: ({cal['ice_barrage_x']}, {cal['ice_barrage_y']})")

    # 4. Spec bar
    print("\n4. Now open COMBAT TAB (F1) and point to the SPEC BAR.")
    input("   Move mouse to the center of SPECIAL ATTACK bar, press ENTER: ")
    mx, my = pyautogui.position()
    cal["spec_bar_x"] = mx - game_rect[0]
    cal["spec_bar_y"] = my - game_rect[1]
    print(f"   Saved Spec Bar at: ({cal['spec_bar_x']}, {cal['spec_bar_y']})")

    # Save
    cal_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calibration.json")
    with open(cal_path, "w") as f:
        json.dump(cal, f, indent=2)

    print(f"\n{'=' * 50}")
    print(f"  CALIBRATION SAVED to {cal_path}")
    print(f"  Inventory: start=({cal['inv_x_start']},{cal['inv_y_start']}) slot={cal['slot_w']}x{cal['slot_h']}")
    print(f"  Ice Barrage: ({cal['ice_barrage_x']}, {cal['ice_barrage_y']})")
    print(f"  Spec Bar: ({cal['spec_bar_x']}, {cal['spec_bar_y']})")
    print(f"{'=' * 50}")
    print("\nCalibration complete! Run the bot normally now: python bot.py")

def load_calibration():
    """Load saved calibration if available."""
    global INV_RELATIVE, ICE_BARRAGE_RELATIVE, SPEC_BAR_RELATIVE
    cal_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calibration.json")
    if os.path.exists(cal_path):
        try:
            with open(cal_path) as f:
                cal = json.load(f)
            if "inv_x_start" in cal:
                INV_RELATIVE["x_start"] = cal["inv_x_start"]
                INV_RELATIVE["y_start"] = cal["inv_y_start"]
                INV_RELATIVE["slot_w"] = cal["slot_w"]
                INV_RELATIVE["slot_h"] = cal["slot_h"]
                INV_RELATIVE["pad_x"] = 0
                INV_RELATIVE["pad_y"] = 0
            if "ice_barrage_x" in cal:
                ICE_BARRAGE_RELATIVE["x"] = cal["ice_barrage_x"]
                ICE_BARRAGE_RELATIVE["y"] = cal["ice_barrage_y"]
            if "spec_bar_x" in cal:
                SPEC_BAR_RELATIVE["x"] = cal["spec_bar_x"]
                SPEC_BAR_RELATIVE["y"] = cal["spec_bar_y"]
            print(f"  Loaded calibration from {cal_path}")
            return True
        except Exception as e:
            print(f"  Calibration load error: {e}")
    return False

# ─── MAIN ───

def main():
    print("=" * 50)
    print("  ROAT PKZ DECIMATOR v4.1")
    print("  Color Marker PVP Assist Tool")
    print("=" * 50)

    # Load calibration
    load_calibration()

    # Find game
    if not find_game_window():
        print("\nWARNING: Roat Pkz window not found!")
        print("Start the game and the bot will auto-detect it.")
    else:
        print(f"\n  Game window found at {game_rect}")

    # Check for calibration flag
    if len(sys.argv) > 1 and sys.argv[1] == "--calibrate":
        calibrate()
        return

    print("\n  HOTKEYS:")
    print("    Q = Mage switch (blue) + Ice Barrage")
    print("    W = Melee switch (red) + combat tab")
    print("    E = Range switch (green) + inventory tab")
    print("    R = SPEC COMBO (yellow) + spec bar + attack")
    print("    F12 = Pause / Resume")
    print("\n  CALIBRATE: python bot.py --calibrate")
    print("\n  COLOR TAGS (RuneLite Inventory Tags):")
    print("    Blue = Mage | Red = Melee | Green = Range | Yellow = Spec")
    print("\n  Ready! Listening for hotkeys...")
    print("  (Only fires when Roat Pkz is focused)")
    print("=" * 50)

    # Start keyboard listener
    listener = keyboard.Listener(on_press=on_key_press)
    listener.start()

    try:
        while running:
            # Periodically refresh game window position
            find_game_window()
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        listener.stop()

if __name__ == "__main__":
    main()
