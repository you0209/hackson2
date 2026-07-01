// ===== 変数定義 =====
//カラーセンサ
const char* colorName       = "Unknown";
const char* PLAY_COLOR_NAME = "Red";

// フォトトランジスタ
volatile bool beatDetected = false;
int           detectCount  = 0;
bool          high         = false;
bool          stateHigh    = false;

// EMA
float         estimatedBeatInterval = 1000.0f;
float         emaAlpha              = 0.2f;
unsigned long previousBeatTime      = 0;
const float   TEMPO_CHANGE_THRESH   = 10.0f;
float         previousNextBeatTime  = 0;

// 状態変化
const char*            START_COLOR_NAME   = "Red";
int                    prebeatCount       = 0;
const int              PREBEAT_THRESH     = 4;
const int              MISSED_BEAT_THRESH = 3;

// ===== 関数 =====
// フォトトランジスタの割り込み
void photoISR() {
  high = digitalRead(PHOTO_PIN);
  if (high) {
    if (!stateHigh) {
      unsigned long now = millis();
      if (now - lastBeatTime > 200) {  // 200ms以内の再検知は無視（300BPM相当）
        beatDetected    = true;
        currentBeatTime = now;
        lastBeatTime    = now;
      }
      stateHigh = true;
    };
  }
  else stateHigh = false;
};

void readPhoto() {
  int raw = analogRead(PHOTO_PIN);
};

// rgbより色を判定
const char* detectColor(float r, float g, float b) {
  struct ColorRef { float r, g, b; const char* name; };
  static const ColorRef refs[] = {
    {0.975f, 0.222f, 0.017f, "Red"},
    {0.130f, 0.957f, 0.261f, "Green"},
    {0.088f, 0.406f, 0.910f, "Blue"},
    {0.524f, 0.828f, 0.203f, "Yellow"},
    {0.082f, 0.754f, 0.653f, "Cyan"},
    {0.519f, 0.371f, 0.770f, "Magenta"},
    {0.364f, 0.717f, 0.596f, "White"}
  };
  float mag       = sqrt(r*r + g*g + b*b);
  float nr        = r / mag, ng = g / mag, nb = b / mag;
  float bestScore = -1;
  const char* bestName  = "Unknown";
  for (const auto& ref : refs) {
    float score = nr*ref.r + ng*ref.g + nb*ref.b;
    if (score > bestScore) {
      bestScore = score;
      bestName  = ref.name;
    };
  };
  return bestName;
};

// カラーセンサの値を平滑化しcolorNameに色を代入
void readColor() {
  AE_S13683_LEDResult result = colorSensor.getColorSensorResultOneshot();
  colorName  = detectColor(result.red, result.green, result.blue);
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
  {0x7F, 0x00, 0x00, "Red    "},
  {0x00, 0x7F, 0x00, "Green  "},
  {0x00, 0x00, 0x7F, "Blue   "},
  {0x7F, 0x7F, 0x00, "Yellow "},
  {0x00, 0x7F, 0x7F, "Cyan   "},
  {0x7F, 0x00, 0x7F, "Magenta"},
  {0x7F, 0x7F, 0x7F, "White  "},
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
  matrix.renderBitmap(frame, 8, 12);
};

// ===== メインループ =====
void B_loop() {
  led();
  photoISR();
  if (beatDetected) {
    detectCount++;
    if (state == PREBEAT) prebeatCount++;
    beatFlashUntil  = millis() + BEAT_FLASH_MS;
    readColor();
    if (state != IDLE) {
      if (!isPlaying && strcmp(colorName, PLAY_COLOR_NAME) == 0) isPlaying = true;
      updateTempoByPhoto();
      beatUpdated  = true;
    };
  };
  updateState();
  showMatrix();
  beatDetected = false;
};