/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_LD2420

// HLK-LD2420 24 GHz radar presence sensor
//
// The presence output OT2 is read by Sensor_Motion. This class talks to the UART of the sensor to read and write the
// configuration that the sensor keeps in its own flash: the detection range (min/max gate), the presence timeout and
// the move/still thresholds of the 16 gates.
//
// The commands and the serial port are LD24xxSensor, the UART pins are listed in LD24xxSerial.h. The module has no
// baud rate command, the rate depends on its firmware version.
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2420

#include <Arduino_compat.h>
#include "LD24xxSensor.h"

// firmware 1.5.3 and later uses 115200 baud, older versions 256000 baud (found by the baud rate search)
#ifndef IOT_SENSOR_LD2420_BAUD
#    define IOT_SENSOR_LD2420_BAUD 115200
#endif

namespace LD2420 {

    static constexpr uint8_t kGateCount = 16;

    struct Params {
        uint32_t minGate;
        uint32_t maxGate;
        uint32_t timeout;
        uint32_t move[kGateCount];
        uint32_t still[kGateCount];

        Params() :
            minGate(0),
            maxGate(0),
            timeout(0),
            move(),
            still()
        {
        }
    };

}

class Sensor_LD2420 : public LD24xxSensorT<LD2420::Params> {
public:
    static constexpr uint8_t kGateCount = LD2420::kGateCount;
    static constexpr uint8_t kMaxGate = kGateCount - 1;
    static constexpr uint32_t kMaxThreshold = 0xffff;
    static constexpr uint32_t kMaxTimeout = 0xffff;
    // distance of one gate in cm
    static constexpr uint8_t kGateDistance = 70;

    using Params = LD2420::Params;

public:
    Sensor_LD2420(const String &name);
    virtual ~Sensor_LD2420();

    virtual void publishState() override {}
    virtual void getValues(WebUINS::Events &array, bool timer) override {}
    virtual void createWebUI(WebUINS::Root &webUI) override {}

    // the card of /sensor.html: detection range and presence timeout
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    // the form of /ld2420.html: move/still thresholds of the gates
    void createGateForm(FormUI::Form::BaseForm &form);

    #if AT_MODE_SUPPORTED
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

protected:
    virtual uint8_t _getStepCount(OpType type) const override;
    virtual bool _sendStep(OpType type, uint8_t step) override;
    virtual bool _parseReply(uint8_t step, const uint8_t *data, uint8_t length) override;
    virtual void _printParams(Print &output) const override;
    virtual void _printStatus(Print &output) const override;
    virtual void _feedMonitor(uint8_t data) override;
    virtual uint32_t _getWriteMask(const Params &params) const override;
    #if AT_MODE_SUPPORTED
        virtual bool _atModeParams(AtModeArgs &args, const String &cmd, Params &params) override;
    #endif

private:
    void _sendReadParams(const uint16_t *registers, uint8_t count);
    void _sendWriteParams(const uint16_t *registers, const uint32_t *values, uint8_t count);
    // a text line of the simple mode in _op->buffer
    void _printMonitorLine();

    static uint32_t _getFactoryMove(uint8_t gate);
    static uint32_t _getFactoryStill(uint8_t gate);
};

#endif
