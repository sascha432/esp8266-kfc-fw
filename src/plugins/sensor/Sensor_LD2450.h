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
// areas), writes it back, prints the reported targets and pushes them to the live view.
//
// IOT_SENSOR_LD2450_PRESENCE=1 publishes the presence and the number of targets (WebUI and MQTT). The serial port
// stays open for it all the time, the USB serial console is paused (LD24xxSerial), the WebSocket console is not.
//
// The commands and the serial port are LD24xxSensor, the UART pins are listed in LD24xxSerial.h.
// https://github.com/esphome/esphome/tree/dev/esphome/components/ld2450

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "LD24xxSensor.h"

// the baud rate the module has to use, a module found on another rate is set back to it
#ifndef IOT_SENSOR_LD2450_BAUD
#    define IOT_SENSOR_LD2450_BAUD 256000
#endif

// 1 = presence and target count (WebUI and MQTT), the serial port stays open and the USB serial console is paused
#ifndef IOT_SENSOR_LD2450_PRESENCE
#    define IOT_SENSOR_LD2450_PRESENCE 0
#endif

class AsyncWebSocketClient;
class WsClient;
class WsClientAsyncWebSocket;

namespace LD2450 {

    static constexpr uint8_t kTargetCount = 3;
    static constexpr uint8_t kZoneCount = 3;
    // bytes per target: x, y, speed and resolution
    static constexpr uint8_t kTargetSize = 8;
    // the area of the module in mm: x -3000..3000, y 0..6000. A target outside is a false detection, it does not count
    // for the presence
    static constexpr int16_t kAreaX = 3000;
    static constexpr int16_t kAreaY = 6000;

    enum class ZoneType : uint8_t {
        DISABLED = 0,
        DETECTION = 1,
        FILTER = 2,
    };

    // area of a zone, the corners in mm
    struct Zone {
        int16_t x1;
        int16_t y1;
        int16_t x2;
        int16_t y2;
    };

    // reported position of a target, all zero while there is no target
    struct Target {
        int16_t x;
        int16_t y;
        // mm/s
        int32_t speed;
        uint16_t resolution;
    };

    struct Params {
        uint8_t mac[6];
        bool bluetooth;
        bool multiTarget;
        ZoneType zoneType;
        Zone zone[kZoneCount];

        Params() :
            mac(),
            bluetooth(false),
            multiTarget(false),
            zoneType(ZoneType::DISABLED),
            zone()
        {
        }
    };

}

class Sensor_LD2450 : public LD24xxSensorT<LD2450::Params> {
public:
    static constexpr uint8_t kTargetCount = LD2450::kTargetCount;
    static constexpr uint8_t kZoneCount = LD2450::kZoneCount;
    static constexpr uint8_t kTargetSize = LD2450::kTargetSize;
    static constexpr int16_t kAreaX = LD2450::kAreaX;
    static constexpr int16_t kAreaY = LD2450::kAreaY;

    using ZoneType = LD2450::ZoneType;
    using Zone = LD2450::Zone;
    using Target = LD2450::Target;
    using Params = LD2450::Params;

    // The live view of the radar (/ld2450-radar.html): the targets are pushed over the WebSocket /ld2450-live, the
    // sensor reports 10 frames per second and one HTTP request per frame would stall the web server. The serial port
    // is open while at least one page is connected, or all the time for the presence (the serial console is paused
    // meanwhile, see LD24xxSerial).
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

public:
    #if IOT_SENSOR_LD2450_PRESENCE
        using Plugins = KFCConfigurationClasses::PluginsType;
    #endif

    Sensor_LD2450(const String &name);
    virtual ~Sensor_LD2450();

    #if IOT_SENSOR_LD2450_PRESENCE
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
        virtual uint8_t getAutoDiscoveryCount() const override;

        virtual void publishState() override;
        virtual void getValues(WebUINS::Events &array, bool timer) override;
        virtual void createWebUI(WebUINS::Root &webUI) override;

        // reads the presence timeout, writes a saved form to the module
        virtual void reconfigure(PGM_P source) override;
    #else
        virtual void publishState() override {}
        virtual void getValues(WebUINS::Events &array, bool timer) override {}
        virtual void createWebUI(WebUINS::Root &webUI) override {}
    #endif

    virtual void setup() override;
    virtual void shutdown() override;

    // the card of /sensor.html: target tracking mode and zone type
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    // the form of /ld2450.html: the three zone areas
    void createAreaForm(FormUI::Form::BaseForm &form);

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
    virtual bool _isBluetoothOn() const override;
    virtual bool _keepsStream() const override;
    virtual uint32_t _getWriteMask(const Params &params) const override;
    #if AT_MODE_SUPPORTED
        virtual bool _atModeParams(AtModeArgs &args, const String &cmd, Params &params) override;
    #endif

private:
    // one client of the live view: the stream runs while the first page is connected
    class WsTargets;

    // the configuration of the sensor for the live view: zone type and areas
    void _sendConfig();
    void _broadcastTargets();
    // creates the web socket of the live view, returns false if the web server is not running yet
    bool _createSocket();
    // true while the stream has to run: always for the presence, otherwise while a page of the live view is connected
    bool _wantsStream() const;
    // starts the stream when it is wanted and the serial port is free
    void _restartStream();
    // starts and stops the stream, called from the main loop
    static void _streamStart();
    static void _streamStop();

    static PGM_P _getZoneTypeStr(ZoneType type);
    static int16_t _getCoordinate(const uint8_t *ptr);
    static int32_t _getSpeed(const uint8_t *ptr);
    // decodes three targets of a frame, returns the number of targets that are not empty
    static uint8_t _decodeTargets(const uint8_t *data, Target *targets);
    // true for a target inside the area of the module (kAreaX, kAreaY), false for an empty slot or a false detection
    static bool _isInArea(const Target &target);
    // the number of targets inside the area
    static uint8_t _countInArea(const Target *targets);

    #if IOT_SENSOR_LD2450_PRESENCE
        // the number of targets of a frame that the stream or the monitor received
        void _updatePresence(uint8_t count);
        // switches the presence off after the timeout, publishes the changes
        void _checkPresence();
        // publishes the presence and the number of targets (MQTT and WebUI)
        void _publishPresence();
        // pushes the presence state to the WebUI clients
        void _publishWebUI();
        // the presence timeout in milliseconds from the configuration
        static uint32_t _getPresenceTimeout();
        const __FlashStringHelper *_getId() const;
        String _getTopic() const;
        String _getTargetsTopic() const;
        // the HTML of the WebUI tile: "Detected, 2 targets", "Detected", "Clear" or "Unknown"
        String _getStateStr() const;
    #endif

private:
    // the targets of the live view, written by the main loop and sent to the web socket clients
    Live _live;
    // retries creating the web socket until the web server is running
    Event::Timer _socketTimer;
    // restarts the live view when the serial port is free again
    Event::Timer _streamTimer;
    // set by the web socket when a client has authenticated, the configuration is sent by the main loop
    volatile bool _configPending;
    // the web socket of the live view, created by setup()
    WsClientAsyncWebSocket *_wsSocket;
    #if IOT_SENSOR_LD2450_PRESENCE
        // checks the presence timeout
        Event::Timer _presenceTimer;
        // millis() of the last frame with a target
        uint32_t _lastTarget;
        // millis() of the last target count that was published
        uint32_t _lastTargetsPublish;
        uint32_t _presenceTimeout;
        // 0xff until the first frame has been received
        uint8_t _presence;
        // the number of targets of the last frame
        uint8_t _targets;
        // the number of targets that was published last
        uint8_t _publishedTargets;
    #endif
    // the sensor of the web socket clients, one instance is created by the sensor plugin
    static Sensor_LD2450 *_instance;
};

inline bool Sensor_LD2450::_keepsStream() const
{
    return IOT_SENSOR_LD2450_PRESENCE;
}

#if IOT_SENSOR_LD2450_PRESENCE

    inline const __FlashStringHelper *Sensor_LD2450::_getId() const
    {
        return F("ld2450");
    }

    inline String Sensor_LD2450::_getTopic() const
    {
        return MQTT::Client::formatTopic(_getId());
    }

    inline String Sensor_LD2450::_getTargetsTopic() const
    {
        return MQTT::Client::formatTopic(F("ld2450/targets"));
    }

#endif

#endif
