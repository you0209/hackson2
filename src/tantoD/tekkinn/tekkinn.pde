class InstrumentUnit {
  AudioOutput out;

  final int REST_NOTE = 6;

  // 担当C・D共通仕様
  // 0=C4, 1=D4, 2=E4, 3=F4, 4=G4, 5=A4
  String[] pitchNames = {
    "C4", "D4", "E4", "F4", "G4", "A4"
  };

  float masterVolume = 1.0f;

  ArrayList<GlockenspielInstrument> activeVoices =
    new ArrayList<GlockenspielInstrument>();

  InstrumentUnit(AudioOutput out) {
    this.out = out;
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
  }

  // キーボード確認用：音価指定なし（四分音符として発音）
  void noteOn(int noteIndex, int velocity) {
    noteOn(noteIndex, velocity, 4);
  }

  // noteLength: 音価（2=2分 / 4=4分 / 8=8分音符）。響き・余韻の長さ（Decay/Release）に反映する。
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

    // 元コードの maxAmp=0.32 を基準に、VEL と masterVolume を反映
    float maxAmp = 0.32f * velocityRate * masterVolume;

    GlockenspielInstrument glock =
      new GlockenspielInstrument(noteIndex, frequency, maxAmp, noteLength);

    activeVoices.add(glock);
    glock.noteOn(0.0f);

    println(
      "[InstrumentUnit] Glockenspiel noteOn index=" + noteIndex +
      " pitch=" + pitchNames[noteIndex] +
      " freq=" + nf(frequency, 0, 2) +
      " amp=" + nf(maxAmp, 0, 3) +
      " noteLength=" + noteLength
    );
  }

  void noteOff(int noteIndex) {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      GlockenspielInstrument g = activeVoices.get(i);

      if (g.noteIndex == noteIndex) {
        g.noteOff();
        activeVoices.remove(i);
      }
    }
  }

  void noteOffAll() {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      GlockenspielInstrument g = activeVoices.get(i);
      g.noteOff();
      activeVoices.remove(i);
    }
  }

  float convertToFrequency(int noteIndex) {
    return Frequency.ofPitch(pitchNames[noteIndex]).asHz();
  }

  // ============================================================
  // 鉄琴音色クラス 最終版
  // ============================================================
  class GlockenspielInstrument implements Instrument {
    int noteIndex;

    Oscil[] waves;
    ADSR[] adsrs;

    Oscil detuneWave;
    ADSR detuneEnv;

    Noise hitNoise;
    ADSR hitEnv;

    Summer mix;
    MoogFilter lpFilter;
    BitCrush crusher;
    ADSR gateEnv;

    // 実音源(鉄琴 C4)解析に基づくモード比率（知覚基音 = 入力周波数×4 = 2オクターブ上）
    // 比率は知覚基音に対する倍率。×2.62 が最も長く鳴る主モード。
    float[] ratios = {
      1.00f,   // 知覚基音（打鍵ピッチ ≈1046Hz）
      2.27f,
      2.62f,   // 主モード（最も長く残る）
      4.62f,
      4.97f,
      7.30f,
      10.59f   // 高次：打音の明るい「チン」
    };

    float[] amplitudes = {
      0.55f,
      0.30f,
      1.00f,   // 主モードを最大に
      0.45f,
      0.45f,
      0.40f,
      0.70f    // 打音は強いがすぐ消える
    };

    float[] decayTimes = {
      0.80f,
      0.45f,
      1.20f,   // 主モードは長く残る
      0.25f,
      0.25f,
      0.15f,
      0.08f    // 高次はごく短い
    };

    GlockenspielInstrument(int noteIndex, float frequency, float maxAmp, int noteLength) {
      this.noteIndex = noteIndex;

      // 音価（noteLength）に応じた減衰・余韻の倍率（2分=長め / 4分=標準 / 8分=短め）。
      float lengthScale = getLengthScale(noteLength);

      // 鉄琴は記譜より2オクターブ高く鳴る → 基音を×4した値を知覚基音にする
      float f1 = frequency * 4.0f;

      mix = new Summer();

      waves = new Oscil[ratios.length];
      adsrs = new ADSR[ratios.length];

      for (int i = 0; i < ratios.length; i++) {
        waves[i] = new Oscil(
          f1 * ratios[i],
          1.0f,
          Waves.SINE
        );

        float partialAmp = maxAmp * amplitudes[i];

        adsrs[i] = new ADSR(
          partialAmp,
          0.002f,
          decayTimes[i] * lengthScale,
          0.0f,
          0.85f * lengthScale
        );

        waves[i]
          .patch(adsrs[i])
          .patch(mix);
      }

      // 主モード(×2.62)のすぐ近くに置き、ゆっくりしたうなり（金属の揺れ）を作る
      detuneWave = new Oscil(
        f1 * 2.64f,
        1.0f,
        Waves.SINE
      );

      detuneEnv = new ADSR(
        maxAmp * 0.090f,
        0.002f,
        0.85f * lengthScale,
        0.0f,
        0.75f * lengthScale
      );

      detuneWave
        .patch(detuneEnv)
        .patch(mix);

      hitNoise = new Noise(
        maxAmp * 0.20f,
        Noise.Tint.WHITE
      );

      hitEnv = new ADSR(
        1.0f,
        0.001f,
        0.020f,
        0.0f,
        0.010f
      );

      hitNoise
        .patch(hitEnv)
        .patch(mix);

      lpFilter = new MoogFilter(
        14000,
        0.02f,
        MoogFilter.Type.LP
      );

      crusher = new BitCrush(
        16,
        44100
      );

      gateEnv = new ADSR(
        1.0f,
        0.001f,
        0.001f,
        1.0f,
        1.00f * lengthScale
      );

      mix.patch(lpFilter)
         .patch(crusher)
         .patch(gateEnv);
    }

    /*
      音価コード（noteLength）→ 減衰・余韻の倍率（lengthScale）。
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
      gateEnv.patch(out);

      for (int i = 0; i < adsrs.length; i++) {
        adsrs[i].noteOn();
      }

      detuneEnv.noteOn();
      hitEnv.noteOn();

      gateEnv.noteOn();
    }

    void noteOff() {
      for (int i = 0; i < adsrs.length; i++) {
        adsrs[i].noteOff();
      }

      detuneEnv.noteOff();
      hitEnv.noteOff();

      gateEnv.noteOff();
      gateEnv.unpatchAfterRelease(out);
    }
  }
}
