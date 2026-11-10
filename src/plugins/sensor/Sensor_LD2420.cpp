/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_LD2420

#include <LoopFunctions.h>
#include "Sensor_LD2420.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    constexpr uint16_t kCmdReadVersion = 0x0000;
    constexpr uint16_t kCmdWriteParams = 0x0007;
    constexpr uint16_t kCmdReadParams = 0x0008;
    constexpr uint16_t kCmdRestart = 0x0068;
    constexpr uint16_t kCmdDisableConfig = LD24xxSerial::kCmdDisableConfig;
    constexpr uint16_t kCmdEnableConfig = LD24xxSerial::kCmdEnableConfig;
    constexpr uint8_t kProtocolVersion[2] = { 0x02, 0x00 };

    constexpr uint16_t kRegMinGate = 0x0000;
    constexpr uint16_t kRegMaxGate = 0x0001;
    constexpr uint16_t kRegTimeout = 0x0004;
    constexpr uint16_t kRegMoveThreshold = 0x0010;  // + gate
    constexpr uint16_t kRegStillThreshold = 0x0020; // + gate

    constexpr uint8_t kEnergyHeader[4] = { 0xf4, 0xf3, 0xf2, 0xf1 };
    constexpr uint8_t kEnergyFooter[4] = { 0xf8, 0xf7, 0xf6, 0xf5 };
    // header, length, presence, distance, 16 gate energies, footer
    constexpr uint8_t kEnergyFrameLength = 4 + 2 + 1 + 2 + Sensor_LD2420::kGateCount * 2 + 4;

    // the sensor replies within about 50ms
    constexpr uint32_t kReplyTimeout = 300;
    // pause between a reply and the next command
    constexpr uint32_t kCommandGap = 10;
    constexpr uint8_t kMaxRetries = 3;
    constexpr uint32_t kBootReadDelay = 5000;
    constexpr uint32_t kMonitorOutputInterval = 250;
    constexpr uint32_t kMonitorRepeatInterval = 1000;
    constexpr uint32_t kFallbackBaud = 256000;
    // a saved form waits for a running command in these steps
    constexpr uint32_t kPendingWriteRetry = 250;
    // distance of a gate in centimeters
    constexpr uint32_t kGateDistance = 70;

    // steps of OpType::READ
    constexpr uint8_t kReadEnable = 0;
    constexpr uint8_t kReadVersion = 1;
    constexpr uint8_t kReadRange = 2;
    constexpr uint8_t kReadGate = 3;
    constexpr uint8_t kReadDisable = kReadGate + Sensor_LD2420::kGateCount;

    // steps of OpType::WRITE, bit 0 of writeMask is the range, bit 1 + n gate n
    constexpr uint8_t kWriteEnable = 0;
    constexpr uint8_t kWriteRange = 1;
    constexpr uint8_t kWriteGate = 2;
    constexpr uint8_t kWriteDisable = kWriteGate + Sensor_LD2420::kGateCount;

    // steps of OpType::RESTART
    constexpr uint8_t kRestartEnable = 0;
    constexpr uint8_t kRestartSend = 1;

    // factory defaults, read from a new module with firmware v1.6.1 (the factory reset of ESPHome uses older values)
    constexpr uint32_t kFactoryMinGate = 0;
    constexpr uint32_t kFactoryMaxGate = 12;
    constexpr uint32_t kFactoryTimeout = 30;
    static const uint16_t kFactoryMove[Sensor_LD2420::kGateCount] PROGMEM = {
        60000, 30000, 3000, 2000, 500, 400, 400, 300, 300, 300, 300, 250, 250, 200, 200, 200
    };
    static const uint16_t kFactoryStill[Sensor_LD2420::kGateCount] PROGMEM = {
        40000, 20000, 400, 300, 300, 200, 200, 150, 150, 100, 100, 100, 100, 100, 100, 100
    };

}

Sensor_LD2420::Sensor_LD2420(const String &name) :
    MQTT::Sensor(MQTT::SensorType::LD2420),
    _name(name),
    _version(),
    _serial(),
    _op(),
    _params(),
    _formParams(),
    _pendingWrite(),
    _timer(),
    _baud(IOT_SENSOR_LD2420_BAUD)
{
}

Sensor_LD2420::~Sensor_LD2420()
{
    shutdown();
}

void Sensor_LD2420::setup()
{
    // the sensor needs a moment after power on
    _Timer(_timer).add(Event::milliseconds(kBootReadDelay), false, [this](Event::CallbackTimerPtr) {
        _start(OpType::READ, nullptr);
    });
}

void Sensor_LD2420::shutdown()
{
    _Timer(_timer).remove();
    if (_op) {
        LoopFunctions::remove(this);
        _op.reset();
    }
    _serial.reset();
}

void Sensor_LD2420::getStatus(Print &output)
{
    output.print(_name);
    if (_params) {
        output.printf_P(PSTR(" firmware %s, detection gates %u-%u, presence timeout %us"), _version.c_str(), static_cast<unsigned>(_params->minGate), static_cast<unsigned>(_params->maxGate), static_cast<unsigned>(_params->timeout));
    }
    else if (_op) {
        output.print(F(" reading the configuration"));
    }
    else {
        output.print(F(" not responding"));
    }
    output.println(F(HTML_S(br)));
}

uint32_t Sensor_LD2420::_getFactoryMove(uint8_t gate)
{
    return pgm_read_word(&kFactoryMove[gate]);
}

uint32_t Sensor_LD2420::_getFactoryStill(uint8_t gate)
{
    return pgm_read_word(&kFactoryStill[gate]);
}

// returns false if an operation is running or the memory is too low for the serial port buffers
bool Sensor_LD2420::_start(OpType type, Stream *output)
{
    if (_op) {
        return false;
    }
    if (ESP.getMaxFreeBlockSize() < LD24xxSerial::kMinFreeBlock) {
        return false;
    }
    __LDBG_printf("start type=%u baud=%u", type, _baud);
    _op.reset(new Operation(type, output));
    _openSerial();
    LoopFunctions::add([this]() {
        _loop();
    }, this);
    return true;
}

void Sensor_LD2420::_openSerial()
{
    // the destructor disables the interrupt of the pin: close the old port before opening the new one
    _serial.reset();
    _serial.reset(new LD24xxSerial(_baud));
}

void Sensor_LD2420::_finish(PGM_P error)
{
    auto output = _op->output;
    if (error) {
        __LDBG_printf("error=%s type=%u step=%u", error, _op->type, _op->step);
        // leave the configuration mode of an operation that failed after entering it
        if (_op->type != OpType::MONITOR && _op->step > 0) {
            _sendFrame(kCmdDisableConfig, nullptr, 0);
        }
        if (output) {
            output->printf_P(PSTR("+LD2420: ERROR - %s\n"), error);
        }
    }
    LoopFunctions::remove(this);
    _op.reset();
    _serial.reset();
}

void Sensor_LD2420::_sendFrame(uint16_t command, const uint8_t *data, uint8_t length)
{
    _serial->sendCommand(command, data, length);
    _op->command = command;
}

void Sensor_LD2420::_sendReadParams(const uint16_t *registers, uint8_t count)
{
    uint8_t data[6];
    auto ptr = data;
    for(uint8_t i = 0; i < count; i++) {
        ptr = LD24xxSerial::putUint16(ptr, registers[i]);
    }
    _sendFrame(kCmdReadParams, data, ptr - data);
}

void Sensor_LD2420::_sendWriteParams(const uint16_t *registers, const uint32_t *values, uint8_t count)
{
    uint8_t data[LD24xxSerial::kMaxCommandData];
    auto ptr = data;
    for(uint8_t i = 0; i < count; i++) {
        ptr = LD24xxSerial::putUint16(ptr, registers[i]);
        ptr = LD24xxSerial::putUint32(ptr, values[i]);
    }
    _sendFrame(kCmdWriteParams, data, ptr - data);
}

// sends the frame of the current step and returns true, false if the step has nothing to send
bool Sensor_LD2420::_sendStep()
{
    auto &op = *_op;
    auto step = op.step;
    switch(op.type) {
        case OpType::READ:
            if (step == kReadEnable) {
                _sendFrame(kCmdEnableConfig, kProtocolVersion, sizeof(kProtocolVersion));
            }
            else if (step == kReadVersion) {
                _sendFrame(kCmdReadVersion, nullptr, 0);
            }
            else if (step == kReadRange) {
                const uint16_t registers[] = { kRegMinGate, kRegMaxGate, kRegTimeout };
                _sendReadParams(registers, 3);
            }
            else if (step < kReadDisable) {
                uint8_t gate = step - kReadGate;
                const uint16_t registers[] = { static_cast<uint16_t>(kRegMoveThreshold + gate), static_cast<uint16_t>(kRegStillThreshold + gate) };
                _sendReadParams(registers, 2);
            }
            else {
                _sendFrame(kCmdDisableConfig, nullptr, 0);
            }
            return true;
        case OpType::WRITE:
            if (step == kWriteEnable) {
                _sendFrame(kCmdEnableConfig, kProtocolVersion, sizeof(kProtocolVersion));
            }
            else if (step == kWriteRange) {
                if ((op.writeMask & 1) == 0) {
                    return false;
                }
                const uint16_t registers[] = { kRegMinGate, kRegMaxGate, kRegTimeout };
                const uint32_t values[] = { op.pending.minGate, op.pending.maxGate, op.pending.timeout };
                _sendWriteParams(registers, values, 3);
            }
            else if (step < kWriteDisable) {
                uint8_t gate = step - kWriteGate;
                if ((op.writeMask & (2U << gate)) == 0) {
                    return false;
                }
                const uint16_t registers[] = { static_cast<uint16_t>(kRegMoveThreshold + gate), static_cast<uint16_t>(kRegStillThreshold + gate) };
                const uint32_t values[] = { op.pending.move[gate], op.pending.still[gate] };
                _sendWriteParams(registers, values, 2);
            }
            else {
                _sendFrame(kCmdDisableConfig, nullptr, 0);
            }
            return true;
        case OpType::RESTART:
            if (step == kRestartEnable) {
                _sendFrame(kCmdEnableConfig, kProtocolVersion, sizeof(kProtocolVersion));
            }
            else {
                _sendFrame(kCmdRestart, nullptr, 0);
            }
            return true;
        default:
            break;
    }
    return false;
}

// processes the reply that the serial port received, returns false if it is not the reply to the last command
bool Sensor_LD2420::_handleReply()
{
    auto &op = *_op;
    auto command = _serial->getReplyCommand();
    if (command != (op.command | LD24xxSerial::kReplyFlag)) {
        __LDBG_printf("unexpected reply command=%04x expected=%04x", command, op.command | LD24xxSerial::kReplyFlag);
        return false;
    }
    auto error = _serial->getReplyError();
    if (error) {
        // a garbled command or reply (the frames have no checksum): retried like a timeout
        if (op.output) {
            op.output->printf_P(PSTR("+LD2420: command 0x%04x returned error %u\n"), op.command, error);
        }
        op.replyError = error;
        op.timeout = millis();
        return false;
    }
    auto data = _serial->getReplyData();
    auto dataLength = _serial->getReplyDataLength();
    if (op.type == OpType::READ) {
        // the frames have no checksum: a value out of its range is a garbled reply, it is retried like a timeout
        bool valid = true;
        auto step = op.step;
        if (step == kReadVersion) {
            // length of the version string followed by the string
            if (dataLength >= 2) {
                auto length = std::min<uint16_t>(LD24xxSerial::getUint16(&data[0]), dataLength - 2);
                _version = String();
                _version.concat(reinterpret_cast<const char *>(&data[2]), length);
            }
        }
        else if (step == kReadRange) {
            valid = dataLength >= 12;
            if (valid) {
                op.pending.minGate = LD24xxSerial::getUint32(&data[0]);
                op.pending.maxGate = LD24xxSerial::getUint32(&data[4]);
                op.pending.timeout = LD24xxSerial::getUint32(&data[8]);
                valid = op.pending.maxGate <= kMaxGate && op.pending.minGate <= op.pending.maxGate && op.pending.timeout <= kMaxTimeout;
            }
        }
        else if (step >= kReadGate && step < kReadDisable) {
            valid = dataLength >= 8;
            if (valid) {
                uint8_t gate = step - kReadGate;
                op.pending.move[gate] = LD24xxSerial::getUint32(&data[0]);
                op.pending.still[gate] = LD24xxSerial::getUint32(&data[4]);
                valid = op.pending.move[gate] <= kMaxThreshold && op.pending.still[gate] <= kMaxThreshold;
            }
        }
        if (!valid) {
            if (op.output) {
                op.output->printf_P(PSTR("+LD2420: invalid reply, command 0x%04x, step %u\n"), op.command, op.step);
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

void Sensor_LD2420::_complete()
{
    auto type = _op->type;
    auto output = _op->output;
    if (type == OpType::READ) {
        if (!_params) {
            _params.reset(new Params());
        }
        *_params = _op->pending;
    }
    if (output && type != OpType::RESTART) {
        output->printf_P(PSTR("+LD2420: %u retries, %u receive errors, max. reply time %ums\n"), _op->totalRetries, _serial->getErrors(), static_cast<unsigned>(_op->maxReplyTime));
    }
    _finish();
    switch(type) {
        case OpType::READ:
            if (output) {
                _printParams(*output);
            }
            break;
        case OpType::WRITE:
            if (output) {
                output->println(F("+LD2420: configuration written, reading it back"));
            }
            // verify the values that the sensor stored
            _start(OpType::READ, output);
            break;
        case OpType::RESTART:
            if (output) {
                output->println(F("+LD2420: restarting the sensor"));
            }
            break;
        default:
            break;
    }
}

void Sensor_LD2420::_loop()
{
    if (!_op) {
        return;
    }
    auto &op = *_op;
    if (op.type == OpType::MONITOR) {
        _loopMonitor();
        return;
    }
    if (op.state == StateType::SEND) {
        if (static_cast<int32_t>(millis() - op.nextSend) < 0) {
            return;
        }
        // the last step is the one that leaves the configuration mode
        uint8_t lastStep = (op.type == OpType::READ) ? kReadDisable : (op.type == OpType::WRITE) ? kWriteDisable : kRestartSend;
        while (op.step <= lastStep && !_sendStep()) {
            op.step++;
        }
        if (op.step > lastStep) {
            _complete();
            return;
        }
        if (op.type == OpType::RESTART && op.step == kRestartSend) {
            // the sensor does not reply to a restart
            op.step++;
            return;
        }
        op.state = StateType::WAIT_REPLY;
        op.sent = millis();
        op.timeout = op.sent + kReplyTimeout;
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
    if (op.output) {
        op.output->printf_P(PSTR("+LD2420: timeout, command 0x%04x, step %u, %u bytes received\n"), op.command, op.step, _serial->getReceivedBytes());
    }
    if (++op.retries < kMaxRetries) {
        __LDBG_printf("timeout step=%u retry=%u", op.step, op.retries);
        op.state = StateType::SEND;
        return;
    }
    if (op.step == 0 && !op.baudFallback && !op.replyError) {
        // no reply to the first command: try the baud rate of the other firmware versions
        op.baudFallback = true;
        op.retries = 0;
        op.state = StateType::SEND;
        _baud = (_baud == kFallbackBaud) ? IOT_SENSOR_LD2420_BAUD : kFallbackBaud;
        __LDBG_printf("no reply, trying %u baud", _baud);
        _openSerial();
        return;
    }
    if (op.baudFallback && op.step == 0) {
        // back to the default for the next operation
        _baud = IOT_SENSOR_LD2420_BAUD;
    }
    if (op.replyError) {
        _finish(PSTR("command failed"));
        return;
    }
    if (op.output) {
        op.output->printf_P(PSTR("+LD2420: no reply to command 0x%04x, step %u, %u receive errors\n"), op.command, op.step, _serial->getErrors());
    }
    _finish(PSTR("no reply from the sensor"));
}

// prints the data that the sensor reports: text lines (simple mode) or energy frames (energy mode)
void Sensor_LD2420::_loopMonitor()
{
    auto &op = *_op;
    if (static_cast<int32_t>(millis() - op.endTime) >= 0) {
        if (op.output) {
            op.output->println(F("+LD2420: monitor end"));
        }
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
                _printMonitorRaw(*op.output);
            }
        }
        if (op.length && static_cast<int32_t>(millis() - op.lastOutput) >= 20) {
            _printMonitorRaw(*op.output);
        }
        return;
    }
    while ((data = _serial->read()) >= 0) {
        auto byte = static_cast<uint8_t>(data);
        if (op.length == 0) {
            if (byte == kEnergyHeader[0] || isprint(byte)) {
                op.buffer[op.length++] = byte;
            }
            continue;
        }
        if (op.buffer[0] == kEnergyHeader[0]) {
            // energy frame
            if (op.length < sizeof(kEnergyHeader) && byte != kEnergyHeader[op.length]) {
                op.length = 0;
                continue;
            }
            op.buffer[op.length++] = byte;
            if (op.length == kEnergyFrameLength) {
                op.length = 0;
                if (memcmp(&op.buffer[kEnergyFrameLength - sizeof(kEnergyFooter)], kEnergyFooter, sizeof(kEnergyFooter)) == 0) {
                    _printMonitorFrame(*op.output);
                }
            }
            continue;
        }
        // text line
        if (byte == '\r' || byte == '\n') {
            op.buffer[op.length] = 0;
            auto line = reinterpret_cast<const char *>(op.buffer);
            op.length = 0;
            // simple mode reports "ON" or "OFF" and "Range <cm>" about 20 times per second: print a presence change
            // immediately, a distance change every 250ms and the state once per second
            bool presenceChanged = false;
            if (strcmp_P(line, PSTR("ON")) == 0 || strcmp_P(line, PSTR("OFF")) == 0) {
                uint8_t presence = (line[1] == 'N');
                presenceChanged = (presence != op.presence);
                op.presence = presence;
            }
            else if (strncmp_P(line, PSTR("Range "), 6) == 0) {
                op.distance = atoi(line + 6);
            }
            else {
                op.output->printf_P(PSTR("+LD2420: %s\n"), line);
                continue;
            }
            auto elapsed = static_cast<int32_t>(millis() - op.lastOutput);
            if (presenceChanged || (op.distance != op.lastDistance && elapsed >= static_cast<int32_t>(kMonitorOutputInterval)) || elapsed >= static_cast<int32_t>(kMonitorRepeatInterval)) {
                op.lastOutput = millis();
                op.lastDistance = op.distance;
                op.output->printf_P(PSTR("+LD2420: presence %s, distance %ucm\n"), op.presence ? PSTR("ON") : PSTR("OFF"), op.distance);
            }
            continue;
        }
        if (!isprint(byte) || op.length >= sizeof(op.buffer) - 1) {
            op.length = 0;
            continue;
        }
        op.buffer[op.length++] = byte;
    }
}

void Sensor_LD2420::_printMonitorRaw(Print &output)
{
    auto &op = *_op;
    output.print(F("+LD2420:"));
    for(uint8_t i = 0; i < op.length; i++) {
        output.printf_P(PSTR(" %02x"), op.buffer[i]);
    }
    output.println();
    op.length = 0;
}

void Sensor_LD2420::_printMonitorFrame(Print &output)
{
    auto &op = *_op;
    if (static_cast<int32_t>(millis() - op.lastOutput) < static_cast<int32_t>(kMonitorOutputInterval)) {
        return;
    }
    op.lastOutput = millis();
    auto data = &op.buffer[6];
    output.printf_P(PSTR("+LD2420: presence=%u distance=%ucm energy="), data[0], LD24xxSerial::getUint16(&data[1]));
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        output.printf_P(PSTR("%s%u"), gate ? PSTR(",") : PSTR(""), LD24xxSerial::getUint16(&data[3 + gate * 2]));
    }
    output.println();
}

void Sensor_LD2420::_printParams(Print &output) const
{
    output.printf_P(PSTR("+LD2420: firmware %s, %u baud\n"), _version.c_str(), static_cast<unsigned>(_baud));
    output.printf_P(PSTR("+LD2420: detection gates %u-%u, presence timeout %us\n"), static_cast<unsigned>(_params->minGate), static_cast<unsigned>(_params->maxGate), static_cast<unsigned>(_params->timeout));
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        output.printf_P(PSTR("+LD2420: gate %2u move %5u still %5u\n"), gate, static_cast<unsigned>(_params->move[gate]), static_cast<unsigned>(_params->still[gate]));
    }
}

// writes the changed values, prints "no changes" or an error to output if nothing gets written
void Sensor_LD2420::_queueWrite(const Params &params, Stream *output)
{
    uint32_t mask = 0;
    if (params.minGate != _params->minGate || params.maxGate != _params->maxGate || params.timeout != _params->timeout) {
        mask |= 1;
    }
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        if (params.move[gate] != _params->move[gate] || params.still[gate] != _params->still[gate]) {
            mask |= (2U << gate);
        }
    }
    if (mask == 0) {
        if (output) {
            output->println(F("+LD2420: no changes"));
        }
        return;
    }
    if (!_start(OpType::WRITE, output)) {
        if (output) {
            output->println(F("+LD2420: ERROR - busy or not enough memory"));
        }
        return;
    }
    _op->pending = params;
    _op->writeMask = mask;
}

// the values a form shows: a saved form that has not been written yet, the one being written or the ones read from the
// sensor. returns nullptr if the configuration has not been read
Sensor_LD2420::Params *Sensor_LD2420::_getFormParams()
{
    if (!_params) {
        return nullptr;
    }
    if (!_formParams) {
        _formParams.reset(new Params());
    }
    if (_pendingWrite) {
        *_formParams = *_pendingWrite;
    }
    else if (_op && _op->type == OpType::WRITE) {
        *_formParams = _op->pending;
    }
    else {
        *_formParams = *_params;
    }
    return _formParams.get();
}

void Sensor_LD2420::_applyPendingWrite()
{
    if (!_pendingWrite) {
        return;
    }
    if (_op) {
        // check until the running command has finished. a repeating timer: a single shot timer that is added again
        // from its own callback gets removed after the callback
        _Timer(_timer).add(Event::milliseconds(kPendingWriteRetry), true, [this](Event::CallbackTimerPtr) {
            if (!_op) {
                _Timer(_timer).remove();
                _applyPendingWrite();
            }
        });
        return;
    }
    auto params = std::move(_pendingWrite);
    _queueWrite(*params, nullptr);
}

namespace {

    // prints the distance in meters with one decimal
    void printGateDistance(Print &output, uint8_t gate)
    {
        auto decimeters = gate * kGateDistance / 10;
        output.printf_P(PSTR("%u.%u"), decimeters / 10, decimeters % 10);
    }

}

void Sensor_LD2420::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto params = _getFormParams();
    if (!params) {
        auto &group = form.addCardGroup(F("ldcfg"), PrintString(F("%s: not responding"), _name.c_str()), true);
        group.end();
        return;
    }

    auto &group = form.addCardGroup(F("ldcfg"), PrintString(F("%s, firmware %s"), _name.c_str(), _version.c_str()), true);

    form.addPointerTriviallyCopyable<uint32_t>(F("ldmin"), &params->minGate);
    form.addFormUI(F("Min. Gate"), FormUI::Suffix(F("0.7 m each")), FormUI::MinMax(0, kMaxGate));
    form.addValidator(FormUI::Validator::Range(0, kMaxGate));

    form.addPointerTriviallyCopyable<uint32_t>(F("ldmax"), &params->maxGate);
    form.addFormUI(F("Max. Gate"), FormUI::Suffix(F("0.7 m each")), FormUI::MinMax(1, kMaxGate));
    form.addValidator(FormUI::Validator::Range(1, kMaxGate));
    form.addValidator(FormUI::Validator::CallbackTemplate<int32_t>([](int32_t value, FormUI::Field::BaseField &field) {
        auto minGate = field.getForm().getField(F("ldmin"));
        return !minGate || minGate->getValue().toInt() <= value;
    }, F("The max. gate must not be lower than the min. gate")));

    form.addPointerTriviallyCopyable<uint32_t>(F("ldto"), &params->timeout);
    form.addFormUI(F("Presence Timeout"), FormUI::Suffix(F("seconds")), FormUI::MinMax(0, kMaxTimeout),
        FormUI::SuffixHtml(F("<a class=\"btn btn-secondary\" href=\"ld2420.html\">Gate Thresholds</a>")));
    form.addValidator(FormUI::Validator::Range(0, kMaxTimeout));

    group.end();
}

void Sensor_LD2420::createGateForm(FormUI::Form::BaseForm &form)
{
    auto &ui = form.createWebUI();
    ui.setTitle(F("HLK-LD2420 Gate Thresholds"));
    ui.setContainerId(F("ld2420"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto params = _getFormParams();
    if (!params) {
        auto &group = form.addCardGroup(F("ldgates"), PrintString(F("%s: not responding"), _name.c_str()), true);
        group.end();
        return;
    }

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, kGateCount, ldm, lds);

    auto &group = form.addCardGroup(F("ldgates"), PrintString(F("%s, firmware %s"), _name.c_str(), _version.c_str()), true);
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        // the still threshold is a hidden field shown as second input next to the move threshold
        auto &still = form.addPointerTriviallyCopyable<uint32_t>(F_VAR(lds, gate), &params->still[gate]);
        form.addFormUI(FormUI::Type::HIDDEN);
        form.addValidator(FormUI::Validator::Range(0, kMaxThreshold));

        PrintString label(F("Gate %u ("), gate);
        printGateDistance(label, gate);
        label.print('-');
        printGateDistance(label, gate + 1);
        label.print(F(" m)"));

        form.addPointerTriviallyCopyable<uint32_t>(F_VAR(ldm, gate), &params->move[gate]);
        form.addFormUI(FormUI::Label(label), FormUI::MinMax(0, kMaxThreshold), FormUI::Suffix(F("move")), FormUI::TextInputSuffix(still), FormUI::Suffix(F("still")));
        form.addValidator(FormUI::Validator::Range(0, kMaxThreshold));
    }
    group.end();
}

void Sensor_LD2420::configurationSaved(FormUI::Form::BaseForm *form)
{
    // the form was created in the same request: if _params exists, the form has the fields and _formParams their values
    if (!_params || !_formParams) {
        return;
    }
    if (!_pendingWrite) {
        _pendingWrite.reset(new Params());
    }
    *_pendingWrite = *_formParams;
}

void Sensor_LD2420::reconfigure(PGM_P source)
{
    // the web server calls reconfigure() from the main loop after a form has been saved (invokeReconfigure()): the
    // serial port and the loop function must not be created in the web server context
    _applyPendingWrite();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LD2420, "LD2420", "<read|range,<min gate>,<max gate>|timeout,<seconds>|gate,<0-15|*>,<move>,<still>|factory|restart|mon[,<seconds>[,raw]]>", "HLK-LD2420 radar: read or change the configuration, monitor the reported data");

bool Sensor_LD2420::atModeHandler(AtModeArgs &args)
{
    if (!args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LD2420))) {
        return false;
    }
    auto output = &args.getStream();
    auto cmd = args.toString(0);

    if (F("mon") == cmd) {
        auto seconds = args.toIntMinMax<uint16_t>(1, 0, 300, 10);
        if (_op && _op->type == OpType::MONITOR) {
            _finish();
            if (seconds == 0) {
                args.print(F("monitor stopped"));
                return true;
            }
        }
        if (seconds == 0) {
            return true;
        }
        if (!_start(OpType::MONITOR, output)) {
            args.print(F("ERROR - busy or not enough memory"));
            return true;
        }
        _op->endTime = millis() + seconds * 1000U;
        _op->raw = args.equalsIgnoreCase(2, F("raw"));
        args.print(F("monitor for %us"), seconds);
        return true;
    }

    if (_op) {
        args.print(F("ERROR - busy"));
        return true;
    }

    if (cmd.length() == 0 || F("read") == cmd || F("restart") == cmd) {
        if (!_start((F("restart") == cmd) ? OpType::RESTART : OpType::READ, output)) {
            args.print(F("ERROR - not enough memory"));
        }
        return true;
    }

    if (!_params) {
        args.print(F("ERROR - configuration not read, run +LD2420=read first"));
        return true;
    }

    auto params = *_params;
    if (F("range") == cmd) {
        if (!args.requireArgs(3, 3)) {
            return true;
        }
        params.minGate = args.toIntMinMax<uint32_t>(1, 0, kMaxGate);
        params.maxGate = args.toIntMinMax<uint32_t>(2, 1, kMaxGate);
        if (params.minGate > params.maxGate) {
            args.print(F("ERROR - min gate > max gate"));
            return true;
        }
    }
    else if (F("timeout") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return true;
        }
        params.timeout = args.toIntMinMax<uint32_t>(1, 0, kMaxTimeout);
    }
    else if (F("gate") == cmd) {
        if (!args.requireArgs(4, 4)) {
            return true;
        }
        auto move = args.toIntMinMax<uint32_t>(2, 0, kMaxThreshold);
        auto still = args.toIntMinMax<uint32_t>(3, 0, kMaxThreshold);
        if (args.equals(1, '*')) {
            for(uint8_t gate = 0; gate < kGateCount; gate++) {
                params.move[gate] = move;
                params.still[gate] = still;
            }
        }
        else {
            auto gate = args.toIntMinMax<uint8_t>(1, 0, kMaxGate);
            params.move[gate] = move;
            params.still[gate] = still;
        }
    }
    else if (F("factory") == cmd) {
        params.minGate = kFactoryMinGate;
        params.maxGate = kFactoryMaxGate;
        params.timeout = kFactoryTimeout;
        for(uint8_t gate = 0; gate < kGateCount; gate++) {
            params.move[gate] = _getFactoryMove(gate);
            params.still[gate] = _getFactoryStill(gate);
        }
    }
    else {
        args.invalidArgument(0, F("read|range|timeout|gate|factory|restart|mon"), '|');
        return true;
    }

    _queueWrite(params, output);
    return true;
}

#endif

#endif
