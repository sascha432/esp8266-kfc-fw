/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_LD2450

#include <algorithm>
#include <math.h>
#include <LoopFunctions.h>
#include <web_socket.h>
#include "Sensor_LD2450.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    constexpr uint16_t kCmdEnableConfig = LD24xxSerial::kCmdEnableConfig;
    constexpr uint16_t kCmdDisableConfig = LD24xxSerial::kCmdDisableConfig;
    constexpr uint16_t kCmdReadVersion = 0x00a0;
    constexpr uint16_t kCmdReadMac = 0x00a5;
    constexpr uint16_t kCmdFactoryReset = 0x00a2;
    constexpr uint16_t kCmdRestart = 0x00a3;
    constexpr uint16_t kCmdBluetooth = 0x00a4;
    constexpr uint16_t kCmdSingleTarget = 0x0080;
    constexpr uint16_t kCmdMultiTarget = 0x0090;
    constexpr uint16_t kCmdQueryTargetMode = 0x0091;
    constexpr uint16_t kCmdReadZone = 0x00c1;
    constexpr uint16_t kCmdSetZone = 0x00c2;

    // data of the commands that take one: 0x01 requests the value, the second byte is reserved
    constexpr uint8_t kQueryData[2] = { 0x01, 0x00 };

    // the address the module reports while Bluetooth is off
    constexpr uint8_t kNoMac[6] = { 0x08, 0x05, 0x04, 0x03, 0x02, 0x01 };

    // target data frame: header, three targets of 8 bytes, footer
    constexpr uint8_t kTargetHeader[4] = { 0xaa, 0xff, 0x03, 0x00 };
    constexpr uint8_t kTargetFooter[2] = { 0x55, 0xcc };
    constexpr uint8_t kTargetFrameLength = sizeof(kTargetHeader) + Sensor_LD2450::kTargetCount * Sensor_LD2450::kTargetSize + sizeof(kTargetFooter);

    // the zone type and three areas of four coordinates
    constexpr uint8_t kZoneLength = 2 + Sensor_LD2450::kZoneCount * 8;
    constexpr uint8_t kMaxZoneType = static_cast<uint8_t>(Sensor_LD2450::ZoneType::FILTER);
    // the version string follows two bytes in the reply data
    constexpr uint8_t kVersionOffset = 2;
    constexpr uint8_t kVersionLength = 6;

    // the sensor replies within about 50ms
    constexpr uint32_t kReplyTimeout = 300;
    // pause between a reply and the next command
    constexpr uint32_t kCommandGap = 10;
    constexpr uint8_t kMaxRetries = 3;
    constexpr uint32_t kBootReadDelay = 3000;
    constexpr uint32_t kMonitorOutputInterval = 500;
    constexpr uint32_t kMonitorRepeatInterval = 1000;
    constexpr uint32_t kFallbackBaud = 115200;
    // a saved form waits for a running command in these steps
    constexpr uint32_t kPendingWriteRetry = 250;
    // the sensor reports about 10 frames per second, the live view sends at most one message per frame
    constexpr uint32_t kStreamInterval = 100;

    // steps of OpType::READ
    constexpr uint8_t kReadEnable = 0;
    constexpr uint8_t kReadVersion = 1;
    constexpr uint8_t kReadMac = 2;
    constexpr uint8_t kReadMode = 3;
    constexpr uint8_t kReadZone = 4;
    constexpr uint8_t kReadDisable = kReadZone + 1;

    // steps of OpType::WRITE, bit 0 of writeMask is the tracking mode, bit 1 Bluetooth and bit 2 the zones
    constexpr uint8_t kWriteEnable = 0;
    constexpr uint8_t kWriteMode = 1;
    constexpr uint8_t kWriteBluetooth = 2;
    constexpr uint8_t kWriteZone = 3;
    constexpr uint8_t kWriteDisable = kWriteZone + 1;

    // steps of OpType::RESTART and OpType::FACTORY
    constexpr uint8_t kRestartEnable = 0;
    constexpr uint8_t kRestartSend = 1;

}

Sensor_LD2450 *Sensor_LD2450::_instance = nullptr;

// one page of the live view: the stream runs while at least one page is connected
class Sensor_LD2450::WsTargets : public WsClient {
public:
    WsTargets(AsyncWebSocketClient *client) : WsClient(client) {}

    static WsClient *getInstance(AsyncWebSocketClient *client) {
        return new WsTargets(client);
    }

    // the first page opened: the serial port is opened and the targets are pushed
    virtual void onStart() override {
        LoopFunctions::callOnce([]() {
            _streamStart();
        });
    }

    // the last page closed: the serial port goes back to the console
    virtual void onEnd() override {
        LoopFunctions::callOnce([]() {
            _streamStop();
        });
    }

    // the page draws the zone areas, the configuration is sent by the main loop
    virtual void onAuthenticated(uint8_t *data, size_t len) override {
        if (_instance) {
            _instance->_configPending = true;
        }
    }
};

Sensor_LD2450::Sensor_LD2450(const String &name) :
    MQTT::Sensor(MQTT::SensorType::LD2450),
    _name(name),
    _serial(),
    _op(),
    _params(),
    _formParams(),
    _pendingWrite(),
    _live(),
    _timer(),
    _socketTimer(),
    _configPending(false),
    _wsSocket(nullptr),
    _baud(IOT_SENSOR_LD2450_BAUD)
{
    _instance = this;
}

Sensor_LD2450::~Sensor_LD2450()
{
    shutdown();
    if (_instance == this) {
        _instance = nullptr;
    }
}

void Sensor_LD2450::setup()
{
    // the web server is already running when the sensors are set up, the timer retries if it is not (the page cannot
    // find an endpoint that is created later)
    if (!_createSocket()) {
        _Timer(_socketTimer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr) {
            if (_createSocket()) {
                _Timer(_socketTimer).remove();
            }
        });
    }
    // the sensor needs a moment after power on
    _Timer(_timer).add(Event::milliseconds(kBootReadDelay), false, [this](Event::CallbackTimerPtr) {
        _start(OpType::READ, nullptr);
    });
}

void Sensor_LD2450::shutdown()
{
    _Timer(_timer).remove();
    _Timer(_socketTimer).remove();
    if (_op) {
        LoopFunctions::remove(this);
        _op.reset();
    }
    _serial.reset();
    if (_wsSocket) {
        // closes the pages of the live view, the socket itself cannot be removed from the web server
        _wsSocket->shutdown();
    }
}

void Sensor_LD2450::getStatus(Print &output)
{
    output.print(_name);
    if (_params) {
        char version[20];
        _formatVersion(_params->version, version, sizeof(version));
        output.printf_P(PSTR(" firmware %s, target tracking %s, zone area %s"), version, _params->multiTarget ? PSTR("multi") : PSTR("single"), _getZoneTypeStr(_params->zoneType));
        if (_op) {
            // the serial console is paused while the live view keeps the port open
            output.printf_P(PSTR(HTML_S(br) "%s running"), _getOpTypeStr(_op->type));
        }
    }
    else if (_op) {
        output.print(F(" reading the configuration"));
    }
    else {
        output.print(F(" not responding"));
    }
    output.println(F(HTML_S(br)));
}

// returns false if an operation is running or the memory is too low for the serial port buffers
bool Sensor_LD2450::_start(OpType type, Stream *output)
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

void Sensor_LD2450::_openSerial()
{
    // the destructor disables the interrupt of the pin: close the old port before opening the new one
    _serial.reset();
    _serial.reset(new LD24xxSerial(_baud));
}

void Sensor_LD2450::_finish(PGM_P error)
{
    auto output = _op->output;
    if (error) {
        __LDBG_printf("error=%s type=%u step=%u", error, _op->type, _op->step);
        // leave the configuration mode of an operation that failed after entering it
        if (_op->type != OpType::MONITOR && _op->step > 0) {
            _serial->sendCommand(kCmdDisableConfig, nullptr, 0);
        }
        if (output) {
            output->printf_P(PSTR("+LD2450: ERROR - %s\n"), error);
        }
    }
    LoopFunctions::remove(this);
    _op.reset();
    _serial.reset();
    if (error) {
        // the live view gets the serial port back
        _restartStream();
    }
}

void Sensor_LD2450::_sendFrame(uint16_t command, const uint8_t *data, uint8_t length)
{
    _serial->sendCommand(command, data, length);
    _op->command = command;
}

// sends the frame of the current step and returns true, false if the step has nothing to send
bool Sensor_LD2450::_sendStep()
{
    auto &op = *_op;
    auto step = op.step;
    switch(op.type) {
        case OpType::READ:
            if (step == kReadEnable) {
                _sendFrame(kCmdEnableConfig, kQueryData, sizeof(kQueryData));
            }
            else if (step == kReadVersion) {
                _sendFrame(kCmdReadVersion, nullptr, 0);
            }
            else if (step == kReadMac) {
                _sendFrame(kCmdReadMac, kQueryData, sizeof(kQueryData));
            }
            else if (step == kReadMode) {
                _sendFrame(kCmdQueryTargetMode, nullptr, 0);
            }
            else if (step == kReadZone) {
                _sendFrame(kCmdReadZone, nullptr, 0);
            }
            else {
                _sendFrame(kCmdDisableConfig, nullptr, 0);
            }
            return true;
        case OpType::WRITE:
            if (step == kWriteEnable) {
                _sendFrame(kCmdEnableConfig, kQueryData, sizeof(kQueryData));
            }
            else if (step == kWriteMode) {
                if ((op.writeMask & 1) == 0) {
                    return false;
                }
                _sendFrame(op.pending.multiTarget ? kCmdMultiTarget : kCmdSingleTarget, nullptr, 0);
            }
            else if (step == kWriteBluetooth) {
                if ((op.writeMask & 2) == 0) {
                    return false;
                }
                const uint8_t data[2] = { static_cast<uint8_t>(op.pending.bluetooth ? 0x01 : 0x00), 0x00 };
                _sendFrame(kCmdBluetooth, data, sizeof(data));
            }
            else if (step == kWriteZone) {
                if ((op.writeMask & 4) == 0) {
                    return false;
                }
                uint8_t data[kZoneLength];
                auto ptr = data;
                ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(op.pending.zoneType));
                for(uint8_t i = 0; i < kZoneCount; i++) {
                    auto &zone = op.pending.zone[i];
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.x1));
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.y1));
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.x2));
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.y2));
                }
                _sendFrame(kCmdSetZone, data, ptr - data);
            }
            else {
                _sendFrame(kCmdDisableConfig, nullptr, 0);
            }
            return true;
        case OpType::RESTART:
        case OpType::FACTORY:
            if (step == kRestartEnable) {
                _sendFrame(kCmdEnableConfig, kQueryData, sizeof(kQueryData));
            }
            else {
                _sendFrame((op.type == OpType::FACTORY) ? kCmdFactoryReset : kCmdRestart, nullptr, 0);
            }
            return true;
        default:
            break;
    }
    return false;
}

// processes the reply that the serial port received, returns false if it is not the reply to the last command
bool Sensor_LD2450::_handleReply()
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
            op.output->printf_P(PSTR("+LD2450: command 0x%04x returned error %u\n"), op.command, error);
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
            valid = dataLength >= kVersionOffset + kVersionLength;
            if (valid) {
                memcpy(op.pending.version, &data[kVersionOffset], sizeof(op.pending.version));
            }
        }
        else if (step == kReadMac) {
            valid = dataLength >= sizeof(op.pending.mac);
            if (valid) {
                memcpy(op.pending.mac, data, sizeof(op.pending.mac));
                op.pending.bluetooth = (memcmp(op.pending.mac, kNoMac, sizeof(kNoMac)) != 0);
            }
        }
        else if (step == kReadMode) {
            valid = dataLength >= 1 && (data[0] == 0x01 || data[0] == 0x02);
            if (valid) {
                op.pending.multiTarget = (data[0] == 0x02);
            }
        }
        else if (step == kReadZone) {
            valid = dataLength >= kZoneLength && data[0] <= kMaxZoneType;
            if (valid) {
                op.pending.zoneType = static_cast<ZoneType>(data[0]);
                auto ptr = &data[2];
                for(uint8_t i = 0; i < kZoneCount; i++) {
                    auto &zone = op.pending.zone[i];
                    zone.x1 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr));
                    zone.y1 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr + 2));
                    zone.x2 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr + 4));
                    zone.y2 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr + 6));
                    ptr += 8;
                }
            }
        }
        if (!valid) {
            if (op.output) {
                op.output->printf_P(PSTR("+LD2450: invalid reply, command 0x%04x, step %u\n"), op.command, op.step);
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

void Sensor_LD2450::_complete()
{
    auto type = _op->type;
    auto output = _op->output;
    if (type == OpType::READ) {
        if (!_params) {
            _params.reset(new Params());
        }
        *_params = _op->pending;
    }
    if (output && type != OpType::RESTART && type != OpType::FACTORY) {
        output->printf_P(PSTR("+LD2450: %u retries, %u receive errors, max. reply time %ums\n"), _op->totalRetries, _serial->getErrors(), static_cast<unsigned>(_op->maxReplyTime));
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
                output->println(F("+LD2450: configuration written, reading it back"));
            }
            // verify the values that the sensor stored
            _start(OpType::READ, output);
            break;
        case OpType::RESTART:
            if (output) {
                output->println(F("+LD2450: restarting the sensor, wait a few seconds before the next command"));
            }
            break;
        case OpType::FACTORY:
            if (output) {
                output->println(F("+LD2450: factory defaults restored, the sensor restarts, wait a few seconds before the next command"));
            }
            break;
        default:
            break;
    }
    _restartStream();
}

void Sensor_LD2450::_loop()
{
    if (!_op) {
        return;
    }
    auto &op = *_op;
    if (op.type == OpType::MONITOR) {
        _loopMonitor();
        return;
    }
    if (op.type == OpType::STREAM) {
        _loopStream();
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
        if ((op.type == OpType::RESTART || op.type == OpType::FACTORY) && op.step == kRestartSend) {
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
        op.output->printf_P(PSTR("+LD2450: timeout, command 0x%04x, step %u, %u bytes received\n"), op.command, op.step, _serial->getReceivedBytes());
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
        _baud = (_baud == kFallbackBaud) ? IOT_SENSOR_LD2450_BAUD : kFallbackBaud;
        __LDBG_printf("no reply, trying %u baud", _baud);
        _openSerial();
        return;
    }
    if (op.baudFallback && op.step == 0) {
        // back to the default for the next operation
        _baud = IOT_SENSOR_LD2450_BAUD;
    }
    if (op.replyError) {
        _finish(PSTR("command failed"));
        return;
    }
    if (op.output) {
        op.output->printf_P(PSTR("+LD2450: no reply to command 0x%04x, step %u, %u receive errors\n"), op.command, op.step, _serial->getErrors());
    }
    _finish(PSTR("no reply from the sensor"));
}

// prints the target frames the sensor reports, or the received bytes
void Sensor_LD2450::_loopMonitor()
{
    auto &op = *_op;
    if (static_cast<int32_t>(millis() - op.endTime) >= 0) {
        if (op.output) {
            op.output->println(F("+LD2450: monitor end"));
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
        if (_feedTargetFrame(static_cast<uint8_t>(data))) {
            _printMonitorFrame(*op.output);
        }
    }
}

// feeds one byte of the target frames, returns true when a complete frame is in _op->buffer
bool Sensor_LD2450::_feedTargetFrame(uint8_t data)
{
    auto &op = *_op;
    if (op.length == 0) {
        if (data == kTargetHeader[0]) {
            op.buffer[op.length++] = data;
        }
        return false;
    }
    if (op.length < sizeof(kTargetHeader) && data != kTargetHeader[op.length]) {
        // not the header of a target frame, a byte that begins one is kept
        op.length = 0;
        if (data == kTargetHeader[0]) {
            op.buffer[op.length++] = data;
        }
        return false;
    }
    op.buffer[op.length++] = data;
    if (op.length < kTargetFrameLength) {
        return false;
    }
    op.length = 0;
    return memcmp(&op.buffer[kTargetFrameLength - sizeof(kTargetFooter)], kTargetFooter, sizeof(kTargetFooter)) == 0;
}

// the live view: every frame is decoded, at most one message per 100ms is pushed to the pages
void Sensor_LD2450::_loopStream()
{
    auto &op = *_op;
    if (!_wsSocket || _wsSocket->count() == 0) {
        // the last page is gone (its close frame can get lost, the socket is the only reliable signal), the serial port
        // goes back to the console
        _live.count = 0;
        _live.pending = false;
        _finish();
        return;
    }
    int data;
    while ((data = _serial->read()) >= 0) {
        if (_feedTargetFrame(static_cast<uint8_t>(data))) {
            _updateLive(&op.buffer[sizeof(kTargetHeader)]);
        }
    }
    if (_configPending) {
        _configPending = false;
        _sendConfig();
    }
    if (_live.pending && static_cast<int32_t>(millis() - _live.lastSend) >= static_cast<int32_t>(kStreamInterval)) {
        _live.pending = false;
        _live.lastSend = millis();
        _broadcastTargets();
    }
}

void Sensor_LD2450::_updateLive(const uint8_t *data)
{
    _live.count = _decodeTargets(data, _live.target);
    _live.stamp = millis();
    _live.pending = true;
}

// the configuration of the sensor for the live view: the page draws the zone areas with it
void Sensor_LD2450::_sendConfig()
{
    if (!_wsSocket || !_params) {
        return;
    }
    PrintString json;
    json.print(F("{\"cfg\":{\"multi\":"));
    json.printf_P(PSTR("%u,\"bluetooth\":%u,\"zone\":%u,\"areas\":["), _params->multiTarget ? 1 : 0, _params->bluetooth ? 1 : 0, static_cast<unsigned>(_params->zoneType));
    for(uint8_t i = 0; i < kZoneCount; i++) {
        auto &zone = _params->zone[i];
        json.printf_P(PSTR("%s[%d,%d,%d,%d]"), i ? PSTR(",") : PSTR(""), zone.x1, zone.y1, zone.x2, zone.y2);
    }
    json.print(F("]}}"));
    WsClient::broadcast(_wsSocket, nullptr, std::move(json));
}

// pushes the targets to the pages: x and y in mm, the speed in mm/s and the resolution in mm
void Sensor_LD2450::_broadcastTargets()
{
    PrintString json;
    json.print(F("{\"t\":["));
    for(uint8_t i = 0; i < kTargetCount; i++) {
        auto &target = _live.target[i];
        json.printf_P(PSTR("%s[%d,%d,%d,%u]"), i ? PSTR(",") : PSTR(""), target.x, target.y, target.speed, static_cast<unsigned>(target.resolution));
    }
    json.print(F("]}"));
    WsClient::broadcast(_wsSocket, nullptr, std::move(json));
}

bool Sensor_LD2450::_createSocket()
{
    if (_wsSocket) {
        return true;
    }
    auto server = WebServer::Plugin::getWebServerObject();
    if (!server) {
        return false;
    }
    // the web socket that pushes the targets of the live view (/ld2450-radar.html)
    auto ws = new WsClientAsyncWebSocket(F("/ld2450-live"), &_wsSocket);
    ws->onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        WsClient::onWsEvent(server, client, static_cast<int>(type), data, len, arg, WsTargets::getInstance);
    });
    server->addHandler(ws);
    __LDBG_printf("web socket=%p", ws);
    return true;
}

void Sensor_LD2450::_streamStart()
{
    if (!_instance) {
        return;
    }
    auto &sensor = *_instance;
    // the page must not show the targets of the last session
    sensor._live.count = 0;
    sensor._live.pending = false;
    sensor._live.stamp = millis();
    sensor._live.lastSend = millis() - kStreamInterval;
    for(uint8_t i = 0; i < kTargetCount; i++) {
        sensor._live.target[i] = Target();
    }
    sensor._start(OpType::STREAM, nullptr);
}

void Sensor_LD2450::_streamStop()
{
    if (!_instance) {
        return;
    }
    auto &sensor = *_instance;
    if (sensor._op && sensor._op->type == OpType::STREAM) {
        sensor._finish();
    }
    sensor._live.pending = false;
    sensor._live.count = 0;
}

// the live view gets the serial port back after a command
void Sensor_LD2450::_restartStream()
{
    if (_op || !_wsSocket || _wsSocket->count() == 0) {
        return;
    }
    if (_start(OpType::STREAM, nullptr)) {
        _live.pending = false;
        _live.stamp = millis();
        _live.lastSend = millis() - kStreamInterval;
    }
}

void Sensor_LD2450::_printMonitorRaw(Print &output)
{
    auto &op = *_op;
    output.print(F("+LD2450:"));
    for(uint8_t i = 0; i < op.length; i++) {
        output.printf_P(PSTR(" %02x"), op.buffer[i]);
    }
    output.println();
    op.length = 0;
}

void Sensor_LD2450::_printMonitorFrame(Print &output)
{
    auto &op = *_op;
    auto data = &op.buffer[sizeof(kTargetHeader)];
    auto elapsed = static_cast<int32_t>(millis() - op.lastOutput);
    if (elapsed < static_cast<int32_t>(kMonitorOutputInterval) || (elapsed < static_cast<int32_t>(kMonitorRepeatInterval) && memcmp(op.targets, data, sizeof(op.targets)) == 0)) {
        // the sensor reports about 20 frames per second: print a change every 500ms and the state once per second
        return;
    }
    memcpy(op.targets, data, sizeof(op.targets));
    op.lastOutput = millis();

    Target targets[kTargetCount];
    auto count = _decodeTargets(data, targets);
    if (count == 0) {
        output.println(F("+LD2450: no targets"));
        return;
    }
    output.printf_P(PSTR("+LD2450: %u target%s"), count, (count == 1) ? PSTR("") : PSTR("s"));
    for(uint8_t i = 0; i < kTargetCount; i++) {
        auto &target = targets[i];
        if (target.x || target.y) {
            auto distance = static_cast<uint16_t>(sqrtf(static_cast<float>(target.x) * target.x + static_cast<float>(target.y) * target.y));
            output.printf_P(PSTR(", x %dmm y %dmm %dmm/s %umm away, resolution %umm"), target.x, target.y, target.speed, distance, target.resolution);
        }
    }
    output.println();
}

void Sensor_LD2450::_printParams(Print &output) const
{
    char version[20];
    _formatVersion(_params->version, version, sizeof(version));
    output.printf_P(PSTR("+LD2450: firmware %s, %u baud, bluetooth %s\n"), version, static_cast<unsigned>(_baud), _params->bluetooth ? PSTR("on") : PSTR("off"));
    output.printf_P(PSTR("+LD2450: mac %02x:%02x:%02x:%02x:%02x:%02x\n"), _params->mac[0], _params->mac[1], _params->mac[2], _params->mac[3], _params->mac[4], _params->mac[5]);
    output.printf_P(PSTR("+LD2450: target tracking %s\n"), _params->multiTarget ? PSTR("multi") : PSTR("single"));
    output.printf_P(PSTR("+LD2450: zone area %s\n"), _getZoneTypeStr(_params->zoneType));
    for(uint8_t i = 0; i < kZoneCount; i++) {
        auto &zone = _params->zone[i];
        output.printf_P(PSTR("+LD2450: area %u x %d..%d mm, y %d..%d mm\n"), i, zone.x1, zone.x2, zone.y1, zone.y2);
    }
}

// writes the changed values, prints "no changes" or an error to output if nothing gets written
void Sensor_LD2450::_queueWrite(const Params &params, Stream *output)
{
    uint32_t mask = 0;
    if (params.multiTarget != _params->multiTarget) {
        mask |= 1;
    }
    if (params.bluetooth != _params->bluetooth) {
        mask |= 2;
    }
    if (params.zoneType != _params->zoneType || memcmp(params.zone, _params->zone, sizeof(params.zone)) != 0) {
        mask |= 4;
    }
    if (mask == 0) {
        if (output) {
            output->println(F("+LD2450: no changes"));
        }
        return;
    }
    if (!_start(OpType::WRITE, output)) {
        if (output) {
            output->println(F("+LD2450: ERROR - busy or not enough memory"));
        }
        return;
    }
    _op->pending = params;
    _op->writeMask = mask;
}

// the values a form shows: a saved form that has not been written yet, the one being written or the ones read from the
// sensor. returns nullptr if the configuration has not been read
Sensor_LD2450::Params *Sensor_LD2450::_getFormParams()
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

void Sensor_LD2450::_applyPendingWrite()
{
    if (!_pendingWrite) {
        return;
    }
    if (_op && _op->type == OpType::STREAM) {
        // the values are written while the pages of the live view wait, the stream restarts afterwards
        _finish();
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

void Sensor_LD2450::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto params = _getFormParams();
    if (!params) {
        auto &group = form.addCardGroup(F("ld2cfg"), PrintString(F("%s: not responding"), _name.c_str()), true);
        group.end();
        return;
    }

    auto &group = form.addCardGroup(F("ld2cfg"), PrintString(F("%s, firmware %s"), _name.c_str(), _getVersion().c_str()), true);

    form.addCallbackGetterSetter<uint8_t>(F("ld2mode"), [params](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            params->multiTarget = (value == 1);
        }
        else {
            value = params->multiTarget ? 1 : 0;
        }
        return true;
    });
    form.addFormUI(F("Target Tracking"), FormUI::List(
        0, F("Single Target"),
        1, F("Multiple Targets")
    ));

    form.addCallbackGetterSetter<uint8_t>(F("ld2zone"), [params](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            if (value <= kMaxZoneType) {
                params->zoneType = static_cast<ZoneType>(value);
            }
        }
        else {
            value = static_cast<uint8_t>(params->zoneType);
        }
        return true;
    });
    form.addFormUI(F("Zone Area"), FormUI::List(
        static_cast<uint8_t>(ZoneType::DISABLED), F("Disabled"),
        static_cast<uint8_t>(ZoneType::DETECTION), F("Detection"),
        static_cast<uint8_t>(ZoneType::FILTER), F("Filter")
    ), FormUI::SuffixHtml(F("<a class=\"btn btn-secondary\" href=\"ld2450-radar.html\">Live View</a> <a class=\"btn btn-secondary\" href=\"ld2450.html\">Zone Areas</a>")));

    group.end();
}

void Sensor_LD2450::createAreaForm(FormUI::Form::BaseForm &form)
{
    auto &ui = form.createWebUI();
    ui.setTitle(F("HLK-LD2450 Zone Areas"));
    ui.setContainerId(F("ld2450"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto params = _getFormParams();
    if (!params) {
        auto &group = form.addCardGroup(F("ld2areas"), PrintString(F("%s: not responding"), _name.c_str()), true);
        group.end();
        return;
    }

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, kZoneCount, ld2x1, ld2y1, ld2x2, ld2y2);

    auto &group = form.addCardGroup(F("ld2areas"), PrintString(F("%s, firmware %s"), _name.c_str(), _getVersion().c_str()), true);
    for(uint8_t i = 0; i < kZoneCount; i++) {
        auto &zone = params->zone[i];
        auto label = PrintString(F("Area %u "), i + 1);

        form.addPointerTriviallyCopyable<int16_t>(F_VAR(ld2x1, i), &zone.x1);
        form.addFormUI(FormUI::Label(label + F("X1")), FormUI::MinMax(INT16_MIN, INT16_MAX), FormUI::Suffix(F("mm")));
        form.addValidator(FormUI::Validator::Range(INT16_MIN, INT16_MAX));

        form.addPointerTriviallyCopyable<int16_t>(F_VAR(ld2y1, i), &zone.y1);
        form.addFormUI(FormUI::Label(label + F("Y1")), FormUI::MinMax(INT16_MIN, INT16_MAX), FormUI::Suffix(F("mm")));
        form.addValidator(FormUI::Validator::Range(INT16_MIN, INT16_MAX));

        form.addPointerTriviallyCopyable<int16_t>(F_VAR(ld2x2, i), &zone.x2);
        form.addFormUI(FormUI::Label(label + F("X2")), FormUI::MinMax(INT16_MIN, INT16_MAX), FormUI::Suffix(F("mm")));
        form.addValidator(FormUI::Validator::Range(INT16_MIN, INT16_MAX));

        form.addPointerTriviallyCopyable<int16_t>(F_VAR(ld2y2, i), &zone.y2);
        form.addFormUI(FormUI::Label(label + F("Y2")), FormUI::MinMax(INT16_MIN, INT16_MAX), FormUI::Suffix(F("mm")));
        form.addValidator(FormUI::Validator::Range(INT16_MIN, INT16_MAX));
    }
    group.end();
}

void Sensor_LD2450::configurationSaved(FormUI::Form::BaseForm *form)
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

void Sensor_LD2450::reconfigure(PGM_P source)
{
    // the web server calls reconfigure() from the main loop after a form has been saved (invokeReconfigure()): the
    // serial port and the loop function must not be created in the web server context
    _applyPendingWrite();
}

void Sensor_LD2450::_formatVersion(const uint8_t *version, char *buffer, size_t size)
{
    snprintf_P(buffer, size, PSTR("%u.%02X.%02X%02X%02X%02X"),
        static_cast<unsigned>(version[1]), static_cast<unsigned>(version[0]), static_cast<unsigned>(version[5]),
        static_cast<unsigned>(version[4]), static_cast<unsigned>(version[3]), static_cast<unsigned>(version[2]));
}

String Sensor_LD2450::_getVersion() const
{
    char version[20];
    _formatVersion(_params->version, version, sizeof(version));
    return String(version);
}

PGM_P Sensor_LD2450::_getZoneTypeStr(ZoneType type)
{
    switch(type) {
        case ZoneType::DETECTION:
            return PSTR("detection");
        case ZoneType::FILTER:
            return PSTR("filter");
        case ZoneType::DISABLED:
        default:
            return PSTR("disabled");
    }
}

PGM_P Sensor_LD2450::_getOpTypeStr(OpType type)
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
        case OpType::MONITOR:
            return PSTR("monitor");
        case OpType::STREAM:
            return PSTR("live view");
        case OpType::NONE:
        default:
            return PSTR("none");
    }
}

// the coordinates and the speed are a magnitude with the sign in the highest bit, not a two's complement value
int16_t Sensor_LD2450::_getCoordinate(const uint8_t *ptr)
{
    int16_t value = (ptr[1] & 0x7f) << 8 | ptr[0];
    return (ptr[1] & 0x80) ? value : -value;
}

int32_t Sensor_LD2450::_getSpeed(const uint8_t *ptr)
{
    int32_t value = (ptr[1] & 0x7f) << 8 | ptr[0];
    value = (ptr[1] & 0x80) ? value : -value;
    // 0.1 cm/s per count
    return value * 10;
}

// decodes the three targets of a frame, returns the number of targets that are not empty
uint8_t Sensor_LD2450::_decodeTargets(const uint8_t *data, Target *targets)
{
    uint8_t count = 0;
    for(uint8_t i = 0; i < kTargetCount; i++) {
        auto ptr = &data[i * kTargetSize];
        auto &target = targets[i];
        target.x = _getCoordinate(ptr);
        target.y = _getCoordinate(ptr + 2);
        target.speed = _getSpeed(ptr + 4);
        target.resolution = LD24xxSerial::getUint16(ptr + 6);
        if (target.x || target.y) {
            count++;
        }
    }
    return count;
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LD2450, "LD2450", "<read|mode,<single|multi>|zone,<off|disabled|detect|filter>|area,<0-2>,<x1>,<y1>,<x2>,<y2>|area,<0-2>,off|bluetooth,<on|off>|factory|restart|mon[,<seconds>[,raw]]>", "HLK-LD2450 radar: read or change the configuration, monitor the reported targets");

bool Sensor_LD2450::atModeHandler(AtModeArgs &args)
{
    if (!args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LD2450))) {
        return false;
    }
    auto output = &args.getStream();
    auto cmd = args.toString(0);

    if (F("mon") == cmd) {
        auto seconds = args.toIntMinMax<uint16_t>(1, 0, 300, 10);
        if (_op && _op->type == OpType::MONITOR) {
            _finish();
            if (seconds == 0) {
                _restartStream();
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
        args.print(F("ERROR - busy, %s running"), _getOpTypeStr(_op->type));
        return true;
    }

    if (cmd.length() == 0 || F("read") == cmd || F("restart") == cmd || F("factory") == cmd) {
        auto type = (F("restart") == cmd) ? OpType::RESTART : (F("factory") == cmd) ? OpType::FACTORY : OpType::READ;
        if (!_start(type, output)) {
            args.print(F("ERROR - not enough memory"));
        }
        return true;
    }

    if (!_params) {
        args.print(F("ERROR - configuration not read, run +LD2450=read first"));
        return true;
    }

    auto params = *_params;
    if (F("mode") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return true;
        }
        if (args.equals(1, F("multi"))) {
            params.multiTarget = true;
        }
        else if (args.equals(1, F("single"))) {
            params.multiTarget = false;
        }
        else {
            args.invalidArgument(1, F("single|multi"), '|');
            return true;
        }
    }
    else if (F("zone") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return true;
        }
        if (args.equals(1, F("off"))) {
            // the areas are ignored while the type is disabled
            params.zoneType = ZoneType::DISABLED;
            for(uint8_t i = 0; i < kZoneCount; i++) {
                params.zone[i] = Zone();
            }
        }
        else if (args.equals(1, F("disabled"))) {
            params.zoneType = ZoneType::DISABLED;
        }
        else if (args.equals(1, F("detect"))) {
            params.zoneType = ZoneType::DETECTION;
        }
        else if (args.equals(1, F("filter"))) {
            params.zoneType = ZoneType::FILTER;
        }
        else {
            args.invalidArgument(1, F("off|disabled|detect|filter"), '|');
            return true;
        }
    }
    else if (F("area") == cmd) {
        if (!args.requireArgs(3, 6)) {
            return true;
        }
        auto index = args.toIntMinMax<uint8_t>(1, 0, kZoneCount - 1);
        if (args.equals(2, F("off"))) {
            params.zone[index] = Zone();
        }
        else if (args.size() == 6) {
            params.zone[index].x1 = args.toIntMinMax<int16_t>(2, INT16_MIN, INT16_MAX);
            params.zone[index].y1 = args.toIntMinMax<int16_t>(3, INT16_MIN, INT16_MAX);
            params.zone[index].x2 = args.toIntMinMax<int16_t>(4, INT16_MIN, INT16_MAX);
            params.zone[index].y2 = args.toIntMinMax<int16_t>(5, INT16_MIN, INT16_MAX);
        }
        else {
            args.invalidArgument(2, F("<x1>,<y1>,<x2>,<y2>"));
            return true;
        }
    }
    else if (F("bluetooth") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return true;
        }
        if (args.isTrue(1)) {
            params.bluetooth = true;
        }
        else if (args.isFalse(1)) {
            params.bluetooth = false;
        }
        else {
            args.invalidArgument(1, F("on|off"), '|');
            return true;
        }
    }
    else {
        args.invalidArgument(0, F("read|mode|zone|area|bluetooth|factory|restart|mon"), '|');
        return true;
    }

    _queueWrite(params, output);
    return true;
}

#endif

#endif
