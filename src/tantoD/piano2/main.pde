import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

Minim minim;
AudioOutput out;

CommunicationManager comm;
InstrumentUnit instrument;

// ピアノ版なので PART_NAME は Piano
final String PART_NAME = "Piano";

// 音量。元のピアノ音色を保つため 1.0 にする
float masterVolume = 1.0f;

void setup() {
  size(760, 320);
  pixelDensity(1);

  minim = new Minim(this);
  out = minim.getLineOut(Minim.STEREO, 1024);
  out.setTempo(120);

  instrument = new InstrumentUnit(out);
  instrument.setMasterVolume(masterVolume);

  comm = new CommunicationManager(this, instrument);

  textSize(16);

  println("=== Sound_Piano Started ===");
  println("Packet: NOTE_OFF / NOTE / VEL / LONG");
  println("NOTE: 0=C4, 1=D4, 2=E4, 3=F4, 4=G4, 5=A4, 6=REST");
  println("LONG: 2=Half Note, 4=Quarter Note, 8=Eighth Note");
}

void draw() {
  background(0);

  drawWaveform();
  drawInfo();
}

void drawWaveform() {
  stroke(255);

  for (int i = 0; i < out.bufferSize() - 1; i++) {
    float x1 = map(i, 0, out.bufferSize() - 1, 0, width);
    float x2 = map(i + 1, 0, out.bufferSize() - 1, 0, width);

    line(
      x1,
      135 - out.left.get(i) * 80,
      x2,
      135 - out.left.get(i + 1) * 80
    );

    line(
      x1,
      245 - out.right.get(i) * 80,
      x2,
      245 - out.right.get(i + 1) * 80
    );
  }
}

void drawInfo() {
  fill(255);
  text("Sound_Piano — LONG Controlled Harmonic Decay Model", 20, 30);
  text("4Byte Packet: NOTE_OFF / NOTE / VEL / LONG", 20, 55);
  text("NOTE: 0=C4  1=D4  2=E4  3=F4  4=G4  5=A4  6=REST", 20, 80);
  text("LONG: 2=Half Note  4=Quarter Note  8=Eighth Note", 20, 105);
  text("Keyboard Test: A=C4  S=D4  D=E4  F=F4  G=G4  H=A4", 20, 130);

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

void serialEvent(Serial p) {
  if (comm != null) {
    comm.serialEvent(p);
  }
}

// キーボード確認用
void keyPressed() {
  int note = keyToNoteIndex(key);

  if (note >= 0) {
    instrument.noteOffAll();

    // キーボード確認では四分音符扱い
    instrument.noteOn(note, 100, 4);
  }
}

void keyReleased() {
  int note = keyToNoteIndex(key);

  if (note >= 0) {
    instrument.noteOff(note);
  }
}

int keyToNoteIndex(char k) {
  switch (Character.toLowerCase(k)) {
    case 'a': return 0; // C4
    case 's': return 1; // D4
    case 'd': return 2; // E4
    case 'f': return 3; // F4
    case 'g': return 4; // G4
    case 'h': return 5; // A4
    default:  return -1;
  }
}

void stop() {
  if (instrument != null) {
    instrument.noteOffAll();
  }

  if (out != null) {
    out.close();
  }

  if (minim != null) {
    minim.stop();
  }

  super.stop();
}
