// 赤外線センサ
#define IR_PIN A0
volatile float irDistance = 0.0f;
const int IR_WINDOW_SIZE = 3;
float irDistanceBuffer[IR_WINDOW_SIZE] = {0.0f};
float calibratedIrValue = 0.0f;
float previousCalibratedIrValue = 0.0f;

// ピーク検出用
const int IR_PEAK_WINDOW_SIZE = 3;
float calibratedIrHistory[IR_PEAK_WINDOW_SIZE] = {0.0f};
const float IR_PEAK_THRESH = 10.0f;
const float IR_PEAK_MARGIN = 3.0f;

// フォトトランジスタ
#define PHOTO_PIN A1
int photoValue = 0;
float calibratedPhotoValue = 0.0f;
const float BEAT_DETECT_THRESH = 50.0f;
const float PERFORM_START_THRESH = 85.0f;
bool beatDetected = false;
bool photoHigh = false;

// キャリブレーション
float irMax = 0.0f;
float irMin = 9999.0f;
int photoMax = 0;
int photoMin = 9999;

// EMA
float estimatedBeatInterval = 1000.0f; // [us]
float emaAlpha = 0.2f;
unsigned long currentBeatTime = 0;
unsigned long previousBeatTime = 0;
unsigned long nextBeatTime = 0;
unsigned long predictedIrBeatTime = 0;
const float TEMPO_CHANGE_THRESH = 10.0f;
bool isTempoChanged = false;
bool beatUpdated = false;

// 状態変化
enum State {IDLE, PREBEAT, PLAYING, TEMPO_CHANGE, FERMATA};
State state = IDLE;
int prebeatCount = 0;
const int PREBEAT_THRESH = 4;
int stableCount = 0;
const int STABLE_THRESH = 3;
const float STOP_THRESH = 5.0f;
int staticCount = 0;
const int FERMATA_THRESH = 3;
unsigned long lastBeatTime = 0;       // 最後に拍を検出した時刻 [us]
const int MISSED_BEAT_THRESH = 3;     // 演奏中止に必要な未検出拍数
bool isPlaying = false;

// その他
const float V_REF = 5.0f;
const int INSTRUMENT_ID = 0;

unsigned long currentTimes[2] = {0};

unsigned long startCalibrate = 0;

bool nextBeatPrinted = false;

unsigned long prevNextBeatTime = 0;

// フォトトランジスタの値を取得、キャリブレート
void readPhoto() {
  //Serial.println("function: readPhoto");
  photoValue = analogRead(PHOTO_PIN);
  //Serial.println(photoValue);
  //Serial.print("photoValue: ");
  //Serial.println(photoValue);
  if (photoMax > photoMin) {
    calibratedPhotoValue = (float)(photoMax - photoValue) / (float)(photoMax - photoMin) * 100.0f;
    //Serial.print("calibratedPhotoValue: ");
    //Serial.println(calibratePhotovalue);
  };
};

// 赤外線センサの値を取得
void readIr() {
  int irRaw = analogRead(IR_PIN);
  float irVoltage = irRaw * (V_REF / 1023.0f);
  irDistance = 27.728f * pow(irVoltage, -1.2045f);

  for (int i = IR_WINDOW_SIZE - 1; i > 0; i--) irDistanceBuffer[i] = irDistanceBuffer[i - 1];
  irDistanceBuffer[0] = irDistance;
};

// キャリブレーション時の処理
void calibrate() {
  if (irDistance > irMax) irMax = irDistance;
  if (irDistance < irMin) irMin = irDistance;

  if (photoValue > photoMax) photoMax = photoValue;
  if (photoValue < photoMin) photoMin = photoValue;
};

// 取得距離を平滑化、キャリブレート
void updateIrAverage() {
  float total = 0.0f;
  for (int i = 0; i < IR_WINDOW_SIZE; i++) total += irDistanceBuffer[i];
  previousCalibratedIrValue = calibratedIrValue;
  calibratedIrValue = total / (float)IR_WINDOW_SIZE;
  if (irMax > irMin) {
    calibratedIrValue = (calibratedIrValue - irMin) / (irMax - irMin) * 100.0f;
  };
  for (int i = IR_PEAK_WINDOW_SIZE - 1; i > 0; i--) calibratedIrHistory[i] = calibratedIrHistory[i - 1];
  calibratedIrHistory[0] = calibratedIrValue;
};

// 距離の山を検知
bool detectIrPeak() {
  float current = calibratedIrHistory[0];
  float center = calibratedIrHistory[1];
  float previous = calibratedIrHistory[2];
  return (
    center > IR_PEAK_THRESH &&
    (center - current) > IR_PEAK_MARGIN &&
    (center - previous) > IR_PEAK_MARGIN
  );
};

// 加速度より振り下ろし時刻を予測
void predictBeatByIr() {};

// フォトトランジスタの値より拍を判定
void detectPhotoBeat() {
  //Serial.println("function: detectPhotoBeat");
  if (!photoHigh && calibratedPhotoValue <= BEAT_DETECT_THRESH) {
    photoHigh = true;
    currentBeatTime = millis();
    lastBeatTime = currentBeatTime;
    beatDetected = true;
    if (state == PREBEAT) prebeatCount++;
  }
  else if (photoHigh && calibratedPhotoValue > BEAT_DETECT_THRESH + 20.0f) photoHigh = false;
  if (state != IDLE && state != PREBEAT && !isPlaying && calibratedPhotoValue >= PERFORM_START_THRESH) isPlaying = true;
};

// フォトトランジスタの値より次の拍を予測
void updateTempoByPhoto() {
  //Serial.println("function: updateTempoByPhoto");
  if (previousBeatTime != 0) {
    float measuredBeatInterval = currentBeatTime - previousBeatTime;
    float tempoErrorRate = abs(measuredBeatInterval - estimatedBeatInterval) / estimatedBeatInterval * 100.0f;
    if (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH) {
      isTempoChanged = true;
      estimatedBeatInterval = measuredBeatInterval;
    }
    else estimatedBeatInterval = emaAlpha * measuredBeatInterval + (1.0f - emaAlpha) * estimatedBeatInterval;
    previousBeatTime = currentBeatTime;
    nextBeatTime = currentBeatTime + (unsigned long)estimatedBeatInterval;
  }
  else {
    previousBeatTime = currentBeatTime;
    nextBeatTime = currentBeatTime * 2;
  };

  Serial.print("estimatedBeatInterval: ");
  Serial.print(estimatedBeatInterval);
  Serial.print(", time: ");
  Serial.println(millis());
};

// 赤外線センサの値より現在の拍を予測
void updateTempoByIr() {
  float predictionError = abs((long)nextBeatTime - (long)predictedIrBeatTime) / estimatedBeatInterval * 100.0f;
  if (state != PREBEAT && predictionError > TEMPO_CHANGE_THRESH) {
    isTempoChanged = true;
    estimatedBeatInterval = predictedIrBeatTime - currentBeatTime;
    nextBeatTime = predictedIrBeatTime;
  };
};

// フェルマータを判定
bool detectFermata() {
  unsigned long currentTime = millis();
  if (currentTime > nextBeatTime + (unsigned long)(estimatedBeatInterval / 4)) {
    if (abs(calibratedIrValue - previousCalibratedIrValue) < STOP_THRESH) {
      staticCount++;
      if (staticCount >= FERMATA_THRESH) {
        staticCount = 0;
        return true;
      };
    }
    else staticCount = 0;
  };
  return false;
};

// 状態変化処理
void updateState() {
  //Serial.println("function: updateState");
  switch (state) {
    case IDLE: {
      calibrate();
      isPlaying = false;
      if (millis() - startCalibrate >= 5000) {
        prebeatCount = 0;
        state = PREBEAT;
        Serial.print("photoMin: ");
        Serial.print(photoMin);
        Serial.print(", photoMax: ");
        Serial.println(photoMax);
        Serial.println("-> PREBEAT");
      };
      break;
    };
    case PREBEAT: {
      if (prebeatCount >= PREBEAT_THRESH) {
        prebeatCount = 0;
        state = PLAYING;
        Serial.println("-> PLAYING");
      };
      break;
    };
    case PLAYING: {
      if (isTempoChanged) {
        state = TEMPO_CHANGE;
        Serial.println("-> TEMPO_CHANGE");
      }
      else if (millis() - lastBeatTime > (unsigned long)(estimatedBeatInterval * MISSED_BEAT_THRESH)) {
        state = IDLE;
        startCalibrate = millis();
        Serial.println("-> IDLE");
      };
      break;
    };
    case TEMPO_CHANGE: {
      if (isTempoChanged) stableCount = 0;
      else stableCount++;
      if (stableCount >= STABLE_THRESH) {
        stableCount = 0;
        state = PLAYING;
        Serial.println("-> PLAYING");
      };
      if (millis() - lastBeatTime > (unsigned long)(estimatedBeatInterval * MISSED_BEAT_THRESH)) {
        state = IDLE;
        startCalibrate = millis();
        Serial.println("-> IDLE");
      };
      break;
    };
    case FERMATA: {
      if (abs(calibratedIrValue - previousCalibratedIrValue) > STOP_THRESH) {
        state = PLAYING;
        Serial.println("-> PLAYING");
      };
      break;
    };
  };
};

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN, INPUT);
  pinMode(PHOTO_PIN, INPUT);
  startCalibrate = millis();
  Serial.println("-> IDLE");
};

void loop() {
  if (millis() >= prevNextBeatTime && !nextBeatPrinted && prevNextBeatTime != 0) {
    Serial.print("nextBeatTime: ");
    Serial.print(prevNextBeatTime);
    Serial.print(", now: ");
    Serial.println(millis());
    nextBeatPrinted = true;
  };

  readPhoto();
  detectPhotoBeat();
  if (state != IDLE && beatDetected) {
    isTempoChanged = false;
    prevNextBeatTime = nextBeatTime;  // 更新前の値を保存
    updateTempoByPhoto();             // ここでnextBeatTimeが更新される
    beatDetected = false;
    beatUpdated = true;
    nextBeatPrinted = false;
  };
  updateState();
};
