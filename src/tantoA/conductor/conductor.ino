/**
 * Arduino オーケストラ — 指揮者人形ユニット（担当 A）
 * Arduino Uno R4 WiFi 用ファームウェア
 *
 * 対応コマンド（Processing → Arduino, 115200bps, 改行終端）:
 *   START        : 予備拍 → 演奏中
 *   STOP         : 演奏停止 → 待機
 *   FERMATA      : フェルマータ開始（腕を短時間固定）
 *   BPM:xxx      : BPM 変更（40〜160 の整数）
 *   INSTRUMENT:n : 楽器キュー（n=1〜5、対応光量で1拍だけ LED 発光）
 */

#include <Servo.h>
#include "FspTimer.h"

// ───────────────────────────────────────────
// ピン定義
// ───────────────────────────────────────────
#define PIN_SERVO  9    // D9 (PWM) — サーボモータ
#define PIN_LED    6    // D6 (PWM) — 指揮用 LED（330Ω 経由）

// ───────────────────────────────────────────
// 状態定義
// ───────────────────────────────────────────
enum State {
  STATE_STANDBY,    // 待機
  STATE_PREBEAT,    // 予備拍（4 拍分）
  STATE_PLAYING,    // 演奏中
  STATE_FERMATA,    // フェルマータ
  STATE_STOP        // 演奏停止（腕を安全位置へ移動後、待機へ）
};

// ───────────────────────────────────────────
// 定数
// ───────────────────────────────────────────
const int BPM_MIN         = 60;
const int BPM_MAX         = 140;
const int BPM_DEFAULT     = 100;

const int SERVO_CENTER    = 30;   // 腕の中心角度 [deg]  (0°〜60° の中点)
const int SERVO_AMPLITUDE = 30;   // 腕の振れ幅   [deg] (±30° → 0°〜60°)
const int SERVO_SAFE      =  0;   // 停止・待機時の安全位置（起点 0°）
const int LED_ON_STEPS    =  2;   // LED 点灯を維持するステップ数（最前点の前後 ±1 ステップ）

// サーボ正弦波の分割ステップ数（1 拍 = SERVO_STEPS ステップ）
const int SERVO_STEPS      = 20;
const int SERVO_FRONT_STEP = SERVO_STEPS / 4;  // 最前点ステップ（sin 最大 = step 5）
const int INSTRUMENT_BRIGHTNESS[6] = { 0, 51, 102, 153, 204, 255 };
// インデックス 0 は未使用、1〜5 が楽器 A〜E に対応

// ───────────────────────────────────────────
// グローバル変数
// ───────────────────────────────────────────
Servo              servo;

FspTimer           beatTimer;
uint8_t            timerType = AGT_TIMER;
int8_t             timerCh   = -1;

volatile State     currentState   = STATE_STANDBY;
volatile int       currentBPM     = BPM_DEFAULT;
volatile int       servoStep      = 0;
volatile int       prebeatCount   = 0;
volatile bool      beatFlag       = false;

// 楽器キュー：0 = なし、1〜5 = 楽器 A〜E
volatile int       pendingCue     = 0;

String serialBuffer = "";

// ───────────────────────────────────────────
// 内部ヘルパー宣言
// ───────────────────────────────────────────
void setupTimer(int bpm);
void onBeatTimer(timer_callback_args_t *args);

// ===================================================================
// setup()
// ===================================================================
void setup() {
  Serial.begin(115200);

  // サーボ初期化
  servo.attach(PIN_SERVO);
  servo.write(SERVO_SAFE);

  // LED 初期化（PWM ピン）
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, 0);

  // タイマチャンネルを一度だけ確保して起動
  timerCh = FspTimer::get_available_timer(timerType);
  if (timerCh < 0) {
    timerType = GPT_TIMER;
    timerCh   = FspTimer::get_available_timer(timerType);
  }
  setupTimer(BPM_DEFAULT);

  Serial.println("READY");
}

// ===================================================================
// loop()
// ===================================================================
void loop() {
  // ── シリアル受信 ──
  String cmd = receiveCommand();
  if (cmd.length() > 0) {
    if (cmd == "START") {
      changeState(STATE_PREBEAT);
    } else if (cmd == "STOP") {
      changeState(STATE_STOP);
    } else if (cmd == "FERMATA") {
      if (currentState == STATE_PLAYING) {
        changeState(STATE_FERMATA);
      }
    } else if (cmd.startsWith("BPM:")) {
      int val = cmd.substring(4).toInt();
      setBPM(val);
    } else if (cmd.startsWith("INSTRUMENT:")) {
      int n = cmd.substring(11).toInt();
      if (n >= 1 && n <= 5 && currentState == STATE_PLAYING) {
        pendingCue = n;
      }
    }
  }

  // ── STOP 後処理：サーボを安全位置へ動かして待機へ ──
  if (currentState == STATE_STOP) {
    servo.write(SERVO_SAFE);
    delay(300);
    changeState(STATE_STANDBY);
  }
}

// ===================================================================
// onBeatTimer()  — タイマ割り込みコールバック
// ===================================================================
void onBeatTimer(timer_callback_args_t *args) {
  (void)args;

  switch (currentState) {
    case STATE_PLAYING:
    case STATE_PREBEAT:
      updateServo();
      updateLED();
      if (servoStep == 0) {          // 1 拍完了
        beatFlag = true;
        if (currentState == STATE_PREBEAT) {
          prebeatCount++;
          if (prebeatCount >= 4) {
            currentState = STATE_PLAYING;
            prebeatCount = 0;
          }
        }
      }
      break;

    case STATE_FERMATA:
      servo.write(SERVO_CENTER + SERVO_AMPLITUDE);
      analogWrite(PIN_LED, 0);
      break;

    case STATE_STANDBY:
    case STATE_STOP:
    default:
      servo.write(SERVO_SAFE);
      analogWrite(PIN_LED, 0);
      break;
  }
}

// ===================================================================
// updateServo()
// ===================================================================
void updateServo() {
  float phase = (2.0 * PI * servoStep) / SERVO_STEPS;
  int angle   = SERVO_CENTER + (int)(SERVO_AMPLITUDE * sin(phase));
  servo.write(angle);
  servoStep = (servoStep + 1) % SERVO_STEPS;
}

// ===================================================================
// updateLED()
//   最前点（SERVO_FRONT_STEP）のステップでのみ発光
//   servoStep はインクリメント後なので、SERVO_FRONT_STEP+1 と比較する
//   ・pendingCue があれば対応光量で点灯し消費
//   ・なければ通常フラッシュ（最大輝度）
// ===================================================================
void updateLED() {
  // updateServo() でインクリメント済みなので、
  // 実行前の servoStep == SERVO_FRONT_STEP のとき → 現在 servoStep == SERVO_FRONT_STEP+1
  bool atFront = (servoStep == SERVO_FRONT_STEP + 1);

  if (atFront) {
    int brightness = 255;
    if (pendingCue > 0) {
      brightness = INSTRUMENT_BRIGHTNESS[pendingCue];
      pendingCue = 0;
    }
    analogWrite(PIN_LED, brightness);
  } else {
    analogWrite(PIN_LED, 0);
  }
}

// ===================================================================
// receiveCommand()
// ===================================================================
String receiveCommand() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      String cmd = serialBuffer;
      serialBuffer = "";
      cmd.trim();
      if (cmd.length() > 0) return cmd;
    } else {
      serialBuffer += c;
    }
  }
  return "";
}

// ===================================================================
// changeState(State s)
// ===================================================================
void changeState(State s) {
  currentState = s;

  switch (s) {
    case STATE_STANDBY:
      servo.write(SERVO_SAFE);
      analogWrite(PIN_LED, 0);
      servoStep    = 0;
      prebeatCount = 0;
      pendingCue   = 0;
      break;

    case STATE_PREBEAT:
      servoStep    = 0;
      prebeatCount = 0;
      break;

    case STATE_PLAYING:
      servoStep = 0;
      break;

    case STATE_FERMATA:
      servo.write(SERVO_CENTER + SERVO_AMPLITUDE);
      analogWrite(PIN_LED, 0);
      break;

    case STATE_STOP:
      analogWrite(PIN_LED, 0);
      pendingCue = 0;
      break;
  }
}

// ===================================================================
// setBPM(int bpm)
// ===================================================================
void setBPM(int bpm) {
  if (bpm < BPM_MIN || bpm > BPM_MAX) {
    Serial.println("ERR:BPM_RANGE");
    return;
  }
  currentBPM = bpm;

  beatTimer.stop();
  beatTimer.end();
  setupTimer(bpm);

  Serial.print("BPM_OK:");
  Serial.println(bpm);
}

// ===================================================================
// setupTimer(int bpm)
// ===================================================================
void setupTimer(int bpm) {
  unsigned long periodUs = 60000000UL / (unsigned long)bpm / SERVO_STEPS;
  float freqHz = 1000000.0f / (float)periodUs;

  beatTimer.begin(TIMER_MODE_PERIODIC,
                  timerType,
                  timerCh,
                  freqHz,
                  0.0f,
                  onBeatTimer);

  beatTimer.setup_overflow_irq();
  beatTimer.open();
  beatTimer.start();
}
