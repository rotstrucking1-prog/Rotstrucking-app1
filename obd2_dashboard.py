#!/usr/bin/env python3
"""
═══════════════════════════════════════════════════════════════
  OBD2 DIAGNOSTIC DASHBOARD — 2009 VW Jetta TDI 2.0L
  Built for ROTS TRUCKING LLC
  Adapter: Vgate vLinker FS (ELM327 USB)
═══════════════════════════════════════════════════════════════
"""

import sys
import os
import time
import threading
import serial
import serial.tools.list_ports

# ─── Try to import PyQt5, install if missing ───
try:
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
except ImportError:
    print("Installing PyQt5...")
    os.system(f"{sys.executable} -m pip install PyQt5")
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

import math
import struct

# ═══════════════════════════════════════════════════════════════
#  ELM327 COMMUNICATION
# ═══════════════════════════════════════════════════════════════

class ELM327:
    """Handles serial communication with ELM327/Vgate adapter"""

    def __init__(self):
        self.port = None
        self.serial = None
        self.connected = False
        self.protocol = "Unknown"
        self.elm_version = "Unknown"
        self.voltage = "N/A"
        self.tx_count = 0
        self.rx_count = 0
        self.error_count = 0

    def find_port(self):
        """Find the Vgate/ELM327 adapter port"""
        ports = serial.tools.list_ports.comports()
        for p in ports:
            desc = (p.description or "").lower()
            mfg = (p.manufacturer or "").lower()
            if any(x in desc for x in ["elm", "obd", "vgate", "vlinker", "usb-serial", "ch340", "ft232", "cp210"]):
                return p.device
            if any(x in mfg for x in ["elm", "vgate", "wch", "ftdi", "silicon"]):
                return p.device
        # Try common ports
        for p in ports:
            if "COM" in p.device or "ttyUSB" in p.device or "ttyACM" in p.device:
                return p.device
        return None

    def connect(self, port=None):
        """Connect to adapter and initialize — tries multiple baud rates"""
        if port is None:
            port = self.find_port()
        if port is None:
            return False, "No OBD2 adapter found. Is the USB cable plugged in?"

        self.port = port
        self.connect_log = []

        # Try multiple baud rates — Vgate vLinker FS often uses 115200
        baud_rates = [115200, 38400, 9600, 57600, 230400, 500000]

        for baud in baud_rates:
            self.connect_log.append(f"Trying {port} @ {baud} baud...")
            try:
                if self.serial and self.serial.is_open:
                    self.serial.close()
                    time.sleep(0.3)

                self.serial = serial.Serial(
                    port=port,
                    baudrate=baud,
                    timeout=2,
                    write_timeout=2,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE
                )
                time.sleep(0.5)

                # Flush any garbage
                self.serial.flushInput()
                self.serial.flushOutput()

                # Reset adapter
                resp = self._send_cmd("ATZ", delay=2.0)
                self.connect_log.append(f"  ATZ response: {repr(resp)}")

                if not resp or ("ELM" not in resp.upper() and "STN" not in resp.upper()
                               and "AT" not in resp.upper() and "OK" not in resp.upper()):
                    self.connect_log.append(f"  No valid response at {baud} baud")
                    continue

                self.connect_log.append(f"  ** ADAPTER FOUND at {baud} baud! **")
                time.sleep(0.3)

                # Echo off
                self._send_cmd("ATE0")

                # Get version
                ver = self._send_cmd("ATI")
                if ver:
                    self.elm_version = ver.strip()
                    self.connect_log.append(f"  Version: {self.elm_version}")

                # Linefeed off
                self._send_cmd("ATL0")

                # Spaces off for easier parsing
                self._send_cmd("ATS0")

                # Headers off
                self._send_cmd("ATH0")

                # Adaptive timing auto
                self._send_cmd("ATAT2")

                # Set timeout (longer for diesel ECU)
                self._send_cmd("ATST96")

                # Auto-detect protocol
                self._send_cmd("ATSP0")

                # Try a test command to force protocol detection
                test = self._send_cmd("0100", delay=2.0)
                self.connect_log.append(f"  PID 0100 response: {repr(test)}")

                if test and "UNABLE" not in test.upper() and "NO DATA" not in test.upper() and "ERROR" not in test.upper() and "?" not in test:
                    self.connected = True
                    # Get detected protocol
                    proto = self._send_cmd("ATDPN")
                    if proto:
                        proto_num = proto.strip().replace("A", "")
                        proto_names = {
                            "1": "SAE J1850 PWM",
                            "2": "SAE J1850 VPW",
                            "3": "ISO 9141-2",
                            "4": "ISO 14230-4 (KWP slow)",
                            "5": "ISO 14230-4 (KWP fast)",
                            "6": "ISO 15765-4 (CAN 11/500)",
                            "7": "ISO 15765-4 (CAN 29/500)",
                            "8": "ISO 15765-4 (CAN 11/250)",
                            "9": "ISO 15765-4 (CAN 29/250)",
                        }
                        self.protocol = proto_names.get(proto_num, f"Protocol {proto_num}")
                        self.connect_log.append(f"  Protocol: {self.protocol}")

                    # Get battery voltage
                    volts = self._send_cmd("ATRV")
                    if volts:
                        self.voltage = volts.strip()
                        self.connect_log.append(f"  Battery: {self.voltage}")

                    return True, f"Connected on {port} @ {baud} baud"
                else:
                    # Try forcing each CAN protocol (most likely for 2009 TDI)
                    for proto_id in ["6", "7", "8", "9", "3", "4", "5"]:
                        self._send_cmd(f"ATSP{proto_id}")
                        time.sleep(0.5)
                        test = self._send_cmd("0100", delay=2.0)
                        self.connect_log.append(f"  Protocol {proto_id} test: {repr(test)}")
                        if test and "UNABLE" not in test.upper() and "NO DATA" not in test.upper() and "?" not in test:
                            self.connected = True
                            proto_names = {
                                "3": "ISO 9141-2",
                                "4": "ISO 14230-4 (KWP slow)",
                                "5": "ISO 14230-4 (KWP fast)",
                                "6": "ISO 15765-4 (CAN 11/500)",
                                "7": "ISO 15765-4 (CAN 29/500)",
                                "8": "ISO 15765-4 (CAN 11/250)",
                                "9": "ISO 15765-4 (CAN 29/250)",
                            }
                            self.protocol = proto_names.get(proto_id, f"Protocol {proto_id}")
                            volts = self._send_cmd("ATRV")
                            if volts:
                                self.voltage = volts.strip()
                            return True, f"Connected on {port} @ {baud} (forced {self.protocol})"

                    self.connect_log.append(f"  Adapter responded but vehicle ECU not talking")

            except serial.SerialException as e:
                self.connect_log.append(f"  Port error: {str(e)}")
                continue
            except Exception as e:
                self.connect_log.append(f"  Error: {str(e)}")
                continue

        # None of the baud rates worked — build diagnostic message
        log_text = "\n".join(self.connect_log)
        return False, f"Adapter found but not responding.\n\nChecklist:\n1. Is ignition key turned to ON? (not just ACC)\n2. Close any other OBD apps (Peterbilt Dashboard, etc)\n3. Unplug USB, wait 5 sec, plug back in\n4. Try running as Administrator\n\nDiagnostic log:\n{log_text}"

    def _send_cmd(self, cmd, delay=0.3):
        """Send AT/OBD command and get response"""
        if not self.serial or not self.serial.is_open:
            return None
        try:
            self.serial.flushInput()
            self.serial.write((cmd + "\r").encode())
            self.tx_count += 1
            time.sleep(delay)

            response = b""
            timeout = time.time() + 3
            while time.time() < timeout:
                if self.serial.in_waiting > 0:
                    chunk = self.serial.read(self.serial.in_waiting)
                    response += chunk
                    if b">" in chunk:
                        break
                time.sleep(0.05)

            text = response.decode("ascii", errors="ignore")
            # Clean up response
            text = text.replace("\r", "\n").replace(">", "").strip()
            # Remove echo
            lines = [l.strip() for l in text.split("\n") if l.strip() and l.strip() != cmd]
            result = "\n".join(lines)
            if result and "ERROR" not in result.upper():
                self.rx_count += 1
            return result

        except Exception as e:
            self.error_count += 1
            return None

    def query_pid(self, pid_hex):
        """Query a standard OBD2 PID and return raw hex bytes"""
        resp = self._send_cmd(pid_hex, delay=0.2)
        if resp is None:
            return None
        # Filter out non-data lines
        resp = resp.upper().replace(" ", "")
        lines = resp.split("\n")
        for line in lines:
            line = line.strip()
            if line.startswith("41") or line.startswith("61"):
                # Standard mode 01 or mode 21 response
                try:
                    return bytes.fromhex(line)
                except:
                    pass
        return None

    def get_dtc(self):
        """Read diagnostic trouble codes"""
        resp = self._send_cmd("03", delay=1.0)
        if resp is None:
            return []
        codes = []
        resp = resp.upper().replace(" ", "")
        lines = resp.split("\n")
        for line in lines:
            line = line.strip()
            if line.startswith("43"):
                # Parse DTC bytes
                data = line[2:]  # strip "43"
                for i in range(0, len(data), 4):
                    if i + 4 <= len(data):
                        dtc_hex = data[i:i+4]
                        if dtc_hex == "0000":
                            continue
                        first = int(dtc_hex[0], 16)
                        prefix = ["P", "C", "B", "U"][first >> 2]
                        code = prefix + str(first & 0x03) + dtc_hex[1:]
                        codes.append(code)
        return codes

    def clear_dtc(self):
        """Clear all trouble codes"""
        resp = self._send_cmd("04", delay=2.0)
        return resp

    def get_vin(self):
        """Try to read VIN"""
        resp = self._send_cmd("0902", delay=2.0)
        if resp is None:
            return "N/A"
        # VIN decoding from multi-frame response
        resp = resp.upper().replace(" ", "")
        lines = resp.split("\n")
        hex_data = ""
        for line in lines:
            line = line.strip()
            # Look for data after header bytes
            if len(line) >= 4:
                hex_data += line
        # Try to extract ASCII from hex
        vin = ""
        try:
            raw = bytes.fromhex(hex_data)
            for b in raw:
                if 0x20 <= b <= 0x7E:
                    vin += chr(b)
        except:
            pass
        # VIN is 17 chars
        if len(vin) >= 17:
            # Find the 17-char VIN in the string
            for i in range(len(vin) - 16):
                candidate = vin[i:i+17]
                if all(c.isalnum() for c in candidate):
                    return candidate
        return vin if vin else "N/A"

    def disconnect(self):
        """Close connection"""
        if self.serial and self.serial.is_open:
            try:
                self._send_cmd("ATZ")
                self.serial.close()
            except:
                pass
        self.connected = False


# ═══════════════════════════════════════════════════════════════
#  OBD2 PIDs — Standard + TDI Enhanced
# ═══════════════════════════════════════════════════════════════

class OBD2PIDs:
    """Standard OBD2 PID definitions"""

    @staticmethod
    def rpm(data):
        """PID 010C — Engine RPM"""
        if data and len(data) >= 4:
            return ((data[2] * 256) + data[3]) / 4.0
        return None

    @staticmethod
    def speed(data):
        """PID 010D — Vehicle Speed (km/h)"""
        if data and len(data) >= 3:
            return data[2]  # km/h
        return None

    @staticmethod
    def speed_mph(data):
        """Convert speed to MPH"""
        kmh = OBD2PIDs.speed(data)
        if kmh is not None:
            return kmh * 0.621371
        return None

    @staticmethod
    def coolant_temp(data):
        """PID 0105 — Coolant Temperature (°F)"""
        if data and len(data) >= 3:
            c = data[2] - 40
            return c * 9.0 / 5.0 + 32
        return None

    @staticmethod
    def intake_temp(data):
        """PID 010F — Intake Air Temperature (°F)"""
        if data and len(data) >= 3:
            c = data[2] - 40
            return c * 9.0 / 5.0 + 32
        return None

    @staticmethod
    def engine_load(data):
        """PID 0104 — Engine Load (%)"""
        if data and len(data) >= 3:
            return data[2] * 100.0 / 255.0
        return None

    @staticmethod
    def throttle(data):
        """PID 0111 — Throttle Position (%)"""
        if data and len(data) >= 3:
            return data[2] * 100.0 / 255.0
        return None

    @staticmethod
    def maf(data):
        """PID 0110 — MAF Air Flow Rate (g/s)"""
        if data and len(data) >= 4:
            return ((data[2] * 256) + data[3]) / 100.0
        return None

    @staticmethod
    def boost_pressure(data):
        """PID 010B — Intake Manifold Pressure (PSI) — boost gauge for TDI"""
        if data and len(data) >= 3:
            kpa = data[2]
            psi = kpa * 0.145038
            return psi
        return None

    @staticmethod
    def fuel_pressure(data):
        """PID 010A — Fuel Pressure (PSI)"""
        if data and len(data) >= 3:
            return data[2] * 3 * 0.145038  # kPa to PSI
        return None

    @staticmethod
    def fuel_rail_pressure(data):
        """PID 0123 — Fuel Rail Gauge Pressure (PSI) — diesel common rail"""
        if data and len(data) >= 4:
            kpa = ((data[2] * 256) + data[3]) * 10
            return kpa * 0.145038
        return None

    @staticmethod
    def timing_advance(data):
        """PID 010E — Timing Advance (degrees)"""
        if data and len(data) >= 3:
            return data[2] / 2.0 - 64
        return None

    @staticmethod
    def fuel_trim_short_b1(data):
        """PID 0106 — Short Term Fuel Trim Bank 1 (%)"""
        if data and len(data) >= 3:
            return (data[2] - 128) * 100.0 / 128.0
        return None

    @staticmethod
    def fuel_trim_long_b1(data):
        """PID 0107 — Long Term Fuel Trim Bank 1 (%)"""
        if data and len(data) >= 3:
            return (data[2] - 128) * 100.0 / 128.0
        return None

    @staticmethod
    def battery_voltage_elm(elm):
        """Read battery voltage from ELM327 AT command"""
        return elm._send_cmd("ATRV")

    @staticmethod
    def oil_temp(data):
        """PID 015C — Oil Temperature (°F)"""
        if data and len(data) >= 3:
            c = data[2] - 40
            return c * 9.0 / 5.0 + 32
        return None

    @staticmethod
    def fuel_rate(data):
        """PID 015E — Engine Fuel Rate (gal/hr)"""
        if data and len(data) >= 4:
            lph = ((data[2] * 256) + data[3]) * 0.05  # L/h
            return lph * 0.264172  # to gal/hr
        return None

    @staticmethod
    def exhaust_temp_b1s1(data):
        """PID 0178 — Exhaust Gas Temp Bank 1 Sensor 1 (°F)"""
        if data and len(data) >= 4:
            c = ((data[2] * 256) + data[3]) / 10.0 - 40
            return c * 9.0 / 5.0 + 32
        return None

    @staticmethod
    def dpf_temp_inlet(data):
        """PID 017C — DPF Temperature Inlet (°F)"""
        if data and len(data) >= 4:
            c = ((data[2] * 256) + data[3]) / 10.0 - 40
            return c * 9.0 / 5.0 + 32
        return None

    @staticmethod
    def dpf_pressure(data):
        """PID 017A — DPF Differential Pressure (PSI)"""
        if data and len(data) >= 4:
            pa = ((data[2] * 256) + data[3]) * 0.01  # Pa (unsigned with offset?)
            return pa * 0.000145038  # Pa to PSI
        return None


# ═══════════════════════════════════════════════════════════════
#  GAUGE WIDGET
# ═══════════════════════════════════════════════════════════════

class GaugeWidget(QWidget):
    """Circular gauge display"""

    def __init__(self, title, unit, min_val, max_val, warn_val=None, danger_val=None, decimals=0, parent=None):
        super().__init__(parent)
        self.title = title
        self.unit = unit
        self.min_val = min_val
        self.max_val = max_val
        self.warn_val = warn_val
        self.danger_val = danger_val
        self.decimals = decimals
        self.value = 0
        self.setMinimumSize(180, 180)

    def set_value(self, val):
        if val is not None:
            self.value = val
            self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        w = self.width()
        h = self.height()
        side = min(w, h)
        cx, cy = w / 2, h / 2
        r = side / 2 - 10

        # Background
        painter.setBrush(QBrush(QColor(25, 25, 35)))
        painter.setPen(Qt.NoPen)
        painter.drawEllipse(QPointF(cx, cy), r, r)

        # Arc background
        pen = QPen(QColor(60, 60, 70), 8)
        pen.setCapStyle(Qt.RoundCap)
        painter.setPen(pen)
        arc_r = r - 15
        rect = QRectF(cx - arc_r, cy - arc_r, arc_r * 2, arc_r * 2)
        painter.drawArc(rect, 225 * 16, -270 * 16)

        # Value arc
        if self.max_val > self.min_val:
            ratio = max(0, min(1, (self.value - self.min_val) / (self.max_val - self.min_val)))
        else:
            ratio = 0

        # Color based on value
        if self.danger_val is not None and self.value >= self.danger_val:
            color = QColor(255, 50, 50)
        elif self.warn_val is not None and self.value >= self.warn_val:
            color = QColor(255, 180, 0)
        else:
            color = QColor(0, 200, 100)

        pen = QPen(color, 8)
        pen.setCapStyle(Qt.RoundCap)
        painter.setPen(pen)
        sweep = -270 * ratio
        painter.drawArc(rect, 225 * 16, int(sweep * 16))

        # Value text
        if self.decimals == 0:
            val_text = f"{int(self.value)}"
        else:
            val_text = f"{self.value:.{self.decimals}f}"

        painter.setPen(QColor(255, 255, 255))
        font = QFont("Consolas", int(side / 7), QFont.Bold)
        painter.setFont(font)
        painter.drawText(QRectF(cx - r, cy - 20, r * 2, 40), Qt.AlignCenter, val_text)

        # Unit
        painter.setPen(QColor(160, 160, 180))
        font = QFont("Segoe UI", int(side / 14))
        painter.setFont(font)
        painter.drawText(QRectF(cx - r, cy + 15, r * 2, 25), Qt.AlignCenter, self.unit)

        # Title
        painter.setPen(QColor(120, 120, 140))
        font = QFont("Segoe UI", int(side / 16))
        painter.setFont(font)
        painter.drawText(QRectF(cx - r, cy + 35, r * 2, 20), Qt.AlignCenter, self.title)

        painter.end()


# ═══════════════════════════════════════════════════════════════
#  DTC PANEL
# ═══════════════════════════════════════════════════════════════

class DTCPanel(QWidget):
    """Diagnostic Trouble Code panel"""

    def __init__(self, elm, parent=None):
        super().__init__(parent)
        self.elm = elm
        layout = QVBoxLayout(self)

        title = QLabel("🔧 TROUBLE CODES")
        title.setStyleSheet("font-size: 18px; font-weight: bold; color: #FFA500; padding: 10px;")
        layout.addWidget(title)

        btn_layout = QHBoxLayout()
        self.scan_btn = QPushButton("🔍 SCAN FOR CODES")
        self.scan_btn.setStyleSheet("""
            QPushButton { background: #2563EB; color: white; padding: 12px 24px;
                         border-radius: 6px; font-size: 14px; font-weight: bold; }
            QPushButton:hover { background: #1D4ED8; }
        """)
        self.scan_btn.clicked.connect(self.scan_codes)
        btn_layout.addWidget(self.scan_btn)

        self.clear_btn = QPushButton("🗑️ CLEAR ALL CODES")
        self.clear_btn.setStyleSheet("""
            QPushButton { background: #DC2626; color: white; padding: 12px 24px;
                         border-radius: 6px; font-size: 14px; font-weight: bold; }
            QPushButton:hover { background: #B91C1C; }
        """)
        self.clear_btn.clicked.connect(self.clear_codes)
        btn_layout.addWidget(self.clear_btn)

        layout.addLayout(btn_layout)

        self.code_list = QTextEdit()
        self.code_list.setReadOnly(True)
        self.code_list.setStyleSheet("""
            QTextEdit { background: #1a1a2e; color: #E0E0E0; font-family: Consolas;
                       font-size: 14px; border: 1px solid #333; border-radius: 6px; padding: 10px; }
        """)
        layout.addWidget(self.code_list)

    def scan_codes(self):
        if not self.elm.connected:
            self.code_list.setText("❌ Not connected to vehicle")
            return
        self.code_list.setText("🔍 Scanning for trouble codes...")
        QApplication.processEvents()
        codes = self.elm.get_dtc()
        if codes:
            text = f"⚠️ Found {len(codes)} trouble code(s):\n\n"
            for code in codes:
                text += f"  🔴 {code}\n"
            text += "\n\nLook up codes at: https://www.obd-codes.com/"
        else:
            text = "✅ No trouble codes found! Vehicle is clean."
        self.code_list.setText(text)

    def clear_codes(self):
        if not self.elm.connected:
            self.code_list.setText("❌ Not connected to vehicle")
            return
        reply = QMessageBox.question(self, "Clear Codes",
                                      "This will clear ALL trouble codes and turn off the Check Engine Light.\n\nAre you sure?",
                                      QMessageBox.Yes | QMessageBox.No)
        if reply == QMessageBox.Yes:
            self.elm.clear_dtc()
            self.code_list.setText("✅ All codes cleared! Check Engine Light should turn off.\n\nDrive for a bit and re-scan to see if any codes return.")


# ═══════════════════════════════════════════════════════════════
#  MAIN WINDOW
# ═══════════════════════════════════════════════════════════════

class OBD2Dashboard(QMainWindow):
    """Main dashboard window"""

    def __init__(self):
        super().__init__()
        self.elm = ELM327()
        self.polling = False
        self.poll_thread = None

        self.setWindowTitle("🚗 OBD2 DASHBOARD — 2009 VW Jetta TDI")
        self.setMinimumSize(1100, 750)
        self.setStyleSheet("""
            QMainWindow { background: #0f0f1a; }
            QTabWidget::pane { border: 1px solid #333; background: #0f0f1a; }
            QTabBar::tab { background: #1a1a2e; color: #888; padding: 10px 20px;
                          font-size: 13px; font-weight: bold; border: 1px solid #333;
                          border-bottom: none; border-top-left-radius: 6px;
                          border-top-right-radius: 6px; margin-right: 2px; }
            QTabBar::tab:selected { background: #2563EB; color: white; }
            QTabBar::tab:hover { background: #333; color: #ddd; }
        """)

        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(10, 10, 10, 10)

        # ─── Header ───
        header = QWidget()
        header.setStyleSheet("background: #1a1a2e; border-radius: 8px; padding: 8px;")
        header_layout = QHBoxLayout(header)

        title_label = QLabel("🚗 2009 VW JETTA TDI — OBD2 DASHBOARD")
        title_label.setStyleSheet("font-size: 20px; font-weight: bold; color: #60A5FA;")
        header_layout.addWidget(title_label)

        header_layout.addStretch()

        # Port selector
        self.port_combo = QComboBox()
        self.port_combo.setStyleSheet("background: #2a2a3e; color: white; padding: 6px; min-width: 120px;")
        self.refresh_ports()
        header_layout.addWidget(QLabel("Port:"))
        header_layout.addWidget(self.port_combo)

        refresh_btn = QPushButton("🔄")
        refresh_btn.setStyleSheet("background: #333; color: white; padding: 6px 10px; border-radius: 4px;")
        refresh_btn.clicked.connect(self.refresh_ports)
        header_layout.addWidget(refresh_btn)

        self.connect_btn = QPushButton("🔌 CONNECT")
        self.connect_btn.setStyleSheet("""
            QPushButton { background: #16A34A; color: white; padding: 8px 20px;
                         border-radius: 6px; font-weight: bold; font-size: 13px; }
            QPushButton:hover { background: #15803D; }
        """)
        self.connect_btn.clicked.connect(self.toggle_connection)
        header_layout.addWidget(self.connect_btn)

        main_layout.addWidget(header)

        # ─── Status bar ───
        self.status_bar = QLabel("⚪ Disconnected — Plug in Vgate adapter and turn ignition ON")
        self.status_bar.setStyleSheet("color: #888; font-size: 12px; padding: 4px 8px;")
        main_layout.addWidget(self.status_bar)

        # ─── Tabs ───
        self.tabs = QTabWidget()
        main_layout.addWidget(self.tabs)

        # Tab 1: Live Gauges
        self.gauges_tab = QWidget()
        self.setup_gauges_tab()
        self.tabs.addTab(self.gauges_tab, "📊 LIVE DATA")

        # Tab 2: Trouble Codes
        self.dtc_panel = DTCPanel(self.elm)
        self.tabs.addTab(self.dtc_panel, "🔧 TROUBLE CODES")

        # Tab 3: Vehicle Info
        self.info_tab = QWidget()
        self.setup_info_tab()
        self.tabs.addTab(self.info_tab, "ℹ️ VEHICLE INFO")

        # Tab 4: Data Log
        self.log_tab = QWidget()
        self.setup_log_tab()
        self.tabs.addTab(self.log_tab, "📝 DATA LOG")

        # ─── Connection info panel ───
        conn_panel = QWidget()
        conn_panel.setStyleSheet("background: #1a1a2e; border-radius: 6px; padding: 6px;")
        conn_layout = QHBoxLayout(conn_panel)

        self.conn_status = QLabel("⚪ Not Connected")
        self.conn_status.setStyleSheet("color: #888; font-size: 11px;")
        conn_layout.addWidget(self.conn_status)

        self.conn_proto = QLabel("Protocol: —")
        self.conn_proto.setStyleSheet("color: #888; font-size: 11px;")
        conn_layout.addWidget(self.conn_proto)

        self.conn_stats = QLabel("TX: 0 | RX: 0 | Errors: 0")
        self.conn_stats.setStyleSheet("color: #888; font-size: 11px;")
        conn_layout.addWidget(self.conn_stats)

        self.conn_voltage = QLabel("Battery: —")
        self.conn_voltage.setStyleSheet("color: #888; font-size: 11px;")
        conn_layout.addWidget(self.conn_voltage)

        main_layout.addWidget(conn_panel)

        # ─── Timer for UI updates ───
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.update_ui)
        self.update_timer.start(200)

        # Storage for latest readings
        self.latest = {}

    def setup_gauges_tab(self):
        layout = QVBoxLayout(self.gauges_tab)

        # Row 1: Primary gauges
        row1 = QHBoxLayout()
        self.gauge_rpm = GaugeWidget("RPM", "rpm", 0, 5000, warn_val=4000, danger_val=4500)
        self.gauge_speed = GaugeWidget("SPEED", "mph", 0, 120)
        self.gauge_coolant = GaugeWidget("COOLANT", "°F", 100, 260, warn_val=220, danger_val=240)
        self.gauge_boost = GaugeWidget("BOOST", "psi", 0, 40, warn_val=30, danger_val=35)
        row1.addWidget(self.gauge_rpm)
        row1.addWidget(self.gauge_speed)
        row1.addWidget(self.gauge_coolant)
        row1.addWidget(self.gauge_boost)
        layout.addLayout(row1)

        # Row 2: Secondary gauges
        row2 = QHBoxLayout()
        self.gauge_load = GaugeWidget("ENGINE LOAD", "%", 0, 100, warn_val=85, danger_val=95)
        self.gauge_throttle = GaugeWidget("THROTTLE", "%", 0, 100)
        self.gauge_fuel_rail = GaugeWidget("FUEL RAIL", "psi", 0, 30000, warn_val=25000)
        self.gauge_maf = GaugeWidget("AIR FLOW", "g/s", 0, 300, decimals=1)
        row2.addWidget(self.gauge_load)
        row2.addWidget(self.gauge_throttle)
        row2.addWidget(self.gauge_fuel_rail)
        row2.addWidget(self.gauge_maf)
        layout.addLayout(row2)

        # Row 3: Diesel-specific
        row3 = QHBoxLayout()
        self.gauge_oil_temp = GaugeWidget("OIL TEMP", "°F", 100, 300, warn_val=250, danger_val=280)
        self.gauge_egt = GaugeWidget("EXHAUST TEMP", "°F", 100, 1400, warn_val=1100, danger_val=1300)
        self.gauge_intake = GaugeWidget("INTAKE TEMP", "°F", -20, 200, warn_val=150, danger_val=180)
        self.gauge_timing = GaugeWidget("TIMING", "deg", -20, 40, decimals=1)
        row3.addWidget(self.gauge_oil_temp)
        row3.addWidget(self.gauge_egt)
        row3.addWidget(self.gauge_intake)
        row3.addWidget(self.gauge_timing)
        layout.addLayout(row3)

    def setup_info_tab(self):
        layout = QVBoxLayout(self.info_tab)
        self.info_text = QTextEdit()
        self.info_text.setReadOnly(True)
        self.info_text.setStyleSheet("""
            QTextEdit { background: #1a1a2e; color: #E0E0E0; font-family: Consolas;
                       font-size: 14px; border: 1px solid #333; border-radius: 6px; padding: 15px; }
        """)
        self.info_text.setText("Connect to vehicle to see info...")
        layout.addWidget(self.info_text)

    def setup_log_tab(self):
        layout = QVBoxLayout(self.log_tab)

        btn_layout = QHBoxLayout()
        self.log_btn = QPushButton("📝 START LOGGING")
        self.log_btn.setStyleSheet("""
            QPushButton { background: #2563EB; color: white; padding: 10px 20px;
                         border-radius: 6px; font-weight: bold; }
        """)
        self.log_btn.clicked.connect(self.toggle_logging)
        btn_layout.addWidget(self.log_btn)

        self.export_btn = QPushButton("💾 EXPORT CSV")
        self.export_btn.setStyleSheet("""
            QPushButton { background: #7C3AED; color: white; padding: 10px 20px;
                         border-radius: 6px; font-weight: bold; }
        """)
        self.export_btn.clicked.connect(self.export_log)
        btn_layout.addWidget(self.export_btn)
        layout.addLayout(btn_layout)

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setStyleSheet("""
            QTextEdit { background: #1a1a2e; color: #00FF88; font-family: Consolas;
                       font-size: 12px; border: 1px solid #333; border-radius: 6px; padding: 10px; }
        """)
        layout.addWidget(self.log_text)

        self.logging_active = False
        self.log_data = []

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.port_combo.addItem(f"{p.device} — {p.description}", p.device)
        if self.port_combo.count() == 0:
            self.port_combo.addItem("No ports found", "")

    def toggle_connection(self):
        if self.elm.connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_combo.currentData()
        if not port:
            self.status_bar.setText("❌ No port selected — plug in the Vgate adapter")
            return

        self.status_bar.setText(f"🔄 Connecting to {port}...")
        self.connect_btn.setEnabled(False)
        QApplication.processEvents()

        success, msg = self.elm.connect(port)
        if success:
            self.status_bar.setText(f"✅ {msg}")
            self.connect_btn.setText("🔌 DISCONNECT")
            self.connect_btn.setStyleSheet("""
                QPushButton { background: #DC2626; color: white; padding: 8px 20px;
                             border-radius: 6px; font-weight: bold; font-size: 13px; }
                QPushButton:hover { background: #B91C1C; }
            """)
            self.conn_status.setText("🟢 Connected")
            self.conn_status.setStyleSheet("color: #00FF88; font-size: 11px;")
            self.conn_proto.setText(f"Protocol: {self.elm.protocol}")
            self.conn_voltage.setText(f"Battery: {self.elm.voltage}")

            # Load vehicle info
            self.load_vehicle_info()

            # Start polling
            self.polling = True
            self.poll_thread = threading.Thread(target=self.poll_loop, daemon=True)
            self.poll_thread.start()
        else:
            self.status_bar.setText(f"❌ {msg}")

        self.connect_btn.setEnabled(True)

    def disconnect(self):
        self.polling = False
        if self.poll_thread:
            self.poll_thread.join(timeout=3)
        self.elm.disconnect()
        self.status_bar.setText("⚪ Disconnected")
        self.connect_btn.setText("🔌 CONNECT")
        self.connect_btn.setStyleSheet("""
            QPushButton { background: #16A34A; color: white; padding: 8px 20px;
                         border-radius: 6px; font-weight: bold; font-size: 13px; }
            QPushButton:hover { background: #15803D; }
        """)
        self.conn_status.setText("⚪ Not Connected")
        self.conn_status.setStyleSheet("color: #888; font-size: 11px;")

    def load_vehicle_info(self):
        info = "═══════════════════════════════════════\n"
        info += "  VEHICLE INFORMATION\n"
        info += "═══════════════════════════════════════\n\n"
        info += f"  Vehicle:    2009 Volkswagen Jetta TDI\n"
        info += f"  Engine:     2.0L TDI Diesel (CBEA/CJAA)\n\n"

        vin = self.elm.get_vin()
        info += f"  VIN:        {vin}\n\n"
        info += f"  Adapter:    {self.elm.elm_version}\n"
        info += f"  Port:       {self.elm.port}\n"
        info += f"  Protocol:   {self.elm.protocol}\n"
        info += f"  Battery:    {self.elm.voltage}\n\n"

        # Check supported PIDs
        info += "─── SUPPORTED PIDs ───\n\n"
        pids_to_check = {
            "0100": "PIDs 01-20",
            "0120": "PIDs 21-40",
            "0140": "PIDs 41-60",
            "0160": "PIDs 61-80",
        }
        for pid, desc in pids_to_check.items():
            data = self.elm.query_pid(pid)
            if data:
                # Parse supported PIDs bitmask
                if len(data) >= 6:
                    bitmask = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5]
                    supported = []
                    base = int(pid[2:], 16)
                    for i in range(32):
                        if bitmask & (1 << (31 - i)):
                            supported.append(f"{base + i + 1:02X}")
                    info += f"  {desc}: {', '.join(supported)}\n"
            else:
                info += f"  {desc}: Not available\n"

        self.info_text.setText(info)

    def poll_loop(self):
        """Background thread that polls OBD2 PIDs"""
        # PID query list: (cmd, key)
        pids = [
            ("010C", "rpm"),
            ("010D", "speed"),
            ("0105", "coolant"),
            ("010B", "boost"),
            ("0104", "load"),
            ("0111", "throttle"),
            ("0123", "fuel_rail"),
            ("0110", "maf"),
            ("015C", "oil_temp"),
            ("0178", "egt"),
            ("010F", "intake_temp"),
            ("010E", "timing"),
        ]

        while self.polling:
            for pid_cmd, key in pids:
                if not self.polling:
                    break
                data = self.elm.query_pid(pid_cmd)
                if data:
                    self.latest[key] = data
                time.sleep(0.05)

            # Battery voltage (AT command, not PID)
            volts = self.elm._send_cmd("ATRV")
            if volts:
                self.latest["battery"] = volts.strip()

            time.sleep(0.1)

    def update_ui(self):
        """Update gauge displays from latest data"""
        if not self.elm.connected:
            return

        # Update connection stats
        self.conn_stats.setText(f"TX: {self.elm.tx_count} | RX: {self.elm.rx_count} | Errors: {self.elm.error_count}")

        # RPM
        data = self.latest.get("rpm")
        if data:
            val = OBD2PIDs.rpm(data)
            if val is not None:
                self.gauge_rpm.set_value(val)

        # Speed
        data = self.latest.get("speed")
        if data:
            val = OBD2PIDs.speed_mph(data)
            if val is not None:
                self.gauge_speed.set_value(val)

        # Coolant
        data = self.latest.get("coolant")
        if data:
            val = OBD2PIDs.coolant_temp(data)
            if val is not None:
                self.gauge_coolant.set_value(val)

        # Boost
        data = self.latest.get("boost")
        if data:
            val = OBD2PIDs.boost_pressure(data)
            if val is not None:
                self.gauge_boost.set_value(val)

        # Engine Load
        data = self.latest.get("load")
        if data:
            val = OBD2PIDs.engine_load(data)
            if val is not None:
                self.gauge_load.set_value(val)

        # Throttle
        data = self.latest.get("throttle")
        if data:
            val = OBD2PIDs.throttle(data)
            if val is not None:
                self.gauge_throttle.set_value(val)

        # Fuel Rail Pressure
        data = self.latest.get("fuel_rail")
        if data:
            val = OBD2PIDs.fuel_rail_pressure(data)
            if val is not None:
                self.gauge_fuel_rail.set_value(val)

        # MAF
        data = self.latest.get("maf")
        if data:
            val = OBD2PIDs.maf(data)
            if val is not None:
                self.gauge_maf.set_value(val)

        # Oil Temp
        data = self.latest.get("oil_temp")
        if data:
            val = OBD2PIDs.oil_temp(data)
            if val is not None:
                self.gauge_oil_temp.set_value(val)

        # EGT
        data = self.latest.get("egt")
        if data:
            val = OBD2PIDs.exhaust_temp_b1s1(data)
            if val is not None:
                self.gauge_egt.set_value(val)

        # Intake Temp
        data = self.latest.get("intake_temp")
        if data:
            val = OBD2PIDs.intake_temp(data)
            if val is not None:
                self.gauge_intake.set_value(val)

        # Timing
        data = self.latest.get("timing")
        if data:
            val = OBD2PIDs.timing_advance(data)
            if val is not None:
                self.gauge_timing.set_value(val)

        # Battery voltage
        bat = self.latest.get("battery")
        if bat:
            self.conn_voltage.setText(f"Battery: {bat}")

        # Logging
        if self.logging_active:
            timestamp = time.strftime("%H:%M:%S")
            rpm_data = self.latest.get("rpm")
            rpm_val = OBD2PIDs.rpm(rpm_data) if rpm_data else 0
            speed_data = self.latest.get("speed")
            speed_val = OBD2PIDs.speed_mph(speed_data) if speed_data else 0
            coolant_data = self.latest.get("coolant")
            coolant_val = OBD2PIDs.coolant_temp(coolant_data) if coolant_data else 0

            entry = {
                "time": timestamp,
                "rpm": rpm_val or 0,
                "speed": speed_val or 0,
                "coolant": coolant_val or 0,
            }
            self.log_data.append(entry)
            self.log_text.append(
                f"[{timestamp}] RPM: {entry['rpm']:.0f} | "
                f"Speed: {entry['speed']:.1f} mph | "
                f"Coolant: {entry['coolant']:.0f}°F"
            )

    def toggle_logging(self):
        self.logging_active = not self.logging_active
        if self.logging_active:
            self.log_btn.setText("⏹️ STOP LOGGING")
            self.log_btn.setStyleSheet("""
                QPushButton { background: #DC2626; color: white; padding: 10px 20px;
                             border-radius: 6px; font-weight: bold; }
            """)
            self.log_text.clear()
            self.log_data = []
        else:
            self.log_btn.setText("📝 START LOGGING")
            self.log_btn.setStyleSheet("""
                QPushButton { background: #2563EB; color: white; padding: 10px 20px;
                             border-radius: 6px; font-weight: bold; }
            """)

    def export_log(self):
        if not self.log_data:
            QMessageBox.information(self, "Export", "No data to export. Start logging first.")
            return
        path, _ = QFileDialog.getSaveFileName(self, "Save Log", "jetta_tdi_log.csv", "CSV Files (*.csv)")
        if path:
            with open(path, "w") as f:
                f.write("Time,RPM,Speed_MPH,Coolant_F\n")
                for entry in self.log_data:
                    f.write(f"{entry['time']},{entry['rpm']:.0f},{entry['speed']:.1f},{entry['coolant']:.0f}\n")
            QMessageBox.information(self, "Export", f"Log saved to:\n{path}")

    def closeEvent(self, event):
        self.polling = False
        self.elm.disconnect()
        event.accept()


# ═══════════════════════════════════════════════════════════════
#  LAUNCH
# ═══════════════════════════════════════════════════════════════

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    # Dark palette
    palette = QPalette()
    palette.setColor(QPalette.Window, QColor(15, 15, 26))
    palette.setColor(QPalette.WindowText, QColor(200, 200, 220))
    palette.setColor(QPalette.Base, QColor(26, 26, 46))
    palette.setColor(QPalette.AlternateBase, QColor(35, 35, 55))
    palette.setColor(QPalette.Text, QColor(200, 200, 220))
    palette.setColor(QPalette.Button, QColor(35, 35, 55))
    palette.setColor(QPalette.ButtonText, QColor(200, 200, 220))
    palette.setColor(QPalette.Highlight, QColor(37, 99, 235))
    app.setPalette(palette)

    window = OBD2Dashboard()
    window.show()
    sys.exit(app.exec_())
