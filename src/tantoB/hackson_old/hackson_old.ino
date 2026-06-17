// 赤外線センサ
volatile float irDistance = 0.0f; // 赤外線センサによる距離 [cm]
const int irWindowSize = 3;       // 移動平均のためのサンプル数
float irDistances[irWindowSize] = {0.0f}; // 距離バッファ（移動平均用）
float d_bar_n = 0.0f;    // 現在の距離移動平均（正規化済み） [%]
float d_bar_prev = 0.0f; // 前回の距離移動平均（正規化済み） [%]

// フォトトランジスタ
bool beatDetected = false; // 拍検出フラグ（割り込みでセット）

// キャリブレーション
float irMax = 0.0f;    // キャリブレーション中の距離最大値 [cm]
float irMin = 9999.0f; // キャリブレーション中の距離最小値 [cm]

// EMA テンポ推定
float T_n = 500.0f;         // 現在の拍間隔推定値 [us]
float alpha = 0.2f;         // EMA 係数
unsigned long t_beat = 0;   // 現在の拍の検出時刻 [us]
unsigned long t_prev = 0;   // 前回の拍の検出時刻 [us]
unsigned long t_next = 0;   // 次回の推定拍検出時刻 [us]
const float TEMPO_CHANGE_THRESH = 10.0f; // テンポ変化判定閾値 [%]
float tempoErrorRate = 0.0f; // テンポ誤差率 [%]
bool isTempoChanged = false; // テンポ変化判定フラグ
bool beatUpdated = false;    // 推定拍更新フラグ（担当Cと共有）

// 赤外線センサによるテンポ推定
unsigned long tPredictBeat = 0; // 赤外線センサによる次の拍の推定絶対時刻 [us]

// 状態機械
enum State {IDLE, PREBEAT, PLAYING, TEMPO_CHANGE, FERMATA};
State state = IDLE; // 現在の状態（担当Cと共有）

// 予備振り
int prebeatCount = 0;         // 予備振りカウント
const int PREBEAT_THRESH = 4; // 必要な予備振り回数

// テンポ変化からの通常演奏復帰
int stableCount = 0;         // 安定拍連続カウント
const int STABLE_THRESH = 3; // 復帰に必要な安定拍数

// 静止判定
const float STOP_THRESH = 5.0f; // 静止と判定される移動距離 [%]（要調整）

// フェルマータ判定
int staticCount = 0;          // 静止サンプル連続カウント
const int FERMATA_THRESH = 3; // フェルマータ判定に必要な静止サンプル連続数

// フォールバック
int missedBeats = 0;         // 連続未検出拍数
const int MISS_THRESH = 3;   // 演奏中止に必要な未検出拍数

// その他
const float vRef = 5.0f;     // 電源電圧 [V]
#define IR_PIN A0            // 赤外線センサのピン
#define PHOTO_PIN 2          // フォトトランジスタのピン

void photoISR() {
  t_beat = micros();
  beatDetected = true;
  if (state == PREBEAT) prebeatCount++;
};

void readIR() {
  int IR_raw = analogRead(IR_PIN);
  float IR_vol = IR_raw * (vRef / 1023.0);
  irDistance = 27.728 * pow(IR_vol, -1.2045);
  for (int i = irWindowSize-1; i > 0; i--) {
    irDistances[i] = irDistances[i-1];
  };
  irDistances[0] = irDistance;
};

void calibrate() {
  if (irDistance > irMax) irMax = irDistance;
  if (irDistance < irMin) irMin = irDistance;
};

void updateDBar() {
  float totalDistance = 0.0f;
  for (int i = 0; i < irWindowSize; i++) {
    totalDistance = totalDistance + irDistances[i];
  };
  d_bar_prev = d_bar_n;
  d_bar_n = totalDistance / (float)irWindowSize;
  d_bar_n = (d_bar_n - irMin) / (irMax - irMin) * 100.0;
};

void predictNextBeat() {};

void updateTempoByPhoto() {
  float delta_t_n = t_beat - t_prev;
  tempoErrorRate = abs(delta_t_n - T_n)/T_n * 100.0f;
  if (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH) {
    isTempoChanged = true;  
    T_n = delta_t_n;
  }
  else T_n = alpha * delta_t_n + ( 1 - alpha ) * T_n;
  t_prev = t_beat;
  t_next = t_beat + T_n;
};

void updateTempoByIR() {
  tempoErrorRate = abs((long)t_next - (long)tPredictBeat) / (float)T_n * 100.0f;
  if (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH) {
    isTempoChanged = true;
    T_n = tPredictBeat - t_beat;
    t_next = tPredictBeat;
  };
};

bool detectFermata() {
  unsigned long t_current = micros();
  if (t_current > t_next + T_n/4) {
    if (abs(d_bar_n - d_bar_prev) < STOP_THRESH) {
      staticCount++;
      if (staticCount >= FERMATA_THRESH) {
        staticCount = 0;
        return true;
      }
      else return false;
    };
  };
  staticCount = 0;
  return false;
};

void updateState() {
  switch (state) {
    case IDLE: {
      if (abs(d_bar_n - d_bar_prev) > STOP_THRESH) {
        irMax = 0.0f;
        irMin = 9999.0f;
        prebeatCount = 0;
        state = PREBEAT;
      };
      break;
    };
    case PREBEAT: {
      calibrate();
      if (prebeatCount >= PREBEAT_THRESH) {
        prebeatCount = 0;
        state = PLAYING;
      };
      break;
    };
    case PLAYING: {
      if (detectFermata()) {
        sendFermata();
        state = FERMATA;  
      }
      else if (isTempoChanged) state = TEMPO_CHANGE;
      else if (missedBeats >= MISS_THRESH) state = IDLE;
      break;
    };
    case TEMPO_CHANGE: {
      if (isTempoChanged) stableCount = 0;
      else stableCount++;
      if (stableCount >= STABLE_THRESH) {
        stableCount = 0;
        state = PLAYING;
      };
      break;
    };
    case FERMATA: {
      if (abs(d_bar_n - d_bar_prev) > STOP_THRESH) {
        sendResume();
        state = PLAYING;
      };
      break;
    };
  };
};

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN, INPUT);
  pinMode(PHOTO_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PHOTO_PIN), photoISR, RISING);
};

void loop() {
  readIR();
  updateDBar();
  if (beatDetected) {
    isTempoChanged = false;
    missedBeats = 0;
    updateTempoByPhoto();
    predictNextBeat();
    updateTempoByIR();
    beatDetected = false;
    beatUpdated = true;
  }
  else missedBeats++;
  updateState();
};
