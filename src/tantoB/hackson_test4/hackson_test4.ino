#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

VL53L0X sensor;

// 共通変数
#define PHOTO_PIN A1
#define LED_PIN 3
#define XSHUT_PIN 7
unsigned long nextBeatTime = 0; //次の拍の予測絶対時刻(milli)
unsigned long currentBeatTime = 0;
bool beatUpdated = false; //nectBeatTime更新時にtrue
bool isPlaying = false; //演奏開始時true
enum State {IDLE, CALIBRATE, PREBEAT, PLAYING, TEMPO_CHANGE, FERMATA};
State state = IDLE;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  // XSHUTでVL53L0Xをハードウェアリセット
  pinMode(XSHUT_PIN, OUTPUT);
  digitalWrite(XSHUT_PIN, LOW);
  delay(10);
  digitalWrite(XSHUT_PIN, HIGH);
  delay(10);

  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("VL53L0X init FAILED");
    while (1);  // 初期化失敗時はここで停止
  }
  Serial.println("VL53L0X init OK");
  sensor.startContinuous();
  pinMode(PHOTO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
};

void loop() {
  B_loop();
  C_loop();
};