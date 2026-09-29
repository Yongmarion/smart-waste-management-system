/**
 * Smart Dustbin — NodeMCU ESP8266 Firmware
 * 
 * SETUP:
 *   1. Fill in YOUR_WIFI_NAME, YOUR_WIFI_PASSWORD
 *   2. Replace YOUR_PC_IP with your computer's local IP (e.g. 192.168.1.105)
 *   3. Upload to NodeMCU via Arduino IDE
 *   4. Start the Node.js backend first: node server.js
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>

//  Pin definitions 
#define TRIG       D6
#define ECHO       D7
#define SERVO_PIN  D4

//  WiFi credentials 
const char* ssid     = "Galaxy S23 Ultra 5443";
const char* password = "Ndefir7263";

//  Backend URL  (change YOUR_PC_IP to your computer's local IP) 
const char* serverUrl = "http://10.73.98.175:3000/api/data";

//  Globals 
Servo lidServo;
int   usageCount = 0;
bool  lidOpen    = false;

// Setup 
void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  lidServo.attach(SERVO_PIN);
  lidServo.write(0);   // lid closed

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("NodeMCU IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Sending data to: ");
  Serial.println(serverUrl);
}

// Ultrasonic distance (cm) 
float getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  if (duration == 0) return -1;
  return duration * 0.034f / 2.0f;
}

//  Send JSON payload to backend 
void sendData(float distance, String lidStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected — skipping send");
    return;
  }

  HTTPClient http;
  WiFiClient client;
  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);  // 5s timeout

  // Build JSON
  String json = "{";
  json += "\"distance\":"    + String(distance, 2) + ",";
  json += "\"lidStatus\":\""  + lidStatus + "\",";
  json += "\"usageCount\":"  + String(usageCount) + ",";
  json += "\"timestamp\":\""  + String(millis()) + "\"";
  json += "}";

  int httpCode = http.POST(json);

  if (httpCode > 0) {
    Serial.printf("[HTTP] POST %s → %d\n", lidStatus.c_str(), httpCode);
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// Main loop
void loop() {
  float distance = getDistance();

  Serial.printf("Distance: %.1f cm\n", distance);

  //  Hand detected (< 15 cm) → open lid 
  if (distance > 0 && distance < 15 && !lidOpen) {
    lidOpen = true;
    usageCount++;

    Serial.printf(">> LID OPEN  (use #%d)\n", usageCount);
    lidServo.write(90);         // open position
    sendData(distance, "OPEN");

    delay(2000);                // hold open for 2 seconds

    lidServo.write(0);          // close
    Serial.println(">> LID CLOSED");
    sendData(distance, "CLOSED");
  }

  //  Hand gone (> 15 cm) → allow next activation 
  if (distance > 15) {
    lidOpen = false;
  }

  delay(200);   // 5 Hz polling
}
