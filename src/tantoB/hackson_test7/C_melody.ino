int noteIndex = 0; //現在の音符ポインタ
unsigned long sendTime = 0; //シリアル通信を行う時間
uint8_t note_off = 1; //前の音の終了
uint8_t note = 0; //音名インデックス（休符＝7）
uint8_t vel = 0; //音の強さ（0~127）
uint8_t note_long = 0; //音の長さ
bool sounded = true; //音を鳴らしたか
bool reset = false;
int chapterIndex = -1;
int chapterBeatCount = 0;
const int CHAPTER_INTERVAL = 4;
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
      chapterIndex = -1;
      chapterBeatCount = 0;
      beatUpdated = false;
      sounded = false;
      Serial.write(note_off);
      reset = true;
    };
  }
  else {
    reset = false;
    if (beatUpdated && sounded) {
      chapterBeatCount++;
      if (chapterBeatCount >= CHAPTER_INTERVAL+1) {
        chapterIndex++;
        chapterBeatCount = 0;
      };
      if (strcmp(colorName, "White") != 0) {
        chapterIndex++;
        chapterBeatCount = 0;
        float beatIndex = (float)chapterIndex * CHAPTER_INTERVAL;
        noteIndex = 0;
        while (beatIndex > 0.0 && noteIndex < SCORE_LEN) {
          if (score[noteIndex][2] == 8) beatIndex -= 0.5;
          if (score[noteIndex][2] == 4) beatIndex -= 1.0;
          if (score[noteIndex][2] == 2) beatIndex -= 2.0;
          noteIndex ++;
        };
      };
      if (noteIndex >= SCORE_LEN) {
          chapterIndex = -1;
          chapterBeatCount = 0;
          noteIndex = 0;
        };
      note = score[noteIndex][0];
      vel  = score[noteIndex][1];
      note_long = score[noteIndex][2];
      sendTime = sendTime_set(nextBeatTime);
      if (note_long == 8 && i == 1) {
        sendTime = currentBeatTime + ( sendTime - currentBeatTime ) * 0.5;
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
          i = 0;
        };
      };
      sounded = true;
    };
  };
};


