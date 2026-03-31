#pragma once

#include "pico/stdlib.h"
#include "hardware/spi.h"

// Pin definitions — must match your hardware
#define RST_PIN     9
#define DC_PIN      10
#define CS_PIN      11
#define BUSY_PIN    8
#define PWR_PIN     6

class EpdIf {
public:
    EpdIf() {}
    ~EpdIf() {}

    static int  IfInit(void);
    static void DigitalWrite(int pin, int value);
    static int  DigitalRead(int pin);
    static void DelayMs(unsigned int ms);
    static void SpiTransfer(unsigned char data);
};

// Arduino compatibility shims for RP2040 / Pico SDK
#ifndef HIGH
#define HIGH 1
#define LOW  0
#endif
#ifndef COLORED
#define COLORED   0
#define UNCOLORED 1
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#endif
