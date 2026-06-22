int noteIndex = 0; //現在の音符ポインタ
unsigned long soundTime;
int i=0;
unsigned long sendTime = 0; //シリアル通信を行う時間
uint8_t note_off = 1; //前の音の終了
uint8_t note = 0; //音名インデックス（休符＝7）
uint8_t vel = 0; //音の強さ（0~127）
uint8_t note_long = 0; //音の長さ
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
  unsigned long sendTime = nextBeatTime;
  return sendTime;
}


//void setup() {
 // Serial.begin(115200);

  //Serial.print("Note_OFF");
  //Serial.print(",");
  //Serial.print("NOTE");
  //Serial.print(",");
  //Serial.println("VEL");
//}


void C_loop() {
  if (state != IDLE && state != CALIBRATE && state != PREBEAT && state != FERMATA) {
    if(noteIndex < SCORE_LEN){

        note = score[noteIndex][0];
        vel  = score[noteIndex][1];
        note_long = score[noteIndex][2];
      if (beatUpdated && isPlaying) {

           if (note_long == 8){
            soundTime = currentBeatTime + ( nextBeatTime - currentBeatTime ) * 0.5;
            sendTime = sendTime_set(soundTime);

            if (millis() >= sendTime){
           Serial.write(note_off);
           Serial.write(note);
           Serial.write(vel);
           Serial.write(note_long);

           noteIndex++;
           beatUpdated = false;
            }

        }

          if (note_long == 4){
            sendTime = sendTime_set(nextBeatTime);
            if(millis() >= sendTime){
              Serial.write(note_off);
              Serial.write(note);
              Serial.write(vel);
              Serial.write(note_long);

              noteIndex++;
              beatUpdated = false;
            }
          }
           if (note_long == 2){
            if (i==0){
           sendTime = sendTime_set(nextBeatTime);
            if (millis() >= sendTime){
           Serial.write(note_off);
           Serial.write(note);
           Serial.write(vel);
           Serial.write(note_long);

           beatUpdated = false;
           noteIndex++;
           i++;
            }           
           }
            else {
              sendTime = sendTime_set(nextBeatTime);    
              if (millis() >= sendTime){          
              beatUpdated = false;
              i = 0;
              }
             }
      
      }
      if (noteIndex == SCORE_LEN) noteIndex = 0;
    }
  }
 
}
}


