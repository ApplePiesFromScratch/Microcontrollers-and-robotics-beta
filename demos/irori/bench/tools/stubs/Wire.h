#pragma once
#include <stdint.h>
struct TwoWire { bool begin(int sda, int scl, uint32_t freq = 0); bool end(void); void setTimeOut(uint16_t ms); void beginTransmission(uint8_t addr); uint8_t endTransmission(bool stop = true);
  uint8_t requestFrom(uint8_t addr, uint8_t n); int read(void); int available(void); };
extern TwoWire Wire;
