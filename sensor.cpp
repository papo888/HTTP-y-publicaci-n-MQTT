#include <Arduino.h>
#include "config.h"
#include "sensor.h"

#ifdef USE_MOCK_SENSOR
static float clamp(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
void sensorInit(){ randomSeed((uint32_t)esp_random()); }
float readDistanceCm(){
  static float base = 100.0;                     // cm
  if(random(0,100)<5) base = random(10,60);      // obstáculo cercano ocasional
  base += (random(-5,6))*0.3;                    // drift
  base = clamp(base,8.0,200.0);
  float noise = (random(-10,11)+random(-10,11))*0.1; // ~gauss aprox
  return clamp(base + noise, 8.0, 400.0);
}
#else
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
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000UL); // 30ms timeout
  if(dur==0) return NAN;
  float d = (dur * 0.0343f)/2.0f;                // cm a 20°C
  return (d<2.0f || d>400.0f) ? NAN : d;
}
#endif
