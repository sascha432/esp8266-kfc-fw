/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_LD2410C

// HLK-LD2410C 24 GHz radar presence sensor
//
// The module reports presence through its OUT pin and over its UART. This class publishes the OUT pin as occupancy
// (WebUI and MQTT), reads and writes the configuration that the module keeps in its own flash (max. moving/still gate,
// nobody duration and the move/still sensitivity of the 9 gates) and prints the data frames it reports.
//
// A read also switches the module to engineering mode: the data frames then carry the energy of every gate, the light
// value and the state of OUT. The module forgets it when it restarts, the live view reads it again when it receives a
// frame without the gates. The live view of the gate form (/ld2410c.html) receives the frames over the WebSocket
// /ld2410c-live, the serial port stays open while a page is connected.
//
// The commands and the serial port are LD24xxSensor, the UART pins are listed in LD24xxSerial.h.
//   IOT_SENSOR_LD2410C_OUT_PIN  <- OUT of the sensor (presence output)
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2410

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "LD24xxSensor.h"

// the baud rate the module has to use, a module found on another rate is set back to it
#ifndef IOT_SENSOR_LD2410C_BAUD
#    define IOT_SENSOR_LD2410C_BAUD 256000
#endif

// presence output (OUT) of the module, -1 = not connected (no presence state)
#ifndef IOT_SENSOR_LD2410C_OUT_PIN
#    define IOT_SENSOR_LD2410C_OUT_PIN -1
#endif

// the module drives the output high while it detects someone
#ifndef IOT_SENSOR_LD2410C_OUT_INVERTED
#    define IOT_SENSOR_LD2410C_OUT_INVERTED 0
#endif

// interval the presence output is read
#ifndef IOT_SENSOR_LD2410C_POLL_INTERVAL
#    define IOT_SENSOR_LD2410C_POLL_INTERVAL 250
#endif

class AsyncWebSocketClient;
class WsClient;
class WsClientAsyncWebSocket;

namespace LD2410C {

    static constexpr uint8_t kGateCount = 9;

    // what an engineering mode frame reports besides the target: the energy of every gate (0-100), the light value
    // (0-255) and the state of OUT
    struct Gates {
        uint8_t move[kGateCount];
        uint8_t still[kGateCount];
        uint8_t light;
        uint8_t out;

        Gates() :
            move(),
            still(),
            light(0),
            out(0)
        {
        }
    };

    // the frames of the live view: the target of the last frame and the highest energy of every gate since the last
    // message, a frame that falls between two messages keeps its peak
    struct Live {
        uint8_t state;
        uint16_t moveDistance;
        uint8_t moveEnergy;
        uint16_t stillDistance;
        uint8_t stillEnergy;
        uint16_t detectDistance;
        Gates gates;
        // the module reports in engineering mode
        bool hasGates;
        // a frame has been received that has not been sent yet
        bool pending;
        // millis() of the last message that was pushed to the clients
        uint32_t lastSend;

        Live() :
            state(0),
            moveDistance(0),
            moveEnergy(0),
            stillDistance(0),
            stillEnergy(0),
            detectDistance(0),
            gates(),
            hasGates(false),
            pending(false),
            lastSend(0)
        {
        }
    };

    // the configuration the module keeps in its flash
    struct Params {
        uint8_t maxMoveGate;
        uint8_t maxStillGate;
        uint16_t duration;
        uint8_t move[kGateCount];
        uint8_t still[kGateCount];
        // Bluetooth MAC address, read only
        uint8_t mac[6];
        bool bluetooth;

        Params() :
            maxMoveGate(0),
            maxStillGate(0),
            duration(0),
            move(),
            still(),
            mac(),
            bluetooth(false)
        {
        }
    };

}

class Sensor_LD2410C : public LD24xxSensorT<LD2410C::Params> {
public:
    static constexpr uint8_t kGateCount = LD2410C::kGateCount;
    static constexpr uint8_t kMaxGate = kGateCount - 1;
    static constexpr uint8_t kMaxSensitivity = 100;
    static constexpr uint32_t kMaxDuration = 0xffff;
    // distance of one gate in cm
    static constexpr uint8_t kGateDistance = 75;

    using Params = LD2410C::Params;
    using Gates = LD2410C::Gates;
    using Live = LD2410C::Live;

public:
    Sensor_LD2410C(const String &name);
    virtual ~Sensor_LD2410C();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;

    virtual void setup() override;
    virtual void shutdown() override;

    // the card of /sensor.html: max. gates and nobody duration
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    // the form of /ld2410c.html: move/still sensitivity of the gates
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
    virtual void _loopStream() override;
    virtual void _onIdle() override;
    virtual uint32_t _getWriteMask(const Params &params) const override;
    virtual bool _isBluetoothOn() const override;
    #if AT_MODE_SUPPORTED
        virtual bool _atModeParams(AtModeArgs &args, const String &cmd, Params &params) override;
    #endif

private:
    // one client of the live view: the stream runs while the first page is connected
    class WsGates;

    // the gates of an engineering mode frame, false for a frame of the basic mode or one that is too short
    static bool _decodeGates(const uint8_t *frame, Gates &gates);
    // prints the energy of every gate, a * marks a gate above its value (the configuration has been read)
    void _printGates(Print &output, const Gates &gates) const;
    // a frame without the gates: the module has restarted and left engineering mode, a read switches it on again
    void _readEngineeringMode();
    // the maximum gates and the nobody duration for the live view
    void _sendConfig();
    void _broadcastGates();
    // creates the web socket of the live view, returns false if the web server is not running yet
    bool _createSocket();
    // starts the live view while a page is connected and the serial port is free
    void _restartStream();
    // starts and stops the stream, called from the main loop
    static void _streamStart();
    static void _streamStop();

    // reads the presence output and publishes a change
    void _readPresence();
    // pushes the presence state to the WebUI clients
    void _publishWebUI();
    const __FlashStringHelper *_getId() const;
    String _getTopic() const;
    // the state like Home Assistant shows an occupancy sensor
    const __FlashStringHelper *_getStateStr() const;

private:
    Event::Timer _presenceTimer;
    uint8_t _outPin;
    // 0xff while the state of the output is not known
    uint8_t _presence;
    // the monitor prints the energy of every gate (+LD2410C=mon,<seconds>,gates)
    bool _monitorGates;
    // the monitor has printed that the frames have no gates
    bool _monitorNoGates;
    // the frames of the live view, written by the main loop and sent to the web socket clients
    Live _live;
    // retries creating the web socket until the web server is running
    Event::Timer _socketTimer;
    // restarts the live view when the serial port is free again
    Event::Timer _streamTimer;
    // millis() of the last read that a frame without the gates started
    uint32_t _engineeringRead;
    // set by the web socket when a client has authenticated, the configuration is sent by the main loop
    volatile bool _configPending;
    // the web socket of the live view, created by setup()
    WsClientAsyncWebSocket *_wsSocket;
    // the sensor of the web socket clients, one instance is created by the sensor plugin
    static Sensor_LD2410C *_instance;
};

inline const __FlashStringHelper *Sensor_LD2410C::_getId() const
{
    return F("ld2410c");
}

inline String Sensor_LD2410C::_getTopic() const
{
    return MQTT::Client::formatTopic(_getId());
}

inline const __FlashStringHelper *Sensor_LD2410C::_getStateStr() const
{
    switch(_presence) {
        case 1:
            return F("Detected");
        case 0:
            return F("Clear");
        default:
            break;
    }
    return F("Unknown");
}

#endif
