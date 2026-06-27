// I2C Scanner
// 目的: I2C接続されたデバイスのアドレスを確認する
// 対象: S13683 カラーセンサ（Hamamatsu）の接続確認用
//
// 配線:
//   S13683 VCC → Arduino 3.3V  ※5Vは不可
//   S13683 GND → Arduino GND
//   S13683 SDA → Arduino A4（またはSDAピン）
//   S13683 SCL → Arduino A5（またはSCLピン）
//
// 使い方:
//   1. 書き込んで起動
//   2. シリアルモニタを115200bpsで開く
//   3. "Found: 0x??" の行に表示されたアドレスを記録する

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(1000);

  Serial.println("=== I2C Scanner ===");
  Serial.println("Scanning...");

  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Found device at: 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("No I2C devices found.");
    Serial.println("配線を確認してください。");
  } else {
    Serial.print(found);
    Serial.println(" device(s) found.");
  }

  Serial.println("Done.");
}

void loop() {}
