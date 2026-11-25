#pragma once
#include <stdint.h>
struct EEPROMClass { bool begin(size_t size); uint8_t read(int addr); void write(int addr, uint8_t v); bool commit(void); };
extern EEPROMClass EEPROM;
