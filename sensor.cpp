#include <Arduino.h>
#include "settings.h"
#include "sensor.h"

#ifdef USE_MOCK_SENSOR
// -------- MOCK ----------
static float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
void sensorInit(){ randomSeed((uint32_t)esp_random()); }
float readDistanceCm(){
  static float base = 100.0f;                       // cm
  if (random(0,100) < 5) base = random(10,60);      // obstáculo ocasional
  base += (random(-5,6))*0.3f;                      // drift suave
  base = clampf(base, 8.0f, 200.0f);
  float noise = (random(-10,11) + random(-10,11)) * 0.1f;
  return clampf(base + noise, 8.0f, 400.0f);
}
#else
// -------- HC-SR04 real ----------
void sensorInit(){
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  delay(50);
}
float readDistanceCm(){
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000UL); // timeout 30 ms
  if (dur==0) return NAN;
  float d = (dur * 0.0343f) / 2.0f;                     // cm
  return (d<2.0f || d>400.0f) ? NAN : d;
}
#endif
