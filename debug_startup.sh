#!/bin/bash

# LibreOffice Startup Debug Script
# This script will help us identify the exact crash location when LibreOffice starts

echo "🔍 LibreOffice Startup Debug Script"
echo "This will help identify the crash when LibreOffice starts up"

# Set up debugging environment
export SAL_LOG="+WARN+INFO.cppu.typelib+INFO.unotools.config+INFO.vcl.app"
export SAL_LOG_FILE="/tmp/libreoffice_startup_debug.log"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LO_APP="$SCRIPT_DIR/instdir/LibreOfficeDev.app"

echo "📍 LibreOffice path: $LO_APP"

if [ ! -d "$LO_APP" ]; then
    echo "❌ LibreOfficeDev.app not found! Make sure the build completed successfully."
    echo "💡 Run 'make' to build LibreOffice first"
    exit 1
fi

echo "🚀 Starting LibreOffice with debugging enabled..."
echo "📝 Debug log will be written to: $SAL_LOG_FILE"

# Clear previous log
> "$SAL_LOG_FILE"

# Start LibreOffice and capture crash information
echo "⚡ Launching LibreOffice..."

# Try to start LibreOffice - this may crash
open -a "$LO_APP" --args --safe-mode 2>&1 &
LO_PID=$!

echo "🔄 LibreOffice PID: $LO_PID"
echo "⏳ Waiting for startup or crash..."

# Monitor for 30 seconds
for i in {1..30}; do
    if ! kill -0 $LO_PID 2>/dev/null; then
        echo "💥 LibreOffice process ended (crashed or closed)"
        break
    fi
    echo -n "."
    sleep 1
done

echo ""

# Check if process is still running
if kill -0 $LO_PID 2>/dev/null; then
    echo "✅ LibreOffice appears to be running successfully!"
    echo "🎉 No crash detected during startup"
else
    echo "❌ LibreOffice crashed or exited during startup"
    echo ""
    echo "📊 Debug information:"
    
    # Show debug log if it exists
    if [ -f "$SAL_LOG_FILE" ]; then
        echo "🔍 Last 50 lines of debug log:"
        tail -50 "$SAL_LOG_FILE"
        echo ""
        echo "📁 Full debug log saved to: $SAL_LOG_FILE"
    fi
    
    # Check system crash logs
    echo "🕵️  Checking for crash reports..."
    CRASH_DIR="$HOME/Library/Logs/DiagnosticReports"
    if [ -d "$CRASH_DIR" ]; then
        # Look for recent LibreOffice crashes
        find "$CRASH_DIR" -name "*LibreOffice*" -o -name "*soffice*" -o -name "*LibreOfficeDev*" | \
        xargs ls -lt | head -5
    fi
fi

echo ""
echo "🛠️  For manual investigation:"
echo "   • Debug log: $SAL_LOG_FILE"
echo "   • Crash reports: ~/Library/Logs/DiagnosticReports/"
echo "   • To run with more debugging: export SAL_LOG=\"+ALL\""
echo ""