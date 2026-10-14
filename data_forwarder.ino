/*
  data_forwarder.ino
  Streams IMU data over USB serial for the Edge Impulse data forwarder.

  Board:    Arduino Nano 33 BLE Sense Rev2 (BMI270 / BMM150 IMU)
  Output:   accX,accY,accZ,gyrX,gyrY,gyrZ  (one line per reading, numbers only)

  Original Nano 33 BLE Sense (LSM9DS1)? Change the include to:
    #include <Arduino_LSM9DS1.h>

  Notes
  - This is the sketch the training data was recorded with. It is not paced
    by a timer, so the forwarder measures about 71 to 73 Hz and the figure
    varies a little between sessions. Set the impulse frequency to match.
  - Do not print any text. The forwarder only accepts numbers.
  - If the IMU fails to start, the built-in LED flashes quickly.
  - Close the Serial Monitor before running edge-impulse-data-forwarder.
*/

#include <Arduino_BMI270_BMM150.h>

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
}

void loop() {
  float ax, ay, az, gx, gy, gz;

  if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    IMU.readGyroscope(gx, gy, gz);

    Serial.print(ax); Serial.print(',');
    Serial.print(ay); Serial.print(',');
    Serial.print(az); Serial.print(',');
    Serial.print(gx); Serial.print(',');
    Serial.print(gy); Serial.print(',');
    Serial.println(gz);
  }
}
