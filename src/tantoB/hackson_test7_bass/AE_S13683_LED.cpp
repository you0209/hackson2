#include "AE_S13683_LED.h"

#include <limits.h>

constexpr uint8_t CAT3626Address = 0x66;
constexpr uint8_t S13683Address = 0x2A;

enum class S13683Register : uint8_t {
  // コントロールレジスタ
  CONTROL = 0x00,

  // マニュアルタイミングレジスタ
  MANUAL_TIMING_HIGH = 0x01,  // 上位バイト
  MANUAL_TIMING_LOW = 0x02,   // 下位バイト

  // センサデータレジスタ
  RED_DATA_HIGH = 0x03,         // Red,   上位バイト
  RED_DATA_LOW = 0x04,          // Red,   下位バイト
  GREEN_DATA_HIGH = 0x05,       // Green, 上位バイト
  GREEN_DATA_LOW = 0x06,        // Green, 下位バイト
  BLUE_DATA_HIGH = 0x07,        // Blue,  上位バイト
  BLUE_DATA_LOW = 0x08,         // Blue,  下位バイト
  CORRECTION_DATA_HIGH = 0x09,  // 補正,   上位バイト
  CORRECTION_DATA_LOW = 0x0A,   // 補正,   下位バイト
};

enum class CAT3626Register : uint8_t {
  REGA = 0x00,  // LED A
  REGB = 0x01,  // LED B (未使用)
  REGC = 0x02,  // LED C (未使用)
  REGEN = 0x03  // LED イネーブル
};

AE_S13683_LED::AE_S13683_LED() {}

bool AE_S13683_LED::begin(TwoWire *wire) {
  // Wire確認
  if (wire == nullptr) { return false; }
  this->wire = wire;
  return true;
}

void AE_S13683_LED::colorSensorConfigAuto(colorSensorGain gain, colorSensorAutoExposureTime time) {
  writeData(S13683Address, static_cast<uint8_t>(S13683Register::CONTROL),
            static_cast<uint8_t>(gain) | static_cast<uint8_t>(time));
}

AE_S13683_LEDResult AE_S13683_LED::getColorSensorResultAuto() {
  return readColorSensorResult();
}

bool AE_S13683_LED::colorSensorConfigOneshot(colorSensorGain gain, uint64_t exposureTimeus) {
  struct timeUnitResigserPair {
    uint64_t unit;
    uint8_t reg;
  };
  constexpr timeUnitResigserPair pair[] = {{175, 0x00}, {2800, 0x01}, {44800, 0x02}, {358400, 0x03}};

  // 範囲外は設定不可
  if ((exposureTimeus < pair[0].unit) || (exposureTimeus > pair[3].unit * static_cast<uint64_t>(65535))) {
    totalExposureTime = 0;
    return false;
  }

  // 露光時間からレジスター値を計算
  for (auto p : pair) {
    if (exposureTimeus <= p.unit * static_cast<uint64_t>(65535)) {
      ctrlSetting = static_cast<uint8_t>(0x04) | static_cast<uint8_t>(gain) | p.reg;
      writeData(S13683Address, static_cast<uint8_t>(S13683Register::CONTROL),
                ctrlSetting | 0x80);
      writeData(S13683Address, static_cast<uint8_t>(S13683Register::MANUAL_TIMING_LOW),
                (exposureTimeus / p.unit) & 0xff);
      writeData(S13683Address, static_cast<uint8_t>(S13683Register::MANUAL_TIMING_HIGH),
                (exposureTimeus / p.unit) >> 8);
      break;
    }
  }

  // 処理用に内部的に記録
  totalExposureTime = exposureTimeus * 4;

  return true;
}

AE_S13683_LEDResult AE_S13683_LED::getColorSensorResultOneshot() {
  // 設定されていない場合はすぐに抜ける
  if (totalExposureTime == 0) {
    return (AE_S13683_LEDResult){0, 0, 0, 0, 0, 0, 0};
  }

  // ADCリセット
  writeData(S13683Address, static_cast<uint8_t>(S13683Register::CONTROL), ctrlSetting | 0x80);
  // ADCリセット解除
  writeData(S13683Address, static_cast<uint8_t>(S13683Register::CONTROL), ctrlSetting);

  // 露光時間より多く待機
  for (uint64_t i = totalExposureTime;;) {
    if (i > UINT_MAX) {
      delayMicroseconds(UINT_MAX);
      i -= UINT_MAX;
    } else {
      delayMicroseconds(i);
      break;
    }
  }
  delay(1);

  // 結果を取得
  return readColorSensorResult();
}

void AE_S13683_LED::ledDriverConfig(bool enable, LED_CURRENT led_current) {
  if (enable) {
    writeData(CAT3626Address, static_cast<uint8_t>(CAT3626Register::REGEN), 0x03);
  } else {
    writeData(CAT3626Address, static_cast<uint8_t>(CAT3626Register::REGEN), 0x00);
  }
  writeData(CAT3626Address, static_cast<uint8_t>(CAT3626Register::REGA), static_cast<uint8_t>(led_current));
}

AE_S13683_LEDResult AE_S13683_LED::readColorSensorResult() {
  AE_S13683_LEDResult result;
  result.rawRed = readData(S13683Address, static_cast<uint8_t>(S13683Register::RED_DATA_HIGH)) << 8;
  result.rawRed |= readData(S13683Address, static_cast<uint8_t>(S13683Register::RED_DATA_LOW));
  result.rawGreen = readData(S13683Address, static_cast<uint8_t>(S13683Register::GREEN_DATA_HIGH)) << 8;
  result.rawGreen |= readData(S13683Address, static_cast<uint8_t>(S13683Register::GREEN_DATA_LOW));
  result.rawBlue = readData(S13683Address, static_cast<uint8_t>(S13683Register::BLUE_DATA_HIGH)) << 8;
  result.rawBlue |= readData(S13683Address, static_cast<uint8_t>(S13683Register::BLUE_DATA_LOW));
  result.rawCorrection = readData(S13683Address, static_cast<uint8_t>(S13683Register::CORRECTION_DATA_HIGH)) << 8;
  result.rawCorrection |= readData(S13683Address, static_cast<uint8_t>(S13683Register::CORRECTION_DATA_LOW));
  if (result.rawCorrection < result.rawRed) {
    result.red = result.rawRed - result.rawCorrection;
  } else {
    result.red = 0;
  }
  if (result.rawCorrection < result.rawGreen) {
    result.green = result.rawGreen - result.rawCorrection;
  } else {
    result.green = 0;
  }
  if (result.rawCorrection < result.rawBlue) {
    result.blue = result.rawBlue - result.rawCorrection;
  } else {
    result.blue = 0;
  }
  return result;
}

void AE_S13683_LED::writeData(uint8_t add, uint8_t reg, uint8_t data) {
  wire->beginTransmission(add);
  wire->write(reg);
  wire->write(data);
  wire->endTransmission();
}

uint8_t AE_S13683_LED::readData(uint8_t add, uint8_t reg) {
  wire->beginTransmission(add);
  wire->write(reg);
  wire->endTransmission(false);
  wire->requestFrom(add, 1);
  return wire->read();
}
