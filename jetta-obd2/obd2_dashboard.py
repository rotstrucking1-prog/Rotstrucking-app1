#!/usr/bin/env python3
"""
===================================================================
  OBD2 DASHBOARD — 2009 VW Jetta TDI 2.0L
  Built for ROTS TRUCKING LLC
  Adapter: Vgate vLinker FS (ELM327 USB)
  Protocol: 6 (ISO 15765-4 CAN 11-bit/500k) — CONFIRMED
  PIDs: 19 confirmed via diagnostic scan May 2 2026
===================================================================
"""

import sys
import os
import time
import threading
import serial
import serial.tools.list_ports

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

# ===================================================================
#  CONFIRMED PIDs from diagnostic scan (Protocol 6)
# ===================================================================
# 0x04 - Engine Load %           0x05 - Coolant Temp
# 0x0B - Intake Manifold (Boost) 0x0C - RPM
# 0x0D - Speed                   0x0F - Intake Air Temp
# 0x10 - MAF Air Flow            0x1F - Runtime Since Start
# 0x21 - Distance w/ MIL         0x31 - Distance Since Clear
# 0x33 - Barometric Pressure     0x42 - Control Module Voltage
# 0x46 - Ambient Air Temp        0x4C - Commanded Throttle
# ===================================================================


class ELM327:
    """ELM327 adapter communication — locked to Protocol 6"""

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
        ports = serial.tools.list_ports.comports()
        for p in ports:
            desc = (p.description or "").lower()
            mfg = (p.manufacturer or "").lower()
            if any(x in desc for x in ["elm", "obd", "vgate", "vlinker", "usb-serial", "ch340", "ft232", "cp210"]):
                return p.device
            if any(x in mfg for x in ["elm", "vgate", "wch", "ftdi", "silicon"]):
                return p.device
        for p in ports:
            if "COM" in p.device or "ttyUSB" in p.device or "ttyACM" in p.device:
                return p.device
        return None

    def connect(self, port=None):
        if port is None:
            port = self.find_port()
        if port is None:
            return False, "No OBD2 adapter found. Is the USB cable plugged in?"

        self.port = port
        log = []

        # Confirmed working: 115200 baud with Protocol 6
        baud_rates = [115200, 38400, 9600, 57600, 230400, 500000]

        for baud in baud_rates:
            log.append(f"Trying {port} @ {baud}...")
            try:
                if self.serial and self.serial.is_open:
                    self.serial.close()
                    time.sleep(0.3)

                self.serial = serial.Serial(
                    port=port, baudrate=baud, timeout=2, write_timeout=2,
                    bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE
                )
                time.sleep(0.5)
                self.serial.flushInput()
                self.serial.flushOutput()

                resp = self._send_cmd("ATZ", delay=2.0)
                if not resp or ("ELM" not in resp.upper() and "OK" not in resp.upper()):
                    continue

                log.append(f"  Adapter found at {baud}!")
                time.sleep(0.3)

                self._send_cmd("ATE0")
                ver = self._send_cmd("ATI")
                if ver:
                    self.elm_version = ver.strip()
                self._send_cmd("ATL0")
                self._send_cmd("ATS0")
                self._send_cmd("ATH0")
                self._send_cmd("ATAT2")
                self._send_cmd("ATST96")

                # Force Protocol 6 — confirmed working
                self._send_cmd("ATSP6")
                time.sleep(0.3)

                test = self._send_cmd("0100", delay=2.0)
                if test and "UNABLE" not in test.upper() and "NO DATA" not in test.upper() and "?" not in test:
                    self.connected = True
                    self.protocol = "ISO 15765-4 (CAN 11/500)"

                    volts = self._send_cmd("ATRV")
                    if volts:
                        self.voltage = volts.strip()

                    return True, f"Connected on {port} ({self.elm_version}) — Protocol 6 CAN"

                # Fallback: try auto-detect
                self._send_cmd("ATSP0")
                time.sleep(0.3)
                test = self._send_cmd("0100", delay=2.0)
                if test and "UNABLE" not in test.upper() and "NO DATA" not in test.upper() and "?" not in test:
                    self.connected = True
                    proto = self._send_cmd("ATDPN")
                    if proto:
                        pn = proto.strip().replace("A", "")
                        names = {"6": "CAN 11/500", "7": "CAN 29/500", "8": "CAN 11/250", "9": "CAN 29/250"}
                        self.protocol = names.get(pn, f"Protocol {pn}")
                    volts = self._send_cmd("ATRV")
                    if volts:
                        self.voltage = volts.strip()
                    return True, f"Connected on {port} ({self.elm_version}) — {self.protocol}"

            except Exception as e:
                log.append(f"  Error: {e}")
                continue

        return False, "Adapter not responding.\n\n" + "\n".join(log)

    def _send_cmd(self, cmd, delay=0.01):
        if not self.serial or not self.serial.is_open:
            return None
        try:
            self.serial.flushInput()
            self.serial.write((cmd + "\r").encode())
            self.tx_count += 1
            time.sleep(delay)

            response = b""
            timeout = time.time() + 0.15
            while time.time() < timeout:
                if self.serial.in_waiting > 0:
                    chunk = self.serial.read(self.serial.in_waiting)
                    response += chunk
                    if b">" in chunk:
                        break
                time.sleep(0.005)

            text = response.decode("ascii", errors="ignore")
            text = text.replace("\r", "\n").replace(">", "").strip()
            lines = [l.strip() for l in text.split("\n") if l.strip() and l.strip() != cmd]
            result = "\n".join(lines)
            if result and "ERROR" not in result.upper():
                self.rx_count += 1
            return result
        except Exception:
            self.error_count += 1
            return None

    def query_pid(self, pid_hex):
        resp = self._send_cmd(pid_hex, delay=0.01)
        if resp is None:
            return None
        resp = resp.upper().replace(" ", "")
        for line in resp.split("\n"):
            line = line.strip()
            if line.startswith("41") or line.startswith("61"):
                try:
                    return bytes.fromhex(line)
                except:
                    pass
        return None

    def get_dtc(self):
        resp = self._send_cmd("03", delay=1.0)
        if resp is None:
            return []
        codes = []
        resp = resp.upper().replace(" ", "")
        for line in resp.split("\n"):
            line = line.strip()
            if line.startswith("43"):
                data = line[2:]
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
        return self._send_cmd("04", delay=2.0)

    def get_vin(self):
        resp = self._send_cmd("0902", delay=2.0)
        if resp is None:
            return "N/A"
        resp = resp.upper().replace(" ", "")
        hex_data = ""
        for line in resp.split("\n"):
            if len(line.strip()) >= 4:
                hex_data += line.strip()
        vin = ""
        try:
            raw = bytes.fromhex(hex_data)
            for b in raw:
                if 0x20 <= b <= 0x7E:
                    vin += chr(b)
        except:
            pass
        if len(vin) >= 17:
            for i in range(len(vin) - 16):
                candidate = vin[i:i+17]
                if all(c.isalnum() for c in candidate):
                    return candidate
        return vin if vin else "N/A"

    def disconnect(self):
        if self.serial and self.serial.is_open:
            try:
                self._send_cmd("ATZ")
                self.serial.close()
            except:
                pass
        self.connected = False


# ===================================================================
#  PID PARSERS — ONLY CONFIRMED WORKING PIDs
# ===================================================================

def parse_rpm(data):
    """PID 0x0C — RPM"""
    if data and len(data) >= 4:
        return ((data[2] * 256) + data[3]) / 4.0
    return None

def parse_speed_mph(data):
    """PID 0x0D — Speed (MPH)"""
    if data and len(data) >= 3:
        return data[2] * 0.621371
    return None

def parse_coolant_f(data):
    """PID 0x05 — Coolant Temp (F)"""
    if data and len(data) >= 3:
        return (data[2] - 40) * 9.0 / 5.0 + 32
    return None

def parse_load(data):
    """PID 0x04 — Engine Load %"""
    if data and len(data) >= 3:
        return data[2] * 100.0 / 255.0
    return None

def parse_boost_psi(data):
    """PID 0x0B — Intake Manifold / Boost (PSI)"""
    if data and len(data) >= 3:
        return data[2] * 0.145038  # kPa to PSI
    return None

def parse_maf(data):
    """PID 0x10 — MAF (g/s)"""
    if data and len(data) >= 4:
        return ((data[2] * 256) + data[3]) / 100.0
    return None

def parse_intake_temp_f(data):
    """PID 0x0F — Intake Air Temp (F)"""
    if data and len(data) >= 3:
        return (data[2] - 40) * 9.0 / 5.0 + 32
    return None

def parse_voltage(data):
    """PID 0x42 — Control Module Voltage (V)"""
    if data and len(data) >= 4:
        return ((data[2] * 256) + data[3]) / 1000.0
    return None

def parse_ambient_f(data):
    """PID 0x46 — Ambient Air Temp (F)"""
    if data and len(data) >= 3:
        return (data[2] - 40) * 9.0 / 5.0 + 32
    return None

def parse_cmd_throttle(data):
    """PID 0x4C — Commanded Throttle Actuator %"""
    if data and len(data) >= 3:
        return data[2] * 100.0 / 255.0
    return None

def parse_baro_psi(data):
    """PID 0x33 — Barometric Pressure (PSI)"""
    if data and len(data) >= 3:
        return data[2] * 0.145038
    return None

def parse_runtime_min(data):
    """PID 0x1F — Runtime Since Start (minutes)"""
    if data and len(data) >= 4:
        return ((data[2] * 256) + data[3]) / 60.0
    return None


# ===================================================================
#  GAUGE DEFINITIONS — 12 CONFIRMED GAUGES
# ===================================================================

GAUGES = [
    # (title, unit, pid_cmd, parser, min, max, warn, danger, decimals)
    ("RPM",             "rpm",  "010C", parse_rpm,          0, 5000, 4000, 4500, 0),
    ("Speed",           "mph",  "010D", parse_speed_mph,    0, 120,  None, None, 0),
    ("Boost",           "PSI",  "010B", parse_boost_psi,    0, 40,   30,   35,   1),
    ("Coolant Temp",    "\u00b0F",   "0105", parse_coolant_f,    100, 260, 220,  240,  0),
    ("Engine Load",     "%",    "0104", parse_load,          0, 100,  85,   95,   1),
    ("MAF Flow",        "g/s",  "0110", parse_maf,           0, 200,  None, None, 1),
    ("Intake Temp",     "\u00b0F",   "010F", parse_intake_temp_f, -20, 180, 140,  160,  0),
    ("Battery",         "V",    "0142", parse_voltage,       10, 16,  None, None, 1),
    ("Ambient Temp",    "\u00b0F",   "0146", parse_ambient_f,    -20, 130, None, None, 0),
    ("Cmd Throttle",    "%",    "014C", parse_cmd_throttle,  0, 100,  None, None, 1),
    ("Barometric",      "PSI",  "0133", parse_baro_psi,      12, 16,  None, None, 2),
    ("Runtime",         "min",  "011F", parse_runtime_min,   0, 600,  None, None, 1),
]


# ===================================================================
#  GAUGE WIDGET
# ===================================================================

class GaugeWidget(QWidget):
    def __init__(self, title, unit, min_val, max_val, warn_val=None, danger_val=None, decimals=0, parent=None):
        super().__init__(parent)
        self.title = title
        self.unit = unit
        self.min_val = min_val
        self.max_val = max_val
        self.warn_val = warn_val
        self.danger_val = danger_val
        self.decimals = decimals
        self.value = None
        self.setMinimumSize(160, 160)

    def set_value(self, val):
        self.value = val
        self.update()

    def paintEvent(self, event):
        w = self.width()
        h = self.height()
        side = min(w, h) - 10
        cx, cy = w // 2, h // 2

        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.fillRect(self.rect(), QColor(30, 30, 30))

        # Gauge arc background
        rect = QRectF(cx - side//2 + 10, cy - side//2 + 10, side - 20, side - 20)
        pen = QPen(QColor(60, 60, 60), 8, Qt.SolidLine, Qt.RoundCap)
        p.setPen(pen)
        p.drawArc(rect, 225 * 16, -270 * 16)

        # Value arc
        if self.value is not None:
            frac = max(0, min(1, (self.value - self.min_val) / max(1, self.max_val - self.min_val)))
            color = QColor(0, 200, 80)  # green
            if self.danger_val is not None and self.value >= self.danger_val:
                color = QColor(255, 50, 50)  # red
            elif self.warn_val is not None and self.value >= self.warn_val:
                color = QColor(255, 180, 0)  # yellow

            pen.setColor(color)
            pen.setWidth(8)
            p.setPen(pen)
            span = int(-270 * frac * 16)
            p.drawArc(rect, 225 * 16, span)

        # Title
        p.setPen(QColor(180, 180, 180))
        p.setFont(QFont("Segoe UI", 9, QFont.Bold))
        p.drawText(QRectF(0, cy - side//4 - 5, w, 20), Qt.AlignCenter, self.title)

        # Value
        p.setPen(QColor(255, 255, 255))
        p.setFont(QFont("Segoe UI", 18, QFont.Bold))
        if self.value is not None:
            fmt = f"%.{self.decimals}f"
            val_text = fmt % self.value
        else:
            val_text = "---"
        p.drawText(QRectF(0, cy - 12, w, 30), Qt.AlignCenter, val_text)

        # Unit
        p.setPen(QColor(120, 120, 120))
        p.setFont(QFont("Segoe UI", 8))
        p.drawText(QRectF(0, cy + 18, w, 20), Qt.AlignCenter, self.unit)

        p.end()


# ===================================================================
#  MAIN DASHBOARD WINDOW
# ===================================================================

class Dashboard(QMainWindow):
    update_signal = pyqtSignal()

    def __init__(self):
        super().__init__()
        self.setWindowTitle("ROTS Trucking — Jetta TDI OBD2 Dashboard")
        self.setStyleSheet("background-color: #1e1e1e; color: white;")
        self.resize(900, 700)

        self.elm = ELM327()
        self.running = False
        self.gauge_widgets = []

        self.update_signal.connect(self._refresh_gauges)

        self._build_ui()

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setSpacing(5)

        # Header
        header = QLabel("ROTS TRUCKING — 2009 VW JETTA TDI 2.0L")
        header.setFont(QFont("Segoe UI", 14, QFont.Bold))
        header.setAlignment(Qt.AlignCenter)
        header.setStyleSheet("color: #00c853; padding: 5px;")
        main_layout.addWidget(header)

        # Status bar
        self.status_label = QLabel("Not connected — click Connect")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet("color: #ff9800; font-size: 11px; padding: 3px;")
        main_layout.addWidget(self.status_label)

        # Stats row
        stats = QHBoxLayout()
        self.lbl_reads = QLabel("Reads: 0")
        self.lbl_errors = QLabel("Errors: 0")
        self.lbl_volts = QLabel("Battery: --")
        for lbl in [self.lbl_reads, self.lbl_errors, self.lbl_volts]:
            lbl.setStyleSheet("color: #888; font-size: 10px;")
            lbl.setAlignment(Qt.AlignCenter)
            stats.addWidget(lbl)
        main_layout.addLayout(stats)

        # Gauge grid — 4 columns x 3 rows
        gauge_grid = QGridLayout()
        gauge_grid.setSpacing(5)
        for i, (title, unit, pid_cmd, parser, mn, mx, wrn, dng, dec) in enumerate(GAUGES):
            gw = GaugeWidget(title, unit, mn, mx, wrn, dng, dec)
            row = i // 4
            col = i % 4
            gauge_grid.addWidget(gw, row, col)
            self.gauge_widgets.append((gw, pid_cmd, parser))
        main_layout.addLayout(gauge_grid)

        # Buttons
        btn_row = QHBoxLayout()
        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setStyleSheet("background: #00c853; color: black; font-weight: bold; padding: 8px 20px; border-radius: 4px;")
        self.btn_connect.clicked.connect(self._toggle_connect)
        btn_row.addWidget(self.btn_connect)

        btn_dtc = QPushButton("Read Codes")
        btn_dtc.setStyleSheet("background: #ff9800; color: black; font-weight: bold; padding: 8px 20px; border-radius: 4px;")
        btn_dtc.clicked.connect(self._read_codes)
        btn_row.addWidget(btn_dtc)

        btn_clear = QPushButton("Clear Codes")
        btn_clear.setStyleSheet("background: #f44336; color: white; font-weight: bold; padding: 8px 20px; border-radius: 4px;")
        btn_clear.clicked.connect(self._clear_codes)
        btn_row.addWidget(btn_clear)

        btn_vin = QPushButton("Read VIN")
        btn_vin.setStyleSheet("background: #2196f3; color: white; font-weight: bold; padding: 8px 20px; border-radius: 4px;")
        btn_vin.clicked.connect(self._read_vin)
        btn_row.addWidget(btn_vin)

        main_layout.addLayout(btn_row)

        # Log area
        self.log_box = QTextEdit()
        self.log_box.setReadOnly(True)
        self.log_box.setMaximumHeight(120)
        self.log_box.setStyleSheet("background: #111; color: #0f0; font-family: Consolas; font-size: 10px; border: 1px solid #333;")
        main_layout.addWidget(self.log_box)

    def _log(self, msg):
        self.log_box.append(f"[{time.strftime('%H:%M:%S')}] {msg}")
        self.log_box.verticalScrollBar().setValue(self.log_box.verticalScrollBar().maximum())

    def _toggle_connect(self):
        if self.running:
            self.running = False
            self.elm.disconnect()
            self.btn_connect.setText("Connect")
            self.btn_connect.setStyleSheet("background: #00c853; color: black; font-weight: bold; padding: 8px 20px; border-radius: 4px;")
            self.status_label.setText("Disconnected")
            self.status_label.setStyleSheet("color: #ff9800; font-size: 11px; padding: 3px;")
            self._log("Disconnected")
            return

        self.status_label.setText("Connecting...")
        self.status_label.setStyleSheet("color: #ff9800; font-size: 11px; padding: 3px;")
        self._log("Searching for adapter...")
        QApplication.processEvents()

        ok, msg = self.elm.connect()
        if ok:
            self._log(f"CONNECTED: {msg}")
            self.status_label.setText(f"LIVE — {self.elm.elm_version} — Protocol 6 CAN")
            self.status_label.setStyleSheet("color: #00c853; font-size: 11px; padding: 3px;")
            self.btn_connect.setText("Disconnect")
            self.btn_connect.setStyleSheet("background: #f44336; color: white; font-weight: bold; padding: 8px 20px; border-radius: 4px;")
            self.running = True
            t = threading.Thread(target=self._poll_loop, daemon=True)
            t.start()
        else:
            self._log(f"FAILED: {msg}")
            self.status_label.setText("Connection failed — check log")
            self.status_label.setStyleSheet("color: #f44336; font-size: 11px; padding: 3px;")

    def _poll_loop(self):
        """Background thread: reads all confirmed PIDs — fast refresh"""
        while self.running and self.elm.connected:
            for gw, pid_cmd, parser in self.gauge_widgets:
                if not self.running:
                    break
                data = self.elm.query_pid(pid_cmd)
                val = parser(data) if data else None
                gw.set_value(val)

            self.update_signal.emit()

    def _refresh_gauges(self):
        self.lbl_reads.setText(f"Reads: {self.elm.rx_count}")
        self.lbl_errors.setText(f"Errors: {self.elm.error_count}")
        self._volt_counter = getattr(self, "_volt_counter", 0) + 1
        if self._volt_counter >= 30:
            self._volt_counter = 0
            volts = self.elm._send_cmd("ATRV") if self.elm.connected else None
            if volts:
                self.lbl_volts.setText(f"Battery: {volts.strip()}")

    def _read_codes(self):
        if not self.elm.connected:
            self._log("Not connected")
            return
        self._log("Reading trouble codes...")
        QApplication.processEvents()
        codes = self.elm.get_dtc()
        if codes:
            self._log(f"FOUND {len(codes)} code(s):")
            for c in codes:
                self._log(f"  {c}")
            QMessageBox.warning(self, "Trouble Codes", f"Found {len(codes)} code(s):\n\n" + "\n".join(codes))
        else:
            self._log("No trouble codes found")
            QMessageBox.information(self, "Trouble Codes", "No trouble codes found!")

    def _clear_codes(self):
        if not self.elm.connected:
            self._log("Not connected")
            return
        reply = QMessageBox.question(self, "Clear Codes", "Clear ALL trouble codes and reset Check Engine light?",
                                     QMessageBox.Yes | QMessageBox.No)
        if reply == QMessageBox.Yes:
            self._log("Clearing codes...")
            self.elm.clear_dtc()
            self._log("Codes cleared — Check Engine light should be off")

    def _read_vin(self):
        if not self.elm.connected:
            self._log("Not connected")
            return
        self._log("Reading VIN...")
        QApplication.processEvents()
        vin = self.elm.get_vin()
        self._log(f"VIN: {vin}")
        QMessageBox.information(self, "Vehicle VIN", f"VIN: {vin}")

    def closeEvent(self, event):
        self.running = False
        self.elm.disconnect()
        event.accept()


# ===================================================================
#  MAIN
# ===================================================================

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    # Dark palette
    palette = QPalette()
    palette.setColor(QPalette.Window, QColor(30, 30, 30))
    palette.setColor(QPalette.WindowText, Qt.white)
    palette.setColor(QPalette.Base, QColor(20, 20, 20))
    palette.setColor(QPalette.Text, Qt.white)
    palette.setColor(QPalette.Button, QColor(50, 50, 50))
    palette.setColor(QPalette.ButtonText, Qt.white)
    app.setPalette(palette)

    window = Dashboard()
    window.show()
    sys.exit(app.exec_())
