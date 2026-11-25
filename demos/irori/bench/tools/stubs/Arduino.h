#pragma once
#include <stdint.h>
#include <sys/param.h>   /* the real core pulls this in (through riscv/csr.h): HZ is 60 there */
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
struct HwSerial { void begin(unsigned long); void print(const char *); void print(char); int available(void); int read(void); int printf(const char *, ...) __attribute__((format(printf, 2, 3))); };
extern HwSerial Serial, Serial0;
struct EspClass { uint32_t getFreeHeap(); }; extern EspClass ESP;
void delay(uint32_t); void delayMicroseconds(uint32_t); unsigned long micros(void); unsigned long millis(void);
void pinMode(uint8_t, uint8_t); int digitalRead(uint8_t); void digitalWrite(uint8_t, uint8_t); int analogRead(uint8_t); void analogReadResolution(int);
unsigned uxTaskGetStackHighWaterMark(void *);
/* names the real core puts in every sketch's scope (pins_arduino.h for the esp32c3 variant, Arduino.h, binary.h):
   declared here so a sketch that reuses one fails here as it fails in the IDE. Added after CANDLE's RX met the core's RX. */
static const uint8_t TX = 21; static const uint8_t RX = 20;
static const uint8_t SDA = 8; static const uint8_t SCL = 9;
static const uint8_t SS = 7; static const uint8_t MOSI = 6; static const uint8_t MISO = 5; static const uint8_t SCK = 4;
static const uint8_t A0 = 0, A1 = 1, A2 = 2, A3 = 3, A4 = 4, A5 = 5;
static const uint8_t LED_BUILTIN = 8;
#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352
#define SERIAL 0x0
#define DISPLAY 0x1
#define LSBFIRST 0
#define CHANGE 0x03
#define FALLING 0x02
#define RISING 0x01
#define INPUT 0x01
#define bit(b) (1UL << (b))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define sq(x) ((x) * (x))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define F(s) (s)
#define B0 0
#define B1 1
#define B10 2
#define B11 3
#define B100 4
#define B101 5
#define B110 6
#define B111 7
bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution); bool ledcWrite(uint8_t pin, uint32_t duty); bool ledcDetach(uint8_t pin);
#define RTC_NOINIT_ATTR
typedef struct { uint32_t owner; uint32_t count; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED { 0, 0 }
void vPortEnterCritical(portMUX_TYPE *); void vPortExitCritical(portMUX_TYPE *);
#define portENTER_CRITICAL(m) vPortEnterCritical(m)
#define portEXIT_CRITICAL(m) vPortExitCritical(m)
#define portENTER_CRITICAL_ISR(m) vPortEnterCritical(m)
#define portEXIT_CRITICAL_ISR(m) vPortExitCritical(m)
void attachInterruptArg(uint8_t pin, void (*)(void *), void *arg, int mode); void detachInterrupt(uint8_t pin);
void enableLoopWDT(void);
