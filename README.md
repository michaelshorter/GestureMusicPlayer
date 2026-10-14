# Gesture-controlled dance player

A hand-held TinyML controller. An Arduino Nano 33 BLE Sense recognises dance moves from its gyroscope, and a Processing sketch on the computer plays a different song for each move. Putting the board down fades the music out.

The model runs entirely on the board, trained with [Edge Impulse](https://edgeimpulse.com). This is a follow-up to an earlier voice-controlled lamp project.

## How it works

```
Nano 33 BLE Sense Rev2
  gyro (3 axes, ~71 Hz) -> 2 s window -> Spectral Analysis -> classifier
        |
        |  USB serial, one line every 250 ms:  label,confidence
        v
Processing sketch
  per-gesture confidence threshold -> 3 matching readings in a row
  -> play song (interrupts the previous one)
  idle for 3 readings -> 3 s fade-out
```

**Classes:** `disco`, `football`, `rave`, `rock` (these trigger songs), plus `idle` and `not_dancing` (these never trigger, and `idle` stops the music).

## Hardware and software

- Arduino Nano 33 BLE Sense **Rev2** (BMI270 / BMM150 IMU). For the original Sense (LSM9DS1), swap the IMU include in the sketches to `<Arduino_LSM9DS1.h>`.
- A data-capable USB cable
- Arduino IDE (the Rev2 needs the `Arduino_BMI270_BMM150` library)
- [Edge Impulse](https://edgeimpulse.com) account and the CLI (`npm install -g edge-impulse-cli`)
- [Processing 4](https://processing.org) with the Sound library
- Your own mp3 files

## Repository layout

```
arduino/
  data_forwarder/               streams IMU data for recording training clips
  data_forwarder_fixed_rate/    timer-paced alternative (steady 50 Hz)
  inference/                    runs the trained model on the board
processing/
  dance_player/                 the song player (put mp3s in its data folder)
docs/
  NOTES.md                      what I learned and changed to get it working
```

## Build it yourself

### 1. Record training data

1. Upload `arduino/data_forwarder/data_forwarder.ino` (double-tap the reset button if the upload stalls).
2. Close the Arduino Serial Monitor, then run:
   ```
   edge-impulse-data-forwarder --clean
   ```
3. Log in, pick your project, and name the axes exactly, in this order:
   ```
   accX,accY,accZ,gyrX,gyrY,gyrZ
   ```
4. In Data acquisition, record **2 s clips**, one label per move, plus `idle` and `not_dancing`.

Tips that made the biggest difference:
- **Record live-style.** Do each move the way you would dance to music, not carefully while clicking Record.
- **Vary speed, size and grip** in batches rather than within one.
- **Give casual movement its own class.** Walking, fidgeting, waving the board about and picking it up and putting it down go in `not_dancing`. Without it, moves like rave get triggered by ordinary movement.
- Keep classes roughly balanced (about 60 to 100 clips each) and keep a test set from a separate session.

Check the frequency the forwarder reports (about 71 to 73 Hz with the default sketch). Don't mix clips from different sketches in one project.

### 2. Train in Edge Impulse

Create impulse:
- Time series: window 2000 ms, frequency matching your clips (I used 71 Hz), zero-pad on
- **Input axes: gyrX, gyrY, gyrZ only.** Untick the three accelerometer axes.
- Processing block: Spectral Analysis (run Autotune, then Save parameters, then Generate features)
- Learning block: Classification (defaults: 100 epochs, learning rate 0.0005)

Check the feature explorer before training. Each class should form a cluster. Then train and check the confusion matrix and Model testing.

### 3. Deploy to the board

1. Deployment > Arduino library > Build, and download the ZIP.
2. Arduino IDE: Sketch > Include Library > Add .ZIP Library.
3. Open `arduino/inference/inference.ino`, change the `#include <YOUR_PROJECT_NAME_inferencing.h>` line to your library's header name, and upload.
4. Rebuild and reinstall the library every time you retrain.

### 4. Calibrate the thresholds

Live confidence is much lower than you would expect, so a default of 0.80 will probably never trigger. Calibrate:

1. Set `DEBUG_ALL` to `true` in `inference.ino` and upload.
2. Open the Serial Monitor at 115200 and do each move for about 10 s. Do the same with the board still on a desk and with casual movement.
3. For each gesture, the threshold should sit above the highest value that class reaches during the other runs, and below its typical winning value.
4. Set `DEBUG_ALL` back to `false` and re-upload.

### 5. Run the player

1. Put `disco.mp3`, `football.mp3`, `rock.mp3` and `rave.mp3` in `processing/dance_player/data/`.
2. Close the Serial Monitor.
3. Open `dance_player.pde` in Processing and press Run.

## Settings

| Setting | Value | Meaning |
|---|---|---|
| `DISCO_THRESHOLD` | 0.50 | Confidence needed for disco |
| `FOOTBALL_THRESHOLD` | 0.60 | Confidence needed for football |
| `ROCK_THRESHOLD` | 0.45 | Confidence needed for rock |
| `RAVE_THRESHOLD` | 0.45 | Confidence needed for rave |
| `HITS_NEEDED` | 3 | Consecutive matching readings before a song triggers |
| `ALLOWED_MISSES` | 1 | Stray readings tolerated mid-streak |
| `MIN_GAP_MS` | 1000 | Minimum time between triggers |
| `IDLE_HITS_NEEDED` | 3 | Idle readings in a row before the fade starts |
| `IDLE_THRESHOLD` | 0.80 | Confidence needed for idle |
| `FADE_MS` | 3000 | Fade-out length (0 = instant stop) |
| `INFER_EVERY_MS` (Arduino) | 250 | Time between inferences |

These thresholds were calibrated on my model and my hands. Expect to recalibrate yours, and again after any retrain.

## Results

Last validation run: 83% accuracy, loss 0.61, ROC AUC 0.97. Validation sets are small (about 7 to 17 clips per class), so individual class scores move a lot with one clip. The live test with the board in my hand was the check that mattered.

## Troubleshooting

- **Nothing in the Serial Monitor, board alive:** the IMU library probably doesn't match your board. A Rev2 needs `Arduino_BMI270_BMM150`.
- **Upload stuck on "Uploading":** double-tap reset to enter bootloader mode, reselect the port, upload again.
- **Forwarder says "0 samples" and a 400 error:** the board stopped streaming. Close other programs using the port, replug the board, and check the data forwarder sketch is on it.
- **Processing shows "Detected: X" but nothing plays:** the label is missing from `triggerLabels`, or its confidence is below that gesture's threshold. A label that isn't in `triggerLabels` can never trigger.
- **Processing ignores everything:** `DEBUG_ALL` is still `true`, so the lines aren't in `label,confidence` format.
- **A move is read as another move:** record more live-style clips of it, and check the feature explorer for overlap before retraining. A threshold cannot separate classes that overlap.
- **Music fades mid-dance:** raise `IDLE_HITS_NEEDED` or `IDLE_THRESHOLD`, or stop counting `not_dancing` as idle in `serialEvent`.

More detail is in [docs/NOTES.md](docs/NOTES.md).

## Possible next steps

- Go wireless: advertise a BLE characteristic with the gesture ID and play audio from a Web Bluetooth page, powered by a small LiPo.
- Merge `not_dancing` into `idle` and simplify the idle check.
- Try a six-axis model if a move needs board orientation to be recognised.

## Licence

Add a licence of your choice before publishing.
