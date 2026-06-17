// 赤外線センサ
volatile float irDistance = 0.0f;
const int IR_WINDOW_SIZE = 3;
float irDistanceBuffer[IR_WINDOW_SIZE] = {0.0f};
float calibratedIrValue = 0.0f;
float previousCalibratedIrValue = 0.0f;

// ピーク検出用
const int IR_PEAK_WINDOW_SIZE = 3;
float calibratedIrHistory[IR_PEAK_WINDOW_SIZE] = {0.0f};
const float IR_PEAK_THRESH = 60.0f;
const float IR_PEAK_MARGIN = 3.0f;

// フォトトランジスタ
int photoValue = 0;
float calibratedPhotoValue = 0.0f;
const float BEAT_DETECT_THRESH = 50.0f;
const float PERFORM_START_THRESH = 80.0f;
bool beatDetected = false;
bool photoHigh = false;

// キャリブレーション
unsigned long startCalibrateTime = 0;
const unsigned long CALIBRATE_THRESH = 5000;
float irMax = 0.0f;
float irMin = 9999.0f;
int photoMax = 0;
int photoMin = 9999;

// EMA
float estimatedBeatInterval = 1000.0f;
float emaAlpha = 0.2f;
unsigned long previousBeatTime = 0;
unsigned long predictedIrBeatTime = 0;
const float TEMPO_CHANGE_THRESH = 10.0f;
bool isTempoChanged = false;
float previousNextBeatTime = 0;

// 状態変化
int prebeatCount = 0;
const int PREBEAT_THRESH = 4;
int stableCount = 0;
const int STABLE_THRESH = 3;
const float STOP_THRESH = 5.0f;
int staticCount = 0;
const int FERMATA_THRESH = 3;
unsigned long lastBeatTime = 0;
const int MISSED_BEAT_THRESH = 3;
bool isCalibrated = false;

// その他
const float V_REF = 5.0f;
const int INSTRUMENT_ID = 0;


// フォトトランジスタの値を取得、キャリブレート
void readPhoto() {
  photoValue = analogRead(PHOTO_PIN);
  if (photoMax > photoMin) {
    calibratedPhotoValue = (float)(photoValue - photoMin) * 100.0f / (float)(photoMax - photoMin);
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
  if (!photoHigh && calibratedPhotoValue >= BEAT_DETECT_THRESH) {
    if (state != IDLE && state != CALIBRATE && state != PREBEAT && !isPlaying && calibratedPhotoValue >= PERFORM_START_THRESH) isPlaying = true;
    photoHigh = true;
    currentBeatTime = millis();
    lastBeatTime = currentBeatTime;
    beatDetected = true;
    if (state == PREBEAT) prebeatCount++;
  }
  else if (photoHigh && calibratedPhotoValue < BEAT_DETECT_THRESH - 20.0f) photoHigh = false;
};

// フォトトランジスタの値より次の拍を予測
void updateTempoByPhoto() {
  if (previousBeatTime != 0) {
    float measuredBeatInterval = currentBeatTime - previousBeatTime;
    float tempoErrorRate = abs(measuredBeatInterval - estimatedBeatInterval) / estimatedBeatInterval * 100.0f;
    if (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH) {
      isTempoChanged = true;
      estimatedBeatInterval = measuredBeatInterval;
    }
    else estimatedBeatInterval = emaAlpha * measuredBeatInterval + (1.0f - emaAlpha) * estimatedBeatInterval;
    previousBeatTime = currentBeatTime;
    previousNextBeatTime = nextBeatTime;
    nextBeatTime = currentBeatTime + (unsigned long)estimatedBeatInterval;
  }
  else {
    previousBeatTime = currentBeatTime;
    previousNextBeatTime = nextBeatTime;
    nextBeatTime = currentBeatTime * 2;
  };
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
  switch (state) {
    case IDLE: {
      isPlaying = false;
      if (abs(calibratedIrValue - previousCalibratedIrValue) > STOP_THRESH) {
        if (!isCalibrated) {
          irMax = 0.0f;
          irMin = 9999.0f;
          photoMax = 0;
          photoMin = 9999;
          startCalibrateTime = millis();
          state = CALIBRATE;
          Serial.print("CALIBRATE");
        }
        else {
          prebeatCount = 0;
          state = PREBEAT;
          Serial.print("PREBEAT");
        };
      };
      break;
    };
    case CALIBRATE: {
      calibrate();
      if (millis() - startCalibrateTime >= CALIBRATE_THRESH) {
        isCalibrated = true;
        prebeatCount = 0;
        state = PREBEAT;
        Serial.print("PREBEAT");
      };
      break;
    };
    case PREBEAT: {
      if (prebeatCount >= PREBEAT_THRESH) {
        prebeatCount = 0;
        state = PLAYING;
        Serial.print("PLAYING");
      };
      break;
    };
    case PLAYING: {
      if (detectFermata()) {
        state = FERMATA;
        Serial.print("FERMATA");
      }
      else if (isTempoChanged) {
        state = TEMPO_CHANGE;
        Serial.print("TEMPO_CHANGE");  
      }
      else if (millis() - lastBeatTime > (unsigned long)(estimatedBeatInterval * MISSED_BEAT_THRESH)) {
        state = IDLE;
        Serial.print("IDLE");
      };
      break;
    };
    case TEMPO_CHANGE: {
      if (isTempoChanged) stableCount = 0;
      else stableCount++;
      if (stableCount >= STABLE_THRESH) {
        stableCount = 0;
        state = PLAYING;
        Serial.print("PLAYING");
      };
      if (millis() - lastBeatTime > (unsigned long)(estimatedBeatInterval * MISSED_BEAT_THRESH)) {
        state = IDLE;
        Serial.print("IDLE");  
      };
      break;
    };
    case FERMATA: {
      if (abs(calibratedIrValue - previousCalibratedIrValue) > STOP_THRESH) {
        state = PLAYING;
        Serial.print("PLAYING");
      };
      break;
    };
  };
};

const int maxValue = 255;
const int minValue = 0;
int value = 170;
int stepValue = 1;
int stepCount = 1;
int step = 0;
unsigned long prevmillis = 0;
bool ledState = false;

void led() {
  if (state == CALIBRATE || state == PREBEAT) {
    unsigned long now = millis();
    if (now - prevmillis >= 1000 && ledState) {
      prevmillis = now;
      analogWrite(LED_PIN, 255);
      ledState = false;
    }
    else if (now - prevmillis >= 10 && !ledState) {
      analogWrite(LED_PIN, 0);
      ledState = true;
    };
  }
  else {
    unsigned long now = millis();
    if (now - prevmillis >= 1000) {
      step++;
      if (step % stepCount + 1 == stepCount && value + stepValue <= maxValue) value += stepValue;
      prevmillis = now;
      analogWrite(LED_PIN, 255);
      ledState = false;
    }
    else if (now - prevmillis >= 10 && !ledState) {
      analogWrite(LED_PIN, 0);
      ledState = true;
    };
  };
};

bool test = false;

void B_loop() {
  led();
  readPhoto();
  readIr();
  updateIrAverage();
  detectPhotoBeat();
  if (state != IDLE && state != CALIBRATE && state != PREBEAT && detectIrPeak() && !beatDetected) {
    predictBeatByIr();
    updateTempoByIr();
  };
  if (beatDetected) {
    unsigned long now = millis();
    if (isPlaying) {
      Serial.print("PLAYING");
    }
    else {
      Serial.print("BEAT");
    };
    isTempoChanged = false;
    updateTempoByPhoto();
    Serial.print("now: ");
    Serial.print(now);
    Serial.print(", ema: ");
    Serial.print(previousNextBeatTime);
    Serial.print(", diff: ");
    Serial.println(now - previousNextBeatTime);
    beatDetected = false;
    beatUpdated = true;
  };
  updateState();
};
