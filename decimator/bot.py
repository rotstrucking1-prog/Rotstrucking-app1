#!/usr/bin/env python3
"""
ROAT PKZ DECIMATOR v4.2 — PVP Assist Tool (Color Marker Edition)
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
  3-point calibration for exact slot sizing

DEBUG:    python bot.py --debug
  Screenshots game, draws grid overlay on all 28 slots, labels detected colors.
  Saves debug_grid.png so you can verify alignment.

REQUIREMENTS: pip install pyautogui pillow pynput
"""

import pyautogui
import time
import sys
import os
import ctypes
import json
import threading
from PIL import Image, ImageGrab, ImageDraw, ImageFont
from pynput import keyboard

# --- SPEED ---
pyautogui.PAUSE = 0
pyautogui.FAILSAFE = False

# --- COLOR DEFINITIONS ---
# RuneLite tag colors are semi-transparent overlays at slot edges/corners.
# These ranges are intentionally wide to catch blended pixels.
COLOR_RANGES = {
    "blue":   {"r": (0, 140),   "g": (0, 140),   "b": (120, 255)},
    "red":    {"r": (120, 255), "g": (0, 120),    "b": (0, 120)},
    "green":  {"r": (0, 140),   "g": (120, 255),  "b": (0, 140)},
    "yellow": {"r": (150, 255), "g": (150, 255),  "b": (0, 120)},
}

# Color display names for debug
COLOR_DRAW = {
    "blue":   (0, 120, 255),
    "red":    (255, 40, 40),
    "green":  (40, 220, 40),
    "yellow": (255, 220, 0),
    None:     (128, 128, 128),
}

# --- INVENTORY GRID ---
INV_COLS = 4
INV_ROWS = 7
INV_SLOTS = 28

# Standard RuneLite inventory layout (relative to game window top-left).
# These are the DEFAULT values — calibration overrides them.
# x_start/y_start = top-left pixel of slot 0.
# pitch_x/pitch_y = distance from one slot's left edge to the next slot's left edge.
# slot_w/slot_h = clickable area of one slot (for color sampling bounds).
INV_GRID = {
    "x_start": 563,  # left edge of slot 0
    "y_start": 213,  # top edge of slot 0
    "pitch_x": 42,   # pixels from slot N left to slot N+1 left (horizontally)
    "pitch_y": 36,   # pixels from slot N top to slot N+1 top (vertically)
    "slot_w": 36,     # pixel width of the actual slot box (for sampling)
    "slot_h": 32,     # pixel height of the actual slot box (for sampling)
}

# Special click positions relative to game window
SPEC_BAR_RELATIVE = {"x": 643, "y": 423}
ICE_BARRAGE_RELATIVE = {"x": 687, "y": 375}
TARGET_RELATIVE = {"x": 345, "y": 268}

# --- STATE ---
paused = False
running = True
game_hwnd = None
game_rect = None
switch_lock = threading.Lock()

# --- WIN32 HELPERS ---
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

# --- SLOT POSITION HELPERS ---

def get_slot_origin(slot_index):
    """Get the relative (to game window) top-left corner of a slot."""
    col = slot_index % INV_COLS
    row = slot_index // INV_COLS
    x = INV_GRID["x_start"] + col * INV_GRID["pitch_x"]
    y = INV_GRID["y_start"] + row * INV_GRID["pitch_y"]
    return (x, y)

def get_slot_center_screen(slot_index):
    """Get absolute screen coordinates for center of inventory slot 0-27."""
    if game_rect is None:
        return None
    rx, ry = get_slot_origin(slot_index)
    cx = rx + INV_GRID["slot_w"] // 2
    cy = ry + INV_GRID["slot_h"] // 2
    return (game_rect[0] + cx, game_rect[1] + cy)

# --- INVENTORY COLOR SCANNING ---

def detect_slot_color(img, slot_index):
    """Check what color marker tag is on this slot.
    Samples CORNERS + EDGES of the slot where tag color is most visible
    (item sprite covers the center, tag color shows around the edges).
    Returns 'blue','red','green','yellow' or None.
    """
    sx, sy = get_slot_origin(slot_index)
    w = INV_GRID["slot_w"]
    h = INV_GRID["slot_h"]

    # Build sample points: 4 corners (3x3 blocks) + 4 edge midpoints (3px each)
    sample_points = []

    # Corners — 3x3 pixel blocks at each corner
    for dy in range(3):
        for dx in range(3):
            sample_points.append((sx + dx, sy + dy))              # top-left
            sample_points.append((sx + w - 3 + dx, sy + dy))      # top-right
            sample_points.append((sx + dx, sy + h - 3 + dy))      # bottom-left
            sample_points.append((sx + w - 3 + dx, sy + h - 3 + dy))  # bottom-right

    # Edge midpoints — 3 pixels each
    mid_x = sx + w // 2
    mid_y = sy + h // 2
    for d in [-1, 0, 1]:
        sample_points.append((mid_x + d, sy))          # top edge mid
        sample_points.append((mid_x + d, sy + h - 1))  # bottom edge mid
        sample_points.append((sx, mid_y + d))           # left edge mid
        sample_points.append((sx + w - 1, mid_y + d))   # right edge mid

    color_counts = {"blue": 0, "red": 0, "green": 0, "yellow": 0}
    total_sampled = 0

    for px, py in sample_points:
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

    # Need at least 8% of sampled pixels matching
    threshold = max(total_sampled * 0.08, 3)
    best = max(color_counts, key=color_counts.get)
    if color_counts[best] >= threshold:
        return best
    return None

def scan_inventory():
    """Screenshot game area and detect color-tagged slots.
    Returns dict: {'blue': [slot_indices], 'red': [...], ...}
    """
    global game_rect, game_hwnd
    if game_rect is None:
        find_game_window()
    if game_rect is None:
        return {}

    # Refresh window position
    rect = ctypes.wintypes.RECT()
    GetWindowRect(game_hwnd, ctypes.byref(rect))
    game_rect = (rect.left, rect.top, rect.right, rect.bottom)

    # Screenshot just the game window
    try:
        img = ImageGrab.grab(bbox=game_rect)
    except Exception as e:
        print(f"  Screenshot error: {e}")
        return {}

    result = {"blue": [], "red": [], "green": [], "yellow": []}
    for slot in range(INV_SLOTS):
        color = detect_slot_color(img, slot)
        if color:
            result[color].append(slot)

    # Debug output
    tagged = sum(len(v) for v in result.values())
    if tagged > 0:
        parts = []
        for c in ["blue", "red", "green", "yellow"]:
            if result[c]:
                parts.append(f"{c}:{result[c]}")
        print(f"  [SCAN] Found {tagged} tagged slots: {', '.join(parts)}")
    else:
        print("  [SCAN] No tagged slots found")

    return result

# --- CLICKING ---

def fast_click(x, y):
    """Instant move + click."""
    ctypes.windll.user32.SetCursorPos(int(x), int(y))
    time.sleep(0.008)
    pyautogui.click()

def click_slots(slot_list):
    """Click a list of inventory slots rapidly."""
    for slot in slot_list:
        center = get_slot_center_screen(slot)
        if center:
            fast_click(center[0], center[1])
            time.sleep(0.035)

def click_relative(rel_x, rel_y):
    """Click a position relative to game window."""
    if game_rect is None:
        return
    screen_x = game_rect[0] + rel_x
    screen_y = game_rect[1] + rel_y
    fast_click(screen_x, screen_y)

# --- SWITCH FUNCTIONS ---

def do_mage_switch():
    """Q — Equip all blue items + open spellbook + pre-select Ice Barrage."""
    if not switch_lock.acquire(blocking=False):
        return
    try:
        if not is_game_focused():
            return
        print("[Q] MAGE SWITCH")
        find_game_window()
        inv = scan_inventory()
        blue_slots = inv.get("blue", [])
        if blue_slots:
            focus_game()
            click_slots(blue_slots)
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
        time.sleep(0.02)
        pyautogui.press("F2")
        print(f"  -> Equipped {len(green_slots)} range items")
    finally:
        switch_lock.release()

def do_spec_combo():
    """R — Full spec KO combo."""
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
        if yellow_slots:
            click_slots(yellow_slots)
            time.sleep(0.02)
        pyautogui.press("F1")
        time.sleep(0.06)
        click_relative(SPEC_BAR_RELATIVE["x"], SPEC_BAR_RELATIVE["y"])
        time.sleep(0.04)
        click_relative(SPEC_BAR_RELATIVE["x"], SPEC_BAR_RELATIVE["y"])
        time.sleep(0.04)
        click_relative(TARGET_RELATIVE["x"], TARGET_RELATIVE["y"])
        print(f"  -> Spec weapon equipped + spec bar + attack!")
    finally:
        switch_lock.release()

# --- KEYBOARD LISTENER ---

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

# --- CALIBRATION MODE (3-POINT) ---

def calibrate():
    """Interactive 3-point calibration for exact slot spacing.
    Point 1: Center of slot 1 (top-left slot)
    Point 2: Center of slot 2 (one slot right of slot 1)
    Point 3: Center of slot 5 (one slot below slot 1)
    From these 3 points we derive exact pitch_x, pitch_y, slot positions.
    """
    print("\n" + "=" * 50)
    print("  DECIMATOR v4.2 CALIBRATION (3-Point)")
    print("=" * 50)
    print("\nMake sure Roat Pkz is open with INVENTORY tab visible (F2).")
    print("You'll point your mouse to 5 positions.\n")

    if not find_game_window():
        print("ERROR: Can't find Roat Pkz window!")
        return

    print(f"Found game window at {game_rect}")
    cal = {}

    # Point 1: Center of slot 1
    input("\n1. Move mouse to the CENTER of inventory SLOT 1 (top-left slot), press ENTER: ")
    mx1, my1 = pyautogui.position()
    cal["slot1_cx"] = mx1 - game_rect[0]
    cal["slot1_cy"] = my1 - game_rect[1]
    print(f"   Slot 1 center: ({cal['slot1_cx']}, {cal['slot1_cy']})")

    # Point 2: Center of slot 2
    input("\n2. Move mouse to the CENTER of inventory SLOT 2 (one right of slot 1), press ENTER: ")
    mx2, my2 = pyautogui.position()
    cal["slot2_cx"] = mx2 - game_rect[0]
    cal["slot2_cy"] = my2 - game_rect[1]
    pitch_x = cal["slot2_cx"] - cal["slot1_cx"]
    print(f"   Slot 2 center: ({cal['slot2_cx']}, {cal['slot2_cy']}) -> pitch_x = {pitch_x}")

    # Point 3: Center of slot 5 (first slot of row 2)
    input("\n3. Move mouse to the CENTER of inventory SLOT 5 (directly below slot 1), press ENTER: ")
    mx3, my3 = pyautogui.position()
    cal["slot5_cx"] = mx3 - game_rect[0]
    cal["slot5_cy"] = my3 - game_rect[1]
    pitch_y = cal["slot5_cy"] - cal["slot1_cy"]
    print(f"   Slot 5 center: ({cal['slot5_cx']}, {cal['slot5_cy']}) -> pitch_y = {pitch_y}")

    # Derive slot dimensions (slot box is ~85% of pitch — rest is gap)
    cal["pitch_x"] = pitch_x
    cal["pitch_y"] = pitch_y
    cal["slot_w"] = max(int(pitch_x * 0.85), 20)
    cal["slot_h"] = max(int(pitch_y * 0.85), 20)
    # Top-left of slot 0 = center minus half slot size
    cal["x_start"] = cal["slot1_cx"] - cal["slot_w"] // 2
    cal["y_start"] = cal["slot1_cy"] - cal["slot_h"] // 2

    print(f"\n   Derived: pitch=({pitch_x},{pitch_y}) slot_size=({cal['slot_w']}x{cal['slot_h']})")
    print(f"   Grid origin: ({cal['x_start']}, {cal['y_start']})")

    # Point 4: Ice Barrage
    print("\n4. Now open your SPELLBOOK (F4) and point to ICE BARRAGE.")
    input("   Move mouse to ICE BARRAGE spell, press ENTER: ")
    mx, my = pyautogui.position()
    cal["ice_barrage_x"] = mx - game_rect[0]
    cal["ice_barrage_y"] = my - game_rect[1]
    print(f"   Ice Barrage: ({cal['ice_barrage_x']}, {cal['ice_barrage_y']})")

    # Point 5: Spec bar
    print("\n5. Now open COMBAT TAB (F1) and point to the SPEC BAR.")
    input("   Move mouse to center of SPECIAL ATTACK bar, press ENTER: ")
    mx, my = pyautogui.position()
    cal["spec_bar_x"] = mx - game_rect[0]
    cal["spec_bar_y"] = my - game_rect[1]
    print(f"   Spec Bar: ({cal['spec_bar_x']}, {cal['spec_bar_y']})")

    # Save
    cal_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calibration.json")
    with open(cal_path, "w") as f:
        json.dump(cal, f, indent=2)

    print(f"\n{'=' * 50}")
    print(f"  CALIBRATION SAVED to {cal_path}")
    print(f"  Pitch: {pitch_x}px horizontal, {pitch_y}px vertical")
    print(f"  Slot box: {cal['slot_w']}x{cal['slot_h']}")
    print(f"{'=' * 50}")
    print("\nNow run:  python bot.py --debug")
    print("to verify the grid lines up with your inventory slots.")

def load_calibration():
    """Load saved calibration if available."""
    global INV_GRID, ICE_BARRAGE_RELATIVE, SPEC_BAR_RELATIVE
    cal_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calibration.json")
    if os.path.exists(cal_path):
        try:
            with open(cal_path) as f:
                cal = json.load(f)
            if "pitch_x" in cal:
                # v4.2 calibration format
                INV_GRID["x_start"] = cal["x_start"]
                INV_GRID["y_start"] = cal["y_start"]
                INV_GRID["pitch_x"] = cal["pitch_x"]
                INV_GRID["pitch_y"] = cal["pitch_y"]
                INV_GRID["slot_w"] = cal["slot_w"]
                INV_GRID["slot_h"] = cal["slot_h"]
            elif "inv_x_start" in cal:
                # v4.1 calibration format (backwards compatible)
                INV_GRID["x_start"] = cal["inv_x_start"]
                INV_GRID["y_start"] = cal["inv_y_start"]
                INV_GRID["pitch_x"] = cal["slot_w"]  # old format lumped gap into slot
                INV_GRID["pitch_y"] = cal["slot_h"]
                INV_GRID["slot_w"] = int(cal["slot_w"] * 0.85)
                INV_GRID["slot_h"] = int(cal["slot_h"] * 0.85)
            if "ice_barrage_x" in cal:
                ICE_BARRAGE_RELATIVE["x"] = cal["ice_barrage_x"]
                ICE_BARRAGE_RELATIVE["y"] = cal["ice_barrage_y"]
            if "spec_bar_x" in cal:
                SPEC_BAR_RELATIVE["x"] = cal["spec_bar_x"]
                SPEC_BAR_RELATIVE["y"] = cal["spec_bar_y"]
            print(f"  Calibration loaded: pitch=({INV_GRID['pitch_x']},{INV_GRID['pitch_y']}) slot=({INV_GRID['slot_w']}x{INV_GRID['slot_h']})")
            return True
        except Exception as e:
            print(f"  Calibration load error: {e}")
    return False

# --- DEBUG MODE ---

def debug_grid():
    """Screenshot the game, draw grid overlay on all 28 slots, show detected colors.
    Saves debug_grid.png next to bot.py.
    """
    print("\n" + "=" * 50)
    print("  DEBUG GRID OVERLAY")
    print("=" * 50)

    if not find_game_window():
        print("ERROR: Can't find Roat Pkz window!")
        return

    print(f"Game window: {game_rect}")
    print(f"Grid config: x_start={INV_GRID['x_start']} y_start={INV_GRID['y_start']}")
    print(f"  pitch_x={INV_GRID['pitch_x']} pitch_y={INV_GRID['pitch_y']}")
    print(f"  slot_w={INV_GRID['slot_w']} slot_h={INV_GRID['slot_h']}")

    # Screenshot game window
    try:
        img = ImageGrab.grab(bbox=game_rect)
    except Exception as e:
        print(f"Screenshot error: {e}")
        return

    draw = ImageDraw.Draw(img)

    # Try to load a font for labels
    try:
        font = ImageFont.truetype("arial.ttf", 11)
    except:
        try:
            font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11)
        except:
            font = ImageFont.load_default()

    print("\nSlot grid analysis:")
    print("-" * 60)

    for slot in range(INV_SLOTS):
        sx, sy = get_slot_origin(slot)
        w = INV_GRID["slot_w"]
        h = INV_GRID["slot_h"]

        # Detect color for this slot
        color = detect_slot_color(img, slot)
        draw_color = COLOR_DRAW.get(color, (128, 128, 128))

        # Draw rectangle around slot
        line_width = 3 if color else 1
        for i in range(line_width):
            draw.rectangle(
                [sx - i, sy - i, sx + w + i, sy + h + i],
                outline=draw_color
            )

        # Label: slot number + color
        label = f"{slot + 1}"
        if color:
            label += f" {color[0].upper()}"
        draw.text((sx + 2, sy + 2), label, fill=draw_color, font=font)

        # Also sample and print the actual corner pixel colors for debugging
        col = slot % INV_COLS
        row = slot // INV_COLS
        try:
            tl_pixel = img.getpixel((sx + 1, sy + 1))[:3]
            tr_pixel = img.getpixel((sx + w - 2, sy + 1))[:3]
            bl_pixel = img.getpixel((sx + 1, sy + h - 2))[:3]
            br_pixel = img.getpixel((sx + w - 2, sy + h - 2))[:3]
        except:
            tl_pixel = tr_pixel = bl_pixel = br_pixel = (0, 0, 0)

        color_str = color if color else "---"
        print(f"  Slot {slot + 1:2d} (r{row+1}c{col+1}) @ ({sx},{sy}): {color_str:6s}  "
              f"TL={tl_pixel} TR={tr_pixel} BL={bl_pixel} BR={br_pixel}")

    # Save debug image
    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "debug_grid.png")
    img.save(out_path)
    print("-" * 60)

    # Summary
    results = {}
    for slot in range(INV_SLOTS):
        c = detect_slot_color(img, slot)
        if c:
            if c not in results:
                results[c] = []
            results[c].append(slot + 1)

    print(f"\nSummary:")
    if results:
        for c in ["blue", "red", "green", "yellow"]:
            if c in results:
                print(f"  {c.upper()}: slots {results[c]}")
    else:
        print("  NO TAGGED SLOTS DETECTED")
        print("  Check: Are inventory tags enabled in RuneLite? Is the inventory tab open?")

    print(f"\nDebug image saved: {out_path}")
    print("Open it to verify the grid rectangles line up with your inventory slots.")
    print("If they're OFF, run:  python bot.py --calibrate")

# --- MAIN ---

def main():
    print("=" * 50)
    print("  ROAT PKZ DECIMATOR v4.2")
    print("  Color Marker PVP Assist Tool")
    print("=" * 50)

    # Load calibration
    load_calibration()

    # Check for flags BEFORE finding game window
    if len(sys.argv) > 1:
        if sys.argv[1] == "--calibrate":
            if not find_game_window():
                print("\nERROR: Can't find Roat Pkz window!")
                return
            calibrate()
            return
        elif sys.argv[1] == "--debug":
            find_game_window()
            debug_grid()
            return

    # Normal mode
    if not find_game_window():
        print("\nWARNING: Roat Pkz window not found!")
        print("Start the game and the bot will auto-detect it.")
    else:
        print(f"\n  Game window found at {game_rect}")

    print("\n  HOTKEYS:")
    print("    Q = Mage switch (blue) + Ice Barrage")
    print("    W = Melee switch (red) + combat tab")
    print("    E = Range switch (green) + inventory tab")
    print("    R = SPEC COMBO (yellow) + spec bar + attack")
    print("    F12 = Pause / Resume")
    print("\n  TOOLS:")
    print("    python bot.py --calibrate    (3-point inventory calibration)")
    print("    python bot.py --debug        (screenshot + grid overlay)")
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
            find_game_window()
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        listener.stop()

if __name__ == "__main__":
    main()
