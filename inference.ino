/*
  inference.ino
  Runs the Edge Impulse gesture classifier on the Nano and prints results
  over USB serial.

  Board:   Arduino Nano 33 BLE Sense Rev2 (BMI270 / BMM150 IMU)
  Input:   gyroscope only (gyrX, gyrY, gyrZ), in that order, raw units
  Output:  DEBUG_ALL = false  ->  "label,confidence"   (for Processing)
           DEBUG_ALL = true   ->  all class probabilities (for Serial Monitor)

  Setup
  1. In Edge Impulse: Deployment > Arduino library > Build, download the ZIP.
  2. Arduino IDE: Sketch > Include Library > Add .ZIP Library.
  3. Change the #include below to your library's header name.
  4. Set DEBUG_ALL to false before running the Processing sketch.

  The sample rate and window size are read from the exported library, so
  they always match the impulse you trained.

  Original Nano 33 BLE Sense (LSM9DS1)? Use #include <Arduino_LSM9DS1.h>.
*/

#include <YOUR_PROJECT_NAME_inferencing.h>   // change to your library's header name
#include <Arduino_BMI270_BMM150.h>

// ---- tune these ----
#define DEBUG_ALL      false  // true = all probabilities, false = "label,conf" for Processing
#define INFER_EVERY_MS 250    // time between inferences
// --------------------

#if EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3
#error "Impulse should have 3 axes (gyrX, gyrY, gyrZ)"
#endif

const int N = EI_CLASSIFIER_RAW_SAMPLE_COUNT;           // samples per window
static float ring[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];  // circular buffer
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

const unsigned long intervalUs = (unsigned long)(EI_CLASSIFIER_INTERVAL_MS * 1000.0f);
unsigned long nextTick;
unsigned long lastInfer = 0;
int head = 0;
int count = 0;
float gx = 0, gy = 0, gz = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  if (!IMU.begin()) {
    while (1) {                       // fast flash = IMU failed
      digitalWrite(LED_BUILTIN, HIGH); delay(100);
      digitalWrite(LED_BUILTIN, LOW);  delay(100);
    }
  }
  nextTick = micros();
}

void runInference() {
  // copy the ring buffer into a linear array, oldest sample first
  for (int i = 0; i < N; i++) {
    int idx = ((head + i) % N) * 3;
    features[i * 3 + 0] = ring[idx + 0];
    features[i * 3 + 1] = ring[idx + 1];
    features[i * 3 + 2] = ring[idx + 2];
  }

  signal_t signal;
  numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

  ei_impulse_result_t result;
  if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) return;

  int best = 0;
  for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > result.classification[best].value) best = i;
  }

#if DEBUG_ALL
  Serial.print(">> ");
  Serial.print(result.classification[best].label);
  Serial.print("  |  ");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(result.classification[i].label);
    Serial.print(' ');
    Serial.print(result.classification[i].value, 2);
    if (i < EI_CLASSIFIER_LABEL_COUNT - 1) Serial.print("  ");
  }
  Serial.println();
#else
  Serial.print(result.classification[best].label);
  Serial.print(',');
  Serial.println(result.classification[best].value, 2);
#endif
}

void loop() {
  // sample the gyro at the impulse's rate into the ring buffer
  unsigned long now = micros();
  if ((long)(now - nextTick) >= 0) {
    nextTick += intervalUs;

    if (IMU.gyroscopeAvailable()) IMU.readGyroscope(gx, gy, gz);

    ring[head * 3 + 0] = gx;
    ring[head * 3 + 1] = gy;
    ring[head * 3 + 2] = gz;
    head = (head + 1) % N;
    if (count < N) count++;
  }

  // classify on a fixed interval once the buffer is full
  if (count >= N && millis() - lastInfer >= INFER_EVERY_MS) {
    lastInfer = millis();
    runInference();
  }
}
