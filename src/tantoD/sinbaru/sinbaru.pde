class InstrumentUnit {
  Minim minim;

  final int REST_NOTE = 6;

  int sampleRate = 44100;
  float duration = 4.0f;

  float masterVolume = 1.0f;

  AudioSample cymbal;

  InstrumentUnit(Minim minim) {
    this.minim = minim;
    cymbal = createCymbal();
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
  }

  AudioSample getCymbalSample() {
    return cymbal;
  }

  void noteOn(int noteIndex, int velocity) {
    if (noteIndex == REST_NOTE) {
      return;
    }

    if (noteIndex < 0 || noteIndex > 5) {
      println("[InstrumentUnit] Invalid note index: " + noteIndex);
      return;
    }

    // シンバルは一定音量のアクセント音として扱う。
    // VELは受信・ログ表示するが、音量変化には反映しない。
    cymbal.trigger();

    println(
      "[InstrumentUnit] Cymbal trigger" +
      " noteIndex=" + noteIndex +
      " velocity=" + velocity
    );
  }

  void noteOff(int noteIndex) {
    // シンバルは余韻を自然に残すため、NOTE_OFFでは止めない
  }

  void noteOffAll() {
    // シンバルは余韻を自然に残すため、強制停止しない
  }

  void close() {
    if (cymbal != null) {
      cymbal.close();
    }
  }

  // ============================================================
  // シンバル音を作る AudioSample版
  // ============================================================
  AudioSample createCymbal() {
    int totalSamples =
      int(
        sampleRate *
        duration
      );

    float[] samples =
      new float[
        totalSamples
      ];

    /*
      シンバルは広い高域ノイズと金属的な倍音が重要。
      GarageBand比較で強すぎないように、
      音量・アタック・高域ノイズを少し抑えた版。
    */
    float[] freqs = {
      2800,
      3600,
      4700,
      6100,
      7600,
      9300,
      11600,
      14200
    };

    float[] phases =
      new float[
        freqs.length
      ];

    for (int i = 0; i < phases.length; i++) {
      phases[i] =
        random(
          TWO_PI
        );
    }

    float last =
      0;

    for (int i = 0; i < totalSamples; i++) {
      float t =
        i /
        float(
          sampleRate
        );

      // 一瞬で立ち上がるが、強すぎないよう少し丸める
      float attack =
        min(
          1.0f,
          t /
          0.015f
        );

      // シンバルらしい長い余韻
      float decay =
        exp(
          -t *
          1.55f
        );

      float envelope =
        attack *
        decay;

      // ホワイトノイズ
      float noise =
        random(
          -1,
          1
        );

      // 簡易ハイパス処理
      float highNoise =
        noise -
        last *
        0.94f;

      last =
        noise;

      // 金属的な非整数倍音
      float metal =
        0;

      for (int j = 0; j < freqs.length; j++) {
        float wobble =
          1.0f +
          0.0025f *
          sin(
            TWO_PI *
            5.0f *
            t +
            j
          );

        metal +=
          sin(
            TWO_PI *
            freqs[j] *
            wobble *
            t +
            phases[j]
          ) /
          freqs.length;
      }

      // ノイズ主体、金属倍音を少し混ぜる
      float v =
        highNoise *
        0.58f +
        metal *
        0.32f;

      // 最初の打撃成分
      float hit =
        exp(
          -t *
          90.0f
        ) *
        random(
          -1,
          1
        ) *
        0.75f;

      // 全体音量を調整
      samples[i] =
        (
          v +
          hit
        ) *
        envelope *
        0.42f *
        masterVolume;

      // 音割れ防止
      samples[i] =
        constrain(
          samples[i],
          -1.0f,
          1.0f
        );
    }

    AudioFormat format =
      new AudioFormat(
        sampleRate,
        16,
        1,
        true,
        false
      );

    return minim.createSample(
      samples,
      format,
      1024
    );
  }
}
