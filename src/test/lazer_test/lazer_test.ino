#include <Wire.h>
#include <VL53L0X.h>

#define XSHUT_PIN 7
VL53L0X sensor;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(XSHUT_PIN, OUTPUT);
  digitalWrite(XSHUT_PIN, LOW);
  delay(10);
  digitalWrite(XSHUT_PIN, HIGH);
  delay(10);

  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("VL53L0X init FAILED");
    while (1);  // 初期化失敗時はここで停止
  }
  Serial.println("VL53L0X init OK");
  sensor.startContinuous();
};

void loop() {
  float distance = sensor.readRangeContinuousMillimeters()/10.0;

  if (sensor.timeoutOccurred()) {
    Serial.println("タイムアウト");
  }
  else {
    Serial.print("距離: ");
    Serial.print(distance);
    Serial.println(" cm");
  };
}