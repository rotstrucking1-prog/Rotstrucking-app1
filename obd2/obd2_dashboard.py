#!/usr/bin/env python3
"""
ROTS Trucking — OBD2 Diagnostic Dashboard
Works with Vgate vLinker FS (ELM327) adapter
For: 2004 Kia Spectra EX 2.0L / 2004 Kia Sedona 3.5L V6
"""

import sys
import time
import serial
import serial.tools.list_ports
import threading
from datetime import datetime

try:
    from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                                  QHBoxLayout, QGridLayout, QLabel, QPushButton,
                                  QTextEdit, QGroupBox, QComboBox, QFrame,
                                  QProgressBar, QTabWidget, QTableWidget,
                                  QTableWidgetItem, QHeaderView, QMessageBox)
    from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject
    from PyQt5.QtGui import QFont, QColor, QPalette, QIcon
except ImportError:
    print("Installing PyQt5...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "PyQt5"])
    from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                                  QHBoxLayout, QGridLayout, QLabel, QPushButton,
                                  QTextEdit, QGroupBox, QComboBox, QFrame,
                                  QProgressBar, QTabWidget, QTableWidget,
                                  QTableWidgetItem, QHeaderView, QMessageBox)
    from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject
    from PyQt5.QtGui import QFont, QColor, QPalette, QIcon

try:
    import pyserial
except:
    pass

# ============================================================
# ELM327 OBD2 PROTOCOL
# ============================================================

# Standard OBD2 PIDs (Mode 01)
OBD2_PIDS = {
    "RPM":              {"pid": "010C", "bytes": 2, "formula": lambda a, b: ((a * 256) + b) / 4, "unit": "RPM", "min": 0, "max": 8000, "warn": 6500, "icon": "🔄"},
    "Speed":            {"pid": "010D", "bytes": 1, "formula": lambda a: a * 0.621371, "unit": "MPH", "min": 0, "max": 120, "warn": 85, "icon": "🚗"},
    "Coolant Temp":     {"pid": "0105", "bytes": 1, "formula": lambda a: (a - 40) * 9/5 + 32, "unit": "°F", "min": -40, "max": 280, "warn": 230, "icon": "🌡️"},
    "Intake Temp":      {"pid": "010F", "bytes": 1, "formula": lambda a: (a - 40) * 9/5 + 32, "unit": "°F", "min": -40, "max": 215, "warn": 180, "icon": "🌬️"},
    "Engine Load":      {"pid": "0104", "bytes": 1, "formula": lambda a: a * 100 / 255, "unit": "%", "min": 0, "max": 100, "warn": 90, "icon": "⚡"},
    "Throttle":         {"pid": "0111", "bytes": 1, "formula": lambda a: a * 100 / 255, "unit": "%", "min": 0, "max": 100, "warn": 95, "icon": "🦶"},
    "MAF Rate":         {"pid": "0110", "bytes": 2, "formula": lambda a, b: ((a * 256) + b) / 100, "unit": "g/s", "min": 0, "max": 655, "warn": 500, "icon": "💨"},
    "Fuel Pressure":    {"pid": "010A", "bytes": 1, "formula": lambda a: a * 3 * 0.145038, "unit": "PSI", "min": 0, "max": 115, "warn": 100, "icon": "⛽"},
    "Timing Advance":   {"pid": "010E", "bytes": 1, "formula": lambda a: (a / 2) - 64, "unit": "°", "min": -64, "max": 64, "warn": 50, "icon": "⏱️"},
    "Short Fuel Trim":  {"pid": "0106", "bytes": 1, "formula": lambda a: (a - 128) * 100 / 128, "unit": "%", "min": -100, "max": 100, "warn": 25, "icon": "📊"},
    "Long Fuel Trim":   {"pid": "0107", "bytes": 1, "formula": lambda a: (a - 128) * 100 / 128, "unit": "%", "min": -100, "max": 100, "warn": 25, "icon": "📈"},
    "Battery Voltage":  {"pid": "0142", "bytes": 2, "formula": lambda a, b: ((a * 256) + b) / 1000, "unit": "V", "min": 0, "max": 16, "warn": 15, "icon": "🔋"},
}

# DTC trouble code prefixes
DTC_PREFIX = {
    '0': 'P0', '1': 'P1', '2': 'P2', '3': 'P3',
    '4': 'C0', '5': 'C1', '6': 'C2', '7': 'C3',
    '8': 'B0', '9': 'B1', 'A': 'B2', 'B': 'B3',
    'C': 'U0', 'D': 'U1', 'E': 'U2', 'F': 'U3'
}


class ELM327:
    """ELM327 OBD2 adapter communication"""

    def __init__(self):
        self.ser = None
        self.connected = False
        self.port = None
        self.protocol_name = "Unknown"
        self.elm_version = "Unknown"
        self.voltage = "N/A"

    def find_port(self):
        """Find the ELM327 adapter port"""
        ports = serial.tools.list_ports.comports()
        for p in ports:
            desc = (p.description or "").lower()
            mfg = (p.manufacturer or "").lower()
            if any(x in desc for x in ["elm", "obd", "vlinker", "vgate", "ft232", "usb-serial", "usb serial"]):
                return p.device
            if any(x in mfg for x in ["ftdi", "vgate", "elm"]):
                return p.device
        # Fallback: try common ports
        for p in ports:
            if "COM" in p.device or "ttyUSB" in p.device:
                return p.device
        return None

    def connect(self, port=None):
        """Connect to ELM327 adapter"""
        if port is None:
            port = self.find_port()
        if port is None:
            return False, "No OBD2 adapter found. Check USB connection."

        self.port = port
        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=38400,
                timeout=2,
                write_timeout=2
            )
            time.sleep(1)

            # Try 38400 first (vLinker FS default), then 9600, then 115200
            for baud in [38400, 9600, 115200]:
                self.ser.baudrate = baud
                self.ser.flushInput()
                self.ser.flushOutput()

                # Reset ELM327
                resp = self._send_cmd("ATZ", timeout=3)
                if "ELM" in resp.upper():
                    self.elm_version = resp.strip()
                    break
            else:
                self.ser.close()
                return False, f"Adapter on {port} not responding. Is the key ON?"

            # Configure adapter
            self._send_cmd("ATE0")       # Echo off
            self._send_cmd("ATL0")       # Linefeeds off
            self._send_cmd("ATS0")       # Spaces off
            self._send_cmd("ATH0")       # Headers off
            self._send_cmd("ATSP0")      # Auto-detect protocol

            # Get voltage
            v = self._send_cmd("ATRV")
            if v and "V" in v.upper():
                self.voltage = v.strip()

            # Try a test read to confirm vehicle connection
            resp = self._send_cmd("0100", timeout=5)  # Supported PIDs
            if "41" in resp:
                self.connected = True
                # Get protocol name
                prot = self._send_cmd("ATDPN")
                prot_names = {
                    "1": "SAE J1850 PWM", "2": "SAE J1850 VPW",
                    "3": "ISO 9141-2", "4": "ISO 14230 KWP (5-baud)",
                    "5": "ISO 14230 KWP (fast)", "6": "ISO 15765 CAN (11-bit, 500kb)",
                    "7": "ISO 15765 CAN (29-bit, 500kb)", "8": "ISO 15765 CAN (11-bit, 250kb)",
                    "9": "ISO 15765 CAN (29-bit, 250kb)", "A": "SAE J1939 CAN"
                }
                prot_num = prot.strip().replace("A", "").replace("a", "")
                if len(prot_num) > 0:
                    self.protocol_name = prot_names.get(prot_num[-1], f"Protocol {prot_num}")
                return True, f"Connected on {port} ({self.elm_version})"
            else:
                # Still connected to adapter but no vehicle
                if "UNABLE TO CONNECT" in resp.upper() or "NO DATA" in resp.upper():
                    self.ser.close()
                    return False, f"Adapter found on {port} but can't reach vehicle ECU. Is the key ON?"
                self.connected = True
                return True, f"Connected on {port} ({self.elm_version}) — testing PIDs..."

        except serial.SerialException as e:
            return False, f"Port {port} error: {str(e)}"
        except Exception as e:
            return False, f"Connection error: {str(e)}"

    def _send_cmd(self, cmd, timeout=2):
        """Send AT/OBD command and get response"""
        if not self.ser or not self.ser.is_open:
            return ""
        try:
            self.ser.flushInput()
            self.ser.write((cmd + "\r").encode())
            time.sleep(0.1)

            response = ""
            end_time = time.time() + timeout
            while time.time() < end_time:
                if self.ser.in_waiting:
                    chunk = self.ser.read(self.ser.in_waiting).decode("ascii", errors="ignore")
                    response += chunk
                    if ">" in response:
                        break
                time.sleep(0.05)

            # Clean response
            response = response.replace(">", "").replace("\r", " ").replace("\n", " ").strip()
            # Remove echo
            if response.startswith(cmd):
                response = response[len(cmd):].strip()
            return response
        except Exception:
            return ""

    def read_pid(self, pid_hex):
        """Read a single OBD2 PID and return raw hex bytes"""
        resp = self._send_cmd(pid_hex, timeout=2)
        if not resp or "NO DATA" in resp.upper() or "ERROR" in resp.upper() or "UNABLE" in resp.upper():
            return None

        # Parse hex bytes from response
        # Response format: "41 0C 1A F8" (mode+1, pid, data bytes)
        clean = resp.replace(" ", "").upper()

        # Find the response marker (41 for mode 01 response)
        idx = clean.find("41")
        if idx == -1:
            return None

        hex_data = clean[idx:]
        try:
            byte_list = [int(hex_data[i:i+2], 16) for i in range(0, len(hex_data), 2)]
            # Skip mode byte (41) and PID byte
            return byte_list[2:]  # data bytes only
        except (ValueError, IndexError):
            return None

    def read_dtcs(self):
        """Read diagnostic trouble codes"""
        resp = self._send_cmd("03", timeout=3)
        if not resp or "NO DATA" in resp.upper():
            return []

        clean = resp.replace(" ", "").upper()
        idx = clean.find("43")
        if idx == -1:
            return []

        hex_data = clean[idx+2:]
        codes = []
        for i in range(0, len(hex_data) - 3, 4):
            code_hex = hex_data[i:i+4]
            if code_hex == "0000":
                continue
            first_char = code_hex[0]
            prefix = DTC_PREFIX.get(first_char, "P0")
            code = prefix + code_hex[1:]
            codes.append(code)
        return codes

    def clear_dtcs(self):
        """Clear diagnostic trouble codes"""
        resp = self._send_cmd("04", timeout=3)
        return "44" in resp.upper() or "OK" in resp.upper()

    def get_vin(self):
        """Try to read VIN"""
        resp = self._send_cmd("0902", timeout=5)
        if not resp or "NO DATA" in resp.upper():
            return "Not available"
        # VIN decoding is complex with multi-frame — return raw for now
        clean = resp.replace(" ", "")
        try:
            # Try to extract ASCII from hex
            idx = clean.find("4902")
            if idx >= 0:
                hex_str = clean[idx+6:]
                vin = ""
                for i in range(0, len(hex_str)-1, 2):
                    c = int(hex_str[i:i+2], 16)
                    if 32 <= c <= 126:
                        vin += chr(c)
                return vin if len(vin) >= 10 else "Not available"
        except:
            pass
        return "Not available"

    def disconnect(self):
        """Disconnect from adapter"""
        if self.ser and self.ser.is_open:
            try:
                self._send_cmd("ATZ")
                self.ser.close()
            except:
                pass
        self.connected = False


# ============================================================
# GUI
# ============================================================

class DataSignal(QObject):
    update = pyqtSignal(dict)
    log = pyqtSignal(str)
    status = pyqtSignal(str, str)  # message, color


class GaugeWidget(QFrame):
    """Single gauge display"""

    def __init__(self, name, unit, min_val, max_val, warn_val, icon=""):
        super().__init__()
        self.name = name
        self.unit = unit
        self.min_val = min_val
        self.max_val = max_val
        self.warn_val = warn_val
        self.value = 0.0
        self.supported = True

        self.setFrameStyle(QFrame.Box | QFrame.Raised)
        self.setLineWidth(2)
        self.setStyleSheet("""
            QFrame {
                background-color: #1a1a2e;
                border: 2px solid #16213e;
                border-radius: 10px;
            }
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)

        # Icon + Name
        header = QLabel(f"{icon} {name}")
        header.setFont(QFont("Segoe UI", 10, QFont.Bold))
        header.setStyleSheet("color: #a0a0d0; border: none;")
        header.setAlignment(Qt.AlignCenter)
        layout.addWidget(header)

        # Value
        self.value_label = QLabel("--")
        self.value_label.setFont(QFont("Consolas", 28, QFont.Bold))
        self.value_label.setStyleSheet("color: #00ff88; border: none;")
        self.value_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.value_label)

        # Unit
        unit_label = QLabel(unit)
        unit_label.setFont(QFont("Segoe UI", 9))
        unit_label.setStyleSheet("color: #6060a0; border: none;")
        unit_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(unit_label)

        # Progress bar
        self.bar = QProgressBar()
        self.bar.setMinimum(0)
        self.bar.setMaximum(100)
        self.bar.setValue(0)
        self.bar.setTextVisible(False)
        self.bar.setFixedHeight(8)
        self.bar.setStyleSheet("""
            QProgressBar {
                background-color: #0d1117;
                border: 1px solid #30363d;
                border-radius: 4px;
            }
            QProgressBar::chunk {
                background-color: #00ff88;
                border-radius: 3px;
            }
        """)
        layout.addWidget(self.bar)

    def update_value(self, val):
        """Update gauge with new value"""
        if val is None:
            self.value_label.setText("N/A")
            self.value_label.setStyleSheet("color: #555555; border: none;")
            self.supported = False
            return

        self.supported = True
        self.value = val

        # Format value
        if abs(val) >= 1000:
            self.value_label.setText(f"{val:,.0f}")
        elif abs(val) >= 100:
            self.value_label.setText(f"{val:.0f}")
        elif abs(val) >= 10:
            self.value_label.setText(f"{val:.1f}")
        else:
            self.value_label.setText(f"{val:.2f}")

        # Color based on warning threshold
        if self.warn_val > 0 and val >= self.warn_val:
            color = "#ff4444"
            bar_color = "#ff4444"
        elif self.warn_val > 0 and val >= self.warn_val * 0.85:
            color = "#ffaa00"
            bar_color = "#ffaa00"
        else:
            color = "#00ff88"
            bar_color = "#00ff88"

        self.value_label.setStyleSheet(f"color: {color}; border: none;")
        self.bar.setStyleSheet(f"""
            QProgressBar {{
                background-color: #0d1117;
                border: 1px solid #30363d;
                border-radius: 4px;
            }}
            QProgressBar::chunk {{
                background-color: {bar_color};
                border-radius: 3px;
            }}
        """)

        # Update progress bar
        if self.max_val > self.min_val:
            pct = max(0, min(100, int((val - self.min_val) / (self.max_val - self.min_val) * 100)))
            self.bar.setValue(pct)


class OBD2Dashboard(QMainWindow):
    """Main OBD2 Dashboard Window"""

    def __init__(self):
        super().__init__()
        self.elm = ELM327()
        self.signals = DataSignal()
        self.running = False
        self.gauges = {}
        self.reading_thread = None
        self.read_count = 0
        self.error_count = 0
        self.start_time = None
        self.dtc_codes = []

        self.setWindowTitle("🚗 ROTS Trucking — OBD2 Dashboard")
        self.setMinimumSize(1000, 700)
        self.setStyleSheet("""
            QMainWindow { background-color: #0d1117; }
            QTabWidget::pane { border: 1px solid #30363d; background-color: #0d1117; }
            QTabBar::tab { background: #161b22; color: #c9d1d9; padding: 8px 16px; border: 1px solid #30363d; }
            QTabBar::tab:selected { background: #1a1a2e; color: #00ff88; border-bottom: 2px solid #00ff88; }
        """)

        # Signals
        self.signals.update.connect(self._handle_update)
        self.signals.log.connect(self._handle_log)
        self.signals.status.connect(self._handle_status)

        self._build_ui()

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(10, 10, 10, 10)

        # Header
        header = QLabel("🚗 ROTS TRUCKING — OBD2 LIVE DASHBOARD")
        header.setFont(QFont("Segoe UI", 16, QFont.Bold))
        header.setStyleSheet("color: #00ff88; padding: 5px;")
        header.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(header)

        # Connection bar
        conn_bar = QHBoxLayout()

        self.port_combo = QComboBox()
        self.port_combo.setStyleSheet("background: #161b22; color: #c9d1d9; padding: 5px; border: 1px solid #30363d;")
        self.port_combo.setMinimumWidth(200)
        self._refresh_ports()
        conn_bar.addWidget(QLabel("Port:"))
        conn_bar.addWidget(self.port_combo)

        refresh_btn = QPushButton("🔄 Refresh")
        refresh_btn.setStyleSheet("background: #161b22; color: #c9d1d9; padding: 5px 10px; border: 1px solid #30363d;")
        refresh_btn.clicked.connect(self._refresh_ports)
        conn_bar.addWidget(refresh_btn)

        self.connect_btn = QPushButton("🔌 CONNECT")
        self.connect_btn.setStyleSheet("background: #238636; color: white; padding: 8px 20px; font-weight: bold; border-radius: 5px;")
        self.connect_btn.clicked.connect(self._toggle_connection)
        conn_bar.addWidget(self.connect_btn)

        conn_bar.addStretch()

        self.status_label = QLabel("⚪ Not connected")
        self.status_label.setFont(QFont("Segoe UI", 10))
        self.status_label.setStyleSheet("color: #8b949e;")
        conn_bar.addWidget(self.status_label)

        # Style the port label
        for i in range(conn_bar.count()):
            w = conn_bar.itemAt(i).widget()
            if isinstance(w, QLabel) and w != self.status_label:
                w.setStyleSheet("color: #c9d1d9;")

        main_layout.addLayout(conn_bar)

        # Tabs
        tabs = QTabWidget()
        main_layout.addWidget(tabs)

        # Tab 1: Live Gauges
        gauge_tab = QWidget()
        gauge_layout = QGridLayout(gauge_tab)
        gauge_layout.setSpacing(8)

        row, col = 0, 0
        for name, info in OBD2_PIDS.items():
            gauge = GaugeWidget(name, info["unit"], info["min"], info["max"], info["warn"], info.get("icon", ""))
            self.gauges[name] = gauge
            gauge_layout.addWidget(gauge, row, col)
            col += 1
            if col >= 4:
                col = 0
                row += 1

        tabs.addTab(gauge_tab, "📊 Live Gauges")

        # Tab 2: Trouble Codes
        dtc_tab = QWidget()
        dtc_layout = QVBoxLayout(dtc_tab)

        dtc_btn_row = QHBoxLayout()
        scan_btn = QPushButton("🔍 Scan for Codes")
        scan_btn.setStyleSheet("background: #1f6feb; color: white; padding: 8px 20px; font-weight: bold; border-radius: 5px;")
        scan_btn.clicked.connect(self._scan_dtcs)
        dtc_btn_row.addWidget(scan_btn)

        clear_btn = QPushButton("🗑️ Clear All Codes")
        clear_btn.setStyleSheet("background: #da3633; color: white; padding: 8px 20px; font-weight: bold; border-radius: 5px;")
        clear_btn.clicked.connect(self._clear_dtcs)
        dtc_btn_row.addWidget(clear_btn)
        dtc_btn_row.addStretch()
        dtc_layout.addLayout(dtc_btn_row)

        self.dtc_table = QTableWidget()
        self.dtc_table.setColumnCount(2)
        self.dtc_table.setHorizontalHeaderLabels(["Code", "Type"])
        self.dtc_table.horizontalHeader().setStretchLastSection(True)
        self.dtc_table.setStyleSheet("""
            QTableWidget { background: #161b22; color: #c9d1d9; gridline-color: #30363d; border: 1px solid #30363d; }
            QHeaderView::section { background: #1a1a2e; color: #a0a0d0; padding: 5px; border: 1px solid #30363d; }
        """)
        dtc_layout.addWidget(self.dtc_table)

        self.dtc_status = QLabel("Press 'Scan for Codes' to check for trouble codes")
        self.dtc_status.setStyleSheet("color: #8b949e; padding: 5px;")
        dtc_layout.addWidget(self.dtc_status)

        tabs.addTab(dtc_tab, "⚠️ Trouble Codes")

        # Tab 3: Vehicle Info
        info_tab = QWidget()
        info_layout = QVBoxLayout(info_tab)

        self.info_table = QTableWidget()
        self.info_table.setColumnCount(2)
        self.info_table.setHorizontalHeaderLabels(["Property", "Value"])
        self.info_table.horizontalHeader().setStretchLastSection(True)
        self.info_table.setStyleSheet("""
            QTableWidget { background: #161b22; color: #c9d1d9; gridline-color: #30363d; border: 1px solid #30363d; }
            QHeaderView::section { background: #1a1a2e; color: #a0a0d0; padding: 5px; border: 1px solid #30363d; }
        """)
        info_layout.addWidget(self.info_table)

        tabs.addTab(info_tab, "ℹ️ Vehicle Info")

        # Tab 4: Log
        log_tab = QWidget()
        log_layout = QVBoxLayout(log_tab)

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setStyleSheet("""
            QTextEdit {
                background-color: #0d1117;
                color: #c9d1d9;
                font-family: Consolas;
                font-size: 11px;
                border: 1px solid #30363d;
            }
        """)
        log_layout.addWidget(self.log_text)
        tabs.addTab(log_tab, "📋 Log")

        # Connection info bar at bottom
        bottom_bar = QHBoxLayout()
        self.conn_info = QLabel("Adapter: -- | Protocol: -- | Voltage: --")
        self.conn_info.setStyleSheet("color: #6060a0; font-size: 10px;")
        bottom_bar.addWidget(self.conn_info)

        bottom_bar.addStretch()

        self.stats_label = QLabel("Reads: 0 | Errors: 0 | Uptime: 0s")
        self.stats_label.setStyleSheet("color: #6060a0; font-size: 10px;")
        bottom_bar.addWidget(self.stats_label)

        main_layout.addLayout(bottom_bar)

        # Stats timer
        self.stats_timer = QTimer()
        self.stats_timer.timeout.connect(self._update_stats)
        self.stats_timer.start(1000)

    def _refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.port_combo.addItem(f"{p.device} — {p.description}", p.device)
        if self.port_combo.count() == 0:
            self.port_combo.addItem("No ports found", None)

    def _toggle_connection(self):
        if self.running:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self.port_combo.currentData()
        self.signals.log.emit(f"Connecting to {port}...")
        self.signals.status.emit("🟡 Connecting...", "#ffaa00")

        def do_connect():
            success, msg = self.elm.connect(port)
            if success:
                self.signals.status.emit(f"🟢 {msg}", "#00ff88")
                self.signals.log.emit(f"✅ {msg}")
                self.running = True
                self.start_time = time.time()

                # Update vehicle info
                self._fetch_vehicle_info()

                # Start reading loop
                self._read_loop()
            else:
                self.signals.status.emit(f"🔴 {msg}", "#ff4444")
                self.signals.log.emit(f"❌ {msg}")

        self.reading_thread = threading.Thread(target=do_connect, daemon=True)
        self.reading_thread.start()

        self.connect_btn.setText("⏹️ DISCONNECT")
        self.connect_btn.setStyleSheet("background: #da3633; color: white; padding: 8px 20px; font-weight: bold; border-radius: 5px;")

    def _disconnect(self):
        self.running = False
        time.sleep(0.5)
        self.elm.disconnect()
        self.signals.status.emit("⚪ Disconnected", "#8b949e")
        self.signals.log.emit("Disconnected from adapter")
        self.connect_btn.setText("🔌 CONNECT")
        self.connect_btn.setStyleSheet("background: #238636; color: white; padding: 8px 20px; font-weight: bold; border-radius: 5px;")

    def _read_loop(self):
        """Continuous PID reading loop"""
        while self.running:
            data = {}
            for name, info in OBD2_PIDS.items():
                if not self.running:
                    break
                raw = self.elm.read_pid(info["pid"])
                if raw is not None:
                    try:
                        if info["bytes"] == 1 and len(raw) >= 1:
                            val = info["formula"](raw[0])
                        elif info["bytes"] == 2 and len(raw) >= 2:
                            val = info["formula"](raw[0], raw[1])
                        else:
                            val = None
                        data[name] = val
                        self.read_count += 1
                    except Exception:
                        data[name] = None
                        self.error_count += 1
                else:
                    data[name] = None

            if data:
                self.signals.update.emit(data)

            time.sleep(0.1)

    def _fetch_vehicle_info(self):
        """Fetch vehicle info and update info tab"""
        info_items = [
            ("Adapter", self.elm.elm_version),
            ("Port", self.elm.port or "N/A"),
            ("Protocol", self.elm.protocol_name),
            ("Battery Voltage", self.elm.voltage),
        ]

        vin = self.elm.get_vin()
        info_items.append(("VIN", vin))

        # Update connection bar
        self.signals.log.emit(f"Protocol: {self.elm.protocol_name}")
        self.signals.log.emit(f"Voltage: {self.elm.voltage}")
        self.signals.log.emit(f"VIN: {vin}")

        # Update info table on main thread
        def update_table():
            self.info_table.setRowCount(len(info_items))
            for i, (prop, val) in enumerate(info_items):
                self.info_table.setItem(i, 0, QTableWidgetItem(prop))
                self.info_table.setItem(i, 1, QTableWidgetItem(str(val)))
            self.conn_info.setText(f"Adapter: {self.elm.elm_version} | Protocol: {self.elm.protocol_name} | Voltage: {self.elm.voltage}")

        QTimer.singleShot(0, update_table)

    def _scan_dtcs(self):
        if not self.elm.connected:
            self.dtc_status.setText("❌ Not connected to vehicle")
            return

        def do_scan():
            codes = self.elm.read_dtcs()
            self.dtc_codes = codes

            def update_ui():
                if not codes:
                    self.dtc_table.setRowCount(1)
                    self.dtc_table.setItem(0, 0, QTableWidgetItem("✅ No codes"))
                    self.dtc_table.setItem(0, 1, QTableWidgetItem("All clear!"))
                    self.dtc_status.setText("✅ No trouble codes found — engine is clean!")
                    self.dtc_status.setStyleSheet("color: #00ff88; padding: 5px;")
                else:
                    self.dtc_table.setRowCount(len(codes))
                    for i, code in enumerate(codes):
                        self.dtc_table.setItem(i, 0, QTableWidgetItem(code))
                        if code.startswith("P"):
                            ctype = "Powertrain"
                        elif code.startswith("C"):
                            ctype = "Chassis"
                        elif code.startswith("B"):
                            ctype = "Body"
                        elif code.startswith("U"):
                            ctype = "Network"
                        else:
                            ctype = "Unknown"
                        self.dtc_table.setItem(i, 1, QTableWidgetItem(ctype))
                    self.dtc_status.setText(f"⚠️ Found {len(codes)} trouble code(s)")
                    self.dtc_status.setStyleSheet("color: #ffaa00; padding: 5px;")

            QTimer.singleShot(0, update_ui)

        threading.Thread(target=do_scan, daemon=True).start()
        self.dtc_status.setText("🔍 Scanning...")

    def _clear_dtcs(self):
        if not self.elm.connected:
            self.dtc_status.setText("❌ Not connected to vehicle")
            return

        reply = QMessageBox.question(self, "Clear Codes",
            "Are you sure you want to clear all trouble codes?\nThis will also turn off the Check Engine light.",
            QMessageBox.Yes | QMessageBox.No)
        if reply != QMessageBox.Yes:
            return

        def do_clear():
            ok = self.elm.clear_dtcs()
            def update_ui():
                if ok:
                    self.dtc_table.setRowCount(0)
                    self.dtc_status.setText("✅ Codes cleared! Check Engine light should turn off.")
                    self.dtc_status.setStyleSheet("color: #00ff88; padding: 5px;")
                    self.signals.log.emit("✅ DTC codes cleared")
                else:
                    self.dtc_status.setText("❌ Failed to clear codes")
                    self.dtc_status.setStyleSheet("color: #ff4444; padding: 5px;")
            QTimer.singleShot(0, update_ui)

        threading.Thread(target=do_clear, daemon=True).start()

    def _handle_update(self, data):
        for name, val in data.items():
            if name in self.gauges:
                self.gauges[name].update_value(val)

    def _handle_log(self, msg):
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.append(f"[{timestamp}] {msg}")

    def _handle_status(self, msg, color):
        self.status_label.setText(msg)
        self.status_label.setStyleSheet(f"color: {color};")

    def _update_stats(self):
        uptime = 0
        if self.start_time and self.running:
            uptime = int(time.time() - self.start_time)
        mins, secs = divmod(uptime, 60)
        self.stats_label.setText(f"Reads: {self.read_count:,} | Errors: {self.error_count} | Uptime: {mins}m {secs}s")

    def closeEvent(self, event):
        self.running = False
        self.elm.disconnect()
        event.accept()


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    # Dark palette
    palette = QPalette()
    palette.setColor(QPalette.Window, QColor(13, 17, 23))
    palette.setColor(QPalette.WindowText, QColor(201, 209, 217))
    palette.setColor(QPalette.Base, QColor(22, 27, 34))
    palette.setColor(QPalette.AlternateBase, QColor(13, 17, 23))
    palette.setColor(QPalette.ToolTipBase, QColor(22, 27, 34))
    palette.setColor(QPalette.ToolTipText, QColor(201, 209, 217))
    palette.setColor(QPalette.Text, QColor(201, 209, 217))
    palette.setColor(QPalette.Button, QColor(22, 27, 34))
    palette.setColor(QPalette.ButtonText, QColor(201, 209, 217))
    palette.setColor(QPalette.BrightText, QColor(0, 255, 136))
    palette.setColor(QPalette.Link, QColor(31, 111, 235))
    palette.setColor(QPalette.Highlight, QColor(31, 111, 235))
    palette.setColor(QPalette.HighlightedText, QColor(255, 255, 255))
    app.setPalette(palette)

    window = OBD2Dashboard()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
