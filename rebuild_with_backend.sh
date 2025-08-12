#!/bin/bash
echo "Rebuilding LibreOffice Core..."
echo "This will take 30-45 minutes..."
make
echo ""
echo "Building private backend..."
make private_backend
echo ""
echo "Rebuild complete!"
