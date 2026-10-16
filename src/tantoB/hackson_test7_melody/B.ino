// ===== 変数定義 =====
// カラーセンサ
const char* PLAY_COLOR_NAME = "Yellow"; // 自パート（メロディ）の演奏開始色。パートごとに異なる値を設定する（例：ベース=Blue、シンバル=Red）
int         colorIndex      = -1;       // detectColor()が返す色のインデックス（-1は未判定。showMatrix()の表示用）

// フォトトランジスタ
bool           beatDetected        = false; // 今回のループで新たに拍を検出したらtrue（B_loop()の最後でfalseに戻す単発フラグ）
int            detectCount         = 0;     // 起動からの累計拍検出回数。IDLE→PREBEATへの遷移判定に使用（updateState()参照）
bool           stateHigh           = false; // 前回のphotoISR()呼び出し時点で光量が閾値以上だったか。(photoValue >= PHOTO_HIGH_THRESH) && !stateHigh で「暗→明」の立ち上がりを検出する
float          photoValue          = 0.0;   // フォトトランジスタの光量を0〜100%に正規化した値
const float    PHOTO_HIGH_THRESH   = 10.0;  // 拍（LEDの光）を検出したと判定するための光量閾値(%)
const unsigned long PHOTO_DEBOUNCE = 200;   // 誤検出防止のデバウンス時間(ms)。この時間内は拍検出処理を行わない

// EMA（指数移動平均によるテンポ推定）
float         estimatedBeatInterval = 1000.0f; // 推定拍間隔(ms)。初期値1000ms（=60bpm相当）
float         emaAlpha              = 0.2f;    // EMAの平滑化係数。大きいほど直近の実測値を強く反映する
unsigned long previousBeatTime      = 0;       // 前回の拍検出時刻(ms)。拍間隔の算出に使用
const float   TEMPO_CHANGE_THRESH   = 10.0f;   // 急なテンポ変化とみなす誤差率の閾値(%)。これを超えるとEMAをバイパスし直前の拍間隔をそのまま採用する
float         previousNextBeatTime  = 0;       // 更新前のnextBeatTimeを保持（算出はしているが、本ファイル内では参照箇所なし）

// 状態変化
int         prebeatCount       = 0; // PREBEAT状態に入ってからの拍検出回数
const int   PREBEAT_THRESH     = 4; // PREBEATからPLAYINGへ遷移するために必要な拍数
const int   MISSED_BEAT_THRESH = 3; // 拍の見逃し許容倍率。「推定拍間隔 × この値」だけ拍が来なければPLAYING→IDLEへ戻す

// ===== 関数 =====
// フォトトランジスタによる拍検出
// 名前はISRだが、実際はハードウェア割り込みではなくB_loop()から毎回呼ばれるポーリング処理
void photoISR() {
  unsigned long now = millis();
  if (now - lastBeatTime >= PHOTO_DEBOUNCE) {
    int photoRaw = analogRead(PHOTO_PIN);
    photoValue = (float)photoRaw / 1023.0 * 100.0; // 0〜100%に正規化
    if (photoValue >= PHOTO_HIGH_THRESH) {
      if(!stateHigh) { // 「暗→明」の立ち上がり＝1拍として検出
        beatDetected = true;
        currentBeatTime = now;
        lastBeatTime = currentBeatTime;
        stateHigh = true;
      };
    }
    else stateHigh = false;
  };
};

// RGB値から最も近い基準色を判定する（コサイン類似度）
const char* detectColor(uint16_t r, uint16_t g, uint16_t b, int& outIndex) {
  struct ColorRef { float r, g, b; const char* name; };

  // 各色の基準ベクトル。理論値ではなく、実機でキャリブレーションして得た実測値
  // （理論値をそのまま使うと環境光の影響でうまく判別できなかったため）
  static const ColorRef refs[] = {
    {0.9972f, 0.0749f, 0.0000f, "Red"},
    {0.0545f, 0.9722f, 0.2278f, "Green"},
    {0.0107f, 0.2915f, 0.9565f, "Blue"},
    {0.3878f, 0.9017f, 0.1911f, "Yellow"},
    {0.0403f, 0.7924f, 0.6087f, "Cyan"},
    {0.9407f, 0.2471f, 0.6247f, "Magenta"},
    {0.2854f, 0.7706f, 0.5698f, "White"},
  };
  const int refCount = sizeof(refs) / sizeof(refs[0]);

  float mag = sqrt((float)r*r + (float)g*g + (float)b*b); // ベクトルの大きさ（正規化用）
  //if (mag < 100) return "Unknown";  // 暗すぎて判別不能

  float nr = r / mag, ng = g / mag, nb = b / mag; // 単位ベクトル化（明るさの違いを吸収する）

  float bestScore = -1;
  const char* bestName = "Unknown";
  outIndex = -1;
  for (int i = 0; i < refCount; i++) {
    float score = nr*refs[i].r + ng*refs[i].g + nb*refs[i].b; // 内積＝コサイン類似度
    if (score > bestScore) { bestScore = score; bestName = refs[i].name; outIndex = i; }
  };
  return bestName;
};

// カラーセンサから1回分のRGB値を取得し、colorNameを更新する（拍検出時にのみ呼ばれる）
void readColor() {
  AE_S13683_LEDResult result = colorSensor.getColorSensorResultOneshot();
  colorName = detectColor(result.red, result.green, result.blue, colorIndex);
};

// フォトトランジスタで検出した拍の間隔から、EMAで次拍の時刻を予測する
void updateTempoByPhoto() {
  if (previousBeatTime != 0) {
    float measuredBeatInterval = currentBeatTime - previousBeatTime; // 直前の実測拍間隔
    float tempoErrorRate = abs(measuredBeatInterval - estimatedBeatInterval) / estimatedBeatInterval * 100.0f;
    if (state != PREBEAT && tempoErrorRate > TEMPO_CHANGE_THRESH)
      // 誤差率が閾値を超えた場合：EMAによる平滑化を行わず、急なテンポ変化に即座に追従する
      estimatedBeatInterval = measuredBeatInterval;
    else
      // 通常時：指数移動平均で滑らかに推定値を更新する
      estimatedBeatInterval = emaAlpha * measuredBeatInterval + (1.0f - emaAlpha) * estimatedBeatInterval;
    previousBeatTime     = currentBeatTime;
    previousNextBeatTime = nextBeatTime;
    nextBeatTime         = currentBeatTime + (unsigned long)estimatedBeatInterval; // 次拍の予測絶対時刻
  }
  else {
    // 初回（起動直後）：まだ拍間隔が測れないため暫定値を設定
    previousBeatTime     = currentBeatTime;
    previousNextBeatTime = nextBeatTime;
    nextBeatTime         = currentBeatTime * 2;
  };
};

// 状態遷移処理（IDLE→PREBEAT→PLAYING、および見逃し時のPLAYING→IDLE）
void updateState() {
  switch (state) {
    case IDLE: { // 停止：拍が2回以上検出されたら予備振りへ
      isPlaying = false;
      if (beatDetected && detectCount >= 2) {
        prebeatCount = 0;
        state        = PREBEAT;
      };
      break;
    };
    case PREBEAT: { // 予備振り：拍をPREBEAT_THRESH回検出したら通常演奏へ
      if (prebeatCount >= PREBEAT_THRESH) {
        state = PLAYING;
      };
      break;
    };
    case PLAYING: { // 通常演奏：推定拍間隔のMISSED_BEAT_THRESH倍以上，拍が来なければ見逃しとみなし停止へ戻す（異常時フォールバック）
      if (millis() - lastBeatTime > (unsigned long)(estimatedBeatInterval * MISSED_BEAT_THRESH)) {
        state = IDLE;
      };
      break;
    };
  };
};

// ===== テスト用関数（本番の演奏動作には関与しない、デバッグ表示用） =====
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
const unsigned long COLOR_INTERVAL    = 1000; // 自機ステータスLEDの色を切り替える間隔(ms)
unsigned long       lastColorChange   = 0;
int                 currentColorIndex = 0;
bool                ledHigh           = false;

// 自機ステータスLEDを一定間隔で色を切り替えながら点滅させる（動作確認用）
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
const unsigned long BEAT_FLASH_MS  = 150; // 拍検出時にマトリクスの左上を光らせておく時間(ms)

// 本体基板のLEDマトリクスに、拍検出・現在の状態・演奏可否・判定色を表示する（デバッグ用）
void showMatrix() {
  uint8_t frame[8][12] = {};
  if (millis() < beatFlashUntil) frame[0][0] = 1; // 拍検出時に点灯
  frame[state][1] = 1;                            // 現在の状態（IDLE/PREBEAT/PLAYING）を行位置で表示
  if (!isPlaying) frame[0][2] = 1;
  else frame[1][2] = 1;                           // 演奏中かどうか
  if (colorIndex != -1) frame[colorIndex][3] = 1;  // 直近に判定した色
  matrix.renderBitmap(frame, 8, 12);
};

// ===== メインループ =====
// 担当Bのメイン処理：拍検出→（拍があれば）色判定・テンポ推定→状態更新、を1ループごとに実行する
void B_loop() {
  led();
  photoISR();
  if (beatDetected) {
    beatFlashUntil  = millis() + BEAT_FLASH_MS;
    detectCount++;
    readColor(); // 拍のタイミングでのみカラーセンサを読む（毎ループ読まないことでI2C負荷を抑える）
    if (state == PREBEAT) prebeatCount++;
    if (state != IDLE) { // PREBEAT/PLAYING中のみテンポ推定・演奏開始判定を行う
      if (!isPlaying && strcmp(colorName, PLAY_COLOR_NAME) == 0) isPlaying = true; // 自パートの開始色を検出したら演奏開始
      updateTempoByPhoto();
      beatUpdated = true; // 担当C側へ「次拍予測時刻が更新された」ことを通知
    };
  };
  updateState();
  showMatrix();
  beatDetected = false; // 単発フラグをリセット
};
