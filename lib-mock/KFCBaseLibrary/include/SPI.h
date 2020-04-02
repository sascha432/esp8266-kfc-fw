/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdlib.h>
#include <inttypes.h>

#define SPI_HAS_TRANSACTION 1
#define SPI_HAS_NOTUSINGINTERRUPT 1
#define SPI_ATOMIC_VERSION 1
#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#define SPI_CLOCK_DIV4 0x00
#define SPI_CLOCK_DIV16 0x01
#define SPI_CLOCK_DIV64 0x02
#define SPI_CLOCK_DIV128 0x03
#define SPI_CLOCK_DIV2 0x04
#define SPI_CLOCK_DIV8 0x05
#define SPI_CLOCK_DIV32 0x06
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C
#define SPI_MODE_MASK 0x0C  // CPOL = bit 3, CPHA = bit 2 on SPCR
#define SPI_CLOCK_MASK 0x03  // SPR1 = bit 1, SPR0 = bit 0 on SPCR
#define SPI_2XCLOCK_MASK 0x01  // SPI2X = bit 0 on SPSR

// define SPI_AVR_EIMSK for AVR boards with external interrupt pins
#if defined(EIMSK)
  #define SPI_AVR_EIMSK  EIMSK
#elif defined(GICR)
  #define SPI_AVR_EIMSK  GICR
#elif defined(GIMSK)
  #define SPI_AVR_EIMSK  GIMSK
#endif

class SPISettings {
public:
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {}
    SPISettings() {}
private:
    void init_MightInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {}
    void init_AlwaysInline(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {}
    uint8_t spcr;
    uint8_t spsr;
    friend class SPIClass;
};


class SPIClass {
public:
  static void begin() {}
  static void usingInterrupt(uint8_t interruptNumber) {}
  static void notUsingInterrupt(uint8_t interruptNumber) {}
  static void beginTransaction(SPISettings settings) {}
  static uint8_t transfer(uint8_t data) { return 0;  }
  static uint16_t transfer16(uint16_t data) { return 0;  }
  static void transfer(void *buf, size_t count) {}
  static void endTransaction(void) {}

  static void end() {}
  static void setBitOrder() {}
  static void setDataMode() {}
  static void setClockDivider() {}
  static void attachInterrupt() {}
 //static void detachInterrupt() {}

private:
  static uint8_t initialized;
  static uint8_t interruptMode; // 0=none, 1=mask, 2=global
  static uint8_t interruptMask; // which interrupts to mask
  static uint8_t interruptSave; // temp storage, to restore state
  #ifdef SPI_TRANSACTION_MISMATCH_LED
  static uint8_t inTransactionFlag;
  #endif
};

extern SPIClass SPI;
