/*
  dance_player.pde
  Reads "label,confidence" lines from the Nano over USB serial and plays a
  song when a gesture is recognised. Fades the music out when the board is
  put down.

  Requires
  - Processing 4 with the Sound library (Sketch > Import Library > Manage Libraries)
  - One mp3 per trigger label in the sketch's data folder, named after the label:
      disco.mp3, football.mp3, rock.mp3, rave.mp3
  - The Nano running inference.ino with DEBUG_ALL = false
  - Serial Monitor closed (only one program can hold the port)
*/

import processing.serial.*;
import processing.sound.*;

// ---- tune these ----
String[] triggerLabels = {"disco", "football", "rock", "rave"};

// confidence needed per gesture (calibrated on my model, recalibrate for yours)
float   DISCO_THRESHOLD    = 0.50;
float   FOOTBALL_THRESHOLD = 0.60;
float   ROCK_THRESHOLD     = 0.45;
float   RAVE_THRESHOLD     = 0.45;

int     HITS_NEEDED      = 3;     // consecutive matching inferences before triggering
int     ALLOWED_MISSES   = 1;     // stray non-matching readings tolerated mid-streak
int     MIN_GAP_MS       = 1000;  // minimum time between any two triggers
boolean STOP_ON_IDLE     = true;  // fade out music after sustained idle
int     IDLE_HITS_NEEDED = 3;     // consecutive idle readings before the fade starts
float   IDLE_THRESHOLD   = 0.80;  // minimum confidence for idle
int     FADE_MS          = 3000;  // fade-out length (0 = stop instantly)
// --------------------

Serial port;
HashMap<String, SoundFile> songs = new HashMap<String, SoundFile>();

String currentLabel = "-";
float  currentConf  = 0;
String lastLabel    = "";
int    streak       = 0;
int    misses       = 0;
int    idleStreak   = 0;
int    lastTrigger  = -100000;
String nowPlaying   = "-";
String pending      = "";
boolean stopRequested = false;

boolean fading      = false;
int     fadeStart   = 0;

float thresholdFor(String label) {
  if (label.equals("disco"))    return DISCO_THRESHOLD;
  if (label.equals("football")) return FOOTBALL_THRESHOLD;
  if (label.equals("rock"))     return ROCK_THRESHOLD;
  if (label.equals("rave"))     return RAVE_THRESHOLD;
  return 1.0;   // unknown label: never triggers
}

void setup() {
  size(600, 320);
  textFont(createFont("Helvetica", 24));

  String[] ports = Serial.list();
  printArray(ports);
  String chosen = null;
  for (String p : ports) if (p.contains("usbmodem")) chosen = p;
  if (chosen == null) { println("No usbmodem port found"); exit(); return; }

  port = new Serial(this, chosen, 115200);
  port.bufferUntil('\n');

  // a label must be in triggerLabels to be able to trigger anything
  for (String label : triggerLabels) {
    songs.put(label, new SoundFile(this, label + ".mp3"));
  }
}

void serialEvent(Serial p) {
  String line = p.readStringUntil('\n');
  if (line == null) return;
  String[] parts = split(trim(line), ',');
  if (parts.length != 2) return;

  currentLabel = parts[0];
  currentConf  = float(parts[1]);

  boolean isTrigger = songs.containsKey(currentLabel) && currentConf >= thresholdFor(currentLabel);
  boolean isIdle    = (currentLabel.equals("idle") || currentLabel.equals("not_dancing"))
                      && currentConf >= IDLE_THRESHOLD;

  // idle streak: consecutive confident idle readings only
  if (isIdle) {
    idleStreak++;
    if (STOP_ON_IDLE && idleStreak >= IDLE_HITS_NEEDED) {
      stopRequested = true;
      idleStreak = 0;
    }
  } else {
    idleStreak = 0;
  }

  // gesture streak
  if (isTrigger) {
    misses = 0;
    if (currentLabel.equals(lastLabel)) streak++; else streak = 1;
    lastLabel = currentLabel;

    if (streak >= HITS_NEEDED && millis() - lastTrigger > MIN_GAP_MS) {
      pending = currentLabel;
      lastTrigger = millis();
      streak = 0;
    }
  } else {
    misses++;
    if (misses > ALLOWED_MISSES) {
      streak = 0;
      lastLabel = "";
    }
  }
}

void stopAllAndReset() {
  for (SoundFile s : songs.values()) {
    s.stop();
    s.amp(1.0);          // restore volume for next time
  }
  nowPlaying = "-";
  fading = false;
}

void draw() {
  // new gesture: cancels any fade and wins over a pending stop
  if (pending.length() > 0) {
    SoundFile s = songs.get(pending);
    boolean alreadyPlaying = pending.equals(nowPlaying) && s.isPlaying();
    if (alreadyPlaying) {
      s.amp(1.0);        // same song during a fade: back to full volume
      fading = false;
    } else {
      stopAllAndReset();
      s.play();
      nowPlaying = pending;
    }
    pending = "";
    stopRequested = false;
  }

  // idle: start the fade (once)
  if (stopRequested) {
    if (!fading && !nowPlaying.equals("-")) {
      if (FADE_MS <= 0) {
        stopAllAndReset();
      } else {
        fading = true;
        fadeStart = millis();
      }
    }
    stopRequested = false;
  }

  // run the fade
  if (fading) {
    float t = (millis() - fadeStart) / (float) FADE_MS;
    if (t >= 1.0) {
      stopAllAndReset();
    } else {
      SoundFile s = songs.get(nowPlaying);
      if (s != null) s.amp(1.0 - t);
    }
  }

  background(20);
  fill(255);
  text("Detected: " + currentLabel + "  (" + nf(currentConf, 1, 2) + ")", 30, 70);
  text("Gesture streak: " + streak + " / " + HITS_NEEDED, 30, 120);
  text("Idle streak: " + idleStreak + " / " + IDLE_HITS_NEEDED, 30, 170);
  text("Now playing: " + nowPlaying + (fading ? "  (fading)" : ""), 30, 220);
}
