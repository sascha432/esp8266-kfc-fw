/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR && IOT_SENSOR_HAVE_HLW8032

#include <Arduino_compat.h>
#include <SoftwareSerial.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "Sensor_HLW80xx.h"

// pin configuration
#ifndef IOT_SENSOR_HLW8032_RX
#define IOT_SENSOR_HLW8032_RX                   14
#endif

#ifndef IOT_SENSOR_HLW8032_TX
#define IOT_SENSOR_HLW8032_TX                   16
#endif

// 0 to disable
#ifndef IOT_SENSOR_HLW8032_PF
#define IOT_SENSOR_HLW8032_PF                   12
#endif

#ifndef IOT_SENSOR_HLW8032_SERIAL_INTERVAL
#define IOT_SENSOR_HLW8032_SERIAL_INTERVAL      50
#endif

#define IOT_SENSOR_HLW8032_VALUE_TO_INT(value)  ((value.high << 16) | (value.middle << 8) | value.low)
#define IOT_SENSOR_HLW8032_WORD_TO_INT(value)   ((value.high << 8) | value.low)

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

    void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;

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
