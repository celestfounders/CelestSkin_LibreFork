# 🌟 Celest AI for LibreOffice

**Advanced AI-powered spreadsheet analysis and automation system**

![Version](https://img.shields.io/badge/Version-2.0.0-blue)
![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux-green)
![LibreOffice](https://img.shields.io/badge/LibreOffice-7.0%2B-orange)
![Python](https://img.shields.io/badge/Python-3.8%2B-blue)

---

## 📋 Table of Contents

- [🎯 What is Celest AI?](#-what-is-celest-ai)
- [✨ Features](#-features)
- [🏗️ Architecture](#️-architecture)
- [🚀 Quick Start](#-quick-start)
- [📦 Installation](#-installation)
- [🔧 Configuration](#-configuration)
- [📚 Usage Guide](#-usage-guide)
- [🧩 Components](#-components)
- [🛠️ Development](#️-development)
- [🐛 Troubleshooting](#-troubleshooting)
- [📖 API Reference](#-api-reference)
- [🤝 Contributing](#-contributing)

---

## 🎯 What is Celest AI?

Celest AI is a comprehensive AI integration for LibreOffice Calc that provides intelligent spreadsheet analysis, automated operations, and real-time AI assistance. It combines the power of modern AI models with LibreOffice's robust spreadsheet capabilities.

### 🌟 Key Highlights

- **Token-by-token AI conversation display** - See AI thinking in real-time
- **Smart edits timeline** - Full undo/redo with change tracking
- **Multi-model support** - Switch between OpenAI, Claude, and other models
- **Intelligent data analysis** - Automatic insights and recommendations
- **Cost tracking** - Monitor AI usage and costs
- **Vector-based code retrieval** - Learn from high-quality examples

---

## ✨ Features

### 🧠 AI Intelligence
- **Real-time AI Analysis** - Instant insights on your data
- **Code Generation** - AI writes LibreOffice scripts for you
- **Error Recovery** - AI fixes its own mistakes with feedback loops
- **Pattern Recognition** - Identify trends and anomalies
- **Natural Language Queries** - Ask questions about your data

### 🎨 User Interface
- **3-Panel Sidebar**:
  - 💬 **Conversation Panel** - Token-by-token AI thoughts
  - 📊 **Edits Timeline** - Visual change history with undo/redo
  - ⚙️ **Settings Panel** - Model switching, API keys, cost tracking
- **Menu & Toolbar Integration** - Native LibreOffice experience
- **Live Model Statistics** - Real-time usage and cost monitoring

### 🔧 Automation
- **Batch Operations** - Process large datasets efficiently
- **Formula Generation** - AI creates complex formulas
- **Data Validation** - Intelligent error checking
- **Format Optimization** - Automatic styling and formatting
- **Chart Generation** - AI-powered visualizations

### 🏗️ Technical Excellence
- **Sandboxed Execution** - Safe code execution environment
- **Vector Database** - High-quality code example retrieval
- **Multi-threading** - Responsive UI during processing
- **Atomic Operations** - All-or-nothing change sets
- **Session Management** - Persistent conversation history

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    LibreOffice Calc                        │
├─────────────────────────────────────────────────────────────┤
│                 Celest AI Extension                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │ Conversation│ │ Edits       │ │ Settings                │ │
│  │ Panel       │ │ Timeline    │ │ Panel                   │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │ Informer    │ │ Executor    │ │ Lifecycle               │ │
│  │ Component   │ │ Component   │ │ Component               │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ UNO Bridge
                              │
┌─────────────────────────────────────────────────────────────┐
│                    Private Backend                          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │ AI Planner  │ │ Retriever   │ │ Validator               │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│  │ LLM Client  │ │ Data Access │ │ Storage Engine          │ │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ API Calls
                              │
┌─────────────────────────────────────────────────────────────┐
│              External AI Services                           │
│         OpenAI • Claude • Local Models                     │
└─────────────────────────────────────────────────────────────┘
```

---

## 🚀 Quick Start

### 1️⃣ One-Command Installation

```bash
git clone https://github.com/your-org/celest-libreoffice.git
cd celest-libreoffice
chmod +x setup_celest_ai.sh
./setup_celest_ai.sh
```

### 2️⃣ Configure API Keys

1. Open LibreOffice Calc
2. Go to **Tools → Celest AI → Settings**
3. Add your API keys:
   - OpenAI API Key
   - Anthropic API Key (optional)
4. Select your preferred model

### 3️⃣ Start Using AI

1. Create or open a spreadsheet
2. Select data or click **Tools → Celest AI → Show Sidebar**
3. Ask AI: *"Analyze this data and suggest improvements"*
4. Watch real-time AI thinking and apply suggestions

---

## 📦 Installation

### Prerequisites

- **Operating System**: macOS 10.15+ or Linux (Ubuntu 20.04+)
- **Python**: 3.8 or higher
- **Java**: 11 or higher (for LibreOffice build)
- **Memory**: 8GB RAM minimum, 16GB recommended
- **Storage**: 10GB free space for build

### System Dependencies

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew dependencies
brew install python3 java git make cmake
```

#### Linux (Ubuntu/Debian)
```bash
# Install build dependencies
sudo apt update
sudo apt install -y build-essential git python3 python3-pip \
    openjdk-11-jdk make cmake curl unzip

# Additional LibreOffice build dependencies
sudo apt install -y libxml2-dev libxslt1-dev libcurl4-openssl-dev \
    libssl-dev zlib1g-dev libbz2-dev
```

### Installation Methods

#### Method 1: Complete Installation (Recommended)
```bash
./setup_celest_ai.sh
```

#### Method 2: Component-by-Component
```bash
# 1. Setup LibreOffice with private backend
cd private_backend
./setup_backend.sh

# 2. Build LibreOffice
make

# 3. Setup extension
cd ../celestextension
./setup_extension.sh

# 4. Install extension manually
```

#### Method 3: Development Setup
```bash
# Skip build if you have LibreOffice already
./setup_celest_ai.sh --skip-build

# Skip tests for faster setup
./setup_celest_ai.sh --skip-tests
```

---

## 🔧 Configuration

### API Keys Setup

Create `~/.celest/config/api_keys.json`:
```json
{
  "openai": {
    "api_key": "sk-your-openai-key",
    "organization": "your-org-id",
    "default_model": "gpt-4"
  },
  "anthropic": {
    "api_key": "sk-ant-your-key",
    "default_model": "claude-3-sonnet"
  }
}
```

### Model Configuration

Configure models in `~/.celest/config/models.json`:
```json
{
  "models": {
    "gpt-4": {
      "provider": "openai",
      "input_cost_per_1k": 0.03,
      "output_cost_per_1k": 0.06,
      "context_window": 8192,
      "capabilities": ["code", "analysis", "reasoning"]
    },
    "claude-3-sonnet": {
      "provider": "anthropic", 
      "input_cost_per_1k": 0.015,
      "output_cost_per_1k": 0.075,
      "context_window": 200000,
      "capabilities": ["code", "analysis", "reasoning", "long_context"]
    }
  }
}
```

### Extension Settings

Access via **Tools → Celest AI → Settings**:

- **Model Selection**: Choose your preferred AI model
- **Cost Limits**: Set daily/monthly spending limits
- **Auto-Analysis**: Enable automatic data analysis
- **Sidebar Position**: Left/right sidebar placement
- **Conversation History**: Number of messages to retain
- **Code Safety**: Validation strictness level

---

## 📚 Usage Guide

### Basic Workflow

1. **Open LibreOffice Calc** with Celest AI
2. **Load or create data** in your spreadsheet
3. **Open Celest AI Sidebar** (F12 or Tools menu)
4. **Interact with AI** through natural language

### Common Use Cases

#### 📊 Data Analysis
```
You: "Analyze the sales data in columns A-D and identify trends"

AI: I'll analyze your sales data. Let me examine the structure...
    [Real-time thinking displayed]
    
    Key findings:
    - 23% growth in Q3
    - Product Category C underperforming
    - Seasonal pattern detected
    
    Recommendations:
    - Focus marketing on Category C
    - Prepare for Q4 seasonal increase
```

#### 🧮 Formula Creation
```
You: "Create a formula to calculate commission based on tiered rates"

AI: I'll create a tiered commission formula...
    [Shows step-by-step reasoning]
    
    Formula: =IF(B2<1000, B2*0.05, IF(B2<5000, B2*0.07, B2*0.10))
    
    Applied to: Column C (Commission)
    Logic: 5% up to $1k, 7% up to $5k, 10% above $5k
```

#### 📈 Visualization
```
You: "Create a chart showing monthly revenue trends"

AI: Creating revenue trend chart...
    [Processing data structure]
    
    Chart type: Line chart
    X-axis: Months (Column A)
    Y-axis: Revenue (Column B)
    Added trendline with R² = 0.89
```

### Advanced Features

#### 🔄 Undo/Redo System
- **Visual Timeline**: See all AI changes in chronological order
- **Granular Control**: Undo specific changes without affecting others
- **Change Preview**: See what will change before applying
- **Batch Operations**: Group related changes together

#### 🎯 Code Retrieval
- **Vector Search**: Find relevant examples from high-quality code database
- **Learning System**: Improves suggestions based on successful operations
- **Context Awareness**: Considers your current data structure

#### 🛡️ Safety Features
- **Sandboxed Execution**: AI code runs in isolated environment
- **Validation Layers**: Multiple checks before applying changes
- **Rollback Capability**: Automatic snapshots before major changes
- **Data Integrity**: Prevents destructive operations

---

## 🧩 Components

### Extension Components

#### 🔍 Informer
**Purpose**: Captures and analyzes spreadsheet data
- Extracts active sheet ranges
- Infers data types automatically
- Maintains cached state for performance
- Supports SQL subview queries

**Files**: `extension/components/informer.py`

#### ⚡ Executor
**Purpose**: Applies AI-generated changes to spreadsheet
- Executes edit plans safely
- Records all changes for undo system
- Handles atomic operations
- Integrates with timeline UI

**Files**: `extension/components/executor.py`

#### 🔄 Lifecycle
**Purpose**: Manages extension lifecycle and backend communication
- Handles initialization and shutdown
- Manages session state
- Validates tokens and handles errors
- Coordinates with backend services

**Files**: `extension/components/lifecycle.py`

### Backend Components

#### 🧠 AI Planner
**Purpose**: Converts tasks into executable code
- Generates Python code from natural language
- Implements feedback loops for error correction
- Maintains reasoning history
- Integrates with vector retrieval

**Files**: `backend/ai/planner.py`

#### 🔍 Retriever
**Purpose**: Vector-based code example retrieval
- FAISS-powered similarity search
- High-quality code example database
- Learns from successful operations
- Provides contextual suggestions

**Files**: `backend/ai/retriever.py`

#### 🛡️ Validator
**Purpose**: Ensures code safety and correctness
- Static code analysis
- Destructive operation prevention
- Simulated execution testing
- Risk assessment

**Files**: `backend/ai/validator.py`

#### 💾 Storage Engine
**Purpose**: Persistent data storage with SQLite
- WAL mode for concurrency
- Session and conversation history
- Token usage tracking
- Undo/redo state management

**Files**: `backend/storage/sqlite_engine.py`, `backend/storage/models.py`

---

## 🛠️ Development

### Development Setup

```bash
# Clone repository
git clone https://github.com/your-org/celest-libreoffice.git
cd celest-libreoffice

# Setup development environment
python3 -m venv dev_env
source dev_env/bin/activate
pip install -r requirements-dev.txt

# Install pre-commit hooks
pre-commit install
```

### Building Components

#### Extension Development
```bash
cd celestextension
./setup_extension.sh

# Test extension
cd celest-server
source venv/bin/activate
python3 test_direct_integration.py
```

#### Backend Development
```bash
cd private_backend
./setup_backend.sh

# Test backend
python3 test_backend.py
```

### Running Tests

```bash
# Run all tests
python3 -m pytest

# Run specific component tests
python3 -m pytest tests/test_planner.py
python3 -m pytest tests/test_retriever.py
python3 -m pytest tests/test_executor.py

# Run integration tests
python3 tests/test_integration.py
```

### Code Quality

```bash
# Format code
black .

# Check linting
flake8 .

# Type checking
mypy .

# Security scanning
bandit -r .
```

---

## 🐛 Troubleshooting

### Common Issues

#### ❌ LibreOffice Build Fails
**Symptoms**: Make command fails with compilation errors

**Solutions**:
1. Check system dependencies: `./setup_celest_ai.sh --check-deps`
2. Clean build: `make clean && make`
3. Reduce parallel jobs: `make -j2` instead of `make -j8`
4. Check disk space: Need 10GB+ free

#### ❌ Extension Not Loading
**Symptoms**: Celest AI menu doesn't appear

**Solutions**:
1. Check extension installation: Tools → Extension Manager
2. Verify Python path in extension
3. Check LibreOffice logs: `~/.config/libreoffice/4/user/logs/`
4. Reinstall extension: `unopkg add celest-ai-2.0.oxt --force`

#### ❌ Backend Connection Failed
**Symptoms**: "Backend not ready" errors

**Solutions**:
1. Check backend status: `curl http://localhost:5001/api/status`
2. Restart backend: `./stop_celest_ai.sh && ./start_celest_ai.sh`
3. Check logs: `tail -f ~/.celest/logs/backend.log`
4. Verify Python environment: `source backend/venv/bin/activate`

#### ❌ AI Responses Slow/Empty
**Symptoms**: AI takes too long or returns empty responses

**Solutions**:
1. Check API keys: Verify in Settings panel
2. Monitor rate limits: Check model usage dashboard
3. Try different model: Switch to faster model
4. Check internet connection: `ping api.openai.com`

### Debug Mode

Enable debug logging:
```bash
export CELEST_DEBUG=1
export CELEST_LOG_LEVEL=DEBUG
./start_celest_ai.sh
```

View debug logs:
```bash
tail -f ~/.celest/logs/debug.log
```

### Performance Optimization

#### Memory Usage
- Reduce conversation history: Settings → History Limit
- Clear cache: `rm -rf ~/.celest/cache/`
- Monitor with: `htop` or Activity Monitor

#### Response Speed
- Use faster models: GPT-3.5 instead of GPT-4
- Enable caching: Settings → Enable Response Cache
- Reduce context: Settings → Context Window Limit

---

## 📖 API Reference

### Extension API

#### Informer Component
```python
from CelestAI import Informer

informer = Informer(context)

# Get current sheet data
data = informer.get_active_sheet_data()

# Execute SQL query
result = informer.execute_sql_subview("SELECT * FROM data WHERE value > 100")

# Send to backend
response = informer.send_to_backend("/api/analyze")
```

#### Executor Component
```python
from CelestAI import Executor

executor = Executor(context)

# Apply edit plan
plan = {
    "edits": [
        {"type": "cell_value", "row": 0, "col": 0, "value": "New Value"}
    ]
}
result = executor.apply_edit_plan(plan)

# Undo/Redo
executor.undo_last_change()
executor.redo_last_change()
```

### Backend API

#### REST Endpoints

##### GET /api/status
Get backend status and health information.

**Response**:
```json
{
    "status": "ready",
    "version": "2.0.0",
    "models_available": ["gpt-4", "claude-3-sonnet"],
    "uptime": 3600
}
```

##### POST /api/analyze
Analyze spreadsheet data and provide insights.

**Request**:
```json
{
    "session_id": "session_123",
    "data": {
        "sheet_name": "Sales Data",
        "range": {"start_row": 0, "end_row": 100},
        "data": [["Header1", "Header2"], ["Value1", "Value2"]]
    },
    "analysis_type": "trends"
}
```

**Response**:
```json
{
    "insights": [
        {
            "type": "trend",
            "description": "Sales increasing 15% monthly",
            "confidence": 0.89,
            "supporting_data": {...}
        }
    ],
    "recommendations": [
        {
            "action": "Create forecast chart",
            "priority": "high",
            "estimated_impact": "Improve decision making"
        }
    ]
}
```

##### POST /api/execute
Execute an AI-generated plan on spreadsheet data.

**Request**:
```json
{
    "session_id": "session_123", 
    "plan": {
        "edits": [
            {"type": "cell_formula", "row": 1, "col": 3, "formula": "=SUM(A:A)"}
        ],
        "reasoning": "Calculate total of column A"
    }
}
```

**Response**:
```json
{
    "status": "success",
    "results": [
        {"type": "cell_formula", "success": true, "value": 15420}
    ],
    "undo_data": {...}
}
```

### Python SDK

```python
from celest_ai import CelestClient

# Initialize client
client = CelestClient(api_key="your-key")

# Start session
session = client.create_session(document_id="spreadsheet_1")

# Analyze data
analysis = session.analyze(data, analysis_type="comprehensive")

# Execute plan
result = session.execute(plan)

# Get conversation history
history = session.get_conversation_history()
```

---

## 🤝 Contributing

We welcome contributions! Here's how to get started:

### Contributing Guidelines

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Follow code style**: Use Black, Flake8, and MyPy
4. **Add tests**: Ensure 80%+ test coverage
5. **Update documentation**: Include docstrings and README updates
6. **Submit pull request**: Include detailed description

### Development Workflow

```bash
# Setup development environment
git clone https://github.com/your-org/celest-libreoffice.git
cd celest-libreoffice
./setup_development.sh

# Create feature branch
git checkout -b feature/new-ai-model

# Make changes and test
python3 -m pytest
pre-commit run --all-files

# Submit changes
git commit -m "Add support for new AI model"
git push origin feature/new-ai-model
```

### Code Standards

- **Python**: PEP 8, type hints, docstrings
- **JavaScript**: ESLint, JSDoc comments
- **Testing**: Pytest, 80%+ coverage
- **Documentation**: Clear, comprehensive, examples

### Reporting Issues

Use GitHub Issues with:
- **Clear title** describing the problem
- **Steps to reproduce** the issue
- **Expected vs actual behavior**
- **Environment details** (OS, LibreOffice version, Python version)
- **Log files** if applicable

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **LibreOffice Community** - For the amazing office suite platform
- **OpenAI & Anthropic** - For providing powerful AI models
- **FAISS Team** - For the excellent vector search library
- **Contributors** - Everyone who helped build and improve this project

---

## 📞 Support

- **Documentation**: [Full Documentation](https://docs.celest-ai.com)
- **Issues**: [GitHub Issues](https://github.com/your-org/celest-libreoffice/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/celest-libreoffice/discussions)
- **Email**: support@celest-ai.com
- **Discord**: [Celest AI Community](https://discord.gg/celest-ai)

---

*Made with ❤️ by the Celest AI Team*