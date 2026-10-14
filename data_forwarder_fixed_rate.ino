/*
  data_forwarder_fixed_rate.ino
  Same job as data_forwarder.ino, but paced by a timer so the sample rate
  is steady at FREQUENCY_HZ.

  Use this if you are starting a fresh dataset and want a locked rate.
  Do NOT mix clips from this sketch with clips from data_forwarder.ino in
  one project. Pick one, and set the impulse frequency to match.

  Board: Arduino Nano 33 BLE Sense Rev2 (BMI270 / BMM150 IMU)
  Original Sense (LSM9DS1)? Use #include <Arduino_LSM9DS1.h> instead.
*/

#include <Arduino_BMI270_BMM150.h>

#define FREQUENCY_HZ 50
#define INTERVAL_US (1000000UL / FREQUENCY_HZ)

unsigned long nextTick;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  if (!IMU.begin()) {
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }
  nextTick = micros();
}

void loop() {
  unsigned long now = micros();
  if ((long)(now - nextTick) >= 0) {
    nextTick += INTERVAL_US;

    if (IMU.accelerationAvailable()) IMU.readAcceleration(ax, ay, az);
    if (IMU.gyroscopeAvailable())    IMU.readGyroscope(gx, gy, gz);

    Serial.print(ax); Serial.print(',');
    Serial.print(ay); Serial.print(',');
    Serial.print(az); Serial.print(',');
    Serial.print(gx); Serial.print(',');
    Serial.print(gy); Serial.print(',');
    Serial.println(gz);
  }
}
