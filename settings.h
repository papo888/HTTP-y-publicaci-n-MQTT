#ifndef CONFIG_H
#define CONFIG_H

// === Selección de sensor ===
// Comenta para usar el HC-SR04 físico
#define USE_MOCK_SENSOR 1

// === Pines motores (ajusta a tu cableado/driver) ===
#define PIN_ENA  14
#define PIN_IN1  27
#define PIN_IN2  26
#define PIN_ENB  25
#define PIN_IN3  33
#define PIN_IN4  32

// PWM
#define PWM_CH_A 0
#define PWM_CH_B 1
#define PWM_FREQ 5000
#define PWM_RES  8


// === Pines HC-SR04 ===
#define PIN_TRIG  5
#define PIN_ECHO  18

// === WiFi y MQTT ===
#define WIFI_SSID      "JuanPablo 2.4"
#define WIFI_PASSWORD  "1021632516"

#define MQTT_HOST      "test.mosquitto.org"
#define MQTT_PORT      1883
#define MQTT_USER      nullptr
#define MQTT_PASS      nullptr
#define MQTT_CLIENTID_PREFIX "esp32-car-"

#define MQTT_TOPIC_MOVEMENT   "esp32car/commands"
#define MQTT_TOPIC_DISTANCE   "esp32car/telemetry/distance"  // <— distinto al de movimiento

// Telemetría periódica
#define TELEMETRY_PERIOD_MS 1000

// Reintentos
#define MQTT_RETRY_MS 2000
#define WIFI_RETRY_MS 500

#endif
