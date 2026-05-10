"""
One Tick Karambwan — Rogues' Den Cooker
Rapidly clicks between raw karambwan (inventory) and the fire.
1-tick cooking method for max Cooking XP/hr.

Item: Raw Karambwan (3142)
Fire: Rogues' Den eternal fire (Object 43475)

Controls:
  F9  = Set KARAMBWAN spot (hover over raw karambwan in inventory)
  F10 = Set FIRE spot (hover over the fire in game)
  F11 = START / STOP cooking
  F12 = QUIT

Usage: C:\Python314\python.exe one_tick_karambwan.py
Speed: C:\Python314\python.exe one_tick_karambwan.py --delay 0.05
"""

import time
import sys
import threading
import argparse
import ctypes
import random

# ── Args ──
parser = argparse.ArgumentParser(description="One Tick Karambwan Cooker")
parser.add_argument("--delay", type=float, default=0.15,
                    help="Seconds between each click (default 0.15 = ~150ms)")
args = parser.parse_args()

DELAY = args.delay

# ── Win32 API ──
SendInput = ctypes.windll.user32.SendInput
GetCursorPos = ctypes.windll.user32.GetCursorPos
SetCursorPos = ctypes.windll.user32.SetCursorPos
GetAsyncKeyState = ctypes.windll.user32.GetAsyncKeyState

VK_F9  = 0x78
VK_F10 = 0x79
VK_F11 = 0x7A
VK_F12 = 0x7B

MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP   = 0x0004

class MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", ctypes.c_long),
        ("dy", ctypes.c_long),
        ("mouseData", ctypes.c_ulong),
        ("dwFlags", ctypes.c_ulong),
        ("time", ctypes.c_ulong),
        ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))
    ]

class INPUT(ctypes.Structure):
    class _INPUT(ctypes.Union):
        _fields_ = [("mi", MOUSEINPUT)]
    _fields_ = [
        ("type", ctypes.c_ulong),
        ("ii", _INPUT)
    ]

class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

def click_at(x, y):
    """Move cursor and click with tiny human-like jitter."""
    # Add +-2px jitter so clicks aren't pixel-perfect every time
    jx = x + random.randint(-2, 2)
    jy = y + random.randint(-2, 2)
    SetCursorPos(jx, jy)
    time.sleep(0.003 + random.random() * 0.004)  # 3-7ms settle
    inp_down = INPUT()
    inp_down.type = 0
    inp_down.ii.mi.dwFlags = MOUSEEVENTF_LEFTDOWN
    inp_down.ii.mi.dwExtraInfo = ctypes.pointer(ctypes.c_ulong(0))
    SendInput(1, ctypes.byref(inp_down), ctypes.sizeof(inp_down))
    time.sleep(0.003 + random.random() * 0.004)
    inp_up = INPUT()
    inp_up.type = 0
    inp_up.ii.mi.dwFlags = MOUSEEVENTF_LEFTUP
    inp_up.ii.mi.dwExtraInfo = ctypes.pointer(ctypes.c_ulong(0))
    SendInput(1, ctypes.byref(inp_up), ctypes.sizeof(inp_up))

def get_mouse_pos():
    pt = POINT()
    GetCursorPos(ctypes.byref(pt))
    return (pt.x, pt.y)

def key_pressed(vk):
    state = GetAsyncKeyState(vk)
    return (state & 0x0001) != 0

# ── State ──
karambwan_spot = None
fire_spot = None
running = False
alive = True
cook_count = 0

def clicker_loop():
    global running, cook_count
    while alive:
        if running and karambwan_spot and fire_spot:
            # Click karambwan in inventory (selects "Use")
            click_at(karambwan_spot[0], karambwan_spot[1])
            time.sleep(DELAY + random.random() * 0.02)
            if not running:
                continue
            # Click fire (uses karambwan on fire)
            click_at(fire_spot[0], fire_spot[1])
            time.sleep(DELAY + random.random() * 0.02)
            cook_count += 1
            if cook_count % 28 == 0:
                print(f"    Cooked ~{cook_count} karambwans")
        else:
            time.sleep(0.01)

t = threading.Thread(target=clicker_loop, daemon=True)
t.start()

print()
print("=" * 55)
print("  ONE TICK KARAMBWAN — Rogues' Den Cooker")
print("=" * 55)
print(f"  Click delay: {DELAY}s (~{int(1/DELAY/2)} cooks/sec)")
print()
print("  SETUP:")
print("  1. Stand at Rogues' Den fire with bank open nearby")
print("  2. Fill inventory with raw karambwans")
print("  3. Close bank")
print()
print("  F9  = Mark KARAMBWAN (hover over one, press F9)")
print("  F10 = Mark FIRE (hover over fire, press F10)")
print("  F11 = START / STOP cooking")
print("  F12 = QUIT")
print("=" * 55)
print()

try:
    while alive:
        time.sleep(0.01)

        if key_pressed(VK_F9):
            karambwan_spot = get_mouse_pos()
            print(f"  [KARAMBWAN] Spot set: ({karambwan_spot[0]}, {karambwan_spot[1]})")

        if key_pressed(VK_F10):
            fire_spot = get_mouse_pos()
            print(f"  [FIRE] Spot set: ({fire_spot[0]}, {fire_spot[1]})")

        if key_pressed(VK_F11):
            if not karambwan_spot or not fire_spot:
                print("  !! Set both spots first (F9 = karambwan, F10 = fire)")
            else:
                running = not running
                if running:
                    print(f"  >>> COOKING — {DELAY}s delay — F11 to stop")
                else:
                    print(f"  ||| PAUSED — {cook_count} cooked so far")

        if key_pressed(VK_F12):
            print(f"\n  [QUIT] Total cooked: ~{cook_count}. Bye!")
            alive = False
            break

except KeyboardInterrupt:
    alive = False
    print(f"\n  Stopped. Total cooked: ~{cook_count}")
