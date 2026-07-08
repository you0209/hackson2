/**
 * Arduino オーケストラ — 指揮者人形ユニット（担当 A）スタンドアロン版
 * Arduino Uno R4 WiFi 用ファームウェア
 * Processing 不要。シリアルターミナルから操作可能。
 *
 * 対応コマンド（115200bps, 改行終端）:
 *   START        : 予備拍 → 演奏中
 *   STOP         : 演奏停止 → 待機
 *   FERMATA      : フェルマータ開始
 *   RESUME       : フェルマータ解除 → 演奏中
 *   INSTRUMENT:n : 次の最前点で楽器色をワンショット発光（n=1〜5）
 *
 * LED パターン:
 *   - 通常拍: White フラッシュ
 *   - 4拍ごと: Cyan フラッシュ（小節カウント用）
 *   - INSTRUMENT:n 受信後の次の最前点: 楽器色でワンショット発光
 *
 * 使用 LED: OST4ML8132A（8mm インテリジェント RGB LED）
 *   制御: DIN_H（D2）+ DIN_L（D3）の2線式、各10kΩ経由でLED DINへ合流
 */

#include <Servo.h>
#include "FspTimer.h"

// ───────────────────────────────────────────
// ピン定義
// ───────────────────────────────────────────
#define PIN_SERVO   9   // D9  — サーボモータ
#define PIN_DIN_H   2   // D2  — OST4ML8132A DIN_H（10kΩ経由）
#define PIN_DIN_L   3   // D3  — OST4ML8132A DIN_L（10kΩ経由）

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
const int BPM_DEFAULT = 60;

const int SERVO_CENTER    = 30;
const int SERVO_AMPLITUDE = 30;
const int SERVO_SAFE      =  0;

const int SERVO_STEPS      = 20;
const int SERVO_FRONT_STEP = SERVO_STEPS / 4;

// 楽器ごとの発光色 {R, G, B}（0〜255）
const uint8_t INSTRUMENT_COLOR[6][3] = {
  {  0,   0,   0},  // 0: 未使用
  {255,   0,   0},  // 1: Red
  {  0, 255,   0},  // 2: Green
  {  0,   0, 255},  // 3: Blue
  {255, 255,   0},  // 4: Yellow
  {127,   0, 127},  // 5: Magenta
};

// 4拍ごとのキューフラッシュ色（小節カウント用）
const uint8_t COLOR_CYAN[3] = {0, 255, 255};

// ───────────────────────────────────────────
// グローバル変数
// ───────────────────────────────────────────
Servo   servo;

FspTimer beatTimer;
uint8_t  timerType = AGT_TIMER;
int8_t   timerCh   = -1;

volatile State currentState  = STATE_STANDBY;
volatile int   servoStep     = 0;
volatile int   prebeatCount  = 0;
volatile bool  stateChanged  = false;
volatile bool  ledUpdateFlag = false;
volatile uint8_t ledR = 0, ledG = 0, ledB = 0;

int  beatCount      = 0;
int  autoColorIndex = 1;  // 自動サイクル色インデックス（1〜5）

String serialBuffer = "";

// ───────────────────────────────────────────
// 関数宣言
// ───────────────────────────────────────────
void setupTimer(int bpm);
void onBeatTimer(timer_callback_args_t *args);
void updateServo();
void updateLED();
void sendLED(uint8_t r, uint8_t g, uint8_t b);
void sendBit(bool bit);
void ledOff();
String receiveCommand();
void changeState(State s);

// ===================================================================
// setup()
// ===================================================================
void setup() {
  Serial.begin(115200);

  servo.attach(PIN_SERVO);
  servo.write(SERVO_SAFE);

  pinMode(PIN_DIN_H, OUTPUT);
  pinMode(PIN_DIN_L, OUTPUT);
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);

  ledOff();

  timerCh = FspTimer::get_available_timer(timerType);
  if (timerCh < 0) {
    timerType = GPT_TIMER;
    timerCh   = FspTimer::get_available_timer(timerType);
  }
  setupTimer(BPM_DEFAULT);

  Serial.println("READY");
  changeState(STATE_PREBEAT);  // 電源ON後に自動スタート
}

// ===================================================================
// loop()
// ===================================================================
void loop() {
  if (ledUpdateFlag) {
    ledUpdateFlag = false;
    sendLED(ledR, ledG, ledB);
  }

  String cmd = receiveCommand();
  if (cmd.length() > 0) {
    if (cmd == "START") {
      changeState(STATE_PREBEAT);
    } else if (cmd == "STOP") {
      changeState(STATE_STOP);
    } else if (cmd == "FERMATA") {
      if (currentState == STATE_PLAYING) changeState(STATE_FERMATA);
    } else if (cmd == "RESUME") {
      if (currentState == STATE_FERMATA) changeState(STATE_PLAYING);
    }
  }

  if (currentState == STATE_STOP) {
    servo.write(SERVO_SAFE);
    changeState(STATE_STANDBY);
  }

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
      // servo.write() は changeState() で一度だけ送る。毎ステップ再送するとストール時に焼損するため送らない
      ledR = 0; ledG = 0; ledB = 0;
      ledUpdateFlag = true;
      break;

    case STATE_STANDBY:
    case STATE_STOP:
    default:
      // 同様に servo.write() は changeState() に任せる
      ledR = 0; ledG = 0; ledB = 0;
      ledUpdateFlag = true;
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
//   最前点でフラッシュ。優先度: 楽器色（8拍ごと自動サイクル）> Cyan（4拍ごと）> White
// ===================================================================
void updateLED() {
  bool atFront = (servoStep == SERVO_FRONT_STEP + 1);
  if (atFront) {
    beatCount++;

    if (beatCount == 8) {
      // 最初の8拍目のみ: Red
      ledR = INSTRUMENT_COLOR[1][0];
      ledG = INSTRUMENT_COLOR[1][1];
      ledB = INSTRUMENT_COLOR[1][2];
    } else if (beatCount % 4 == 0) {
      // 4拍ごと: Cyan
      ledR = COLOR_CYAN[0]; ledG = COLOR_CYAN[1]; ledB = COLOR_CYAN[2];
    } else {
      // その他: White
      ledR = 255; ledG = 255; ledB = 255;
    }
  } else {
    ledR = 0; ledG = 0; ledB = 0;
  }
  ledUpdateFlag = true;
}

// ===================================================================
// sendLED()
// ===================================================================
void sendLED(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 7; i >= 0; i--) sendBit((b >> i) & 0x01);
  for (int i = 7; i >= 0; i--) sendBit((g >> i) & 0x01);
  for (int i = 7; i >= 0; i--) sendBit((r >> i) & 0x01);
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);
  delayMicroseconds(3000);
}

// ===================================================================
// sendBit()
// ===================================================================
void sendBit(bool bit) {
  if (bit) {
    digitalWrite(PIN_DIN_H, HIGH);
    digitalWrite(PIN_DIN_L, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);
  } else {
    digitalWrite(PIN_DIN_H, LOW);
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_DIN_H, HIGH);
    delayMicroseconds(10);
  }
  delayMicroseconds(5);
}

// ===================================================================
// ledOff()
// ===================================================================
void ledOff() {
  sendLED(0, 0, 0);
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
      ledOff();
      servoStep       = 0;
      prebeatCount    = 0;
      beatCount       = 0;
      autoColorIndex  = 1;
      Serial.println("STATE:STANDBY");
      break;
    case STATE_PREBEAT:
      servoStep       = 0;
      prebeatCount    = 0;
      beatCount       = 0;
      autoColorIndex  = 1;
      Serial.println("STATE:PREBEAT");
      break;
    case STATE_PLAYING:
      servoStep = 0;
      Serial.println("STATE:PLAYING");
      break;
    case STATE_FERMATA:
      servo.write(SERVO_CENTER + SERVO_AMPLITUDE);
      ledOff();
      Serial.println("STATE:FERMATA");
      break;
    case STATE_STOP:
      ledOff();
      Serial.println("STATE:STOP");
      break;
  }
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
