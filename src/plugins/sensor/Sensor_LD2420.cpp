/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_LD2420

#include <algorithm>
#include "Sensor_LD2420.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    const char kName[] PROGMEM = "LD2420";

    constexpr uint16_t kCmdReadVersion = 0x0000;
    constexpr uint16_t kCmdWriteParams = 0x0007;
    constexpr uint16_t kCmdReadParams = 0x0008;
    constexpr uint16_t kCmdRestart = 0x0068;
    // data of the command that enables the configuration mode: the protocol version
    constexpr uint8_t kEnableConfigValue = 0x02;

    constexpr uint16_t kRegMinGate = 0x0000;
    constexpr uint16_t kRegMaxGate = 0x0001;
    constexpr uint16_t kRegTimeout = 0x0004;
    constexpr uint16_t kRegMoveThreshold = 0x0010;  // + gate
    constexpr uint16_t kRegStillThreshold = 0x0020; // + gate

    // energy frame: header, length, presence, distance, 16 gate energies, footer
    constexpr uint8_t kEnergyHeader[4] = { 0xf4, 0xf3, 0xf2, 0xf1 };
    constexpr uint8_t kEnergyFooter[4] = { 0xf8, 0xf7, 0xf6, 0xf5 };
    constexpr uint8_t kEnergyFrameLength = 4 + 2 + 1 + 2 + LD2420::kGateCount * 2 + 4;

    constexpr uint16_t kBootReadDelay = 5000;

    // the steps of a read: the version, the range and a step per gate
    constexpr uint8_t kReadVersion = 0;
    constexpr uint8_t kReadRange = 1;
    constexpr uint8_t kReadGate = 2;
    constexpr uint8_t kReadSteps = kReadGate + LD2420::kGateCount;

    // the steps of a write, bit n of the write mask is step n: the range, then a step per gate
    constexpr uint8_t kWriteRange = 0;
    constexpr uint8_t kWriteGate = 1;
    constexpr uint8_t kWriteSteps = kWriteGate + LD2420::kGateCount;

    // factory defaults, read from a new module with firmware v1.6.1 (the factory reset of ESPHome uses older values)
    constexpr uint32_t kFactoryMinGate = 0;
    constexpr uint32_t kFactoryMaxGate = 12;
    constexpr uint32_t kFactoryTimeout = 30;
    const uint16_t kFactoryMove[LD2420::kGateCount] PROGMEM = {
        60000, 30000, 3000, 2000, 500, 400, 400, 300, 300, 300, 300, 250, 250, 200, 200, 200
    };
    const uint16_t kFactoryStill[LD2420::kGateCount] PROGMEM = {
        40000, 20000, 400, 300, 300, 200, 200, 150, 150, 100, 100, 100, 100, 100, 100, 100
    };

}

Sensor_LD2420::Sensor_LD2420(const String &name) :
    LD24xxSensorT(MQTT::SensorType::LD2420, name, {
        kName, IOT_SENSOR_LD2420_BAUD, kBootReadDelay, kCmdRestart, 0, kEnableConfigValue, false, 0
    })
{
}

Sensor_LD2420::~Sensor_LD2420()
{
    shutdown();
}

void Sensor_LD2420::_printStatus(Print &output) const
{
    if (_params) {
        output.printf_P(PSTR(", detection gates %u-%u, presence timeout %us"), static_cast<unsigned>(_params->minGate), static_cast<unsigned>(_params->maxGate), static_cast<unsigned>(_params->timeout));
    }
}

uint32_t Sensor_LD2420::_getFactoryMove(uint8_t gate)
{
    return pgm_read_word(&kFactoryMove[gate]);
}

uint32_t Sensor_LD2420::_getFactoryStill(uint8_t gate)
{
    return pgm_read_word(&kFactoryStill[gate]);
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

uint8_t Sensor_LD2420::_getStepCount(OpType type) const
{
    return (type == OpType::READ) ? kReadSteps : kWriteSteps;
}

bool Sensor_LD2420::_sendStep(OpType type, uint8_t step)
{
    if (type == OpType::READ) {
        if (step == kReadVersion) {
            _sendFrame(kCmdReadVersion);
        }
        else if (step == kReadRange) {
            const uint16_t registers[] = { kRegMinGate, kRegMaxGate, kRegTimeout };
            _sendReadParams(registers, 3);
        }
        else {
            uint8_t gate = step - kReadGate;
            const uint16_t registers[] = { static_cast<uint16_t>(kRegMoveThreshold + gate), static_cast<uint16_t>(kRegStillThreshold + gate) };
            _sendReadParams(registers, 2);
        }
        return true;
    }
    auto &op = _getOp();
    if ((op.writeMask & (1U << step)) == 0) {
        return false;
    }
    if (step == kWriteRange) {
        const uint16_t registers[] = { kRegMinGate, kRegMaxGate, kRegTimeout };
        const uint32_t values[] = { op.pending.minGate, op.pending.maxGate, op.pending.timeout };
        _sendWriteParams(registers, values, 3);
    }
    else {
        uint8_t gate = step - kWriteGate;
        const uint16_t registers[] = { static_cast<uint16_t>(kRegMoveThreshold + gate), static_cast<uint16_t>(kRegStillThreshold + gate) };
        const uint32_t values[] = { op.pending.move[gate], op.pending.still[gate] };
        _sendWriteParams(registers, values, 2);
    }
    return true;
}

bool Sensor_LD2420::_parseReply(uint8_t step, const uint8_t *data, uint8_t length)
{
    auto &pending = _getOp().pending;
    if (step == kReadVersion) {
        // the length of the version string followed by the string
        if (length >= 2) {
            _version = String();
            _version.concat(reinterpret_cast<const char *>(&data[2]), std::min<uint16_t>(LD24xxSerial::getUint16(data), length - 2));
        }
        return true;
    }
    if (step == kReadRange) {
        if (length < 12) {
            return false;
        }
        pending.minGate = LD24xxSerial::getUint32(&data[0]);
        pending.maxGate = LD24xxSerial::getUint32(&data[4]);
        pending.timeout = LD24xxSerial::getUint32(&data[8]);
        return pending.maxGate <= kMaxGate && pending.minGate <= pending.maxGate && pending.timeout <= kMaxTimeout;
    }
    if (length < 8) {
        return false;
    }
    uint8_t gate = step - kReadGate;
    pending.move[gate] = LD24xxSerial::getUint32(&data[0]);
    pending.still[gate] = LD24xxSerial::getUint32(&data[4]);
    return pending.move[gate] <= kMaxThreshold && pending.still[gate] <= kMaxThreshold;
}

void Sensor_LD2420::_printParams(Print &output) const
{
    _prefix(output).printf_P(PSTR("firmware %s, %u baud\n"), _version.c_str(), static_cast<unsigned>(_baud));
    _prefix(output).printf_P(PSTR("detection gates %u-%u, presence timeout %us\n"), static_cast<unsigned>(_params->minGate), static_cast<unsigned>(_params->maxGate), static_cast<unsigned>(_params->timeout));
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        _prefix(output).printf_P(PSTR("gate %2u move %5u still %5u\n"), gate, static_cast<unsigned>(_params->move[gate]), static_cast<unsigned>(_params->still[gate]));
    }
}

uint32_t Sensor_LD2420::_getWriteMask(const Params &params) const
{
    uint32_t mask = 0;
    if (params.minGate != _params->minGate || params.maxGate != _params->maxGate || params.timeout != _params->timeout) {
        mask |= (1U << kWriteRange);
    }
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        if (params.move[gate] != _params->move[gate] || params.still[gate] != _params->still[gate]) {
            mask |= (1U << (kWriteGate + gate));
        }
    }
    return mask;
}

// the module reports text lines (simple mode) or energy frames (energy mode)
void Sensor_LD2420::_feedMonitor(uint8_t data)
{
    auto &op = *_op;
    if ((op.length == 0) ? (data == kEnergyHeader[0]) : (op.buffer[0] == kEnergyHeader[0])) {
        if (!_feedFrame(data, kEnergyHeader, kEnergyFooter, sizeof(kEnergyFooter), kEnergyFrameLength)) {
            return;
        }
        // presence, distance and the energy of the gates
        auto frame = &op.buffer[6];
        const uint8_t key[3] = { frame[0], frame[1], frame[2] };
        if (!_monitorThrottle(key, sizeof(key), 1)) {
            return;
        }
        auto &output = *op.output;
        _prefix(output).printf_P(PSTR("presence=%u distance=%ucm energy="), frame[0], LD24xxSerial::getUint16(&frame[1]));
        for(uint8_t gate = 0; gate < kGateCount; gate++) {
            output.printf_P(PSTR("%s%u"), gate ? PSTR(",") : PSTR(""), LD24xxSerial::getUint16(&frame[3 + gate * 2]));
        }
        output.println();
        return;
    }
    if (data == '\r' || data == '\n') {
        if (op.length) {
            op.buffer[op.length] = 0;
            op.length = 0;
            _printMonitorLine();
        }
        return;
    }
    if (!isprint(data) || op.length >= sizeof(op.buffer) - 1) {
        op.length = 0;
        return;
    }
    op.buffer[op.length++] = data;
}

// the simple mode reports "ON" or "OFF" and "Range <cm>" about 20 times per second
void Sensor_LD2420::_printMonitorLine()
{
    auto &op = *_op;
    auto line = reinterpret_cast<const char *>(op.buffer);
    if (strcmp_P(line, PSTR("ON")) == 0 || strcmp_P(line, PSTR("OFF")) == 0) {
        op.monitorPresence = (line[1] == 'N') ? 1 : 0;
    }
    else if (strncmp_P(line, PSTR("Range "), 6) == 0) {
        op.monitorDistance = atoi(line + 6);
    }
    else {
        _prefix(*op.output).printf_P(PSTR("%s\n"), line);
        return;
    }
    const uint8_t key[3] = { op.monitorPresence, static_cast<uint8_t>(op.monitorDistance), static_cast<uint8_t>(op.monitorDistance >> 8) };
    if (_monitorThrottle(key, sizeof(key), 1)) {
        _prefix(*op.output).printf_P(PSTR("presence %s, distance %ucm\n"), (op.monitorPresence == 1) ? PSTR("ON") : PSTR("OFF"), op.monitorDistance);
    }
}

void Sensor_LD2420::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto params = _getFormParams();
    auto &group = form.addCardGroup(F("ldcfg"), _getFormTitle(), true);
    if (params) {
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
    }
    group.end();
}

void Sensor_LD2420::createGateForm(FormUI::Form::BaseForm &form)
{
    auto &ui = form.createWebUI();
    ui.setTitle(F("HLK-LD2420 Gate Thresholds"));
    ui.setContainerId(F("ld2420"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto params = _getFormParams();
    auto &group = form.addCardGroup(F("ldgates"), _getFormTitle(), true);
    if (params) {
        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, kGateCount, ldm, lds);
        for(uint8_t gate = 0; gate < kGateCount; gate++) {
            _addGateField<uint32_t>(form, gate, kGateDistance, F_VAR(ldm, gate), params->move[gate], F_VAR(lds, gate), params->still[gate], kMaxThreshold);
        }
    }
    group.end();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LD2420, "LD2420", "<read|range,<min gate>,<max gate>|timeout,<seconds>|gate,<0-15|*>,<move>,<still>|factory|restart|mon[,<seconds>[,raw]]|tx[,<seconds>]>", "HLK-LD2420 radar: read or change the configuration, monitor the reported data");

bool Sensor_LD2420::atModeHandler(AtModeArgs &args)
{
    if (!args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LD2420))) {
        return false;
    }
    _atModeHandler(args);
    return true;
}

bool Sensor_LD2420::_atModeParams(AtModeArgs &args, const String &cmd, Params &params)
{
    if (F("range") == cmd) {
        if (!args.requireArgs(3, 3)) {
            return false;
        }
        params.minGate = args.toIntMinMax<uint32_t>(1, 0, kMaxGate);
        params.maxGate = args.toIntMinMax<uint32_t>(2, 1, kMaxGate);
        if (params.minGate > params.maxGate) {
            args.print(F("ERROR - min gate > max gate"));
            return false;
        }
    }
    else if (F("timeout") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return false;
        }
        params.timeout = args.toIntMinMax<uint32_t>(1, 0, kMaxTimeout);
    }
    else if (F("gate") == cmd) {
        if (!args.requireArgs(4, 4)) {
            return false;
        }
        bool all = args.equals(1, '*');
        auto gate = all ? 0 : _atModeIndex(args, 1, kMaxGate, F("gate"));
        if (gate < 0) {
            return false;
        }
        auto move = args.toIntMinMax<uint32_t>(2, 0, kMaxThreshold);
        auto still = args.toIntMinMax<uint32_t>(3, 0, kMaxThreshold);
        for(uint8_t i = 0; i < kGateCount; i++) {
            if (all || i == gate) {
                params.move[i] = move;
                params.still[i] = still;
            }
        }
    }
    else if (F("factory") == cmd) {
        // the module has no factory reset command, the defaults are written
        params.minGate = kFactoryMinGate;
        params.maxGate = kFactoryMaxGate;
        params.timeout = kFactoryTimeout;
        for(uint8_t gate = 0; gate < kGateCount; gate++) {
            params.move[gate] = _getFactoryMove(gate);
            params.still[gate] = _getFactoryStill(gate);
        }
    }
    else {
        args.invalidArgument(0, F("read|range|timeout|gate|factory|restart|mon|tx"), '|');
        return false;
    }
    return true;
}

#endif

#endif
