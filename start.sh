#!/bin/bash

echo ""
echo "╔══════════════════════════════════════════════╗"
echo "║     SmartBin IoT Dashboard — Starting...     ║"
echo "╚══════════════════════════════════════════════╝"
echo ""

# Check Node.js
if ! command -v node &> /dev/null; then
    echo "[ERROR] Node.js not found!"
    echo "Install it from: https://nodejs.org"
    exit 1
fi
echo "[OK] Node.js $(node --version) found"

# Install dependencies if needed
if [ ! -d "backend/node_modules" ]; then
    echo "[Setup] Installing dependencies..."
    cd backend && npm install && cd ..
    echo "[OK] Dependencies installed"
fi

# Show IP
IP=$(hostname -I 2>/dev/null | awk '{print $1}')
if [ -z "$IP" ]; then
    IP=$(ipconfig getifaddr en0 2>/dev/null || echo "unknown")
fi

echo ""
echo "─────────────────────────────────────────────"
echo "📡 Your PC IP Address: $IP"
echo "👉 Use in Arduino:  http://$IP:3000/api/data"
echo "─────────────────────────────────────────────"
echo ""
echo "🌐 Dashboard: http://localhost:3000"
echo ""
echo "Press Ctrl+C to stop"
echo ""

cd backend
node server.js
