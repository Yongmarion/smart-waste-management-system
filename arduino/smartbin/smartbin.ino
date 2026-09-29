#include <ESP8266HTTPClient.h>
#include <Servo.h>


// Pin Definitions 
#define TRIG_PIN    D6
#define ECHO_PIN    D7
#define SERVO_PIN   D4
#define LED_PIN     D0   // Built-in LED for status (optional)

//  USER CONFIG — CHANGE THESE 
const char* WIFI_SSID     = "Galaxy S23 Ultra 5443";
const char* WIFI_PASSWORD = "Ndefir7263";
const char* SERVER_URL    = "http://10.66.138.175:3000/api/data";
// 

//  Tuning Parameters 
const float  TRIGGER_DISTANCE_CM = 15.0;   // Hand detection threshold
const int    LID_OPEN_ANGLE      = 180;     // Degrees to open lid
const int    LID_CLOSED_ANGLE    = 0;      // Degrees for closed lid
const int    LID_OPEN_DELAY_MS   = 2000;   // How long lid stays open (ms)
const int    LOOP_DELAY_MS       = 200;    // Main loop delay (ms)
const int    WIFI_RETRY_DELAY    = 500;    // WiFi retry interval (ms)
const int    MAX_WIFI_RETRIES    = 40;     // ~20 seconds before giving up
const int    HEARTBEAT_INTERVAL  = 5000;   // Send heartbeat every 5s (ms)

//  State Variables 
Servo   lidServo;
int     usageCount   = 0;
bool    lidOpen      = false;
unsigned long lastHeartbeat = 0;

//  WiFi Connect 
void connectWiFi() {
  Serial.println("\n[WiFi] Connecting to: " + String(WIFI_SSID));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < MAX_WIFI_RETRIES) {
    delay(WIFI_RETRY_DELAY);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Connected!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_PIN, LOW); // LED ON = connected (active low)
  } else {
    Serial.println("\n[WiFi] ✗ Failed to connect. Running offline.");
    digitalWrite(LED_PIN, HIGH);
  }
}

//  Setup 
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n");
  Serial.println("  SmartBin System Starting  ");
  Serial.println("----------");

  // Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN,  OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED OFF initially

  // Servo: start closed
  lidServo.attach(SERVO_PIN);
  lidServo.write(LID_CLOSED_ANGLE);
  Serial.println("[Servo] Initialized → CLOSED");

  // WiFi
  connectWiFi();

  Serial.println("[System] Ready. Waiting for hand detection...\n");
}

// Get Ultrasonic Distance 
float getDistance() {
  // Clear trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send 10us pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo (30ms timeout = ~5 meter max range)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) return -1.0; // No echo / timeout

  float distance = (duration * 0.034) / 2.0;
  return distance;
}

// Send Data to Backend 
bool sendData(float distance, const char* lidStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi disconnected — attempting reconnect...");
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return false;
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000); // 5 second timeout

  // Build JSON payload
  String payload = "{";
  payload += "\"distance\":"    + String(distance, 1) + ",";
  payload += "\"lidStatus\":\""  + String(lidStatus) + "\",";
  payload += "\"usageCount\":"  + String(usageCount) + ",";
  payload += "\"timestamp\":\""  + String(millis()) + "\"";
  payload += "}";

  Serial.print("[HTTP] POST → ");
  Serial.print(SERVER_URL);
  Serial.print(" | ");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.print("[HTTP] Response: ");
    Serial.println(httpCode);
    http.end();
    return (httpCode == 200);
  } else {
    Serial.print("[HTTP] Error: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return false;
  }
}

// Open Lid Sequence 
void openLid(float triggerDistance) {
  lidOpen = true;
  usageCount++;

  Serial.println("──────────────────────────────");
  Serial.print("[Lid] OPENING  | distance=");
  Serial.print(triggerDistance);
  Serial.print("cm | usageCount=");
  Serial.println(usageCount);

  // Physically open
  lidServo.write(LID_OPEN_ANGLE);

  // Notify backend: OPEN
  sendData(triggerDistance, "OPEN");

  // Stay open
  delay(LID_OPEN_DELAY_MS);

  // Close
  lidServo.write(LID_CLOSED_ANGLE);
  Serial.println("[Lid] CLOSING");

  // Notify backend: CLOSED
  sendData(triggerDistance, "CLOSED");
  Serial.println("──────────────────────────────");
}

//  Heartbeat (keep-alive ping every 5s) 
void maybeHeartbeat(float distance) {
  unsigned long now = millis();
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    // Send current state so dashboard stays "ONLINE"
    sendData(distance, lidOpen ? "OPEN" : "CLOSED");
    Serial.println("[Heartbeat] Sent");
  }
}

//  Main Loop 
void loop() {
  float distance = getDistance();

  // Print distance reading
  if (distance > 0) {
    Serial.print("[Sensor] Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  } else {
    Serial.println("[Sensor] No echo received");
  }

  // Hand Detected: open lid 
  if (distance > 0 && distance < TRIGGER_DISTANCE_CM && !lidOpen) {
    openLid(distance);
  }

  //  Hand Gone: reset flag so next detection is possible 
  if (distance > TRIGGER_DISTANCE_CM || distance < 0) {
    if (lidOpen) {
      Serial.println("[Sensor] Hand removed — ready for next activation");
    }
    lidOpen = false;
  }

  //  Heartbeat to keep dashboard showing ONLINE 
  maybeHeartbeat(distance > 0 ? distance : 99.9);

  delay(LOOP_DELAY_MS);
}
