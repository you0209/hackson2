import processing.serial.*;
import ddf.minim.*;
import javax.sound.sampled.AudioFormat;

Minim minim;

CommunicationManager comm;
InstrumentUnit instrument;

float masterVolume = 1.0f;

void setup() {
  size(760, 320);
  pixelDensity(1);

  minim = new Minim(this);

  // シンバルは AudioSample 方式なので Minim を渡す
  instrument = new InstrumentUnit(minim);
  instrument.setMasterVolume(masterVolume);

  comm = new CommunicationManager(this, instrument);

  textSize(16);

  println("=== Sound_Cymbal Started ===");
  println("Packet: NOTE_OFF / NOTE / VEL / LONG");
  println("Cymbal: NOTE 0-5 = trigger, NOTE 6 = REST");
}

void draw() {
  background(0);

  drawInfo();
  drawWaveform();
}

void drawInfo() {
  fill(255);
  text("Sound_Cymbal — AudioSample Version", 20, 30);
  text("4Byte Packet: NOTE_OFF / NOTE / VEL / LONG", 20, 55);
  text("NOTE: 0-5 = Cymbal Trigger   6 = REST", 20, 80);
  text("Keyboard Test: P = Cymbal", 20, 105);
  text("AudioSample: noise + metal partials + hit + attack/decay", 20, 130);

  if (comm.isConnected()) {
    fill(80, 220, 120);
    text("Serial: Connected", 20, 285);
  } else {
    fill(255, 120, 120);
    text("Serial: Not Connected / Keyboard test only", 20, 285);
  }

  fill(255);
  text(
    "Last Packet  NOTE_OFF=" + comm.lastNoteOff +
    "  NOTE=" + comm.lastNote +
    "  VEL=" + comm.lastVelocity +
    "  LONG=" + comm.lastLong,
    260,
    285
  );
}

void drawWaveform() {
  AudioSample sample = instrument.getCymbalSample();

  if (sample == null) {
    return;
  }

  stroke(255);

  for (int i = 0; i < sample.bufferSize() - 1; i++) {
    float x1 = map(i, 0, sample.bufferSize(), 0, width);
    float x2 = map(i + 1, 0, sample.bufferSize(), 0, width);

    line(
      x1,
      190 - sample.left.get(i) * 80,
      x2,
      190 - sample.left.get(i + 1) * 80
    );
  }
}

void serialEvent(Serial p) {
  if (comm != null) {
    comm.serialEvent(p);
  }
}

// キーボード確認用
void keyPressed() {
  char pressedKey = Character.toLowerCase(key);

  if (pressedKey == 'p') {
    instrument.noteOn(0, 110);
  }
}

void stop() {
  if (instrument != null) {
    instrument.close();
  }

  if (minim != null) {
    minim.stop();
  }

  super.stop();
}
