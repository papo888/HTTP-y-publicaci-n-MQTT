/*
 * ESP32 Car Controller – HTTP endpoint + MQTT + Telemetría ultrasónica
 * Usa config.h, sensor.h y sensor.cpp
 */

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include "config.h"   // aquí van todos los #define
#include "sensor.h"   // declara sensorInit() y readDistanceCm()

// =======================
// Variables globales
// =======================
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

struct Motion {
  bool active = false;
  String direction = "stop";
  uint8_t speedPct = 0;
  uint32_t startedAt = 0;
  uint32_t durationMs = 0;
} motion;

unsigned long lastTelemetry = 0;

// =======================
// Declaración de funciones
// =======================
void motorsStop();
void applyMotion(const String& direction, uint8_t speedPct);
void startMotion(const String& direction, uint8_t speedPct, uint32_t durationMs);
void publishMoveMQTT(const String& clientIP, const String& direction, uint8_t speedPct, uint32_t durationMs, const String& status);

// =======================
// Funciones principales
// =======================
void ensureMqtt() {
  if (mqttClient.connected()) return;
  String cid = String(MQTT_CLIENTID_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
  mqttClient.connect(cid.c_str());
}

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
    digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, HIGH);
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
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  ledcSetup(PWM_CH_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENA, PWM_CH_A);
  ledcSetup(PWM_CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENB, PWM_CH_B);

  motorsStop();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(WiFi.localIP());

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  ensureMqtt();

  server.on("/health", HTTP_GET, handleHealth);
  server.begin();

  sensorInit();
}

void loop() {
  server.handleClient();
  if (!mqttClient.connected()) ensureMqtt();
  mqttClient.loop();

  if (motion.active && millis() - motion.startedAt >= motion.durationMs) {
    motorsStop();
    motion.active = false;
  }

  // Telemetría del sensor
  if (mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastTelemetry >= TELEMETRY_PERIOD_MS) {
      lastTelemetry = now;
      float d = readDistanceCm();
      char payload[128];
      snprintf(payload, sizeof(payload),
        "{\"distance\":%.2f,\"unit\":\"cm\",\"ts\":%lu}", d, now);
      mqttClient.publish(MQTT_TOPIC_DISTANCE, payload, true);
    }
  }
}
