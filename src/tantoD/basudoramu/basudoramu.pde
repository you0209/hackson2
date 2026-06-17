class InstrumentUnit {
  AudioOutput out;

  final int REST_NOTE = 6;

  float masterVolume = 1.0f;

  ArrayList<BassDrumInstrument> activeVoices =
    new ArrayList<BassDrumInstrument>();

  InstrumentUnit(AudioOutput out) {
    this.out = out;
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
  }

  void noteOn(int noteIndex, int velocity) {
    if (noteIndex == REST_NOTE) {
      return;
    }

    if (noteIndex < 0 || noteIndex > 5) {
      println("[InstrumentUnit] Invalid note index: " + noteIndex);
      return;
    }

    float velocityRate =
      constrain(velocity / 127.0f, 0.0f, 1.0f);

    float ampRate =
      velocityRate * masterVolume;

    BassDrumInstrument bass =
      new BassDrumInstrument(ampRate);

    activeVoices.add(bass);

    // 短い打楽器なので playNote で自動的に noteOff させる
    out.playNote(
      0.0f,
      0.50f,
      bass
    );

    println(
      "[InstrumentUnit] BassDrum noteOn" +
      " noteIndex=" + noteIndex +
      " velocity=" + velocity +
      " ampRate=" + nf(ampRate, 0, 3)
    );
  }

  void noteOff(int noteIndex) {
    noteOffAll();
  }

  void noteOffAll() {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      BassDrumInstrument b = activeVoices.get(i);
      b.noteOff();
      activeVoices.remove(i);
    }
  }

  // ============================================================
  // バスドラム音色クラス
  // body + punch + click -> LP -> BitCrush -> gateADSR
  // ============================================================
  class BassDrumInstrument implements Instrument {
    Oscil body;       // 低い胴鳴り
    Oscil punch;      // 中低域のパンチ
    Noise click;      // 最初のビーター音

    Line bodyAmp;
    Line punchAmp;
    Line clickAmp;

    Line bodyPitch;
    Line punchPitch;

    Summer mix;

    MoogFilter lpFilter;
    BitCrush crusher;
    ADSR gateEnv;

    float ampRate;

    BassDrumInstrument(float ampRate) {
      this.ampRate = ampRate;

      mix = new Summer();

      body =
        new Oscil(
          120,
          0,
          Waves.SINE
        );

      punch =
        new Oscil(
          180,
          0,
          Waves.SINE
        );

      click =
        new Noise(
          0,
          Noise.Tint.WHITE
        );

      bodyAmp =
        new Line();

      punchAmp =
        new Line();

      clickAmp =
        new Line();

      bodyPitch =
        new Line();

      punchPitch =
        new Line();

      bodyAmp.patch(
        body.amplitude
      );

      punchAmp.patch(
        punch.amplitude
      );

      clickAmp.patch(
        click.amplitude
      );

      bodyPitch.patch(
        body.frequency
      );

      punchPitch.patch(
        punch.frequency
      );

      body.patch(mix);
      punch.patch(mix);
      click.patch(mix);

      // バスドラムの低域を残しつつ、クリック成分も少し通す
      lpFilter =
        new MoogFilter(
          2200,
          0.08f,
          MoogFilter.Type.LP
        );

      crusher =
        new BitCrush(
          16,
          44100
        );

      gateEnv =
        new ADSR(
          1.0f,
          0.001f,
          0.001f,
          1.0f,
          0.12f
        );

      mix.patch(lpFilter)
         .patch(crusher)
         .patch(gateEnv);
    }

    void noteOn(float duration) {
      gateEnv.patch(out);

      // 低音のピッチを素早く落とす
      bodyPitch.activate(
        0.07f,
        140.0f,
        45.0f
      );

      // パンチ成分も短く落とす
      punchPitch.activate(
        0.045f,
        220.0f,
        75.0f
      );

      // 低音の余韻を短くして、ボーンと残りすぎないようにする
      bodyAmp.activate(
        0.22f,
        0.82f * ampRate,
        0.0f
      );

      // 中低域のパンチを短めに出す
      punchAmp.activate(
        0.09f,
        0.38f * ampRate,
        0.0f
      );

      // クリック音は一瞬だけ、アタックを出す
      clickAmp.activate(
        0.010f,
        0.36f * ampRate,
        0.0f
      );

      gateEnv.noteOn();
    }

    void noteOff() {
      gateEnv.noteOff();
      gateEnv.unpatchAfterRelease(out);
    }
  }
}
