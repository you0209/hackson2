#pragma once

#include <Arduino.h>
#include <Wire.h>

using namespace std;

struct AE_S13683_LEDResult {
  uint16_t red;
  uint16_t green;
  uint16_t blue;
  uint16_t rawRed;
  uint16_t rawGreen;
  uint16_t rawBlue;
  uint16_t rawCorrection;
};

class AE_S13683_LED {
 public:
  AE_S13683_LED();

  enum class colorSensorGain : uint8_t {
    GAIN_HIGH = 0x08,
    GAIN_LOW = 0x00
  };

  enum class colorSensorAutoExposureTime : uint8_t {
    T87_5us = 0x00,
    T1_4ms = 0x01,
    T22_4ms = 0x02,
    T179_2ms = 0x03
  };

  enum class LED_CURRENT : uint8_t {
    C0_5mA = 0,    //  0.5mA
    C1_0mA = 1,    //  1.0mA
    C1_5mA = 2,    //  1.5mA
    C2_0mA = 3,    //  2.0mA
    C2_5mA = 4,    //  2.5mA
    C3_0mA = 5,    //  3.0mA
    C3_5mA = 6,    //  3.5mA
    C4_0mA = 7,    //  4.0mA
    C4_5mA = 8,    //  4.5mA
    C5_0mA = 9,    //  5.0mA
    C5_5mA = 10,   //  5.5mA
    C6_0mA = 11,   //  6.0mA
    C6_5mA = 12,   //  6.5mA
    C7_0mA = 13,   //  7.0mA
    C7_5mA = 14,   //  7.5mA
    C8_0mA = 15,   //  8.0mA
    C8_5mA = 16,   //  8.5mA
    C9_0mA = 17,   //  9.0mA
    C9_5mA = 18,   //  9.5mA
    C10_0mA = 19,  // 10.0mA
    C10_5mA = 20,  // 10.5mA
    C11_0mA = 21,  // 11.0mA
    C11_5mA = 22,  // 11.5mA
    C12_0mA = 23,  // 12.0mA
    C12_5mA = 24,  // 12.5mA
    C13_0mA = 25,  // 13.0mA
    C13_5mA = 26,  // 13.5mA
    C14_0mA = 27,  // 14.0mA
    C14_5mA = 28,  // 14.5mA
    C15_0mA = 29,  // 15.0mA
    C15_5mA = 30,  // 15.5mA
    C16_0mA = 31,  // 16.0mA
    C16_5mA = 32,  // 16.5mA
    C17_0mA = 33,  // 17.0mA
    C17_5mA = 34,  // 17.5mA
    C18_0mA = 35,  // 18.0mA
    C18_5mA = 36,  // 18.5mA
    C19_0mA = 37,  // 19.0mA
    C19_5mA = 38,  // 19.5mA
    C20_0mA = 39,  // 20.0mA
    C20_5mA = 40,  // 20.5mA
    C21_0mA = 41,  // 21.0mA
    C21_5mA = 42,  // 21.5mA
    C22_0mA = 43,  // 22.0mA
    C22_5mA = 44,  // 22.5mA
    C23_0mA = 45,  // 23.0mA
    C23_5mA = 46,  // 23.5mA
    C24_0mA = 47,  // 24.0mA
    C24_5mA = 48,  // 24.5mA
    C25_0mA = 49,  // 25.0mA
    C25_5mA = 50,  // 25.5mA
    C26_0mA = 51,  // 26.0mA
    C26_5mA = 52,  // 26.5mA
    C27_0mA = 53,  // 27.0mA
    C27_5mA = 54,  // 27.5mA
    C28_0mA = 55,  // 28.0mA
    C28_5mA = 56,  // 28.5mA
    C29_0mA = 57,  // 29.0mA
    C29_5mA = 58,  // 29.5mA
    C30_0mA = 59,  // 30.0mA
    C30_5mA = 60,  // 30.5mA
    C31_0mA = 61,  // 31.0mA
    C31_5mA = 62,  // 31.5mA
    C32_0mA = 63   // 32.0mA
  };

  bool begin(TwoWire *wire = &Wire);

  // カラーセンサー 繰り返し測定モード
  void colorSensorConfigAuto(colorSensorGain gain = colorSensorGain::GAIN_HIGH,
                             colorSensorAutoExposureTime time = colorSensorAutoExposureTime::T22_4ms);
  AE_S13683_LEDResult getColorSensorResultAuto();

  // カラーセンサー 単発測定モード
  bool colorSensorConfigOneshot(colorSensorGain gain = colorSensorGain::GAIN_HIGH,
                                uint64_t exposureTimeus = 20 * 1000);
  AE_S13683_LEDResult getColorSensorResultOneshot();

  // LEDドライバー設定
  void ledDriverConfig(bool enable = true, LED_CURRENT led_current = LED_CURRENT::C10_0mA);

 private:
  void writeData(uint8_t add, uint8_t reg, uint8_t data);
  uint8_t readData(uint8_t add, uint8_t reg);
  AE_S13683_LEDResult readColorSensorResult();

  TwoWire *wire;
  uint64_t totalExposureTime = 0;
  uint8_t ctrlSetting = 0;
};
