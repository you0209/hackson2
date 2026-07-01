
//=============================================================================//
int LED_COUNT = 1;
//=============================================================================//

//=========================================================//
// 初期化信号送出
//=========================================================//
void LED_Init() {
  digitalWrite(DIN_H, LOW);
  digitalWrite(DIN_L, HIGH);
  delay(10); 
}
//=========================================================//
// 値固定(Set)信号送出
//=========================================================//
void LED_Set() {
  digitalWrite(DIN_H, LOW);
  digitalWrite(DIN_L, HIGH);
  delay(10); 
}

//=========================================================//
// H(1)信号送出
//=========================================================//
void LED_Hi_Bit() {
  digitalWrite(DIN_H, HIGH);
  digitalWrite(DIN_L, HIGH);
  delayMicroseconds(1);
  digitalWrite(DIN_H, LOW);
  digitalWrite(DIN_L, HIGH);
  delayMicroseconds(1);
}
//=========================================================//
// L(0)信号送出
//=========================================================//
void LED_Low_Bit() {
  digitalWrite(DIN_H, LOW);
  digitalWrite(DIN_L, LOW);
  delayMicroseconds(1);
  digitalWrite(DIN_H, LOW);
  digitalWrite(DIN_L, HIGH);
  delayMicroseconds(50);
}

//=========================================================//
// LED用にＲＧＢカラー値を変換して指定する関数 
//=========================================================//
void LED_Color_RGB(byte led_r, byte led_g, byte led_b) {
  for (int k = 0; k <= 7; k++) {  //青
    if ((bitRead(led_b, 7 - k)) == 1) {
      LED_Hi_Bit();
    }
    else {
      LED_Low_Bit();
    }
  }
  for (int k = 0; k <= 7; k++) {  //緑
    if ((bitRead(led_g, 7 - k)) == 1) {
      LED_Hi_Bit();
    }
    else {
      LED_Low_Bit();
    }
  }
  for (int k = 0; k <= 7; k++) {  //赤
    if ((bitRead(led_r, 7 - k)) == 1) {
      LED_Hi_Bit();
    }
    else {
      LED_Low_Bit();
    }
  }
}
//=============================================================================//

void setLEDColor(byte r, byte g, byte b) {
  // 長めのラッチ信号でLED内部カウンターを確実にリセット
  digitalWrite(DIN_H, LOW);
  digitalWrite(DIN_L, HIGH);
  delay(50);

  // 3回送って少なくとも1回は正しく受け取れるようにする
  for (int repeat = 0; repeat < 3; repeat++) {
    for (int n = 0; n < LED_COUNT; n++) {
      LED_Color_RGB(r, g, b);
    }
    digitalWrite(DIN_H, LOW);
    digitalWrite(DIN_L, HIGH);
    delay(5);
  }
}
