class InstrumentUnit {
  AudioOutput out;

  final int REST_NOTE = 6;

  // 担当C・D共通仕様
  // 0=C4, 1=D4, 2=E4, 3=F4, 4=G4, 5=A4
  String[] pitchNames = {
    "C4", "D4", "E4", "F4", "G4", "A4"
  };

  float masterVolume = 1.0f;

  ArrayList<PianoInstrument> activeVoices = new ArrayList<PianoInstrument>();

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

    // アンサンブルバランス実測に基づき基準振幅を0.45→0.60に引き上げ
    float maxAmp = 0.60f * velocityRate * masterVolume;

    PianoInstrument piano = new PianoInstrument(noteIndex, frequency, maxAmp, noteLength);
    activeVoices.add(piano);

    piano.noteOn(0.0f);

    println(
      "[InstrumentUnit] Piano noteOn index=" + noteIndex +
      " pitch=" + pitchNames[noteIndex] +
      " freq=" + nf(frequency, 0, 2) +
      " amp=" + nf(maxAmp, 0, 3) +
      " noteLength=" + noteLength
    );
  }

  void noteOff(int noteIndex) {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      PianoInstrument p = activeVoices.get(i);

      if (p.noteIndex == noteIndex) {
        p.noteOff();
        activeVoices.remove(i);
      }
    }
  }

  void noteOffAll() {
    for (int i = activeVoices.size() - 1; i >= 0; i--) {
      PianoInstrument p = activeVoices.get(i);
      p.noteOff();
      activeVoices.remove(i);
    }
  }

  float convertToFrequency(int noteIndex) {
    return Frequency.ofPitch(pitchNames[noteIndex]).asHz();
  }

  /*
    PianoInstrument
    GarageBand のピアノ音に近づけるため、
    ・倍音ごとに別々の ADSR を設定
    ・高い倍音ほど早く減衰
    ・基音と2倍音に少しズレた成分を追加
    ・最初だけ短いノイズを加えて打鍵感を表現
    ・LPフィルタで高域を少し丸める
  */
  class PianoInstrument implements Instrument {
    int noteIndex;

    Summer mix;

    Oscil h1a, h1b;
    Oscil h2a, h2b;
    Oscil h3a, h3b;
    Oscil h4a, h4b;
    Oscil h5, h6, h7, h8, h9;

    ADSR e1a, e1b;
    ADSR e2a, e2b;
    ADSR e3a, e3b;
    ADSR e4a, e4b;
    ADSR e5, e6, e7, e8, e9;

    Noise hammerNoise;
    ADSR hammerEnv;

    MoogFilter lpFilter;
    ADSR masterEnv;

    PianoInstrument(int noteIndex, float frequency, float maxAmp, int noteLength) {
      this.noteIndex = noteIndex;

      // 音価（noteLength）に応じた減衰・余韻の倍率（2分=長め / 4分=標準 / 8分=短め）。
      float lengthScale = getLengthScale(noteLength);

      mix = new Summer();

      /*
        ピアノ弦の非整数倍音（インハーモニシティ）。
        実際のピアノ弦は剛性により高次倍音ほど理想倍音より高めにズレる。
        係数は中音域のグランドピアノを参考にした値。
      */
      h1a = new Oscil(frequency * 1.0000f, 1.0f, Waves.SINE);
      h1b = new Oscil(frequency * 1.0003f, 1.0f, Waves.SINE); // 微細なコーラス

      h2a = new Oscil(frequency * 2.0008f, 1.0f, Waves.SINE);
      h2b = new Oscil(frequency * 2.0014f, 1.0f, Waves.SINE);

      h3a = new Oscil(frequency * 3.0020f, 1.0f, Waves.SINE);
      h3b = new Oscil(frequency * 3.0030f, 1.0f, Waves.SINE);

      h4a = new Oscil(frequency * 4.0040f, 1.0f, Waves.SINE);
      h4b = new Oscil(frequency * 4.0055f, 1.0f, Waves.SINE);

      h5 = new Oscil(frequency * 5.0070f, 1.0f, Waves.SINE);
      h6 = new Oscil(frequency * 6.0100f, 1.0f, Waves.SINE);
      h7 = new Oscil(frequency * 7.0140f, 1.0f, Waves.SINE);
      h8 = new Oscil(frequency * 8.0190f, 1.0f, Waves.SINE);
      h9 = new Oscil(frequency * 9.0250f, 1.0f, Waves.SINE);

      /*
        倍音ごとにADSRを設定。
        高い倍音ほど早く減衰 → 時間が経つにつれ音色が丸くなるピアノの特性を再現。
        サステインは0に近い値（ピアノはキーを押している間も自然減衰する）。
        velocityRateを活用し、強打ほど高次倍音が目立つよう振幅を調整。
      */
      float velocityRate = constrain(maxAmp / 0.60f, 0.0f, 1.0f);

      /*
        ピアノ最大の特徴「二段減衰（double decay）」を再現する。
        実際のピアノ弦は、打鍵直後に速く減衰する「プロンプト音」と、
        その後に長く尾を引く「アフターサウンド」の2つの減衰率を持つ。
        各倍音を a=プロンプト（速い初期減衰）/ b=アフターサウンド（長い余韻）の
        2層に分け、sustain=0 の長い decay で「押している間も鳴り続けながら減衰する」
        自然な挙動を作る（Minim の ADSR は sustain で固定されるため decay≈持続時間）。
        低音ほど長く響き、高音ほど短いという register 依存も反映する。
      */
      // noteIndex 0(C4)→5(A4)。高音ほど余韻が短くなる係数。
      float regTail = 1.0f - 0.35f * (noteIndex / 5.0f);

      // 基音：プロンプト（速い初期減衰）を 0.70→0.18s に短縮。
      // 本物のピアノは打鍵後0.1〜0.2sで明るい成分が消えて余韻に移行する。
      e1a = new ADSR(maxAmp * 0.55f, 0.002f, 0.18f * lengthScale, 0.0f, 0.30f * lengthScale);
      e1b = new ADSR(maxAmp * 0.36f, 0.004f, 2.50f * regTail * lengthScale, 0.0f, 0.95f * lengthScale);

      // 2倍音
      e2a = new ADSR(maxAmp * 0.34f, 0.002f, 0.14f * lengthScale, 0.0f, 0.25f * lengthScale);
      e2b = new ADSR(maxAmp * 0.20f, 0.003f, 1.30f * regTail * lengthScale, 0.0f, 0.80f * lengthScale);

      // 3倍音
      e3a = new ADSR(maxAmp * 0.20f, 0.001f, 0.11f * lengthScale, 0.0f, 0.20f * lengthScale);
      e3b = new ADSR(maxAmp * 0.11f, 0.002f, 1.20f * regTail * lengthScale, 0.0f, 0.60f * lengthScale);

      // 4倍音
      e4a = new ADSR(maxAmp * (0.11f + 0.07f * velocityRate), 0.001f, 0.08f * lengthScale, 0.0f, 0.15f * lengthScale);
      e4b = new ADSR(maxAmp * (0.04f + 0.04f * velocityRate), 0.001f, 1.40f * regTail * lengthScale, 0.0f, 0.25f * lengthScale);

      // 5〜9倍音：打鍵直後の輝きを加えるが、過剰にすると「てゅん」になるため控えめに。
      // 振幅は低め・減衰は少し余裕を持たせて自然に消える。
      e5 = new ADSR(maxAmp * (0.07f + 0.05f * velocityRate), 0.001f, 0.09f * lengthScale, 0.0f, 0.05f * lengthScale);
      e6 = new ADSR(maxAmp * (0.05f + 0.04f * velocityRate), 0.001f, 0.07f * lengthScale, 0.0f, 0.04f * lengthScale);
      e7 = new ADSR(maxAmp * (0.04f + 0.03f * velocityRate), 0.001f, 0.05f * lengthScale, 0.0f, 0.03f * lengthScale);
      e8 = new ADSR(maxAmp * (0.03f + 0.02f * velocityRate), 0.001f, 0.04f * lengthScale, 0.0f, 0.02f * lengthScale);
      e9 = new ADSR(maxAmp * (0.02f + 0.01f * velocityRate), 0.001f, 0.03f * lengthScale, 0.0f, 0.01f * lengthScale);

      h1a.patch(e1a).patch(mix);
      h1b.patch(e1b).patch(mix);

      h2a.patch(e2a).patch(mix);
      h2b.patch(e2b).patch(mix);

      h3a.patch(e3a).patch(mix);
      h3b.patch(e3b).patch(mix);

      h4a.patch(e4a).patch(mix);
      h4b.patch(e4b).patch(mix);

      h5.patch(e5).patch(mix);
      h6.patch(e6).patch(mix);
      h7.patch(e7).patch(mix);
      h8.patch(e8).patch(mix);
      h9.patch(e9).patch(mix);

      /*
        打鍵音：ピアノのハンマーが弦を叩く「コツッ」という音。
        PINKノイズは白色雑音より低域成分が豊かで木やフェルトの質感に近い。
        強打ほど打鍵音を大きく、持続時間もわずかに長くする。
      */
      // 打鍵音：ハンマーが弦を叩く「コン」という衝撃音。
      // 振幅を大きく・アタックを鋭く。ここがピアノ認識の鍵。
      hammerNoise = new Noise(1.0f, Noise.Tint.PINK);
      float hammerAmp   = maxAmp * (0.20f + 0.15f * velocityRate);
      float hammerDecay = 0.006f + 0.008f * velocityRate;
      hammerEnv = new ADSR(hammerAmp, 0.0001f, hammerDecay, 0.0f, 0.004f);
      hammerNoise.patch(hammerEnv).patch(mix);

      /*
        LPフィルタ：ベロシティが高いほどカットオフを上げて明るい音色に。
        ピアノは強打で倍音が増えて明るくなる特性がある。
        resonanceは低めに保ちナチュラルな響きを維持。
      */
      // LPF：明るさをベロシティで可変。BitCrushは除去（デジタル感の原因）。
      float filterFreq = 5000f + 5000f * velocityRate;
      lpFilter = new MoogFilter(filterFreq, 0.03f, MoogFilter.Type.LP);

      masterEnv = new ADSR(1.0f, 0.001f, 0.01f, 1.0f, 0.50f * lengthScale);

      mix.patch(lpFilter)
         .patch(masterEnv);
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
      masterEnv.patch(out);

      e1a.noteOn();
      e1b.noteOn();

      e2a.noteOn();
      e2b.noteOn();

      e3a.noteOn();
      e3b.noteOn();

      e4a.noteOn();
      e4b.noteOn();

      e5.noteOn();
      e6.noteOn();
      e7.noteOn();
      e8.noteOn();
      e9.noteOn();

      hammerEnv.noteOn();
      masterEnv.noteOn();
    }

    void noteOff() {
      e1a.noteOff();
      e1b.noteOff();

      e2a.noteOff();
      e2b.noteOff();

      e3a.noteOff();
      e3b.noteOff();

      e4a.noteOff();
      e4b.noteOff();

      e5.noteOff();
      e6.noteOff();
      e7.noteOff();
      e8.noteOff();
      e9.noteOff();

      hammerEnv.noteOff();
      masterEnv.noteOff();

      masterEnv.unpatchAfterRelease(out);
    }
  }
}
