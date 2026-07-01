// ============================================================
// LED点灯 → フォトトランジスタ検出 までのラグ測定プログラム
// （hackson_test3 互換版）
//
// 目的:
//   hackson_test3 で実際に起こる「光検出ラグ」を測定する．
//   検出条件・正規化方式を hackson_test3 と同一にすることで，
//   実装上のラグ（遅延補正値の根拠となる値）を得る．
//
// 結線（hackson_test3 と同じピン配置）:
//   LED        : LED_PIN  = D3   （330Ω 経由でGND）
//   フォトTr   : PHOTO_PIN = A1   （10kΩ プルダウン）
//
// 検出ロジック（hackson_test3 と同一）:
//   1. キャリブレーション期間に photoMin / photoMax を絶対最小・最大として記録．
//      (hackson_test3 ではCALIBRATE状態で振り運動中に値を更新する)
//   2. 計測時:
//        calibratedPhotoValue = (val - photoMin) * 100 / (photoMax - photoMin)
//   3. calibratedPhotoValue >= BEAT_DETECT_THRESH (= 50.0) で検出とする．
//   4. 単発のサンプルで即発火（hackson_test3 と同じ動作）．
//
// 1試行の流れ:
//   1. LED OFF にして残光が落ちるまで delay（INTER_TRIAL_DELAY_MS）．
//   2. t1 = micros() を記録 → 同時に LED HIGH．
//   3. analogRead → 正規化 → 50% 超えた瞬間に t2 = micros()．
//   4. ラグ = t2 - t1 [マイクロ秒] を配列に保存．
//
// 出力:
//   各試行のラグ（Trial番号, ラグ値）を CSV 形式で Serial に出力．
//   100 試行終了後，統計値（平均，最小，最大，標準偏差）を表示．
// ============================================================

// === ピン配置（hackson_test3 互換） ===
#define PHOTO_PIN A1
#define LED_PIN   3

// === 計測パラメータ ===
const int N_TRIALS = 100;                       // 試行回数
const unsigned long TIMEOUT_US = 1000000UL;     // 1試行のタイムアウト（1秒）
const int INTER_TRIAL_DELAY_MS = 80;            // 試行間の待機（残光減衰時間）

// === 検出パラメータ（hackson_test3 と同じ） ===
const float BEAT_DETECT_THRESH = 50.0f;         // 正規化後の検出閾値 [%]

// === キャリブレーションパラメータ ===
const int CALIB_CYCLES = 3;                     // OFF/ON 切り替え回数
const int CALIB_SAMPLES_PER_PHASE = 30;         // 各相のサンプル数
const int CALIB_SETTLE_MS = 300;                // 各相切り替え後の待機時間
const int MIN_RANGE_ABORT = 30;                 // photoMax-photoMin がこれ未満なら中止
const int MIN_RANGE_WARN = 200;                 // photoMax-photoMin がこれ未満なら警告

// === 計測結果 ===
unsigned long lagsUs[N_TRIALS];
int validCount = 0;

// hackson_test3 と同じ変数名・運用
int photoMin = 9999;
int photoMax = 0;

// ------------------------------------------------------------
// 自動キャリブレーション
// LED の OFF/ON を交互に複数サイクル繰り返し，観測された最小値を photoMin，
// 最大値を photoMax として記録する（hackson_test3 の CALIBRATE 処理と同等）．
// ------------------------------------------------------------
void calibrate() {
  Serial.println();
  Serial.println("=== Calibration (hackson_test3 互換) ===");
  Serial.println("LED とフォトトランジスタを正しく配置してください．");
  Serial.print(CALIB_CYCLES);
  Serial.print(" サイクル × 各相 ");
  Serial.print(CALIB_SAMPLES_PER_PHASE);
  Serial.println(" サンプル で OFF/ON を交互に測定．");
  Serial.println("最小値 photoMin と 最大値 photoMax を記録し，hackson_test3 と");
  Serial.println("同じ正規化式で 0〜100% にスケールします．");
  Serial.println();

  photoMin = 9999;
  photoMax = 0;
  long offSum = 0, onSum = 0;
  int totalOff = 0, totalOn = 0;

  for (int c = 0; c < CALIB_CYCLES; c++) {
    // ---- OFF 相 ----
    digitalWrite(LED_PIN, LOW);
    delay(CALIB_SETTLE_MS);
    int cycleOffMin = 9999, cycleOffMax = 0;
    for (int i = 0; i < CALIB_SAMPLES_PER_PHASE; i++) {
      int v = analogRead(PHOTO_PIN);
      if (v < photoMin) photoMin = v;
      if (v > photoMax) photoMax = v;
      if (v < cycleOffMin) cycleOffMin = v;
      if (v > cycleOffMax) cycleOffMax = v;
      offSum += v;
      totalOff++;
      delay(3);
    }

    // ---- ON 相 ----
    digitalWrite(LED_PIN, HIGH);
    delay(CALIB_SETTLE_MS);
    int cycleOnMin = 9999, cycleOnMax = 0;
    for (int i = 0; i < CALIB_SAMPLES_PER_PHASE; i++) {
      int v = analogRead(PHOTO_PIN);
      if (v < photoMin) photoMin = v;
      if (v > photoMax) photoMax = v;
      if (v < cycleOnMin) cycleOnMin = v;
      if (v > cycleOnMax) cycleOnMax = v;
      onSum += v;
      totalOn++;
      delay(3);
    }

    // サイクルごとに途中経過表示
    Serial.print("  cycle ");
    Serial.print(c + 1);
    Serial.print(" : OFF range[");
    Serial.print(cycleOffMin); Serial.print(","); Serial.print(cycleOffMax);
    Serial.print("]  ON range[");
    Serial.print(cycleOnMin); Serial.print(","); Serial.print(cycleOnMax);
    Serial.println("]");
  }

  digitalWrite(LED_PIN, LOW);
  delay(200);

  int offAvg = offSum / totalOff;
  int onAvg  = onSum  / totalOn;
  int range  = photoMax - photoMin;

  // 検出が起きる "生値" を逆算（表示用）
  // calibrated = (val - photoMin) * 100 / range >= 50
  // → val >= photoMin + range * 50 / 100
  int thresholdRaw = photoMin + (int)((float)range * BEAT_DETECT_THRESH / 100.0f);

  Serial.println();
  Serial.print("photoMin    = "); Serial.println(photoMin);
  Serial.print("photoMax    = "); Serial.println(photoMax);
  Serial.print("range       = "); Serial.println(range);
  Serial.print("OFF avg     = "); Serial.println(offAvg);
  Serial.print("ON  avg     = "); Serial.println(onAvg);
  Serial.print("検出閾値    = "); Serial.print(BEAT_DETECT_THRESH);
  Serial.println("% (正規化値)");
  Serial.print("検出生値    = "); Serial.print(thresholdRaw);
  Serial.println(" (= photoMin + range × 0.5)");

  // --- 妥当性判定 ---
  if (range < MIN_RANGE_ABORT) {
    Serial.println();
    Serial.println("==============================================");
    Serial.println("ERROR: photoMax - photoMin が小さすぎます．");
    Serial.println("       LED と フォトTr が向かい合っていない可能性．");
    Serial.println("       測定を中止します．配置を直してリセットしてください．");
    Serial.println("==============================================");
    while (true) delay(10000);
  }
  if (range < MIN_RANGE_WARN) {
    Serial.println();
    Serial.println("WARNING: レンジがやや小さい．光路を改善するとより安定します．");
  }
}

// ------------------------------------------------------------
// 正規化（hackson_test3 と同じ式）
//   calibratedPhotoValue = (val - photoMin) * 100 / (photoMax - photoMin)
// ------------------------------------------------------------
inline float normalize(int val) {
  if (photoMax <= photoMin) return 0.0f;
  return (float)(val - photoMin) * 100.0f / (float)(photoMax - photoMin);
}

// ------------------------------------------------------------
// 統計値の計算と表示
// ------------------------------------------------------------
void analyzeResults() {
  if (validCount == 0) {
    Serial.println("ERROR: 有効な計測がありません．");
    return;
  }

  unsigned long sum = 0;
  unsigned long maxLag = 0;
  unsigned long minLag = 0xFFFFFFFFUL;
  for (int i = 0; i < N_TRIALS; i++) {
    if (lagsUs[i] == 0) continue;
    sum += lagsUs[i];
    if (lagsUs[i] > maxLag) maxLag = lagsUs[i];
    if (lagsUs[i] < minLag) minLag = lagsUs[i];
  }
  unsigned long avg = sum / validCount;

  double sumSqDiff = 0.0;
  for (int i = 0; i < N_TRIALS; i++) {
    if (lagsUs[i] == 0) continue;
    double diff = (double)lagsUs[i] - (double)avg;
    sumSqDiff += diff * diff;
  }
  double stddev = sqrt(sumSqDiff / validCount);

  Serial.println();
  Serial.println("=========== Statistics ===========");
  Serial.print("Valid trials : ");
  Serial.print(validCount);
  Serial.print(" / ");
  Serial.println(N_TRIALS);
  Serial.print("Average lag  : ");
  Serial.print(avg);
  Serial.print(" us  (");
  Serial.print(avg / 1000.0, 3);
  Serial.println(" ms)");
  Serial.print("Min lag      : ");
  Serial.print(minLag);
  Serial.println(" us");
  Serial.print("Max lag      : ");
  Serial.print(maxLag);
  Serial.println(" us");
  Serial.print("Std deviation: ");
  Serial.print(stddev, 1);
  Serial.println(" us");
  Serial.println("==================================");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PHOTO_PIN, INPUT);
  digitalWrite(LED_PIN, LOW);

  delay(2000);
  Serial.println();
  Serial.println("=== LED→Photo Lag Measurement (hackson_test3 互換) ===");
  Serial.print("Pin config: LED=D");
  Serial.print(LED_PIN);
  Serial.print(", PHOTO=A");
  Serial.println(PHOTO_PIN - A0);
  Serial.print("Trials    : ");
  Serial.println(N_TRIALS);

  // === キャリブレーション ===
  calibrate();
  delay(1000);

  // === 計測ループ ===
  Serial.println();
  Serial.println("Trial,Lag_us");

  validCount = 0;
  for (int i = 0; i < N_TRIALS; i++) {
    // 1. LED OFF にして残光が落ちるのを待つ
    digitalWrite(LED_PIN, LOW);
    delay(INTER_TRIAL_DELAY_MS);

    // 2. 計測開始：LED ON と同時に t1 を記録
    unsigned long t1 = micros();
    digitalWrite(LED_PIN, HIGH);

    // 3. analogRead → 正規化 → 50% 超えで即発火（hackson_test3 と同じ）
    unsigned long t2 = 0;
    while (true) {
      int val = analogRead(PHOTO_PIN);
      float cv = normalize(val);
      if (cv >= BEAT_DETECT_THRESH) {
        t2 = micros();
        break;
      }
      if (micros() - t1 > TIMEOUT_US) {
        break;
      }
    }

    // 4. 結果を保存
    if (t2 > 0) {
      lagsUs[i] = t2 - t1;
      validCount++;
      Serial.print(i + 1);
      Serial.print(",");
      Serial.println(lagsUs[i]);
    } else {
      lagsUs[i] = 0;
      Serial.print(i + 1);
      Serial.println(",TIMEOUT");
    }
  }

  digitalWrite(LED_PIN, LOW);

  // === 統計 ===
  analyzeResults();
  Serial.println();
  Serial.println("Done.");
}

void loop() {
  // 計測は setup() で一度実行するのみ．以降は何もしない．
}
