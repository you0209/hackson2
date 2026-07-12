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
  B_loop();
  C_loop();
};