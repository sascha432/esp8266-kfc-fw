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
// The serial port and the command frames are LD24xxSerial: OT1 (sensor TX) on GPIO13, RX of the sensor on GPIO2, the
// port is only open while a command runs. All commands run from the main loop without blocking.
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2420

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <memory>
#include "MQTTSensor.h"
#include "LD24xxSerial.h"

// firmware 1.5.3 and later uses 115200 baud, older versions 256000 baud
#ifndef IOT_SENSOR_LD2420_BAUD
#    define IOT_SENSOR_LD2420_BAUD 115200
#endif

class Sensor_LD2420 : public MQTT::Sensor {
public:
    static constexpr uint8_t kGateCount = 16;
    static constexpr uint8_t kMaxGate = kGateCount - 1;
    static constexpr uint32_t kMaxThreshold = 0xffff;
    static constexpr uint32_t kMaxTimeout = 0xffff;

    struct Params {
        uint32_t minGate;
        uint32_t maxGate;
        uint32_t timeout;
        uint32_t move[kGateCount];
        uint32_t still[kGateCount];
    };

public:
    Sensor_LD2420(const String &name);
    virtual ~Sensor_LD2420();

    virtual void publishState() override {}
    virtual void getValues(WebUINS::Events &array, bool timer) override {}
    virtual void createWebUI(WebUINS::Root &webUI) override {}
    virtual void getStatus(Print &output) override;

    virtual void setup() override;
    virtual void shutdown() override;

    // the card of /sensor.html: detection range and presence timeout
    virtual bool hasForm() const override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    // stores the values of either form, called in the web server context. reconfigure() writes them to the sensor
    virtual void configurationSaved(FormUI::Form::BaseForm *form) override;
    virtual void reconfigure(PGM_P source) override;

    // the form of /ld2420.html: move/still thresholds of the gates
    void createGateForm(FormUI::Form::BaseForm &form);

    #if AT_MODE_SUPPORTED
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

private:
    enum class OpType : uint8_t {
        NONE,
        READ,
        WRITE,
        RESTART,
        MONITOR,
    };

    enum class StateType : uint8_t {
        SEND,
        WAIT_REPLY,
    };

    // the context of the running operation, allocated while it runs
    struct Operation {
        OpType type;
        StateType state;
        uint8_t step;
        uint8_t retries;
        uint8_t totalRetries;
        // bytes in the monitor buffer
        uint8_t length;
        bool baudFallback;
        bool raw;
        uint16_t command;
        // error code of the last reply, retried like a timeout
        uint16_t replyError;
        uint32_t nextSend;
        uint32_t sent;
        uint32_t maxReplyTime;
        uint32_t timeout;
        uint32_t endTime;
        uint32_t lastOutput;
        uint32_t writeMask;
        Stream *output;
        uint8_t presence;
        uint16_t distance;
        uint16_t lastDistance;
        Params pending;
        // text line or energy frame of the monitor
        uint8_t buffer[64];

        Operation(OpType aType, Stream *aOutput) :
            type(aType),
            state(StateType::SEND),
            step(0),
            retries(0),
            totalRetries(0),
            length(0),
            baudFallback(false),
            raw(false),
            command(0),
            replyError(0),
            nextSend(millis()),
            sent(0),
            maxReplyTime(0),
            timeout(0),
            endTime(0),
            lastOutput(0),
            writeMask(0),
            output(aOutput),
            presence(0xff),
            distance(0),
            lastDistance(0),
            pending(),
            buffer()
        {
        }
    };

    bool _start(OpType type, Stream *output);
    void _openSerial();
    void _complete();
    void _finish(PGM_P error = nullptr);
    void _loop();
    void _loopMonitor();
    bool _sendStep();
    bool _handleReply();
    void _sendFrame(uint16_t command, const uint8_t *data, uint8_t length);
    void _sendReadParams(const uint16_t *registers, uint8_t count);
    void _sendWriteParams(const uint16_t *registers, const uint32_t *values, uint8_t count);
    void _printParams(Print &output) const;
    void _printMonitorFrame(Print &output);
    void _printMonitorRaw(Print &output);
    void _queueWrite(const Params &params, Stream *output);
    Params *_getFormParams();
    void _applyPendingWrite();

    static uint32_t _getFactoryMove(uint8_t gate);
    static uint32_t _getFactoryStill(uint8_t gate);

private:
    String _name;
    String _version;
    std::unique_ptr<LD24xxSerial> _serial;
    std::unique_ptr<Operation> _op;
    std::unique_ptr<Params> _params;
    // the values of the forms, allocated by the first form and kept: a form that is still rendered points to it
    std::unique_ptr<Params> _formParams;
    // the values of a saved form, written from the main loop when the sensor is idle
    std::unique_ptr<Params> _pendingWrite;
    Event::Timer _timer;
    uint32_t _baud;
};

inline bool Sensor_LD2420::hasForm() const
{
    return true;
}

#endif
