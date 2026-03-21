#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#ifndef _VARIANT_RAK11200_
#define _VARIANT_RAK11200_
#endif

#include <stdint.h>

// #ifndef EXTERNAL_NUM_INTERRUPTS
// #define EXTERNAL_NUM_INTERRUPTS 16
// #endif
// #ifndef NUM_DIGITAL_PINS
// #define NUM_DIGITAL_PINS 40
// #endif
// #ifndef NUM_ANALOG_INPUTS
// #define NUM_ANALOG_INPUTS 16
// #endif
// #ifndef analogInputToDigitalPin
// #define analogInputToDigitalPin(p) (((p) < 20) ? (esp32_adc2gpio[(p)]) : -1)
// #endif
// #ifndef digitalPinToInterrupt
// #define digitalPinToInterrupt(p) (((p) < 40) ? (p) : -1)
// #endif
// #ifndef digitalPinHasPWM
// #define digitalPinHasPWM(p) (p < 34)
// #endif

#define LED_GREEN              12
#define LED_BLUE             2

#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif
#define LED_BUILTIN          LED_GREEN

static const uint8_t TX = 1;
static const uint8_t RX = 3;

#define TX1 21
#define RX1 19

#define WB_IO1 14
#define WB_IO2 27
#define WB_IO3 26
#define WB_IO4 23
#define WB_IO5 13
#define WB_IO6 22
#define WB_SW1 34
#define WB_A0 36
#define WB_A1 39
#define WB_CS 32
#define WB_LED1 12
#define WB_LED2 2

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 32;
static const uint8_t MOSI = 25;
static const uint8_t MISO = 35;
static const uint8_t SCK = 33;

#endif /* Pins_Arduino_h */
