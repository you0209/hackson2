/**
 * 拍検出率テスト用 — 子機（担当B）側
 *
 * フォトトランジスタで拍を検出してカウントし，
 * 一定時間光が来なくなったら（テスト終了）結果を出力する．
 *
 * 接続:
 *   A0  — フォトトランジスタ（アナログ読み取り）
 *   D2  — フォトトランジスタ（デジタル割り込み）
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. 親機 beat_rate_test_A を起動
 *   3. 親機が止まると「RESULT: DETECTED=N / TOTAL=?」が出る
 *      ※ TOTAL は親機のシリアルモニタで確認して手動で計算
 *
 * 調整:
 *   PHOTO_THRESH    … 検出閾値（%）。現行B.inoと同じ40%
 *   DEBOUNCE_MS     … 同一拍の二重カウント防止（ms）
 *   TIMEOUT_MS      … この時間以上光が来なければテスト終了
 */

#define PHOTO_PIN     A0
#define PHOTO_INT_PIN 2

const float PHOTO_THRESH = 10.0f;   // 検出閾値（%）
const int   DEBOUNCE_MS  = 200;     // デバウンス時間
const unsigned long TIMEOUT_MS = 3000UL; // 3秒光が来なければ終了

volatile bool beatDetected = false;
volatile bool stateHigh    = false;
volatile bool high         = false;
volatile float photoValue  = 0.0f;
unsigned long lastBeatTime = 0;
unsigned long currentBeatTime = 0;
int detectedCount = 0;
bool testRunning  = false;
bool testDone     = false;
unsigned long lastDetectTime = 0;

void photoISR() {
  unsigned long now = millis();
  if (now - lastBeatTime >= DEBOUNCE_MS) {
    int photoRaw = analogRead(PHOTO_PIN);
    photoValue = (float)photoRaw / 1023.0 * 100.0;
    if (photoValue >= PHOTO_THRESH) high = true;
    else high = false;
    if (high) {
      if (!stateHigh) {
        beatDetected = true;
        currentBeatTime = millis();
        lastBeatTime = currentBeatTime;
        stateHigh = true;
      };
    }
    else stateHigh = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PHOTO_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PHOTO_INT_PIN), photoISR, CHANGE);
  Serial.println("beat_rate_test_B ready — waiting for first beat...");
}

void loop() {
  if (testDone) return;

  if (beatDetected) {
    beatDetected = false;
    detectedCount++;
    lastDetectTime = millis();

    if (!testRunning) {
      testRunning = true;
      Serial.println("Test started.");
    }

    Serial.print("合計: ");
    Serial.print(detectedCount);
    Serial.println("拍");
  }

  // タイムアウト判定（テスト開始後）
  if (testRunning && !testDone) {
    if (millis() - lastDetectTime > TIMEOUT_MS) {
      testDone = true;
      Serial.println("--- Test finished ---");
      Serial.print("RESULT: DETECTED=");
      Serial.println(detectedCount);
      Serial.println("(親機のTOTALと比べて 成功率 = DETECTED/TOTAL × 100 を計算してください)");
    }
  }
}
