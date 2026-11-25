#pragma once
#include <stdint.h>
struct EEPROMClass { bool begin(size_t); uint8_t read(int); void write(int, uint8_t); bool commit(); }; extern EEPROMClass EEPROM;
