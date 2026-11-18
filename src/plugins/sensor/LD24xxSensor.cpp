/**
 * Author: sascha_lammers@gmx.de
 */

#include "LD24xxSensor.h"

#if IOT_SENSOR_HAVE_LD24XX

#include <algorithm>
#include <LoopFunctions.h>
#include "logger.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    // the modules reply within about 50ms
    constexpr uint32_t kReplyTimeout = 300;
    constexpr uint8_t kMaxRetries = 3;
    // the other baud rates of the search: the reply arrives within 50ms at 9600 baud
    constexpr uint32_t kScanReplyTimeout = 150;
    constexpr uint8_t kScanRetries = 2;
    // pause between a reply and the next command
    constexpr uint32_t kCommandGap = 10;
    constexpr uint32_t kMonitorOutputInterval = 500;
    constexpr uint32_t kMonitorRepeatInterval = 1000;
    // a partial line of the raw dump is printed after this pause
    constexpr uint32_t kMonitorRawPause = 20;
    // a saved form waits for the running command in these steps
    constexpr uint32_t kPendingWriteRetry = 250;
    // reads after boot until a module that does not answer is given up
    constexpr uint8_t kBootReadAttempts = 3;
    // a module that restarts to apply a setting answers again after this time
    constexpr uint32_t kRestartDelay = 2000;

    // the version string that the LD2410/LD2450 report follows two bytes in the reply
    constexpr uint8_t kVersionOffset = 2;
    constexpr uint8_t kVersionLength = 6;

    // prints the distance of a gate in meters with one decimal
    void printGateDistance(Print &output, uint8_t gate, uint8_t gateDistance)
    {
        auto decimeters = gate * gateDistance / 10;
        output.printf_P(PSTR("%u.%u"), decimeters / 10, decimeters % 10);
    }

}

LD24xxSensor::Operation::Operation(OpType aType, Stream *aOutput) :
    type(aType),
    state(StateType::SEND),
    step(0),
    retries(0),
    totalRetries(0),
    length(0),
    baudIndex(0),
    raw(false),
    monitorPresence(0xff),
    monitorDistance(0),
    command(0),
    replyError(0),
    writeMask(0),
    firstBaud(0),
    nextSend(millis()),
    sent(0),
    maxReplyTime(0),
    timeout(0),
    endTime(0),
    lastOutput(0),
    txBytes(0),
    output(aOutput),
    buffer(),
    lastKey()
{
}

LD24xxSensor::LD24xxSensor(MQTT::SensorType type, const String &name, const Protocol &protocol) :
    MQTT::Sensor(type),
    _name(name),
    _version(),
    _serial(),
    _op(),
    _protocol(protocol),
    _baud(protocol.baud),
    _readTimer(),
    _writeTimer(),
    _bootReads(0),
    _bluetoothOff(0)
{
}

LD24xxSensor::~LD24xxSensor()
{
    LD24xxSensor::shutdown();
}

void LD24xxSensor::setup()
{
    // the read is repeated after a read that failed: the main loop can be blocked for a second while WiFi connects, the
    // reply gets lost. A module that does not answer is given up after kBootReadAttempts, every attempt pauses the
    // serial console for seconds
    _Timer(_readTimer).add(Event::milliseconds(_protocol.bootReadDelay), true, [this](Event::CallbackTimerPtr) {
        if (_hasParams() || _bootReads >= kBootReadAttempts) {
            _Timer(_readTimer).remove();
            return;
        }
        if (_op && _op->type == OpType::STREAM) {
            // a page of the live view connected before the read and needs the configuration, the stream restarts after it
            _close();
        }
        if (!_op && _start(OpType::READ, nullptr)) {
            _bootReads++;
        }
    });
}

void LD24xxSensor::shutdown()
{
    _Timer(_readTimer).remove();
    _Timer(_writeTimer).remove();
    if (_op) {
        LoopFunctions::remove(this);
        _op.reset();
    }
    _serial.reset();
}

void LD24xxSensor::reconfigure(PGM_P source)
{
    // the serial port and the loop function must not be created in the web server context
    _applyPendingWrite();
}

void LD24xxSensor::getStatus(Print &output)
{
    output.print(_name);
    if (_hasParams()) {
        output.printf_P(PSTR(" firmware %s"), _version.c_str());
    }
    else {
        output.print((_op && _op->type == OpType::READ) ? F(" reading the configuration") : F(" not responding"));
    }
    if (_baud != _protocol.baud) {
        // found on another baud rate, the module could not be set back
        output.printf_P(PSTR(", %u baud"), static_cast<unsigned>(_baud));
    }
    _printStatus(output);
    if (_op && !_isCommand(_op->type) && !(_op->type == OpType::STREAM && _keepsStream())) {
        // the serial console is paused while the monitor or the live view keeps the port open
        output.printf_P(PSTR(HTML_S(br) "%s running"), _getOpTypeStr(_op->type));
    }
    output.println(F(HTML_S(br)));
}

bool LD24xxSensor::_start(OpType type, Stream *output)
{
    if (type != OpType::STREAM) {
        _preemptStream();
    }
    if (_op) {
        return false;
    }
    if (ESP.getMaxFreeBlockSize() < LD24xxSerial::kMinFreeBlock) {
        return false;
    }
    __LDBG_printf("start type=%u baud=%u", type, _baud);
    _op.reset(_createOperation(type, output));
    _openSerial();
    LoopFunctions::add([this]() {
        _loop();
    }, this);
    return true;
}

void LD24xxSensor::_openSerial()
{
    // the destructor gives the port back: close the old one before opening the new one
    _serial.reset();
    _serial.reset(new LD24xxSerial(_baud));
}

void LD24xxSensor::_finish(PGM_P error)
{
    if (error) {
        auto &op = *_op;
        __LDBG_printf("error=%s type=%u step=%u", error, op.type, op.step);
        if (!op.output && _isCommand(op.type)) {
            // before the next frame, it resets the counters of the serial port
            _logResult(error);
        }
        if (_isCommand(op.type)) {
            // leave the configuration mode, also after the first step: a module whose reply got lost stops reporting
            // while it is in it
            _sendFrame(LD24xxSerial::kCmdDisableConfig);
        }
        if (op.output) {
            _prefix(*op.output).printf_P(PSTR("ERROR - %s\n"), error);
        }
    }
    _close();
    _onIdle();
}

void LD24xxSensor::_close()
{
    LoopFunctions::remove(this);
    _op.reset();
    _serial.reset();
}

void LD24xxSensor::_sendFrame(uint16_t command, const uint8_t *data, uint8_t length)
{
    _serial->sendCommand(command, data, length);
    _op->command = command;
}

Print &LD24xxSensor::_prefix(Print &output) const
{
    output.print('+');
    output.print(FPSTR(_protocol.name));
    output.print(F(": "));
    return output;
}

String LD24xxSensor::_getFormTitle() const
{
    if (!_hasParams()) {
        return PrintString(F("%s: not responding"), _name.c_str());
    }
    return PrintString(F("%s, firmware %s"), _name.c_str(), _version.c_str());
}

bool LD24xxSensor::_parseVersion(const uint8_t *data, uint8_t length)
{
    if (length < kVersionOffset + kVersionLength) {
        return false;
    }
    data += kVersionOffset;
    char version[20];
    snprintf_P(version, sizeof(version), PSTR("%u.%02X.%02X%02X%02X%02X"), data[1], data[0], data[5], data[4], data[3], data[2]);
    _version = version;
    return true;
}

template<typename _Type>
void LD24xxSensor::_addGateField(FormUI::Form::BaseForm &form, uint8_t gate, uint8_t gateDistance, const __FlashStringHelper *moveName, _Type &move, const __FlashStringHelper *stillName, _Type &still, _Type maxValue)
{
    // the still value is a hidden field shown as second input next to the move value
    auto &stillField = form.addPointerTriviallyCopyable<_Type>(stillName, &still);
    form.addFormUI(FormUI::Type::HIDDEN);
    form.addValidator(FormUI::Validator::Range(0, maxValue));

    PrintString label(F("Gate %u ("), gate);
    printGateDistance(label, gate, gateDistance);
    label.print('-');
    printGateDistance(label, gate + 1, gateDistance);
    label.print(F(" m)"));

    form.addPointerTriviallyCopyable<_Type>(moveName, &move);
    form.addFormUI(FormUI::Label(label), FormUI::MinMax(0, maxValue), FormUI::Suffix(F("move")), FormUI::TextInputSuffix(stillField), FormUI::Suffix(F("still")));
    form.addValidator(FormUI::Validator::Range(0, maxValue));
}

#if IOT_SENSOR_HAVE_LD2410C
    template void LD24xxSensor::_addGateField<uint8_t>(FormUI::Form::BaseForm &form, uint8_t gate, uint8_t gateDistance, const __FlashStringHelper *moveName, uint8_t &move, const __FlashStringHelper *stillName, uint8_t &still, uint8_t maxValue);
#endif
#if IOT_SENSOR_HAVE_LD2420
    template void LD24xxSensor::_addGateField<uint32_t>(FormUI::Form::BaseForm &form, uint8_t gate, uint8_t gateDistance, const __FlashStringHelper *moveName, uint32_t &move, const __FlashStringHelper *stillName, uint32_t &still, uint32_t maxValue);
#endif

PGM_P LD24xxSensor::_getOpTypeStr(OpType type)
{
    switch(type) {
        case OpType::READ:
            return PSTR("read");
        case OpType::WRITE:
            return PSTR("write");
        case OpType::RESTART:
            return PSTR("restart");
        case OpType::FACTORY:
            return PSTR("factory reset");
        case OpType::BAUD:
            return PSTR("baud rate change");
        case OpType::BLUETOOTH:
            return PSTR("Bluetooth off");
        case OpType::MONITOR:
            return PSTR("monitor");
        case OpType::STREAM:
            return PSTR("live view");
        case OpType::TX:
            return PSTR("TX test");
        case OpType::NONE:
        default:
            break;
    }
    return PSTR("none");
}

void LD24xxSensor::_loop()
{
    if (!_op) {
        return;
    }
    switch(_op->type) {
        case OpType::MONITOR:
            _loopMonitor();
            break;
        case OpType::STREAM:
            _loopStream();
            break;
        case OpType::TX:
            _loopTx();
            break;
        default:
            _loopCommand();
            break;
    }
}

uint8_t LD24xxSensor::_getLastStep() const
{
    switch(_op->type) {
        case OpType::READ:
        case OpType::WRITE:
            return _getStepCount(_op->type) + 1;
        case OpType::BAUD:
        case OpType::FACTORY:
        case OpType::BLUETOOTH:
            // the command, then the restart that applies it
            return 2;
        default:
            // restart
            return 1;
    }
}

bool LD24xxSensor::_sendNextStep()
{
    auto &op = *_op;
    if (op.step == 0) {
        const uint8_t data[2] = { _protocol.enableConfigValue, 0x00 };
        _sendFrame(LD24xxSerial::kCmdEnableConfig, data, sizeof(data));
        return true;
    }
    switch(op.type) {
        case OpType::READ:
        case OpType::WRITE:
            if (op.step <= _getStepCount(op.type)) {
                return _sendStep(op.type, op.step - 1);
            }
            _sendFrame(LD24xxSerial::kCmdDisableConfig);
            return true;
        case OpType::RESTART:
            _sendFrame(_protocol.restartCommand);
            return true;
        case OpType::FACTORY:
            if (op.step == 1) {
                _sendFrame(_protocol.factoryCommand);
            }
            else {
                // the module answers the factory reset and uses the factory values after the restart
                _sendFrame(_protocol.restartCommand);
            }
            return true;
        case OpType::BLUETOOTH:
            if (op.step == 1) {
                // 0x0000 = off
                const uint8_t data[2] = { 0x00, 0x00 };
                _sendFrame(_protocol.bluetoothCommand, data, sizeof(data));
            }
            else {
                _sendFrame(_protocol.restartCommand);
            }
            return true;
        case OpType::BAUD:
            if (op.step == 1) {
                const uint8_t data[2] = { LD24xxSerial::getBaudValue(_protocol.baud), 0x00 };
                _sendFrame(LD24xxSerial::kCmdSetBaudRate, data, sizeof(data));
            }
            else {
                // the new rate is used after the restart
                _sendFrame(_protocol.restartCommand);
            }
            return true;
        default:
            break;
    }
    return false;
}

void LD24xxSensor::_loopCommand()
{
    auto &op = *_op;
    if (op.state == StateType::SEND) {
        if (static_cast<int32_t>(millis() - op.nextSend) < 0) {
            return;
        }
        auto lastStep = _getLastStep();
        while (op.step <= lastStep && !_sendNextStep()) {
            op.step++;
        }
        if (op.step > lastStep) {
            _complete();
            return;
        }
        if (op.type != OpType::READ && op.type != OpType::WRITE && op.step == lastStep) {
            // the module restarts without a reply
            op.step++;
            return;
        }
        op.state = StateType::WAIT_REPLY;
        op.sent = millis();
        op.timeout = op.sent + (_isBaudSearch() ? kScanReplyTimeout : kReplyTimeout);
        return;
    }

    if (_serial->readReply()) {
        _handleReply();
        return;
    }
    if (static_cast<int32_t>(millis() - op.timeout) < 0) {
        return;
    }

    op.totalRetries++;
    if (op.output && !_isBaudSearch()) {
        _prefix(*op.output).printf_P(PSTR("timeout, command 0x%04x, step %u, %u bytes received, %u bytes sent\n"), op.command, op.step, _serial->getReceivedBytes(), _serial->getWrittenBytes());
    }
    if (++op.retries < (_isBaudSearch() ? kScanRetries : kMaxRetries)) {
        __LDBG_printf("timeout step=%u retry=%u", op.step, op.retries);
        op.state = StateType::SEND;
        return;
    }
    if (op.step == 0 && !op.replyError) {
        // no reply to the first command: the module may use another baud rate
        if (op.baudIndex == 0) {
            op.firstBaud = _baud;
        }
        auto baud = LD24xxSerial::getScanBaud(++op.baudIndex, op.firstBaud);
        if (baud) {
            if (op.output) {
                _prefix(*op.output).printf_P(PSTR("no reply at %u baud, trying %u baud\n"), static_cast<unsigned>(_baud), static_cast<unsigned>(baud));
            }
            // the module may have received the command at this rate while its reply got lost
            _sendFrame(LD24xxSerial::kCmdDisableConfig);
            op.retries = 0;
            op.state = StateType::SEND;
            _baud = baud;
            _openSerial();
            return;
        }
        // not found, the next operation starts with the default
        _baud = _protocol.baud;
    }
    if (op.replyError) {
        _finish(PSTR("command failed"));
        return;
    }
    if (op.output) {
        _prefix(*op.output).printf_P(PSTR("no reply to command 0x%04x, step %u, %u receive errors\n"), op.command, op.step, _serial->getErrors());
    }
    _finish(PSTR("no reply from the sensor"));
}

bool LD24xxSensor::_handleReply()
{
    auto &op = *_op;
    auto command = _serial->getReplyCommand();
    if (command != (op.command | LD24xxSerial::kReplyFlag)) {
        if (op.output) {
            _prefix(*op.output).printf_P(PSTR("unexpected reply command 0x%04x (expected 0x%04x), %u bytes of data\n"), command, op.command | LD24xxSerial::kReplyFlag, _serial->getReplyDataLength());
        }
        return false;
    }
    auto error = _serial->getReplyError();
    if (error) {
        // a garbled command or reply: retried like a timeout
        if (op.output) {
            _prefix(*op.output).printf_P(PSTR("command 0x%04x returned error %u\n"), op.command, error);
        }
        op.replyError = error;
        op.timeout = millis();
        return false;
    }
    if (op.type == OpType::READ && op.step >= 1 && op.step <= _getStepCount(op.type)) {
        if (!_parseReply(op.step - 1, _serial->getReplyData(), _serial->getReplyDataLength())) {
            // a value out of its range is a garbled reply
            if (op.output) {
                _prefix(*op.output).printf_P(PSTR("invalid reply, command 0x%04x, step %u\n"), op.command, op.step);
            }
            op.timeout = millis();
            return false;
        }
    }
    op.maxReplyTime = std::max<uint32_t>(op.maxReplyTime, millis() - op.sent);
    op.nextSend = millis() + kCommandGap;
    op.step++;
    op.retries = 0;
    op.replyError = 0;
    op.state = StateType::SEND;
    return true;
}

void LD24xxSensor::_complete()
{
    auto type = _op->type;
    auto output = _op->output;
    if (output && (type == OpType::READ || type == OpType::WRITE)) {
        _prefix(*output).printf_P(PSTR("%u retries, %u receive errors, max. reply time %ums\n"), _op->totalRetries, _serial->getErrors(), static_cast<unsigned>(_op->maxReplyTime));
    }
    if (type == OpType::READ) {
        _storeParams();
    }
    if (!output) {
        _logResult(nullptr);
    }
    _close();
    switch(type) {
        case OpType::READ:
            if (output) {
                _printParams(*output);
            }
            // the baud rate first, Bluetooth is switched off at the next read
            if (!_restoreBaud(output)) {
                #if IOT_SENSOR_LD24XX_BLUETOOTH_OFF
                    if (_protocol.bluetoothCommand && _isBluetoothOn()) {
                        _switchBluetoothOff(output);
                    }
                #endif
            }
            break;
        case OpType::BLUETOOTH:
            if (output) {
                _prefix(*output).println(F("Bluetooth switched off, the sensor restarts"));
            }
            // reads it back after the restart, until the serial port is free
            _Timer(_readTimer).add(Event::milliseconds(kRestartDelay), true, [this](Event::CallbackTimerPtr) {
                if (!_op && _start(OpType::READ, nullptr)) {
                    _Timer(_readTimer).remove();
                }
            });
            break;
        case OpType::WRITE:
            if (output) {
                _prefix(*output).println(F("configuration written, reading it back"));
            }
            _start(OpType::READ, output);
            break;
        case OpType::RESTART:
            if (output) {
                _prefix(*output).println(F("restarting the sensor, wait a few seconds before the next command"));
            }
            break;
        case OpType::FACTORY:
            if (output) {
                _prefix(*output).println(F("factory defaults restored, the sensor restarts, wait a few seconds before the next command"));
            }
            break;
        case OpType::BAUD:
            _baud = _protocol.baud;
            if (output) {
                _prefix(*output).printf_P(PSTR("the sensor restarts at %u baud\n"), static_cast<unsigned>(_baud));
            }
            break;
        default:
            break;
    }
    _onIdle();
}

bool LD24xxSensor::_restoreBaud(Stream *output)
{
    if (!_protocol.hasBaudCommand || _baud == _protocol.baud || !LD24xxSerial::getBaudValue(_protocol.baud)) {
        return false;
    }
    if (!_start(OpType::BAUD, output)) {
        // not enough memory, the next read tries again
        return false;
    }
    if (output) {
        _prefix(*output).printf_P(PSTR("setting the module from %u to %u baud\n"), static_cast<unsigned>(_baud), static_cast<unsigned>(_protocol.baud));
    }
    return true;
}

void LD24xxSensor::_switchBluetoothOff(Stream *output)
{
    if (_bluetoothOff) {
        if (_bluetoothOff == 1) {
            // the read after the restart still finds it on, it is not tried again in this boot
            _bluetoothOff = 2;
            Logger_error(F("%s: Bluetooth is still on after switching it off"), _protocol.name);
        }
        return;
    }
    if (!_start(OpType::BLUETOOTH, output)) {
        // not enough memory, the next read tries again
        return;
    }
    _bluetoothOff = 1;
    if (output) {
        _prefix(*output).println(F("Bluetooth is on, switching it off"));
    }
}

void LD24xxSensor::_logResult(PGM_P error)
{
    auto &op = *_op;
    auto uptime = static_cast<unsigned>(millis() / 1000U);
    if (error) {
        Logger_error(F("%s: %s failed: %s, command 0x%04x step %u at %u baud, %u bytes received, %u receive errors, %u retries, uptime %us"),
            _protocol.name, _getOpTypeStr(op.type), error, op.command, op.step, static_cast<unsigned>(_baud),
            _serial->getReceivedBytes(), _serial->getErrors(), op.totalRetries, uptime);
        return;
    }
    Logger_notice(F("%s: %s ok at %u baud, firmware %s, %u retries, uptime %us"), _protocol.name, _getOpTypeStr(op.type),
        static_cast<unsigned>(_baud), _version.c_str(), op.totalRetries, uptime);
}

void LD24xxSensor::_preemptStream()
{
    if (!_op || _op->type != OpType::STREAM) {
        return;
    }
    _close();
    // after the operation that takes the port has started: the live view retries until the port is free
    LoopFunctions::callOnce([this]() {
        _onIdle();
    });
}

void LD24xxSensor::_loopMonitor()
{
    auto &op = *_op;
    if (static_cast<int32_t>(millis() - op.endTime) >= 0) {
        _prefix(*op.output).println(F("monitor end"));
        _finish();
        return;
    }
    int data;
    if (op.raw) {
        // hex dump, 16 bytes per line, a partial line is printed after a pause
        while ((data = _serial->read()) >= 0) {
            op.buffer[op.length++] = static_cast<uint8_t>(data);
            op.lastOutput = millis();
            if (op.length == 16) {
                _printMonitorRaw();
            }
        }
        if (op.length && static_cast<int32_t>(millis() - op.lastOutput) >= static_cast<int32_t>(kMonitorRawPause)) {
            _printMonitorRaw();
        }
        return;
    }
    while (_op && (data = _serial->read()) >= 0) {
        _feedMonitor(static_cast<uint8_t>(data));
    }
}

void LD24xxSensor::_printMonitorRaw()
{
    auto &op = *_op;
    auto &output = *op.output;
    output.print('+');
    output.print(FPSTR(_protocol.name));
    output.print(':');
    for(uint8_t i = 0; i < op.length; i++) {
        output.printf_P(PSTR(" %02x"), op.buffer[i]);
    }
    output.println();
    op.length = 0;
}

// a continuous 0x55 pattern: a multimeter on the TX pin shows a mid level while UART1 drives it, 0 V or 3.3 V means the
// pin is not driven by the UART
void LD24xxSensor::_loopTx()
{
    auto &op = *_op;
    if (static_cast<int32_t>(millis() - op.endTime) >= 0) {
        if (op.output) {
            _prefix(*op.output).printf_P(PSTR("TX test end, %lu bytes sent\n"), static_cast<unsigned long>(op.txBytes));
        }
        _finish();
        return;
    }
    uint8_t data[32];
    memset(data, 0x55, sizeof(data));
    op.txBytes += _serial->writeRaw(data, sizeof(data));
}

bool LD24xxSensor::_feedFrame(uint8_t data, const uint8_t *header, const uint8_t *footer, uint8_t footerLength, uint8_t frameLength)
{
    auto &op = *_op;
    if (op.length < 4 && data != header[op.length]) {
        // not the header, a byte that begins one is kept
        op.length = 0;
        if (data != header[0]) {
            return false;
        }
    }
    op.buffer[op.length++] = data;
    if (op.length < 6) {
        return false;
    }
    uint16_t length = frameLength ? frameLength : LD24xxSerial::getUint16(&op.buffer[4]) + 10U;
    if (length > sizeof(op.buffer) || length < 6 + footerLength) {
        op.length = 0;
        return false;
    }
    if (op.length < length) {
        return false;
    }
    op.length = 0;
    return memcmp(&op.buffer[length - footerLength], footer, footerLength) == 0;
}

bool LD24xxSensor::_monitorThrottle(const uint8_t *key, uint8_t length, uint8_t urgentLength)
{
    auto &op = *_op;
    length = std::min<uint8_t>(length, sizeof(op.lastKey));
    auto elapsed = static_cast<int32_t>(millis() - op.lastOutput);
    bool urgent = urgentLength && memcmp(op.lastKey, key, urgentLength) != 0;
    bool changed = memcmp(op.lastKey, key, length) != 0;
    if (!urgent && (elapsed < static_cast<int32_t>(kMonitorOutputInterval) || (!changed && elapsed < static_cast<int32_t>(kMonitorRepeatInterval)))) {
        return false;
    }
    memcpy(op.lastKey, key, length);
    op.lastOutput = millis();
    return true;
}

void LD24xxSensor::_applyPendingWrite()
{
    if (!_hasPendingWrite()) {
        return;
    }
    if (_op && _op->type == OpType::STREAM) {
        // the values are written while the pages of the live view wait, the stream restarts afterwards
        _close();
    }
    if (_op) {
        // until the running command has finished. A repeating timer: a single shot timer that is added again from its
        // own callback gets removed after the callback
        _Timer(_writeTimer).add(Event::milliseconds(kPendingWriteRetry), true, [this](Event::CallbackTimerPtr) {
            // the live view can take the port first when the command has finished, the write stops it again
            if (!_op || _op->type == OpType::STREAM) {
                _Timer(_writeTimer).remove();
                _applyPendingWrite();
            }
        });
        return;
    }
    _writePendingParams();
    if (!_op) {
        // nothing changed on the module: the stream that was closed above starts again
        _onIdle();
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

void LD24xxSensor::_atModeHandler(AtModeArgs &args)
{
    auto &output = args.getStream();
    auto cmd = args.toString(0);
    // the live view would answer "busy"
    _preemptStream();

    if (F("mon") == cmd || F("tx") == cmd) {
        bool monitor = (F("mon") == cmd);
        auto seconds = monitor ? args.toIntMinMax<uint16_t>(1, 0, 300, 10) : args.toIntMinMax<uint16_t>(1, 1, 30, 5);
        if (monitor && _op && _op->type == OpType::MONITOR) {
            // a new monitor replaces the running one, 0 stops it
            _finish();
            if (seconds == 0) {
                args.print(F("monitor stopped"));
                return;
            }
        }
        if (seconds == 0) {
            return;
        }
        if (_op) {
            args.print(F("ERROR - busy, %s running"), _getOpTypeStr(_op->type));
            return;
        }
        if (!_start(monitor ? OpType::MONITOR : OpType::TX, &output)) {
            args.print(F("ERROR - not enough memory"));
            return;
        }
        _op->endTime = millis() + seconds * 1000U;
        if (monitor) {
            _op->raw = args.equalsIgnoreCase(2, F("raw"));
            args.print(F("monitor for %us at %u baud"), seconds, static_cast<unsigned>(_baud));
        }
        else {
            args.print(F("TX test for %us, the pin shows a mid level while the UART drives it"), seconds);
        }
        return;
    }

    if (_op) {
        args.print(F("ERROR - busy, %s running"), _getOpTypeStr(_op->type));
        return;
    }

    if (cmd.length() == 0 || F("read") == cmd || F("restart") == cmd || (F("factory") == cmd && _protocol.factoryCommand)) {
        auto type = (F("restart") == cmd) ? OpType::RESTART : (F("factory") == cmd) ? OpType::FACTORY : OpType::READ;
        if (!_start(type, &output)) {
            args.print(F("ERROR - not enough memory"));
        }
        return;
    }

    if (!_hasParams()) {
        args.print(F("ERROR - configuration not read, run +%s=read first"), _protocol.name);
        return;
    }
    _atModeWrite(args, output, cmd);
}

int16_t LD24xxSensor::_atModeIndex(AtModeArgs &args, uint16_t num, uint8_t maxIndex, const __FlashStringHelper *name)
{
    // digits only, toInt() reads "x" as 0
    auto str = args.toString(num);
    int16_t index = str.length() ? 0 : -1;
    for(size_t i = 0; i < str.length() && index >= 0 && index <= maxIndex; i++) {
        index = isdigit(str[i]) ? (index * 10 + (str[i] - '0')) : -1;
    }
    if (index < 0 || index > maxIndex) {
        args.print(F("ERROR - %s must be 0-%u"), name, maxIndex);
        return -1;
    }
    return index;
}

#endif

#endif
