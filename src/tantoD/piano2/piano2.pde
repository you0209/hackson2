// ============================================================
// XyloUnit.pde  ← 鉄琴担当PCのみに入れる
// 設計書 § 3.4.5 表 3.2 準拠
//
// 音色設計:
//   加算合成（非調和倍音3本）
//   比率: 基音 × 1.0, × 2.756, × 5.404（金属打楽器の非調和性）
//   振幅: 1.0, 0.4, 0.1
//   LowPassFilter（8000Hz）+ BitCrusher（16bit）
//   A=0.01s  D=0.05s  S=0.1  R=1.20s
// ============================================================

class XyloUnit extends InstrumentUnit {

  static final int   N_PARTIALS   = 3;
  static final float[] RATIOS     = {1.0f, 2.756f, 5.404f};
  static final float[] AMPLITUDES = {1.0f, 0.4f,   0.1f  };

  Oscil[]      oscs;
  ADSR[]       adsrs;
  MoogFilter[] filts;
  BitCrusher[] crushers;

  XyloUnit(AudioOutput out) {
    super(out);
    initSound();
  }

  // ---- UGenチェーン構築 ----
  void initSound() {
    oscs     = new Oscil[N_PARTIALS];
    adsrs    = new ADSR[N_PARTIALS];
    filts    = new MoogFilter[N_PARTIALS];
    crushers = new BitCrusher[N_PARTIALS];

    for (int i = 0; i < N_PARTIALS; i++) {
      oscs[i]     = new Oscil(440, 0.0f, Waves.SINE);
      adsrs[i]    = new ADSR(AMPLITUDES[i], 0.01f, 0.05f, 0.1f, 1.20f, 0f, 0f);
      filts[i]    = new MoogFilter(8000, 0.1f, MoogFilter.Type.LP);
      crushers[i] = new BitCrusher(16);
      oscs[i].patch(adsrs[i]).patch(filts[i]).patch(crushers[i]).patch(out);
    }
    addLog("XyloUnit: UGen chain initialized.");
  }

  // ---- 発音 ----
  void noteOn(int noteIndex, int velocity) {
    super.noteOn(noteIndex, velocity);
    float velNorm = constrain(velocity / 127.0f, 0.0f, 1.0f);

    for (int i = 0; i < N_PARTIALS; i++) {
      oscs[i].setFrequency(curFreq * RATIOS[i]);
      adsrs[i].setParameters(AMPLITUDES[i] * velNorm, 0.01f, 0.05f, 0.1f, 1.20f, 0f, 0f);
      adsrs[i].noteOn();
    }
    addLog("Xylo noteOn  freq=" + nf(curFreq,0,1) + "Hz  vel=" + velocity);
  }

  // ---- 消音 ----
  void noteOff(int noteIndex) {
    for (ADSR a : adsrs) a.noteOff();
    super.noteOff(noteIndex);
    addLog("Xylo noteOff");
  }

  String instrumentName() { return "Xylophone"; }
}

// ---- createUnit() — main.pde から呼ばれる ----
InstrumentUnit createUnit(AudioOutput out) {
  return new XyloUnit(out);
}
