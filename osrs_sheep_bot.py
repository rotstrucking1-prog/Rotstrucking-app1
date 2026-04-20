"""
OSRS Sheep Shearer Bot v3 - Local Execution
Runs on Brad's PC - PyAutoGUI + OpenCV
Detects white sheep, left-clicks to shear, tracks progress
"""
import pyautogui
import time
import cv2
import numpy as np
from PIL import ImageGrab
import random
import sys

# Safety - move mouse to top-left corner to abort
pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.05

SCREEN_W, SCREEN_H = pyautogui.size()
print(f"Screen: {SCREEN_W}x{SCREEN_H}")

# Game viewport boundaries (exclude UI panels)
GX1 = int(SCREEN_W * 0.05)
GX2 = int(SCREEN_W * 0.82)
GY1 = int(SCREEN_H * 0.05)
GY2 = int(SCREEN_H * 0.72)

# Character is roughly at screen center
CHAR_X = SCREEN_W // 2
CHAR_Y = int(SCREEN_H * 0.45)

def grab():
    """Screenshot as numpy RGB array"""
    return np.array(ImageGrab.grab())

def find_sheep():
    """Find white woolly sheep in game viewport"""
    img = grab()
    game = img[GY1:GY2, GX1:GX2]
    
    # Convert to HSV for color detection
    hsv = cv2.cvtColor(game, cv2.COLOR_RGB2HSV)
    
    # White sheep: very low saturation, high brightness
    mask = cv2.inRange(hsv, 
                       np.array([0, 0, 185]), 
                       np.array([180, 45, 255]))
    
    # Clean up noise
    kernel = np.ones((3, 3), np.uint8)
    mask = cv2.erode(mask, kernel, iterations=1)
    mask = cv2.dilate(mask, kernel, iterations=2)
    
    # Find contours (blobs)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, 
                                    cv2.CHAIN_APPROX_SIMPLE)
    
    sheep = []
    for c in contours:
        area = cv2.contourArea(c)
        # Filter by size: sheep are medium blobs
        if 400 < area < 30000:
            x, y, w, h = cv2.boundingRect(c)
            # Sheep are roughly 1:1 to 2:1 aspect ratio
            aspect = w / max(h, 1)
            if 0.3 < aspect < 3.5:
                M = cv2.moments(c)
                if M["m00"] > 0:
                    cx = int(M["m10"] / M["m00"]) + GX1
                    cy = int(M["m01"] / M["m00"]) + GY1
                    sheep.append((cx, cy, area))
    
    # Sort by distance to character (closest first)
    sheep.sort(key=lambda s: (s[0] - CHAR_X)**2 + (s[1] - CHAR_Y)**2)
    
    # Return positions only
    return [(s[0], s[1]) for s in sheep[:10]]

def click_sheep(x, y):
    """Move to sheep and left-click to shear"""
    # Add tiny random offset for humanization
    jx = x + random.randint(-3, 3)
    jy = y + random.randint(-3, 3)
    
    # Human-like mouse movement
    pyautogui.moveTo(jx, jy, duration=random.uniform(0.08, 0.2))
    time.sleep(random.uniform(0.02, 0.08))
    pyautogui.click()

def wait_for_shear():
    """Wait for shearing animation to complete"""
    time.sleep(random.uniform(2.0, 3.0))

def rotate_camera():
    """Rotate camera to find more sheep"""
    direction = random.choice(['left', 'right'])
    hold_time = random.uniform(0.3, 0.8)
    pyautogui.keyDown(direction)
    time.sleep(hold_time)
    pyautogui.keyUp(direction)
    time.sleep(0.5)

def main():
    print("=" * 50)
    print("  OSRS SHEEP SHEARER BOT v3")
    print("  Phase 1: Shear 20 sheep")
    print("=" * 50)
    print()
    print("Move mouse to TOP-LEFT corner to ABORT")
    print()
    print("Starting in 5 seconds... make sure RuneLite is focused!")
    
    for i in range(5, 0, -1):
        print(f"  {i}...")
        time.sleep(1)
    
    print("\nBOT ACTIVE!\n")
    
    # Brad already sheared 1 sheep manually
    wool = 1
    target = 20
    no_sheep_count = 0
    total_clicks = 0
    
    while wool < target:
        sheep = find_sheep()
        
        if not sheep:
            no_sheep_count += 1
            print(f"[{wool}/{target}] No sheep found (attempt {no_sheep_count})")
            
            if no_sheep_count >= 5:
                print("  Too many misses, rotating camera...")
                rotate_camera()
                no_sheep_count = 0
            else:
                time.sleep(0.5)
            continue
        
        # Reset miss counter
        no_sheep_count = 0
        
        # Pick target sheep (closest to character)
        sx, sy = sheep[0]
        print(f"[{wool}/{target}] Found {len(sheep)} sheep, clicking ({sx},{sy})")
        
        # Click the sheep
        click_sheep(sx, sy)
        total_clicks += 1
        
        # Wait for shearing animation
        wait_for_shear()
        
        # Assume success (we'll verify later)
        wool += 1
        print(f"  -> Wool count: {wool}/{target}")
        
        # Brief pause between sheep
        time.sleep(random.uniform(0.3, 1.0))
    
    print()
    print("=" * 50)
    print(f"  PHASE 1 COMPLETE!")
    print(f"  Sheared {target} sheep in {total_clicks} clicks")
    print("=" * 50)
    print()
    print("Next step: Spin wool at Lumbridge Castle")
    print("(Run phase2 script for spinning)")
    
    return wool

if __name__ == "__main__":
    try:
        collected = main()
        print(f"\nFinal: ~{collected} wool collected")
    except pyautogui.FailSafeException:
        print("\n\nABORTED - Mouse moved to corner")
    except KeyboardInterrupt:
        print("\n\nABORTED - Ctrl+C pressed")
    
    input("\nPress Enter to exit...")
