/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_LD2450

#include <math.h>
#include <LoopFunctions.h>
#include <web_socket.h>
#include "Sensor_LD2450.h"
#if IOT_SENSOR_LD2450_PRESENCE
#    include <WebUISocket.h>
#endif
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    const char kName[] PROGMEM = "LD2450";

    constexpr uint16_t kCmdReadVersion = 0x00a0;
    constexpr uint16_t kCmdFactoryReset = 0x00a2;
    constexpr uint16_t kCmdRestart = 0x00a3;
    constexpr uint16_t kCmdBluetooth = 0x00a4;
    constexpr uint16_t kCmdReadMac = 0x00a5;
    constexpr uint16_t kCmdSingleTarget = 0x0080;
    constexpr uint16_t kCmdMultiTarget = 0x0090;
    constexpr uint16_t kCmdQueryTargetMode = 0x0091;
    constexpr uint16_t kCmdReadZone = 0x00c1;
    constexpr uint16_t kCmdSetZone = 0x00c2;
    // data of the command that enables the configuration mode
    constexpr uint8_t kEnableConfigValue = 0x01;

    // data of the commands that take one: 0x01 requests the value, the second byte is reserved
    constexpr uint8_t kQueryData[2] = { 0x01, 0x00 };

    // the address the module reports while Bluetooth is off
    constexpr uint8_t kNoMac[6] = { 0x08, 0x05, 0x04, 0x03, 0x02, 0x01 };

    // target data frame: header, three targets of 8 bytes, footer
    constexpr uint8_t kTargetHeader[4] = { 0xaa, 0xff, 0x03, 0x00 };
    constexpr uint8_t kTargetFooter[2] = { 0x55, 0xcc };
    constexpr uint8_t kTargetFrameLength = sizeof(kTargetHeader) + LD2450::kTargetCount * LD2450::kTargetSize + sizeof(kTargetFooter);

    // the zone type and three areas of four coordinates
    constexpr uint8_t kZoneLength = 2 + LD2450::kZoneCount * 8;
    constexpr uint8_t kMaxZoneType = static_cast<uint8_t>(LD2450::ZoneType::FILTER);

    constexpr uint16_t kBootReadDelay = 3000;
    // the sensor reports about 10 frames per second, the live view sends at most one message per frame
    constexpr uint32_t kStreamInterval = 100;
    // the live view waits in these steps while a command or the monitor holds the serial port
    constexpr uint32_t kStreamRetry = 250;
    #if IOT_SENSOR_LD2450_PRESENCE
        // the presence timeout is checked in this interval
        constexpr uint32_t kPresenceInterval = 250;
        // a changed number of targets is published at most this often, the count of a frame changes 10 times per second
        constexpr uint32_t kTargetsPublishInterval = 1000;
    #endif

    // the steps of a read
    constexpr uint8_t kReadVersion = 0;
    constexpr uint8_t kReadMac = 1;
    constexpr uint8_t kReadMode = 2;
    constexpr uint8_t kReadZone = 3;
    constexpr uint8_t kReadSteps = 4;

    // the steps of a write, bit n of the write mask is step n. Bluetooth is not written: the module applies it after a
    // restart (IOT_SENSOR_LD24XX_BLUETOOTH_OFF)
    constexpr uint8_t kWriteMode = 0;
    constexpr uint8_t kWriteZone = 1;
    constexpr uint8_t kWriteSteps = 2;

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
    LD24xxSensorT(MQTT::SensorType::LD2450, name, {
        kName, IOT_SENSOR_LD2450_BAUD, kBootReadDelay, kCmdRestart, kCmdFactoryReset, kEnableConfigValue, true,
        kCmdBluetooth
    }),
    _live(),
    _socketTimer(),
    _streamTimer(),
    _configPending(false),
    _wsSocket(nullptr)
    #if IOT_SENSOR_LD2450_PRESENCE
        ,
        _presenceTimer(),
        _lastTarget(0),
        _lastTargetsPublish(0),
        _presenceTimeout(_getPresenceTimeout()),
        _presence(0xff),
        _targets(0),
        _publishedTargets(0)
    #endif
{
    _instance = this;
    #if IOT_SENSOR_LD2450_PRESENCE
        REGISTER_SENSOR_CLIENT(this);
    #endif
}

Sensor_LD2450::~Sensor_LD2450()
{
    shutdown();
    #if IOT_SENSOR_LD2450_PRESENCE
        UNREGISTER_SENSOR_CLIENT(this);
    #endif
    if (_instance == this) {
        _instance = nullptr;
    }
}

void Sensor_LD2450::setup()
{
    #if IOT_SENSOR_LD2450_PRESENCE
        // the stream starts when the boot read has finished (_onIdle())
        _Timer(_presenceTimer).add(Event::milliseconds(kPresenceInterval), true, [this](Event::CallbackTimerPtr) {
            _checkPresence();
        });
    #endif
    // the web server is already running when the sensors are set up, the timer retries if it is not (the page cannot
    // find an endpoint that is created later)
    if (!_createSocket()) {
        _Timer(_socketTimer).add(Event::seconds(1), true, [this](Event::CallbackTimerPtr) {
            if (_createSocket()) {
                _Timer(_socketTimer).remove();
            }
        });
    }
    LD24xxSensor::setup();
}

void Sensor_LD2450::shutdown()
{
    #if IOT_SENSOR_LD2450_PRESENCE
        _Timer(_presenceTimer).remove();
    #endif
    _Timer(_socketTimer).remove();
    _Timer(_streamTimer).remove();
    LD24xxSensor::shutdown();
    if (_wsSocket) {
        // closes the pages of the live view, the socket itself cannot be removed from the web server
        _wsSocket->shutdown();
    }
}

void Sensor_LD2450::_printStatus(Print &output) const
{
    if (_params) {
        output.printf_P(PSTR(", target tracking %s, zone area %s"), _params->multiTarget ? PSTR("multi") : PSTR("single"), _getZoneTypeStr(_params->zoneType));
    }
    #if IOT_SENSOR_LD2450_PRESENCE
        if (_presence == 1) {
            output.printf_P(PSTR(HTML_S(br) "presence detected, %u target%s"), _publishedTargets, (_publishedTargets == 1) ? PSTR("") : PSTR("s"));
        }
        else if (_presence == 0) {
            output.print(F(HTML_S(br) "presence clear"));
        }
    #endif
}

uint8_t Sensor_LD2450::_getStepCount(OpType type) const
{
    return (type == OpType::READ) ? kReadSteps : kWriteSteps;
}

bool Sensor_LD2450::_sendStep(OpType type, uint8_t step)
{
    if (type == OpType::READ) {
        switch(step) {
            case kReadVersion:
                _sendFrame(kCmdReadVersion);
                break;
            case kReadMac:
                _sendFrame(kCmdReadMac, kQueryData, sizeof(kQueryData));
                break;
            case kReadMode:
                _sendFrame(kCmdQueryTargetMode);
                break;
            default:
                _sendFrame(kCmdReadZone);
                break;
        }
        return true;
    }
    auto &op = _getOp();
    if ((op.writeMask & (1U << step)) == 0) {
        return false;
    }
    switch(step) {
        case kWriteMode:
            _sendFrame(op.pending.multiTarget ? kCmdMultiTarget : kCmdSingleTarget);
            break;
        default: {
                uint8_t data[kZoneLength];
                auto ptr = LD24xxSerial::putUint16(data, static_cast<uint16_t>(op.pending.zoneType));
                for(const auto &zone: op.pending.zone) {
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.x1));
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.y1));
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.x2));
                    ptr = LD24xxSerial::putUint16(ptr, static_cast<uint16_t>(zone.y2));
                }
                _sendFrame(kCmdSetZone, data, ptr - data);
            }
            break;
    }
    return true;
}

bool Sensor_LD2450::_parseReply(uint8_t step, const uint8_t *data, uint8_t length)
{
    auto &pending = _getOp().pending;
    switch(step) {
        case kReadVersion:
            return _parseVersion(data, length);
        case kReadMac:
            if (length < sizeof(pending.mac)) {
                return false;
            }
            memcpy(pending.mac, data, sizeof(pending.mac));
            pending.bluetooth = (memcmp(pending.mac, kNoMac, sizeof(kNoMac)) != 0);
            return true;
        case kReadMode:
            if (length < 1 || (data[0] != 0x01 && data[0] != 0x02)) {
                return false;
            }
            pending.multiTarget = (data[0] == 0x02);
            return true;
        default:
            break;
    }
    if (length < kZoneLength || data[0] > kMaxZoneType) {
        return false;
    }
    pending.zoneType = static_cast<ZoneType>(data[0]);
    auto ptr = &data[2];
    for(auto &zone: pending.zone) {
        zone.x1 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr));
        zone.y1 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr + 2));
        zone.x2 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr + 4));
        zone.y2 = static_cast<int16_t>(LD24xxSerial::getUint16(ptr + 6));
        ptr += 8;
    }
    return true;
}

void Sensor_LD2450::_printParams(Print &output) const
{
    _prefix(output).printf_P(PSTR("firmware %s, %u baud, bluetooth %s\n"), _version.c_str(), static_cast<unsigned>(_baud), _params->bluetooth ? PSTR("on") : PSTR("off"));
    auto mac = _params->mac;
    _prefix(output).printf_P(PSTR("mac %02x:%02x:%02x:%02x:%02x:%02x\n"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _prefix(output).printf_P(PSTR("target tracking %s\n"), _params->multiTarget ? PSTR("multi") : PSTR("single"));
    _prefix(output).printf_P(PSTR("zone area %s\n"), _getZoneTypeStr(_params->zoneType));
    for(uint8_t i = 0; i < kZoneCount; i++) {
        auto &zone = _params->zone[i];
        _prefix(output).printf_P(PSTR("area %u x %d..%d mm, y %d..%d mm\n"), i, zone.x1, zone.x2, zone.y1, zone.y2);
    }
}

bool Sensor_LD2450::_isBluetoothOn() const
{
    return _params && _params->bluetooth;
}

uint32_t Sensor_LD2450::_getWriteMask(const Params &params) const
{
    uint32_t mask = 0;
    if (params.multiTarget != _params->multiTarget) {
        mask |= (1U << kWriteMode);
    }
    if (params.zoneType != _params->zoneType || memcmp(params.zone, _params->zone, sizeof(params.zone)) != 0) {
        mask |= (1U << kWriteZone);
    }
    return mask;
}

// prints the reported targets: a change every 500ms at most, the state once per second
void Sensor_LD2450::_feedMonitor(uint8_t data)
{
    if (!_feedFrame(data, kTargetHeader, kTargetFooter, sizeof(kTargetFooter), kTargetFrameLength)) {
        return;
    }
    auto targetData = &_op->buffer[sizeof(kTargetHeader)];
    Target targets[kTargetCount];
    auto count = _decodeTargets(targetData, targets);
    #if IOT_SENSOR_LD2450_PRESENCE
        // the monitor holds the serial port, the presence keeps working
        _updatePresence(_countInArea(targets));
    #endif
    if (!_monitorThrottle(targetData, kTargetCount * kTargetSize)) {
        return;
    }
    auto &output = *_op->output;
    if (count == 0) {
        _prefix(output).println(F("no targets"));
        return;
    }
    _prefix(output).printf_P(PSTR("%u target%s"), count, (count == 1) ? PSTR("") : PSTR("s"));
    for(const auto &target: targets) {
        if (target.x || target.y) {
            auto distance = static_cast<uint16_t>(sqrtf(static_cast<float>(target.x) * target.x + static_cast<float>(target.y) * target.y));
            output.printf_P(PSTR(", x %dmm y %dmm %dmm/s %umm away, resolution %umm"), target.x, target.y, target.speed, distance, target.resolution);
            if (!_isInArea(target)) {
                output.print(F(", outside the area"));
            }
        }
    }
    output.println();
}

// the stream: every frame is decoded for the presence, at most one message per 100ms is pushed to the pages of the live
// view
void Sensor_LD2450::_loopStream()
{
    if (!_wantsStream()) {
        // the last page is gone (its close frame can get lost, the socket is the only reliable signal), the serial port
        // goes back to the console
        _live.count = 0;
        _live.pending = false;
        _finish();
        return;
    }
    int data;
    while ((data = _serial->read()) >= 0) {
        if (_feedFrame(static_cast<uint8_t>(data), kTargetHeader, kTargetFooter, sizeof(kTargetFooter), kTargetFrameLength)) {
            _live.count = _decodeTargets(&_op->buffer[sizeof(kTargetHeader)], _live.target);
            _live.stamp = millis();
            _live.pending = true;
            #if IOT_SENSOR_LD2450_PRESENCE
                _updatePresence(_countInArea(_live.target));
            #endif
        }
    }
    if (!_wsSocket || _wsSocket->count() == 0) {
        // the presence keeps the stream, there is no page to send the targets to
        _live.pending = false;
        return;
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

void Sensor_LD2450::_onIdle()
{
    // the live view gets the serial port back
    _restartStream();
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
    sensor._live = Live();
    sensor._restartStream();
}

void Sensor_LD2450::_streamStop()
{
    if (!_instance) {
        return;
    }
    auto &sensor = *_instance;
    sensor._live.pending = false;
    sensor._live.count = 0;
    #if !IOT_SENSOR_LD2450_PRESENCE
        // the presence keeps the stream
        _Timer(sensor._streamTimer).remove();
        if (sensor._op && sensor._op->type == OpType::STREAM) {
            sensor._finish();
        }
    #endif
}

bool Sensor_LD2450::_wantsStream() const
{
    return IOT_SENSOR_LD2450_PRESENCE || (_wsSocket && _wsSocket->count() != 0);
}

void Sensor_LD2450::_restartStream()
{
    if (!_wantsStream() || (_op && _op->type == OpType::STREAM)) {
        _Timer(_streamTimer).remove();
        return;
    }
    if (!_op && _start(OpType::STREAM, nullptr)) {
        _Timer(_streamTimer).remove();
        _live.pending = false;
        _live.stamp = millis();
        _live.lastSend = millis() - kStreamInterval;
        // the configuration may have been read since the page connected
        _configPending = true;
        return;
    }
    // a command or the monitor holds the serial port, or the memory is low
    if (!_streamTimer) {
        _Timer(_streamTimer).add(Event::milliseconds(kStreamRetry), true, [this](Event::CallbackTimerPtr) {
            _restartStream();
        });
    }
}

void Sensor_LD2450::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto params = _getFormParams();
    auto &group = form.addCardGroup(F("ld2cfg"), _getFormTitle(), true);
    if (params) {
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
    }
    #if IOT_SENSOR_LD2450_PRESENCE
        // stored in the firmware, also shown while the module does not answer
        auto &cfg = Plugins::Sensor::getWriteableConfig();
        if (cfg.ld2450.presence_timeout * 1000U != _presenceTimeout) {
            // the form shows the timeout in effect: a configuration that did not exist yet is all zero, one of another
            // layout keeps its old bytes here while getConfig() uses the defaults
            cfg.ld2450.presence_timeout = _presenceTimeout / 1000U;
        }
        form.addObjectGetterSetter(F("ld2pto"), FormGetterSetter(cfg.ld2450, presence_timeout));
        form.addFormUI(F("Presence Timeout"), FormUI::Suffix(F("seconds")));
        cfg.ld2450.addRangeValidatorFor_presence_timeout(form);
    #endif
    group.end();
}

void Sensor_LD2450::createAreaForm(FormUI::Form::BaseForm &form)
{
    auto &ui = form.createWebUI();
    ui.setTitle(F("HLK-LD2450 Zone Areas"));
    ui.setContainerId(F("ld2450"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto params = _getFormParams();
    auto &group = form.addCardGroup(F("ld2areas"), _getFormTitle(), true);
    if (params) {
        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, kZoneCount, ld2x1, ld2y1, ld2x2, ld2y2);

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
    }
    group.end();
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
            break;
    }
    return PSTR("disabled");
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

bool Sensor_LD2450::_isInArea(const Target &target)
{
    if (!target.x && !target.y) {
        return false;
    }
    return target.x >= -kAreaX && target.x <= kAreaX && target.y >= 0 && target.y <= kAreaY;
}

uint8_t Sensor_LD2450::_countInArea(const Target *targets)
{
    uint8_t count = 0;
    for(uint8_t i = 0; i < kTargetCount; i++) {
        if (_isInArea(targets[i])) {
            count++;
        }
    }
    return count;
}

#if IOT_SENSOR_LD2450_PRESENCE

// a target switches the presence on at once, the timer switches it off
void Sensor_LD2450::_updatePresence(uint8_t count)
{
    _targets = count;
    if (count) {
        _lastTarget = millis();
    }
    if (_presence == 0xff || (count && _presence == 0)) {
        // the first frame or a new target
        _presence = count ? 1 : 0;
        _publishPresence();
    }
}

void Sensor_LD2450::_checkPresence()
{
    if (_presence == 1 && static_cast<int32_t>(millis() - _lastTarget) >= static_cast<int32_t>(_presenceTimeout)) {
        // no target within the timeout, also when the module stopped reporting or a command holds the serial port
        _presence = 0;
        _targets = 0;
        _publishPresence();
        return;
    }
    if (_presence != 0xff && _targets != _publishedTargets && static_cast<int32_t>(millis() - _lastTargetsPublish) >= static_cast<int32_t>(kTargetsPublishInterval)) {
        _publishPresence();
    }
}

void Sensor_LD2450::_publishPresence()
{
    _publishedTargets = _targets;
    _lastTargetsPublish = millis();
    publishState();
    _publishWebUI();
}

void Sensor_LD2450::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(
            WebUINS::Events(WebUINS::Values(_getId(), _getStateStr()))
        ));
    }
}

uint32_t Sensor_LD2450::_getPresenceTimeout()
{
    auto cfg = Plugins::Sensor::getConfig().ld2450;
    uint32_t timeout = cfg.presence_timeout;
    if (timeout == 0) {
        // a configuration that did not exist yet is all zero
        timeout = cfg.kDefaultValueFor_presence_timeout;
    }
    return timeout * 1000U;
}

String Sensor_LD2450::_getStateStr() const
{
    PrintString str(F("<div class=\"pt-3\">"));
    switch(_presence) {
        case 1:
            str.print(F("Detected"));
            if (_publishedTargets) {
                str.printf_P(PSTR(", %u target%s"), _publishedTargets, (_publishedTargets == 1) ? PSTR("") : PSTR("s"));
            }
            break;
        case 0:
            str.print(F("Clear"));
            break;
        default:
            str.print(F("Unknown"));
            break;
    }
    str.print(F("</div>"));
    return str;
}

void Sensor_LD2450::publishState()
{
    if (isConnected() && _presence != 0xff) {
        publish(_getTopic(), true, MQTT::Client::toBoolOnOff(_presence != 0));
        publish(_getTargetsTopic(), true, String(_publishedTargets));
    }
}

void Sensor_LD2450::getValues(WebUINS::Events &array, bool timer)
{
    array.append(WebUINS::Values(_getId(), _getStateStr()));
}

void Sensor_LD2450::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(), _name, emptyString).setConfig(_renderConfig)));
}

MQTT::AutoDiscovery::EntityPtr Sensor_LD2450::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, _getId(), format)) {
                discovery->addStateTopic(_getTopic());
                discovery->addDeviceClass(F("occupancy"));
                discovery->addName(F("Presence"));
                discovery->addObjectId(MQTT::Client::getBaseTopicPrefix() + F("presence"));
            }
            break;
        case 1:
            if (discovery->create(this, F("ld2450_targets"), format)) {
                discovery->addStateTopic(_getTargetsTopic());
                discovery->addStateClass(F("measurement"));
                discovery->addIcon(F("mdi:account-multiple"));
                discovery->addName(F("Targets"));
                discovery->addObjectId(MQTT::Client::getBaseTopicPrefix() + F("targets"));
            }
            break;
        default:
            break;
    }
    return discovery;
}

uint8_t Sensor_LD2450::getAutoDiscoveryCount() const
{
    return 2;
}

void Sensor_LD2450::reconfigure(PGM_P source)
{
    _presenceTimeout = _getPresenceTimeout();
    LD24xxSensor::reconfigure(source);
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LD2450, "LD2450", "<read|mode,<single|multi>|zone,<off|disabled|detect|filter>|area,<0-2>,<x1>,<y1>,<x2>,<y2>|area,<0-2>,off|factory|restart|mon[,<seconds>[,raw]]|tx[,<seconds>]>", "HLK-LD2450 radar: read or change the configuration, monitor the reported targets");

bool Sensor_LD2450::atModeHandler(AtModeArgs &args)
{
    if (!args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LD2450))) {
        return false;
    }
    _atModeHandler(args);
    return true;
}

bool Sensor_LD2450::_atModeParams(AtModeArgs &args, const String &cmd, Params &params)
{
    if (F("mode") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return false;
        }
        if (args.equals(1, F("multi"))) {
            params.multiTarget = true;
        }
        else if (args.equals(1, F("single"))) {
            params.multiTarget = false;
        }
        else {
            args.invalidArgument(1, F("single|multi"), '|');
            return false;
        }
    }
    else if (F("zone") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return false;
        }
        if (args.equals(1, F("off"))) {
            // the areas are ignored while the type is disabled
            params.zoneType = ZoneType::DISABLED;
            for(auto &zone: params.zone) {
                zone = Zone();
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
            return false;
        }
    }
    else if (F("area") == cmd) {
        if (!args.requireArgs(3, 6)) {
            return false;
        }
        auto index = _atModeIndex(args, 1, kZoneCount - 1, F("area"));
        if (index < 0) {
            return false;
        }
        auto &zone = params.zone[index];
        if (args.equals(2, F("off"))) {
            zone = Zone();
        }
        else if (args.size() == 6) {
            zone.x1 = args.toIntMinMax<int16_t>(2, INT16_MIN, INT16_MAX);
            zone.y1 = args.toIntMinMax<int16_t>(3, INT16_MIN, INT16_MAX);
            zone.x2 = args.toIntMinMax<int16_t>(4, INT16_MIN, INT16_MAX);
            zone.y2 = args.toIntMinMax<int16_t>(5, INT16_MIN, INT16_MAX);
        }
        else {
            args.invalidArgument(2, F("<x1>,<y1>,<x2>,<y2>"));
            return false;
        }
    }
    else {
        args.invalidArgument(0, F("read|mode|zone|area|factory|restart|mon|tx"), '|');
        return false;
    }
    return true;
}

#endif

#endif
