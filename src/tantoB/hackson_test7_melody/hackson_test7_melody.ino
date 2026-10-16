#include <Arduino.h>
#include <Wire.h>
#include "Arduino_LED_Matrix.h"
#include "AE_S13683_LED.h"

ArduinoLEDMatrix matrix;      // デバッグ表示用（本体基板のLEDマトリクス）
AE_S13683_LED    colorSensor; // カラーセンサドライバ（演奏開始キュー判定に使用）

// 共通変数
#define PHOTO_PIN A0  // フォトトランジスタ入力（指揮者人形のLED点滅＝拍を検出）
#define LED_PIN   5   // デバッグ用LED出力（本番の演奏動作には関与しない）
#define DIN_H     2   // 自機ステータスLED制御用（led()内のsetLEDColorが使用）
#define DIN_L     3   // 同上

unsigned long nextBeatTime    = 0;         // 次の拍が来ると予測される絶対時刻(ms)。担当Cが楽譜進行の送信タイミング決定に使用
unsigned long currentBeatTime = 0;         // 直近に検出した拍の絶対時刻(ms)
unsigned long lastBeatTime    = 0;         // 直近に「拍あり」と判定した時刻(ms)。photoISR()のデバウンス判定と、見逃し検出（updateState()）の基準にも使用
bool          beatUpdated     = false;     // nextBeatTime更新時にtrue。担当C側がこのフラグを見て楽譜を進行させる
bool          isPlaying       = false;     // 自パートの演奏が開始したらtrue（PLAY_COLOR_NAMEの色を検出した時点でtrueになる）
const char*   colorName       = "Unknown"; // 直近にカラーセンサで判定した色名
enum          State           {IDLE, PREBEAT, PLAYING}; // 状態機械：停止／予備振り／通常演奏の3状態（詳細はB.inoのupdateState()）
State         state           = IDLE;

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
    10000  // 最小露光時間 175µs
  );
  colorSensor.ledDriverConfig(false);
};

void loop() {
  B_loop(); // 拍検出・色判別・テンポ推定・状態管理（担当B）
  C_loop(); // 楽譜進行・シリアル送信（担当C）
};
