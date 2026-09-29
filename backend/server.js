/**
 * Smart Dustbin Monitoring System - Node.js Backend
 * Receives data from NodeMCU ESP8266, serves API to frontend dashboard
 */

const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = 3000;

// ─── Middleware ────────────────────────────────────────────────────────────────
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, '../frontend')));

// ─── In-Memory Data Store ──────────────────────────────────────────────────────
let systemState = {
  distance: 0,
  lidStatus: 'CLOSED',
  usageCount: 0,
  lastUpdated: new Date().toISOString(),
  isConnected: false,
};

// Circular buffer: last 500 events
const MAX_LOGS = 500;
let eventLog = [];

// Hourly usage buckets for analytics [0..23]
let hourlyUsage = new Array(24).fill(0);

// Track last known usageCount to detect increments from hardware
let lastKnownUsageCount = 0;

// Connection watchdog — mark offline if no data in 10s
let connectionTimer = null;

function resetConnectionWatchdog() {
  if (connectionTimer) clearTimeout(connectionTimer);
  systemState.isConnected = true;
  connectionTimer = setTimeout(() => {
    systemState.isConnected = false;
    console.log('[Watchdog] Hardware connection lost');
  }, 10000);
}

// ─── Helper: push event to log ─────────────────────────────────────────────────
function pushEvent(type, distance, lidStatus, usageCount, ts) {
  const entry = {
    id: Date.now() + Math.random(),
    type,           // 'OPEN' | 'CLOSED' | 'HEARTBEAT'
    distance: parseFloat(distance).toFixed(1),
    lidStatus,
    usageCount,
    timestamp: ts || new Date().toISOString(),
    hour: new Date().getHours(),
  };
  eventLog.unshift(entry);           // newest first
  if (eventLog.length > MAX_LOGS) eventLog.pop();
  return entry;
}

// ─── Analytics helpers ─────────────────────────────────────────────────────────
function computeAnalytics() {
  const now = new Date();
  const currentHour = now.getHours();

  // Peak usage hour
  let peakHour = 0;
  let peakCount = 0;
  hourlyUsage.forEach((count, h) => {
    if (count > peakCount) { peakCount = count; peakHour = h; }
  });

  // Last-hour activity
  const lastHourEvents = eventLog.filter(e => {
    const t = new Date(e.timestamp);
    return (now - t) <= 3600000 && e.type === 'OPEN';
  });

  // Last-5-minute activity
  const last5MinEvents = eventLog.filter(e => {
    const t = new Date(e.timestamp);
    return (now - t) <= 300000 && e.type === 'OPEN';
  });

  // Inactivity detection
  const lastOpenEvent = eventLog.find(e => e.type === 'OPEN');
  let inactiveMins = null;
  if (lastOpenEvent) {
    inactiveMins = Math.floor((now - new Date(lastOpenEvent.timestamp)) / 60000);
  }

  // Smart alerts
  const alerts = [];
  if (last5MinEvents.length >= 5) {
    alerts.push({ level: 'danger', message: 'High Activity Detected — many uses in last 5 minutes' });
  }
  if (systemState.usageCount >= 50) {
    alerts.push({ level: 'warning', message: 'Dustbin used 50+ times today — consider emptying' });
  }
  if (inactiveMins !== null && inactiveMins >= 60) {
    alerts.push({ level: 'info', message: `Low Activity Period — no usage in ${inactiveMins} minutes` });
  }
  if (lastHourEvents.length > 10) {
    alerts.push({ level: 'warning', message: 'Usage Trend Increasing — heavy usage this hour' });
  }
  if (!systemState.isConnected) {
    alerts.push({ level: 'danger', message: 'Hardware Offline — no data received in 10 seconds' });
  }

  return {
    totalUsageToday: systemState.usageCount,
    peakHour,
    peakCount,
    lastHourUsage: lastHourEvents.length,
    last5MinUsage: last5MinEvents.length,
    inactiveMins,
    hourlyBreakdown: hourlyUsage,
    alerts,
  };
}

// ─── Routes ────────────────────────────────────────────────────────────────────

/**
 * POST /api/data
 * Payload from NodeMCU:
 * { distance, lidStatus, usageCount, timestamp }
 */
app.post('/api/data', (req, res) => {
  const { distance, lidStatus, usageCount, timestamp } = req.body;

  if (distance === undefined || !lidStatus) {
    return res.status(400).json({ error: 'Missing required fields: distance, lidStatus' });
  }

  // Detect a new lid-open (usageCount incremented by hardware)
  const isNewOpen = (lidStatus === 'OPEN') && (parseInt(usageCount) > lastKnownUsageCount);
  if (isNewOpen) {
    const h = new Date().getHours();
    hourlyUsage[h] = (hourlyUsage[h] || 0) + 1;
    lastKnownUsageCount = parseInt(usageCount);
  }

  // Update system state
  systemState = {
    distance: parseFloat(distance).toFixed(1),
    lidStatus,
    usageCount: parseInt(usageCount),
    lastUpdated: new Date().toISOString(),
    isConnected: true,
  };

  // Push to event log
  const eventType = lidStatus === 'OPEN' ? 'OPEN' : 'CLOSED';
  pushEvent(eventType, distance, lidStatus, usageCount);

  resetConnectionWatchdog();

  console.log(`[${new Date().toLocaleTimeString()}] ${lidStatus} | dist=${distance}cm | uses=${usageCount}`);

  res.json({ status: 'ok', received: systemState });
});

/**
 * GET /api/state
 * Returns latest system state
 */
app.get('/api/state', (req, res) => {
  res.json(systemState);
});

/**
 * GET /api/logs?limit=50
 * Returns event log
 */
app.get('/api/logs', (req, res) => {
  const limit = parseInt(req.query.limit) || 50;
  res.json(eventLog.slice(0, limit));
});

/**
 * GET /api/analytics
 * Returns computed analytics and alerts
 */
app.get('/api/analytics', (req, res) => {
  res.json(computeAnalytics());
});

/**
 * POST /api/reset
 * Reset daily usage counters (for testing or daily reset)
 */
app.post('/api/reset', (req, res) => {
  systemState.usageCount = 0;
  lastKnownUsageCount = 0;
  hourlyUsage = new Array(24).fill(0);
  eventLog = [];
  console.log('[Reset] Daily counters cleared');
  res.json({ status: 'reset', message: 'Daily counters and logs cleared' });
});

/**
 * POST /api/simulate
 * Simulates a hardware event for testing without hardware
 */
app.post('/api/simulate', (req, res) => {
  const distance = (Math.random() * 10 + 2).toFixed(1);
  const fakeUsage = systemState.usageCount + 1;

  // Simulate OPEN
  const h = new Date().getHours();
  hourlyUsage[h] = (hourlyUsage[h] || 0) + 1;
  lastKnownUsageCount = fakeUsage;

  systemState = {
    distance,
    lidStatus: 'OPEN',
    usageCount: fakeUsage,
    lastUpdated: new Date().toISOString(),
    isConnected: true,
  };
  pushEvent('OPEN', distance, 'OPEN', fakeUsage);
  resetConnectionWatchdog();

  // Auto-close after 2 seconds
  setTimeout(() => {
    systemState.lidStatus = 'CLOSED';
    systemState.lastUpdated = new Date().toISOString();
    pushEvent('CLOSED', distance, 'CLOSED', fakeUsage);
  }, 2000);

  res.json({ status: 'simulated', distance, usageCount: fakeUsage });
});

// ─── Fallback: serve frontend ──────────────────────────────────────────────────
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, '../frontend/index.html'));
});

// ─── Start ─────────────────────────────────────────────────────────────────────
app.listen(PORT, '0.0.0.0', () => {
  console.log('');
  console.log('╔══════════════════════════════════════════════╗');
  console.log('║     Smart Dustbin Monitoring System          ║');
  console.log('║     Backend running on port ' + PORT + '            ║');
  console.log('╠══════════════════════════════════════════════╣');
  console.log('║  Dashboard  →  http://localhost:' + PORT + '         ║');
  console.log('║  Data API   →  POST /api/data                ║');
  console.log('║  Analytics  →  GET  /api/analytics           ║');
  console.log('║  Simulate   →  POST /api/simulate            ║');
  console.log('╚══════════════════════════════════════════════╝');
  console.log('');
  console.log('Waiting for NodeMCU connection...');
});
