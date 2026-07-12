/**
 * hackson_test7 + lag_measure2 統合版
 *
 * hackson_test7（拍検出→色判定→テンポ推定→メロディ演奏キュー送信）を
 * 実機のフォトトランジスタで動かしつつ、同じ基板上でマイクによる
 * 「光検出→音検出」の遅延を計測する．
 *
 * hackson_test7オリジナルは B_loop() 内で simulateBeat()（乱数で拍を
 * 模擬生成）を使っていたが，実際の光→音遅延を測るには意味がないため
 * 既存の photoISR()（実際にA0を読む関数，元は未使用だった）を使うよう
 * 差し替えている．readColor() も同様に呼ばれていなかったため追加した．
 *
 * 接続:
 *   A0        — フォトトランジスタ（親機LEDの光を検出）
 *   A1        — マイクモジュール（スピーカの音を検出）
 *   I2C(SDA/SCL) — カラーセンサ AE_S13683_LED
 *   D5        — 子機側ステータスLED
 *   D2/D3     — OST4ML5B32A DIN_H/DIN_L
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. 親機LEDが最初に光った時点で遅延計測セッション開始
 *   3. 光→音のペアが来るたびに [lag] Light->Sound delay: を記録
 *   4. LAG_SESSION_TIMEOUT_MS 光が来なくなったらセッション終了、平均を出力
 *
 * 注意:
 *   C_melody.ino の演奏キューは Serial.write() で下流（音源側）に
 *   バイナリ送信される。マイクは実際に音が鳴る場所（下流のスピーカ）の
 *   近くに置くこと。このシリアルには本計測のログ（テキスト）も同じ
 *   ポートに出力されるため、下流機器側のパース処理によっては
 *   テキストログが混線する可能性がある（hackson_test7オリジナルの
 *   デバッグprintと同様の制約）。
 */

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_LED_Matrix.h"
#include "AE_S13683_LED.h"

ArduinoLEDMatrix matrix;
AE_S13683_LED    colorSensor;

// 共通変数
#define PHOTO_PIN A0
#define LED_PIN   5
#define DIN_H     2
#define DIN_L     3
unsigned long nextBeatTime    = 0; //次の拍の予測絶対時刻(milli)
unsigned long currentBeatTime = 0;
unsigned long lastBeatTime    = 0;
bool          beatUpdated     = false; //nextBeatTime更新時にtrue
bool          isPlaying       = false; //演奏開始時true
const char*   colorName       = "Unknown";
enum          State           {IDLE, PREBEAT, PLAYING};
State         state           = IDLE;

// ───────────────────────────────────────────
// 光→音 遅延計測（lag_measure2 を統合）
// ───────────────────────────────────────────
#define MIC_PIN A1
const float          LAG_MIC_THRESH         = 550;      // 無音時の中央値（約512）より少し高め
const unsigned long  LAG_TIMEOUT_US         = 500000UL; // 500ms 単発タイムアウト（光→音）
const unsigned long  LAG_SESSION_TIMEOUT_MS = 3000UL;   // 3秒光が来なければセッション終了

bool          lagWaiting     = false;   // 音検出待ち
unsigned long lagLightTimeUs = 0;
bool          lagTestRunning = false;   // 最初の光を検出したらtrue
bool          lagTestDone    = false;   // セッション終了後true
long          lagSumDelay    = 0;       // 遅延の合計（us）
int           lagSampleCount = 0;       // 計測サンプル数

void lagMeasure_loop() {
  if (lagTestDone) return;

  if (lagWaiting) {
    int micVal = analogRead(MIC_PIN);
    if (micVal >= LAG_MIC_THRESH) {
      unsigned long soundTimeUs = micros();
      long diff = (long)(soundTimeUs - lagLightTimeUs);
      lagSumDelay += diff;
      lagSampleCount++;
      Serial.print("[lag] Light->Sound delay: ");
      Serial.print(diff);
      Serial.println(" us");
      lagWaiting = false;
    } else if (micros() - lagLightTimeUs > LAG_TIMEOUT_US) {
      Serial.println("[lag] TIMEOUT: no sound detected within 500ms");
      lagWaiting = false;
    }
  }

  if (lagTestRunning && !lagTestDone) {
    if (millis() - lastBeatTime > LAG_SESSION_TIMEOUT_MS) {
      lagTestDone = true;
      Serial.println("[lag] --- Test finished ---");
      Serial.print("[lag] RESULT: SAMPLES=");
      Serial.println(lagSampleCount);
      if (lagSampleCount > 0) {
        float avgDelay = (float)lagSumDelay / lagSampleCount;
        Serial.print("[lag] AVERAGE Light->Sound delay = ");
        Serial.print(avgDelay);
        Serial.println(" us");
      } else {
        Serial.println("[lag] AVERAGE Light->Sound delay = N/A (no samples)");
      }
    }
  }
};

// ===================================================================
// setup()
// ===================================================================
void setup() {
  pinMode(PHOTO_PIN, INPUT);
  pinMode(LED_PIN,   OUTPUT);
  pinMode(DIN_H,     OUTPUT);
  pinMode(DIN_L,     OUTPUT);
  Serial.begin(115200);
  matrix.begin();
  Wire.begin();
  LED_Init();
  colorSensor.begin(&Wire);
  colorSensor.colorSensorConfigOneshot(
    AE_S13683_LED::colorSensorGain::GAIN_HIGH,
    2000  // 最小露光時間 175µs
  );
  colorSensor.ledDriverConfig(false);

  Serial.println("lag_measure3 ready (hackson_test7 + lag_measure2)");
};

// ===================================================================
// loop()
//   photoISR()は先頭で1回だけ呼び、beatDetectedをB_loop()より先に
//   参照して遅延計測の起点(lagLightTimeUs)を取る。
//   B_loop()側でbeatDetectedは通常どおり消費・リセットされる。
// ===================================================================
void loop() {
  photoISR();  // B.ino定義。A0を読み、立ち上がりエッジでbeatDetected=trueにする

  if (beatDetected && !lagWaiting) {
    if (!lagTestRunning) {
      lagTestRunning = true;
      Serial.println("[lag] Test started.");
    }
    lagLightTimeUs = micros();
    lagWaiting = true;
  }

  B_loop();  // beatDetectedを消費し、色判定・テンポ推定・演奏状態を更新
  C_loop();  // 演奏キューをSerialへ送信

  lagMeasure_loop();
};
