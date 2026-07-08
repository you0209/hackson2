/**
 * サーボ単体テスト
 * シリアルモニタから操作:
 *   f → 前方（60度）に移動
 *   b → 初期位置（0度）に戻る
 *   s → sin波で1サイクル動かす
 */

#include <Servo.h>

#define PIN_SERVO 9

const int SERVO_SAFE      =  0;
const int SERVO_CENTER    = 30;
const int SERVO_AMPLITUDE = 30;
const int SERVO_STEPS     = 20;

Servo servo;

void setup() {
  Serial.begin(115200);
  servo.attach(PIN_SERVO);
  servo.write(SERVO_SAFE);
  Serial.println("READY");
  Serial.println("f: 前方(60度)  b: 戻る(0度)  s: sinサイクル");
}

void loop() {
  if (!Serial.available()) return;

  char c = Serial.read();

  if (c == 'f') {
    int angle = SERVO_CENTER + SERVO_AMPLITUDE;
    servo.write(angle);
    Serial.print("-> "); Serial.print(angle); Serial.println("度");

  } else if (c == 'b') {
    servo.write(SERVO_SAFE);
    Serial.println("-> 0度（初期位置）");

  } else if (c == 's') {
    Serial.println("sin 1サイクル開始");
    for (int step = 0; step < SERVO_STEPS; step++) {
      float phase = (2.0 * PI * step) / SERVO_STEPS;
      int angle   = SERVO_CENTER + (int)(SERVO_AMPLITUDE * sin(phase));
      servo.write(angle);
      Serial.print("step="); Serial.print(step);
      Serial.print(" angle="); Serial.println(angle);
      delay(100);
    }
    Serial.println("完了");
  }
}
