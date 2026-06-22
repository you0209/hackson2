#include <Arduino.h>

// 共通変数
#define PHOTO_PIN A1
#define IR_PIN A2
unsigned long nextBeatTime = 0; //次の拍の予測絶対時刻(milli)
bool beatUpdated = false; //nectBeatTime更新時にtrue
bool isPlaying = false; //演奏開始時true
enum State {IDLE, CALIBRATE, PREBEAT, PLAYING, TEMPO_CHANGE, FERMATA};
State state = IDLE;
unsigned long currentBeatTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN, INPUT);
  pinMode(PHOTO_PIN, INPUT);
  pinMode(A0, OUTPUT);
};

void loop() {
  B_loop();
  C_loop();
};