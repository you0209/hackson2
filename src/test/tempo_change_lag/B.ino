// ===== 変数定義 =====
//カラーセンサ
const char* PLAY_COLOR_NAME = "Red";
int         colorIndex      = -1;

// フォトトランジスタ
bool  beatDetected = false;
int   detectCount  = 0;
bool  high         = false;
bool  stateHigh    = false;
float photoValue   = 0.0;

// EMA
float         estimatedBeatInterval = 1000.0f;
float         emaAlpha              = 0.2f;
unsigned long previousBeatTime      = 0;
const float   TEMPO_CHANGE_THRESH   = 10.0f;
float         previousNextBeatTime  = 0;

// テンポ変化からの復帰計測
int  exceedStreak    = 0;      // 連続でTEMPO_CHANGE_THRESHを超えた拍数
bool inExceedStreak  = false;  // 超過が継続中か
bool streakIsFaster  = false;  // 今回のストリークがBPM上昇方向か（開始時に確定）

// 状態変化
int         prebeatCount       = 0;
const int   PREBEAT_THRESH     = 4;
const int   MISSED_BEAT_THRESH = 3;

// ===== 関数 =====
// フォトトランジスタの割り込み（hackson_test7と同じ閾値・デバウンス）
void photoISR() {
  unsigned long now = millis();
  if (now - lastBeatTime >= 200) {
    int photoRaw = analogRead(PHOTO_PIN);
    photoValue = (float)photoRaw / 1023.0 * 100.0;
    if (photoValue >= 40.0) high = true;
    else high = false;
    if (high){
      if (!stateHigh) {
        beatDetected = true;
        currentBeatTime = millis();
        lastBeatTime = currentBeatTime;
        stateHigh = true;
      };
    }
    else stateHigh = false;
  };
};

// rgbより色を判定
const char* detectColor(uint16_t r, uint16_t g, uint16_t b, int& outIndex) {
  struct ColorRef { float r, g, b; const char* name; };

  static const ColorRef refs[] = {
    {0.9971f, 0.0761f, 0.0000f, "Red"},
    {0.0507f, 0.9692f, 0.2409f, "Green"},
    {0.0036f, 0.2950f, 0.9555f, "Blue"},
    {0.5271f, 0.8306f, 0.1798f, "Yellow"},
    {0.0344f, 0.7824f, 0.6219f, "Cyan"},
    {0.8517f, 0.2174f, 0.4769f, "Magenta"},
    {0.4008f, 0.7343f, 0.5482f, "White"},
  };
  const int refCount = sizeof(refs) / sizeof(refs[0]);

  float mag = sqrt((float)r*r + (float)g*g + (float)b*b);
  //if (mag < 100) return "Unknown";  // 暗すぎて判別不能

  float nr = r / mag, ng = g / mag, nb = b / mag;

  float bestScore = -1;
  const char* bestName = "Unknown";
  outIndex = -1;
  for (int i = 0; i < refCount; i++) {
    float score = nr*refs[i].r + ng*refs[i].g + nb*refs[i].b;
    if (score > bestScore) { bestScore = score; bestName = refs[i].name; outIndex = i; }
  };
  return bestName;
};

// カラーセンサの値を平滑化しcolorNameに色を代入
void readColor() {
  AE_S13683_LEDResult result = colorSensor.getColorSensorResultOneshot();
  colorName = detectColor(result.red, result.green, result.blue, colorIndex);
};

// フォトトランジスタの値より次の拍を予測
// hackson_test7と同じ計算・分岐に加え、TEMPO_CHANGE_THRESHの連続超過拍数を
// 数え、収まった時点で「何拍で通常状態に戻ったか」「BPMが上がったか下がったか」を出力する。
void updateTempoByPhoto() {
  if (previousBeatTime != 0) {
    float measuredBeatInterval = currentBeatTime - previousBeatTime;
    float tempoErrorRate       = abs(measuredBeatInterval - estimatedBeatInterval) / estimatedBeatInterval * 100.0f;
    // 拍間隔が短くなった=BPM上昇、長くなった=BPM低下
    bool  isFaster              = (measuredBeatInterval < estimatedBeatInterval);

    bool exceeded = (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH);

    if (exceeded) {
      if (!inExceedStreak) streakIsFaster = isFaster;  // ストリーク開始時の方向を記録
      exceedStreak++;
      inExceedStreak = true;
      Serial.print("[tempo] 変化検出中 ");
      Serial.print(isFaster ? "UP(速く)" : "DOWN(遅く)");
      Serial.print(" errorRate=");
      Serial.print(tempoErrorRate);
      Serial.print("%  BPM ");
      Serial.print(60000.0f / estimatedBeatInterval);
      Serial.print("->");
      Serial.print(60000.0f / measuredBeatInterval);
      Serial.print("  連続");
      Serial.print(exceedStreak);
      Serial.println("拍目");
      estimatedBeatInterval = measuredBeatInterval;
    } else {
      if (inExceedStreak) {
        Serial.print("=== テンポ変化(");
        Serial.print(streakIsFaster ? "BPM上昇" : "BPM低下");
        Serial.print(")から ");
        Serial.print(exceedStreak);
        Serial.println(" 拍で通常状態に戻りました ===");
        exceedStreak   = 0;
        inExceedStreak = false;
      }
      estimatedBeatInterval = emaAlpha * measuredBeatInterval + (1.0f - emaAlpha) * estimatedBeatInterval;
    }

    previousBeatTime     = currentBeatTime;
    previousNextBeatTime = nextBeatTime;
    nextBeatTime         = currentBeatTime + (unsigned long)estimatedBeatInterval;
  }
  else {
    previousBeatTime     = currentBeatTime;
    previousNextBeatTime = nextBeatTime;
    nextBeatTime         = currentBeatTime * 2;
  };
};

// 状態変化処理
void updateState() {
  switch (state) {
    case IDLE: {
      isPlaying = false;
      if (beatDetected && detectCount >= 2) {
        prebeatCount = 0;
        state        = PREBEAT;
      };
      break;
    };
    case PREBEAT: {
      if (prebeatCount >= PREBEAT_THRESH) {
        state = PLAYING;
      };
      break;
    };
    case PLAYING: {
      if (millis() - lastBeatTime > (unsigned long)(estimatedBeatInterval * MISSED_BEAT_THRESH)) {
        state = IDLE;
      };
      break;
    };
  };
};

// ===== テスト用関数 =====
// led用変数
struct Color {
  byte r, g, b;
  const char* name;
};

const Color COLORS[] = {
  {0xFF, 0x00, 0x00, "Red    "},
  {0x00, 0xFF, 0x00, "Green  "},
  {0x00, 0x00, 0xFF, "Blue   "},
  {0xFF, 0xFF, 0x00, "Yellow "},
  {0x00, 0xFF, 0xFF, "Cyan   "},
  {0xFF, 0x00, 0xFF, "Magenta"},
  {0xFF, 0xFF, 0xFF, "White  "},
};
const int           COLOR_COUNT       = sizeof(COLORS) / sizeof(COLORS[0]);
const unsigned long COLOR_INTERVAL    = 1000;
unsigned long       lastColorChange   = 0;
int                 currentColorIndex = 0;
bool                ledHigh           = false;

// led操作
void led() {
  unsigned long now = millis();
  if (!ledHigh) {
    if (now - lastColorChange >= COLOR_INTERVAL) {
      currentColorIndex = (currentColorIndex + 1) % COLOR_COUNT;
      setLEDColor(COLORS[currentColorIndex].r,
                  COLORS[currentColorIndex].g,
                  COLORS[currentColorIndex].b);
      lastColorChange = millis(); // ← setLEDColor後に取る
      ledHigh = true;
    };
  } else {
    if (now - lastColorChange >= 10) {
      setLEDColor(0x00, 0x00, 0x00);
      lastColorChange = millis(); // ← こっちも
      ledHigh = false;
    };
  };
};

// matrix用変数
unsigned long       beatFlashUntil = 0;
const unsigned long BEAT_FLASH_MS  = 150;

// matrix表示
void showMatrix() {
  uint8_t frame[8][12] = {};
  if (millis() < beatFlashUntil) frame[0][0] = 1;
  frame[state][1] = 1;
  if (!isPlaying) frame[0][2] = 1;
  else frame[1][2] = 1;
  if (colorIndex != -1) frame[colorIndex][3] = 1;
  matrix.renderBitmap(frame, 8, 12);
};

// ===== メインループ =====
// photoISR()はtempo_change_lag.ino側のloop()で呼ばれ、beatDetectedをセットする
void B_loop() {
  if (beatDetected) {
    beatFlashUntil  = millis() + BEAT_FLASH_MS;
    detectCount++;
    readColor();
    if (state == PREBEAT) prebeatCount++;
    if (state != IDLE) {
      if (!isPlaying && strcmp(colorName, PLAY_COLOR_NAME) == 0) isPlaying = true;
      updateTempoByPhoto();
      beatUpdated = true;
    };
  };
  updateState();
  showMatrix();
  beatDetected = false;
};
