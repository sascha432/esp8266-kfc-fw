/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "LD24xxSerial.h"

#if IOT_SENSOR_HAVE_LD24XX

// Base class of the HLK LD24xx radar sensors (Sensor_LD2410C, Sensor_LD2420, Sensor_LD2450)
//
// The radars share the frames and the serial port (LD24xxSerial), a build has one of them. This class runs the
// operations from the main loop without blocking: it sends the command of each step, waits for the reply and retries a
// timeout or a garbled reply, searches a module that does not answer on the other baud rates and sets it back to its
// rate (Protocol::baud), prints the monitor and runs the TX test. It reads the configuration after boot and writes a
// saved form when the module is idle.
//
// An operation that talks to the module (read, write, restart, factory reset, baud rate) has these steps
//   0         enable the configuration mode
//   1..n      read and write: the n steps of the sensor (_getStepCount(), _sendStep(), _parseReply())
//   n + 1     read and write: leave the configuration mode
//   1         restart: the command, the module restarts without a reply
//   1, 2      factory reset and baud rate: the command, then the restart that applies it (no reply)
//
// A sensor derives from LD24xxSensorT<Params>, which keeps its configuration, and implements its steps, the decoding of
// its replies and data frames, its forms and the AT subcommands that change the configuration.
//
// The log (serial console, syslog, log file) has the result of every command that the firmware sends by itself (boot
// read, saved form, baud rate change) with the bytes and receive errors of the last attempt.

// 1 = a read that finds the Bluetooth of a module on switches it off (Protocol::bluetoothCommand), the module restarts to
// apply it. Once per boot and module, a new module and a factory reset have Bluetooth on. 0 = Bluetooth is left alone
#ifndef IOT_SENSOR_LD24XX_BLUETOOTH_OFF
#    define IOT_SENSOR_LD24XX_BLUETOOTH_OFF 1
#endif

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <memory>
#include "MQTTSensor.h"

class LD24xxSensor : public MQTT::Sensor {
public:
    // what the radars differ in
    struct Protocol {
        // the name of the AT command and the prefix of its output, e.g. "LD2450" (PROGMEM)
        PGM_P name;
        // the baud rate the module has to use, a module found on another rate is set back to it (hasBaudCommand)
        uint32_t baud;
        // milliseconds after boot before the configuration is read, the module needs a moment after power on
        uint16_t bootReadDelay;
        uint16_t restartCommand;
        // 0 if the module has no factory reset command
        uint16_t factoryCommand;
        // data of the command that enables the configuration mode
        uint8_t enableConfigValue;
        // the module can change its baud rate (command 0x00A1)
        bool hasBaudCommand;
        // command that switches Bluetooth on or off, the module uses it after a restart. 0 if the module has none
        uint16_t bluetoothCommand;
    };

    LD24xxSensor(MQTT::SensorType type, const String &name, const Protocol &protocol);
    virtual ~LD24xxSensor();

    virtual void getStatus(Print &output) override;
    virtual void setup() override;
    virtual void shutdown() override;
    virtual bool hasForm() const override;
    // the web server calls it from the main loop after a form has been saved: writes the values to the module
    virtual void reconfigure(PGM_P source) override;

protected:
    enum class OpType : uint8_t {
        NONE,
        READ,
        WRITE,
        RESTART,
        FACTORY,
        // sets the module to Protocol::baud and restarts it
        BAUD,
        // switches Bluetooth off and restarts the module
        BLUETOOTH,
        MONITOR,
        // the serial port stays open for the live view of the LD2450
        STREAM,
        // continuous pattern on the TX pin to check the wiring
        TX,
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
        // bytes in buffer
        uint8_t length;
        // attempt of the baud rate search (LD24xxSerial::getScanBaud()), 0 is the rate the operation started with
        uint8_t baudIndex;
        // the monitor prints the received bytes
        bool raw;
        // presence and distance a monitor collects from several lines (LD2420 simple mode)
        uint8_t monitorPresence;
        uint16_t monitorDistance;
        uint16_t command;
        // error code of the last reply, retried like a timeout
        uint16_t replyError;
        // the changed values a write sends, the bits are defined by the sensor
        uint32_t writeMask;
        // the baud rate the operation started with
        uint32_t firstBaud;
        uint32_t nextSend;
        uint32_t sent;
        uint32_t maxReplyTime;
        uint32_t timeout;
        uint32_t endTime;
        // millis() of the last monitor line or received byte
        uint32_t lastOutput;
        // bytes that the TX test sent
        uint32_t txBytes;
        Stream *output;
        // the data frame or text line that the monitor collects, the raw dump
        uint8_t buffer[64];
        // what the monitor printed last (_monitorThrottle())
        uint8_t lastKey[24];

        Operation(OpType aType, Stream *aOutput);
        virtual ~Operation() {}
    };

    // --- implemented by LD24xxSensorT
    virtual Operation *_createOperation(OpType type, Stream *output) = 0;
    virtual bool _hasParams() const = 0;
    // a read has been completed
    virtual void _storeParams() = 0;
    virtual bool _hasPendingWrite() const = 0;
    virtual void _writePendingParams() = 0;
    #if AT_MODE_SUPPORTED
        // a subcommand that changes the configuration
        virtual void _atModeWrite(AtModeArgs &args, Stream &output, const String &cmd) = 0;
    #endif

    // --- implemented by the sensor
    // the steps between enabling and leaving the configuration mode of a read or write
    virtual uint8_t _getStepCount(OpType type) const = 0;
    // sends the command of step 0 to _getStepCount() - 1, returns false if the step has nothing to send (a value that
    // did not change)
    virtual bool _sendStep(OpType type, uint8_t step) = 0;
    // validates and stores the reply of a read step. The frames have no checksum: false is a garbled reply, it is retried
    virtual bool _parseReply(uint8_t step, const uint8_t *data, uint8_t length) = 0;
    virtual void _printParams(Print &output) const = 0;
    // decodes a byte that the module reports while the monitor runs, prints with _monitorThrottle()
    virtual void _feedMonitor(uint8_t data) = 0;
    // the status after the firmware version, the configuration if it has been read
    virtual void _printStatus(Print &output) const {}
    // runs while OpType::STREAM is active
    virtual void _loopStream() {}
    // called when an operation has finished
    virtual void _onIdle() {}
    // true if the last read found Bluetooth on (IOT_SENSOR_LD24XX_BLUETOOTH_OFF)
    virtual bool _isBluetoothOn() const { return false; }
    // true if OpType::STREAM runs all the time (LD2450 presence), the status does not list it
    virtual bool _keepsStream() const { return false; }

    // --- for the sensor
    // returns false if an operation is running or the memory is too low. The live view gives way (type is not STREAM)
    bool _start(OpType type, Stream *output);
    // ends the operation, an error is printed and leaves the configuration mode
    void _finish(PGM_P error = nullptr);
    // ends the operation without calling _onIdle(): another one follows
    void _close();
    void _sendFrame(uint16_t command, const uint8_t *data = nullptr, uint8_t length = 0);
    // collects a data frame in _op->buffer: the 4 byte header, then frameLength bytes or (frameLength 0) the length
    // field after the header + 10 bytes. Returns true when a complete frame with the footer is in the buffer
    bool _feedFrame(uint8_t data, const uint8_t *header, const uint8_t *footer, uint8_t footerLength, uint8_t frameLength);
    // the modules report 10-20 frames per second: a change of key is printed every 500ms at most, a change of the first
    // urgentLength bytes (the presence) at once and the state once per second. Returns true if the frame is printed
    bool _monitorThrottle(const uint8_t *key, uint8_t length, uint8_t urgentLength = 0);
    // the version in the reply of the LD2410/LD2450 (command 0x00A0), returns false if the reply is too short
    bool _parseVersion(const uint8_t *data, uint8_t length);
    // prints "+<name>: "
    Print &_prefix(Print &output) const;
    // title of the card of a form: the name and the firmware version, or "not responding"
    String _getFormTitle() const;
    // a row of a gate form: the move value with the still value as a second input, the label is the distance of the gate
    template<typename _Type>
    static void _addGateField(FormUI::Form::BaseForm &form, uint8_t gate, uint8_t gateDistance, const __FlashStringHelper *moveName, _Type &move, const __FlashStringHelper *stillName, _Type &still, _Type maxValue);
    static PGM_P _getOpTypeStr(OpType type);

    #if AT_MODE_SUPPORTED
        // read, restart, factory, mon and tx, the other subcommands are passed to _atModeWrite()
        void _atModeHandler(AtModeArgs &args);
        // the index of a gate or an area in argument num. Not clamped like a value: an index out of range would change
        // another gate, it prints an error and returns -1
        static int16_t _atModeIndex(AtModeArgs &args, uint16_t num, uint8_t maxIndex, const __FlashStringHelper *name);
    #endif

private:
    void _openSerial();
    void _loop();
    void _loopCommand();
    void _loopMonitor();
    void _loopTx();
    // sends the frame of the current step, returns false if the step has nothing to send
    bool _sendNextStep();
    uint8_t _getLastStep() const;
    bool _handleReply();
    void _complete();
    void _applyPendingWrite();
    void _printMonitorRaw();
    // true while the first command is sent on the other baud rates
    bool _isBaudSearch() const;
    static bool _isCommand(OpType type);
    // after a read that found the module on another rate: sets it back to Protocol::baud, returns true if the change
    // has been started
    bool _restoreBaud(Stream *output);
    // after a read that found Bluetooth on: switches it off, once per boot
    void _switchBluetoothOff(Stream *output);
    // logs the result of an operation that the firmware started by itself (no output)
    void _logResult(PGM_P error);
    // takes the serial port from the live view, which resumes when the port is free again. A command never waits for it
    // or answers "busy"
    void _preemptStream();

protected:
    String _name;
    String _version;
    std::unique_ptr<LD24xxSerial> _serial;
    std::unique_ptr<Operation> _op;
    Protocol _protocol;
    // the baud rate the module answered at
    uint32_t _baud;

private:
    // repeats the boot read after a read that failed
    Event::Timer _readTimer;
    // a saved form waits until the running command has finished
    Event::Timer _writeTimer;
    // reads that the boot read has started
    uint8_t _bootReads;
    // 0 = Bluetooth has not been switched off in this boot, 1 = switched off, 2 = still on afterwards (logged)
    uint8_t _bluetoothOff;
};

inline bool LD24xxSensor::hasForm() const
{
    return true;
}

inline bool LD24xxSensor::_isBaudSearch() const
{
    return _op->step == 0 && _op->baudIndex != 0;
}

inline bool LD24xxSensor::_isCommand(OpType type)
{
    return type == OpType::READ || type == OpType::WRITE || type == OpType::RESTART || type == OpType::FACTORY || type == OpType::BAUD || type == OpType::BLUETOOTH;
}

// The configuration of a sensor: _params is what the module reported, _formParams the values of the forms (a form that
// is still rendered points to it), _pendingWrite a saved form that is written when the module is idle. A write sends
// the values that changed (_getWriteMask()) and reads the configuration back
template<typename _ParamsType>
class LD24xxSensorT : public LD24xxSensor {
public:
    using ParamsType = _ParamsType;

    LD24xxSensorT(MQTT::SensorType type, const String &name, const Protocol &protocol) :
        LD24xxSensor(type, name, protocol),
        _params(),
        _formParams(),
        _pendingWrite()
    {
    }

    // called in the web server context: the form was created in the same request, if _params exists, the form has the
    // fields and _formParams their values. reconfigure() writes them
    virtual void configurationSaved(FormUI::Form::BaseForm *form) override
    {
        if (!_params || !_formParams) {
            return;
        }
        if (!_pendingWrite) {
            _pendingWrite.reset(new ParamsType());
        }
        *_pendingWrite = *_formParams;
    }

protected:
    struct Operation : LD24xxSensor::Operation {
        // the values that a read collects or a write sends
        ParamsType pending;

        Operation(OpType aType, Stream *aOutput) :
            LD24xxSensor::Operation(aType, aOutput),
            pending()
        {
        }
    };

    // the values that differ from _params, a bit per write step of the sensor. 0 if nothing changed
    virtual uint32_t _getWriteMask(const ParamsType &params) const = 0;

    #if AT_MODE_SUPPORTED
        // changes params for a subcommand, returns false if it printed an error
        virtual bool _atModeParams(AtModeArgs &args, const String &cmd, ParamsType &params) = 0;
    #endif

    Operation &_getOp() const
    {
        return static_cast<Operation &>(*_op);
    }

    // the values the forms show: a saved form that has not been written yet, the one being written or the ones read
    // from the module. nullptr if the configuration has not been read
    ParamsType *_getFormParams()
    {
        if (!_params) {
            return nullptr;
        }
        if (!_formParams) {
            _formParams.reset(new ParamsType());
        }
        if (_pendingWrite) {
            *_formParams = *_pendingWrite;
        }
        else if (_op && _op->type == OpType::WRITE) {
            *_formParams = _getOp().pending;
        }
        else {
            *_formParams = *_params;
        }
        return _formParams.get();
    }

    // writes the values that changed, prints "no changes" or an error to output if nothing gets written
    void _queueWrite(const ParamsType &params, Stream *output)
    {
        auto mask = _getWriteMask(params);
        if (mask == 0) {
            if (output) {
                _prefix(*output).println(F("no changes"));
            }
            return;
        }
        if (!_start(OpType::WRITE, output)) {
            if (output) {
                _prefix(*output).println(F("ERROR - not enough memory"));
            }
            return;
        }
        auto &op = _getOp();
        op.pending = params;
        op.writeMask = mask;
    }

    virtual LD24xxSensor::Operation *_createOperation(OpType type, Stream *output) override
    {
        return new Operation(type, output);
    }

    virtual bool _hasParams() const override
    {
        return static_cast<bool>(_params);
    }

    virtual void _storeParams() override
    {
        if (!_params) {
            _params.reset(new ParamsType());
        }
        *_params = _getOp().pending;
    }

    virtual bool _hasPendingWrite() const override
    {
        return static_cast<bool>(_pendingWrite);
    }

    virtual void _writePendingParams() override
    {
        auto params = std::move(_pendingWrite);
        _queueWrite(*params, nullptr);
    }

    #if AT_MODE_SUPPORTED
        virtual void _atModeWrite(AtModeArgs &args, Stream &output, const String &cmd) override
        {
            auto params = *_params;
            if (_atModeParams(args, cmd, params)) {
                _queueWrite(params, &output);
            }
        }
    #endif

protected:
    std::unique_ptr<ParamsType> _params;
    std::unique_ptr<ParamsType> _formParams;
    std::unique_ptr<ParamsType> _pendingWrite;
};

#endif
