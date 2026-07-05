/**
 * Arduino オーケストラ — 指揮者人形ユニット（担当 A）
 * Arduino Uno R4 WiFi 用ファームウェア
 *
 * 対応コマンド（Processing → Arduino, 115200bps, 改行終端）:
 *   START        : 予備拍 → 演奏中
 *   STOP         : 演奏停止 → 待機
 *   FERMATA      : フェルマータ開始
 *   RESUME       : フェルマータ解除 → 演奏中
 *   BPM:xxx      : BPM 変更（60〜120 の整数）
 *   INSTRUMENT:n : 楽器キュー（n=1〜5、対応色で最前点1回だけ RGB LED 発光）
 *
 * Arduino → Processing 通知:
 *   STATE:STANDBY / STATE:PREBEAT / STATE:PLAYING / STATE:FERMATA / STATE:STOP
 *   BPM_OK:xxx
 *
 * 使用 LED: OST4ML8132A（8mm インテリジェント RGB LED）
 *   制御: DIN_H（D2）+ DIN_L（D3）の2線式、各10kΩ経由でLED DINへ合流
 *   データ順: B→G→R（各8bit、計24bit）
 *   タイミング: TH/TN/TL 各1〜100μs
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
const int BPM_MIN     = 60;
const int BPM_MAX     = 120;
const int BPM_DEFAULT = 60;

const int SERVO_CENTER    = 30;
const int SERVO_AMPLITUDE = 30;
const int SERVO_SAFE      =  0;

const int SERVO_STEPS      = 20;
const int SERVO_FRONT_STEP = SERVO_STEPS / 4;

// 楽器ごとの発光色 {R, G, B}（0〜255）
// インデックス0は未使用、1〜5が楽器A〜E

const uint8_t INSTRUMENT_COLOR[6][3] = {
  {  0,   0,   0},  // 0: 未使用
  {255,   0,   0},  // 1: ピアノ       (Red)
  {  0, 255,   0},  // 2: 鉄琴         (Green)
  {  0,   0, 255},  // 3: トランペット  (Blue)
  {255, 255,   0},  // 4: ドラム        (Yellow)
  {127,   0, 127},  // 5: シンバル      (Magenta)
};

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
volatile bool  stateChanged  = false;
volatile bool  ledUpdateFlag = false;
volatile uint8_t ledR = 0, ledG = 0, ledB = 0;

// カラーキュー（5色分）
uint8_t cueQueue[5]  = {0};
int     cueCount     = 0;    // セット済み数
int     cueIndex     = 0;    // 再生中の現在位置
bool    cueActive    = false; // START_CUE後にtrue
int     beatCount    = 0;    // START_CUE後の拍カウント

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
void setBPM(int bpm);

// ===================================================================
// setup()
// ===================================================================
void setup() {
  Serial.begin(115200);

  servo.attach(PIN_SERVO);
  servo.write(SERVO_SAFE);

  // OST4ML8132A ピン初期化
  // 待機状態：DIN_H=LOW, DIN_L=HIGH（データシート参考回路より）
  pinMode(PIN_DIN_H, OUTPUT);
  pinMode(PIN_DIN_L, OUTPUT);
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);

  // 消灯
  ledOff();

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
  // LED更新（ISRからのフラグで受け取り、loop()側で送信）
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
    } else if (cmd == "START_CUE") {
      if (currentState == STATE_PLAYING && cueCount > 0) {
        cueActive  = true;
        cueIndex   = 0;
        beatCount  = 7;  // 次の拍で即座に1色目を発光
        Serial.println("CUE_ACTIVE");
      }
    } else if (cmd.startsWith("INSTRUMENT:")) {
      int n = cmd.substring(11).toInt();
      if (n >= 1 && n <= 5) {
        if (cueCount < 5) {
          cueQueue[cueCount++] = n;
          Serial.print("CUE:");
          Serial.print(cueCount);
          Serial.println("/5");
        } else {
          Serial.println("ERR:CUE_FULL");
        }
      }
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
      ledR = 0; ledG = 0; ledB = 0;
      ledUpdateFlag = true;
      break;

    case STATE_STANDBY:
    case STATE_STOP:
    default:
      servo.write(SERVO_SAFE);
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
//   atBeat（最前点）でキュー更新・色決定
//   atFront（最前点±1ステップ）で点灯ウィンドウを広げる
//   実際の送信は loop() 側で行う
// ===================================================================
void updateLED() {
  bool atBeat  = (servoStep == SERVO_FRONT_STEP + 1);
  bool atFront = (servoStep >= SERVO_FRONT_STEP && servoStep <= SERVO_FRONT_STEP + 2);

  if (atBeat) {
    // 拍の瞬間：キュー更新と色決定
    if (cueActive) {
      beatCount++;
      if (beatCount % 8 == 0 && cueIndex < cueCount) {
        int cue = cueQueue[cueIndex++];
        ledR = INSTRUMENT_COLOR[cue][0];
        ledG = INSTRUMENT_COLOR[cue][1];
        ledB = INSTRUMENT_COLOR[cue][2];
        if (cueIndex >= cueCount) cueActive = false;  // 全色終了
      } else {
        ledR = 255; ledG = 255; ledB = 255;  // 白
      }
    } else {
      ledR = 255; ledG = 255; ledB = 255;  // 常時白フラッシュ
    }
  } else if (!atFront) {
    // ウィンドウ外：消灯
    ledR = 0; ledG = 0; ledB = 0;
  }
  // atFront && !atBeat: 前後ウィンドウ内 → ledR/G/B を維持して点灯継続
  ledUpdateFlag = true;
}

// ===================================================================
// sendLED()  — OST4ML8132A へ 24bit 送信（B→G→R順）
//   DIN_H/DIN_L を抵抗合成し DIN に VDD/VDD2/0V の3値を作る方式
//   1: VDD(High)をTH保持 → VDD/2(Mid)をTN保持
//   0: 0V(Low)をTL保持   → VDD/2(Mid)をTN保持
// ===================================================================
void sendLED(uint8_t r, uint8_t g, uint8_t b) {
  // B→G→R の順で送信
  for (int i = 7; i >= 0; i--) sendBit((b >> i) & 0x01);
  for (int i = 7; i >= 0; i--) sendBit((g >> i) & 0x01);
  for (int i = 7; i >= 0; i--) sendBit((r >> i) & 0x01);

  // ラッチ：DIN_H=LOW, DIN_L=HIGH を 3ms 以上保持
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);
  delayMicroseconds(3000);
}

// ===================================================================
// sendBit()  — 1ビット送信
// ===================================================================
void sendBit(bool bit) {
  // OST4ML8132A: DIN_H/DIN_Lを抵抗合成し、DINピンにVDD/VDD2/0Vの3値を作る方式
  //   HIGH+HIGH => VDD(High)   HIGH+LOW => VDD/2(Mid)   LOW+LOW => 0V(Low)
  if (bit) {
    // 1: VDD(High) を TH 保持 → VDD/2(Mid) を TN 保持
    digitalWrite(PIN_DIN_H, HIGH);
    digitalWrite(PIN_DIN_L, HIGH);
    delayMicroseconds(10);  // TH
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);  // TN
  } else {
    // 0: 0V(Low) を TL 保持 → VDD/2(Mid) を TN 保持
    digitalWrite(PIN_DIN_H, LOW);
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);  // TL
    digitalWrite(PIN_DIN_H, HIGH);
    delayMicroseconds(10);  // TN
  }
  // ここで必ず DIN_H=HIGH, DIN_L=LOW（Mid）で終わる → 次ビットの起点として一貫
  delayMicroseconds(5);
}

// ===================================================================
// ledOff()  — LED 消灯
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
      servoStep    = 0;
      prebeatCount = 0;
      cueCount     = 0;
      cueIndex     = 0;
      cueActive    = false;
      beatCount    = 0;
      memset(cueQueue, 0, sizeof(cueQueue));
      Serial.println("STATE:STANDBY");
      break;
    case STATE_PREBEAT:
      servoStep    = 0;
      prebeatCount = 0;
      cueIndex     = 0;
      cueActive    = false;
      beatCount    = 0;
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
      cueActive = false;
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
