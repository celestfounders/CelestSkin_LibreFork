# Private Backend AI Server

This is the production-ready LibreOffice private backend AI server that provides:

## Features

✅ **Auto-start Integration**: Automatically starts when LibreOffice launches
✅ **PyUNO Bridge**: Direct communication with LibreOffice Calc via UNO socket
✅ **Production Build System**: Fully integrated with LibreOffice build system
✅ **Cross-Platform**: Works on macOS, Linux, and Windows
✅ **Headless Operation**: Can run without LibreOffice UI

## Core Components

- `Module_private_backend.mk`: Build system integration
- `Package_private_backend_runtime.mk`: Runtime file packaging
- `autostart_lo_python.py`: Auto-start lifecycle manager
- `source/python_service/ai_server_worker.py`: Core AI server with PyUNO integration

## Build Instructions

```bash
make private_backend.clean && make private_backend
```

## Architecture

The backend uses the singleton Desktop access pattern to prevent URP bridge disposal:

```python
desktop = remote_ctx.getValueByName("/singletons/com.sun.star.frame.theDesktop")
```

Environment setup ensures proper PyUNO loading with LibreOffice's bundled Python.

## Status

✅ **FULLY FUNCTIONAL** - All core features working as of 2025-08-11
