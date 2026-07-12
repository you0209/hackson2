/**
 * カラーセンサ判定率テスト
 *
 * フォトトランジスタが光（拍）を検出するたびにカラーセンサを起動し，
 *   ・フォトトランジスタが光を検出した回数
 *   ・検出した色が TEST_COLOR と一致した回数
 * を数える．
 * 一定時間光が来なくなったら（テスト終了）結果を出力する．
 * 参考: src/tantoB/hackson_test7_melody (B.ino / hackson_test7_melody.ino)
 *
 * 接続:
 *   A0        — フォトトランジスタ（アナログ読み取り）
 *   I2C(SDA/SCL) — カラーセンサ AE_S13683_LED
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. TEST_COLOR に指定した色をフォトトランジスタ検出時にセンサへかざす
 *   3. 光が TIMEOUT_MS 以上来なくなったらテスト終了，結果を出力
 *
 * 調整:
 *   TEST_COLOR   … 一致判定したい色名（"Red","Green","Blue","Yellow","Cyan","Magenta","White"）
 *   PHOTO_THRESH … フォトトランジスタ検出閾値（%）
 *   DEBOUNCE_MS  … 同一拍の二重カウント防止（ms）
 *   TIMEOUT_MS   … この時間以上光が来なければテスト終了
 */

#include <Wire.h>
#include "AE_S13683_LED.h"

AE_S13683_LED colorSensor;

#define PHOTO_PIN A0

const char*         TEST_COLOR   = "Magenta";  // 判定したい色
const float         PHOTO_THRESH = 10.0f;     // 検出閾値（%）
const unsigned long DEBOUNCE_MS  = 200;       // デバウンス時間
const unsigned long TIMEOUT_MS   = 3000UL;    // 3秒光が来なければ終了

// フォトトランジスタ
bool          beatDetected = false;
bool          high         = false;
bool          stateHigh    = false;
float         photoValue   = 0.0f;
unsigned long lastBeatTime = 0;

// カウンタ
int photoDetectCount = 0;  // フォトトランジスタが光を検出した回数
int colorMatchCount  = 0;  // TEST_COLORと一致した回数

bool          testRunning    = false;
bool          testDone       = false;
unsigned long lastDetectTime = 0;

// rgbより色を判定
// 基準色は実測キャリブレーション値（各色4サンプルの平均を正規化）
const char* detectColor(uint16_t r, uint16_t g, uint16_t b) {
  struct ColorRef { float r, g, b; const char* name; };
  static const ColorRef refs[] = {
    {0.9972f, 0.0749f, 0.0000f, "Red"},
    {0.0545f, 0.9722f, 0.2278f, "Green"},
    {0.0107f, 0.2915f, 0.9565f, "Blue"},
    {0.3878f, 0.9017f, 0.1911f, "Yellow"},
    {0.0403f, 0.7924f, 0.6087f, "Cyan"},
    {0.9407f, 0.2471f, 0.6247f, "Magenta"},
    {0.2854f, 0.7706f, 0.5698f, "White"},
  };
  const int refCount = sizeof(refs) / sizeof(refs[0]);

  float mag = sqrt((float)r * r + (float)g * g + (float)b * b);
  if (mag < 1.0f) return "Unknown";

  float nr = r / mag, ng = g / mag, nb = b / mag;

  float bestScore = -1;
  const char* bestName = "Unknown";
  for (int i = 0; i < refCount; i++) {
    float score = nr * refs[i].r + ng * refs[i].g + nb * refs[i].b;
    if (score > bestScore) { bestScore = score; bestName = refs[i].name; }
  }
  return bestName;
}

// フォトトランジスタの検出（beat_rate_test_B と同じロジック）
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
  pinMode(PHOTO_PIN, INPUT);

  Wire.begin();
  colorSensor.begin(&Wire);
  colorSensor.colorSensorConfigOneshot(
    AE_S13683_LED::colorSensorGain::GAIN_HIGH,
    10000  // 最小露光時間 175µs
  );
  colorSensor.ledDriverConfig(false);

  Serial.print("colo_test ready — TEST_COLOR = ");
  Serial.println(TEST_COLOR);
}

void loop() {
  if (testDone) return;
  photoISR();

  if (beatDetected) {
    beatDetected = false;
    photoDetectCount++;
    lastDetectTime = millis();

    if (!testRunning) {
      testRunning = true;
      Serial.println("Test started.");
    }

    AE_S13683_LEDResult result = colorSensor.getColorSensorResultOneshot();
    const char* colorName = detectColor(result.red, result.green, result.blue);
    bool matched = (strcmp(colorName, TEST_COLOR) == 0);
    if (matched) colorMatchCount++;

    Serial.print("光検出回数: ");
    Serial.print(photoDetectCount);
    Serial.print(" / 検出色: ");
    Serial.print(colorName);
    Serial.print(matched ? " (一致)" : "");
    Serial.print(" / 一致回数: ");
    Serial.println(colorMatchCount);
  }

  // タイムアウト判定（テスト開始後）
  if (testRunning && !testDone) {
    if (millis() - lastDetectTime > TIMEOUT_MS) {
      testDone = true;
      Serial.println("--- Test finished ---");
      Serial.print("RESULT: PHOTO_COUNT=");
      Serial.print(photoDetectCount);
      Serial.print(" / COLOR_MATCH(");
      Serial.print(TEST_COLOR);
      Serial.print(")=");
      Serial.println(colorMatchCount);
      float rate = photoDetectCount > 0
                     ? (float)colorMatchCount / photoDetectCount * 100.0f
                     : 0.0f;
      Serial.print("一致率 = ");
      Serial.print(rate);
      Serial.println(" %");
    }
  }
}
