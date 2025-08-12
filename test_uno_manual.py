#!/usr/bin/env python3

import sys
import os

# Add LibreOffice Python path
lo_path = os.path.join(os.getcwd(), "instdir/LibreOfficeDev.app/Contents/Resources")
sys.path.insert(0, lo_path)

try:
    import uno
    from com.sun.star.connection import NoConnectException
    from com.sun.star.task import XJob
    print("✓ UNO imports successful")
    
    # Connect to LibreOffice
    local_context = uno.getComponentContext()
    resolver = local_context.ServiceManager.createInstanceWithContext(
        "com.sun.star.bridge.UnoUrlResolver", local_context)
    
    try:
        # Try to connect to running LibreOffice instance
        context = resolver.resolve("uno:socket,host=localhost,port=2083;urp;StarOffice.ComponentContext")
        print("✓ Connected to LibreOffice")
        
        # Try to create our service
        service_manager = context.ServiceManager
        private_backend = service_manager.createInstance("com.sun.star.private.backend.PrivateBackendService")
        
        if private_backend:
            print("✓ PrivateBackendService created successfully!")
            
            # Test if it implements XJob
            if hasattr(private_backend, 'execute'):
                print("✓ Service implements XJob interface")
                
                # Try to execute it
                result = private_backend.execute([])
                print(f"✓ Service executed, result: {result}")
            else:
                print("✗ Service doesn't implement XJob interface")
        else:
            print("✗ Failed to create PrivateBackendService")
            
    except NoConnectException:
        print("✗ Could not connect to LibreOffice - starting with pipe connection...")
        
        # Try pipe connection instead
        try:
            context = resolver.resolve("uno:pipe,name=libreoffice_pipe;urp;StarOffice.ComponentContext")
            print("✓ Connected via pipe")
            service_manager = context.ServiceManager
            private_backend = service_manager.createInstance("com.sun.star.private.backend.PrivateBackendService")
            if private_backend:
                print("✓ PrivateBackendService created via pipe!")
        except:
            print("✗ Pipe connection also failed")
            
except ImportError as e:
    print(f"✗ Failed to import UNO: {e}")
    print("LibreOffice path:", lo_path)
    print("Available files:", os.listdir(lo_path) if os.path.exists(lo_path) else "Path not found")
except Exception as e:
    print(f"✗ Error: {e}")