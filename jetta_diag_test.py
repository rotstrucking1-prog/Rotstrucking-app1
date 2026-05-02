#!/usr/bin/env python3
"""
ROTS Trucking — Jetta TDI Full Diagnostic Scanner
Tries EVERYTHING to read data from the ECU
"""
import serial
import serial.tools.list_ports
import time
import sys

def find_port():
    ports = list(serial.tools.list_ports.comports())
    print(f"\n{'='*60}")
    print("ROTS TRUCKING — JETTA TDI DIAGNOSTIC SCANNER")
    print(f"{'='*60}")
    print(f"\nFound {len(ports)} COM port(s):")
    for p in ports:
        print(f"  {p.device} — {p.description} (VID:{p.vid} PID:{p.pid})")
    
    for p in ports:
        if p.vid and p.pid:
            return p.device
    if ports:
        return ports[0].device
    return None

def send_cmd(ser, cmd, wait=2.0):
    ser.flushInput()
    ser.write((cmd + "\r").encode())
    time.sleep(wait)
    resp = ser.read(ser.in_waiting).decode('ascii', errors='ignore').strip()
    return resp

def test_baud(port, baud):
    try:
        ser = serial.Serial(port, baud, timeout=2)
        time.sleep(0.5)
        resp = send_cmd(ser, "ATZ", 2.0)
        if "ELM" in resp:
            return ser, resp
        ser.close()
    except:
        pass
    return None, None

def main():
    port = find_port()
    if not port:
        print("\n*** NO COM PORT FOUND — is the Vgate plugged in? ***")
        input("\nPress Enter to exit...")
        return
    
    print(f"\nUsing port: {port}")
    
    # Step 1: Find working baud rate
    print(f"\n{'='*60}")
    print("TEST 1: FINDING ADAPTER (trying all baud rates)")
    print(f"{'='*60}")
    
    ser = None
    for baud in [115200, 38400, 9600, 57600, 230400, 500000]:
        print(f"  Trying {baud} baud... ", end="")
        s, resp = test_baud(port, baud)
        if s:
            ser = s
            print(f"YES! → {resp}")
            break
        else:
            print("no response")
    
    if not ser:
        print("\n*** ADAPTER NOT RESPONDING on any baud rate ***")
        input("\nPress Enter to exit...")
        return
    
    # Step 2: Get adapter info
    print(f"\n{'='*60}")
    print("TEST 2: ADAPTER INFO")
    print(f"{'='*60}")
    
    for cmd, label in [("ATI", "Identity"), ("AT@1", "Description"), ("ATRV", "Voltage"), ("ATDP", "Current Protocol"), ("ATDPN", "Protocol Number")]:
        resp = send_cmd(ser, cmd, 1.0)
        resp_clean = resp.replace(cmd, "").replace(">", "").strip()
        print(f"  {label}: {resp_clean}")
    
    # Step 3: Initialize properly
    print(f"\n{'='*60}")
    print("TEST 3: INITIALIZING CONNECTION TO CAR")
    print(f"{'='*60}")
    
    send_cmd(ser, "ATE0", 0.5)  # echo off
    send_cmd(ser, "ATL0", 0.5)  # linefeeds off
    send_cmd(ser, "ATS0", 0.5)  # spaces off
    send_cmd(ser, "ATH1", 0.5)  # headers ON (shows ECU address)
    send_cmd(ser, "ATST FF", 0.5)  # max timeout
    send_cmd(ser, "ATAT2", 0.5)  # adaptive timing aggressive
    
    # Step 4: Try each protocol manually
    print(f"\n{'='*60}")
    print("TEST 4: TRYING ALL OBD2 PROTOCOLS")
    print(f"{'='*60}")
    
    protocols = {
        "0": "Auto detect",
        "1": "SAE J1850 PWM (Ford)",
        "2": "SAE J1850 VPW (GM)",
        "3": "ISO 9141-2 (older Euro/Asian)",
        "4": "ISO 14230-4 KWP (5 baud init)",
        "5": "ISO 14230-4 KWP (fast init)",
        "6": "ISO 15765-4 CAN 11-bit 500k",
        "7": "ISO 15765-4 CAN 29-bit 500k",
        "8": "ISO 15765-4 CAN 11-bit 250k",
        "9": "ISO 15765-4 CAN 29-bit 250k",
        "A": "SAE J1939 CAN 29-bit 250k"
    }
    
    working_protocol = None
    
    for pnum, pname in protocols.items():
        print(f"\n  Protocol {pnum}: {pname}")
        send_cmd(ser, f"ATSP{pnum}", 1.0)
        
        # Try basic RPM query
        resp = send_cmd(ser, "010C", 3.0)
        resp_clean = resp.replace(">", "").strip()
        
        if "UNABLE TO CONNECT" in resp_clean or "NO DATA" in resp_clean or "ERROR" in resp_clean or "?" in resp_clean:
            print(f"    010C (RPM): {resp_clean} ❌")
        elif "41 0C" in resp_clean or "410C" in resp_clean:
            print(f"    010C (RPM): {resp_clean} ✅ GOT DATA!")
            working_protocol = pnum
            break
        else:
            print(f"    010C (RPM): {resp_clean}")
            if len(resp_clean) > 4 and "SEARCH" not in resp_clean:
                working_protocol = pnum
                break
    
    if not working_protocol:
        # Try auto with a simpler command
        print(f"\n  Last resort — auto protocol with 0100 (supported PIDs)...")
        send_cmd(ser, "ATSP0", 1.0)
        resp = send_cmd(ser, "0100", 5.0)
        resp_clean = resp.replace(">", "").strip()
        print(f"    0100: {resp_clean}")
        if "41 00" in resp_clean or "4100" in resp_clean:
            working_protocol = "auto"
            print("    ✅ Auto protocol found data!")
        
        # Check what protocol was actually selected
        dp = send_cmd(ser, "ATDPN", 1.0).replace(">", "").strip()
        print(f"    Selected protocol: {dp}")
    
    if not working_protocol:
        print(f"\n{'='*60}")
        print("*** NO PROTOCOL COULD REACH THE ECU ***")
        print(f"{'='*60}")
        print("\nPossible causes:")
        print("  1. Key is not in ON position (turn key, don't start engine)")
        print("  2. OBD2 port has no power (check fuse)")
        print("  3. ECU needs engine running (start the Jetta)")
        print("  4. Wiring issue at OBD2 connector")
        print("\nTry starting the engine and run this test again.")
        ser.close()
        input("\nPress Enter to exit...")
        return
    
    # Step 5: Scan ALL standard PIDs
    print(f"\n{'='*60}")
    print(f"TEST 5: SCANNING ALL STANDARD OBD2 PIDs")
    print(f"Working protocol: {working_protocol}")
    print(f"{'='*60}")
    
    # Turn headers off for cleaner reading
    send_cmd(ser, "ATH0", 0.5)
    
    pid_names = {
        "0100": "Supported PIDs 01-20",
        "0101": "Monitor status (MIL/DTC count)",
        "0103": "Fuel system status",
        "0104": "Engine load %",
        "0105": "Coolant temp",
        "0106": "Short fuel trim bank 1",
        "0107": "Long fuel trim bank 1",
        "010A": "Fuel pressure",
        "010B": "Intake manifold pressure",
        "010C": "RPM",
        "010D": "Speed",
        "010E": "Timing advance",
        "010F": "Intake air temp",
        "0110": "MAF air flow rate",
        "0111": "Throttle position",
        "011C": "OBD standard",
        "011F": "Runtime since start",
        "0120": "Supported PIDs 21-40",
        "0121": "Distance with MIL on",
        "012F": "Fuel tank level",
        "0131": "Distance since codes cleared",
        "0133": "Barometric pressure",
        "0140": "Supported PIDs 41-60",
        "0142": "Control module voltage",
        "0143": "Absolute load value",
        "0144": "Commanded equiv ratio",
        "0146": "Ambient air temp",
        "014C": "Commanded throttle actuator",
        "014D": "Time with MIL on",
        "014E": "Time since codes cleared",
        "0151": "Fuel type",
        "015B": "Hybrid battery remaining",
        "015C": "Engine oil temp",
        "015E": "Fuel rate",
        "0160": "Supported PIDs 61-80",
        "0161": "Driver torque demand",
        "0162": "Actual engine torque",
        "0163": "Engine reference torque",
        "0166": "Mass air flow sensor",
        "016B": "Exhaust gas temp bank 1 sensor 1",
        "016C": "Exhaust gas temp bank 1 sensor 2",
    }
    
    # Also try Mode 09 (vehicle info)
    mode9_pids = {
        "0900": "Mode 9 supported PIDs",
        "0902": "VIN (Vehicle ID Number)",
        "090A": "ECU name",
    }
    
    supported = []
    not_supported = []
    
    for pid, name in pid_names.items():
        resp = send_cmd(ser, pid, 2.0)
        resp_clean = resp.replace(">", "").strip()
        
        expected_prefix = "41" + pid[2:]
        
        if "NO DATA" in resp_clean or "UNABLE" in resp_clean or "ERROR" in resp_clean or "?" in resp_clean or resp_clean == "":
            not_supported.append(pid)
            print(f"  {pid} {name}: ❌ not supported")
        else:
            supported.append((pid, name, resp_clean))
            print(f"  {pid} {name}: ✅ {resp_clean}")
    
    # Mode 9
    print(f"\n{'='*60}")
    print("TEST 6: VEHICLE INFO (Mode 09)")
    print(f"{'='*60}")
    
    for pid, name in mode9_pids.items():
        resp = send_cmd(ser, pid, 3.0)
        resp_clean = resp.replace(">", "").strip()
        if "NO DATA" not in resp_clean and "ERROR" not in resp_clean and resp_clean:
            print(f"  {pid} {name}: ✅ {resp_clean}")
        else:
            print(f"  {pid} {name}: ❌")
    
    # Mode 03 — read trouble codes
    print(f"\n{'='*60}")
    print("TEST 7: TROUBLE CODES (Mode 03)")
    print(f"{'='*60}")
    
    resp = send_cmd(ser, "03", 3.0)
    resp_clean = resp.replace(">", "").strip()
    if "NO DATA" in resp_clean or "4300" in resp_clean:
        print("  No trouble codes found ✅")
    else:
        print(f"  Raw response: {resp_clean}")
    
    # Mode 07 — pending codes
    resp = send_cmd(ser, "07", 3.0)
    resp_clean = resp.replace(">", "").strip()
    if "NO DATA" in resp_clean or "4700" in resp_clean:
        print("  No pending codes ✅")
    else:
        print(f"  Pending codes raw: {resp_clean}")
    
    # Summary
    print(f"\n{'='*60}")
    print("SUMMARY")
    print(f"{'='*60}")
    print(f"  Adapter: ELM327 on {port}")
    print(f"  Protocol: {working_protocol}")
    print(f"  Supported PIDs: {len(supported)}")
    print(f"  Not supported: {len(not_supported)}")
    
    if supported:
        print(f"\n  ✅ SUPPORTED PIDs (these will work on the dashboard):")
        for pid, name, val in supported:
            print(f"     {pid} — {name}")
    
    print(f"\n{'='*60}")
    print("TEST COMPLETE — Take a screenshot and send to George!")
    print(f"{'='*60}")
    
    ser.close()
    input("\nPress Enter to exit...")

if __name__ == "__main__":
    main()
