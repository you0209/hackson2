/**
 * 光→音 遅延計測スケッチ
 *
 * 接続:
 *   A0 — フォトトランジスタ（LEDの光を検出）
 *   A1 — マイクモジュール（スピーカの音を検出）
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. 親機LEDが光り → 子機から音が出るタイミングで自動計測
 *   3. 「Light→Sound delay: XXXXus」が出力される
 *
 * 調整:
 *   PHOTO_THRESH  … フォトトランジスタの検出閾値（%）。photoISRと同じ基準
 *   DEBOUNCE_MS   … 同一拍の二重カウント防止（ms）。photoISRと同じ基準
 *   MIC_THRESH    … マイクの閾値（音を検出するレベル）
 *   TIMEOUT_US    … 光を検出後この時間内に音が来なければタイムアウト（us）
 */

#define PHOTO_PIN   A0
#define MIC_PIN     A1

const float          PHOTO_THRESH = 10.0f;      // 検出閾値（%）
const unsigned long  DEBOUNCE_MS  = 200;        // デバウンス時間
#define MIC_THRESH    550   // 無音時の中央値（約512）より少し高め
#define TIMEOUT_US    500000UL  // 500ms タイムアウト

// フォトトランジスタ（photoISRと同じロジック）
bool          beatDetected = false;
bool          high         = false;
bool          stateHigh    = false;
float         photoValue   = 0.0f;
unsigned long lastBeatTime = 0;

bool  waiting     = false;   // 光検出待ち
unsigned long lightTime = 0;

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
  photoISR();

  if (!waiting) {
    // 光検出待ち
    if (beatDetected) {
      beatDetected = false;
      lightTime = micros();
      waiting = true;
      Serial.print("Light detected! photo=");
      Serial.print(photoValue);
      Serial.print("% at ");
      Serial.print(lightTime);
      Serial.println("us");
    }
  } else {
    // 音検出 or タイムアウト待ち（この間の光検出は破棄）
    beatDetected = false;
    int micVal = analogRead(MIC_PIN);

    if (micVal >= MIC_THRESH) {
      unsigned long soundTime = micros();
      long diff = (long)(soundTime - lightTime);
      Serial.print("Sound detected! mic=");
      Serial.print(micVal);
      Serial.print("  Light->Sound delay: ");
      Serial.print(diff);
      Serial.println(" us");
      waiting = false;
      delay(500);  // 連続検出防止
      Serial.println("--- waiting for light ---");
    } else if (micros() - lightTime > TIMEOUT_US) {
      Serial.println("TIMEOUT: no sound detected within 500ms");
      waiting = false;
      Serial.println("--- waiting for light ---");
    }
  }
}
