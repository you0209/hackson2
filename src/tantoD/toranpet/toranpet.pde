class InstrumentUnit {
  AudioOutput out;

  final int REST_NOTE = 6;

  // 設計書にあるLPフィルタを使うか
  // true  : LPあり
  // false : LPなし
  boolean USE_DESIGN_LP_FILTER = true;

  // 担当C・D共通仕様
  // 0=C4, 1=D4, 2=E4, 3=F4, 4=G4, 5=A4
  String[] pitchNames = {
    "C4", "D4", "E4", "F4", "G4", "A4"
  };

  float masterVolume = 1.0f;

  // トランペットらしい倍音を持つ共有波形
  Waveform trumpetWave;

  ArrayList<TrumpetInstrument> activeVoices =
    new ArrayList<TrumpetInstrument>();

  InstrumentUnit(AudioOutput out) {
    this.out = out;
    initTrumpetWave();
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
  }

  void initTrumpetWave() {
    trumpetWave = WavetableGenerator.gen10(
      4096,
      new float[] {
        0.72f,  // 第1倍音
        1.00f,  // 第2倍音
        0.92f,  // 第3倍音
        0.76f,  // 第4倍音
        0.58f,  // 第5倍音
        0.42f,  // 第6倍音
        0.30f,  // 第7倍音
        0.22f,  // 第8倍音
        0.16f,  // 第9倍音
        0.11f,  // 第10倍音
        0.075f, // 第11倍音
        0.050f  // 第12倍音
      }
    );

    println("[InstrumentUnit] Trumpet waveform initialized.");
  }

  void noteOn(int noteIndex, int velocity) {
    if (noteIndex == REST_NOTE) {
      return;
    }

    if (noteIndex < 0 || noteIndex >= pitchNames.length) {
      println("[InstrumentUnit] Invalid note index: " + noteIndex);
      return;
    }

    float frequency = convertToFrequency(noteIndex);
    float velocityRate = constrain(velocity / 127.0f, 0.0f, 1.0f);

    // 元コードの volume=0.36 を基準に、VEL と masterVolume を反映
    float volume = 0.36f * velocityRate * masterVolume;

    TrumpetInstrument trumpet =
      new TrumpetInstrument(noteIndex, frequency, volume);

    activeVoices.add(trumpet);
    trumpet.noteOn(0.0f);

    println(
      "[InstrumentUnit] Trumpet noteOn index=" + noteIndex +
      " pitch=" + pitchNames[noteIndex] +
      " freq=" + nf(frequency, 0, 2) +
      " volume=" + nf(volume, 0, 3)
    );
  }

  void noteOff(int noteIndex) {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      TrumpetInstrument t = activeVoices.get(i);

      if (t.noteIndex == noteIndex) {
        t.noteOff();
        activeVoices.remove(i);
      }
    }
  }

  void noteOffAll() {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      TrumpetInstrument t = activeVoices.get(i);
      t.noteOff();
      activeVoices.remove(i);
    }
  }

  float convertToFrequency(int noteIndex) {
    return Frequency.ofPitch(pitchNames[noteIndex]).asHz();
  }

  // ============================================================
  // トランペット音色クラス 最終版
  // ============================================================
  class TrumpetInstrument implements Instrument {
    int noteIndex;

    Summer mainMix;
    Summer brightMix;

    Oscil mainOsc;
    Oscil bodyOsc;
    Oscil detuneOsc;

    Oscil edgeOsc;
    Oscil edgeDetuneOsc;
    Oscil brassBiteOsc;

    Noise breathNoise;

    ADSR mainEnv;
    ADSR brightEnv;
    ADSR breathEnv;

    MoogFilter mainLP;
    MoogFilter brightLP;

    BitCrush mainCrusher;
    BitCrush brightCrusher;

    TrumpetInstrument(int noteIndex, float frequency, float volume) {
      this.noteIndex = noteIndex;

      mainMix = new Summer();
      brightMix = new Summer();

      // ========================================================
      // 主音成分
      // ========================================================
      mainOsc = new Oscil(
        frequency,
        volume * 0.78f,
        trumpetWave
      );

      // 少し低めにずらして太さを出す
      bodyOsc = new Oscil(
        frequency * 0.997f,
        volume * 0.22f,
        trumpetWave
      );

      // 少し高めにずらして自然な揺れを出す
      detuneOsc = new Oscil(
        frequency * 1.003f,
        volume * 0.09f,
        trumpetWave
      );

      // ========================================================
      // 明るい倍音成分
      // ========================================================
      edgeOsc = new Oscil(
        frequency * 2.01f,
        volume * 0.065f,
        Waves.SAW
      );

      edgeDetuneOsc = new Oscil(
        frequency * 2.04f,
        volume * 0.030f,
        Waves.SINE
      );

      brassBiteOsc = new Oscil(
        frequency * 3.02f,
        volume * 0.020f,
        Waves.SINE
      );

      // ========================================================
      // 息成分
      // ========================================================
      breathNoise = new Noise(
        volume * 0.020f
      );

      // ========================================================
      // エンベロープ
      // ========================================================

      // 主音：ゆっくりめに立ち上がり、強すぎない持続にする
      mainEnv = new ADSR(
        1.0f,
        0.075f,  // Attack
        0.18f,   // Decay
        0.62f,   // Sustain
        0.24f    // Release
      );

      // 明るい倍音：最初に出て、少し早めに落ちる
      brightEnv = new ADSR(
        1.0f,
        0.035f,  // Attack
        0.22f,   // Decay
        0.30f,   // Sustain
        0.16f    // Release
      );

      // 息：最初に少し出て、持続中は薄く残る
      breathEnv = new ADSR(
        1.0f,
        0.015f,  // Attack
        0.080f,  // Decay
        0.045f,  // Sustain
        0.080f   // Release
      );

      // ========================================================
      // フィルタ・エフェクト
      // ========================================================
      mainLP = new MoogFilter(
        3300,
        0.04f,
        MoogFilter.Type.LP
      );

      brightLP = new MoogFilter(
        5200,
        0.05f,
        MoogFilter.Type.LP
      );

      mainCrusher = new BitCrush(
        16,
        44100
      );

      brightCrusher = new BitCrush(
        16,
        44100
      );

      // ========================================================
      // パッチ接続
      // ========================================================

      mainOsc.patch(mainMix);
      bodyOsc.patch(mainMix);
      detuneOsc.patch(mainMix);

      edgeOsc.patch(brightMix);
      edgeDetuneOsc.patch(brightMix);
      brassBiteOsc.patch(brightMix);

      if (USE_DESIGN_LP_FILTER) {
        mainMix
          .patch(mainLP)
          .patch(mainCrusher)
          .patch(mainEnv);

        brightMix
          .patch(brightLP)
          .patch(brightCrusher)
          .patch(brightEnv);

      } else {
        mainMix
          .patch(mainCrusher)
          .patch(mainEnv);

        brightMix
          .patch(brightCrusher)
          .patch(brightEnv);
      }

      breathNoise
        .patch(breathEnv);
    }

    void noteOn(float duration) {
      mainEnv.patch(out);
      brightEnv.patch(out);
      breathEnv.patch(out);

      mainEnv.noteOn();
      brightEnv.noteOn();
      breathEnv.noteOn();
    }

    void noteOff() {
      mainEnv.noteOff();
      brightEnv.noteOff();
      breathEnv.noteOff();

      mainEnv.unpatchAfterRelease(out);
      brightEnv.unpatchAfterRelease(out);
      breathEnv.unpatchAfterRelease(out);
    }
  }
}
