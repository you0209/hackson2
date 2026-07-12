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
bool          beatUpdated     = false; //nectBeatTime更新時にtrue
bool          isPlaying       = false; //演奏開始時true
bool          beatMissRecovered = false; //見逃した拍を検出しnoteIndex調整が必要な時true
const char*   colorName       = "Unknown";
enum          State           {IDLE, PREBEAT, PLAYING};
State         state           = IDLE;

// ───────────────────────────────────────────
// 子機間の共通トリガー同期
//   複数の子機のD4を同じ信号線に共通接続しておき、
//   その信号が最初にHIGHになった瞬間のmillis()を基準時刻とする。
//   以降の予測値(nextBeatTime)はこの基準時刻からの相対値で比較できる。
// ───────────────────────────────────────────
#define SYNC_PIN 4
bool          syncStarted = false;  // 基準時刻を確定済みか
unsigned long startTime   = 0;      // 共通トリガー検出時のmillis()

void setup() {
  pinMode(PHOTO_PIN, INPUT);
  pinMode(LED_PIN,   OUTPUT);
  pinMode(DIN_H,     OUTPUT);
  pinMode(DIN_L,     OUTPUT);
  pinMode(SYNC_PIN,  INPUT);
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
  if (!syncStarted && digitalRead(SYNC_PIN) == HIGH) {
    startTime   = millis();
    syncStarted = true;
    Serial.print("[sync] SYNC_PIN HIGH検出。startTime=");
    Serial.println(startTime);
  }
  B_loop();
  C_loop();
};