# -*- coding: utf-8 -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

"""
Private Backend Python UNO Service

This service provides the main interface for the private backend system.
It handles:
- Starting and managing Python worker processes
- Communication between LibreOffice and backend services
- Service discovery and lifecycle management
"""

import uno
import unohelper
import os
import sys
import subprocess
import threading
import time
from com.sun.star.task import XJob

class PrivateBackendService(unohelper.Base, XJob):
    """
    Main Python UNO service for private backend management.
    
    This service can be invoked from LibreOffice and provides:
    - Worker process management
    - Service orchestration
    - Communication interfaces
    """
    
    def __init__(self, ctx):
        """Initialize the private backend service"""
        print("🚀 PrivateBackendService.__init__ called! Service is being created.")
        self.ctx = ctx
        self.workers = {}  # Track running worker processes
        self.backend_manager = None
        
        # Import and initialize the backend manager
        try:
            from .BackendManager import BackendManager
            self.backend_manager = BackendManager(ctx)
            
            # Auto-start backend services on initialization
            print("Private Backend Service: Starting auto-initialization...")
            if self.backend_manager:
                self.backend_manager.start_all_services()
                # Start monitoring loop for auto-restart and state writing
                try:
                    self.backend_manager.start_monitoring()
                except Exception as _:
                    pass
                print("Private Backend Service: Auto-start completed")
        except Exception as e:
            print(f"Error initializing BackendManager: {e}")
            import traceback
            traceback.print_exc()
    
    def execute(self, args):
        """
        Execute backend operations (XJob interface)
        
        Args:
            args: Arguments passed from LibreOffice
            
        Returns:
            Result of the operation
        """
        print(f"🎯 PrivateBackendService.execute called with args: {args}")
        try:
            # Start the backend services
            if self.backend_manager:
                print("📋 Backend manager available, starting services...")
                self.backend_manager.start_all_services()
                print("✅ Services started successfully")
                return uno.Any("string", "Private backend services started")
            else:
                print("❌ Backend manager not available")
                return uno.Any("string", "Backend manager not available")
                
        except Exception as e:
            print(f"Error in PrivateBackendService.execute: {e}")
            import traceback
            traceback.print_exc()
            return uno.Any("string", f"Error: {e}")
    
    def start_worker(self, worker_name, script_path, args=None):
        """
        Start a specific worker process
        
        Args:
            worker_name: Unique name for the worker
            script_path: Path to the Python script
            args: Optional arguments for the script
            
        Returns:
            Success status
        """
        if self.backend_manager:
            return self.backend_manager.start_worker(worker_name, script_path, args)
        return False
    
    def stop_worker(self, worker_name):
        """
        Stop a specific worker process
        
        Args:
            worker_name: Name of the worker to stop
            
        Returns:
            Success status
        """
        if self.backend_manager:
            return self.backend_manager.stop_worker(worker_name)
        return False
    
    def get_worker_status(self, worker_name=None):
        """
        Get status of workers
        
        Args:
            worker_name: Optional specific worker name
            
        Returns:
            Worker status information
        """
        if self.backend_manager:
            return self.backend_manager.get_worker_status(worker_name)
        return {}
    
    def execute_backend_command(self, command, data=None):
        """
        Execute a command in the backend system
        
        Args:
            command: Command to execute
            data: Optional data for the command
            
        Returns:
            Command result
        """
        if self.backend_manager:
            return self.backend_manager.execute_command(command, data)
        return None


# UNO service registration
g_ImplementationHelper = unohelper.ImplementationHelper()
g_ImplementationHelper.addImplementation(
    PrivateBackendService,
    "com.sun.star.private.backend.PrivateBackendService",
    ("com.sun.star.private.backend.PrivateBackendService", "com.sun.star.task.Job")
)