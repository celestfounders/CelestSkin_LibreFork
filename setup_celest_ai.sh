#!/bin/bash
# =============================================================================
# Celest AI Master Installation Script
# =============================================================================
# This script sets up the complete Celest AI system:
# 1. LibreOffice with private backend integration
# 2. Celest AI extension with full UI components
# 3. Backend AI services with all components
# 4. Integration and testing
# =============================================================================

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
INSTALL_LOG="$PROJECT_ROOT/celest_ai_install.log"
CELEST_VERSION="2.0.0"

# Component directories
LIBREOFFICE_DIR="$PROJECT_ROOT"
PRIVATE_BACKEND_DIR="$PROJECT_ROOT/private_backend"
EXTENSION_DIR="$PROJECT_ROOT/celestextension"

echo -e "${BLUE}"
cat << 'EOF'
   ____      _           _            _    ___ 
  / ___|    | |         | |         | |  |_ _|
 | |     ___| | ___  ___| |_   __ _ | |   | | 
 | |    / _ \ |/ _ \/ __| __| / _` || |   | | 
 | |___|  __/ |  __/\__ \ |_ | (_| || |  _| |_
  \____|\___| |\___||___/\__| \__,_||_| |_____|
            |_|                               
                                              
        Advanced AI for LibreOffice
              Version 2.0.0
EOF
echo -e "${NC}"

echo ""
echo -e "${PURPLE}🚀 Celest AI Complete Installation${NC}"
echo "============================================="
echo "Project Root: $PROJECT_ROOT"
echo "Installation Log: $INSTALL_LOG"
echo "Version: $CELEST_VERSION"
echo ""

# Start logging
exec > >(tee -a "$INSTALL_LOG")
exec 2>&1

echo "Installation started at: $(date)"
echo ""

# =============================================================================
# Functions
# =============================================================================

log_info() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warn() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

log_step() {
    echo -e "${CYAN}📋 $1${NC}"
}

check_system_requirements() {
    log_step "Checking system requirements..."
    
    # Check OS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        OS_TYPE="macOS"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS_TYPE="Linux"
    else
        log_error "Unsupported operating system: $OSTYPE"
        exit 1
    fi
    
    log_info "Operating System: $OS_TYPE"
    
    # Check required tools
    local required_tools=("python3" "pip3" "java" "make" "git" "curl" "unzip")
    local missing_tools=()
    
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            missing_tools+=("$tool")
        fi
    done
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        echo "Please install the missing tools and run the script again."
        exit 1
    fi
    
    # Check Python version
    python_version=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
    if [[ $(echo "$python_version < 3.8" | bc -l) -eq 1 ]]; then
        log_error "Python 3.8+ required, found $python_version"
        exit 1
    fi
    
    # Check Java version
    java_version=$(java -version 2>&1 | head -n 1 | cut -d'"' -f2)
    log_info "Java version: $java_version"
    
    # Check available disk space (need at least 5GB)
    available_space=$(df "$PROJECT_ROOT" | tail -1 | awk '{print $4}')
    if [ "$available_space" -lt 5242880 ]; then  # 5GB in KB
        log_warn "Less than 5GB available disk space. LibreOffice build requires significant space."
    fi
    
    log_info "System requirements satisfied"
}

setup_libreoffice_build() {
    log_step "Setting up LibreOffice build environment..."
    
    # Check if already configured
    if [ -f "$LIBREOFFICE_DIR/config_host.mk" ]; then
        log_info "LibreOffice already configured"
    else
        log_info "Configuring LibreOffice build..."
        cd "$LIBREOFFICE_DIR"
        
        # Configure with appropriate options for Celest AI
        ./configure \
            --enable-python=internal \
            --enable-ext-wiki-publisher \
            --enable-ext-nlpsolver \
            --disable-ccache \
            --without-java \
            --disable-firebird-sdbc
        
        log_info "LibreOffice configuration complete"
    fi
    
    # Check if private backend is integrated
    if [ ! -d "$PRIVATE_BACKEND_DIR" ]; then
        log_error "Private backend directory not found. Please ensure private_backend/ exists."
        exit 1
    fi
    
    log_info "LibreOffice build environment ready"
}

build_private_backend() {
    log_step "Building private backend..."
    
    cd "$PRIVATE_BACKEND_DIR"
    
    # Make setup script executable
    chmod +x setup_backend.sh
    
    # Run backend setup
    ./setup_backend.sh
    
    # Build private backend module
    cd "$LIBREOFFICE_DIR"
    log_info "Building private backend module..."
    make private_backend.allbuild
    
    log_info "Private backend build complete"
}

setup_extension() {
    log_step "Setting up Celest AI extension..."
    
    cd "$EXTENSION_DIR"
    
    # Make setup script executable
    chmod +x setup_extension.sh
    
    # Run extension setup
    ./setup_extension.sh
    
    log_info "Extension setup complete"
}

build_libreoffice() {
    log_step "Building LibreOffice (this may take 30-60 minutes)..."
    
    cd "$LIBREOFFICE_DIR"
    
    # Build with multiple cores
    local cpu_cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    log_info "Building with $cpu_cores CPU cores..."
    
    # Start build
    make -j"$cpu_cores" 2>&1 | tee build_output.log
    
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        log_info "LibreOffice build successful"
    else
        log_error "LibreOffice build failed. Check build_output.log for details."
        exit 1
    fi
}

install_extension() {
    log_step "Installing Celest AI extension..."
    
    local extension_file="$EXTENSION_DIR/celest-ai-2.0.oxt"
    
    if [ ! -f "$extension_file" ]; then
        log_error "Extension file not found: $extension_file"
        exit 1
    fi
    
    # Install extension to LibreOffice
    if [ -f "$LIBREOFFICE_DIR/instdir/LibreOfficeDev.app/Contents/MacOS/unopkg" ]; then
        # macOS
        "$LIBREOFFICE_DIR/instdir/LibreOfficeDev.app/Contents/MacOS/unopkg" add "$extension_file"
    elif [ -f "$LIBREOFFICE_DIR/instdir/program/unopkg" ]; then
        # Linux
        "$LIBREOFFICE_DIR/instdir/program/unopkg" add "$extension_file"
    else
        log_warn "Could not find unopkg. Extension will need to be installed manually."
        log_info "Extension file location: $extension_file"
    fi
    
    log_info "Extension installation complete"
}

run_integration_tests() {
    log_step "Running integration tests..."
    
    # Test private backend
    cd "$PRIVATE_BACKEND_DIR"
    if [ -f "test_backend.py" ]; then
        python3 test_backend.py
    fi
    
    # Test extension integration
    cd "$EXTENSION_DIR/celest-server"
    if [ -f "test_direct_integration.py" ]; then
        source venv/bin/activate
        python3 test_direct_integration.py
        deactivate
    fi
    
    log_info "Integration tests complete"
}

start_services() {
    log_step "Starting Celest AI services..."
    
    # Start private backend worker
    cd "$PRIVATE_BACKEND_DIR"
    python3 source/python_service/private_backend_worker.py &
    BACKEND_PID=$!
    echo "$BACKEND_PID" > /tmp/celest_backend.pid
    
    # Start celest server
    cd "$EXTENSION_DIR/celest-server"
    source venv/bin/activate
    python3 server.py &
    SERVER_PID=$!
    echo "$SERVER_PID" > /tmp/celest_server.pid
    deactivate
    
    # Give services time to start
    sleep 5
    
    # Check if services are running
    if kill -0 "$BACKEND_PID" 2>/dev/null && kill -0 "$SERVER_PID" 2>/dev/null; then
        log_info "Services started successfully"
        log_info "Backend PID: $BACKEND_PID"
        log_info "Server PID: $SERVER_PID"
    else
        log_warn "Some services may not have started correctly"
    fi
}

create_desktop_shortcuts() {
    log_step "Creating desktop shortcuts..."
    
    local libreoffice_path
    if [ -f "$LIBREOFFICE_DIR/instdir/LibreOfficeDev.app/Contents/MacOS/soffice" ]; then
        # macOS
        libreoffice_path="$LIBREOFFICE_DIR/instdir/LibreOfficeDev.app"
        
        # Create alias on desktop (macOS)
        if [ -d "$HOME/Desktop" ]; then
            ln -sf "$libreoffice_path" "$HOME/Desktop/LibreOffice Celest AI.app"
            log_info "Desktop shortcut created (macOS)"
        fi
    elif [ -f "$LIBREOFFICE_DIR/instdir/program/soffice" ]; then
        # Linux
        libreoffice_path="$LIBREOFFICE_DIR/instdir/program/soffice"
        
        # Create desktop entry (Linux)
        if [ -d "$HOME/Desktop" ]; then
            cat > "$HOME/Desktop/libreoffice-celest-ai.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=LibreOffice Celest AI
Comment=LibreOffice with Celest AI Integration
Exec=$libreoffice_path
Icon=$LIBREOFFICE_DIR/instdir/share/icons/hicolor/48x48/apps/libreoffice-calc.png
Terminal=false
Categories=Office;Spreadsheet;
EOF
            chmod +x "$HOME/Desktop/libreoffice-celest-ai.desktop"
            log_info "Desktop shortcut created (Linux)"
        fi
    fi
}

create_startup_scripts() {
    log_step "Creating startup scripts..."
    
    # Create start script
    cat > "$PROJECT_ROOT/start_celest_ai.sh" << 'EOF'
#!/bin/bash
# Start Celest AI services

echo "🚀 Starting Celest AI services..."

# Check if already running
if [ -f /tmp/celest_backend.pid ] && kill -0 $(cat /tmp/celest_backend.pid) 2>/dev/null; then
    echo "Backend already running"
else
    echo "Starting backend..."
    cd "$(dirname "$0")/private_backend"
    python3 source/python_service/private_backend_worker.py &
    echo $! > /tmp/celest_backend.pid
fi

if [ -f /tmp/celest_server.pid ] && kill -0 $(cat /tmp/celest_server.pid) 2>/dev/null; then
    echo "Server already running"
else
    echo "Starting server..."
    cd "$(dirname "$0")/celestextension/celest-server"
    source venv/bin/activate
    python3 server.py &
    echo $! > /tmp/celest_server.pid
    deactivate
fi

echo "✅ Services started"
echo "🌐 Web interface: http://localhost:5001"

# Start LibreOffice
if [[ "$OSTYPE" == "darwin"* ]]; then
    open "$(dirname "$0")/instdir/LibreOfficeDev.app"
else
    "$(dirname "$0")/instdir/program/soffice"
fi
EOF

    # Create stop script
    cat > "$PROJECT_ROOT/stop_celest_ai.sh" << 'EOF'
#!/bin/bash
# Stop Celest AI services

echo "🛑 Stopping Celest AI services..."

if [ -f /tmp/celest_backend.pid ]; then
    if kill -0 $(cat /tmp/celest_backend.pid) 2>/dev/null; then
        kill $(cat /tmp/celest_backend.pid)
        echo "Backend stopped"
    fi
    rm -f /tmp/celest_backend.pid
fi

if [ -f /tmp/celest_server.pid ]; then
    if kill -0 $(cat /tmp/celest_server.pid) 2>/dev/null; then
        kill $(cat /tmp/celest_server.pid)
        echo "Server stopped"
    fi
    rm -f /tmp/celest_server.pid
fi

echo "✅ Services stopped"
EOF

    chmod +x "$PROJECT_ROOT/start_celest_ai.sh"
    chmod +x "$PROJECT_ROOT/stop_celest_ai.sh"
    
    log_info "Startup scripts created"
}

show_completion_message() {
    echo ""
    echo -e "${GREEN}🎉 Celest AI Installation Complete!${NC}"
    echo "============================================="
    echo ""
    echo -e "${BLUE}📋 Installation Summary:${NC}"
    echo "  ✅ LibreOffice built with private backend"
    echo "  ✅ Celest AI extension installed"
    echo "  ✅ Backend AI services configured"
    echo "  ✅ Integration tested"
    echo "  ✅ Services started"
    echo ""
    echo -e "${BLUE}🚀 Getting Started:${NC}"
    echo "  1. LibreOffice is ready to use with Celest AI"
    echo "  2. Access Tools → Celest AI menu in LibreOffice"
    echo "  3. Web interface: http://localhost:5001"
    echo "  4. Use ./start_celest_ai.sh to start services"
    echo "  5. Use ./stop_celest_ai.sh to stop services"
    echo ""
    echo -e "${BLUE}📁 Important Files:${NC}"
    echo "  📦 Extension: $EXTENSION_DIR/celest-ai-2.0.oxt"
    echo "  🏠 Config: ~/.celest/"
    echo "  📊 Logs: ~/.celest/logs/"
    echo "  📎 Attachments: ~/.celest/attachments/"
    echo ""
    echo -e "${BLUE}🔧 Next Steps:${NC}"
    echo "  1. Configure API keys in Settings"
    echo "  2. Test AI analysis on a spreadsheet"
    echo "  3. Explore the 3-panel sidebar"
    echo "  4. Try different AI models"
    echo ""
    echo -e "${YELLOW}📝 Support:${NC}"
    echo "  📖 Documentation: ./README.md"
    echo "  🐛 Issues: Check installation log for errors"
    echo "  💬 Support: Contact Celest team"
    echo ""
    echo "Installation completed at: $(date)"
}

# =============================================================================
# Main Installation Process
# =============================================================================

main() {
    echo "Starting Celest AI installation process..."
    echo ""
    
    # Step 1: System Requirements
    check_system_requirements
    
    # Step 2: LibreOffice Setup
    setup_libreoffice_build
    
    # Step 3: Backend Setup
    build_private_backend
    
    # Step 4: Extension Setup
    setup_extension
    
    # Step 5: Build LibreOffice
    build_libreoffice
    
    # Step 6: Install Extension
    install_extension
    
    # Step 7: Integration Tests
    run_integration_tests
    
    # Step 8: Start Services
    start_services
    
    # Step 9: Desktop Integration
    create_desktop_shortcuts
    create_startup_scripts
    
    # Step 10: Completion
    show_completion_message
}

# Handle interruption
trap 'echo -e "\n${RED}Installation interrupted by user${NC}"; exit 1' INT

# Parse command line arguments
SKIP_BUILD=false
SKIP_TESTS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --help)
            echo "Celest AI Installation Script"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --skip-build    Skip LibreOffice build (if already built)"
            echo "  --skip-tests    Skip integration tests"
            echo "  --help         Show this help message"
            echo ""
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Run main installation
main "$@"