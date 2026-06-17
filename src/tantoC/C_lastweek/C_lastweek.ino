#include<Arduino.h>

  int noteIndex = 0; //現在の音符ポインタ
  unsigned long t_next  = 0; //次の拍の予測絶対時間
  uint8_t sendTime = 0; //シリアル通信を行う時間
  bool beatUpdated = false; //拍予測の更新
  enum State {IDLE , PREBEAT , PLAYING , TEMPO_CHANGE , FERMATA };
  State state =IDLE; //現在の状態
  uint8_t note_off = 1; //前の音の終了
  uint8_t note = 0; //音名インデックス（休符＝7）
  uint8_t vel = 0; //音の強さ（0~127）
  uint8_t note_long = 0; //音の長さ
  const uint8_t score[][3] ={
    {0, 100,1},
    {1, 100,1},
    {2, 100,1}
  };
  const int SCORE_LEN = sizeof ( score ) / sizeof ( score[0]);

  unsigned long sendTime_set(unsigned long t_next) {//担当Bよりより出された次の拍の予測絶対時間に補正値をかける
  unsigned long sendTime = t_next;

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


//void loop() {
  if (state != FERMATA) {
    if(noteIndex <= SCORE_LEN){

      if (beatUpdated == true) {
        sendTime = sendTime_set(t_next);

        note = score[noteIndex][0];
        vel  = score[noteIndex][1];
        note_long = score[noteIndex][2];
        
         if (millis() >= sendTime){
           Serial.write(note_off);
           Serial.write(note);
           Serial.write(vel);
           Serial.write(note_long);

           //Serial.print(note_off);
           //Serial.print(",");
           //Serial.print(note);
           //Serial.print(",");
           //Serial.println(vel);

           noteIndex++;
           beatUpdated = false;

        }
      
      }
    }
  }
 
//}
