# Gesture-controlled dance player: project notes

Arduino Nano 33 BLE Sense Rev2, Edge Impulse, Processing. Four hand-held dance moves trigger songs on a computer; putting the board down fades the music out.

## 1. What the finished system is

**Pipeline**
1. The Nano reads the gyroscope and runs the Edge Impulse model on the board.
2. It prints one line per inference over USB serial: `label,confidence`.
3. A Processing sketch reads those lines, applies thresholds and streak rules, and plays or fades the music.

**Final model settings**
- Classes: `disco`, `football`, `rave`, `rock`, `idle`, `not_dancing`
- Input: gyro only (gyrX, gyrY, gyrZ), 2000 ms window, 71 Hz
- Processing block: Spectral Analysis (Autotune result: low-pass 16 Hz, order 6, FFT 256, log spectrum on)
- Learning block: Classification, defaults (100 epochs, learning rate 0.0005)
- Last validation result: 83% accuracy, loss 0.61, ROC AUC 0.97

**Final Processing settings**
```java
String[] triggerLabels = {"disco", "football", "rock", "rave"};
float DISCO_THRESHOLD    = 0.50;
float FOOTBALL_THRESHOLD = 0.60;
float ROCK_THRESHOLD     = 0.45;
float RAVE_THRESHOLD     = 0.45;
int   HITS_NEEDED        = 3;
int   ALLOWED_MISSES     = 1;
int   MIN_GAP_MS         = 1000;
boolean STOP_ON_IDLE     = true;
int   IDLE_HITS_NEEDED   = 3;
float IDLE_THRESHOLD     = 0.80;
int   FADE_MS            = 3000;
```
Arduino: `INFER_EVERY_MS = 250`, and `DEBUG_ALL = false` when running with Processing.

The disco, football and rock thresholds were fitted to an earlier version of the model. Re-check them (see section 6) whenever the model is retrained.

## 2. Hardware and software setup lessons

- **Check which board you have.** The Rev2 uses a BMI270/BMM150 IMU and needs `Arduino_BMI270_BMM150.h`. The original Sense uses the LSM9DS1 library. With the wrong library, `IMU.begin()` fails silently and nothing prints.
- **Arduino IDE 2.x "Indexing" hang.** The status-bar counter is the code-completion engine, not the package index. Deleting the cached index files in `~/Library/Arduino15` got it moving. Arduino IDE 1.8.19 and `arduino-cli` avoid indexing entirely.
- **Upload stuck on "Uploading".** Double-tap the reset button to enter bootloader mode (the LED pulses), reselect the port, then upload again.
- **Only one program can hold the serial port.** Close the Serial Monitor before running the Edge Impulse data forwarder or Processing.
- **A restart does not clear cache files.** They live on disk. Only deleting them clears them.
- **The data forwarder needs numbers only.** Any startup text on serial breaks it. Use an LED flash to signal errors instead.
- **Axis names and order must never change:** `accX,accY,accZ,gyrX,gyrY,gyrZ`. Enter them identically every session.
- **The forwarder rate varies between sessions** (71, 72, 73 Hz) because the recording sketch is not paced by a timer. In Create impulse, type the frequency into the box (not the circular arrow, which resets it to the detected rate). Studio resamples clips to the impulse frequency.
- **"0 samples" and a 400 error from the forwarder** means the board stopped streaming. Close other port users, replug the board, and check the streaming sketch (not the inference sketch) is on the board.

## 3. Data collection lessons

- **Live-style recordings beat careful ones.** Disco read as football live until I recorded new clips doing it the way I dance to music. Recording while clicking Record gives a more careful version of the move than the real one.
- **Variety beats volume.** Record in batches, changing speed, size and grip between batches rather than within them.
- **Start mid-move.** Starting a clip mid-move gives varied phase, so the model sees moves at any point in their cycle.
- **You need non-gesture classes.** Without `idle` or `not_dancing` the model has to call everything one of the moves, which causes false triggers.
- **Casual movement needs its own labelled data.** Rave only separated from walking and fidgeting after I recorded 50 to 60 `not_dancing` clips of exactly that.
- **Keep classes roughly balanced.** A class with far more clips pulls borderline readings towards it.
- **Hold back a test set**, ideally recorded in a separate session, so the score is not flattered by near-identical clips.
- **Final data counts (before the last top-up):** training disco 101, football 54, idle 42, not_dancing 59, rave 97, rock 44. I then added not_dancing, rock, idle and football clips.

## 4. Edge Impulse lessons

- **Look at the raw traces first.** Comparing clips side by side showed the moves had different signatures (rock is a regular wave at about 3.5 Hz, disco is a large slow swing, football is mostly gyroZ). That ruled out "the moves look identical".
- **Three of the six sensor axes were hurting the model.** The first impulse used all six axes (accX, accY, accZ, gyrX, gyrY, gyrZ). In the raw traces the three accelerometer lines sat flat near zero, because acceleration is in g (about ±1) while the gyro is in degrees per second (hundreds). They carried almost no signal for these moves but still added input features for the model to wade through. Untick them in Create impulse so only gyrX, gyrY and gyrZ remain.
  - Validation accuracy went from 48% (six axes) to 65% (gyro only), and the feature explorer clusters tightened. I also ran Autotune in that same step, so I can't say exactly how much came from each change.
  - The deployed Arduino sketch must then feed only three values, in the order gyrX, gyrY, gyrZ. A compile-time check on `EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME` catches a mismatch.
  - The trade-off: with no accelerometer the model cannot tell how the board is held, because gravity is what gives orientation. If a move ever needs orientation to be recognised, a six-axis model is worth revisiting.
  - The data is still recorded with all six axes, so the option stays open without re-recording.
- **Autotune only tunes the processing block.** Untick unwanted axes before running it. It cannot fix indistinguishable data or bad labels.
- **Check the feature explorer before training.** Separate clusters mean it is worth training. One smear means fix the data or the processing block first.
- **Switching blocks (Spectral, Flatten, Raw) was not the fix.** All three failed the same way until the data and axes changed.
- **Validation sets are tiny.** With about 7 clips in a class, one clip moves a percentage by 14% or more. Read direction, not individual cells.
- **Changing one thing at a time** is how I could tell what helped.
- **Fixing one confusion can move it elsewhere.** Strengthening `not_dancing` fixed rave but pulled some rock and idle into it.
- **After retraining, rebuild the Arduino library.** The Serial Monitor shows whatever model is deployed, so confirm it is the latest.

**Progress across runs (validation accuracy)**
| Run | Change | Accuracy |
|---|---|---|
| 1 | Six axes, Spectral | 48% |
| 2 | Gyro only, Autotune | 65% |
| 3 | Added live-style disco | 67% |
| 4 | Added live-style rave | 77.5% |
| 5 | Added not_dancing, rock, idle, football | 83% |

## 5. Arduino inference sketch lessons

- **Feed the model exactly what it was trained on.** Same axes, same order, same raw units, same sample rate. The library applies scaling and filtering itself.
- **Take the rate and window size from the library constants** (`EI_CLASSIFIER_INTERVAL_MS`, `EI_CLASSIFIER_RAW_SAMPLE_COUNT`) so the sketch stays matched if the impulse changes.
- **Use a ring buffer** and run inference on a fixed interval (250 ms here).
- **A `DEBUG_ALL` switch** prints every class probability for the Serial Monitor. Set it to `false` before Processing, because Processing ignores any line that is not `label,confidence`.

## 6. Threshold and trigger lessons

- **Live confidence is much lower than 0.80.** Disco sat around 0.5 to 0.6, rock 0.4 to 0.7, rave 0.45 to 0.65. The original 0.80 would never have triggered.
- **Thresholds are per gesture.** Each class has its own confidence range, so each gets its own value.
- **How to set them from data.** Paste about 10 s of Serial Monitor output for each move, the desk, and casual movement. A good threshold is above the highest that class reaches during the other runs and below its typical winning value. Then replay the streak rules over the numbers to see what would trigger and when.
- **Consecutive readings overlap heavily.** Each reading looks at a 2 s window that has barely moved, so "5 in a row" gave less protection than the number suggests. Dropping to 3 saved about half a second at little cost.
- **A threshold cannot separate classes that overlap.** Rave scored as high on casual movement as on real rave, so no threshold worked. The fix was more data.
- **Allowed misses** (one stray reading tolerated mid-streak) stop a single dip resetting the count.
- **Rave had a bug that was not the model.** It showed as "Detected: rave" at 0.5 to 0.6 but never played, because `"rave"` was missing from `triggerLabels`. A label not in that list can never trigger, whatever its threshold.

## 6b. Per-gesture threshold calibration (what each run showed)

I captured about 10 s of Serial Monitor output (`DEBUG_ALL = true`) for each move, the desk, and casual movement, then replayed the Processing streak rules over the numbers. These readings are from the model before the last retrain, with a 5-hit streak at the time. A threshold has to sit above the highest that class reaches in the other runs, and below its typical winning value.

| Gesture | What it did live | Chosen threshold | Reasoning |
|---|---|---|---|
| **Disco** | Won 44 of 47 readings, but mostly at 0.5 to 0.6. Only about 6 readings reached 0.80. Main rival was rave (0.2 to 0.45). | **0.50** | 0.80 and 0.70 never triggered. 0.50 triggered in about 1.75 s. It stayed clear of the rock run, where disco only touched 0.5 on isolated readings. |
| **Rock** | Won 36 of 45 readings at 0.40 to 0.70, peak 0.70. Rival was rave (0.2 to 0.39). | **0.45** (try 0.40 if slow) | Never reached 0.80. Rock peaked at 0.29 during disco, so 0.45 is safe from it. |
| **Football** | Strongest gesture. Won 41 of 42 readings once started, settling at 0.80 to 0.94. | **0.60** | 0.80 only fired after about 7.75 s because dips to 0.73 to 0.79 kept breaking the streak. Football peaked at 0.11 in the disco run and 0.18 in the rock run, so there was room. |
| **Rave** | Won about 24 of 29 readings at 0.45 to 0.55, peak 0.66. | **0.45** | Worked against the other gestures. It did not work against casual movement in the first model (see below). |
| **Idle (desk)** | 0.99 to 1.00 on every reading. | **0.80** | Large margin. Nothing else rose above 0.01. |
| **Not_dancing** | Peaked at 0.67 in the casual run. | n/a | Never reached 0.80, so it would not have stopped the music. |

**Rave and casual movement.** In the casual-movement run rave scored 0.41 to 0.79, as high as or higher than in real rave. At 0.45 and 0.50 it false-triggered while only moving the board about, and at 0.55 and above it stopped firing during real rave as well. No threshold separated them, so rave was left out of `triggerLabels` until the model improved. Adding about 50 labelled casual-movement clips cut the not_dancing to rave leak in validation from 38% to 13%, and rave then triggered at 0.45.

**General findings**
- Dropping `HITS_NEEDED` from 5 to 3 saved about half a second, and the replayed runs showed no new false triggers for disco, rock or football.
- Nothing during casual movement reached the 0.80 idle threshold, so walking about with the board does not cut the music. Only a still board does.
- Disco and rave overlap (both read around 0.5 and can trade places), so check transitions between them live.
- Retraining shifts every class's confidence, so these values are a starting point after any new model, not a permanent setting.

## 7. Processing behaviour changes I made

1. **Interrupt on a new gesture.** Replaced the 4 s cooldown that blocked everything with a 1 s minimum gap, and a rule that the same song does not restart while playing.
2. **Stop on idle.** After 3 consecutive confident idle readings, with its own threshold (`IDLE_HITS_NEEDED`, `IDLE_THRESHOLD`). The desk reads idle at 0.99 to 1.00.
3. **Idle also counts `not_dancing`** while the classes are separate, because a board at rest or being picked up can split its probability between them.
4. **Per-gesture thresholds** in the tune-these block.
5. **Fade-out** over `FADE_MS` using `amp()`, then stop. A new gesture during a fade cancels it, the same gesture restores full volume, and volume resets to full whenever a song stops.

## 8. Mistakes and corrections along the way

- I assumed the original Sense IMU. It was a Rev2, so the first sketch failed silently.
- The first streaming sketch ran at about 72 Hz, not the 100 Hz I expected. I recorded a batch at that rate and carried it through by setting the impulse to match.
- The "indexing" hang was a different problem from the Serial Monitor problem.
- I first suggested 72 Hz data would be stretched at 50 Hz. Studio resamples properly, so it was fine to keep the data.
- Rave was left out of the Processing list, then needed adding back once the model improved.
- A 3 s window would have needed all clips re-recorded, so I kept 2 s.

## 9. If I come back to this

- **Re-check thresholds** after any retrain by pasting fresh runs of each move, the desk, and casual movement.
- **Possible clean-up:** merge `not_dancing` into `idle` and drop the extra condition in `isIdle`, but test the desk stop first.
- **If the music fades mid-dance,** raise `IDLE_HITS_NEEDED` or `IDLE_THRESHOLD`, or stop counting `not_dancing` as idle.
- **Next stage:** go wireless. The Nano can advertise a BLE characteristic that notifies the gesture ID, and a Web Bluetooth page in Chrome can play the audio with a small LiPo for power.
- **Option to try:** a six-axis model (adding the accelerometer) if a move needs board orientation to be told apart.
