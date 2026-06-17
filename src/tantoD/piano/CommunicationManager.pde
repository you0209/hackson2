class CommunicationManager {
  PApplet parent;
  Serial port;
  InstrumentUnit instrument;

  final int REST_NOTE = 6;
  final int PACKET_SIZE = 4;
  final int BAUD_RATE = 115200;

  int lastNoteOff  = -1;
  int lastNote     = -1;
  int lastVelocity = -1;
  int lastLong     = -1;

  CommunicationManager(PApplet parent, InstrumentUnit instrument) {
    this.parent = parent;
    this.instrument = instrument;

    connectSerial();
  }

  void connectSerial() {
    println("--- Available Serial Ports ---");
    printArray(Serial.list());
    println("------------------------------");

    // Arduino Uno R4 WiFi など、Macでよく出る名前を優先
    for (int i = 0; i < Serial.list().length; i++) {
      String portName = Serial.list()[i];

      if (portName.contains("usbmodem") || portName.contains("usbserial")) {
        port = new Serial(parent, portName, BAUD_RATE);
        port.clear();
        port.buffer(PACKET_SIZE);

        println("[Serial] Connected Auto: " + portName);
        return;
      }
    }

    // 見つからない場合は先頭ポートへ接続
    if (Serial.list().length > 0) {
      String portName = Serial.list()[0];

      port = new Serial(parent, portName, BAUD_RATE);
      port.clear();
      port.buffer(PACKET_SIZE);

      println("[Serial] Connected Fallback: " + portName);
      return;
    }

    println("[Serial] No serial port found. Keyboard test only.");
  }

  boolean isConnected() {
    return port != null;
  }

  void serialEvent(Serial p) {
    while (p.available() >= PACKET_SIZE) {
      int noteOff = p.read();
      int note    = p.read();
      int vel     = p.read();
      int lng     = p.read();

      parsePacket(noteOff, note, vel, lng);
    }
  }

  void parsePacket(int noteOff, int note, int vel, int lng) {
    lastNoteOff  = noteOff;
    lastNote     = note;
    lastVelocity = vel;
    lastLong     = lng;

    println(
      "[PACKET] NOTE_OFF=" + noteOff +
      " NOTE=" + note +
      " VEL=" + vel +
      " LONG=" + lng
    );

    // NOTE_OFF = 1 のとき、前音を消音
    if (noteOff == 1) {
      instrument.noteOffAll();
      println("[EVENT] noteOffAll()");
    }

    // NOTE = 6 は休符
    if (note == REST_NOTE) {
      println("[EVENT] REST");
      return;
    }

    // NOTE は 0〜5 のみ有効
    if (note < 0 || note > 5) {
      println("[ERROR] Invalid note index: " + note);
      return;
    }

    // VEL = 0 のときは発音しない
    if (vel <= 0) {
      println("[EVENT] Velocity zero. No sound.");
      return;
    }

    // NOTE_OFF = 0 の場合は、前音を残したまま新しい音を重ねる
    instrument.noteOn(note, vel);
    println("[EVENT] noteOn note=" + note + " vel=" + vel + " long=" + lng);
  }
}
