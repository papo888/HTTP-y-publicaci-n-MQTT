/*
 * ESP32 Car Controller - HTTP endpoint + MQTT publication
 * Single control endpoint + healthcheck
 * 
 * Hardware assumed:
 *   - Motor driver L298N or similar (2 DC motors with direction + PWM speed)
 *   - ESP32 (DevKit v1)
 * 
 * Networking:
 *   - HTTP server on port 80
 *   - POST/GET /api/move?direction=forward|backward|left|right|stop&speed=0-100&duration_ms=0-5000
 *   - GET /health
 * 
 * MQTT:
 *   - Publishes every accepted control command to topic baseTopic (see config)
 *   - JSON payload includes client_ip, direction, speed, duration_ms, ts
 */

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

/************ USER CONFIG ************/
const char* WIFI_SSID     = ".";
const char* WIFI_PASSWORD = "nicolas01";

const char* MQTT_HOST     = "test.mosquitto.org";   // Example public broker
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_USER     = nullptr;               // set if your broker needs it
const char* MQTT_PASS     = nullptr;               // set if your broker needs it
const char* MQTT_CLIENT_ID= "esp32-car-" __DATE__ "-" __TIME__;
const char* MQTT_TOPIC    = "esp32car/commands";
/****************************************/

// Motor pins (change to your wiring)
const int ENA = 14;  // PWM left
const int IN1 = 27;
const int IN2 = 26;

const int ENB = 25;  // PWM right
const int IN3 = 33;
const int IN4 = 32;

// PWM channels
const int CH_A = 0;
const int CH_B = 1;
const int PWM_FREQ = 20000;   // 20kHz for quiet motors
const int PWM_RES  = 8;       // 0..255

// HTTP server
WebServer server(80);

// WiFi + MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Motion state
struct Motion {
  bool active = false;
  String direction = "stop";
  uint8_t speedPct = 0;           // 0-100
  uint32_t startedAt = 0;
  uint32_t durationMs = 0;        // 0-5000
} motion;

// Utils
void motorsStop();
void applyMotion(const String& direction, uint8_t speedPct);
void startMotion(const String& direction, uint8_t speedPct, uint32_t durationMs);
void publishMQTT(const String& clientIP, const String& direction, uint8_t speedPct, uint32_t durationMs, const String& status);

// ---------- Networking helpers ----------
void ensureMqtt() {
  if (mqttClient.connected()) return;
  String cid = String(MQTT_CLIENT_ID) + String(random(0xffff), HEX);
  if (MQTT_USER && MQTT_PASS) {
    mqttClient.connect(cid.c_str(), MQTT_USER, MQTT_PASS);
  } else {
    mqttClient.connect(cid.c_str());
  }
}

String jsonEscape(const String& s) {
  String out;
  for (size_t i=0;i<s.length();++i) {
    char c = s[i];
    if (c=='"') out += "\\\"";
    else if (c=='\\') out += "\\\\";
    else if (c=='\n') out += "\\n";
    else out += c;
  }
  return out;
}

// ---------- HTTP Handlers ----------
void handleHealth() {
  String body = "{";
  body += "\"status\":\"ok\",";
  body += "\"uptime_ms\":" + String(millis()) + ",";
  body += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\",";
  body += "\"mqtt_connected\":" + String(mqttClient.connected() ? "true" : "false") + ",";
  body += "\"motion_active\":" + String(motion.active ? "true" : "false");
  body += "}";
  server.send(200, "application/json", body);
}

bool parseUint(const String& s, uint32_t& out) {
  if (s.length()==0) return false;
  for (size_t i=0;i<s.length();++i) if (!isDigit(s[i])) return false;
  out = s.toInt();
  return true;
}

void handleMove() {
  // Accept either GET query params or x-www-form-urlencoded
  String dir = server.arg("direction");
  String sp  = server.arg("speed");
  String dur = server.arg("duration_ms");

  // Basic validation
  dir.toLowerCase();
  if (dir != "forward" && dir != "backward" && dir != "left" && dir != "right" && dir != "stop") {
    server.send(400, "application/json", "{\"error\":\"direction must be one of forward|backward|left|right|stop\"}");
    return;
  }

  uint32_t speedPct32 = 0;
  if (!parseUint(sp, speedPct32) || speedPct32>100) {
    server.send(400, "application/json", "{\"error\":\"speed must be integer 0..100\"}");
    return;
  }
  uint8_t speedPct = (uint8_t)speedPct32;

  uint32_t duration = 0;
  if (!parseUint(dur, duration) || duration>5000) {
    server.send(400, "application/json", "{\"error\":\"duration_ms must be integer 0..5000\"}");
    return;
  }

  if (motion.active) {
    // To keep safety, we reject concurrent commands
    server.send(409, "application/json", "{\"error\":\"motion already active; try again later\"}");
    return;
  }

  String clientIP = server.client().remoteIP().toString();
  startMotion(dir, speedPct, duration);
  publishMQTT(clientIP, dir, speedPct, duration, "accepted");

  String body = "{";
  body += "\"status\":\"accepted\",";
  body += "\"direction\":\"" + dir + "\",";
  body += "\"speed\":" + String(speedPct) + ",";
  body += "\"duration_ms\":" + String(duration) + ",";
  body += "\"client_ip\":\"" + clientIP + "\"";
  body += "}";

  server.send(202, "application/json", body);
}

// ---------- Motor control ----------
uint8_t pctToPwm(uint8_t pct) {
  if (pct>100) pct = 100;
  return (uint8_t)((pct * 255) / 100);
}

void motorsStop() {
  ledcWrite(CH_A, 0);
  ledcWrite(CH_B, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void applyMotion(const String& direction, uint8_t speedPct) {
  uint8_t pwm = pctToPwm(speedPct);
  if (direction == "forward") {
    // Left forward
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    // Right forward
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_A, pwm);
    ledcWrite(CH_B, pwm);
  } else if (direction == "backward") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    ledcWrite(CH_A, pwm);
    ledcWrite(CH_B, pwm);
  } else if (direction == "left") {
    // Left motor backward, right motor forward for pivot turn
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_A, pwm);
    ledcWrite(CH_B, pwm);
  } else if (direction == "right") {
    // Left motor forward, right motor backward
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    ledcWrite(CH_A, pwm);
    ledcWrite(CH_B, pwm);
  } else { // stop
    motorsStop();
  }
}

void startMotion(const String& direction, uint8_t speedPct, uint32_t durationMs) {
  motion.active = true;
  motion.direction = direction;
  motion.speedPct = speedPct;
  motion.durationMs = durationMs;
  motion.startedAt = millis();
  applyMotion(direction, speedPct);
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  ledcSetup(CH_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, CH_A);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENB, CH_B);

  motorsStop();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  // MQTT
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  ensureMqtt();
  if (mqttClient.connected()) {
    Serial.println("MQTT connected");
  } else {
    Serial.println("MQTT not connected (will retry in loop)");
  }

  // HTTP routes
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/api/move", HTTP_ANY, handleMove);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Non-blocking motion timeout
  if (motion.active) {
    uint32_t now = millis();
    if (now - motion.startedAt >= motion.durationMs) {
      motorsStop();
      motion.active = false;
    }
  }

  // Keep network stacks alive
  server.handleClient();

  // Keep MQTT connected
  if (!mqttClient.connected()) {
    static uint32_t last = 0;
    if (millis() - last > 2000) {
      last = millis();
      ensureMqtt();
    }
  } else {
    mqttClient.loop();
  }
}

void publishMQTT(const String& clientIP, const String& direction, uint8_t speedPct, uint32_t durationMs, const String& status) {
  if (!mqttClient.connected()) return;
  String payload = "{";
  payload += "\"status\":\"" + status + "\",";
  payload += "\"direction\":\"" + jsonEscape(direction) + "\",";
  payload += "\"speed\":" + String(speedPct) + ",";
  payload += "\"duration_ms\":" + String(durationMs) + ",";
  payload += "\"client_ip\":\"" + jsonEscape(clientIP) + "\",";
  payload += "\"ts\":" + String((uint32_t) (millis())) + "";
  payload += "}";

  mqttClient.publish(MQTT_TOPIC, payload.c_str());
}
