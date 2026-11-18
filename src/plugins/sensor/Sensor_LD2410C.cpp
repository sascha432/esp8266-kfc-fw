/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_LD2410C

#include <LoopFunctions.h>
#include <WebUISocket.h>
#include <web_socket.h>
#include "Sensor_LD2410C.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    const char kName[] PROGMEM = "LD2410C";

    constexpr uint16_t kCmdSetDistances = 0x0060;
    constexpr uint16_t kCmdReadParams = 0x0061;
    // the data frames carry the energy of every gate until the module restarts
    constexpr uint16_t kCmdEnableEngineering = 0x0062;
    constexpr uint16_t kCmdSetGate = 0x0064;
    constexpr uint16_t kCmdReadVersion = 0x00a0;
    constexpr uint16_t kCmdFactoryReset = 0x00a2;
    constexpr uint16_t kCmdRestart = 0x00a3;
    constexpr uint16_t kCmdBluetooth = 0x00a4;
    constexpr uint16_t kCmdReadMac = 0x00a5;
    // data of the MAC query
    constexpr uint8_t kQueryData[2] = { 0x01, 0x00 };
    // the address the module reports while Bluetooth is off
    constexpr uint8_t kNoMac[6] = { 0x08, 0x05, 0x04, 0x03, 0x02, 0x01 };
    // data of the command that enables the configuration mode
    constexpr uint8_t kEnableConfigValue = 0x01;

    // the keys of the values in a write command, each followed by a 32 bit value
    constexpr uint16_t kKeyMaxMoveGate = 0x0000;
    constexpr uint16_t kKeyMaxStillGate = 0x0001;
    constexpr uint16_t kKeyDuration = 0x0002;
    constexpr uint16_t kKeyGate = 0x0000;
    constexpr uint16_t kKeyMoveSensitivity = 0x0001;
    constexpr uint16_t kKeyStillSensitivity = 0x0002;

    // data frame: header, length, data type, data head, target state, distances, footer
    constexpr uint8_t kFrameHeader[4] = { 0xf4, 0xf3, 0xf2, 0xf1 };
    constexpr uint8_t kFrameFooter[4] = { 0xf8, 0xf7, 0xf6, 0xf5 };
    constexpr uint8_t kDataHead = 0xaa;
    // header, length, type, data head, state, moving distance and energy, still distance and energy, detection
    // distance, footer
    constexpr uint8_t kMinFrameLength = 4 + 2 + 11 + 4;

    // offsets in a data frame
    constexpr uint8_t kFrameType = 6;
    constexpr uint8_t kFrameDataHead = 7;
    constexpr uint8_t kFrameTargetState = 8;
    constexpr uint8_t kFrameMovingDistance = 9;
    constexpr uint8_t kFrameMovingEnergy = 11;
    constexpr uint8_t kFrameStillDistance = 12;
    constexpr uint8_t kFrameStillEnergy = 14;
    constexpr uint8_t kFrameDetectDistance = 15;
    // engineering mode: the last moving gate N and the last still gate M, the moving energy of gates 0..N, the still
    // energy of gates 0..M, the light value and the state of OUT, then the tail of the basic frame (0x55, 0x00)
    constexpr uint8_t kFrameTypeEngineering = 0x01;
    constexpr uint8_t kFrameGates = 17;
    // bytes from the header to the end of the data: the length field counts from the type
    constexpr uint8_t kFrameDataOffset = 6;
    constexpr uint8_t kFrameTailLength = 2;

    // the reply of the parameter read: data head, 2 reserved bytes, the values
    constexpr uint8_t kParamHead = 0xaa;
    constexpr uint8_t kParamDataLength = 24;
    constexpr uint8_t kParamMaxMoveGate = 2;
    constexpr uint8_t kParamMaxStillGate = 3;
    constexpr uint8_t kParamMoveSensitivity = 4;
    constexpr uint8_t kParamStillSensitivity = kParamMoveSensitivity + LD2410C::kGateCount;
    constexpr uint8_t kParamDuration = kParamStillSensitivity + LD2410C::kGateCount;

    constexpr uint16_t kBootReadDelay = 3000;
    // the module reports about 10 frames per second, the live view sends at most one message per frame
    constexpr uint32_t kStreamInterval = 100;
    // the live view waits in these steps while a command or the monitor holds the serial port
    constexpr uint32_t kStreamRetry = 250;
    // a frame without the gates starts a read at most this often: a module that does not switch to engineering mode
    // would be read again and again
    constexpr uint32_t kEngineeringReadInterval = 10000;

    // the steps of a read
    constexpr uint8_t kReadVersion = 0;
    constexpr uint8_t kReadParams = 1;
    // the Bluetooth MAC address, a module with Bluetooth off reports kNoMac
    constexpr uint8_t kReadMac = 2;
    // switches engineering mode on, the reply has no data
    constexpr uint8_t kReadEngineering = 3;
    constexpr uint8_t kReadSteps = 4;

    // the steps of a write, bit n of the write mask is step n: the max. gates and the nobody duration, then a step per
    // gate
    constexpr uint8_t kWriteDistances = 0;
    constexpr uint8_t kWriteGate = 1;
    constexpr uint8_t kWriteSteps = kWriteGate + LD2410C::kGateCount;

    uint8_t *putParam(uint8_t *ptr, uint16_t key, uint32_t value)
    {
        ptr = LD24xxSerial::putUint16(ptr, key);
        return LD24xxSerial::putUint32(ptr, value);
    }

}

Sensor_LD2410C *Sensor_LD2410C::_instance = nullptr;

// one page of the live view: the stream runs while at least one page is connected
class Sensor_LD2410C::WsGates : public WsClient {
public:
    WsGates(AsyncWebSocketClient *client) : WsClient(client) {}

    static WsClient *getInstance(AsyncWebSocketClient *client) {
        return new WsGates(client);
    }

    // the first page opened: the serial port is opened and the frames are pushed
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

    // the page greys out the gates beyond the maximum, the configuration is sent by the main loop
    virtual void onAuthenticated(uint8_t *data, size_t len) override {
        if (_instance) {
            _instance->_configPending = true;
        }
    }
};

Sensor_LD2410C::Sensor_LD2410C(const String &name) :
    LD24xxSensorT(MQTT::SensorType::LD2410C, name, {
        kName, IOT_SENSOR_LD2410C_BAUD, kBootReadDelay, kCmdRestart, kCmdFactoryReset, kEnableConfigValue, true,
        kCmdBluetooth
    }),
    _presenceTimer(),
    _outPin(IOT_SENSOR_LD2410C_OUT_PIN),
    _presence(0xff),
    _monitorGates(false),
    _monitorNoGates(false),
    _live(),
    _socketTimer(),
    _streamTimer(),
    _engineeringRead(0),
    _configPending(false),
    _wsSocket(nullptr)
{
    _instance = this;
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_LD2410C::~Sensor_LD2410C()
{
    shutdown();
    UNREGISTER_SENSOR_CLIENT(this);
    if (_instance == this) {
        _instance = nullptr;
    }
}

void Sensor_LD2410C::setup()
{
    #if IOT_SENSOR_LD2410C_OUT_PIN != -1
        pinMode(_outPin, INPUT);
        _Timer(_presenceTimer).add(Event::milliseconds(IOT_SENSOR_LD2410C_POLL_INTERVAL), true, [this](Event::CallbackTimerPtr) {
            _readPresence();
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

void Sensor_LD2410C::shutdown()
{
    _Timer(_presenceTimer).remove();
    _Timer(_socketTimer).remove();
    _Timer(_streamTimer).remove();
    LD24xxSensor::shutdown();
    if (_wsSocket) {
        // closes the pages of the live view, the socket itself cannot be removed from the web server
        _wsSocket->shutdown();
    }
}

void Sensor_LD2410C::_readPresence()
{
    #if IOT_SENSOR_LD2410C_OUT_PIN != -1
        bool presence = digitalRead(_outPin) == HIGH;
        #if IOT_SENSOR_LD2410C_OUT_INVERTED
            presence = !presence;
        #endif
        if (_presence == 0xff || (_presence != 0) != presence) {
            _presence = presence ? 1 : 0;
            publishState();
            _publishWebUI();
        }
    #endif
}

void Sensor_LD2410C::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(
            WebUINS::Events(WebUINS::Values(_getId(), _getStateStr()))
        ));
    }
}

void Sensor_LD2410C::publishState()
{
    #if IOT_SENSOR_LD2410C_OUT_PIN != -1
        if (isConnected() && _presence != 0xff) {
            publish(_getTopic(), true, MQTT::Client::toBoolOnOff(_presence != 0));
        }
    #endif
}

void Sensor_LD2410C::getValues(WebUINS::Events &array, bool timer)
{
    array.append(WebUINS::Values(_getId(), PrintString(F("<div class=\"pt-3\">%s</div>"), _getStateStr())));
}

void Sensor_LD2410C::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(), _name, emptyString).setConfig(_renderConfig)));
}

MQTT::AutoDiscovery::EntityPtr Sensor_LD2410C::getAutoDiscovery(FormatType format, uint8_t num)
{
    #if IOT_SENSOR_LD2410C_OUT_PIN != -1
        auto discovery = new MQTT::AutoDiscovery::Entity();
        if (num == 0) {
            if (!discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, _getId(), format)) {
                return discovery;
            }
            discovery->addStateTopic(_getTopic());
            discovery->addDeviceClass(F("occupancy"));
            discovery->addName(F("Presence"));
            discovery->addObjectId(MQTT::Client::getBaseTopicPrefix() + F("presence"));
        }
        return discovery;
    #else
        return nullptr;
    #endif
}

uint8_t Sensor_LD2410C::getAutoDiscoveryCount() const
{
    #if IOT_SENSOR_LD2410C_OUT_PIN != -1
        return 1;
    #else
        return 0;
    #endif
}

void Sensor_LD2410C::_printStatus(Print &output) const
{
    if (_params) {
        output.printf_P(PSTR(", gates 1-%u/%u, nobody duration %us"), _params->maxMoveGate, _params->maxStillGate, _params->duration);
    }
    #if IOT_SENSOR_LD2410C_OUT_PIN != -1
        if (_presence != 0xff) {
            output.printf_P(PSTR(HTML_S(br) "presence %s"), _presence ? PSTR("detected") : PSTR("clear"));
        }
    #endif
}

uint8_t Sensor_LD2410C::_getStepCount(OpType type) const
{
    return (type == OpType::READ) ? kReadSteps : kWriteSteps;
}

bool Sensor_LD2410C::_sendStep(OpType type, uint8_t step)
{
    if (type == OpType::READ) {
        switch(step) {
            case kReadVersion:
                _sendFrame(kCmdReadVersion);
                break;
            case kReadMac:
                _sendFrame(kCmdReadMac, kQueryData, sizeof(kQueryData));
                break;
            case kReadEngineering:
                _sendFrame(kCmdEnableEngineering);
                break;
            default:
                _sendFrame(kCmdReadParams);
                break;
        }
        return true;
    }
    auto &op = _getOp();
    if ((op.writeMask & (1U << step)) == 0) {
        return false;
    }
    uint8_t data[18];
    auto ptr = data;
    if (step == kWriteDistances) {
        ptr = putParam(ptr, kKeyMaxMoveGate, op.pending.maxMoveGate);
        ptr = putParam(ptr, kKeyMaxStillGate, op.pending.maxStillGate);
        ptr = putParam(ptr, kKeyDuration, op.pending.duration);
        _sendFrame(kCmdSetDistances, data, ptr - data);
        return true;
    }
    uint8_t gate = step - kWriteGate;
    ptr = putParam(ptr, kKeyGate, gate);
    ptr = putParam(ptr, kKeyMoveSensitivity, op.pending.move[gate]);
    ptr = putParam(ptr, kKeyStillSensitivity, op.pending.still[gate]);
    _sendFrame(kCmdSetGate, data, ptr - data);
    return true;
}

bool Sensor_LD2410C::_parseReply(uint8_t step, const uint8_t *data, uint8_t length)
{
    if (step == kReadVersion) {
        return _parseVersion(data, length);
    }
    if (step == kReadEngineering) {
        // the status of the reply has been checked
        return true;
    }
    if (step == kReadMac) {
        auto &pending = _getOp().pending;
        if (length < sizeof(pending.mac)) {
            return false;
        }
        memcpy(pending.mac, data, sizeof(pending.mac));
        pending.bluetooth = (memcmp(pending.mac, kNoMac, sizeof(kNoMac)) != 0);
        return true;
    }
    if (length < kParamDataLength || data[0] != kParamHead) {
        return false;
    }
    auto &pending = _getOp().pending;
    pending.maxMoveGate = data[kParamMaxMoveGate];
    pending.maxStillGate = data[kParamMaxStillGate];
    pending.duration = LD24xxSerial::getUint16(&data[kParamDuration]);
    if (pending.maxMoveGate > kMaxGate || pending.maxStillGate > kMaxGate) {
        return false;
    }
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        pending.move[gate] = data[kParamMoveSensitivity + gate];
        pending.still[gate] = data[kParamStillSensitivity + gate];
        if (pending.move[gate] > kMaxSensitivity || pending.still[gate] > kMaxSensitivity) {
            return false;
        }
    }
    return true;
}

void Sensor_LD2410C::_printParams(Print &output) const
{
    _prefix(output).printf_P(PSTR("firmware %s, %u baud, bluetooth %s\n"), _version.c_str(), static_cast<unsigned>(_baud), _params->bluetooth ? PSTR("on") : PSTR("off"));
    _prefix(output).printf_P(PSTR("mac %02x:%02x:%02x:%02x:%02x:%02x\n"), _params->mac[0], _params->mac[1], _params->mac[2], _params->mac[3], _params->mac[4], _params->mac[5]);
    _prefix(output).printf_P(PSTR("max. moving gate %u, max. still gate %u, nobody duration %us\n"), _params->maxMoveGate, _params->maxStillGate, static_cast<unsigned>(_params->duration));
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        _prefix(output).printf_P(PSTR("gate %u move %3u still %3u\n"), gate, _params->move[gate], _params->still[gate]);
    }
}

bool Sensor_LD2410C::_isBluetoothOn() const
{
    return _params && _params->bluetooth;
}

uint32_t Sensor_LD2410C::_getWriteMask(const Params &params) const
{
    uint32_t mask = 0;
    if (params.maxMoveGate != _params->maxMoveGate || params.maxStillGate != _params->maxStillGate || params.duration != _params->duration) {
        mask |= (1U << kWriteDistances);
    }
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        if (params.move[gate] != _params->move[gate] || params.still[gate] != _params->still[gate]) {
            mask |= (1U << (kWriteGate + gate));
        }
    }
    return mask;
}

bool Sensor_LD2410C::_decodeGates(const uint8_t *frame, Gates &gates)
{
    if (frame[kFrameType] != kFrameTypeEngineering) {
        return false;
    }
    uint16_t end = kFrameDataOffset + LD24xxSerial::getUint16(&frame[4]);
    uint8_t moveGates = frame[kFrameGates] + 1;
    uint8_t stillGates = frame[kFrameGates + 1] + 1;
    uint8_t offset = kFrameGates + 2;
    if (moveGates > kGateCount || stillGates > kGateCount || offset + moveGates + stillGates + kFrameTailLength > end) {
        return false;
    }
    gates = Gates();
    memcpy(gates.move, &frame[offset], moveGates);
    offset += moveGates;
    memcpy(gates.still, &frame[offset], stillGates);
    offset += stillGates;
    // the light value and OUT follow on the LD2410B/C
    if (offset + 2 + kFrameTailLength <= end) {
        gates.light = frame[offset];
        gates.out = frame[offset + 1];
    }
    return true;
}

void Sensor_LD2410C::_printGates(Print &output, const Gates &gates) const
{
    _prefix(output).print(F("gate  "));
    for(uint8_t gate = 0; gate < kGateCount; gate++) {
        output.printf_P(PSTR("%4u "), gate);
    }
    output.printf_P(PSTR("  light %u, out %u\n"), gates.light, gates.out);
    for(uint8_t still = 0; still < 2; still++) {
        auto energy = still ? gates.still : gates.move;
        _prefix(output).print(still ? F("still ") : F("move  "));
        for(uint8_t gate = 0; gate < kGateCount; gate++) {
            char mark = ' ';
            if (_params && gate <= (still ? _params->maxStillGate : _params->maxMoveGate) && energy[gate] > (still ? _params->still[gate] : _params->move[gate])) {
                mark = '*';
            }
            output.printf_P(PSTR("%4u%c"), energy[gate], mark);
        }
        output.println();
    }
}

// prints the reported presence and distances: a presence change at once, a distance change every 500ms at most and the
// state once per second. With the gates (mon,<seconds>,gates) a change of their energy is printed every 500ms at most
void Sensor_LD2410C::_feedMonitor(uint8_t data)
{
    if (!_feedFrame(data, kFrameHeader, kFrameFooter, sizeof(kFrameFooter), 0)) {
        return;
    }
    auto frame = _op->buffer;
    if (LD24xxSerial::getUint16(&frame[4]) + 10U < kMinFrameLength || frame[kFrameDataHead] != kDataHead) {
        return;
    }
    bool presence = frame[kFrameTargetState] != 0;
    // the distance is reported without presence as well
    auto detectDistance = presence ? LD24xxSerial::getUint16(&frame[kFrameDetectDistance]) : 0;
    Gates gates;
    bool hasGates = _monitorGates && _decodeGates(frame, gates);
    uint8_t key[3 + 2 * kGateCount] = { presence, static_cast<uint8_t>(detectDistance), static_cast<uint8_t>(detectDistance >> 8) };
    uint8_t keyLength = 3;
    if (hasGates) {
        memcpy(&key[3], gates.move, kGateCount);
        memcpy(&key[3 + kGateCount], gates.still, kGateCount);
        keyLength = sizeof(key);
    }
    if (!_monitorThrottle(key, keyLength, 1)) {
        return;
    }
    auto &output = *_op->output;
    if (!presence) {
        _prefix(output).println(F("presence clear"));
    }
    else {
        _prefix(output).printf_P(PSTR("presence detected, moving %ucm (energy %u), still %ucm (energy %u), detected %ucm\n"),
            LD24xxSerial::getUint16(&frame[kFrameMovingDistance]), frame[kFrameMovingEnergy],
            LD24xxSerial::getUint16(&frame[kFrameStillDistance]), frame[kFrameStillEnergy], detectDistance);
    }
    if (hasGates) {
        _printGates(output, gates);
    }
    else if (_monitorGates && !_monitorNoGates) {
        _monitorNoGates = true;
        _prefix(output).printf_P(PSTR("the frames have no gates, the module is not in engineering mode: +%s=read switches it on\n"), _protocol.name);
    }
}

// the live view: every frame is decoded, at most one message per 100ms is pushed to the pages
void Sensor_LD2410C::_loopStream()
{
    if (!_wsSocket || _wsSocket->count() == 0) {
        // the last page is gone (its close frame can get lost, the socket is the only reliable signal), the serial port
        // goes back to the console
        _live.pending = false;
        _finish();
        return;
    }
    int data;
    while ((data = _serial->read()) >= 0) {
        if (!_feedFrame(static_cast<uint8_t>(data), kFrameHeader, kFrameFooter, sizeof(kFrameFooter), 0)) {
            continue;
        }
        auto frame = _op->buffer;
        if (LD24xxSerial::getUint16(&frame[4]) + 10U < kMinFrameLength || frame[kFrameDataHead] != kDataHead) {
            continue;
        }
        Gates gates;
        bool hasGates = _decodeGates(frame, gates);
        if (!hasGates && static_cast<int32_t>(millis() - _engineeringRead) >= static_cast<int32_t>(kEngineeringReadInterval)) {
            // ends the stream, it restarts after the read
            _readEngineeringMode();
            return;
        }
        if (hasGates && _live.pending && _live.hasGates) {
            // the peak of the frames between two messages
            for(uint8_t gate = 0; gate < kGateCount; gate++) {
                _live.gates.move[gate] = std::max(_live.gates.move[gate], gates.move[gate]);
                _live.gates.still[gate] = std::max(_live.gates.still[gate], gates.still[gate]);
            }
            _live.gates.light = gates.light;
            _live.gates.out = gates.out;
        }
        else {
            _live.gates = gates;
        }
        _live.hasGates = hasGates;
        _live.state = frame[kFrameTargetState];
        _live.moveDistance = LD24xxSerial::getUint16(&frame[kFrameMovingDistance]);
        _live.moveEnergy = frame[kFrameMovingEnergy];
        _live.stillDistance = LD24xxSerial::getUint16(&frame[kFrameStillDistance]);
        _live.stillEnergy = frame[kFrameStillEnergy];
        _live.detectDistance = LD24xxSerial::getUint16(&frame[kFrameDetectDistance]);
        _live.pending = true;
    }
    if (_configPending) {
        _configPending = false;
        _sendConfig();
    }
    if (_live.pending && static_cast<int32_t>(millis() - _live.lastSend) >= static_cast<int32_t>(kStreamInterval)) {
        _live.pending = false;
        _live.lastSend = millis();
        _broadcastGates();
    }
}

void Sensor_LD2410C::_readEngineeringMode()
{
    _engineeringRead = millis();
    _close();
    // the read switches engineering mode on, _onIdle() restarts the live view when it has finished
    if (!_start(OpType::READ, nullptr)) {
        _onIdle();
    }
}

void Sensor_LD2410C::_onIdle()
{
    // the live view gets the serial port back
    _restartStream();
}

// the maximum gates and the nobody duration: the page greys out the gates that the module ignores
void Sensor_LD2410C::_sendConfig()
{
    if (!_wsSocket || !_params) {
        return;
    }
    PrintString json(F("{\"cfg\":{\"mm\":%u,\"ms\":%u,\"dur\":%u}}"), _params->maxMoveGate, _params->maxStillGate, static_cast<unsigned>(_params->duration));
    WsClient::broadcast(_wsSocket, nullptr, std::move(json));
}

// pushes a frame to the pages: target state, moving distance and energy, still distance and energy, detection distance,
// the energy of every gate (the peak since the last message), the light value and OUT
void Sensor_LD2410C::_broadcastGates()
{
    PrintString json(F("{\"f\":[%u,%u,%u,%u,%u,%u]"), _live.state, _live.moveDistance, _live.moveEnergy, _live.stillDistance, _live.stillEnergy, _live.detectDistance);
    if (_live.hasGates) {
        for(uint8_t still = 0; still < 2; still++) {
            auto energy = still ? _live.gates.still : _live.gates.move;
            json.print(still ? F("],\"s\":[") : F(",\"m\":["));
            for(uint8_t gate = 0; gate < kGateCount; gate++) {
                if (gate) {
                    json.print(',');
                }
                json.print(energy[gate]);
            }
        }
        json.printf_P(PSTR("],\"l\":%u,\"o\":%u"), _live.gates.light, _live.gates.out);
    }
    json.print('}');
    WsClient::broadcast(_wsSocket, nullptr, std::move(json));
}

bool Sensor_LD2410C::_createSocket()
{
    if (_wsSocket) {
        return true;
    }
    auto server = WebServer::Plugin::getWebServerObject();
    if (!server) {
        return false;
    }
    // the web socket that pushes the frames of the live view (/ld2410c.html)
    auto ws = new WsClientAsyncWebSocket(F("/ld2410c-live"), &_wsSocket);
    ws->onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        WsClient::onWsEvent(server, client, static_cast<int>(type), data, len, arg, WsGates::getInstance);
    });
    server->addHandler(ws);
    __LDBG_printf("web socket=%p", ws);
    return true;
}

void Sensor_LD2410C::_streamStart()
{
    if (!_instance) {
        return;
    }
    auto &sensor = *_instance;
    // the page must not show the frames of the last session
    sensor._live = Live();
    sensor._restartStream();
}

void Sensor_LD2410C::_streamStop()
{
    if (!_instance) {
        return;
    }
    auto &sensor = *_instance;
    _Timer(sensor._streamTimer).remove();
    if (sensor._op && sensor._op->type == OpType::STREAM) {
        sensor._finish();
    }
    sensor._live.pending = false;
}

void Sensor_LD2410C::_restartStream()
{
    if (!_wsSocket || _wsSocket->count() == 0 || (_op && _op->type == OpType::STREAM)) {
        _Timer(_streamTimer).remove();
        return;
    }
    if (!_op && _start(OpType::STREAM, nullptr)) {
        _Timer(_streamTimer).remove();
        _live.pending = false;
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

void Sensor_LD2410C::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto params = _getFormParams();
    auto &group = form.addCardGroup(F("lccfg"), _getFormTitle(), true);
    if (params) {
        form.addPointerTriviallyCopyable<uint8_t>(F("lcmm"), &params->maxMoveGate);
        form.addFormUI(F("Max. Moving Gate"), FormUI::Suffix(F("0.75 m each")), FormUI::MinMax(1, kMaxGate));
        form.addValidator(FormUI::Validator::Range(1, kMaxGate));

        form.addPointerTriviallyCopyable<uint8_t>(F("lcms"), &params->maxStillGate);
        form.addFormUI(F("Max. Still Gate"), FormUI::Suffix(F("0.75 m each")), FormUI::MinMax(1, kMaxGate));
        form.addValidator(FormUI::Validator::Range(1, kMaxGate));

        form.addPointerTriviallyCopyable<uint16_t>(F("lcdur"), &params->duration);
        form.addFormUI(F("Nobody Duration"), FormUI::Suffix(F("seconds")), FormUI::MinMax(0, kMaxDuration),
            FormUI::SuffixHtml(F("<a class=\"btn btn-secondary\" href=\"ld2410c.html\">Gate Sensitivity</a>")));
        form.addValidator(FormUI::Validator::Range(0, kMaxDuration));
    }
    group.end();
}

void Sensor_LD2410C::createGateForm(FormUI::Form::BaseForm &form)
{
    auto &ui = form.createWebUI();
    ui.setTitle(F("HLK-LD2410C Gate Sensitivity"));
    ui.setContainerId(F("ld2410c"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto params = _getFormParams();
    auto &group = form.addCardGroup(F("lcgates"), _getFormTitle(), true);
    if (params) {
        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, kGateCount, lcm, lcs);
        for(uint8_t gate = 0; gate < kGateCount; gate++) {
            _addGateField<uint8_t>(form, gate, kGateDistance, F_VAR(lcm, gate), params->move[gate], F_VAR(lcs, gate), params->still[gate], kMaxSensitivity);
        }
    }
    group.end();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(LD2410C, "LD2410C", "<read|range,<max move gate>,<max still gate>|timeout,<seconds>|gate,<0-8|*>,<move>,<still>|factory|restart|mon[,<seconds>[,raw|gates]]|out|tx[,<seconds>]>", "HLK-LD2410C radar: read or change the configuration, monitor the reported data");

bool Sensor_LD2410C::atModeHandler(AtModeArgs &args)
{
    if (!args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(LD2410C))) {
        return false;
    }
    if (args.equals(0, F("mon"))) {
        // read by _feedMonitor(), the base class starts the monitor
        _monitorGates = args.equalsIgnoreCase(2, F("gates"));
        _monitorNoGates = false;
    }
    if (args.equals(0, F("out"))) {
        #if IOT_SENSOR_LD2410C_OUT_PIN != -1
            args.print(F("output GPIO%u is %s, presence %s"), _outPin, (digitalRead(_outPin) == HIGH) ? PSTR("high") : PSTR("low"), (_presence == 1) ? PSTR("detected") : (_presence == 0) ? PSTR("clear") : PSTR("unknown"));
        #else
            args.print(F("no presence output configured (IOT_SENSOR_LD2410C_OUT_PIN)"));
        #endif
        return true;
    }
    _atModeHandler(args);
    return true;
}

bool Sensor_LD2410C::_atModeParams(AtModeArgs &args, const String &cmd, Params &params)
{
    if (F("range") == cmd) {
        if (!args.requireArgs(3, 3)) {
            return false;
        }
        params.maxMoveGate = args.toIntMinMax<uint8_t>(1, 1, kMaxGate);
        params.maxStillGate = args.toIntMinMax<uint8_t>(2, 1, kMaxGate);
    }
    else if (F("timeout") == cmd) {
        if (!args.requireArgs(2, 2)) {
            return false;
        }
        params.duration = args.toIntMinMax<uint16_t>(1, 0, kMaxDuration);
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
        auto move = args.toIntMinMax<uint8_t>(2, 0, kMaxSensitivity);
        auto still = args.toIntMinMax<uint8_t>(3, 0, kMaxSensitivity);
        for(uint8_t i = 0; i < kGateCount; i++) {
            if (all || i == gate) {
                params.move[i] = move;
                params.still[i] = still;
            }
        }
    }
    else {
        args.invalidArgument(0, F("read|range|timeout|gate|factory|restart|mon|out|tx"), '|');
        return false;
    }
    return true;
}

#endif

#endif
