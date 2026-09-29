@echo off
title SmartBin — IoT Dashboard
color 0B

echo.
echo  ╔══════════════════════════════════════════════╗
echo  ║     SmartBin IoT Dashboard — Starting...     ║
echo  ╚══════════════════════════════════════════════╝
echo.

:: Check Node.js
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo  [ERROR] Node.js not found!
    echo  Please download and install from: https://nodejs.org
    echo.
    pause
    exit /b 1
)

echo  [OK] Node.js found
echo.

:: Install dependencies if needed
if not exist "backend\node_modules" (
    echo  [Setup] Installing dependencies...
    cd backend
    npm install
    cd ..
    echo  [OK] Dependencies installed
    echo.
)

:: Find local IP for display
echo  ─────────────────────────────────────────────
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /i "IPv4"') do (
    set IP=%%a
    goto :found_ip
)
:found_ip
set IP=%IP: =%
echo  📡 Your PC IP Address: %IP%
echo  👉 Use this in Arduino: http://%IP%:3000/api/data
echo  ─────────────────────────────────────────────
echo.
echo  🌐 Dashboard will open at: http://localhost:3000
echo.
echo  Press Ctrl+C to stop the server
echo.

:: Start server
cd backend
node server.js
