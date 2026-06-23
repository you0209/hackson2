/**
 * Arduino オーケストラ — 指揮者人形ユニット（担当 A）
 * Arduino Uno R4 WiFi 用ファームウェア
 *
 * 対応コマンド（Processing → Arduino, 115200bps, 改行終端）:
 *   START        : 予備拍 → 演奏中
 *   STOP         : 演奏停止 → 待機
 *   FERMATA      : フェルマータ開始
 *   RESUME       : フェルマータ解除 → 演奏中
 *   BPM:xxx      : BPM 変更（40〜160 の整数）
 *   INSTRUMENT:n : 楽器キュー（n=1〜5、対応光量で最前点1回だけ LED 発光）
 *
 * Arduino → Processing 通知:
 *   STATE:STANDBY / STATE:PREBEAT / STATE:PLAYING / STATE:FERMATA / STATE:STOP
 *   BPM_OK:xxx
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
  STATE_STANDBY,
  STATE_PREBEAT,
  STATE_PLAYING,
  STATE_FERMATA,
  STATE_STOP
};

// ───────────────────────────────────────────
// 定数
// ───────────────────────────────────────────
const int BPM_MIN         = 60;
const int BPM_MAX         = 120;
const int BPM_DEFAULT     = 60;

const int SERVO_CENTER    = 30;
const int SERVO_AMPLITUDE = 30;
const int SERVO_SAFE      =  0;

const int SERVO_STEPS      = 20;
const int SERVO_FRONT_STEP = SERVO_STEPS / 4;

const int INSTRUMENT_BRIGHTNESS[6] = { 0, 51, 102, 153, 204, 255 };

// ───────────────────────────────────────────
// グローバル変数
// ───────────────────────────────────────────
Servo   servo;

FspTimer beatTimer;
uint8_t  timerType = AGT_TIMER;
int8_t   timerCh   = -1;

volatile State currentState  = STATE_STANDBY;
volatile int   currentBPM    = BPM_DEFAULT;
volatile int   servoStep     = 0;
volatile int   prebeatCount  = 0;
volatile bool  beatFlag      = false;
volatile bool  stateChanged  = false;  // 予備拍→演奏中の遷移を loop() へ通知

volatile int   pendingCue    = 0;

String serialBuffer = "";

// ───────────────────────────────────────────
// 関数宣言
// ───────────────────────────────────────────
void setupTimer(int bpm);
void onBeatTimer(timer_callback_args_t *args);
void updateServo();
void updateLED();
String receiveCommand();
void changeState(State s);
void setBPM(int bpm);

// ===================================================================
// setup()
// ===================================================================
void setup() {
  Serial.begin(115200);

  servo.attach(PIN_SERVO);
  servo.write(SERVO_SAFE);

  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, 0);

  timerCh = FspTimer::get_available_timer(timerType);
  if (timerCh < 0) {
    timerType = GPT_TIMER;
    timerCh   = FspTimer::get_available_timer(timerType);
  }
  setupTimer(BPM_DEFAULT);

  Serial.println("READY");
  Serial.println("STATE:STANDBY");
}

// ===================================================================
// loop()
// ===================================================================
void loop() {
  // シリアル受信
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
    } else if (cmd == "RESUME") {
      if (currentState == STATE_FERMATA) {
        changeState(STATE_PLAYING);
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

  // STOP 後処理
  if (currentState == STATE_STOP) {
    servo.write(SERVO_SAFE);
    changeState(STATE_STANDBY);
  }

  // 予備拍→演奏中の遷移通知（ISR内では Serial 送信できないため）
  if (stateChanged) {
    stateChanged = false;
    Serial.println("STATE:PLAYING");
  }
}

// ===================================================================
// onBeatTimer()
// ===================================================================
void onBeatTimer(timer_callback_args_t *args) {
  (void)args;

  switch (currentState) {
    case STATE_PLAYING:
    case STATE_PREBEAT:
      updateServo();
      updateLED();
      if (servoStep == 0) {
        beatFlag = true;
        if (currentState == STATE_PREBEAT) {
          prebeatCount++;
          if (prebeatCount >= 4) {
            currentState = STATE_PLAYING;
            prebeatCount = 0;
            stateChanged = true;
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
// ===================================================================
void updateLED() {
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
// changeState()
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
      Serial.println("STATE:STANDBY");
      break;
    case STATE_PREBEAT:
      servoStep    = 0;
      prebeatCount = 0;
      Serial.println("STATE:PREBEAT");
      break;
    case STATE_PLAYING:
      servoStep = 0;
      Serial.println("STATE:PLAYING");
      break;
    case STATE_FERMATA:
      servo.write(SERVO_CENTER + SERVO_AMPLITUDE);
      analogWrite(PIN_LED, 0);
      Serial.println("STATE:FERMATA");
      break;
    case STATE_STOP:
      analogWrite(PIN_LED, 0);
      pendingCue = 0;
      Serial.println("STATE:STOP");
      break;
  }
}

// ===================================================================
// setBPM()
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
// setupTimer()
// ===================================================================
void setupTimer(int bpm) {
  unsigned long periodUs = 60000000UL / (unsigned long)bpm / SERVO_STEPS;
  float freqHz = 1000000.0f / (float)periodUs;

  beatTimer.begin(TIMER_MODE_PERIODIC,
                  timerType, timerCh,
                  freqHz, 0.0f,
                  onBeatTimer);
  beatTimer.setup_overflow_irq();
  beatTimer.open();
  beatTimer.start();
}
