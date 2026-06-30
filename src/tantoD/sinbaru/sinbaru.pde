class InstrumentUnit {
  AudioOutput out;

  final int REST_NOTE = 6;

  int sampleRate = 44100;

  float masterVolume = 1.0f;

  // 音価別の3サンプル（2分=長め / 4分=標準 / 8分=短め）。
  // 事前レンダリングした波形を Sampler(UGen) に載せ、他の楽器と同じく out へ patch する
  // （インターフェース統一）。音源ファイルは使わず、モーダル加算合成で生成する。
  Sampler cymbal2, cymbal4, cymbal8;

  InstrumentUnit(AudioOutput out) {
    this.out = out;
    cymbal2 = createCymbal(1.8f);   // 2分音符：長め
    cymbal4 = createCymbal(1.0f);   // 4分音符：標準
    cymbal8 = createCymbal(0.45f);  // 8分音符：短め
  }

  void setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
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

    // 音価で長さの異なるサンプルを選択
    Sampler s = cymbal4;
    if (noteValue == 2) {
      s = cymbal2;
    } else if (noteValue == 8) {
      s = cymbal8;
    }

    // マスター音量 × ベロシティを発音振幅に反映（他楽器と同じ考え方。線形 0〜1）。
    // サンプルは neutral（フル音量相当）でレンダリング済みのため、
    // VEL=127・masterVolume=1.0 のとき amplitude=1.0 で従来と同じ音量になる。
    float velocityRate = constrain(velocity / 127.0f, 0.0f, 1.0f);
    float amp = constrain(masterVolume * velocityRate, 0.0f, 1.0f);
    s.amplitude.setLastValue(amp);
    s.trigger();

    println(
      "[InstrumentUnit] Cymbal trigger" +
      " noteIndex=" + noteIndex +
      " velocity=" + velocity +
      " noteValue=" + noteValue +
      " amp=" + nf(amp, 0, 3)
    );
  }

  void noteOff(int noteIndex) {
    // シンバルは余韻を自然に残すため、NOTE_OFFでは止めない
  }

  void noteOffAll() {
    // シンバルは余韻を自然に残すため、強制停止しない
  }

  void close() {
    if (cymbal2 != null) cymbal2.unpatch(out);
    if (cymbal4 != null) cymbal4.unpatch(out);
    if (cymbal8 != null) cymbal8.unpatch(out);
  }

  // ============================================================
  // クラッシュシンバル音を「密なモーダル加算合成」で生成 — 実音源(sinnbaru.wav)のSTFT解析ベース
  //   STFT解析で判明した本物の特徴：
  //   ・モード数は数百（各時刻で -30dB 以上のピークが 200〜900本）。少数では原理的にベル化する。
  //   ・スペクトル重心が時間とともに降下：アタック≈8kHz → 0.5s≈5kHz → 2〜4s≈3kHz。
  //     ＝高域ほど速く減衰し、低中域（1.5〜3kHz）が長く残る。この「降下」がクラッシュの核心。
  //   ・アタックは明るく密（やや雑音的, flat≈0.16）→ 余韻は音程的な低中域へ。
  //   設計：
  //   ・1.7〜16kHz に多数(≈420)の不規則モードを配置（密度＝シマー／賑わい）。
  //   ・★周波数依存の減衰（高域=速い/低域=遅い）で重心を降下させる＝静的ノイズにならない核心。
  //   ・低域の打撃「ドッ」＋極短のブライトノイズでアタックの衝撃。持続ノイズは入れない。
  //   生成した波形は MultiChannelBuffer→Sampler に載せ、out へ patch して trigger() で鳴らす。
  // ============================================================
  // lengthScale: 音価倍率（2分=1.8 / 4分=1.0 / 8分=0.45）。大きいほど長く鳴る。
  Sampler createCymbal(float lengthScale) {
    // ---- チューニングノブ ----
    // 各値は本物(sinnbaru.wav)のスペクトル重心カーブにPythonでフィットさせたもの
    final int   NUM         = 420;     // モード数。多いほど密＝賑やか（少ないとベル化する）
    final float LOW_HZ      = 1700.0f; // モード最低（持続する低中域の鳴り＝余韻の重心）
    final float HIGH_HZ     = 16000.0f;// モード最高（アタックの明るさ）
    final float DETUNE      = 0.020f;  // モードのデチューン（うなり＝シマー）
    final float TAU_REF     = 1.60f;   // 基準(2kHz)の減衰τ[s]。大きいほど余韻長い
    final float DECAY_TILT  = 0.70f;   // ★高域がどれだけ速く減衰するか。大きいほど重心が速く降下
    final float FREF        = 2000.0f; // 減衰の基準周波数
    final float BLOOM_RISE  = 0.05f;   // シマーの立ち上がり[s]（≈150msかけて開く）
    final float STRIKE_GAIN = 1.20f;   // 低域の打撃「ドッ」の量（実測アタックは低域40%）
    final float BURST_GAIN  = 0.50f;   // アタックの極短ブライトノイズ量
    final float METAL_GAIN  = 1.50f;   // 金属モード全体の量
    final float OUT_GAIN     = 0.55f;  // 出力スケール（割れるなら下げる）

    float dur = 4.2f * lengthScale;    // 4分≈4.2s（実測 -40dB≈4.8s に合わせ長め）
    int totalSamples = int(sampleRate * dur);
    float[] samples = new float[totalSamples];

    float[] amp = new float[NUM], ph = new float[NUM], dph = new float[NUM];
    float[] env = new float[NUM], est = new float[NUM];
    float ratioStep = pow(HIGH_HZ / LOW_HZ, 1.0f / (NUM - 1));

    for (int j = 0; j < NUM; j++) {
      // 等間隔位置に不規則ジッタ＋デチューン → 非調和な金属モード／うなり
      float jitter = pow(ratioStep, random(-0.5f, 0.5f));
      float freq = LOW_HZ * pow(ratioStep, j) * jitter * (1.0f + random(-DETUNE, DETUNE));
      freq = constrain(freq, 80.0f, sampleRate * 0.45f);

      // 初期振幅：広帯域＋3〜6kHz（ブルーミングのシマー帯）をやや強調
      float shape = 0.50f + exp(-sq((freq - 4800.0f) / 3500.0f));
      amp[j] = shape * (0.6f + random(0.0f, 0.4f));

      ph[j]  = random(TWO_PI);
      dph[j] = TWO_PI * freq / sampleRate;

      // ★周波数依存の減衰：高域ほどτが小さく速く消える → 重心が時間で降下する
      float tau = TAU_REF * pow(FREF / freq, DECAY_TILT);
      tau = constrain(tau, 0.06f, 3.0f) * lengthScale;
      env[j] = 1.0f;
      est[j] = exp(-1.0f / (tau * sampleRate));
    }

    float norm = sqrt((float) NUM);

    // 低域の打撃成分（アタックの「ドッ」＝実測アタックの低域40%）
    float[] sFreq = { 90f, 150f, 230f, 330f };
    float[] sPh   = { random(TWO_PI), random(TWO_PI), random(TWO_PI), random(TWO_PI) };
    float last = 0;

    for (int i = 0; i < totalSamples; i++) {
      float t = i / float(sampleRate);
      float bloom = 1.0f - exp(-t / BLOOM_RISE);

      // 密なモードの合計（うなりでシマー）。高域から順に消えて重心が降下する。
      float metal = 0;
      for (int j = 0; j < NUM; j++) {
        metal += amp[j] * sin(ph[j]) * env[j];
        ph[j] += dph[j];
        if (ph[j] > TWO_PI) ph[j] -= TWO_PI;
        env[j] *= est[j];
      }
      metal = (metal / norm) * bloom;

      // 低域打撃（速い減衰）
      float se = exp(-t / 0.05f);
      float strike = 0;
      for (int k = 0; k < sFreq.length; k++) {
        strike += sin(sPh[k]);
        sPh[k] += TWO_PI * sFreq[k] / sampleRate;
        if (sPh[k] > TWO_PI) sPh[k] -= TWO_PI;
      }
      strike = strike / sFreq.length * se * STRIKE_GAIN;

      // 極短のブライトノイズ（接触音）。持続させないのでノイズ臭にならない。
      float white = random(-1, 1);
      float hp = white - last;
      last = white;
      float burst = hp * exp(-t * 16.0f) * BURST_GAIN;

      float v = metal * METAL_GAIN + strike + burst;

      // 終端のクリック防止フェード
      float tail = dur - t;
      if (tail < 0.3f) {
        v *= tail / 0.3f;
      }

      samples[i] = constrain(v * OUT_GAIN, -1.0f, 1.0f);
    }

    // 生成波形を Sampler(UGen) に載せ、out へ patch する（他楽器と同じ経路に統一）
    MultiChannelBuffer buf = new MultiChannelBuffer(totalSamples, 1);
    buf.setChannel(0, samples);
    Sampler s = new Sampler(buf, sampleRate, 4);  // maxVoices=4（重ね鳴り対応）
    s.patch(out);
    return s;
  }
}
