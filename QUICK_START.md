# ⚡ Celest AI Quick Start Guide

**Get up and running with AI-powered LibreOffice in 15 minutes**

---

## 🎯 What You'll Get

After following this guide, you'll have:
- ✅ LibreOffice with integrated AI capabilities
- ✅ 3-panel AI sidebar (Conversation, Timeline, Settings)
- ✅ Menu integration (Tools → Celest AI)
- ✅ Backend AI services running locally
- ✅ Ready for AI-powered spreadsheet analysis

---

## 🚀 One-Command Installation

```bash
# Clone and install everything
git clone https://github.com/your-org/celest-libreoffice.git
cd celest-libreoffice
chmod +x setup_celest_ai.sh
./setup_celest_ai.sh
```

**⏱️ Installation Time**: 30-60 minutes (LibreOffice build takes most time)

**💾 Disk Space**: ~10GB during build, ~5GB after cleanup

---

## 🔧 System Requirements

### Minimum
- **OS**: macOS 10.15+ or Ubuntu 20.04+
- **RAM**: 8GB (16GB recommended)
- **CPU**: 4 cores (8 cores recommended for faster build)
- **Disk**: 10GB free space
- **Python**: 3.8+

### Check Your System
```bash
# Check Python version
python3 --version  # Should be 3.8+

# Check available RAM
free -h  # Linux
top | head -5  # macOS

# Check disk space
df -h .  # Should show 10GB+ available
```

---

## ⚙️ Configuration (Required)

### 1. Get API Keys

You need at least one AI provider:

#### Option A: OpenAI (Recommended)
1. Visit [OpenAI API](https://platform.openai.com/api-keys)
2. Create account and get API key
3. **Cost**: ~$0.03-0.06 per 1K tokens

#### Option B: Anthropic Claude
1. Visit [Anthropic Console](https://console.anthropic.com/)
2. Create account and get API key
3. **Cost**: ~$0.015-0.075 per 1K tokens

### 2. Configure Keys

After installation, open LibreOffice and:
1. Go to **Tools → Celest AI → Settings**
2. Enter your API key(s)
3. Select default model
4. Set usage limits (optional)

**Alternative**: Create config file manually:
```bash
mkdir -p ~/.celest/config
cat > ~/.celest/config/api_keys.json << EOF
{
  "openai": {
    "api_key": "sk-your-openai-key",
    "default_model": "gpt-4"
  }
}
EOF
```

---

## 🎮 First Use

### 1. Open LibreOffice
```bash
# Start with Celest AI
./start_celest_ai.sh
```

### 2. Create Test Data
1. Open Calc (spreadsheet)
2. Enter some sample data:
   ```
   A1: Month    B1: Sales
   A2: Jan      B2: 1000
   A3: Feb      B3: 1200
   A4: Mar      B4: 900
   A5: Apr      B5: 1500
   ```

### 3. Try AI Analysis
1. Select your data (A1:B5)
2. Press **F12** or go to **Tools → Celest AI → Show Sidebar**
3. In the conversation panel, type:
   ```
   "Analyze this sales data and suggest improvements"
   ```
4. Watch AI thinking in real-time! 🤖

### 4. Apply AI Suggestions
- AI will suggest formulas, charts, or formatting
- Review changes in the **Timeline panel**
- Click **Apply** to execute
- Use **Undo** if needed

---

## 📊 Quick Examples

### Example 1: Data Analysis
```
You: "What trends do you see in this data?"

AI: I can see your sales data shows some interesting patterns:
    - February had highest sales (1200)
    - March dipped to 900 (-25% from Feb)  
    - April recovered strongly (1500, +67% from Mar)
    
    Suggestions:
    - Add trend analysis with =TREND() function
    - Create visualization to spot patterns
    - Calculate moving averages
```

### Example 2: Formula Creation
```
You: "Create a formula to calculate quarterly totals"

AI: I'll create a quarterly summary for you:
    - Adding SUM formulas for Q1 total
    - Creating percentage change calculations
    - Formatting results for readability
    
    Formula: =SUM(B2:B4) for Q1 total
    Applied to cell B6
```

### Example 3: Visualization
```
You: "Make a chart showing the sales trend"

AI: Creating sales trend visualization:
    - Chart type: Line chart
    - Data range: A1:B5
    - Added trendline
    - Positioned to avoid data overlap
    
    The chart shows volatility but overall growth trend.
```

---

## 🎛️ Interface Overview

### 3-Panel Sidebar

#### 💬 Conversation Panel (Left)
- **Chat with AI** in natural language
- **See thinking tokens** in real-time
- **Context-aware** of your current selection
- **History** of previous conversations

#### 📊 Timeline Panel (Center)  
- **Visual change history** with timestamps
- **Granular undo/redo** for specific changes
- **Change preview** before applying
- **Batch operations** grouping

#### ⚙️ Settings Panel (Right)
- **Model selection** (GPT-4, Claude, etc.)
- **API key management**
- **Usage tracking** and cost monitoring
- **Preferences** and safety settings

### Menu Integration
- **Tools → Celest AI** - Main menu
- **Toolbar buttons** - Quick access
- **Keyboard shortcuts** - F12 for sidebar

---

## 🔍 Troubleshooting

### Installation Issues

#### ❌ "Make failed with errors"
```bash
# Check system dependencies
sudo apt update && sudo apt upgrade  # Linux
brew update && brew upgrade          # macOS

# Try with fewer cores
make -j2  # Instead of make -j8

# Check disk space
df -h .   # Need 10GB+
```

#### ❌ "Python module not found"
```bash
# Reinstall Python dependencies
cd private_backend/backend
source venv/bin/activate
pip install --upgrade -r requirements.txt
```

#### ❌ "Extension not loading"
```bash
# Check LibreOffice extension
./instdir/LibreOfficeDev.app/Contents/MacOS/unopkg list  # macOS
./instdir/program/unopkg list                            # Linux

# Reinstall extension
./instdir/LibreOfficeDev.app/Contents/MacOS/unopkg add celestextension/celest-ai-2.0.oxt --force
```

### Runtime Issues

#### ❌ "Backend not ready"
```bash
# Check backend status
curl http://localhost:5001/api/status

# Restart services
./stop_celest_ai.sh
./start_celest_ai.sh
```

#### ❌ "AI not responding"  
1. **Check API keys** in Settings panel
2. **Verify internet** connection
3. **Check rate limits** on your AI provider
4. **Try different model** (GPT-3.5 vs GPT-4)

#### ❌ "Sidebar not showing"
1. Press **F12** to toggle
2. Go to **View → Sidebar** → Reset Sidebar
3. Check **Tools → Celest AI → Show Sidebar**

---

## 📈 Next Steps

### 1. Explore Advanced Features
- **Batch operations** on large datasets
- **Custom prompts** for specific analysis
- **Model switching** for different tasks
- **Cost optimization** settings

### 2. Integration Examples
- **Data import** from CSV/Excel files
- **API connections** to external data
- **Automated reporting** workflows
- **Custom dashboard** creation

### 3. Share & Collaborate
- **Export AI insights** as comments
- **Save conversation** history
- **Share enhanced spreadsheets** with team
- **Document AI decisions** for audit trail

---

## 🎯 Success Checklist

After setup, verify everything works:

- [ ] LibreOffice opens with Celest AI menu
- [ ] F12 shows 3-panel sidebar
- [ ] API keys configured in settings
- [ ] AI responds to basic questions
- [ ] Timeline shows change history
- [ ] Undo/redo works correctly
- [ ] Backend services running (check `ps aux | grep celest`)

---

## 🆘 Getting Help

### Documentation
- **Full Guide**: [README_CELEST_AI.md](README_CELEST_AI.md)
- **Extension Details**: [celestextension/README_EXTENSION.md](celestextension/README_EXTENSION.md)
- **Backend Guide**: [private_backend/README_BACKEND.md](private_backend/README_BACKEND.md)

### Support Channels
- **GitHub Issues**: [Report bugs](https://github.com/your-org/celest-libreoffice/issues)
- **Discussions**: [Ask questions](https://github.com/your-org/celest-libreoffice/discussions)
- **Email**: support@celest-ai.com
- **Discord**: [Community chat](https://discord.gg/celest-ai)

### Debug Information
```bash
# Check installation logs
tail -f celest_ai_install.log

# View runtime logs
tail -f ~/.celest/logs/backend.log

# System status
./start_celest_ai.sh --status
```

---

## 🚀 You're Ready!

Congratulations! You now have a powerful AI-enhanced LibreOffice setup. 

**Start exploring**:
1. Create some data in Calc
2. Ask AI to analyze it
3. Watch the magic happen! ✨

*Happy analyzing with Celest AI!* 🎉

---

*Need more help? Check the [Complete Documentation](README_CELEST_AI.md) or [Join our Community](https://discord.gg/celest-ai)*