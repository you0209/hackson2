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

    // 元コードの maxAmp=0.32 を基準に、VEL と masterVolume を反映
    float maxAmp = 0.32f * velocityRate * masterVolume;

    GlockenspielInstrument glock =
      new GlockenspielInstrument(noteIndex, frequency, maxAmp);

    activeVoices.add(glock);
    glock.noteOn(0.0f);

    println(
      "[InstrumentUnit] Glockenspiel noteOn index=" + noteIndex +
      " pitch=" + pitchNames[noteIndex] +
      " freq=" + nf(frequency, 0, 2) +
      " amp=" + nf(maxAmp, 0, 3)
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

    float[] ratios = {
      1.000f,
      2.756f,
      5.404f,
      8.933f,
      13.350f,
      18.640f,
      25.300f
    };

    float[] amplitudes = {
      0.620f,
      0.460f,
      0.260f,
      0.150f,
      0.085f,
      0.045f,
      0.025f
    };

    float[] decayTimes = {
      1.15f,
      0.82f,
      0.52f,
      0.30f,
      0.18f,
      0.10f,
      0.06f
    };

    GlockenspielInstrument(int noteIndex, float frequency, float maxAmp) {
      this.noteIndex = noteIndex;

      mix = new Summer();

      waves = new Oscil[ratios.length];
      adsrs = new ADSR[ratios.length];

      for (int i = 0; i < ratios.length; i++) {
        waves[i] = new Oscil(
          frequency * ratios[i],
          1.0f,
          Waves.SINE
        );

        float partialAmp = maxAmp * amplitudes[i];

        adsrs[i] = new ADSR(
          partialAmp,
          0.002f,
          decayTimes[i],
          0.0f,
          0.85f
        );

        waves[i]
          .patch(adsrs[i])
          .patch(mix);
      }

      detuneWave = new Oscil(
        frequency * 2.742f,
        1.0f,
        Waves.SINE
      );

      detuneEnv = new ADSR(
        maxAmp * 0.090f,
        0.002f,
        0.85f,
        0.0f,
        0.75f
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
        9500,
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
        1.00f
      );

      mix.patch(lpFilter)
         .patch(crusher)
         .patch(gateEnv);
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
