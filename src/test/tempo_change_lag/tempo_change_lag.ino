/**
 * hackson_test7 実演版 + テンポ変化 復帰計測
 *
 * hackson_test7（拍検出→色判定→テンポ推定→メロディ演奏キュー送信）を
 * 実機のフォトトランジスタで動かせる状態にし（simulateBeat()は使わず
 * 実際にA0を読むphotoISR()を使用、readColor()呼び出しも追加）、
 * テンポ急変時にTEMPO_CHANGE_THRESHを連続で超えた拍数を数えて
 * 収まったタイミングで通常状態への復帰拍数をシリアルへ出力する．
 *
 * 接続:
 *   A0        — フォトトランジスタ（親機LEDの光を検出）
 *   I2C(SDA/SCL) — カラーセンサ AE_S13683_LED
 *   D5        — 子機側ステータスLED
 *   D2/D3     — OST4ML5B32A DIN_H/DIN_L
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. 一定テンポで数拍光らせてPLAYING状態にする（PREBEAT_THRESH拍の予備拍後）
 *   3. 急にテンポを変える
 *   4. 変化直後の拍から [tempo] 変化検出中 ... が連続で出力され，
 *      収まった時点で「=== テンポ変化から N 拍で通常状態に戻りました ===」
 *      と出力される
 *
 * 注意:
 *   フォトトランジスタの閾値・デバウンスは hackson_test7 のB.ino
 *   （40%・200ms）をそのまま引き継いでいる。他のtestで使っている
 *   10%基準に揃えたい場合は B.ino の photoISR() 内 40.0 を書き換えること。
 *
 *   C_melody.ino の演奏キューは Serial.write() で下流（音源側）に
 *   バイナリ送信される。本計測のログ（テキスト）も同じシリアルポートに
 *   出力されるため、下流機器側のパース処理によっては混線する可能性が
 *   ある（hackson_test7オリジナルのデバッグprintと同様の制約）。
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

  Serial.println("tempo_change_lag ready (hackson_test7 実演版)");
};

// ===================================================================
// loop()
// ===================================================================
void loop() {
  photoISR();  // B.ino定義。A0を読み、立ち上がりエッジでbeatDetected=trueにする
  B_loop();    // beatDetectedを消費し、色判定・テンポ推定(+テンポ変化計測)・演奏状態を更新
  C_loop();    // 演奏キューをSerialへ送信
};
