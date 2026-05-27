#!/usr/bin/env python3
"""
AoC Development Server v1.0
============================
All-in-one deployment, build, and UE5 management server for Ashes of Creation.

Features:
  - HTTP API for remote file deployment (agent pushes code directly)
  - Auto-deploys source files dropped in Downloads/AOC-Deploy folder
  - Smart file-to-path mapping (auto-scans your source tree)
  - One-click UE5 restart with AocWorld map
  - Full rebuild cycle (close -> build -> relaunch)
  - GitHub pull integration
  - Web dashboard with status + controls
  - Automatic backups before every deployment

Usage:
  python aoc_server.py                    # Start the server (default port 8080)
  python aoc_server.py --deploy           # Deploy files from Downloads, exit
  python aoc_server.py --restart-ue5      # Close + relaunch UE5
  python aoc_server.py --build            # Build editor DLL
  python aoc_server.py --full-cycle       # Deploy + Close + Build + Relaunch
  python aoc_server.py --port 9090        # Use custom port

API Endpoints:
  POST /api/deploy          - Deploy a single file  {filename, content, target_path?}
  POST /api/deploy-batch    - Deploy multiple files  {files: [{filename, content, target_path?}]}
  POST /api/deploy-downloads - Deploy all .h/.cpp from Downloads/AOC-Deploy
  POST /api/ue5/close       - Kill UnrealEditor.exe
  POST /api/ue5/launch      - Launch UE5 with AocWorld
  POST /api/ue5/restart     - Close + Launch
  POST /api/ue5/build       - Run editor build (async)
  POST /api/ue5/build-and-restart - Build then restart (async)
  POST /api/github/pull     - Pull latest from GitHub branch
  GET  /api/status          - Current state (UE5 running, deployments, etc.)
  GET  /api/file-map        - Source tree file mapping
  GET  /                    - Web dashboard
"""

import os
import sys
import json
import time
import shutil
import subprocess
import threading
import urllib.request
import urllib.error
import base64
import re
import argparse
import logging
import signal
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from datetime import datetime
from urllib.parse import urlparse, parse_qs
import traceback

# ============================================================
# CONFIGURATION
# ============================================================

SCRIPT_DIR = Path(__file__).parent
CONFIG_FILE = SCRIPT_DIR / "config.json"

DEFAULT_CONFIG = {
    "project_root": r"C:\Users\Bradh\Documents\Unreal Projects\AOC",
    "source_root": r"C:\Users\Bradh\Documents\Unreal Projects\AOC\Source\AOC",
    "deploy_folder": r"C:\Users\Bradh\Downloads\AOC-Deploy",
    "downloads_folder": r"C:\Users\Bradh\Downloads",
    "backup_folder": r"C:\AoC-Tools\backups",
    "log_folder": r"C:\AoC-Tools\logs",

    "ue5_editor": r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
    "project_file": r"C:\Users\Bradh\Documents\Unreal Projects\AOC\AOC.uproject",
    "build_bat": r"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat",
    "map_name": "/Game/AocWorld",

    "github_owner": "rotstrucking1-prog",
    "github_repo": "Rotstrucking-app1",
    "github_branch": "aoc-source-v14",
    "github_token": "",

    "server_port": 8080,
    "poll_interval": 5,
    "auto_deploy_on_drop": True,
    "auto_backup": True
}


def load_config():
    """Load config from file, falling back to defaults."""
    config = dict(DEFAULT_CONFIG)
    if CONFIG_FILE.exists():
        try:
            with open(CONFIG_FILE, "r") as f:
                user = json.load(f)
            config.update(user)
        except Exception as e:
            print(f"[WARN] Could not read config.json: {e}, using defaults")
    return config


def save_config(config):
    """Save current config to file."""
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(CONFIG_FILE, "w") as f:
        json.dump(config, f, indent=2)


# ============================================================
# LOGGING
# ============================================================

def setup_logging(config):
    log_dir = Path(config["log_folder"])
    log_dir.mkdir(parents=True, exist_ok=True)
    log_file = log_dir / f"aoc-deploy-{datetime.now():%Y%m%d}.log"

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.FileHandler(log_file, encoding="utf-8"),
            logging.StreamHandler(sys.stdout),
        ],
    )
    return logging.getLogger("AoC")


# ============================================================
# SOURCE TREE SCANNER
# ============================================================

class SourceTreeScanner:
    """Scans the UE5 source tree and maps filenames to paths."""

    # Known filename-to-subdirectory mappings (from Brad's real source tree)
    KNOWN_MAPPINGS = {
        # AI
        "AoCHumanoidNPCV2": "AI",
        "AoCNPCCombatBrain": "AI",
        "AoCAILODManager": "AI",
        "AoCOracleCompanion": "AI",
        "AoCNPCBrain": "AI",
        "AoCNPCInventory": "AI",
        "AoCNPCLootBrain": "AI",
        "AoCNPCMemory": "AI",
        "AoCNPCNeedSystem": "AI",
        "AoCNPCPersonality": "AI",
        "AoCNPCRelationship": "AI",
        "AoCNPCSkills": "AI",
        "AoCNPCSocialBrain": "AI",
        "AoCNPCSpeech": "AI",
        "AoCNPCTaskGovernor": "AI",
        "AoCNPCGoalPlanner": "AI",
        "AoCNPCLifeBrain": "AI",
        # AI/Tasks
        "BTTask_AoCAttackTarget": "AI/Tasks",
        "BTTask_AoCFindFood": "AI/Tasks",
        "BTTask_AoCFlee": "AI/Tasks",
        "BTTask_AoCGather": "AI/Tasks",
        "BTTask_AoCIdle": "AI/Tasks",
        "BTTask_AoCPatrol": "AI/Tasks",
        "BTTask_AoCSocialize": "AI/Tasks",
        "BTTask_AoCUseCraftStation": "AI/Tasks",
        # AbilitySystem
        "AoCAbilitySystemComponent": "AbilitySystem",
        # Characters
        "AoCCharacterBase": "Characters",
        "AoCEnemyCharacter": "Characters",
        "AoCPlayerCharacter": "Characters",
        # Combat
        "AoCCombatComponent": "Combat",
        # Core
        "AoCGameMode": "Core",
        "AoCGameState": "Core",
        "AoCPlayerController": "Core",
        "AoCPlayerState": "Core",
        # Dialogue
        "AoCDialogueComponent": "Dialogue",
        "AoCDialogueManager": "Dialogue",
        # Inventory
        "AoCInventoryComponent": "Inventory",
        "AoCItemBase": "Inventory",
        "AoCLootTable": "Inventory",
        # Quest
        "AoCQuestComponent": "Quest",
        "AoCQuestManager": "Quest",
        # Skills
        "AoCSkillComponent": "Skills",
        # Spellcraft
        "AoCSpellCastingComponent": "Spellcraft",
        "AoCSpellEffect": "Spellcraft",
        # UI
        "AoCHUDWidget": "UI",
        "AoCRuntimeUI": "UI",
        # UI/Widgets
        "AoCDialogueWidget": "UI/Widgets",
        # Root
        "AOC": "",
    }

    def __init__(self, source_root):
        self.source_root = Path(source_root)
        self.file_map = {}  # filename (no ext) -> relative dir path
        self.full_map = {}  # full filename -> absolute path
        self.scan()

    def scan(self):
        """Walk the source tree and build the file map."""
        self.file_map = {}
        self.full_map = {}

        if not self.source_root.exists():
            logging.warning(f"Source root not found: {self.source_root}")
            # Fall back to known mappings
            for name, subdir in self.KNOWN_MAPPINGS.items():
                self.file_map[name] = subdir
            return

        for root, dirs, files in os.walk(self.source_root):
            # Skip build artifacts
            dirs[:] = [d for d in dirs if d not in ("Intermediate", "Saved", "Build", "__pycache__")]
            rel_dir = Path(root).relative_to(self.source_root)

            for fname in files:
                if fname.endswith((".h", ".cpp", ".cs")):
                    stem = Path(fname).stem
                    self.file_map[stem] = str(rel_dir) if str(rel_dir) != "." else ""
                    self.full_map[fname] = str(Path(root) / fname)

        logging.info(f"Source tree scanned: {len(self.full_map)} files mapped")

    def get_target_path(self, filename):
        """Get the full target path for a source file."""
        stem = Path(filename).stem
        ext = Path(filename).suffix

        # Check live scan first
        if stem in self.file_map:
            subdir = self.file_map[stem]
            target_dir = self.source_root / subdir if subdir else self.source_root
            return str(target_dir / filename)

        # Check known mappings
        if stem in self.KNOWN_MAPPINGS:
            subdir = self.KNOWN_MAPPINGS[stem]
            target_dir = self.source_root / subdir if subdir else self.source_root
            return str(target_dir / filename)

        return None  # Unknown file - needs explicit target_path

    def get_map_dict(self):
        """Return the full file map as a dict for the API."""
        result = {}
        for stem, subdir in self.file_map.items():
            result[stem] = {
                "directory": subdir or "(root)",
                "h_exists": f"{stem}.h" in self.full_map,
                "cpp_exists": f"{stem}.cpp" in self.full_map,
            }
        return result


# ============================================================
# FILE DEPLOYER
# ============================================================

class FileDeployer:
    """Handles deploying source files to the UE5 project."""

    def __init__(self, config, scanner, logger):
        self.config = config
        self.scanner = scanner
        self.log = logger
        self.deployment_history = []
        self.deploy_lock = threading.Lock()

    def deploy_file(self, filename, content, target_path=None):
        """Deploy a single file to the source tree."""
        with self.deploy_lock:
            return self._deploy_file_impl(filename, content, target_path)

    def _deploy_file_impl(self, filename, content, target_path=None):
        """Internal deploy implementation."""
        # Determine target path
        if target_path:
            # Explicit path given (relative to source root)
            full_target = str(Path(self.config["source_root"]) / target_path)
            if not full_target.endswith(filename):
                full_target = str(Path(full_target) / filename)
        else:
            full_target = self.scanner.get_target_path(filename)

        if not full_target:
            return {
                "success": False,
                "error": f"Unknown file '{filename}' - specify target_path",
                "filename": filename,
            }

        # Create backup if file exists
        if self.config["auto_backup"] and os.path.exists(full_target):
            self._backup_file(full_target)

        # Ensure directory exists
        os.makedirs(os.path.dirname(full_target), exist_ok=True)

        # Write the file
        try:
            with open(full_target, "w", encoding="utf-8", newline="\n") as f:
                f.write(content)

            record = {
                "filename": filename,
                "target": full_target,
                "timestamp": datetime.now().isoformat(),
                "size": len(content),
                "success": True,
            }
            self.deployment_history.append(record)
            self.log.info(f"DEPLOYED: {filename} -> {full_target} ({len(content)} bytes)")
            return record

        except Exception as e:
            self.log.error(f"DEPLOY FAILED: {filename} -> {e}")
            return {"success": False, "error": str(e), "filename": filename}

    def _backup_file(self, filepath):
        """Create a timestamped backup of a file."""
        backup_dir = Path(self.config["backup_folder"])
        backup_dir.mkdir(parents=True, exist_ok=True)

        fname = Path(filepath).name
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = backup_dir / f"{timestamp}_{fname}"

        try:
            shutil.copy2(filepath, backup_path)
            self.log.info(f"BACKUP: {fname} -> {backup_path}")
        except Exception as e:
            self.log.warning(f"Backup failed for {fname}: {e}")

    def deploy_batch(self, files):
        """Deploy multiple files at once."""
        results = []
        for f in files:
            result = self.deploy_file(
                f["filename"],
                f["content"],
                f.get("target_path"),
            )
            results.append(result)
        return results

    def deploy_from_downloads(self):
        """Scan Downloads/AOC-Deploy folder for .h/.cpp files and deploy them."""
        deploy_dir = Path(self.config["deploy_folder"])
        downloads_dir = Path(self.config["downloads_folder"])
        results = []

        for folder in [deploy_dir, downloads_dir]:
            if not folder.exists():
                continue

            for f in folder.iterdir():
                if f.is_file() and f.suffix in (".h", ".cpp") and f.stat().st_size > 0:
                    try:
                        content = f.read_text(encoding="utf-8")
                        result = self.deploy_file(f.name, content)

                        if result.get("success"):
                            # Move processed file to avoid re-deploying
                            processed_dir = deploy_dir / "processed"
                            processed_dir.mkdir(exist_ok=True)
                            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                            dest = processed_dir / f"{timestamp}_{f.name}"
                            if folder == deploy_dir:
                                shutil.move(str(f), str(dest))
                            # Don't move files from general Downloads

                        results.append(result)
                    except Exception as e:
                        results.append({
                            "success": False,
                            "filename": f.name,
                            "error": str(e),
                        })

        return results

    def get_history(self, limit=50):
        """Return recent deployment history."""
        return self.deployment_history[-limit:]


# ============================================================
# UE5 MANAGER
# ============================================================

class UE5Manager:
    """Manages UE5 editor lifecycle."""

    def __init__(self, config, logger):
        self.config = config
        self.log = logger
        self.build_status = {"running": False, "last_result": None}
        self.build_thread = None

    def is_running(self):
        """Check if UE5 is currently running."""
        try:
            result = subprocess.run(
                ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/NH"],
                capture_output=True, text=True, timeout=10,
            )
            return "UnrealEditor.exe" in result.stdout
        except Exception:
            return False

    def close(self, force=True):
        """Close UE5 editor."""
        self.log.info("Closing UE5 Editor...")
        try:
            if force:
                result = subprocess.run(
                    ["taskkill", "/F", "/IM", "UnrealEditor.exe"],
                    capture_output=True, text=True, timeout=30,
                )
            else:
                result = subprocess.run(
                    ["taskkill", "/IM", "UnrealEditor.exe"],
                    capture_output=True, text=True, timeout=30,
                )

            # Wait for process to fully exit
            for _ in range(30):
                if not self.is_running():
                    self.log.info("UE5 closed successfully")
                    return {"success": True, "message": "UE5 closed"}
                time.sleep(1)

            return {"success": False, "message": "UE5 didn't close in time"}
        except Exception as e:
            return {"success": False, "message": str(e)}

    def launch(self):
        """Launch UE5 with the AOC project and AocWorld map."""
        self.log.info("Launching UE5 Editor...")
        editor = self.config["ue5_editor"]
        project = self.config["project_file"]
        map_name = self.config["map_name"]

        if not os.path.exists(editor):
            return {"success": False, "message": f"UE5 editor not found: {editor}"}
        if not os.path.exists(project):
            return {"success": False, "message": f"Project not found: {project}"}

        try:
            # Launch UE5 with the map
            cmd = [editor, project, map_name]
            subprocess.Popen(cmd, creationflags=subprocess.DETACHED_PROCESS)
            self.log.info(f"UE5 launched: {' '.join(cmd)}")
            return {
                "success": True,
                "message": "UE5 launching with AocWorld...",
                "command": " ".join(cmd),
            }
        except Exception as e:
            return {"success": False, "message": str(e)}

    def restart(self):
        """Close and relaunch UE5."""
        close_result = self.close()
        if not close_result["success"]:
            return close_result

        time.sleep(3)  # Brief pause between close and relaunch
        return self.launch()

    def build(self, target="AOCEditor", config_name="Development"):
        """Run editor build (blocking)."""
        build_bat = self.config["build_bat"]
        project = self.config["project_file"]

        cmd = [
            build_bat,
            target,
            "Win64",
            config_name,
            project,
        ]

        self.log.info(f"Starting build: {target} {config_name}")
        self.build_status = {
            "running": True,
            "target": target,
            "started": datetime.now().isoformat(),
            "last_result": None,
        }

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=1800,  # 30 min timeout
                cwd=os.path.dirname(build_bat),
            )

            success = result.returncode == 0
            self.build_status = {
                "running": False,
                "target": target,
                "started": self.build_status["started"],
                "finished": datetime.now().isoformat(),
                "success": success,
                "returncode": result.returncode,
                "last_result": "SUCCESS" if success else "FAILED",
                "output_tail": result.stdout[-2000:] if result.stdout else "",
                "error_tail": result.stderr[-2000:] if result.stderr else "",
            }

            if success:
                self.log.info("BUILD SUCCEEDED!")
            else:
                self.log.error(f"BUILD FAILED (exit code {result.returncode})")

            return self.build_status

        except subprocess.TimeoutExpired:
            self.build_status["running"] = False
            self.build_status["last_result"] = "TIMEOUT"
            self.log.error("Build timed out after 30 minutes")
            return self.build_status

        except Exception as e:
            self.build_status["running"] = False
            self.build_status["last_result"] = f"ERROR: {e}"
            self.log.error(f"Build error: {e}")
            return self.build_status

    def build_async(self, target="AOCEditor", config_name="Development"):
        """Start build in background thread."""
        if self.build_status.get("running"):
            return {"success": False, "message": "Build already in progress"}

        self.build_thread = threading.Thread(
            target=self.build, args=(target, config_name), daemon=True
        )
        self.build_thread.start()
        return {"success": True, "message": f"Build started: {target} {config_name}"}

    def build_and_restart_async(self):
        """Close UE5, build, then relaunch - all async."""
        def _worker():
            self.close()
            time.sleep(3)
            build_result = self.build()
            if build_result.get("success"):
                time.sleep(2)
                self.launch()
            else:
                self.log.error("Build failed - not relaunching UE5")

        if self.build_status.get("running"):
            return {"success": False, "message": "Build already in progress"}

        thread = threading.Thread(target=_worker, daemon=True)
        thread.start()
        return {"success": True, "message": "Build & restart cycle started..."}


# ============================================================
# GITHUB INTEGRATION
# ============================================================

class GitHubPuller:
    """Pull latest source files from GitHub."""

    def __init__(self, config, deployer, logger):
        self.config = config
        self.deployer = deployer
        self.log = logger

    def pull_latest(self):
        """Download all source files from GitHub and deploy them."""
        owner = self.config["github_owner"]
        repo = self.config["github_repo"]
        branch = self.config["github_branch"]
        token = self.config.get("github_token", "")

        if not token:
            return {
                "success": False,
                "error": "No GitHub token configured. Add 'github_token' to config.json",
            }

        results = []
        try:
            # Get the source tree from GitHub API
            tree_url = f"https://api.github.com/repos/{owner}/{repo}/git/trees/{branch}?recursive=1"
            headers = {
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github.v3+json",
                "User-Agent": "AoC-Deploy-Server",
            }

            req = urllib.request.Request(tree_url, headers=headers)
            with urllib.request.urlopen(req, timeout=30) as resp:
                tree_data = json.loads(resp.read().decode())

            # Filter for source files
            source_files = [
                item for item in tree_data.get("tree", [])
                if item["path"].startswith("Source/AOC/")
                and item["type"] == "blob"
                and item["path"].endswith((".h", ".cpp"))
            ]

            self.log.info(f"Found {len(source_files)} source files on GitHub")

            # Download and deploy each file
            for item in source_files:
                file_url = f"https://api.github.com/repos/{owner}/{repo}/contents/{item['path']}?ref={branch}"
                req = urllib.request.Request(file_url, headers=headers)

                try:
                    with urllib.request.urlopen(req, timeout=30) as resp:
                        file_data = json.loads(resp.read().decode())

                    content = base64.b64decode(file_data["content"]).decode("utf-8")
                    filename = os.path.basename(item["path"])

                    # Extract target subdirectory from GitHub path
                    # e.g., "Source/AOC/AI/AoCHumanoidNPCV2.h" -> "AI/AoCHumanoidNPCV2.h"
                    rel_path = item["path"].replace("Source/AOC/", "", 1)

                    result = self.deployer.deploy_file(filename, content, rel_path)
                    results.append(result)

                    time.sleep(0.5)  # Rate limit
                except Exception as e:
                    results.append({
                        "success": False,
                        "filename": os.path.basename(item["path"]),
                        "error": str(e),
                    })

            return {
                "success": True,
                "files_deployed": len([r for r in results if r.get("success")]),
                "files_failed": len([r for r in results if not r.get("success")]),
                "details": results,
            }

        except Exception as e:
            self.log.error(f"GitHub pull failed: {e}")
            return {"success": False, "error": str(e)}


# ============================================================
# FOLDER WATCHER
# ============================================================

class FolderWatcher:
    """Watches the deploy folder for new source files."""

    def __init__(self, config, deployer, logger):
        self.config = config
        self.deployer = deployer
        self.log = logger
        self.seen_files = set()
        self.running = False
        self.thread = None

    def start(self):
        """Start watching in a background thread."""
        if self.running:
            return
        self.running = True

        # Pre-populate seen files so we don't deploy existing files
        deploy_dir = Path(self.config["deploy_folder"])
        if deploy_dir.exists():
            for f in deploy_dir.iterdir():
                if f.is_file():
                    self.seen_files.add(self._file_key(f))

        self.thread = threading.Thread(target=self._watch_loop, daemon=True)
        self.thread.start()
        self.log.info(f"Folder watcher started: {deploy_dir}")

    def stop(self):
        self.running = False

    def _file_key(self, path):
        """Unique key for a file based on name + size + mtime."""
        try:
            stat = path.stat()
            return f"{path.name}:{stat.st_size}:{stat.st_mtime_ns}"
        except Exception:
            return f"{path.name}:unknown"

    def _watch_loop(self):
        """Poll the deploy folder for new files."""
        deploy_dir = Path(self.config["deploy_folder"])
        deploy_dir.mkdir(parents=True, exist_ok=True)
        interval = self.config.get("poll_interval", 5)

        while self.running:
            try:
                for f in deploy_dir.iterdir():
                    if not f.is_file():
                        continue
                    if f.suffix not in (".h", ".cpp"):
                        continue
                    if f.parent.name == "processed":
                        continue

                    key = self._file_key(f)
                    if key not in self.seen_files:
                        self.seen_files.add(key)
                        self.log.info(f"New file detected: {f.name}")

                        try:
                            content = f.read_text(encoding="utf-8")
                            result = self.deployer.deploy_file(f.name, content)

                            if result.get("success"):
                                # Move to processed
                                processed = deploy_dir / "processed"
                                processed.mkdir(exist_ok=True)
                                ts = datetime.now().strftime("%Y%m%d_%H%M%S")
                                shutil.move(str(f), str(processed / f"{ts}_{f.name}"))
                        except Exception as e:
                            self.log.error(f"Auto-deploy error for {f.name}: {e}")

            except Exception as e:
                self.log.error(f"Watcher error: {e}")

            time.sleep(interval)


# ============================================================
# WEB DASHBOARD HTML
# ============================================================

DASHBOARD_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AoC Deploy Server</title>
<style>
* { margin:0; padding:0; box-sizing:border-box; }
body { background:#0d1117; color:#c9d1d9; font-family:'Segoe UI',sans-serif; padding:20px; }
.header { display:flex; justify-content:space-between; align-items:center; margin-bottom:24px; padding-bottom:16px; border-bottom:1px solid #21262d; }
.header h1 { font-size:24px; color:#58a6ff; }
.header .status { padding:6px 16px; border-radius:20px; font-size:14px; font-weight:600; }
.status.online { background:#0d419d; color:#58a6ff; }
.status.offline { background:#490202; color:#f85149; }
.grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(300px,1fr)); gap:16px; margin-bottom:24px; }
.card { background:#161b22; border:1px solid #21262d; border-radius:8px; padding:20px; }
.card h2 { font-size:16px; color:#8b949e; margin-bottom:12px; text-transform:uppercase; letter-spacing:1px; }
.card .value { font-size:32px; font-weight:700; color:#f0f6fc; }
.card .sub { font-size:13px; color:#8b949e; margin-top:4px; }
.actions { display:grid; grid-template-columns:repeat(auto-fit,minmax(200px,1fr)); gap:12px; margin-bottom:24px; }
.btn { background:#21262d; color:#c9d1d9; border:1px solid #30363d; border-radius:6px; padding:12px 20px;
       font-size:15px; cursor:pointer; text-align:center; transition:all .15s; display:flex; align-items:center; gap:8px; justify-content:center; }
.btn:hover { background:#30363d; border-color:#58a6ff; color:#58a6ff; }
.btn.primary { background:#238636; border-color:#2ea043; color:#fff; }
.btn.primary:hover { background:#2ea043; }
.btn.danger { background:#490202; border-color:#f85149; color:#f85149; }
.btn.danger:hover { background:#6e1212; }
.btn.building { background:#1a1a00; border-color:#d29922; color:#d29922; animation:pulse 2s infinite; }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.6} }
.log { background:#0d1117; border:1px solid #21262d; border-radius:8px; padding:16px; max-height:400px;
       overflow-y:auto; font-family:'Cascadia Code','Consolas',monospace; font-size:13px; line-height:1.6; }
.log .entry { padding:2px 0; border-bottom:1px solid #21262d11; }
.log .time { color:#484f58; }
.log .success { color:#3fb950; }
.log .error { color:#f85149; }
.log .info { color:#58a6ff; }
.emoji { font-size:20px; }
#status-banner { padding:8px 16px; border-radius:6px; margin-bottom:16px; display:none; font-weight:600; text-align:center; }
#status-banner.show { display:block; }
#status-banner.success { background:#0d3a1a; color:#3fb950; border:1px solid #238636; }
#status-banner.error { background:#490202; color:#f85149; border:1px solid #6e1212; }
#status-banner.working { background:#1a1a00; color:#d29922; border:1px solid #d29922; animation:pulse 2s infinite; }
</style>
</head>
<body>
<div class="header">
  <h1>⚔️ AoC Deploy Server</h1>
  <div class="status" id="server-status">Checking...</div>
</div>

<div id="status-banner"></div>

<div class="grid">
  <div class="card">
    <h2>UE5 Editor</h2>
    <div class="value" id="ue5-status">...</div>
    <div class="sub" id="ue5-sub"></div>
  </div>
  <div class="card">
    <h2>Deployments</h2>
    <div class="value" id="deploy-count">0</div>
    <div class="sub" id="deploy-sub">files deployed this session</div>
  </div>
  <div class="card">
    <h2>Build Status</h2>
    <div class="value" id="build-status">Idle</div>
    <div class="sub" id="build-sub"></div>
  </div>
  <div class="card">
    <h2>Source Files</h2>
    <div class="value" id="file-count">...</div>
    <div class="sub">mapped in source tree</div>
  </div>
</div>

<h2 style="color:#8b949e; margin-bottom:12px; font-size:14px; text-transform:uppercase; letter-spacing:1px;">Quick Actions</h2>
<div class="actions">
  <button class="btn primary" onclick="apiCall('/api/deploy-downloads','POST')">
    <span class="emoji">📥</span> Deploy from Downloads
  </button>
  <button class="btn" onclick="apiCall('/api/ue5/restart','POST')">
    <span class="emoji">🔄</span> Restart UE5
  </button>
  <button class="btn" onclick="apiCall('/api/ue5/close','POST')">
    <span class="emoji">⏹️</span> Close UE5
  </button>
  <button class="btn" onclick="apiCall('/api/ue5/launch','POST')">
    <span class="emoji">▶️</span> Launch UE5
  </button>
  <button class="btn" onclick="apiCall('/api/ue5/build','POST')">
    <span class="emoji">🔨</span> Build Editor
  </button>
  <button class="btn primary" onclick="apiCall('/api/ue5/build-and-restart','POST')">
    <span class="emoji">⚡</span> Build & Restart
  </button>
  <button class="btn" onclick="apiCall('/api/github/pull','POST')">
    <span class="emoji">🐙</span> Pull from GitHub
  </button>
  <button class="btn danger" onclick="apiCall('/api/ue5/close','POST')">
    <span class="emoji">💀</span> Force Kill UE5
  </button>
</div>

<h2 style="color:#8b949e; margin-bottom:12px; font-size:14px; text-transform:uppercase; letter-spacing:1px;">Deployment Log</h2>
<div class="log" id="log-area">
  <div class="entry info">Connecting to server...</div>
</div>

<script>
let logEntries = [];

async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    const d = await r.json();

    document.getElementById('server-status').textContent = '● Online';
    document.getElementById('server-status').className = 'status online';
    document.getElementById('ue5-status').textContent = d.ue5_running ? '🟢 Running' : '🔴 Stopped';
    document.getElementById('ue5-sub').textContent = d.ue5_running ? 'Editor is active' : 'Editor not running';
    document.getElementById('deploy-count').textContent = d.deployment_count || 0;
    document.getElementById('file-count').textContent = d.source_files_mapped || '...';

    const bs = d.build_status || {};
    const bEl = document.getElementById('build-status');
    const bSub = document.getElementById('build-sub');
    if (bs.running) { bEl.textContent = '🔨 Building...'; bSub.textContent = 'Started: ' + (bs.started||''); }
    else if (bs.last_result === 'SUCCESS') { bEl.textContent = '✅ Success'; bSub.textContent = bs.finished||''; }
    else if (bs.last_result) { bEl.textContent = '❌ ' + bs.last_result; bSub.textContent = bs.finished||''; }
    else { bEl.textContent = 'Idle'; bSub.textContent = ''; }
  } catch(e) {
    document.getElementById('server-status').textContent = '● Offline';
    document.getElementById('server-status').className = 'status offline';
  }
}

async function apiCall(endpoint, method) {
  const banner = document.getElementById('status-banner');
  banner.textContent = '⏳ Working...';
  banner.className = 'show working';

  try {
    const r = await fetch(endpoint, {method: method||'GET'});
    const d = await r.json();
    banner.textContent = d.message || d.error || JSON.stringify(d).slice(0,200);
    banner.className = 'show ' + (d.success !== false ? 'success' : 'error');
    addLog(d.success !== false ? 'success' : 'error', endpoint + ': ' + (d.message || JSON.stringify(d).slice(0,150)));
    fetchStatus();
    setTimeout(() => { banner.className = ''; }, 5000);
  } catch(e) {
    banner.textContent = 'Error: ' + e.message;
    banner.className = 'show error';
    addLog('error', endpoint + ': ' + e.message);
  }
}

function addLog(type, msg) {
  const now = new Date().toLocaleTimeString();
  logEntries.unshift({time: now, type, msg});
  if (logEntries.length > 100) logEntries.pop();
  renderLog();
}

function renderLog() {
  const area = document.getElementById('log-area');
  area.innerHTML = logEntries.map(e =>
    `<div class="entry"><span class="time">[${e.time}]</span> <span class="${e.type}">${e.msg}</span></div>`
  ).join('');
}

fetchStatus();
setInterval(fetchStatus, 5000);
addLog('info', 'Dashboard connected to AoC Deploy Server');
</script>
</body>
</html>"""


# ============================================================
# HTTP REQUEST HANDLER
# ============================================================

class AoCHandler(BaseHTTPRequestHandler):
    """HTTP request handler for the AoC Deploy Server."""

    server_version = "AoC-Deploy-Server/1.0"

    def log_message(self, format, *args):
        """Override to use our logger."""
        logging.getLogger("AoC").info(f"HTTP: {format % args}")

    def _set_headers(self, status=200, content_type="application/json"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _json_response(self, data, status=200):
        self._set_headers(status, "application/json")
        self.wfile.write(json.dumps(data, indent=2).encode("utf-8"))

    def _read_body(self):
        content_length = int(self.headers.get("Content-Length", 0))
        if content_length > 0:
            return json.loads(self.rfile.read(content_length).decode("utf-8"))
        return {}

    def do_OPTIONS(self):
        self._set_headers(204)

    def do_GET(self):
        path = urlparse(self.path).path

        if path == "/":
            # Serve dashboard
            self._set_headers(200, "text/html")
            self.wfile.write(DASHBOARD_HTML.encode("utf-8"))

        elif path == "/api/status":
            app = self.server.app
            self._json_response({
                "server": "AoC Deploy Server v1.0",
                "uptime": str(datetime.now() - app["start_time"]),
                "ue5_running": app["ue5"].is_running(),
                "build_status": app["ue5"].build_status,
                "deployment_count": len(app["deployer"].deployment_history),
                "recent_deployments": app["deployer"].get_history(10),
                "source_files_mapped": len(app["scanner"].file_map),
                "watcher_active": app["watcher"].running,
            })

        elif path == "/api/file-map":
            self._json_response(self.server.app["scanner"].get_map_dict())

        elif path == "/api/health":
            self._json_response({"status": "ok", "timestamp": datetime.now().isoformat()})

        else:
            self._json_response({"error": "Not found"}, 404)

    def do_POST(self):
        path = urlparse(self.path).path
        app = self.server.app

        try:
            if path == "/api/deploy":
                body = self._read_body()
                result = app["deployer"].deploy_file(
                    body["filename"],
                    body["content"],
                    body.get("target_path"),
                )
                self._json_response(result)

            elif path == "/api/deploy-batch":
                body = self._read_body()
                results = app["deployer"].deploy_batch(body["files"])
                succeeded = sum(1 for r in results if r.get("success"))
                self._json_response({
                    "success": True,
                    "message": f"Deployed {succeeded}/{len(results)} files",
                    "results": results,
                })

            elif path == "/api/deploy-downloads":
                results = app["deployer"].deploy_from_downloads()
                succeeded = sum(1 for r in results if r.get("success"))
                self._json_response({
                    "success": True,
                    "message": f"Deployed {succeeded} files from Downloads" if results else "No new files found",
                    "results": results,
                })

            elif path == "/api/ue5/close":
                result = app["ue5"].close()
                self._json_response(result)

            elif path == "/api/ue5/launch":
                result = app["ue5"].launch()
                self._json_response(result)

            elif path == "/api/ue5/restart":
                result = app["ue5"].restart()
                self._json_response(result)

            elif path == "/api/ue5/build":
                result = app["ue5"].build_async()
                self._json_response(result)

            elif path == "/api/ue5/build-and-restart":
                result = app["ue5"].build_and_restart_async()
                self._json_response(result)

            elif path == "/api/github/pull":
                result = app["github"].pull_latest()
                self._json_response(result)

            elif path == "/api/rescan":
                app["scanner"].scan()
                self._json_response({
                    "success": True,
                    "message": f"Source tree rescanned: {len(app['scanner'].file_map)} files",
                })

            elif path == "/api/execute":
                # Run an arbitrary command (use with caution)
                body = self._read_body()
                cmd = body.get("command", "")
                if not cmd:
                    self._json_response({"error": "No command specified"}, 400)
                    return
                try:
                    result = subprocess.run(
                        cmd, shell=True, capture_output=True, text=True, timeout=60
                    )
                    self._json_response({
                        "success": result.returncode == 0,
                        "returncode": result.returncode,
                        "stdout": result.stdout[-5000:],
                        "stderr": result.stderr[-2000:],
                    })
                except Exception as e:
                    self._json_response({"success": False, "error": str(e)})

            else:
                self._json_response({"error": "Not found"}, 404)

        except Exception as e:
            logging.getLogger("AoC").error(f"Request error: {traceback.format_exc()}")
            self._json_response({"error": str(e)}, 500)

    # Alias PUT to POST for compatibility
    do_PUT = do_POST


# ============================================================
# MAIN SERVER
# ============================================================

def run_server(config, log):
    """Start the HTTP server."""
    scanner = SourceTreeScanner(config["source_root"])
    deployer = FileDeployer(config, scanner, log)
    ue5 = UE5Manager(config, log)
    github = GitHubPuller(config, deployer, log)
    watcher = FolderWatcher(config, deployer, log)

    # Ensure deploy folder exists
    Path(config["deploy_folder"]).mkdir(parents=True, exist_ok=True)

    # Start folder watcher
    if config.get("auto_deploy_on_drop", True):
        watcher.start()

    port = config.get("server_port", 8080)
    server = HTTPServer(("0.0.0.0", port), AoCHandler)
    server.app = {
        "config": config,
        "scanner": scanner,
        "deployer": deployer,
        "ue5": ue5,
        "github": github,
        "watcher": watcher,
        "start_time": datetime.now(),
    }

    log.info("=" * 60)
    log.info("  AoC Deploy Server v1.0")
    log.info(f"  Dashboard:  http://localhost:{port}")
    log.info(f"  API:        http://localhost:{port}/api/status")
    log.info(f"  Source:     {config['source_root']}")
    log.info(f"  Deploy:     {config['deploy_folder']}")
    log.info(f"  Watcher:    {'Active' if watcher.running else 'Disabled'}")
    log.info("=" * 60)
    log.info("Server is ready! Press Ctrl+C to stop.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log.info("Server shutting down...")
        watcher.stop()
        server.server_close()


# ============================================================
# CLI COMMANDS
# ============================================================

def cmd_deploy(config, log):
    """Deploy files from Downloads folder."""
    scanner = SourceTreeScanner(config["source_root"])
    deployer = FileDeployer(config, scanner, log)
    results = deployer.deploy_from_downloads()
    if results:
        for r in results:
            status = "✅" if r.get("success") else "❌"
            log.info(f"  {status} {r.get('filename', '?')} -> {r.get('target', r.get('error', '?'))}")
    else:
        log.info("No new files found to deploy")


def cmd_restart(config, log):
    """Restart UE5."""
    ue5 = UE5Manager(config, log)
    result = ue5.restart()
    log.info(f"Result: {result['message']}")


def cmd_build(config, log):
    """Build editor DLL."""
    ue5 = UE5Manager(config, log)
    ue5.close()
    time.sleep(3)
    result = ue5.build()
    if result.get("success"):
        log.info("Build succeeded! Launching UE5...")
        ue5.launch()
    else:
        log.error("Build failed!")
        if result.get("error_tail"):
            log.error(result["error_tail"])


def cmd_full_cycle(config, log):
    """Full deploy + build + restart cycle."""
    log.info("=== FULL CYCLE: Deploy -> Build -> Restart ===")

    # 1. Deploy
    scanner = SourceTreeScanner(config["source_root"])
    deployer = FileDeployer(config, scanner, log)
    results = deployer.deploy_from_downloads()
    deployed = sum(1 for r in results if r.get("success"))
    log.info(f"Deployed {deployed} files")

    # 2. Close UE5
    ue5 = UE5Manager(config, log)
    ue5.close()
    time.sleep(3)

    # 3. Build
    log.info("Starting build...")
    build_result = ue5.build()

    if build_result.get("success"):
        # 4. Relaunch
        time.sleep(2)
        ue5.launch()
        log.info("=== FULL CYCLE COMPLETE! ===")
    else:
        log.error("Build failed! Not launching UE5.")
        log.error("Check the build log for errors.")


# ============================================================
# ENTRY POINT
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="AoC Deploy Server")
    parser.add_argument("--deploy", action="store_true", help="Deploy files from Downloads")
    parser.add_argument("--restart-ue5", action="store_true", help="Restart UE5 editor")
    parser.add_argument("--build", action="store_true", help="Build editor DLL")
    parser.add_argument("--full-cycle", action="store_true", help="Deploy + Build + Restart")
    parser.add_argument("--port", type=int, default=None, help="Server port (default 8080)")
    args = parser.parse_args()

    config = load_config()
    if args.port:
        config["server_port"] = args.port

    # Save config if it doesn't exist
    if not CONFIG_FILE.exists():
        save_config(config)

    log = setup_logging(config)

    if args.deploy:
        cmd_deploy(config, log)
    elif args.restart_ue5:
        cmd_restart(config, log)
    elif args.build:
        cmd_build(config, log)
    elif args.full_cycle:
        cmd_full_cycle(config, log)
    else:
        run_server(config, log)


if __name__ == "__main__":
    main()
