"""
Peterbilt 379 / Cummins N14 — Adapter Diagnostic Tool
Tests EVERY possible way to read J1708 data from the Vgate vLinker FS

The N14 Celect Plus ECM continuously broadcasts on J1708 at 9600 baud.
This script tries multiple modes to capture that data.
"""

import serial
import serial.tools.list_ports
import time
import sys
import os

# Colors for terminal output
class C:
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    CYAN = "\033[96m"
    BOLD = "\033[1m"
    END = "\033[0m"

def find_port():
    """Find the Vgate adapter COM port."""
    print(f"\n{C.CYAN}{'='*60}")
    print("  PETERBILT 379 / N14 ADAPTER DIAGNOSTIC")
    print(f"{'='*60}{C.END}\n")
    
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print(f"{C.RED}ERROR: No COM ports found! Is the adapter plugged in?{C.END}")
        return None
    
    print(f"{C.BOLD}Available COM ports:{C.END}")
    best_port = None
    for p in ports:
        marker = ""
        if any(x in (p.description or "").lower() for x in ["elm", "vgate", "vlinker", "ch340", "ftdi", "cp210", "usb-serial", "usb serial"]):
            marker = f" {C.GREEN}<-- LIKELY ADAPTER{C.END}"
            best_port = p.device
        print(f"  {C.BOLD}{p.device}{C.END}: {p.description} [{p.hwid}]{marker}")
    
    if not best_port and ports:
        best_port = ports[0].device
    
    return best_port

def test_elm327_info(ser):
    """Get adapter identification info."""
    print(f"\n{C.CYAN}--- TEST 1: Adapter Identification ---{C.END}")
    
    commands = [
        ("ATZ", "Reset adapter"),
        ("ATI", "Adapter ID"),
        ("AT@1", "Device description"),
        ("ATRV", "Read voltage"),
        ("ATDPN", "Current protocol number"),
    ]
    
    for cmd, desc in commands:
        ser.reset_input_buffer()
        ser.write((cmd + "\r").encode())
        time.sleep(1.5 if cmd == "ATZ" else 0.5)
        resp = ser.read(ser.in_waiting or 256).decode("ascii", errors="replace").strip()
        status = C.GREEN + "OK" + C.END if resp and "?" not in resp else C.YELLOW + "No response" + C.END
        print(f"  {cmd:8s} ({desc:25s}): {status} -> {repr(resp)}")

def test_protocol_scan(ser):
    """Try every ELM327 protocol to see which ones the adapter supports."""
    print(f"\n{C.CYAN}--- TEST 2: Protocol Scan ---{C.END}")
    
    protocols = {
        "0": "Auto-detect",
        "1": "SAE J1850 PWM (41.6k)",
        "2": "SAE J1850 VPW (10.4k)",
        "3": "ISO 9141-2 (5 baud init)",
        "4": "ISO 14230 KWP (5 baud)",
        "5": "ISO 14230 KWP (fast)",
        "6": "CAN 11-bit 500k",
        "7": "CAN 29-bit 500k",
        "8": "CAN 11-bit 250k",
        "9": "CAN 29-bit 250k",
        "A": "J1939 CAN 29-bit 250k",
        "B": "User CAN",
    }
    
    for num, name in protocols.items():
        ser.reset_input_buffer()
        ser.write(f"ATSP{num}\r".encode())
        time.sleep(0.5)
        resp = ser.read(ser.in_waiting or 128).decode("ascii", errors="replace").strip()
        
        if "OK" in resp:
            # Try reading data on this protocol
            ser.reset_input_buffer()
            ser.write(b"0100\r")  # Standard OBD-II supported PIDs query
            time.sleep(2.0)
            data_resp = ser.read(ser.in_waiting or 256).decode("ascii", errors="replace").strip()
            
            has_data = data_resp and "NO DATA" not in data_resp and "UNABLE" not in data_resp and "ERROR" not in data_resp and "?" not in data_resp
            
            if has_data:
                print(f"  Protocol {num} ({name}): {C.GREEN}SUPPORTED + DATA RECEIVED!{C.END}")
                print(f"    Response: {repr(data_resp[:100])}")
            else:
                print(f"  Protocol {num} ({name}): {C.YELLOW}Supported (no data){C.END}")
        else:
            print(f"  Protocol {num} ({name}): Not supported")

def test_monitor_all(ser):
    """Use ATMA (Monitor All) to listen for any bus traffic."""
    print(f"\n{C.CYAN}--- TEST 3: Monitor All Bus Traffic (5 seconds) ---{C.END}")
    
    # Reset and set to auto protocol
    ser.write(b"ATZ\r")
    time.sleep(1.5)
    ser.read(ser.in_waiting or 256)
    
    ser.write(b"ATE0\r")
    time.sleep(0.3)
    ser.read(ser.in_waiting or 128)
    
    ser.write(b"ATSP0\r")
    time.sleep(0.3)
    ser.read(ser.in_waiting or 128)
    
    # Monitor All
    ser.reset_input_buffer()
    ser.write(b"ATMA\r")
    
    print("  Listening for bus traffic...")
    start = time.time()
    all_data = bytearray()
    
    while time.time() - start < 5.0:
        if ser.in_waiting > 0:
            chunk = ser.read(ser.in_waiting)
            all_data.extend(chunk)
        time.sleep(0.1)
    
    # Send any key to stop ATMA
    ser.write(b"\r")
    time.sleep(0.3)
    ser.read(ser.in_waiting or 256)
    
    if all_data:
        text = all_data.decode("ascii", errors="replace")
        if "NO DATA" in text or "STOPPED" in text:
            print(f"  {C.YELLOW}No bus traffic detected{C.END}")
        else:
            print(f"  {C.GREEN}RECEIVED {len(all_data)} bytes of bus data!{C.END}")
            # Show first 200 chars
            print(f"  Data: {repr(text[:200])}")
    else:
        print(f"  {C.YELLOW}No data received during monitor{C.END}")

def test_j1708_direct(ser):
    """Try J1587/J1708 specific requests."""
    print(f"\n{C.CYAN}--- TEST 4: J1708 Direct PID Requests ---{C.END}")
    
    # Some adapters support J1708 through header manipulation
    # Try setting headers for J1708 (MID-based addressing)
    
    ser.write(b"ATZ\r")
    time.sleep(1.5)
    ser.read(ser.in_waiting or 256)
    
    # Try J1850 VPW (closest baud rate to J1708's 9600)
    ser.write(b"ATSP2\r")
    time.sleep(0.5)
    ser.read(ser.in_waiting or 128)
    
    # Try requesting RPM (J1587 PID 190 from MID 128)
    # In J1587: request PID = 0, target MID = 128, requested PID = 190
    ser.reset_input_buffer()
    
    # Try as hex: AC 00 80 BE (diagnostic tool MID=172, PID=0 request, engine MID=128, PID=190)
    ser.write(b"AC0080BE\r")
    time.sleep(2.0)
    resp = ser.read(ser.in_waiting or 256).decode("ascii", errors="replace").strip()
    print(f"  J1708 RPM request (via J1850): {repr(resp)}")
    
    # Try standard OBD-II RPM request too
    ser.reset_input_buffer()
    ser.write(b"010C\r")  # OBD-II RPM
    time.sleep(2.0)
    resp = ser.read(ser.in_waiting or 256).decode("ascii", errors="replace").strip()
    has_data = resp and "NO DATA" not in resp and "ERROR" not in resp and "?" not in resp and "UNABLE" not in resp
    if has_data:
        print(f"  OBD-II RPM (010C): {C.GREEN}RESPONSE: {repr(resp)}{C.END}")
    else:
        print(f"  OBD-II RPM (010C): {repr(resp)}")
    
    # Try other common OBD-II PIDs
    pids = [
        ("0105", "Coolant Temp"),
        ("010B", "Intake MAP"),
        ("010D", "Vehicle Speed"),
        ("0111", "Throttle Position"),
        ("0142", "Battery Voltage"),
    ]
    
    for pid, name in pids:
        ser.reset_input_buffer()
        ser.write((pid + "\r").encode())
        time.sleep(1.5)
        resp = ser.read(ser.in_waiting or 256).decode("ascii", errors="replace").strip()
        has_data = resp and "NO DATA" not in resp and "ERROR" not in resp and "?" not in resp and "UNABLE" not in resp
        if has_data:
            print(f"  {pid} ({name}): {C.GREEN}RESPONSE: {repr(resp)}{C.END}")
        else:
            print(f"  {pid} ({name}): {repr(resp)}")

def test_raw_9600(port):
    """Open port at raw 9600 baud WITHOUT any ELM327 commands and just listen."""
    print(f"\n{C.CYAN}--- TEST 5: Raw Serial 9600 Baud Listen (5 seconds) ---{C.END}")
    print("  Bypassing ELM327 — listening for raw J1708 bus data...")
    
    try:
        raw_ser = serial.Serial(
            port=port,
            baudrate=9600,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.5
        )
        
        # Don't send any commands — just listen
        raw_ser.reset_input_buffer()
        
        start = time.time()
        all_data = bytearray()
        
        while time.time() - start < 5.0:
            if raw_ser.in_waiting > 0:
                chunk = raw_ser.read(raw_ser.in_waiting)
                all_data.extend(chunk)
            time.sleep(0.05)
        
        raw_ser.close()
        
        if all_data:
            # Check if it looks like J1708 (MID bytes in range 128-255)
            j1708_like = sum(1 for b in all_data if b >= 128) / max(len(all_data), 1)
            print(f"  {C.GREEN}RECEIVED {len(all_data)} raw bytes!{C.END}")
            print(f"  First 50 bytes (hex): {all_data[:50].hex(' ')}")
            print(f"  J1708-like bytes: {j1708_like*100:.1f}% (expect >30% for J1708)")
            
            if j1708_like > 0.2:
                print(f"  {C.GREEN}THIS LOOKS LIKE J1708 DATA!{C.END}")
                # Try to parse first message
                for i in range(len(all_data) - 3):
                    if all_data[i] >= 128:  # Valid MID
                        mid = all_data[i]
                        pid = all_data[i+1]
                        print(f"  Possible J1708 msg: MID={mid}, PID={pid}")
                        break
            else:
                print(f"  {C.YELLOW}Data doesn't look like J1708 (might be ELM327 echo/init){C.END}")
        else:
            print(f"  {C.YELLOW}No raw data received{C.END}")
    
    except serial.SerialException as e:
        print(f"  {C.RED}Serial error: {e}{C.END}")
    except Exception as e:
        print(f"  {C.RED}Error: {e}{C.END}")

def test_stn_commands(ser):
    """Test STN-specific commands (vLinker may use STN chip, not basic ELM327)."""
    print(f"\n{C.CYAN}--- TEST 6: STN/Extended Chip Commands ---{C.END}")
    
    commands = [
        ("STI", "STN chip ID"),
        ("STDI", "STN device info"),
        ("STPX h:AC, d:0080BE, r:1, t:2000", "STN J1708 passthrough"),
        ("STPRS", "STN supported protocols"),
        ("STPR", "STN current protocol"),
        ("STP J1708", "STN set J1708 directly"),
        ("STSBR 9600", "STN set baud 9600"),
    ]
    
    for cmd, desc in commands:
        ser.reset_input_buffer()
        ser.write((cmd + "\r").encode())
        time.sleep(1.0)
        resp = ser.read(ser.in_waiting or 256).decode("ascii", errors="replace").strip()
        
        if resp and "?" not in resp and "ERROR" not in resp:
            print(f"  {cmd:45s}: {C.GREEN}{repr(resp)}{C.END}")
        else:
            print(f"  {cmd:45s}: Not supported")

def test_different_bauds(port):
    """Try different baud rates to see if adapter responds differently."""
    print(f"\n{C.CYAN}--- TEST 7: Multi-Baud Raw Listen ---{C.END}")
    
    for baud in [9600, 19200, 38400, 115200, 250000, 500000]:
        try:
            test_ser = serial.Serial(port=port, baudrate=baud, timeout=0.5)
            test_ser.reset_input_buffer()
            time.sleep(1.0)
            data = test_ser.read(test_ser.in_waiting or 256)
            test_ser.close()
            
            if data and len(data) > 2:
                print(f"  {baud:>7d} baud: {C.GREEN}Received {len(data)} bytes{C.END} -> {data[:30].hex(' ')}")
            else:
                print(f"  {baud:>7d} baud: No data")
        except Exception as e:
            print(f"  {baud:>7d} baud: Error - {e}")

def run_diagnostics():
    """Run all diagnostic tests."""
    port = find_port()
    if not port:
        input("\nPress Enter to exit...")
        return
    
    print(f"\n{C.BOLD}Using port: {port}{C.END}")
    print(f"{C.YELLOW}Make sure the engine is RUNNING or key is ON!{C.END}")
    input(f"\nPress Enter to start diagnostics...")
    
    try:
        # Open at 38400 (default ELM327 baud) for AT command tests
        ser = serial.Serial(
            port=port,
            baudrate=38400,
            timeout=1.0,
            write_timeout=1.0
        )
        
        # Run tests
        test_elm327_info(ser)
        test_protocol_scan(ser)
        test_monitor_all(ser)
        test_j1708_direct(ser)
        test_stn_commands(ser)
        
        ser.close()
        time.sleep(0.5)
        
        # Raw serial tests (opens own connection)
        test_raw_9600(port)
        test_different_bauds(port)
        
    except serial.SerialException as e:
        print(f"\n{C.RED}SERIAL ERROR: {e}")
        print(f"Make sure no other program is using {port}!{C.END}")
    except Exception as e:
        print(f"\n{C.RED}ERROR: {e}{C.END}")
    
    # Summary
    print(f"\n{C.CYAN}{'='*60}")
    print("  DIAGNOSTIC COMPLETE")
    print(f"{'='*60}{C.END}")
    print(f"""
{C.BOLD}What to look for:{C.END}
  - If ANY test showed {C.GREEN}green "RECEIVED" or "DATA"{C.END}, the adapter CAN talk to the ECM
  - If Test 5 (raw 9600) got J1708-like data, I can fix the program to use raw mode
  - If Test 2 found a working protocol with data, I can fix the program to use it
  - If everything says "No data" — the adapter truly can't speak J1708

{C.BOLD}Screenshot this entire output and send it to me!{C.END}
""")
    input("Press Enter to exit...")

if __name__ == "__main__":
    run_diagnostics()
