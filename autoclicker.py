"""
2-Spot Auto Clicker
Clicks between two spots as fast as you want.

Controls:
  F9  = Set Spot 1 (where your mouse is now)
  F10 = Set Spot 2 (where your mouse is now)
  F11 = START / STOP clicking
  F12 = QUIT

Usage: C:\Python314\python.exe autoclicker.py
Optional: C:\Python314\python.exe autoclicker.py --delay 0.05
  (delay in seconds between each click, default 0.08)
"""

import time
import sys
import threading
import argparse
import ctypes

# ── Args ──
parser = argparse.ArgumentParser(description="2-Spot Auto Clicker")
parser.add_argument("--delay", type=float, default=0.08, help="Seconds between clicks (default 0.08)")
args = parser.parse_args()

DELAY = args.delay

# ── Win32 API ──
SendInput = ctypes.windll.user32.SendInput
GetCursorPos = ctypes.windll.user32.GetCursorPos
SetCursorPos = ctypes.windll.user32.SetCursorPos
GetAsyncKeyState = ctypes.windll.user32.GetAsyncKeyState

# Key codes
VK_F9  = 0x78
VK_F10 = 0x79
VK_F11 = 0x7A
VK_F12 = 0x7B

# Input structs
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
    """Move cursor and click instantly."""
    SetCursorPos(x, y)
    time.sleep(0.005)
    # Mouse down
    inp_down = INPUT()
    inp_down.type = 0  # INPUT_MOUSE
    inp_down.ii.mi.dwFlags = MOUSEEVENTF_LEFTDOWN
    inp_down.ii.mi.dwExtraInfo = ctypes.pointer(ctypes.c_ulong(0))
    SendInput(1, ctypes.byref(inp_down), ctypes.sizeof(inp_down))
    time.sleep(0.005)
    # Mouse up
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
    """Check if key was JUST pressed (transition)."""
    state = GetAsyncKeyState(vk)
    return (state & 0x0001) != 0

# ── State ──
spot1 = None
spot2 = None
running = False
alive = True

def clicker_loop():
    global running
    while alive:
        if running and spot1 and spot2:
            click_at(spot1[0], spot1[1])
            time.sleep(DELAY)
            if not running:
                continue
            click_at(spot2[0], spot2[1])
            time.sleep(DELAY)
        else:
            time.sleep(0.01)

# Start clicker thread
t = threading.Thread(target=clicker_loop, daemon=True)
t.start()

print("=" * 50)
print("  2-SPOT AUTO CLICKER")
print("=" * 50)
print(f"  Delay: {DELAY}s between clicks")
print()
print("  F9  = Set Spot 1 (current mouse pos)")
print("  F10 = Set Spot 2 (current mouse pos)")
print("  F11 = START / STOP")
print("  F12 = QUIT")
print("=" * 50)
print()

try:
    while alive:
        time.sleep(0.01)

        if key_pressed(VK_F9):
            spot1 = get_mouse_pos()
            print(f"  [SPOT 1] Set to ({spot1[0]}, {spot1[1]})")

        if key_pressed(VK_F10):
            spot2 = get_mouse_pos()
            print(f"  [SPOT 2] Set to ({spot2[0]}, {spot2[1]})")

        if key_pressed(VK_F11):
            if not spot1 or not spot2:
                print("  !! Set both spots first (F9 and F10)")
            else:
                running = not running
                if running:
                    print(f"  >>> CLICKING — {DELAY}s delay — F11 to stop")
                else:
                    print("  ||| PAUSED")

        if key_pressed(VK_F12):
            print("  [QUIT] Bye!")
            alive = False
            break

except KeyboardInterrupt:
    alive = False
    print("\n  Stopped.")
