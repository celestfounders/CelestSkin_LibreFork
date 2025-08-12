#!/usr/bin/env python3
"""
Private Backend Auto-Start using LibreOffice Python
Uses LibreOffice's built-in Python environment with all dependencies
"""

import os
import sys
import subprocess
import threading
import time
import signal
from pathlib import Path

class LibreOfficeBackendManager:
    """Manages private backend using LibreOffice's Python environment"""
    
    def __init__(self):
        # Use the Python executable running this script
        self.lo_python = sys.executable
        self.ai_server_process = None
        self.monitoring = False

        
    def check_libreoffice_running(self):
        """Check if LibreOffice is running using simple process check"""
        try:
            result = subprocess.run(['pgrep', '-f', 'soffice'], 
                                  capture_output=True, text=True)
            return result.returncode == 0
        except Exception:
            return False
    
    def cleanup_zombie_backends(self):
        """Clean up any zombie backend processes"""
        try:
            print("🧹 Cleaning up zombie backend processes...")
            # Kill any existing ai_server_worker.py processes
            subprocess.run(['pkill', '-f', 'ai_server_worker.py'], 
                          capture_output=True)
            time.sleep(1)
            print("✅ Zombie cleanup completed")
        except Exception as e:
            print(f"⚠️ Zombie cleanup failed: {e}")
    
    def is_backend_running(self):
        """Check if backend is already running"""
        state_file = Path("/tmp/private_backend_state/ai_server.json")
        if state_file.exists():
            try:
                import json
                with open(state_file) as f:
                    state = json.load(f)
                
                # Simple PID check
                result = subprocess.run(['ps', '-p', str(state.get('pid', 0))], 
                                      capture_output=True)
                if result.returncode == 0:
                    print(f"✅ Backend already running (PID: {state['pid']})")
                    return True
                else:
                    state_file.unlink()  # Remove stale state
            except Exception:
                pass
        return False
    
    def start_ai_server(self):
        """Start AI server using LibreOffice Python"""
        try:
            if self.is_backend_running():
                return True
            
            # Resolve ai_server_worker relative to the installed app bundle
            # This script is delivered to program/private_backend/ in instdir.
            # When run in-place from build tree, fall back to source path.
            candidates = [
                Path(sys.executable).resolve().parent.parent / "Resources" / "program" / "private_backend" / "source" / "python_service" / "ai_server_worker.py",
                Path(sys.executable).resolve().parent / "private_backend" / "source" / "python_service" / "ai_server_worker.py",
                Path(__file__).parent / "source" / "python_service" / "ai_server_worker.py",
            ]
            ai_server_script = next((p for p in candidates if p.exists()), None)
            if not ai_server_script.exists():
                print(f"❌ AI server script not found: {ai_server_script}")
                return False
            
            print("🚀 Starting AI server with LibreOffice Python...")
            
            # Start with LibreOffice Python
            self.ai_server_process = subprocess.Popen([
                self.lo_python, str(ai_server_script)
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            
            # Give it time to start
            time.sleep(3)
            
            if self.ai_server_process.poll() is None:
                print(f"✅ AI server started (PID: {self.ai_server_process.pid})")
                return True
            else:
                stdout, stderr = self.ai_server_process.communicate()
                print(f"❌ AI server failed to start")
                if stderr:
                    print(f"Error: {stderr.decode()}")
                return False
                
        except Exception as e:
            print(f"❌ Error starting AI server: {e}")
            return False
    
    def monitor_libreoffice(self):
        """Monitor LibreOffice and shutdown when it exits"""
        self.monitoring = True
        print("👁️  Monitoring LibreOffice...")
        
        while self.monitoring:
            if not self.check_libreoffice_running():
                print("📊 LibreOffice has exited, shutting down...")
                self.shutdown()
                break
            time.sleep(5)
    
    def shutdown(self):
        """Shutdown the backend"""
        print("🔧 Shutting down private backend...")
        self.monitoring = False
        
        # First try to terminate the direct child process
        if self.ai_server_process and self.ai_server_process.poll() is None:
            try:
                self.ai_server_process.terminate()
                self.ai_server_process.wait(timeout=3)
                print("✅ AI server stopped")
            except:
                self.ai_server_process.kill()
                print("🔨 AI server force-killed")
        
        # AGGRESSIVE: Force kill ALL ai_server_worker.py processes
        try:
            print("🧹 Force cleaning ALL ai_server_worker.py processes...")
            subprocess.run(['pkill', '-9', '-f', 'ai_server_worker.py'], 
                          capture_output=True)
            time.sleep(1)
            print("✅ All AI server processes terminated")
        except Exception as e:
            print(f"⚠️ Force cleanup failed: {e}")
        
        # Clean state files
        try:
            state_dir = Path("/tmp/private_backend_state")
            if state_dir.exists():
                for file in state_dir.glob("*.json"):
                    file.unlink()
                print("🧹 State files cleaned")
        except:
            pass
        
        print("✅ Shutdown complete")
    
    def start(self):
        """Start the backend manager"""
        print("🚀 LibreOffice Private Backend Starting...")
        
        # Check LibreOffice Python
        if not Path(self.lo_python).exists():
            print(f"❌ LibreOffice Python not found: {self.lo_python}")
            return
        
        # Clean up any zombie processes first
        self.cleanup_zombie_backends()
        
        # Start AI server
        if not self.start_ai_server():
            return
        
        # Setup signal handlers
        signal.signal(signal.SIGTERM, lambda s, f: self.shutdown())
        signal.signal(signal.SIGINT, lambda s, f: self.shutdown())
        
        # Start monitoring
        monitor_thread = threading.Thread(target=self.monitor_libreoffice, daemon=True)
        monitor_thread.start()
        
        print("✅ Private backend started with LibreOffice Python")
        
        # Keep running
        try:
            while self.monitoring:
                time.sleep(1)
        except KeyboardInterrupt:
            self.shutdown()

def main():
    """Main entry point"""
    manager = LibreOfficeBackendManager()
    manager.start()

if __name__ == "__main__":
    main()
