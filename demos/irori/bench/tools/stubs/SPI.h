#pragma once
#include <stdint.h>
struct SPISettings { SPISettings(uint32_t, uint8_t, uint8_t) {} };
struct SPIClass { void begin(int8_t, int8_t, int8_t, int8_t); void beginTransaction(SPISettings); void write(uint8_t); void writeBytes(const uint8_t *, uint32_t); };
extern SPIClass SPI;
