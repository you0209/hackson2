class InstrumentUnit {
  Minim minim;

  final int REST_NOTE = 6;

  int sampleRate = 44100;

  float masterVolume = 1.0f;

  // 音価別の3サンプル（2分=長め / 4分=標準 / 8分=短め）
  AudioSample cymbal2, cymbal4, cymbal8;

  InstrumentUnit(Minim minim) {
    this.minim = minim;
    cymbal2 = createCymbal(1.8f);   // 2分音符：長め
    cymbal4 = createCymbal(1.0f);   // 4分音符：標準
    cymbal8 = createCymbal(0.45f);  // 8分音符：短め
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
  }

  AudioSample getCymbalSample() {
    return cymbal4;
  }

  // キーボード確認用：音価指定なし（四分音符として発音）
  void noteOn(int noteIndex, int velocity) {
    noteOn(noteIndex, velocity, 4);
  }

  // noteValue: 2/4/8 の音価に応じて、長さの異なるサンプルを鳴らす
  // （2分=長め / 4分=標準 / 8分=短め）。シンバルは自然減衰のため停止は行わない。
  void noteOn(int noteIndex, int velocity, int noteValue) {
    if (noteIndex == REST_NOTE) {
      return;
    }

    if (noteIndex < 0 || noteIndex > 5) {
      println("[InstrumentUnit] Invalid note index: " + noteIndex);
      return;
    }

    // 音価で長さの異なるサンプルを選択（VELは音量には反映しない）
    AudioSample s = cymbal4;
    if (noteValue == 2) {
      s = cymbal2;
    } else if (noteValue == 8) {
      s = cymbal8;
    }
    s.trigger();

    println(
      "[InstrumentUnit] Cymbal trigger" +
      " noteIndex=" + noteIndex +
      " velocity=" + velocity +
      " noteValue=" + noteValue
    );
  }

  void noteOff(int noteIndex) {
    // シンバルは余韻を自然に残すため、NOTE_OFFでは止めない
  }

  void noteOffAll() {
    // シンバルは余韻を自然に残すため、強制停止しない
  }

  void close() {
    if (cymbal2 != null) cymbal2.close();
    if (cymbal4 != null) cymbal4.close();
    if (cymbal8 != null) cymbal8.close();
  }

  // ============================================================
  // シンバル音を Processing で合成（密な非整数モードによる金属シマー主体）
  //   ・1.8k〜約11kHz に多数（300本）の非整数モードを密に重ね、モード同士の
  //     ビート（うなり）でシンバル特有の金属シマーを作る
  //   ・倍音ごとに減衰を変え、明るい打音 → 金属の余韻へ移る
  //   ・ノイズの洗いと打撃バーストでクラッシュの空気感・衝撃を加える
  // ============================================================
  // lengthScale: 音価倍率（2分=1.8 / 4分=1.0 / 8分=0.45）。大きいほど長く鳴る。
  AudioSample createCymbal(float lengthScale) {
    float dur = 3.3f * lengthScale;                 // 2分≈5.9s / 4分≈3.3s / 8分≈1.5s
    int totalSamples = int(sampleRate * dur);
    float[] samples = new float[totalSamples];

    int numPartials = 300;
    float[] pa  = new float[numPartials];
    float[] ph  = new float[numPartials];
    float[] dph = new float[numPartials];
    float[] env = new float[numPartials];
    float[] est = new float[numPartials];

    for (int j = 0; j < numPartials; j++) {
      float base = 1800.0f * pow(1.0060f, j);              // 1.8k〜約10.7kHz
      float freq = base * (1.0f + random(-0.015f, 0.015f));
      float shape = exp(-sq((freq - 4600.0f) / 2900.0f));  // 約4.6kHz中心の山なり（明るめ）
      pa[j] = (0.18f + shape) * (0.5f + random(0.0f, 0.5f));
      ph[j]  = random(TWO_PI);
      dph[j] = TWO_PI * freq / sampleRate;
      float drate = 0.6f + random(0.0f, 0.7f) + (freq / 9000.0f) * 1.1f;
      env[j] = 1.0f;
      est[j] = exp(-(drate / lengthScale) / sampleRate);  // 音価が長いほど減衰を遅く
    }

    float norm = sqrt((float) numPartials);
    float lp = 0, last = 0;

    for (int i = 0; i < totalSamples; i++) {
      float t = i / float(sampleRate);
      float attack = min(1.0f, t / 0.004f);

      // 密なモードの合計（ビートでシマーになる）
      float metal = 0;
      for (int j = 0; j < numPartials; j++) {
        metal += pa[j] * sin(ph[j]) * env[j];
        ph[j] += dph[j];
        if (ph[j] > TWO_PI) ph[j] -= TWO_PI;
        env[j] *= est[j];
      }
      metal /= norm;

      // 中高域のノイズ洗い（空気感）
      float white = random(-1, 1);
      float hp = white - last;
      last = white;
      lp = lp + 0.50f * (hp - lp);
      float band = lp * exp(-t * 1.3f / lengthScale);

      // 明るいクラッシュの広がり＝「パーーン」と伸びる明るいウォッシュ（高域寄りで明るく）
      float crash = (white * 0.25f + hp * 1.2f) * exp(-t * 5.0f / lengthScale);

      // 叩いた瞬間の鋭い衝撃「パッ」
      float burst = white * exp(-t * 22.0f);

      float v = metal * 1.45f + band * 0.28f + crash * 1.9f + burst * 1.7f;
      v *= attack;

      // 終端のクリック防止フェード
      float tail = dur - t;
      if (tail < 0.3f) {
        v *= tail / 0.3f;
      }

      samples[i] = constrain(v * 0.7f * masterVolume, -1.0f, 1.0f);
    }

    AudioFormat format = new AudioFormat(sampleRate, 16, 1, true, false);
    return minim.createSample(samples, format, 1024);
  }

}
