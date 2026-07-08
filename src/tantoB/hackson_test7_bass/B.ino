// ===== 変数定義 =====
//カラーセンサ
const char* PLAY_COLOR_NAME = "Blue";
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

// 状態変化
int         prebeatCount       = 0;
const int   PREBEAT_THRESH     = 4;
const int   MISSED_BEAT_THRESH = 3;

// ===== 関数 =====
// フォトトランジスタの割り込み
void photoISR() {
  unsigned long now = millis();
  if (now - lastBeatTime >= 200) {
    int photoRaw = analogRead(PHOTO_PIN);
    photoValue = (float)photoRaw / 1023.0 * 100.0;
    if (photoValue >= 10.0) high = true;
    else high = false;
    if (high && !stateHigh) {
      beatDetected = true;
      currentBeatTime = millis();
      lastBeatTime = currentBeatTime;
      stateHigh = true;
    }
    else stateHigh = false;
  };
};

// rgbより色を判定
const char* detectColor(uint16_t r, uint16_t g, uint16_t b, int& outIndex) {
  struct ColorRef { float r, g, b; const char* name; };

  static const ColorRef refs[] = {
    {0.9958f, 0.0910f, 0.0000f, "Red"},
    {0.0584f, 0.9727f, 0.2245f, "Green"},
    {0.0235f, 0.3011f, 0.9536f, "Blue"},
    {0.3694f, 0.9102f, 0.1875f, "Yellow"},
    {0.0459f, 0.8085f, 0.5867f, "Cyan"},
    {0.7177f, 0.2621f, 0.6451f, "Magenta"},
    {0.2667f, 0.7942f, 0.5460f, "White"},
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
void updateTempoByPhoto() {
  if (previousBeatTime != 0) {
    float measuredBeatInterval = currentBeatTime - previousBeatTime;
    float tempoErrorRate       = abs(measuredBeatInterval - estimatedBeatInterval) / estimatedBeatInterval * 100.0f;
    if (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH)
      estimatedBeatInterval = measuredBeatInterval;
    else 
      estimatedBeatInterval = emaAlpha * measuredBeatInterval + (1.0f - emaAlpha) * estimatedBeatInterval;
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
void B_loop() {
  led();
  photoISR();
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