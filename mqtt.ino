/*
 * ESP32 Car Controller – HTTP endpoint + MQTT + Telemetría ultrasónica
 * Usa config.h, sensor.h y sensor.cpp
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "settings.h"
#include "sensor.h"

// =======================
// Variables globales
// =======================
WebServer server(80);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

struct Motion {
  bool active = false;
  String direction = "stop";
  uint8_t speedPct = 0;
  uint32_t startedAt = 0;
  uint32_t durationMs = 0;
} motion;

unsigned long lastTelemetry = 0;
#include "root_ca.h"

// =======================
// Declaración de funciones
// =======================
void motorsStop();
void applyMotion(const String& direction, uint8_t speedPct);
void startMotion(const String& direction, uint8_t speedPct, uint32_t durationMs);
void publishMoveMQTT(const String& clientIP, const String& direction, uint8_t speedPct, uint32_t durationMs, const String& status);
void handleHealth();
void handleMove();
bool parseUint(const String& s, uint32_t& out);
String jsonEscape(const String& s);

// =======================
// Helpers MQTT / JSON
// =======================
void ensureMqtt() {
  if (mqttClient.connected()) return;
  String cid = String(MQTT_CLIENTID_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
  mqttClient.connect(cid.c_str(), MQTT_USER, MQTT_PASS);
}

String jsonEscape(const String& s) {
  String out; out.reserve(s.length()+8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c=='"') out += "\\\"";
    else if (c=='\\') out += "\\\\";
    else if (c=='\n') out += "\\n";
    else out += c;
  }
  return out;
}

bool parseUint(const String& s, uint32_t& out) {
  if (s.length()==0) return false;
  for (size_t i=0;i<s.length();++i) if (!isDigit(s[i])) return false;
  out = s.toInt();
  return true;
}

// =======================
// HTTP Handlers
// =======================
void handleHealth() {
  String body = "{";
  body += "\"status\":\"ok\",";
  body += "\"uptime_ms\":" + String(millis()) + ",";
  body += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\",";
  body += "\"mqtt_connected\":" + String(mqttClient.connected() ? "true" : "false") + ",";
  body += "\"motion_active\":" + String(motion.active ? "true" : "false") + ",";
  body += "\"telemetry_topic\":\"" MQTT_TOPIC_DISTANCE "\"";
  body += "}";
  server.send(200, "application/json", body);
}

void handleMove() {
  // Acepta GET (params en URL) o POST x-www-form-urlencoded
  String dir = server.arg("direction");
  String sp  = server.arg("speed");
  String dur = server.arg("duration_ms");

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
    server.send(409, "application/json", "{\"error\":\"motion already active; try again later\"}");
    return;
  }

  String clientIP = server.client().remoteIP().toString();
  startMotion(dir, speedPct, duration);
  publishMoveMQTT(clientIP, dir, speedPct, duration, "accepted");

  String body = "{";
  body += "\"status\":\"accepted\",";
  body += "\"direction\":\"" + dir + "\",";
  body += "\"speed\":" + String(speedPct) + ",";
  body += "\"duration_ms\":" + String(duration) + ",";
  body += "\"client_ip\":\"" + clientIP + "\"";
  body += "}";
  server.send(202, "application/json", body);
}

// ======== Motores ========
uint8_t pctToPwm(uint8_t pct) { return (uint8_t)((pct * 255) / 100); }

void motorsStop() {
  ledcWrite(PWM_CH_A, 0);
  ledcWrite(PWM_CH_B, 0);
  digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
}

void applyMotion(const String& direction, uint8_t speedPct) {
  uint8_t pwm = pctToPwm(speedPct);
  if (direction == "forward") {
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  } else if (direction == "backward") {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
  } else if (direction == "left") {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  } else if (direction == "right") {
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
  } else {
    motorsStop();
    return;
  }
  ledcWrite(PWM_CH_A, pwm);
  ledcWrite(PWM_CH_B, pwm);
}

void startMotion(const String& direction, uint8_t speedPct, uint32_t durationMs) {
  motion.active = true;
  motion.direction = direction;
  motion.speedPct = speedPct;
  motion.durationMs = durationMs;
  motion.startedAt = millis();
  applyMotion(direction, speedPct);
}

// =======================
// SETUP y LOOP
// =======================
void setup() {
  Serial.begin(115200);

  // Motores + PWM
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  ledcSetup(PWM_CH_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENA, PWM_CH_A);
  ledcSetup(PWM_CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENB, PWM_CH_B);
  motorsStop();

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(); Serial.print("IP: "); Serial.println(WiFi.localIP());

  // MQTT
  espClient.setCACert(root_ca);
  mqttClient.setServer(MQTT_HOST, 8883);
  ensureMqtt();

  // HTTP routes
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/api/move", HTTP_ANY, handleMove);   // ← Ruta de movimiento
  server.onNotFound([](){
    server.send(404, "text/plain", String("Not found: ") + server.uri());
  });
  server.begin();
  Serial.println("HTTP server started");

  // Sensor (mock o real)
  sensorInit();
}

void loop() {
  server.handleClient();

  if (!mqttClient.connected()) ensureMqtt();
  mqttClient.loop();

  // Fin de movimiento por duración
  if (motion.active && (millis() - motion.startedAt >= motion.durationMs)) {
    motorsStop();
    motion.active = false;
  }

  // Telemetría del sensor
  if (mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastTelemetry >= TELEMETRY_PERIOD_MS) {
      lastTelemetry = now;
      float d = readDistanceCm();
      char payload[160];
      if (isnan(d)) {
        snprintf(payload, sizeof(payload),
          "{\"device\":\"esp32car\",\"type\":\"ultrasonic\",\"unit\":\"cm\",\"distance\":null,\"ts\":%lu}", now);
      } else {
        snprintf(payload, sizeof(payload),
          "{\"device\":\"esp32car\",\"type\":\"ultrasonic\",\"unit\":\"cm\",\"distance\":%.2f,\"ts\":%lu}", d, now);
      }
      mqttClient.publish(MQTT_TOPIC_DISTANCE, payload, true);
    }
  }
}

// =======================
// MQTT publicación de comandos aceptados
// =======================
void publishMoveMQTT(const String& clientIP, const String& direction, uint8_t speedPct, uint32_t durationMs, const String& status) {
  if (!mqttClient.connected()) return;
  String payload = "{";
  payload += "\"status\":\"" + status + "\",";
  payload += "\"direction\":\"" + jsonEscape(direction) + "\",";
  payload += "\"speed\":" + String(speedPct) + ",";
  payload += "\"duration_ms\":" + String(durationMs) + ",";
  payload += "\"client_ip\":\"" + jsonEscape(clientIP) + "\",";
  payload += "\"ts\":" + String((uint32_t)millis());
  payload += "}";
  mqttClient.publish(MQTT_TOPIC_MOVEMENT, payload.c_str());
}
