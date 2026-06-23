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
        0.42f,  // 第1倍音（実測：基音は弱め）
        0.97f,  // 第2倍音
        0.80f,  // 第3倍音
        1.00f,  // 第4倍音（実測：最強。ブラスのフォルマント）
        0.65f,  // 第5倍音
        0.34f,  // 第6倍音
        0.40f,  // 第7倍音
        0.18f,  // 第8倍音
        0.13f,  // 第9倍音
        0.10f,  // 第10倍音
        0.085f, // 第11倍音
        0.070f, // 第12倍音
        0.060f, // 第13倍音（低い音のこもり防止に高次を延長）
        0.052f, // 第14倍音
        0.045f, // 第15倍音
        0.038f, // 第16倍音
        0.032f, // 第17倍音
        0.026f, // 第18倍音
        0.020f, // 第19倍音
        0.015f  // 第20倍音
      }
    );

    println("[InstrumentUnit] Trumpet waveform initialized.");
  }

  // キーボード確認用：音価指定なし（四分音符として発音）
  void noteOn(int noteIndex, int velocity) {
    noteOn(noteIndex, velocity, 4);
  }

  // noteLength: 音価（2=2分 / 4=4分 / 8=8分音符）。余韻（Release）の長さに反映する。
  // 実際の消音は NOTE_OFF（noteOff / noteOffAll）で行い、ここでは自動消音しない。
  void noteOn(int noteIndex, int velocity, int noteLength) {
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
      new TrumpetInstrument(noteIndex, frequency, volume, noteLength);

    activeVoices.add(trumpet);
    trumpet.noteOn(0.0f);

    println(
      "[InstrumentUnit] Trumpet noteOn index=" + noteIndex +
      " pitch=" + pitchNames[noteIndex] +
      " freq=" + nf(frequency, 0, 2) +
      " volume=" + nf(volume, 0, 3) +
      " noteLength=" + noteLength
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

    Oscil vibMain;   // ビブラートLFO（主音）
    Oscil vibBody;   // ビブラートLFO（ボディ）
    Oscil vibDetune; // ビブラートLFO（デチューン）

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

    TrumpetInstrument(int noteIndex, float frequency, float volume, int noteLength) {
      this.noteIndex = noteIndex;

      // 音価（noteLength）に応じた余韻（Release）の倍率（2分=長め / 4分=標準 / 8分=短め）。
      float lengthScale = getLengthScale(noteLength);

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
      // ビブラート（約5.5Hz）：主音3成分の周波数を±0.5%ゆらす。
      // 持続音にトランペットらしい自然な揺れ（生っぽさ）を与える。
      // 各LFOは「中心周波数(offset) ± depth*sin」を出力し、各Oscilのfrequency入力へ接続。
      // ========================================================
      vibMain = new Oscil(5.5f, frequency * 0.005f, Waves.SINE);
      vibMain.offset.setLastValue(frequency);
      vibMain.patch(mainOsc.frequency);

      vibBody = new Oscil(5.5f, frequency * 0.997f * 0.005f, Waves.SINE);
      vibBody.offset.setLastValue(frequency * 0.997f);
      vibBody.patch(bodyOsc.frequency);

      vibDetune = new Oscil(5.5f, frequency * 1.003f * 0.005f, Waves.SINE);
      vibDetune.offset.setLastValue(frequency * 1.003f);
      vibDetune.patch(detuneOsc.frequency);

      // ========================================================
      // 明るい倍音成分
      // ========================================================
      edgeOsc = new Oscil(
        frequency * 2.01f,
        volume * 0.11f,
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
      // Release は音価（lengthScale）で伸縮させ、余韻の長さを音価に合わせる
      mainEnv = new ADSR(
        1.0f,
        0.075f,           // Attack
        0.18f,            // Decay
        0.62f,            // Sustain
        0.85f * lengthScale  // Release（余韻を長く）
      );

      // 明るい倍音：最初に出て、少し早めに落ちる
      brightEnv = new ADSR(
        1.0f,
        0.035f,           // Attack
        0.22f,            // Decay
        0.30f,            // Sustain
        0.55f * lengthScale  // Release（余韻を長く）
      );

      // 息：最初に少し出て、持続中は薄く残る
      breathEnv = new ADSR(
        1.0f,
        0.015f,           // Attack
        0.080f,           // Decay
        0.045f,           // Sustain
        0.35f * lengthScale // Release（余韻を長く）
      );

      // ========================================================
      // フィルタ・エフェクト
      // ========================================================
      mainLP = new MoogFilter(
        9000,
        0.04f,
        MoogFilter.Type.LP
      );

      brightLP = new MoogFilter(
        9000,
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

    /*
      音価コード（noteLength）→ 余韻（Release）の倍率（lengthScale）。
      2=2分音符（長め） / 4=4分音符（標準） / 8=8分音符（短め）。それ以外は標準扱い。
    */
    float getLengthScale(int noteLength) {
      if (noteLength == 2) {
        return 1.8f;
      } else if (noteLength == 4) {
        return 1.0f;
      } else if (noteLength == 8) {
        return 0.45f;
      } else {
        return 1.0f;
      }
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
