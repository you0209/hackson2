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
 *   PHOTO_THRESH  … フォトトランジスタの閾値（光を検出するレベル）
 *   MIC_THRESH    … マイクの閾値（音を検出するレベル）
 *   TIMEOUT_US    … 光を検出後この時間内に音が来なければタイムアウト（us）
 */

#define PHOTO_PIN   A0
#define MIC_PIN     A1

#define PHOTO_THRESH  500   // 0〜1023、光が当たると上がる場合は高め、下がる場合は低めに設定
#define MIC_THRESH    550   // 無音時の中央値（約512）より少し高め
#define TIMEOUT_US    500000UL  // 500ms タイムアウト

bool  waiting     = false;   // 光検出待ち
unsigned long lightTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("lag_measure2 ready");
  Serial.print("PHOTO_THRESH="); Serial.print(PHOTO_THRESH);
  Serial.print("  MIC_THRESH="); Serial.println(MIC_THRESH);
  Serial.println("--- waiting for light ---");
}

void loop() {
  int photoVal = analogRead(PHOTO_PIN);

  if (!waiting) {
    // 光検出待ち
    if (photoVal >= PHOTO_THRESH) {
      lightTime = micros();
      waiting = true;
      Serial.print("Light detected! photo=");
      Serial.print(photoVal);
      Serial.print(" at ");
      Serial.print(lightTime);
      Serial.println("us");
    }
  } else {
    // 音検出 or タイムアウト待ち
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
