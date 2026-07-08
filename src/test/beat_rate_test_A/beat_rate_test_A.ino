/**
 * 拍検出率テスト用 — 親機（担当A）側
 *
 * conductor.ino と同じサーボ＋LED動作を行い，
 * MAX_BEATS 拍打ったら停止して結果を出力する．
 *
 * 接続:
 *   D9  — サーボモータ（SG92R）
 *   D2  — OST4ML8132A DIN_H（10kΩ経由）
 *   D3  — OST4ML8132A DIN_L（10kΩ経由）
 *
 * 使い方:
 *   1. シリアルモニタを開く（115200bps）
 *   2. 子機 beat_rate_test_B を先に起動しておく
 *   3. このスケッチを書き込むと自動でテスト開始
 *   4. MAX_BEATS 拍後に「DONE: TOTAL=N」が出て停止
 */

#include <Servo.h>
#include "FspTimer.h"

#define PIN_SERVO   9
#define PIN_DIN_H   2
#define PIN_DIN_L   3

const int BPM          = 60;
const int MAX_BEATS    = 50;   // 何拍テストするか
const int SERVO_CENTER    = 30;
const int SERVO_AMPLITUDE = 30;
const int SERVO_SAFE      =  0;
const int SERVO_STEPS      = 20;
const int SERVO_FRONT_STEP = SERVO_STEPS / 4;  // = 5（最前点）

Servo servo;
FspTimer beatTimer;
uint8_t timerType = AGT_TIMER;
int8_t  timerCh   = -1;

volatile int  servoStep = 0;
volatile int  beatCount = 0;
volatile bool ledFlag   = false;
volatile bool done      = false;

// ── LED送信 ──────────────────────────────────────
void sendBit(bool bit) {
  if (bit) {
    digitalWrite(PIN_DIN_H, HIGH); digitalWrite(PIN_DIN_L, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);
  } else {
    digitalWrite(PIN_DIN_H, LOW);  digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_DIN_H, HIGH);
    delayMicroseconds(10);
  }
  delayMicroseconds(5);
}

void sendLED(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 7; i >= 0; i--) sendBit((b >> i) & 1);
  for (int i = 7; i >= 0; i--) sendBit((g >> i) & 1);
  for (int i = 7; i >= 0; i--) sendBit((r >> i) & 1);
  digitalWrite(PIN_DIN_H, LOW); digitalWrite(PIN_DIN_L, HIGH);
  delayMicroseconds(3000);
}

// ── タイマーISR ──────────────────────────────────
void onBeatTimer(timer_callback_args_t *args) {
  (void)args;
  if (done) return;

  float phase = (2.0 * PI * servoStep) / SERVO_STEPS;
  int angle   = SERVO_CENTER + (int)(SERVO_AMPLITUDE * sin(phase));
  servo.write(angle);
  servoStep = (servoStep + 1) % SERVO_STEPS;

  // conductor.ino と同じく最前点でLEDフラッシュ＆カウント
  if (servoStep == SERVO_FRONT_STEP + 1) {
    beatCount++;
    ledFlag = true;
    if (beatCount >= MAX_BEATS) done = true;
  }
}

void setupTimer(int bpm) {
  unsigned long periodUs = 60000000UL / (unsigned long)bpm / SERVO_STEPS;
  float freqHz = 1000000.0f / (float)periodUs;
  beatTimer.begin(TIMER_MODE_PERIODIC, timerType, timerCh, freqHz, 0.0f, onBeatTimer);
  beatTimer.setup_overflow_irq();
  beatTimer.open();
  beatTimer.start();
}

// ── setup / loop ─────────────────────────────────
void setup() {
  Serial.begin(115200);
  servo.attach(PIN_SERVO);
  servo.write(SERVO_SAFE);
  pinMode(PIN_DIN_H, OUTPUT);
  pinMode(PIN_DIN_L, OUTPUT);
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);
  sendLED(0, 0, 0);

  timerCh = FspTimer::get_available_timer(timerType);
  if (timerCh < 0) { timerType = GPT_TIMER; timerCh = FspTimer::get_available_timer(timerType); }
  setupTimer(BPM);

  Serial.print("beat_rate_test_A start: BPM=");
  Serial.print(BPM);
  Serial.print(" MAX_BEATS=");
  Serial.println(MAX_BEATS);
}

void loop() {
  if (ledFlag) {
    ledFlag = false;
    // 最前点で白フラッシュ（conductor.ino と同じタイミング）
    sendLED(255, 255, 255);
    delayMicroseconds(30000);  // 30ms 点灯
    sendLED(0, 0, 0);
    Serial.print("BEAT:");
    Serial.println(beatCount);
  }

  if (done) {
    beatTimer.stop();
    servo.write(SERVO_SAFE);
    sendLED(0, 0, 0);
    Serial.print("DONE: TOTAL=");
    Serial.println(beatCount);
    while (true);  // 停止
  }
}
