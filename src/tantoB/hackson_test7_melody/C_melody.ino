int noteIndex = 0; //現在の音符ポインタ
unsigned long sendTime = 0; //シリアル通信を行う時間
unsigned long present_BeatTime;//currentBeatTimeに補正値をかけた値
uint8_t note_off = 1; //前の音の終了
uint8_t note = 0; //音名インデックス（休符＝7）
uint8_t vel = 0; //音の強さ（0~127）
uint8_t note_long = 0; //音の長さ
bool sounded = true; //音を鳴らしたか
bool reset = false;
int scoreBeatCount = 0;
int cyanCount = 0;
int i = 0;
const uint8_t score[][3] = {
    {0, 100, 4},//ド　
    {1, 100, 4},//レ
    {2, 100, 4},//ミ
    {3, 100, 4},//ファ｜
    {2, 100, 4},//ミ　
    {1, 100, 4},//レ
    {0, 100, 2},//ド｜
    {2, 127, 4},//ミ
    {3, 127, 4},//ファ
    {4, 127, 4},//ソ
    {5, 127, 4},//ラ｜
    {4, 127, 4},//ソ
    {3, 127, 4},//ファ
    {2, 127, 2},//ミ｜
    {0, 90,  4},//ド
    {6, 100, 4},
    {0, 90,  4},//ド｜
    {6, 100, 4},
    {0, 90,  4},//ド
    {6, 100, 4},
    {0, 90,  4},//ド｜
    {6, 100, 4},
    {0, 100, 8},//ド
    {0, 100, 8},//ド
    {1, 100, 8},//レ
    {1, 100, 8},//レ
    {2, 100, 8},//ミ
    {2, 100, 8},//ミ
    {3, 100, 8},//ファ
    {3, 100, 8},//ファ｜
    {2, 100, 4},//ミ
    {1, 100, 4},//レ
    {0, 100, 4},//ド
    {6, 100, 4}
  };
const int SCORE_LEN = sizeof ( score ) / sizeof ( score[0]);

unsigned long sendTime_set(unsigned long nextBeatTime) {//担当Bよりより出された次の拍の予測絶対時間に補正値をかける
  unsigned long sendTime = nextBeatTime - 235;
  return sendTime;
};

void C_loop() {
  if (!isPlaying) {
    if (!reset) {
      noteIndex = 0;
      sendTime = 0;
      i = 0;
      scoreBeatCount = 0;
      cyanCount = 0;
      beatUpdated = false;
      sounded = false;
      Serial.write(note_off);
      Serial.write((uint8_t)0);
      Serial.write((uint8_t)0);
      Serial.write((uint8_t)0);
      reset = true;
    };
  }
  else {
    reset = false;
    if (beatUpdated && sounded) {
      scoreBeatCount++;
      if (beatMissRecovered) {
        scoreBeatCount++;
        beatMissRecovered = false;
      };
      if (strcmp(colorName, PLAY_COLOR_NAME) == 0) {
        noteIndex = 0;
        scoreBeatCount = 0;
        cyanCount = 0;
      } else if (strcmp(colorName, "Cyan") == 0) {
        cyanCount++;
        float beatPos = (float)(cyanCount * 4);
        int newNoteIndex = 0;
        while (beatPos > 0.0 && newNoteIndex < SCORE_LEN) {
          if (score[newNoteIndex][2] == 8) beatPos -= 0.5;
          if (score[newNoteIndex][2] == 4) beatPos -= 1.0;
          if (score[newNoteIndex][2] == 2) beatPos -= 2.0;
          newNoteIndex++;
        };
        if (newNoteIndex < SCORE_LEN) noteIndex = newNoteIndex;
      };
      note = score[noteIndex][0];
      vel  = score[noteIndex][1];
      note_long = score[noteIndex][2];
      sendTime = sendTime_set(nextBeatTime);
      present_BeatTime = sendTime_set(currentBeatTime);
      if (note_long == 8 && i == 1) {
        sendTime = present_BeatTime + ( sendTime - present_BeatTime ) * 0.5;
      };
      sounded = false;
      beatUpdated = false;
    };
    if (millis() >= sendTime && !sounded) {
      if (note_long == 8) {
        Serial.write(note_off);
        Serial.write(note);
        Serial.write(vel);
        Serial.write(note_long);
        noteIndex++;
        if (noteIndex >= SCORE_LEN) { isPlaying = false; sounded = true; return; };
        if (i == 0) {
          i++;
        }
        else if (i == 1) {
          beatUpdated = true;
          i = 0;
        };
      };
      if (note_long == 4){
        Serial.write(note_off);
        Serial.write(note);
        Serial.write(vel);
        Serial.write(note_long);
        noteIndex++;
        if (noteIndex >= SCORE_LEN) { isPlaying = false; sounded = true; return; };
      };
      if (note_long == 2){
        if (i==0){
          Serial.write(note_off);
          Serial.write(note);
          Serial.write(vel);
          Serial.write(note_long);
          i++;
        }
        else if (i == 1) {
          noteIndex++;
          if (noteIndex >= SCORE_LEN) { isPlaying = false; sounded = true; return; };
          i = 0;
        };
      };
      sounded = true;
    };
  };
};


