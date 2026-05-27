#!/usr/bin/env python3
"""
AoC Deploy Bridge v2.0
======================
A zero-dependency HTTP server that enables instant remote deployment of source code,
C++ building, and UE5 management for the Architect of Creation project.

Runs on Brad's PC, exposed via ngrok tunnel.
Replaces the slow/unreliable CRD-based deployment workflow entirely.

ENDPOINTS:
  GET  /health              - Health check (no auth required)
  GET  /status              - Full status: UE5 running, build state, ngrok URL
  POST /deploy              - Deploy one or more source files to Source/AOC/
  POST /build               - Trigger C++ editor or standalone build
  GET  /build-status        - Poll build progress and errors
  POST /close-ue5           - Kill UE5 editor process
  POST /open-ue5            - Launch UE5 editor with specified map
  POST /restart-ue5         - Full cycle: close -> build -> open
  POST /run-command         - Execute arbitrary PowerShell command
  POST /read-file           - Read any file from the project directory
  POST /list-files          - Browse the source directory tree
  ANY  /remote/*            - Proxy to UE5 Remote Control API (localhost:30010)

SECURITY:
  All endpoints except /health require X-API-Key header.
  API Key: aoc-deploy-2026

INSTALL:
  1. Copy this file to C:\\AoC-Tools\\aoc_deploy_bridge.py
  2. Run: python aoc_deploy_bridge.py
  3. In another terminal: ngrok http 8080
  4. Share the ngrok URL with Tasklet

Requires: Python 3.10+ (stdlib only, zero dependencies)
"""

import http.server
import json
import os
import subprocess
import sys
import threading
import time
import urllib.request
import urllib.error
import shutil
import logging
import traceback
from pathlib import Path
from datetime import datetime

# ============================================================
# CONFIGURATION — Edit these paths if your setup differs
# ============================================================
AOC_PROJECT_ROOT = r"C:\Users\Bradh\Documents\Unreal Projects\AOC"
AOC_SOURCE_DIR = os.path.join(AOC_PROJECT_ROOT, "Source", "AOC")
UE5_EDITOR = r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
UE5_BUILD_BAT = r"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"
UE5_UPROJECT = os.path.join(AOC_PROJECT_ROOT, "AOC.uproject")
UE5_REMOTE_CONTROL_PORT = 30010
UE5_REMOTE_CONTROL_URL = f"http://localhost:{UE5_REMOTE_CONTROL_PORT}"
PYTHON_EXE = r"C:\Users\Bradh\AppData\Local\Programs\Python\Python314\python.exe"

# Server config
API_KEY = "aoc-deploy-2026"
PORT = 8080
LOG_DIR = os.path.join(AOC_PROJECT_ROOT, "DeployLogs")
BACKUP_DIR = os.path.join(AOC_PROJECT_ROOT, "DeployBackups")

# ============================================================
# GLOBAL STATE
# ============================================================
build_state = {
    "status": "idle",           # idle | building | success | failed
    "last_build_time": None,
    "last_build_duration": None,
    "last_build_output": "",
    "last_build_errors": [],
    "build_type": None,
    "files_compiled": 0,
}

deploy_log = []  # Last 100 deployments
ngrok_url = None
server_start_time = None


# ============================================================
# UTILITY FUNCTIONS
# ============================================================
def is_ue5_running():
    """Check if UnrealEditor.exe is running."""
    try:
        result = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/FO", "CSV"],
            capture_output=True, text=True, timeout=10
        )
        return "UnrealEditor.exe" in result.stdout
    except Exception:
        return False


def get_ngrok_url():
    """Get the public ngrok URL from the local ngrok API."""
    global ngrok_url
    try:
        req = urllib.request.Request("http://localhost:4040/api/tunnels")
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read())
            for tunnel in data.get("tunnels", []):
                if tunnel.get("proto") == "https":
                    ngrok_url = tunnel["public_url"]
                    return ngrok_url
            # Fallback to any tunnel
            tunnels = data.get("tunnels", [])
            if tunnels:
                ngrok_url = tunnels[0].get("public_url")
                return ngrok_url
    except Exception:
        pass
    return ngrok_url  # Return last known URL


def count_compiled_files(output):
    """Parse build output to count compiled files."""
    count = 0
    for line in output.split("\n"):
        if "] Compile " in line or ".cpp" in line.lower():
            count += 1
    return count


def extract_errors(output):
    """Extract error lines from build output."""
    errors = []
    for line in output.split("\n"):
        line_stripped = line.strip()
        if not line_stripped:
            continue
        lower = line_stripped.lower()
        if "error " in lower or "error:" in lower:
            # Skip noise
            if "0 error" in lower or "error(s)" in lower:
                continue
            errors.append(line_stripped[:500])  # Trim long lines
    return errors[-30:]  # Last 30 errors


# ============================================================
# HTTP REQUEST HANDLER
# ============================================================
class AoCDeployHandler(http.server.BaseHTTPRequestHandler):
    """Handle all incoming HTTP requests for the deploy bridge."""

    # Suppress default logging (we use our own)
    def log_message(self, format, *args):
        pass

    def check_auth(self):
        """Verify API key in request header."""
        key = self.headers.get("X-API-Key", "")
        if key != API_KEY:
            self.send_json(401, {"error": "Unauthorized — include X-API-Key header"})
            return False
        return True

    def send_json(self, code, data):
        """Send a JSON response."""
        try:
            body = json.dumps(data, indent=2, default=str).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", len(body))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
        except Exception:
            pass  # Client may have disconnected

    def read_body(self):
        """Read the request body."""
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return b""
        return self.rfile.read(length)

    def read_json(self):
        """Read and parse JSON body, returns {} if empty."""
        body = self.read_body()
        if not body:
            return {}
        try:
            return json.loads(body)
        except json.JSONDecodeError:
            return {}

    # --- Route Dispatch ---

    def do_OPTIONS(self):
        """Handle CORS preflight."""
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key")
        self.end_headers()

    def do_GET(self):
        if self.path == "/health":
            self.send_json(200, {
                "status": "ok",
                "service": "AoC Deploy Bridge v2.0",
                "uptime_seconds": int(time.time() - server_start_time) if server_start_time else 0,
                "timestamp": datetime.now().isoformat(),
            })
            return

        if not self.check_auth():
            return

        if self.path == "/status":
            self.handle_status()
        elif self.path == "/build-status":
            self.send_json(200, build_state)
        elif self.path.startswith("/remote/"):
            self.proxy_to_ue5()
        else:
            self.send_json(404, {"error": f"Unknown endpoint: {self.path}"})

    def do_POST(self):
        if not self.check_auth():
            return

        route_map = {
            "/deploy": self.handle_deploy,
            "/build": self.handle_build,
            "/close-ue5": self.handle_close_ue5,
            "/open-ue5": self.handle_open_ue5,
            "/restart-ue5": self.handle_restart_ue5,
            "/run-command": self.handle_run_command,
            "/read-file": self.handle_read_file,
            "/list-files": self.handle_list_files,
        }

        handler = route_map.get(self.path)
        if handler:
            try:
                handler()
            except Exception as e:
                logging.error(f"Error handling {self.path}: {traceback.format_exc()}")
                self.send_json(500, {"error": str(e)})
        elif self.path.startswith("/remote/"):
            self.proxy_to_ue5()
        else:
            self.send_json(404, {"error": f"Unknown endpoint: {self.path}"})

    def do_PUT(self):
        if self.path.startswith("/remote/"):
            if not self.check_auth():
                return
            self.proxy_to_ue5()
        else:
            self.send_json(404, {"error": f"Unknown endpoint: {self.path}"})

    # --- Core Handlers ---

    def handle_status(self):
        """Return comprehensive system status."""
        ue5_running = is_ue5_running()
        url = get_ngrok_url()
        self.send_json(200, {
            "ue5_running": ue5_running,
            "build": build_state,
            "ngrok_url": url,
            "project_root": AOC_PROJECT_ROOT,
            "source_dir": AOC_SOURCE_DIR,
            "recent_deploys": deploy_log[-10:],
            "uptime_seconds": int(time.time() - server_start_time) if server_start_time else 0,
            "timestamp": datetime.now().isoformat(),
        })

    def handle_deploy(self):
        """
        Deploy one or more source files to Source/AOC/.

        Body format (single file):
        {
            "path": "AI/AoCHumanoidNPCV2.cpp",
            "content": "// file contents..."
        }

        Body format (batch):
        {
            "files": [
                {"path": "AI/AoCHumanoidNPCV2.h", "content": "..."},
                {"path": "AI/AoCHumanoidNPCV2.cpp", "content": "..."}
            ]
        }
        """
        body = self.read_json()

        # Support single file or batch
        files = body.get("files", [])
        if not files:
            path = body.get("path")
            content = body.get("content")
            if not path or content is None:
                self.send_json(400, {"error": "Missing 'path' or 'content'. Send {path, content} or {files: [{path, content}, ...]}"})
                return
            files = [{"path": path, "content": content}]

        results = []
        for f in files:
            rel_path = f.get("path", "")
            content = f.get("content", "")

            # Security: prevent path traversal
            normalized = os.path.normpath(rel_path)
            if ".." in normalized or normalized.startswith(os.sep) or normalized.startswith("/"):
                results.append({"path": rel_path, "success": False, "error": "Invalid path — no .. or absolute paths"})
                continue

            full_path = os.path.join(AOC_SOURCE_DIR, normalized)

            # Double-check it's within project
            if not os.path.normpath(full_path).startswith(os.path.normpath(AOC_SOURCE_DIR)):
                results.append({"path": rel_path, "success": False, "error": "Path escapes source directory"})
                continue

            try:
                # Create directories if needed
                os.makedirs(os.path.dirname(full_path), exist_ok=True)

                # Backup existing file
                if os.path.exists(full_path):
                    backup_name = f"{os.path.basename(full_path)}.{int(time.time())}.bak"
                    backup_subdir = os.path.join(BACKUP_DIR, os.path.dirname(normalized))
                    os.makedirs(backup_subdir, exist_ok=True)
                    backup_path = os.path.join(backup_subdir, backup_name)
                    shutil.copy2(full_path, backup_path)

                # Write the new file
                with open(full_path, "w", encoding="utf-8-sig") as fh:  # BOM for MSVC compatibility
                    fh.write(content)

                size = len(content)
                results.append({
                    "path": rel_path,
                    "full_path": full_path,
                    "success": True,
                    "size": size,
                    "lines": content.count("\n") + 1,
                })
                logging.info(f"✅ Deployed: {rel_path} ({size:,} bytes, {content.count(chr(10))+1} lines)")

                # Add to deploy log
                deploy_log.append({
                    "path": rel_path,
                    "size": size,
                    "time": datetime.now().isoformat(),
                })
                if len(deploy_log) > 100:
                    deploy_log.pop(0)

            except Exception as e:
                results.append({"path": rel_path, "success": False, "error": str(e)})
                logging.error(f"❌ Deploy failed for {rel_path}: {e}")

        success_count = sum(1 for r in results if r.get("success"))
        total = len(results)
        logging.info(f"Deploy batch complete: {success_count}/{total} files succeeded")

        self.send_json(200, {
            "deployed": results,
            "summary": f"{success_count}/{total} files deployed successfully",
        })

    def handle_build(self):
        """
        Trigger a C++ build.

        Body: {"type": "editor"} or {"type": "standalone"}
        Default: "editor"

        Returns immediately. Poll /build-status for progress.
        """
        if build_state["status"] == "building":
            self.send_json(409, {
                "error": "Build already in progress",
                "build": build_state,
            })
            return

        body = self.read_json()
        build_type = body.get("type", "editor")
        target = "AOCEditor" if build_type == "editor" else "AOC"

        build_state["status"] = "building"
        build_state["build_type"] = build_type
        build_state["last_build_time"] = datetime.now().isoformat()
        build_state["last_build_output"] = ""
        build_state["last_build_errors"] = []
        build_state["files_compiled"] = 0
        build_state["last_build_duration"] = None

        def run_build():
            start = time.time()
            try:
                cmd = f'& "{UE5_BUILD_BAT}" {target} Win64 Development "{UE5_UPROJECT}" -waitmutex'
                logging.info(f"🔨 Build started: {target}")

                result = subprocess.run(
                    ["powershell", "-Command", cmd],
                    capture_output=True,
                    text=True,
                    timeout=900,  # 15 minute timeout
                )

                duration = time.time() - start
                output = result.stdout or ""
                stderr = result.stderr or ""
                full_output = output + "\n" + stderr

                build_state["last_build_duration"] = f"{duration:.1f}s"
                build_state["last_build_output"] = full_output[-8000:]  # Last 8KB
                build_state["files_compiled"] = count_compiled_files(full_output)

                if result.returncode == 0 and "Succeeded" in full_output:
                    build_state["status"] = "success"
                    build_state["last_build_errors"] = []
                    logging.info(f"✅ Build SUCCEEDED in {duration:.1f}s ({build_state['files_compiled']} files)")
                else:
                    build_state["status"] = "failed"
                    build_state["last_build_errors"] = extract_errors(full_output)
                    logging.error(f"❌ Build FAILED in {duration:.1f}s — {len(build_state['last_build_errors'])} errors")

            except subprocess.TimeoutExpired:
                build_state["status"] = "failed"
                build_state["last_build_errors"] = ["Build timed out after 15 minutes"]
                build_state["last_build_duration"] = "timeout"
                logging.error("❌ Build TIMED OUT")
            except Exception as e:
                build_state["status"] = "failed"
                build_state["last_build_errors"] = [str(e)]
                logging.error(f"❌ Build EXCEPTION: {e}")

        threading.Thread(target=run_build, daemon=True).start()

        self.send_json(200, {
            "message": f"Build started: {target} Win64 Development",
            "status": "building",
            "poll_url": "/build-status",
        })

    def handle_close_ue5(self):
        """Force-kill UE5 editor."""
        logging.info("🛑 Closing UE5...")
        try:
            result = subprocess.run(
                ["taskkill", "/F", "/IM", "UnrealEditor.exe"],
                capture_output=True, text=True, timeout=30,
            )
            # Also kill any shader compilers
            subprocess.run(
                ["taskkill", "/F", "/IM", "ShaderCompileWorker.exe"],
                capture_output=True, timeout=10,
            )
            time.sleep(2)
            still_running = is_ue5_running()
            if still_running:
                logging.warning("⚠️ UE5 still running after taskkill")
            else:
                logging.info("✅ UE5 closed")

            self.send_json(200, {
                "success": not still_running,
                "message": "UE5 closed" if not still_running else "UE5 may still be closing",
                "output": result.stdout.strip(),
            })
        except Exception as e:
            self.send_json(500, {"error": str(e)})

    def handle_open_ue5(self):
        """
        Launch UE5 editor with specified map.

        Body: {"map": "AocWorld"}  (default: AocWorld)
        """
        body = self.read_json()
        map_name = body.get("map", "AocWorld")

        if is_ue5_running():
            self.send_json(409, {"error": "UE5 is already running. Close it first with /close-ue5"})
            return

        logging.info(f"🚀 Launching UE5 with map: {map_name}")
        try:
            cmd = [UE5_EDITOR, UE5_UPROJECT, f"/Game/{map_name}"]
            subprocess.Popen(cmd)
            self.send_json(200, {
                "success": True,
                "message": f"UE5 launching with /Game/{map_name}",
                "note": "UE5 takes 3-5 minutes to fully load. Poll /status to check.",
            })
        except Exception as e:
            self.send_json(500, {"error": str(e)})

    def handle_restart_ue5(self):
        """
        Full restart cycle: Close UE5 → Build (optional) → Reopen UE5.

        Body: {
            "build": true,          // default: true
            "build_type": "editor", // default: "editor"
            "map": "AocWorld"       // default: "AocWorld"
        }

        Returns immediately. Poll /status for progress.
        """
        body = self.read_json()
        do_build = body.get("build", True)
        build_type = body.get("build_type", "editor")
        map_name = body.get("map", "AocWorld")

        if build_state["status"] == "building":
            self.send_json(409, {"error": "Build already in progress"})
            return

        logging.info(f"🔄 Restart cycle starting (build={do_build}, map={map_name})")

        def restart_cycle():
            # Step 1: Close UE5
            logging.info("🔄 Step 1/3: Closing UE5...")
            subprocess.run(["taskkill", "/F", "/IM", "UnrealEditor.exe"], capture_output=True)
            subprocess.run(["taskkill", "/F", "/IM", "ShaderCompileWorker.exe"], capture_output=True)
            time.sleep(3)

            # Wait until UE5 is actually closed
            for _ in range(10):
                if not is_ue5_running():
                    break
                time.sleep(2)

            # Step 2: Build (optional)
            if do_build:
                logging.info("🔄 Step 2/3: Building...")
                target = "AOCEditor" if build_type == "editor" else "AOC"
                build_state["status"] = "building"
                build_state["build_type"] = build_type
                build_state["last_build_time"] = datetime.now().isoformat()

                start = time.time()
                try:
                    cmd = f'& "{UE5_BUILD_BAT}" {target} Win64 Development "{UE5_UPROJECT}" -waitmutex'
                    result = subprocess.run(
                        ["powershell", "-Command", cmd],
                        capture_output=True, text=True, timeout=900,
                    )
                    duration = time.time() - start
                    output = (result.stdout or "") + "\n" + (result.stderr or "")

                    build_state["last_build_duration"] = f"{duration:.1f}s"
                    build_state["last_build_output"] = output[-8000:]
                    build_state["files_compiled"] = count_compiled_files(output)

                    if result.returncode == 0 and "Succeeded" in output:
                        build_state["status"] = "success"
                        build_state["last_build_errors"] = []
                        logging.info(f"✅ Build SUCCEEDED in {duration:.1f}s")
                    else:
                        build_state["status"] = "failed"
                        build_state["last_build_errors"] = extract_errors(output)
                        logging.error(f"❌ Build FAILED — aborting restart cycle")
                        return  # Don't reopen on failed build

                except subprocess.TimeoutExpired:
                    build_state["status"] = "failed"
                    build_state["last_build_errors"] = ["Build timed out"]
                    logging.error("❌ Build TIMED OUT — aborting restart cycle")
                    return
                except Exception as e:
                    build_state["status"] = "failed"
                    build_state["last_build_errors"] = [str(e)]
                    logging.error(f"❌ Build exception: {e}")
                    return
            else:
                logging.info("🔄 Step 2/3: Skipping build")

            # Step 3: Reopen UE5
            logging.info(f"🔄 Step 3/3: Opening UE5 with {map_name}...")
            try:
                subprocess.Popen([UE5_EDITOR, UE5_UPROJECT, f"/Game/{map_name}"])
                logging.info("✅ Restart cycle COMPLETE!")
            except Exception as e:
                logging.error(f"❌ Failed to open UE5: {e}")

        threading.Thread(target=restart_cycle, daemon=True).start()

        self.send_json(200, {
            "message": f"Restart cycle started: close → {'build → ' if do_build else ''}open ({map_name})",
            "success": True,
            "poll_url": "/status",
        })

    def handle_run_command(self):
        """
        Execute a PowerShell command and return output.

        Body: {"command": "Get-Process", "timeout": 60}
        """
        body = self.read_json()
        command = body.get("command", "")
        timeout = min(body.get("timeout", 60), 300)

        if not command:
            self.send_json(400, {"error": "Missing 'command'"})
            return

        logging.info(f"💻 Running command: {command[:100]}...")

        try:
            result = subprocess.run(
                ["powershell", "-NoProfile", "-Command", command],
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            self.send_json(200, {
                "stdout": result.stdout[-15000:],  # Last 15KB
                "stderr": result.stderr[-5000:],
                "returncode": result.returncode,
                "success": result.returncode == 0,
            })
        except subprocess.TimeoutExpired:
            self.send_json(200, {
                "stdout": "",
                "stderr": f"Command timed out after {timeout}s",
                "returncode": -1,
                "success": False,
            })
        except Exception as e:
            self.send_json(500, {"error": str(e)})

    def handle_read_file(self):
        """
        Read a file from the project directory.

        Body: {"path": "Source/AOC/AI/AoCOracleCompanion.h"}
              or {"path": "C:\\Users\\Bradh\\Documents\\Unreal Projects\\AOC\\Source\\AOC\\AI\\file.h"}
        """
        body = self.read_json()
        path = body.get("path", "")

        if not path:
            self.send_json(400, {"error": "Missing 'path'"})
            return

        # Allow relative paths (from project root) or absolute
        if os.path.isabs(path):
            full_path = os.path.normpath(path)
        else:
            full_path = os.path.normpath(os.path.join(AOC_PROJECT_ROOT, path))

        # Security: only allow reading from project directory or common system paths
        allowed_roots = [
            os.path.normpath(AOC_PROJECT_ROOT),
            os.path.normpath(LOG_DIR),
            os.path.normpath(BACKUP_DIR),
        ]
        if not any(full_path.startswith(root) for root in allowed_roots):
            self.send_json(403, {"error": "Path outside allowed directories"})
            return

        try:
            with open(full_path, "r", encoding="utf-8-sig") as f:
                content = f.read()
            self.send_json(200, {
                "path": full_path,
                "content": content,
                "size": len(content),
                "lines": content.count("\n") + 1,
            })
        except FileNotFoundError:
            self.send_json(404, {"error": f"File not found: {full_path}"})
        except UnicodeDecodeError:
            self.send_json(200, {
                "path": full_path,
                "content": "[Binary file — cannot display]",
                "size": os.path.getsize(full_path),
                "binary": True,
            })
        except Exception as e:
            self.send_json(500, {"error": str(e)})

    def handle_list_files(self):
        """
        List files in a directory.

        Body: {"dir": "AI"}  (relative to Source/AOC/)
              or {"dir": ""}  (root of Source/AOC/)
        """
        body = self.read_json()
        rel_dir = body.get("dir", "")

        full_dir = os.path.normpath(os.path.join(AOC_SOURCE_DIR, rel_dir))

        # Security
        if not full_dir.startswith(os.path.normpath(AOC_SOURCE_DIR)):
            self.send_json(403, {"error": "Path outside source directory"})
            return

        if not os.path.isdir(full_dir):
            self.send_json(404, {"error": f"Directory not found: {rel_dir}"})
            return

        try:
            entries = []
            for entry in os.scandir(full_dir):
                info = {
                    "name": entry.name,
                    "is_dir": entry.is_dir(),
                }
                if entry.is_file():
                    stat = entry.stat()
                    info["size"] = stat.st_size
                    info["modified"] = datetime.fromtimestamp(stat.st_mtime).isoformat()
                entries.append(info)

            # Sort: dirs first, then by name
            entries.sort(key=lambda e: (not e["is_dir"], e["name"].lower()))

            self.send_json(200, {
                "dir": rel_dir or "/",
                "full_path": full_dir,
                "entries": entries,
                "count": len(entries),
            })
        except Exception as e:
            self.send_json(500, {"error": str(e)})

    # --- UE5 Remote Control Proxy ---

    def proxy_to_ue5(self):
        """
        Proxy any request starting with /remote/ to UE5 Remote Control API.
        This lets us use a single ngrok tunnel for both deployment AND UE5 control.
        """
        ue5_url = f"{UE5_REMOTE_CONTROL_URL}{self.path}"

        try:
            body = self.read_body()
            headers = {"Content-Type": "application/json"}

            req = urllib.request.Request(
                ue5_url,
                data=body if body else None,
                method=self.command,
                headers=headers,
            )

            with urllib.request.urlopen(req, timeout=30) as resp:
                resp_body = resp.read()
                self.send_response(resp.status)
                self.send_header("Content-Type", resp.getheader("Content-Type", "application/json"))
                self.send_header("Content-Length", len(resp_body))
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(resp_body)

        except urllib.error.HTTPError as e:
            resp_body = e.read()
            self.send_response(e.code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", len(resp_body))
            self.end_headers()
            self.wfile.write(resp_body)
        except urllib.error.URLError as e:
            self.send_json(502, {
                "error": f"UE5 Remote Control not reachable: {str(e.reason)}",
                "hint": "Is UE5 running? The Remote Control API may not be ready yet.",
            })
        except Exception as e:
            self.send_json(502, {"error": f"UE5 proxy error: {str(e)}"})


# ============================================================
# SERVER STARTUP
# ============================================================
class ThreadedHTTPServer(http.server.HTTPServer):
    """Handle requests in separate threads for concurrency."""
    allow_reuse_address = True

    def process_request(self, request, client_address):
        thread = threading.Thread(target=self._handle_request, args=(request, client_address))
        thread.daemon = True
        thread.start()

    def _handle_request(self, request, client_address):
        try:
            self.finish_request(request, client_address)
        except Exception:
            self.handle_error(request, client_address)
        finally:
            self.shutdown_request(request)


def print_banner():
    """Print startup banner."""
    print("""
╔══════════════════════════════════════════════════════════╗
║            AoC Deploy Bridge v2.0                        ║
║            Architect of Creation                         ║
║            Remote Deployment Server                      ║
╠══════════════════════════════════════════════════════════╣
║                                                          ║
║  Endpoints:                                              ║
║    GET  /health        — Health check                    ║
║    GET  /status        — Full system status               ║
║    POST /deploy        — Deploy source files              ║
║    POST /build         — Trigger C++ build                ║
║    GET  /build-status  — Poll build progress              ║
║    POST /close-ue5     — Kill UE5 editor                  ║
║    POST /open-ue5      — Launch UE5                       ║
║    POST /restart-ue5   — Full restart cycle                ║
║    POST /run-command   — Run PowerShell command           ║
║    POST /read-file     — Read project files               ║
║    POST /list-files    — Browse source tree                ║
║    ANY  /remote/*      — Proxy to UE5 Remote Control      ║
║                                                          ║
║  API Key: aoc-deploy-2026                                ║
║  Port: {port}                                              ║
║                                                          ║
╚══════════════════════════════════════════════════════════╝
""".format(port=PORT))


def main():
    global server_start_time

    # Create directories
    os.makedirs(LOG_DIR, exist_ok=True)
    os.makedirs(BACKUP_DIR, exist_ok=True)

    # Setup logging
    log_file = os.path.join(LOG_DIR, f"bridge_{datetime.now():%Y%m%d_%H%M%S}.log")
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%H:%M:%S",
        handlers=[
            logging.StreamHandler(sys.stdout),
            logging.FileHandler(log_file, encoding="utf-8"),
        ],
    )

    print_banner()
    logging.info(f"Project: {AOC_PROJECT_ROOT}")
    logging.info(f"Source:  {AOC_SOURCE_DIR}")
    logging.info(f"UE5:     {UE5_EDITOR}")
    logging.info(f"Log:     {log_file}")
    logging.info(f"UE5 Running: {is_ue5_running()}")

    # Check ngrok
    url = get_ngrok_url()
    if url:
        logging.info(f"ngrok URL: {url}")
    else:
        logging.info("ngrok not detected — start it with: ngrok http 8080")

    # Start server
    server_start_time = time.time()
    server = ThreadedHTTPServer(("0.0.0.0", PORT), AoCDeployHandler)
    logging.info(f"🚀 Deploy Bridge listening on http://0.0.0.0:{PORT}")
    logging.info("Ready for connections!")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info("\n🛑 Shutting down Deploy Bridge...")
        server.shutdown()
        logging.info("Goodbye!")


if __name__ == "__main__":
    main()
