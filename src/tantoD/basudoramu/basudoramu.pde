class InstrumentUnit {
  AudioOutput out;

  final int REST_NOTE = 6;

  float masterVolume = 1.0f;

  // 四分音符相当の基準発音長（秒）。キックはタイトに短く。
  final float BASE_DURATION = 0.45f;

  ArrayList<BassDrumInstrument> activeVoices =
    new ArrayList<BassDrumInstrument>();

  InstrumentUnit(AudioOutput out) {
    this.out = out;
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
  }

  /*
    音価コード（noteLength）→ 発音長の倍率（lengthScale）。
    2=2分音符（長め） / 4=4分音符（標準） / 8=8分音符（短め）。それ以外は標準扱い。
    バスドラムは一発ものなので、この倍率で鳴らす長さ（playNote の長さ）を変える。
    テンポには依存しない（長さの主権は本来 NOTE_OFF 側だが、打楽器は自然減衰で扱う）。
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

  // キーボード確認用：音価指定なし（四分音符として発音）
  void noteOn(int noteIndex, int velocity) {
    noteOn(noteIndex, velocity, 4);
  }

  // noteLength: 音価（2=2分 / 4=4分 / 8=8分音符）。鳴らす長さ（playNote の長さ）に反映する。
  void noteOn(int noteIndex, int velocity, int noteLength) {
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
      new BassDrumInstrument(ampRate, getLengthScale(noteLength));

    activeVoices.add(bass);

    // 短い打楽器なので playNote で自動的に noteOff させる。
    // 音価（lengthScale）に応じて鳴らす長さを変える（8分=短め, 2分=長め）。
    // Minim の playNote は内部で duration×(60/tempo) をかけるため、
    // ここで tempo 分を打ち消して durSec を実秒として扱う（テンポ非依存を保つ）。
    float durSec = BASE_DURATION * getLengthScale(noteLength);
    float playDur = durSec * (out.getTempo() / 60.0f);
    out.playNote(
      0.0f,
      playDur,
      bass
    );

    println(
      "[InstrumentUnit] BassDrum noteOn" +
      " noteIndex=" + noteIndex +
      " velocity=" + velocity +
      " ampRate=" + nf(ampRate, 0, 3) +
      " noteLength=" + noteLength +
      " durSec=" + nf(durSec, 0, 3)
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
    float lengthScale;

    BassDrumInstrument(float ampRate, float lengthScale) {
      this.ampRate = ampRate;
      this.lengthScale = lengthScale;

      mix = new Summer();

      body =
        new Oscil(
          150,
          0,
          Waves.SINE
        );

      punch =
        new Oscil(
          240,
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

      // ドラムセットのキック。ビーターのクリック(アタック)を鋭く通すためカットオフを高めに
      lpFilter =
        new MoogFilter(
          4200,
          0.10f,
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
          0.10f
        );

      mix.patch(lpFilter)
         .patch(crusher)
         .patch(gateEnv);
    }

    void noteOn(float duration) {
      gateEnv.patch(out);

      // ピッチを素早く深く落とす（キックの「ドッ」という締まった沈み込み）
      bodyPitch.activate(
        0.02f,
        150.0f,
        50.0f
      );

      // パンチ成分も素早く落とす（アタックの芯）
      punchPitch.activate(
        0.012f,
        240.0f,
        80.0f
      );

      // 低音はタイトに減衰しつつ、パワーを上げて強い「ドッ」にする
      // 8分音符など短い音価では発音自体が短く、0.55秒の立ち上がりが
      // 完了する前に音が切れて低音の芯が弱くなるため、音価が短い時だけ
      // 立ち上がりを詰める（2分・4分音符は元の0.55秒のまま変えない）。
      float bodyRise = 0.55f * min(1.0f, lengthScale);
      bodyAmp.activate(
        bodyRise,
        1.10f * ampRate,
        0.0f
      );

      // 中低域のパンチを強めに出す（キックの芯の太さ）
      punchAmp.activate(
        0.12f,
        0.68f * ampRate,
        0.0f
      );

      // ビーターのクリック（アタック）を大きく＝最初の「パチッ」を強く前に出す
      clickAmp.activate(
        0.016f,
        0.95f * ampRate,
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
