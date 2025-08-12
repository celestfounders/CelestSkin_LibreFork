#!/usr/bin/env python3
"""
AI Server Worker (single-file)

Starts a lightweight HTTP server (no external deps) for prompts.
Auto-started by PrivateBackendService via BackendManager (discovery of *_worker.py).
Writes its runtime info (pid, port) to a state file for other components.
"""

import json
import os
import signal
import socket
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
import subprocess
from pathlib import Path
from datetime import datetime

# Import PyUNO for LibreOffice connectivity
try:
    import uno
    PYUNO_AVAILABLE = True
    print("✅ PyUNO imported successfully")
except ImportError as e:
    PYUNO_AVAILABLE = False
    print(f"⚠️ PyUNO not available: {e}")
except Exception as e:
    PYUNO_AVAILABLE = False
    print(f"⚠️ PyUNO failed to load: {e}")


STATE_DIR = Path(os.environ.get("PRIVATE_BACKEND_STATE_DIR", "/tmp/private_backend_state"))
STATE_DIR.mkdir(parents=True, exist_ok=True)
STATE_FILE = STATE_DIR / "ai_server.json"


class LibreOfficeConnector:
    """Handles connections to LibreOffice via PyUNO socket bridge"""
    
    def __init__(self):
        self.context = None
        self.desktop = None
        self.connected = False
        self.lo_socket_port = 2083  # Default UNO socket port
        
    def connect_to_libreoffice(self):
        """Connect to LibreOffice via UNO socket bridge using singleton pattern"""
        if not PYUNO_AVAILABLE:
            return False
            
        max_attempts = 5
        for attempt in range(max_attempts):
            try:
                print(f"🔗 Connecting to LibreOffice (attempt {attempt + 1}/{max_attempts})")
                
                # Create local context (for client side)
                local_context = uno.getComponentContext()
                
                # Create resolver
                resolver = local_context.ServiceManager.createInstanceWithContext(
                    "com.sun.star.bridge.UnoUrlResolver", local_context)
                
                # Connect to the remote office
                uno_url = f"uno:socket,host=localhost,port={self.lo_socket_port};urp;StarOffice.ComponentContext"
                self.context = resolver.resolve(uno_url)
                print(f"✅ Remote context resolved on attempt {attempt + 1}")
                
                # Health probe: Service Manager
                smgr = self.context.getServiceManager()
                print(f"✅ Service Manager accessible")
                
                # Use SINGLETON Desktop access (preferred method)
                self.desktop = self.context.getValueByName("/singletons/com.sun.star.frame.theDesktop")
                print(f"✅ Singleton Desktop obtained")
                
                # Small settle time to avoid early-disposal races
                import time
                time.sleep(0.05)
                
                # Test with a lightweight service to ensure stability
                cfg = smgr.createInstanceWithContext(
                    "com.sun.star.configuration.ConfigurationProvider", self.context)
                print(f"✅ Configuration service test passed")
                
                self.connected = True
                print(f"✅ Connected to LibreOffice via UNO on port {self.lo_socket_port}")
                return True
                
            except Exception as e:
                error_str = str(e)
                print(f"⚠️ Connection attempt {attempt + 1} failed: {error_str}")
                
                # Handle specific PyUNO exceptions
                if "NoConnectException" in error_str:
                    print(f"🔄 LibreOffice not ready, retrying in 1 second...")
                    import time
                    time.sleep(1)
                    continue
                elif "connection.ConnectionSetupException" in error_str:
                    print(f"🔄 Connection setup failed, retrying...")
                    import time
                    time.sleep(1)
                    continue
                else:
                    print(f"❌ Unexpected error: {error_str}")
                    if attempt < max_attempts - 1:
                        import time
                        time.sleep(1)
                        continue
        
        print(f"❌ Failed to connect to LibreOffice after {max_attempts} attempts")
        self.connected = False
        return False
    
    def get_current_sheet_data(self):
        """Extract data from current Calc sheet"""
        if not self.connected or not self.desktop:
            return {"error": "LibreOffice not connected"}
        
        try:
            document = self.desktop.getCurrentComponent()
            if not document:
                return {"error": "No document open"}
            
            # Check if it's a Calc document
            if not hasattr(document, 'getSheets'):
                return {"error": "Current document is not a Calc spreadsheet"}
            
            active_sheet = document.getCurrentController().getActiveSheet()
            sheet_name = active_sheet.getName()
            
            # Get used range
            cursor = active_sheet.createCursor()
            cursor.gotoEndOfUsedArea(True)
            used_range = cursor.getRangeAddress()
            
            # Extract data from used range
            data_range = active_sheet.getCellRangeByPosition(
                used_range.StartColumn, used_range.StartRow,
                used_range.EndColumn, used_range.EndRow
            )
            
            # Convert to array
            data_array = data_range.getDataArray()
            data_list = []
            for row in data_array:
                data_list.append(list(row))
            
            return {
                "sheet": sheet_name,
                "range": f"{chr(65 + used_range.StartColumn)}{used_range.StartRow + 1}:{chr(65 + used_range.EndColumn)}{used_range.EndRow + 1}",
                "data": data_list,
                "rows": used_range.EndRow - used_range.StartRow + 1,
                "columns": used_range.EndColumn - used_range.StartColumn + 1
            }
            
        except Exception as e:
            return {"error": f"Failed to extract data: {str(e)}"}


class _JSONHandler(BaseHTTPRequestHandler):
    server_version = "AIServer/0.1"
    
    def __init__(self, *args, lo_connector=None, **kwargs):
        self.lo_connector = lo_connector
        super().__init__(*args, **kwargs)

    def _send_json(self, status: int, payload: dict):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        # Quiet server logs; write minimal to stdout
        sys.stdout.write("[AI-SERVER] " + (fmt % args) + "\n")

    def do_GET(self):
        if self.path == "/health":
            lo_status = "connected" if self.lo_connector and self.lo_connector.connected else "disconnected"
            self._send_json(200, {
                "status": "ok",
                "time": datetime.utcnow().isoformat() + "Z",
                "pid": os.getpid(),
                "pyuno_available": PYUNO_AVAILABLE,
                "libreoffice_status": lo_status
            })
            return
            
        elif self.path == "/sheet":
            """Get current sheet data from LibreOffice"""
            if not self.lo_connector or not self.lo_connector.connected:
                self._send_json(503, {"error": "LibreOffice not connected"})
                return
            
            data = self.lo_connector.get_current_sheet_data()
            self._send_json(200, data)
            return
            
        self._send_json(404, {"error": "not_found"})

    def do_POST(self):
        if self.path == "/prompt":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                raw = self.rfile.read(length) if length > 0 else b"{}"
                data = json.loads(raw.decode("utf-8")) if raw else {}
                prompt = data.get("prompt", "")
                session_id = data.get("session_id")
                selection = data.get("selection", {})
                attachments = data.get("attachments", [])
                
                # Get selection from LibreOffice if not provided and connection available
                if not selection and self.lo_connector and self.lo_connector.connected:
                    selection = self.lo_connector.get_current_sheet_data()

                # Enhanced response with LibreOffice data
                response_text = f"Processed prompt: '{prompt}'"
                if selection and 'data' in selection:
                    response_text += f"\nFound sheet data with {len(selection['data'])} rows from '{selection.get('sheet', 'unknown sheet')}'"
                
                response = {
                    "success": True,
                    "session_id": session_id or f"session_{os.getpid()}",
                    "response": response_text,
                    "selection": selection,
                    "attachments": attachments,
                    "metadata": {
                        "model": "ai-server-pyuno-enabled",
                        "pid": os.getpid(),
                        "received_at": datetime.utcnow().isoformat() + "Z",
                        "pyuno_connected": self.lo_connector.connected if self.lo_connector else False,
                    },
                }
                self._send_json(200, response)
            except Exception as e:
                self._send_json(500, {"error": str(e)})
            return
        if self.path == "/git":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                raw = self.rfile.read(length) if length > 0 else b"{}"
                data = json.loads(raw.decode("utf-8")) if raw else {}
                action = data.get("action", "status")
                repo = data.get("repo")  # optional path to repo
                cwd = repo if (isinstance(repo, str) and repo) else None

                def run_git(args):
                    result = subprocess.run(["git"] + args, cwd=cwd, capture_output=True, text=True)
                    return {
                        "code": result.returncode,
                        "stdout": result.stdout.strip(),
                        "stderr": result.stderr.strip(),
                    }

                if action == "status":
                    out = run_git(["status", "--porcelain=v1", "-b"])
                elif action == "pull":
                    out = run_git(["pull", "--ff-only"]) 
                elif action == "branches":
                    out = run_git(["branch", "--show-current"]) 
                else:
                    self._send_json(400, {"error": f"unknown git action: {action}"})
                    return

                self._send_json(200, {
                    "success": out["code"] == 0,
                    "action": action,
                    "repo": cwd,
                    "result": out,
                })
            except Exception as e:
                self._send_json(500, {"error": str(e)})
            return
        if self.path == "/ports":
            try:
                # Return consolidated manager state for all workers/ports
                manager_file = STATE_DIR / "manager_state.json"
                if manager_file.exists():
                    payload = json.loads(manager_file.read_text(encoding="utf-8"))
                    self._send_json(200, payload)
                else:
                    self._send_json(404, {"error": "manager_state not found", "path": str(manager_file)})
            except Exception as e:
                self._send_json(500, {"error": str(e)})
            return

        self._send_json(404, {"error": "not_found"})


def _write_state(port: int):
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    payload = {
        "pid": os.getpid(),
        "port": port,
        "host": "127.0.0.1",
        "started_at": datetime.utcnow().isoformat() + "Z",
    }
    STATE_FILE.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    sys.stdout.write(f"[AI-SERVER] state -> {STATE_FILE}\n")


def _serve_forever():
    # Initialize LibreOffice connector
    lo_connector = LibreOfficeConnector()
    
    # Create handler factory with LibreOffice connector
    def handler_factory(*args, **kwargs):
        return _JSONHandler(*args, lo_connector=lo_connector, **kwargs)
    
    # Bind to ephemeral port (0) to avoid conflicts
    httpd = HTTPServer(("127.0.0.1", 0), handler_factory)
    port = httpd.server_address[1]
    _write_state(port)
    sys.stdout.write(f"[AI-SERVER] listening on 127.0.0.1:{port}\n")
    
    # Try to connect to LibreOffice in background
    def connect_to_libreoffice():
        # Wait a bit for LibreOffice to be ready
        import time
        time.sleep(2)
        for attempt in range(5):
            if lo_connector.connect_to_libreoffice():
                sys.stdout.write("[AI-SERVER] ✅ LibreOffice connected\n")
                break
            else:
                sys.stdout.write(f"[AI-SERVER] ⏳ LibreOffice connection attempt {attempt + 1}/5 failed\n")
                time.sleep(3)
        if not lo_connector.connected:
            sys.stdout.write("[AI-SERVER] ⚠️ Could not connect to LibreOffice\n")
    
    # Start LibreOffice connection in background
    connection_thread = threading.Thread(target=connect_to_libreoffice, daemon=True)
    connection_thread.start()

    stop_event = threading.Event()

    def _handle_sig(signum, frame):
        sys.stdout.write(f"[AI-SERVER] signal {signum} -> shutdown\n")
        stop_event.set()
        try:
            httpd.shutdown()
        except Exception:
            pass

    signal.signal(signal.SIGTERM, _handle_sig)
    signal.signal(signal.SIGINT, _handle_sig)

    # Serve in current thread; HTTPServer.shutdown() will stop serve_forever
    try:
        httpd.serve_forever(poll_interval=0.5)
    finally:
        try:
            httpd.server_close()
        except Exception:
            pass
        # Best-effort cleanup state
        try:
            if STATE_FILE.exists():
                STATE_FILE.unlink()
        except Exception:
            pass
        sys.stdout.write("[AI-SERVER] stopped\n")


def main():
    # Basic stdout banner
    sys.stdout.write("\n=== AI Server Worker (single-file) ===\n")
    sys.stdout.write(f"PID: {os.getpid()}\n")
    sys.stdout.write(f"STATE_DIR: {STATE_DIR}\n")
    sys.stdout.flush()

    _serve_forever()


if __name__ == "__main__":
    main()

