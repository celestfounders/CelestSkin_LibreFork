#!/usr/bin/env python3
"""
Manual test script to trigger our PrivateBackendService
"""

import sys
import os

# Add LibreOffice Python paths
lo_path = os.path.join(os.getcwd(), "instdir/LibreOfficeDev.app/Contents/Resources")
python_path = os.path.join(lo_path, "python")
sys.path.insert(0, lo_path)
sys.path.insert(0, python_path)

try:
    print("🔍 Testing UNO connection...")
    
    # Try to import UNO 
    import uno
    from com.sun.star.beans import PropertyValue
    from com.sun.star.task import XJob
    
    print("✅ UNO import successful")
    
    # Connect to LibreOffice
    localContext = uno.getComponentContext()
    resolver = localContext.ServiceManager.createInstanceWithContext(
        "com.sun.star.bridge.UnoUrlResolver", localContext)
    
    # Try different connection methods
    context = None
    connection_attempts = [
        "uno:socket,host=localhost,port=2083;urp;StarOffice.ComponentContext",
        "uno:pipe,name=libreoffice_pipe;urp;StarOffice.ComponentContext"
    ]
    
    for conn_string in connection_attempts:
        try:
            print(f"🔌 Trying connection: {conn_string}")
            context = resolver.resolve(conn_string)
            print("✅ Connected successfully!")
            break
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            continue
    
    if not context:
        print("💡 Starting LibreOffice with UNO interface...")
        os.system('./instdir/LibreOfficeDev.app/Contents/MacOS/soffice --accept="socket,host=localhost,port=2083;urp;StarOffice.ComponentContext" --headless &')
        import time
        time.sleep(3)
        
        # Try again
        try:
            context = resolver.resolve("uno:socket,host=localhost,port=2083;urp;StarOffice.ComponentContext")
            print("✅ Connected after starting LibreOffice!")
        except Exception as e:
            print(f"❌ Still failed to connect: {e}")
            sys.exit(1)
    
    # Test our service
    print("🎯 Testing PrivateBackendService...")
    service_manager = context.ServiceManager
    
    try:
        # Try to create our service
        private_backend = service_manager.createInstance("com.sun.star.private.backend.PrivateBackendService")
        if private_backend:
            print("✅ PrivateBackendService created successfully!")
            
            # Test execute method
            result = private_backend.execute([])
            print(f"🎯 Service execute result: {result}")
            
        else:
            print("❌ Failed to create PrivateBackendService")
            
    except Exception as e:
        print(f"❌ Error creating service: {e}")
        import traceback
        traceback.print_exc()
        
        # List available services
        print("\n📋 Available services containing 'private' or 'backend':")
        try:
            services = service_manager.getAvailableServiceNames()
            for service in services:
                if 'private' in service.lower() or 'backend' in service.lower():
                    print(f"  - {service}")
        except:
            print("  Could not list services")

except Exception as e:
    print(f"❌ Failed to import UNO or connect: {e}")
    import traceback
    traceback.print_exc()