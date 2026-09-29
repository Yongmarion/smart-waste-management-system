# 🗑️ SmartBin — IoT Dashboard System

A full-stack real-time monitoring dashboard for your ESP8266 Smart Dustbin.

---

## 📁 Project Structure

```
smartbin/
├── backend/
│   ├── server.js          ← Node.js API server
│   └── package.json
├── frontend/
│   └── index.html         ← Dashboard (served by backend)
├── arduino/
│   └── smartbin.ino       ← ESP8266 firmware (ready to flash)
├── start.bat              ← Windows one-click start
├── start.sh               ← Linux/Mac one-click start
└── README.md
```

---

## 🚀 Quick Start (3 Steps)

### Step 1 — Install Node.js
Download from https://nodejs.org (LTS version)

### Step 2 — Start the Backend
**Windows:**  Double-click `start.bat`

**Linux / Mac:**
```bash
chmod +x start.sh && ./start.sh
```

**Manual:**
```bash
cd backend
npm install
node server.js
```

### Step 3 — Open the Dashboard
Visit: **http://localhost:3000**

---

## 🔧 Configure the Arduino (ESP8266)

Open `arduino/smartbin.ino` and update these 3 lines:

```cpp
const char* ssid      = "YOUR_WIFI_NAME";
const char* password  = "YOUR_WIFI_PASSWORD";
const char* serverUrl = "http://YOUR_PC_IP:3000/api/data";
```

### Find your PC IP:
- **Windows:** CMD → `ipconfig` → look for `IPv4 Address`
- **Linux/Mac:** Terminal → `hostname -I`

> ⚠️ Your PC and NodeMCU must be on the **same WiFi network**.

### Flash via Arduino IDE:
1. Install board: **ESP8266 by ESP8266 Community** (Board Manager)
2. Select: **NodeMCU 1.0 (ESP-12E Module)**
3. Libraries needed: `ESP8266WiFi`, `ESP8266HTTPClient`, `Servo`
4. Upload

---

## 📡 API Endpoints

| Method | Endpoint           | Description                     |
|--------|--------------------|---------------------------------|
| POST   | /api/data          | Receive data from NodeMCU       |
| GET    | /api/state         | Latest system state             |
| GET    | /api/logs?limit=50 | Event history                   |
| GET    | /api/analytics     | Analytics + smart alerts        |
| POST   | /api/simulate      | Simulate lid event (no hardware)|
| POST   | /api/reset         | Reset daily counters            |

### NodeMCU POST payload:
```json
{ "distance": 8.5, "lidStatus": "OPEN", "usageCount": 12, "timestamp": "84729" }
```

---

## 🔔 Smart Alert Rules

| Condition                    | Alert Level |
|------------------------------|-------------|
| ≥ 5 uses in last 5 minutes   | 🔴 DANGER   |
| ≥ 50 total uses today        | 🟡 WARNING  |
| No use for ≥ 60 minutes      | 🔵 INFO     |
| > 10 uses in last hour       | 🟡 WARNING  |
| No hardware data for 10s     | 🔴 DANGER   |

---

## 🛠️ Troubleshooting

| Problem | Fix |
|---------|-----|
| Dashboard shows OFFLINE | Check NodeMCU WiFi & IP in Arduino code |
| npm install fails | Install Node.js from nodejs.org |
| No Serial output | Check baud rate is 115200 |
| Port 3000 in use | Change PORT in server.js to 3001 |
