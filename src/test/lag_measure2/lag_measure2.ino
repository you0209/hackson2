/**
 * 光→音 遅延計測スケッチ
 *
 * 接続:
 *   A0 — フォトトランジスタ（LEDの光を検出）
 *   A1 — マイクモジュール（スピーカの音を検出）
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. 親機LEDが最初に光った時点で計測セッション開始
 *   3. 以降、光→音のペアが来るたびに遅延(us)を記録
 *   4. TEST_TIMEOUT_MS 光が来なくなったらセッション終了、誤差の平均を出力
 *
 * 調整:
 *   PHOTO_THRESH    … フォトトランジスタの検出閾値（%）。photoISRと同じ基準
 *   DEBOUNCE_MS     … 同一拍の二重カウント防止（ms）。photoISRと同じ基準
 *   MIC_THRESH      … マイクの閾値（音を検出するレベル）
 *   TIMEOUT_US      … 光を検出後この時間内に音が来なければ単発タイムアウト（us）
 *   TEST_TIMEOUT_MS … この時間以上光が来なければセッション終了（ms）
 */

#define PHOTO_PIN   A0
#define MIC_PIN     A1

const float          PHOTO_THRESH    = 10.0f;      // 検出閾値（%）
const unsigned long  DEBOUNCE_MS     = 200;        // デバウンス時間
#define MIC_THRESH    550   // 無音時の中央値（約512）より少し高め
#define TIMEOUT_US    500000UL  // 500ms タイムアウト（単発の光→音）
const unsigned long  TEST_TIMEOUT_MS = 3000UL;     // 3秒光が来なければセッション終了

// フォトトランジスタ（photoISRと同じロジック）
bool          beatDetected = false;
bool          high         = false;
bool          stateHigh    = false;
float         photoValue   = 0.0f;
unsigned long lastBeatTime = 0;

bool  waiting     = false;   // 光検出待ち
unsigned long lightTime = 0;

// セッション管理・誤差集計
bool  testRunning = false;   // 最初の光を検出したらtrue
bool  testDone    = false;   // セッション終了後true
long  sumDelay    = 0;       // 遅延の合計（us）
int   sampleCount = 0;       // 計測サンプル数

// 10%基準・立ち上がりエッジ検出。highの間とhighから200msは判定しない
void photoISR() {
  unsigned long now = millis();
  if (now - lastBeatTime >= DEBOUNCE_MS) {
    int photoRaw = analogRead(PHOTO_PIN);
    photoValue = (float)photoRaw / 1023.0f * 100.0f;
    if (photoValue >= PHOTO_THRESH) high = true;
    else high = false;
    if (high) {
      if (!stateHigh) {
        beatDetected = true;
        lastBeatTime = now;
        stateHigh    = true;
      }
    } else {
      stateHigh = false;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("lag_measure2 ready");
  Serial.print("PHOTO_THRESH="); Serial.print(PHOTO_THRESH);
  Serial.print("%  MIC_THRESH="); Serial.println(MIC_THRESH);
  Serial.println("--- waiting for light ---");
}

void loop() {
  if (testDone) return;
  photoISR();

  if (beatDetected) {
    beatDetected = false;

    if (!testRunning) {
      testRunning = true;
      Serial.println("Test started.");
    }

    if (!waiting) {
      lightTime = micros();
      waiting = true;
      Serial.print("Light detected! photo=");
      Serial.print(photoValue);
      Serial.print("% at ");
      Serial.print(lightTime);
      Serial.println("us");
    }
  }

  if (waiting) {
    // 音検出 or 単発タイムアウト待ち
    int micVal = analogRead(MIC_PIN);

    if (micVal >= MIC_THRESH) {
      unsigned long soundTime = micros();
      long diff = (long)(soundTime - lightTime);
      sumDelay += diff;
      sampleCount++;
      Serial.print("Sound detected! mic=");
      Serial.print(micVal);
      Serial.print("  Light->Sound delay: ");
      Serial.print(diff);
      Serial.println(" us");
      waiting = false;
    } else if (micros() - lightTime > TIMEOUT_US) {
      Serial.println("TIMEOUT: no sound detected within 500ms");
      waiting = false;
    }
  }

  // セッション終了判定（最初の光検出後、一定時間光が来なければ終了）
  if (testRunning && !testDone) {
    if (millis() - lastBeatTime > TEST_TIMEOUT_MS) {
      testDone = true;
      Serial.println("--- Test finished ---");
      Serial.print("RESULT: SAMPLES=");
      Serial.println(sampleCount);
      if (sampleCount > 0) {
        float avgDelay = (float)sumDelay / sampleCount;
        Serial.print("AVERAGE Light->Sound delay = ");
        Serial.print(avgDelay);
        Serial.println(" us");
      } else {
        Serial.println("AVERAGE Light->Sound delay = N/A (no samples)");
      }
    }
  }
}
