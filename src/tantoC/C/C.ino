int noteIndex = 0; //現在の音符ポインタ
unsigned long time;
int i=0;
unsigned long sendTime = 0; //シリアル通信を行う時間
uint8_t note_off = 1; //前の音の終了
uint8_t note = 0; //音名インデックス（休符＝7）
uint8_t vel = 0; //音の強さ（0~127）
float note_long = 0; //音の長さ
const float score[][3] = {
  {0, 100,1},
  {1, 100,2},
  {2, 100,1},
  {3, 100,0.5}
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

           if (note_long < 1.0){
            time = currentBeatTime + ( nextBeatTime - currentBeatTime ) * note_long;
            sendTime = sendTime_set(time);

            if (millis() >= sendTime){
           Serial.write(note_off);
           Serial.write(note);
           Serial.write(vel);
           Serial.write(note_long);

           noteIndex++;
           beatUpdated = false;
            }

        }
           else {
            if (i==0){
           sendTime = sendTime_set(time);
            if (millis() >= sendTime){
           Serial.write(note_off);
           Serial.write(note);
           Serial.write(vel);
           Serial.write(note_long);

           beatUpdated = false;
           i++;
            }           
           }
             else if (i < note_long){
              sendTime = sendTime_set(time);    
              if (millis() >= sendTime){          
              beatUpdated = false;
              i++;
              }
             }
             else {
               noteIndex++;
               i=0;
               }
      
      }
      if (noteIndex == SCORE_LEN) noteIndex = 0;
    }
  }
 
}
}


