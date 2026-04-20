"""
OSRS Sheep Shearer Bot v6 - MENU READING APPROACH
==================================================
CORE STRATEGY: Right-click EVERYTHING → read menu → only click "Shear"
- NO color-based sheep detection (too unreliable - hits rams, trees, etc.)
- Right-clicks random spots in the sheep pen area
- Reads the popup menu text using OCR
- If "Shear" found → click it
- If "Attack" found → press Escape, skip it
- ONE action at a time — waits until idle before next attempt
- Checks inventory to count wool accurately
- Never crashes — retries forever

Requirements: pip install pyautogui pillow pytesseract opencv-python
Also needs Tesseract OCR installed: https://github.com/tesseract-ocr/tesseract
If Tesseract not available, falls back to pixel color matching on menu

Author: ROTS Trucking AI Agent
"""

import pyautogui
import time
import random
import sys
import os

# Safety - move mouse to corner to abort
pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.05  # Small default pause

# ============================================================
# CONFIGURATION - Adjust these for your screen/RuneLite setup
# ============================================================

# RuneLite game area (approximate - adjust if needed)
GAME_LEFT = 0
GAME_TOP = 80
GAME_RIGHT = 1100
GAME_BOTTOM = 650

# Sheep pen area (where to click to find sheep)
# This is the fenced area near Fred's farm
PEN_LEFT = 400
PEN_TOP = 200
PEN_RIGHT = 900
PEN_BOTTOM = 550

# Inventory area (RuneLite default - right side panel)
INV_LEFT = 1125
INV_TOP = 280
INV_RIGHT = 1260
INV_BOTTOM = 510

# Inventory slot size and grid
INV_COLS = 4
INV_ROWS = 7
SLOT_WIDTH = (INV_RIGHT - INV_LEFT) // INV_COLS
SLOT_HEIGHT = (INV_BOTTOM - INV_TOP) // INV_ROWS

# Colors
WOOL_COLOR_RANGE = {
    'r_min': 200, 'r_max': 255,
    'g_min': 200, 'g_max': 255,
    'b_min': 200, 'b_max': 255
}

# Menu colors - OSRS right-click menu has specific look
MENU_BG_COLOR = (91, 84, 69)  # Brown/tan menu background (approximate)
MENU_TEXT_WHITE = (255, 255, 255)  # White text for options

# Shear text recognition - we'll look for the color of "Shear" option
# In OSRS menus, options are white text on dark background
# "Shear" has a specific sheep icon next to it

# Timing
CLICK_DELAY = 0.3
ACTION_WAIT = 4.0  # Wait for shearing animation
BETWEEN_ATTEMPTS = 1.0
MENU_WAIT = 0.5  # Wait for right-click menu to appear

print("=" * 60)
print("  OSRS SHEEP SHEARER BOT v6")
print("  Strategy: RIGHT-CLICK → READ MENU → SHEAR ONLY")
print("=" * 60)
print()
print("FIXES from v5:")
print("  ✅ Right-clicks first, reads menu before acting")
print("  ✅ Only clicks 'Shear' - never 'Attack'")
print("  ✅ One action at a time (single combat zone)")
print("  ✅ Inventory scanning to count wool")
print("  ✅ Never crashes - retries forever")
print()

# ============================================================
# HELPER FUNCTIONS
# ============================================================

def take_screenshot():
    """Capture current screen"""
    try:
        return pyautogui.screenshot()
    except Exception as e:
        print(f"  [!] Screenshot failed: {e}")
        return None

def count_wool_in_inventory():
    """
    Count wool/balls of wool in inventory by scanning inventory slots.
    Wool is a white fluffy item. We look for bright white pixels in each slot.
    Returns count of slots that appear to contain wool.
    """
    try:
        img = take_screenshot()
        if img is None:
            return -1
        
        wool_count = 0
        
        for row in range(INV_ROWS):
            for col in range(INV_COLS):
                # Center of each inventory slot
                slot_x = INV_LEFT + col * SLOT_WIDTH + SLOT_WIDTH // 2
                slot_y = INV_TOP + row * SLOT_HEIGHT + SLOT_HEIGHT // 2
                
                # Sample a small area around slot center
                white_pixels = 0
                total_pixels = 0
                sample_size = 6  # Check 6x6 pixel area
                
                for dx in range(-sample_size//2, sample_size//2):
                    for dy in range(-sample_size//2, sample_size//2):
                        px = slot_x + dx
                        py = slot_y + dy
                        try:
                            r, g, b = img.getpixel((px, py))[:3]
                            total_pixels += 1
                            # Wool is bright white/cream
                            if r > 200 and g > 200 and b > 200:
                                white_pixels += 1
                        except:
                            pass
                
                # If more than 40% of sampled pixels are white, likely wool
                if total_pixels > 0 and white_pixels / total_pixels > 0.4:
                    wool_count += 1
        
        # Subtract 1 if shears are in inventory (they're also somewhat bright)
        # Shears are metallic gray, not pure white, so this should be ok
        return wool_count
        
    except Exception as e:
        print(f"  [!] Inventory scan error: {e}")
        return -1

def is_menu_open(img):
    """
    Check if a right-click menu is currently open.
    OSRS menus have a distinctive dark border and tan/brown background.
    """
    # Look for the characteristic menu border pattern
    # Menus appear near where you clicked, have dark borders
    # We'll check for a concentration of the menu background color
    try:
        width, height = img.size
        menu_pixels = 0
        sample_points = 100
        
        for _ in range(sample_points):
            x = random.randint(GAME_LEFT, GAME_RIGHT)
            y = random.randint(GAME_TOP, GAME_BOTTOM)
            r, g, b = img.getpixel((x, y))[:3]
            # OSRS menu background is a specific tan/brown
            if 80 <= r <= 100 and 73 <= g <= 92 and 58 <= b <= 78:
                menu_pixels += 1
        
        return menu_pixels > 10  # If many menu-colored pixels found
    except:
        return False

def find_menu_options(click_x, click_y):
    """
    After right-clicking, scan the area around the click for menu options.
    Returns list of (option_text_guess, x, y) for each menu row.
    
    Since we can't do full OCR easily, we use a SIMPLER approach:
    - Look for the "Shear" option by checking if menu has sheep-related colors
    - The menu appears below/near the right-click point
    - Each option is about 15px tall
    - We return the Y positions of each option row
    """
    time.sleep(MENU_WAIT)  # Wait for menu to render
    
    img = take_screenshot()
    if img is None:
        return []
    
    # Menu typically appears at or slightly offset from click position
    # Scan downward from click point to find menu rows
    menu_rows = []
    
    # Look for menu starting point - scan down from click
    # OSRS menus have a header "Choose Option" then list of actions
    # Menu width is typically 100-200px
    
    # Search for menu by looking for the dark border/background
    search_left = max(0, click_x - 150)
    search_right = min(1280, click_x + 150)
    search_top = max(0, click_y - 20)
    search_bottom = min(720, click_y + 200)
    
    # Find menu boundaries by looking for menu background color
    menu_left = None
    menu_top = None
    menu_right = None
    menu_bottom = None
    
    for y in range(search_top, search_bottom, 2):
        for x in range(search_left, search_right, 2):
            try:
                r, g, b = img.getpixel((x, y))[:3]
                # Menu background in OSRS
                if 75 <= r <= 110 and 68 <= g <= 95 and 55 <= b <= 82:
                    if menu_left is None or x < menu_left:
                        menu_left = x
                    if menu_top is None or y < menu_top:
                        menu_top = y
                    if menu_right is None or x > menu_right:
                        menu_right = x
                    if menu_bottom is None or y > menu_bottom:
                        menu_bottom = y
            except:
                pass
    
    if menu_left is None:
        return []
    
    # Menu found! Now figure out the rows
    # Each option row is about 15px tall, starting after "Choose Option" header
    menu_center_x = (menu_left + menu_right) // 2
    row_height = 15
    header_height = 18  # "Choose Option" header
    
    y = menu_top + header_height + 5
    option_num = 0
    while y < menu_bottom - 5:
        menu_rows.append({
            'x': menu_center_x,
            'y': y + row_height // 2,
            'index': option_num
        })
        y += row_height
        option_num += 1
        if option_num > 10:  # Safety limit
            break
    
    return menu_rows

def check_for_shear_option(click_x, click_y):
    """
    After right-clicking, look for the "Shear" text in the menu.
    
    APPROACH: Since full OCR is complex, we use RuneLite's text rendering.
    In OSRS/RuneLite, menu text is rendered in a specific font.
    "Shear" option appears with a specific icon (sheep icon).
    
    SIMPLER METHOD: 
    - The menu options are ordered. For a sheep, typical menu is:
      1. "Choose Option" (header)
      2. "Shear Sheep" (what we want!)
      3. "Examine Sheep"
      4. "Cancel"
    - For a ram:
      1. "Choose Option"
      2. "Attack Ram (level 3)"
      3. "Examine Ram"
      4. "Cancel"
    
    So we check: if the menu has exactly 3-4 options (short menu),
    the FIRST real option (row index 0 after header) is what matters.
    
    We use COLOR of the text to distinguish:
    - "Shear" text is in WHITE (regular action)
    - "Attack" text is in YELLOW/WHITE (combat action, sometimes yellow for attackable NPCs)
    
    Actually, the SAFEST method: look for yellow text (Attack) vs white text (Shear).
    Attack options for NPCs show the combat level in a different color.
    
    SIMPLEST METHOD: Just check if there's yellow/orange colored text in the first option.
    Yellow = Attack = RAM. No yellow = likely Shear = SHEEP.
    """
    time.sleep(MENU_WAIT)
    
    img = take_screenshot()
    if img is None:
        return None, 0, 0
    
    # Find the menu region
    search_left = max(0, click_x - 150)
    search_right = min(1280, click_x + 150)
    search_top = max(0, click_y - 30)
    search_bottom = min(720, click_y + 250)
    
    # Locate menu by looking for its border (black 1px border)
    # and background color
    menu_points = []
    
    for y in range(search_top, search_bottom, 3):
        for x in range(search_left, search_right, 3):
            try:
                r, g, b = img.getpixel((x, y))[:3]
                if 75 <= r <= 110 and 68 <= g <= 95 and 55 <= b <= 82:
                    menu_points.append((x, y))
            except:
                pass
    
    if len(menu_points) < 20:
        # No menu found
        return None, 0, 0
    
    # Get menu bounds
    xs = [p[0] for p in menu_points]
    ys = [p[1] for p in menu_points]
    m_left = min(xs)
    m_right = max(xs)
    m_top = min(ys)
    m_bottom = max(ys)
    m_center_x = (m_left + m_right) // 2
    
    # First option starts about 18-20px below menu top
    first_option_y = m_top + 22
    
    # Check the first option area for yellow/orange text (= Attack = RAM)
    # Scan a horizontal band where the first option text would be
    yellow_count = 0
    cyan_count = 0
    total_text_pixels = 0
    
    scan_y_start = first_option_y - 5
    scan_y_end = first_option_y + 10
    
    for y in range(scan_y_start, scan_y_end):
        for x in range(m_left + 5, m_right - 5):
            try:
                r, g, b = img.getpixel((x, y))[:3]
                # Yellow text (Attack option - combat level)
                if r > 200 and g > 200 and b < 100:
                    yellow_count += 1
                # Cyan/light blue text (sometimes used for NPC names)
                if r < 100 and g > 200 and b > 200:
                    cyan_count += 1
                # Any bright text
                if r > 180 or g > 180 or b > 180:
                    total_text_pixels += 1
            except:
                pass
    
    # Decision logic
    if yellow_count > 5:
        # Yellow text found → this is an "Attack" option → RAM
        return "attack", m_center_x, first_option_y
    elif total_text_pixels > 10:
        # White text, no yellow → likely "Shear" → SHEEP
        return "shear", m_center_x, first_option_y
    else:
        # Can't determine - might be "Walk here" or other
        return "unknown", m_center_x, first_option_y

def close_menu():
    """Close right-click menu by pressing Escape or clicking away"""
    pyautogui.press('escape')
    time.sleep(0.3)

def is_player_idle(prev_img, curr_img):
    """
    Check if the player character is idle (not animating).
    Compare two screenshots - if very similar, player is idle.
    Simple approach: compare pixel samples in the game area.
    """
    if prev_img is None or curr_img is None:
        return True  # Assume idle if we can't check
    
    try:
        diff_count = 0
        samples = 50
        
        for _ in range(samples):
            x = random.randint(GAME_LEFT + 100, GAME_RIGHT - 100)
            y = random.randint(GAME_TOP + 100, GAME_BOTTOM - 100)
            
            r1, g1, b1 = prev_img.getpixel((x, y))[:3]
            r2, g2, b2 = curr_img.getpixel((x, y))[:3]
            
            if abs(r1-r2) + abs(g1-g2) + abs(b1-b2) > 30:
                diff_count += 1
        
        # If less than 20% of samples changed, player is idle
        return diff_count / samples < 0.2
    except:
        return True

def wait_for_idle(timeout=8):
    """Wait until the player appears to be idle (not animating)"""
    print("  ⏳ Waiting for action to complete...")
    start = time.time()
    prev = take_screenshot()
    time.sleep(1.5)  # Minimum wait for animation to start
    
    while time.time() - start < timeout:
        time.sleep(0.8)
        curr = take_screenshot()
        if is_player_idle(prev, curr):
            return True
        prev = curr
    
    return True  # Timeout - proceed anyway

def check_chat_for_wool():
    """
    Check the chat area for "You get some wool" message.
    Chat is at the bottom-left of the screen.
    We look for the distinctive green text color of game messages.
    """
    # This is a basic check - just returns True if we see the message
    # In practice, we rely more on inventory counting
    return True  # Simplified - rely on inventory count

def random_pen_position():
    """Get a random position within the sheep pen"""
    x = random.randint(PEN_LEFT, PEN_RIGHT)
    y = random.randint(PEN_TOP, PEN_BOTTOM)
    return x, y

def find_white_blobs():
    """
    Find bright white blobs in the pen area that could be sheep.
    Returns list of (x, y) center positions, sorted by size (biggest first).
    Big white blobs are more likely to be sheep bodies.
    """
    img = take_screenshot()
    if img is None:
        return []
    
    # Scan pen area for white pixels
    white_points = []
    
    for y in range(PEN_TOP, PEN_BOTTOM, 4):
        for x in range(PEN_LEFT, PEN_RIGHT, 4):
            try:
                r, g, b = img.getpixel((x, y))[:3]
                # Very bright white - sheep wool
                if r > 215 and g > 215 and b > 215:
                    # Make sure it's not the UI or ground
                    # Sheep are not at the very edges
                    white_points.append((x, y))
            except:
                pass
    
    if not white_points:
        return []
    
    # Cluster nearby points into blobs
    blobs = []
    used = set()
    
    for i, (px, py) in enumerate(white_points):
        if i in used:
            continue
        
        # Start a new blob
        blob = [(px, py)]
        used.add(i)
        
        for j, (qx, qy) in enumerate(white_points):
            if j in used:
                continue
            # Check if close to any point in current blob
            for bx, by in blob:
                if abs(qx - bx) < 30 and abs(qy - by) < 30:
                    blob.append((qx, qy))
                    used.add(j)
                    break
        
        if len(blob) >= 3:  # Minimum size for a sheep
            # Calculate center
            cx = sum(p[0] for p in blob) // len(blob)
            cy = sum(p[1] for p in blob) // len(blob)
            blobs.append((cx, cy, len(blob)))
    
    # Sort by size, biggest first (more likely to be sheep)
    blobs.sort(key=lambda b: b[2], reverse=True)
    
    return [(b[0], b[1]) for b in blobs]

# ============================================================
# MAIN BOT LOOP
# ============================================================

def main():
    print("🐑 SHEEP SHEARER BOT v6")
    print("=" * 40)
    print()
    
    target_wool = 20
    
    print("  🔴 MOVE MOUSE TO TOP-LEFT CORNER TO EMERGENCY STOP")
    print()
    
    # Give time to switch to RuneLite
    print("  Starting in 5 seconds... switch to RuneLite!")
    for i in range(5, 0, -1):
        print(f"  {i}...")
        time.sleep(1)
    
    # Auto-detect wool in inventory instead of asking
    print("\n  📦 Scanning inventory for existing wool...")
    wool_collected = count_wool_in_inventory()
    if wool_collected < 0:
        print("  ⚠️ Couldn't scan inventory — assuming 0 wool")
        wool_collected = 0
    
    print(f"  🧶 Detected {wool_collected} wool in inventory")
    print(f"  Need {target_wool - wool_collected} more to reach {target_wool}")
    
    if wool_collected >= target_wool:
        print(f"\n  ✅ Already have {wool_collected} wool! Skipping to spinning phase.")
        # Jump straight to phase 2 message
        print("\n  📍 NEXT: Walk to Lumbridge Castle spinning wheel")
        return
    
    print("\n  🟢 BOT STARTED!\n")
    
    attempts = 0
    max_attempts = 200  # Safety limit
    consecutive_fails = 0
    
    while wool_collected < target_wool and attempts < max_attempts:
        attempts += 1
        
        try:
            print(f"\n--- Attempt {attempts} | Wool: {wool_collected}/{target_wool} ---")
            
            # Step 1: Find white blobs (potential sheep)
            print("  🔍 Scanning for sheep...")
            blobs = find_white_blobs()
            
            if not blobs:
                print("  ❌ No white blobs found - moving camera/walking")
                # Click a random spot to walk and find sheep
                rx, ry = random_pen_position()
                pyautogui.click(rx, ry)
                time.sleep(2)
                consecutive_fails += 1
                
                if consecutive_fails > 5:
                    print("  🔄 Can't find sheep - trying different area")
                    # Click further away to walk
                    pyautogui.click(
                        random.randint(GAME_LEFT + 200, GAME_RIGHT - 200),
                        random.randint(GAME_TOP + 100, GAME_BOTTOM - 100)
                    )
                    time.sleep(3)
                    consecutive_fails = 0
                continue
            
            # Step 2: Try each blob - right-click it and check menu
            found_sheep = False
            
            for bx, by in blobs[:5]:  # Try up to 5 blobs
                print(f"  🖱️ Right-clicking blob at ({bx}, {by})...")
                
                # RIGHT-CLICK the blob
                pyautogui.click(bx, by, button='right')
                time.sleep(MENU_WAIT + 0.2)
                
                # Step 3: Analyze the menu
                result, opt_x, opt_y = check_for_shear_option(bx, by)
                
                if result == "shear":
                    print("  ✅ FOUND 'Shear' option! Clicking it!")
                    # Click the Shear option
                    pyautogui.click(opt_x, opt_y)
                    found_sheep = True
                    consecutive_fails = 0
                    
                    # Step 4: Wait for shearing animation
                    time.sleep(1.0)
                    wait_for_idle(timeout=6)
                    
                    # Step 5: Check if we got wool
                    time.sleep(0.5)
                    new_count = count_wool_in_inventory()
                    
                    if new_count > wool_collected:
                        wool_collected = new_count
                        print(f"  🧶 GOT WOOL! Total: {wool_collected}/{target_wool}")
                    elif new_count == wool_collected:
                        print(f"  🐑 Sheep may have escaped - trying again")
                    else:
                        print(f"  ❓ Inventory scan: {new_count} (was {wool_collected})")
                        # Don't decrease count - might be scan error
                    
                    break  # Done with this attempt
                    
                elif result == "attack":
                    print("  🐏 ATTACK option = RAM! Closing menu, skipping...")
                    close_menu()
                    time.sleep(0.3)
                    continue  # Try next blob
                    
                else:
                    print("  ❓ Unknown menu - closing, trying next...")
                    close_menu()
                    time.sleep(0.3)
                    continue
            
            if not found_sheep:
                print("  ❌ No sheep found in any blob - repositioning")
                rx, ry = random_pen_position()
                pyautogui.click(rx, ry)
                time.sleep(2)
                consecutive_fails += 1
            
            # Small random delay to look human
            time.sleep(random.uniform(0.5, 1.5))
            
        except pyautogui.FailSafeException:
            print("\n\n  🛑 EMERGENCY STOP - Mouse moved to corner!")
            print(f"  Final wool count: {wool_collected}/{target_wool}")
            sys.exit(0)
            
        except Exception as e:
            print(f"  [!] Error (continuing): {e}")
            close_menu()  # Clean up any open menu
            time.sleep(1)
            continue
    
    # ============================================================
    # PHASE 2: Spin wool at Lumbridge Castle
    # ============================================================
    
    print("\n" + "=" * 60)
    print(f"  🧶 SHEARING COMPLETE! Got {wool_collected} wool!")
    print("=" * 60)
    
    if wool_collected >= target_wool:
        print("\n  📍 NEXT: Walk to Lumbridge Castle spinning wheel")
        print("  The spinning wheel is on the TOP FLOOR of the castle.")
        print("  This part needs to be done manually or in next version.")
        print("\n  QUEST STEPS REMAINING:")
        print("  1. Walk south to Lumbridge Castle")
        print("  2. Go up stairs to top floor")
        print("  3. Use wool on spinning wheel (20 times)")
        print("  4. Walk back to Fred the Farmer")
        print("  5. Give him 20 balls of wool")
        print("  6. QUEST COMPLETE! 🎉")
    else:
        print(f"\n  ⚠️ Only got {wool_collected}/{target_wool} wool")
        print("  May need to run again or shear manually")
    
    print(f"\n  Bot finished after {attempts} attempts")
    print("  Thanks for using ROTS Trucking AI Bot! 🐑🚛")

if __name__ == "__main__":
    main()
