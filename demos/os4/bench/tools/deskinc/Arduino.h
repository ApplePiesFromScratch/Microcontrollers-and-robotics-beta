// desk stand-in: only what the sketches touch, read the way simavr's bench reads it (ADC at mid rail = 511, buttons up)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define A0 14
#define A1 15
#define INPUT_PULLUP 2
static inline int analogRead(uint8_t) { return 511; }
static inline int digitalRead(uint8_t) { return 1; }
static inline void pinMode(uint8_t, uint8_t) {}
