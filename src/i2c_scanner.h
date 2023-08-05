 /**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <HardwareSerial.h>
#if DEBUG
#    include "serial_handler.h"
#endif

#define I2C_SCANNER_LIST_PIN_PAIRS 0xffff

#define I2C_SCANNER_SKIP_GPIO(i) (__isPinSerialDebug(i) || isFlashInterfacePin(i))

#if ESP8266

    // defined in framework-arduinoespressif8266/cores/esp8266/uart.cpp
    struct uart_
    {
        int uart_nr;
        int baud_rate;
        bool rx_enabled;
        bool tx_enabled;
        bool rx_overrun;
        bool rx_error;
        uint8_t rx_pin;
        uint8_t tx_pin;
        struct uart_rx_buffer_ * rx_buffer;
    };

    class HardwareSerialEx : public HardwareSerial
    {
    public:
        bool isPinUsed(uint8_t pin) const
        {
            return _uart && ((_uart->rx_enabled && _uart->rx_pin == pin) || (_uart->tx_enabled && _uart->tx_pin == pin));
        }
    };

#else

    // TODO check which pins are used

    class HardwareSerialEx : public HardwareSerial
    {
    public:
        bool isPinUsed(uint8_t pin) const
        {
            return pin == 1 || pin == 3;
        }
    };

#endif

void check_if_exist_I2C(TwoWire &wire, Print &output, uint8_t startAddress, uint8_t endAddress, uint32_t delayMillis = 10)
{
    wire.setClock(100000);
    int nDevices = 0;
    for (auto address = startAddress; address < endAddress; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmission to see if
        // a device did acknowledge to the address.
        wire.beginTransmission(address);
        uint8_t error = wire.endTransmission(true);
        if (error == 0 || error == 3) { // 0=ACK, 3=NACK for data
            output.printf_P(PSTR("I2C device found at address 0x%02x (%u)\n"), address, error);
            nDevices++;
        }
        else if (error == 4) {
            output.printf_P(PSTR("Unknown error at address 0x%02x\n"), address);
        }
        if (delayMillis) {
            delay(delayMillis);
        }
    }
    if (nDevices == 0) {
        output.println(F("No I2C devices found"));
    }
}

void scanI2C(Print &output, int8_t sda, int8_t scl, uint8_t startAddress, uint8_t endAddress)
{
    TwoWire *wire;
    if (sda == 0xff || scl == 0xff || sda == scl || isFlashInterfacePin(sda) || isFlashInterfacePin(scl)) {
        output.printf_P(PSTR("Scanning (SDA : SCL) - "));
        wire = &config.initTwoWire(false, &output);
    }
    else {
        output.printf_P(PSTR("Scanning (SDA : SCL) - GPIO%u : GPIO%u - "), sda, scl);
        Wire.begin(sda, scl);
        wire = &Wire;
    }
    check_if_exist_I2C(*wire, output, startAddress, endAddress);
    config.initTwoWire(true);
}

inline static bool __isPinSerial(const HardwareSerial &serial, uint8_t pin)
{
    return reinterpret_cast<const HardwareSerialEx &>(serial).isPinUsed(pin);
}

inline static bool __isPinSerialDebug(uint8_t pin)
{
    #if DEBUG
        #if KFC_DEBUG_USE_SERIAL1
             return __isPinSerial(Serial1, pin);
        #else
            if __CONSTEXPR17(std::is_base_of_v<HardwareSerial, decltype(Serial)>) {
                return __isPinSerial(*reinterpret_cast<HardwareSerial *>(&Serial), pin);
            }
            else {
                return __isPinSerial(Serial0, pin);
            }
        #endif
    #else
        return false;
    #endif
}

void scanPorts(Print &output, uint16_t startAddress, uint8_t endAddress)
{
    const bool listPinsPairs = (startAddress == I2C_SCANNER_LIST_PIN_PAIRS);
    if (listPinsPairs) {
        output.print(F("PIN pairs...\nSDA:SCL\nExclude pins "));
        for (int i = 0; i <= GPIO_PIN_COUNT; i++) {
            if (I2C_SCANNER_SKIP_GPIO(i)) {
                output.print(i);
                output.print(' ');
            }
        }
        output.println();
    }
    else {
        output.println(F("\nScanning all PIN pairs..."));
    }

    for (uint8_t sda = 0; sda <= GPIO_PIN_COUNT; sda++) {
        if (I2C_SCANNER_SKIP_GPIO(sda)) {
            continue;
        }
        for (uint8_t scl = 0; scl <= GPIO_PIN_COUNT; scl++) {
            if (sda == scl || I2C_SCANNER_SKIP_GPIO(scl)) {
                continue;
            }
            if (listPinsPairs) {
                output.printf_P(PSTR("%u:%u\n"), sda, scl);
            }
            else {
                scanI2C(output, sda, scl, startAddress, endAddress);
            }
        }
    }
}
