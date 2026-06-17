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

    // 元コードの maxAmp=0.45 を基準にする
    float maxAmp = 0.45f * velocityRate * masterVolume;

    PianoInstrument piano = new PianoInstrument(noteIndex, frequency, maxAmp);
    activeVoices.add(piano);

    piano.noteOn(0.0f);

    println(
      "[InstrumentUnit] Piano noteOn index=" + noteIndex +
      " pitch=" + pitchNames[noteIndex] +
      " freq=" + nf(frequency, 0, 2) +
      " amp=" + nf(maxAmp, 0, 3)
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
    Oscil h4, h5, h6, h7, h8;

    ADSR e1a, e1b;
    ADSR e2a, e2b;
    ADSR e3a, e3b;
    ADSR e4, e5, e6, e7, e8;

    Noise hammerNoise;
    ADSR hammerEnv;

    MoogFilter lpFilter;
    BitCrush crusher;
    ADSR masterEnv;

    PianoInstrument(int noteIndex, float frequency, float maxAmp) {
      this.noteIndex = noteIndex;

      mix = new Summer();

      /*
        ピアノ弦は完全な整数倍音だけではなく、
        少しだけズレた成分を含むため、
        基音・2倍音・3倍音に薄いズレ成分を足す。
      */
      h1a = new Oscil(frequency * 1.000f, 1.0f, Waves.SINE);
      h1b = new Oscil(frequency * 1.004f, 1.0f, Waves.SINE);

      h2a = new Oscil(frequency * 2.010f, 1.0f, Waves.SINE);
      h2b = new Oscil(frequency * 2.018f, 1.0f, Waves.SINE);

      h3a = new Oscil(frequency * 3.020f, 1.0f, Waves.SINE);
      h3b = new Oscil(frequency * 3.035f, 1.0f, Waves.SINE);

      h4 = new Oscil(frequency * 4.050f, 1.0f, Waves.SINE);
      h5 = new Oscil(frequency * 5.080f, 1.0f, Waves.SINE);
      h6 = new Oscil(frequency * 6.120f, 1.0f, Waves.SINE);
      h7 = new Oscil(frequency * 7.180f, 1.0f, Waves.SINE);
      h8 = new Oscil(frequency * 8.240f, 1.0f, Waves.SINE);

      /*
        倍音ごとに減衰時間を変える。
        高い倍音ほど早く消すことで、
        ピアノらしい「最初は明るく、後から丸くなる」音にする。
      */
      e1a = new ADSR(maxAmp * 0.55f, 0.003f, 0.80f, 0.002f, 1.00f);
      e1b = new ADSR(maxAmp * 0.25f, 0.004f, 0.95f, 0.001f, 1.10f);

      e2a = new ADSR(maxAmp * 0.38f, 0.002f, 0.50f, 0.001f, 0.75f);
      e2b = new ADSR(maxAmp * 0.16f, 0.002f, 0.45f, 0.000f, 0.70f);

      e3a = new ADSR(maxAmp * 0.25f, 0.002f, 0.32f, 0.000f, 0.55f);
      e3b = new ADSR(maxAmp * 0.10f, 0.002f, 0.28f, 0.000f, 0.50f);

      e4 = new ADSR(maxAmp * 0.16f, 0.001f, 0.22f, 0.000f, 0.40f);
      e5 = new ADSR(maxAmp * 0.12f, 0.001f, 0.15f, 0.000f, 0.30f);
      e6 = new ADSR(maxAmp * 0.08f, 0.001f, 0.10f, 0.000f, 0.25f);
      e7 = new ADSR(maxAmp * 0.05f, 0.001f, 0.07f, 0.000f, 0.20f);
      e8 = new ADSR(maxAmp * 0.03f, 0.001f, 0.05f, 0.000f, 0.15f);

      h1a.patch(e1a).patch(mix);
      h1b.patch(e1b).patch(mix);

      h2a.patch(e2a).patch(mix);
      h2b.patch(e2b).patch(mix);

      h3a.patch(e3a).patch(mix);
      h3b.patch(e3b).patch(mix);

      h4.patch(e4).patch(mix);
      h5.patch(e5).patch(mix);
      h6.patch(e6).patch(mix);
      h7.patch(e7).patch(mix);
      h8.patch(e8).patch(mix);

      /*
        打鍵音。
        ピアノの最初の「コツッ」という成分を短いノイズで表現する。
      */
      hammerNoise = new Noise(0.22f, Noise.Tint.WHITE);
      hammerEnv = new ADSR(maxAmp * 0.14f, 0.001f, 0.010f, 0.0f, 0.006f);
      hammerNoise.patch(hammerEnv).patch(mix);

      /*
        高域を少し丸める。
        低すぎるとこもるため、6500Hz程度に設定。
      */
      lpFilter = new MoogFilter(6500, 0.03f, MoogFilter.Type.LP);

      /*
        設計上のエフェクト。
        16bit のため音質劣化は少なく、音量のまとまりを確認しやすい。
      */
      crusher = new BitCrush(16, 44100);

      /*
        全体を包むエンベロープ。
        個別ADSRの後に全体のリリースをまとめる。
      */
      masterEnv = new ADSR(1.0f, 0.001f, 0.05f, 0.90f, 1.20f);

      mix.patch(lpFilter)
         .patch(crusher)
         .patch(masterEnv);
    }

    void noteOn(float duration) {
      masterEnv.patch(out);

      e1a.noteOn();
      e1b.noteOn();

      e2a.noteOn();
      e2b.noteOn();

      e3a.noteOn();
      e3b.noteOn();

      e4.noteOn();
      e5.noteOn();
      e6.noteOn();
      e7.noteOn();
      e8.noteOn();

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

      e4.noteOff();
      e5.noteOff();
      e6.noteOff();
      e7.noteOff();
      e8.noteOff();

      hammerEnv.noteOff();
      masterEnv.noteOff();

      masterEnv.unpatchAfterRelease(out);
    }
  }
}
