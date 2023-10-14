/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_HLW8032

// Software serial for the ESP8266 that uses edge triggered interrupts to improve performance. ...
// leading to signficant performance improvements when used on serial data ports running at speeds of 9600 baud or less.
// Note that the slower the data rate, the more useful this driver becomes. For high speeds it doesn't confer as much of an advantage.
// https://github.com/CrazyRobMiles/Esp8266EdgeSoftwareSerial

// HLW8032 has a simple UART interface and adopts asynchronous serial communication
// mode, which allows data communication via two one-way pins. The UART interface can realize
// isolated communication by only a low-cost photoelectric coupler. The UART interface operates
// at fixed frequency of 4800 bps and its interval for transmitting data is 50mS, which is suitable
// for design of low velocity
// https://datasheet.lcsc.com/lcsc/1811151523_Hiliwei-Tech-HLW8032_C128023.pdf

// https://github.com/Khaalidi/HLW8032

#error this sensor is not implemented

#include <Arduino_compat.h>
#include <SoftwareSerial.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "Sensor_HLW80xx.h"

// pin configuration
#ifndef IOT_SENSOR_HLW8032_RX
#    define IOT_SENSOR_HLW8032_RX 14
#endif

#ifndef IOT_SENSOR_HLW8032_TX
#    define IOT_SENSOR_HLW8032_TX 16
#endif

// 0 to disable
#ifndef IOT_SENSOR_HLW8032_PF
#    define IOT_SENSOR_HLW8032_PF 12
#endif

#ifndef IOT_SENSOR_HLW8032_SERIAL_INTERVAL
#    define IOT_SENSOR_HLW8032_SERIAL_INTERVAL 50
#endif

#define IOT_SENSOR_HLW8032_VALUE_TO_INT(value) ((value.high << 16) | (value.middle << 8) | value.low)
#define IOT_SENSOR_HLW8032_WORD_TO_INT(value)  ((value.high << 8) | value.low)

typedef struct {
    uint8_t high;
    uint8_t middle;
    uint8_t low;
} HLW8032SerialValue_t;

typedef struct {
    union {
        struct {
            uint8_t error: 1;
            uint8_t powerOverflow: 1;
            uint8_t currentOverflow: 1;
            uint8_t voltageOverflow: 1;
            uint8_t __reserved: 4;
        } status;
        uint8_t statusByte;
    };
    uint8_t detection;
    HLW8032SerialValue_t voltageParameter;
    HLW8032SerialValue_t voltageRegister;
    HLW8032SerialValue_t currentParameter;
    HLW8032SerialValue_t currentRegister;
    HLW8032SerialValue_t powerParameter;
    HLW8032SerialValue_t powerRegister;
    struct {
        uint8_t __reserved: 4;
        uint8_t powerFlag : 1;
        uint8_t currentFlag : 1;
        uint8_t voltageFlag : 1;
        uint8_t PFOverflow : 1;
    } dataUpdate;
    struct {
        uint8_t high;
        uint8_t low;
    } PF;
    uint8_t checksum;
} HLW8032SerialData_t;


class Sensor_HLW8032 : public Sensor_HLW80xx {
public:
    Sensor_HLW8032(const String &name, uint8_t pinRx, uint8_t pinTx, uint8_t pinPF);
    virtual ~Sensor_HLW8032();

    void getStatus(Print &output) override;

    static void loop();

    virtual String _getId(const __FlashStringHelper *type = nullptr);

private:
    void _onReceive(int available);

    uint8_t _pinPF;
    SoftwareSerial _serial;
    Buffer _buffer;
    unsigned long _lastData;
};

#endif
