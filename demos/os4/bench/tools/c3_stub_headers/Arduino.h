#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define MSBFIRST 1
#define SPI_MODE0 0
#define portMAX_DELAY 0xFFFFFFFFu
struct HwSerial { void begin(unsigned long); int printf(const char *, ...) __attribute__((format(printf, 2, 3))); };
extern HwSerial Serial, Serial0;
struct EspClass { uint32_t getFreeHeap(); }; extern EspClass ESP;
void delay(uint32_t); void delayMicroseconds(uint32_t); unsigned long micros(void); unsigned long millis(void);
void pinMode(uint8_t, uint8_t); int digitalRead(uint8_t); void digitalWrite(uint8_t, uint8_t); int analogRead(uint8_t); void analogReadResolution(int);
unsigned uxTaskGetStackHighWaterMark(void *);
