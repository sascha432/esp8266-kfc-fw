/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_LD2450

// HLK-LD2450 24 GHz radar tracking sensor
//
// The sensor reports up to three targets over its UART: the position of a target in mm, its speed and the distance
// resolution. It has no presence output, the reported targets are the presence. This class reads the configuration
// that the sensor keeps in its own flash (firmware version, MAC address, target tracking mode and the three zone
// areas), writes it back and prints the reported targets.
//
// The serial port and the command frames are LD24xxSerial, the pins are the ones of the LD2420: RX of the sensor on
// GPIO2 (UART1 TX), TX of the sensor on GPIO13 (UART0 RX while the port is open). The port is only open while a command
// or the monitor runs and all commands run from the main loop without blocking.
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2450

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <memory>
#include "MQTTSensor.h"
#include "LD24xxSerial.h"

// the default baud rate of the module
#ifndef IOT_SENSOR_LD2450_BAUD
#    define IOT_SENSOR_LD2450_BAUD 256000
#endif

class AsyncWebSocketClient;
class WsClient;
class WsClientAsyncWebSocket;

class Sensor_LD2450 : public MQTT::Sensor {
public:
    static constexpr uint8_t kTargetCount = 3;
    static constexpr uint8_t kZoneCount = 3;
    // bytes per target: x, y, speed and resolution
    static constexpr uint8_t kTargetSize = 8;

    enum class ZoneType : uint8_t {
        DISABLED = 0,
        DETECTION = 1,
        FILTER = 2,
    };

    // reported position of a target, all zero while there is no target
    struct Target {
        int16_t x;
        int16_t y;
        // mm/s
        int32_t speed;
        uint16_t resolution;
    };

    // area of a zone, the corners in mm
    struct Zone {
        int16_t x1;
        int16_t y1;
        int16_t x2;
        int16_t y2;
    };

    struct Params {
        uint8_t version[6];
        uint8_t mac[6];
        bool bluetooth;
        bool multiTarget;
        ZoneType zoneType;
        Zone zone[kZoneCount];

        Params() :
            version(),
            mac(),
            bluetooth(false),
            multiTarget(false),
            zoneType(ZoneType::DISABLED),
            zone()
        {
        }
    };

public:
    Sensor_LD2450(const String &name);
    virtual ~Sensor_LD2450();

    virtual void publishState() override {}
    virtual void getValues(WebUINS::Events &array, bool timer) override {}
    virtual void createWebUI(WebUINS::Root &webUI) override {}
    virtual void getStatus(Print &output) override;

    virtual void setup() override;
    virtual void shutdown() override;

    // the card of /sensor.html: target tracking mode and zone type
    virtual bool hasForm() const override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    // stores the values of either form, called in the web server context. reconfigure() writes them to the sensor
    virtual void configurationSaved(FormUI::Form::BaseForm *form) override;
    virtual void reconfigure(PGM_P source) override;

    // the form of /ld2450.html: the three zone areas
    void createAreaForm(FormUI::Form::BaseForm &form);

    // The live view of the radar (/ld2450-radar.html): the targets are pushed over the WebSocket /ld2450-live, the
    // sensor reports 10 frames per second and one HTTP request per frame would stall the web server. The serial port
    // is open while at least one page is connected (the serial console is paused meanwhile, see LD24xxSerial).
    struct Live {
        uint8_t count;
        Target target[kTargetCount];
        // millis() of the last frame that the sensor sent
        uint32_t stamp;
        // millis() of the last message that was pushed to the clients
        uint32_t lastSend;
        // a frame has been received that has not been sent yet
        bool pending;

        Live() :
            count(0),
            target(),
            stamp(0),
            lastSend(0),
            pending(false)
        {
        }
    };

    #if AT_MODE_SUPPORTED
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

private:
    enum class OpType : uint8_t {
        NONE,
        READ,
        WRITE,
        RESTART,
        FACTORY,
        MONITOR,
        // the serial port is open and the targets are kept in _live for the live view
        STREAM,
    };

    enum class StateType : uint8_t {
        SEND,
        WAIT_REPLY,
    };

    // one client of the live view: the stream runs while the first page is connected
    class WsTargets;

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
        // target data of the frame the monitor printed last
        uint8_t targets[kTargetCount * kTargetSize];
        // the values that a read collects
        Params pending;
        // frame of the monitor
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
            targets(),
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
    // reads the target frames and pushes them to the clients of the live view
    void _loopStream();
    // feeds one byte of the target frames, returns true when a complete frame is in _op->buffer
    bool _feedTargetFrame(uint8_t data);
    // decodes the target data of a frame into _live
    void _updateLive(const uint8_t *data);
    // the configuration of the sensor for the live view: zone type and areas
    void _sendConfig();
    void _broadcastTargets();
    // creates the web socket of the live view, returns false if the web server is not running yet
    bool _createSocket();
    // starts and stops the stream, called from the main loop
    static void _streamStart();
    static void _streamStop();
    // restarts the stream after a command if a page of the live view is still connected
    void _restartStream();
    bool _sendStep();
    bool _handleReply();
    void _sendFrame(uint16_t command, const uint8_t *data, uint8_t length);
    void _printParams(Print &output) const;
    void _printMonitorFrame(Print &output);
    void _printMonitorRaw(Print &output);
    void _queueWrite(const Params &params, Stream *output);
    Params *_getFormParams();
    void _applyPendingWrite();

    static void _formatVersion(const uint8_t *version, char *buffer, size_t size);
    static PGM_P _getZoneTypeStr(ZoneType type);
    static PGM_P _getOpTypeStr(OpType type);
    static int16_t _getCoordinate(const uint8_t *ptr);
    static int32_t _getSpeed(const uint8_t *ptr);
    // decodes three targets of a frame, returns the number of targets that are not empty
    static uint8_t _decodeTargets(const uint8_t *data, Target *targets);

    String _getVersion() const;

private:
    String _name;
    std::unique_ptr<LD24xxSerial> _serial;
    std::unique_ptr<Operation> _op;
    std::unique_ptr<Params> _params;
    // the values of the forms, allocated by the first form and kept: a form that is still rendered points to it
    std::unique_ptr<Params> _formParams;
    // the values of a saved form, written from the main loop when the sensor is idle
    std::unique_ptr<Params> _pendingWrite;
    // the targets of the live view, written by the main loop and sent to the web socket clients
    Live _live;
    Event::Timer _timer;
    // retries creating the web socket until the web server is running
    Event::Timer _socketTimer;
    // set by the web socket when a client has authenticated, the configuration is sent by the main loop
    volatile bool _configPending;
    // the web socket of the live view, created by setup()
    WsClientAsyncWebSocket *_wsSocket;
    uint32_t _baud;
    // the sensor of the web socket clients, one instance is created by the sensor plugin
    static Sensor_LD2450 *_instance;
};

inline bool Sensor_LD2450::hasForm() const
{
    return true;
}

#endif
