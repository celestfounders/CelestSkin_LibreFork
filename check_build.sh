#!/bin/bash

# Build Progress Monitor
echo "📊 LibreOffice Build Progress Monitor"
echo "======================================"

# Check if make is running
MAKE_PIDS=$(pgrep -f "make.*libreoffice" || echo "")
if [ -n "$MAKE_PIDS" ]; then
    echo "✅ Build is currently running (PIDs: $MAKE_PIDS)"
    
    # Show CPU usage
    echo "💻 CPU Usage:"
    ps -p $MAKE_PIDS -o pid,pcpu,pmem,time,comm
    
    echo ""
    echo "⏱️  Build has been running for:"
    ps -p $MAKE_PIDS -o etime
    
else
    echo "❌ No active make process found"
fi

echo ""
echo "📁 Current directory contents:"
ls -la instdir/ 2>/dev/null || echo "   instdir/ not created yet"
ls -la workdir/ 2>/dev/null || echo "   workdir/ not created yet"

echo ""
echo "🔍 Recent build output (last 10 lines):"
# Try to find build logs
if [ -f "build.log" ]; then
    tail -10 build.log
elif [ -f "config.log" ]; then
    echo "📋 Configuration completed, build in progress..."
else
    echo "📝 No build log found - build may be in early stages"
fi

echo ""
echo "💡 To monitor continuously: watch -n 5 ./check_build.sh"
echo "💡 To check final result: ls -la instdir/LibreOfficeDev.app"