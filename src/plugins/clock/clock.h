/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <MillisTimer.h>
#include <vector>
#include "WebUIComponent.h"
#include "animation.h"
#include "clock_button.h"
#include "kfc_fw_config.h"
#include "plugins.h"
#include "../src/plugins/plugins.h"

#if ESP8266 && defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
#    pragma push_macro("DEBUG")
#    undef DEBUG
#    include <IRrecv.h>
#    include <IRremoteESP8266.h>
#    include <IRutils.h>
#    include <IRac.h>
#    pragma pop_macro("DEBUG")
#endif

namespace WebServer {
    class AsyncUpdateWebHandler;
}
class ClockPlugin;

namespace Clock {

    class LoopOptionsBase
    {
    public:
        LoopOptionsBase(ClockPlugin &plugin);

        uint32_t getTimeSinceLastUpdate() const;
        uint32_t getMillis() const;
        uint8_t getBrightness() const;
        bool doUpdate() const;
        bool doRedraw();
        time_t getNow() const;

    protected:
        // uint16_t _updateRate;
        bool &_forceUpdate;
        uint8_t _brightness;
        uint32_t _millis;
        uint32_t _millisSinceLastUpdate;
    };

    inline uint32_t LoopOptionsBase::getTimeSinceLastUpdate() const
    {
        return _millisSinceLastUpdate;
    }

    inline uint32_t LoopOptionsBase::getMillis() const
    {
        return _millis;
    }

    inline uint8_t LoopOptionsBase::getBrightness() const
    {
        return _brightness;
    }

    inline bool LoopOptionsBase::doUpdate() const
    {
        return (_millisSinceLastUpdate >= kUpdateRate);
    }

    inline bool LoopOptionsBase::doRedraw()
    {
        if (_forceUpdate || doUpdate()) {
            _forceUpdate = false;
            return true;
        }
        return false;
    }

    inline time_t LoopOptionsBase::getNow() const
    {
        return time(nullptr);
    }

    using LEDMatrixLoopOptions = LoopOptionsBase;

    class ClockLoopOptions : public LoopOptionsBase {
    public:
        struct tm24 : tm {
            using tm::tm;
            int tm_hour_format() const {
                return tm_format_24h ? tm_hour : ((tm_hour + 23) % 12) + 1;
            }
            void set_format_24h(bool format_24h) {
                tm_format_24h = format_24h;
            }
            bool tm_format_24h{true};
            tm24 &operator=(struct tm *_tm) {
                static_cast<struct tm &>(*this) = *_tm;
                return *this;
            }
            tm24 &operator=(const struct tm &_tm) {
                static_cast<struct tm &>(*this) = _tm;
                return *this;
            }
        } tm24;

    public:
        ClockLoopOptions(ClockPlugin &plugin);

        time_t getNow() const;
        struct tm24 &getLocalTime(time_t *nowPtr = nullptr);
        bool hasTimeChanged() const;
        bool doRedraw();

    protected:
        time_t &_time;
        time_t _now;
        struct tm24 _tm;
        bool format_24h;
    };

    inline time_t ClockLoopOptions::getNow() const
    {
        return _now;
    }

    inline bool ClockLoopOptions::hasTimeChanged() const
    {
        return _now != _time;
    }

    inline bool ClockLoopOptions::doRedraw()
    {
        if (_forceUpdate || doUpdate() || hasTimeChanged()) {
            _forceUpdate = false;
            _time = _now;
            return true;
        }
        return false;
    }

#if IOT_LED_MATRIX
    using LoopOptionsType = LEDMatrixLoopOptions;
#else
    using LoopOptionsType = ClockLoopOptions;
#endif

    struct PublishedStateType {
        int32_t enabled;
        int32_t brightness;
        int32_t color;
        int32_t animation;
        float powerLevel;

        PublishedStateType() : enabled(-1), brightness(-1), color(-1), animation(-1), powerLevel(NAN) {}
    };

}

class ClockPlugin : public PluginComponent, public MQTTComponent
#if IOT_SENSOR_HAVE_MOTION_SENSOR
    , public MotionSensorHandler
#endif
#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
    , public AmbientLightSensorHandler
#endif
{
public:

    using Plugins = KFCConfigurationClasses::PluginsType;

    // using SevenSegmentDisplay = Clock::SevenSegmentDisplay;
    using ClockConfigType = Clock::ClockConfigType;
    using Color = Clock::Color;
    using AnimationType = Clock::AnimationType;
    using InitialStateType = Clock::InitialStateType;
    using LoopOptionsType = Clock::LoopOptionsType;
    using milliseconds = std::chrono::duration<uint32_t, std::ratio<1>>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1000>>;
    using NamedArray = PluginComponents::NamedArray;
    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
        using VisualizerAnimationType = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType::VisualizerAnimationType;
    #endif
    using FireAnimationType = KFCConfigurationClasses::Plugins::ClockConfigNS::FireAnimationType;
    using RainbowConfigType = KFCConfigurationClasses::Plugins::ClockConfigNS::RainbowAnimationType;
    using RainbowMultiplierType = RainbowConfigType::MultiplierType;
    using RainbowColorType = RainbowConfigType::ColorAnimationType;

    static constexpr uint16_t kDefaultUpdateRate  = 1000;  // milliseconds
    static constexpr uint16_t kMinBlinkColonSpeed = 50;
    static constexpr uint16_t kMinFlashingSpeed   = 50;
    static constexpr uint16_t kMinRainbowSpeed    = 1;

    static constexpr uint8_t kUpdateAutoBrightnessInterval = 2;  // seconds
    static constexpr uint8_t kCheckTemperatureInterval     = 5;  // seconds
    static constexpr uint8_t kMinimumTemperatureThreshold  = 30;  // °C
    static constexpr uint8_t kUpdateMQTTInterval           = 30;  // seconds

    // brightness change per button event (2%, 10% and 1% of the maximum brightness)
    static constexpr int kBrightnessChangeClick     = ((2 * 255) / 100) + 1;
    static constexpr int kBrightnessChangeLongPress = ((10 * 255) / 100) + 1;
    static constexpr int kBrightnessChangeHold      = ((1 * 255) / 100) + 1;

    static constexpr uint8_t kMaxBrightness = Clock::kMaxBrightness;

    static constexpr int16_t kAutoBrightnessOff = -1;

    enum class DisplaySensorType : uint8_t {
        OFF  = 0,
        SHOW = 1,
        BUSY = 2,  // waiting for ADC results
        DESTROYED = 3,
    };

// ------------------------------------------------------------------------
// PluginComponent
// ------------------------------------------------------------------------
public:
    ClockPlugin();

    virtual void preSetup(SetupModeType mode) override;
    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
    virtual void createMenu() override;

    enum class TitleType {
        NONE,
        ADD_GROUP,                  // surrounds the form with a group/animation title
        SET_TITLE,                  // set FormUI title to the animation title
        SET_TITLE_AND_ADD_GROUP     // set title of the form to the animation title
    };

    void _createConfigureFormAnimation(AnimationType animation, FormUI::Form::BaseForm &form, ClockConfigType &cfg, TitleType titleType);

    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1
        void _createConfigureFormIRRemote(FormUI::Form::BaseForm &form, ClockConfigType &cfg);
    #endif

    #if AT_MODE_SUPPORTED

    public:
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

// ------------------------------------------------------------------------
// WebUI
// ------------------------------------------------------------------------
public:
    virtual void createWebUI(WebUINS::Root &webUI) override {}
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;
    // virtual bool getValue(const String &id, String &value, bool &state) override;

    void _createWebUI(WebUINS::Root &webUI);
    static void webUIHook(WebUINS::Root &webUI, SensorPlugin::SensorType type);

// ------------------------------------------------------------------------
// MQTT
// ------------------------------------------------------------------------
public:
    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const;
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len) override;

    void _publishState();

public:
    static void loop();
    static void standbyLoop();

private:
    void _loop();
    void  _loopDoUpdate(LoopOptionsType &options);
    // executes all queued tasks (display, brightness, state, config) and publishes the queued
    // animations. must only be called by the loop task, it is the single drain point of _tasks
    void _applyPendingChanges();
    void _setupTimer();
    // renders the display, loop task only
    void _display_show();
    #if IOT_LED_MATRIX_SHOW_UPDATE_PROGRESS
        // renders the progress of an OTA update, see setup()
        void _showUpdateProgressQueued(int progress);
    #endif
    // the body of reconfigure(), loop task only
    void _reconfigureQueued(bool applyConfigOnly);
    // fixes/clamps config values that program the LED driver and the pixel mapping
    void _sanitizeConfig();
    // adopts the configuration of the form/storage, see createConfigureForm()
    void _syncConfigFromStorageQueued();
    // ---------------------------------------------------------------------------------------------
    // Requests (...Deferred) and the loop task code that does the work
    // ---------------------------------------------------------------------------------------------
    //
    // The loop task is the only one that changes the display, the animations and the configuration.
    // A request queues the work in _tasks, the loop task executes it in _applyPendingChanges(), in
    // FIFO order:
    //
    //     void ClockPlugin::_saveStateDeferred()
    //     {
    //         _enqueue([this] { _saveState(); });
    //     }
    //
    // Naming rule:
    //   ...Deferred()  the call is always queued, any task may use it: WebUI/WebSocket (setValue),
    //                  MQTT (onMessage/onConnect), the forms (createConfigureForm) and the OTA
    //                  upload handler.
    //   ...Queued()    can only be reached from the queue, never call it directly.
    //   (no suffix)    executes immediately and must run in the loop task: the queue and the code
    //                  that already runs in the loop task use it - the AT console (the serial
    //                  handler is a loop function and the WebUI console feeds the same stream), the
    //                  render loop, the event timers, PinMonitor, the IR receiver,
    //                  setup()/shutdown()/readConfig().
    //
    // Handlers that fire inside LoopFunctions/__Scheduler.run() (buttonCallbackDeferred,
    // rotaryCallbackDeferred, _irRemoteCallbackDeferred) have the same pair, they must not modify
    // those lists while they are being iterated. reconfigure()/createConfigureForm() keep their
    // framework name, their bodies (_reconfigureQueued/_syncConfigFromStorageQueued) run in the
    // queue only.
    //
    // _applyPendingChanges() is the only drain point, called by _loop()/standbyLoop(), so a request is
    // executed before the next frame is rendered.
    bool _enqueue(TaskQueue::Task task);

    static constexpr size_t kTaskQueueCapacity = 16;
    // FIFO queue of the requests, a full queue discards the request (__DBG_printf). The counters
    // reported by the status page (dropped/processed/peak) are compiled in with DEBUG_TASK_QUEUE=1
    TaskQueue _tasks{kTaskQueueCapacity};

    // loop task only, called directly by loop-task code and by the queue
    void _setBrightness(uint8_t brightness, int ms = -1, uint32_t maxTime = ~0U);
    void _setAnimation(AnimationType animation, uint16_t blendTime = Clock::BlendAnimation::kDefaultTime);
    void _setState(bool state, bool autoOff);
    void _saveState();
    void _setColorAndRefresh(Color color);
    void _publishAnimation(Clock::Animation *animation, uint16_t blendTime = Clock::BlendAnimation::kDefaultTime);
    void enableLoop(bool enable);
    void enableLoopNoClear(bool enable);

    // use these instead of touching _display
    void _show();
    void _clear();
    void _resetDisplay();

    // returns AnimationType::MAX if the name is invalid
    // searched for name, name slug or AnimationType as integer
    // case insensitive
    AnimationType _getAnimationType(const __FlashStringHelper *name) const;
    // get name of the animation
    const __FlashStringHelper *_getAnimationName(AnimationType type) const;
    // get lower case names with dashes
    // "Color Fade" ->  "color-fade"
    const __FlashStringHelper *_getAnimationNameSlug(AnimationType type) const;
    // get title for animation
    const __FlashStringHelper *_getAnimationTitle(AnimationType type) const;

public:
    void setColorAndRefreshDeferred(Color color);
    // time represents fading level 0 to max, the fading time is relative to the different between the brightness levels
    void setBrightnessDeferred(uint8_t brightness, int32_t millis = -1, uint32_t maxTime = ~0U);
    // use NONE to remove all animations
    // use NEXT to remove the current animation and start the next one. if next animation is not set, animation is set to NONE
    void setAnimationDeferred(AnimationType animation, uint16_t blendTime = Clock::BlendAnimation::kDefaultTime);
    void nextAnimation()
    {
        // the animation is published directly, this runs in the loop task
        _setAnimation(AnimationType((_config.animation + 1) % int(AnimationType::LAST)), 1000);
    }

    uint16_t _blendTime{Clock::BlendAnimation::kDefaultTime};

    // read defaults and copy to local storage
    void readConfig(bool setup);
    // get stored configuration and update it with local storage
    ClockConfigType &getWriteableConfig();

    // ------------------------------------------------------------------------
    // Motion sensor
    // ------------------------------------------------------------------------
    #if IOT_SENSOR_HAVE_MOTION_SENSOR
    public:
        virtual void eventMotionDetected(bool motion) override;
        virtual bool eventMotionAutoOff(bool state);

    private:
        bool _motionAutoOff{false};

    #endif

    #if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1

        private:
            void beginIRReceiver();
            void endIRReceiver();

            static constexpr uint32_t kStandbyLoopDelay = 10;

            // applies an action to a received NEC frame, see docs/IR_Remote_44_Keys.md.
            // _irRemoteCallbackDeferred() checks if the code is assigned and queues the action for
            // the loop task (the receiver runs inside the event scheduler), _irRemoteActionQueued()
            // applies it
            void _irRemoteCallbackDeferred(uint32_t code, bool repeat);
            void _irRemoteActionQueued(uint32_t code, bool repeat);

            // Captures a button press for the "IR Remote" form: while learning is enabled no action
            // is executed, the raw code is only reported to the browser (/ir-remote.json), which
            // displays it in a modal dialog next to the field. The browser enables learning when the
            // dialog opens and disables it when it is closed.
            void _irSetLearnMode(bool enable);
            void _irLearnTimeoutCheck();
            static void _irWebHandler(AsyncWebServerRequest *request);
            static void _registerIRRemoteWebHandler();

            volatile bool _irLearnMode{false};      // no actions while a button is being captured
            volatile uint32_t _irLearnId{0};        // incremented for every frame received while learning
            volatile uint32_t _irLearnCode{0};      // code of the last frame received while learning
            volatile uint32_t _irLearnTimeout{0};   // millis() after which learning is disabled again

            // learning is disabled if the browser stopped polling (max. time a dialog can stay open)
            static constexpr uint32_t kIRLearnTimeout = 60000;

        #if ESP32

            // The ESP32 uses a software receiver, see clock_ir_receiver.cpp: FastLED's RMT driver
            // claims all RMT channels and memory blocks, so IRremoteESP8266 (RMT based on the
            // ESP32) cannot be used. The interrupt only records the time between two edges, the
            // pulses are decoded by _irDecodeNec()
            enum class NecResult : uint8_t {
                VALID,          // a complete NEC frame
                INCOMPLETE,     // the frame has not been received completely yet
                INVALID,        // not a NEC frame
            };

            static void IRAM_ATTR _irPinISR();
            void _irDecodeNec();
            void _irReset();

            // parses one NEC frame at edges[index], next is the index of the following pulse,
            // only set for NecResult::VALID
            static NecResult _irParseNecFrame(const uint16_t *edges, uint16_t count, uint16_t index, uint16_t &next, uint32_t &code, bool &repeat);

            // 1 NEC frame = lead mark + lead space + 32 * (mark + space) + stop mark
            static constexpr uint8_t kNecFrameEdges = 2 + 32 * 2 + 1;

            // holds more than 2 frames, a key held down repeats the frame every 110ms
            static constexpr uint16_t kIREdgeBufferSize = 2 * (kNecFrameEdges + 1) + 32;

            // edges closer than this are ignored (glitch filter), the shortest NEC pulse is 560us
            static constexpr uint16_t kIRGlitchFilter = 50;

            // no edge for this long means the frame has been received completely
            // (the NEC repeat frame is sent 110ms after the previous frame)
            static constexpr uint16_t kIRFrameIdleTime = 20000;

            // time between two edges in microseconds, clamped to 0xffff, anything above the 9ms
            // lead mark only means that the input has been idle
            volatile uint16_t _irEdges[kIREdgeBufferSize];
            volatile uint16_t _irEdgeCount;
            volatile uint32_t _irLastEdge;
            volatile bool _irOverflow;

        #else

            IRrecv *_irReceiver{nullptr};
            decode_results _irResults{};

        #endif

            Event::Timer _irTimer;

            // reported by getStatus(), written by the loop task that decodes the frames
            uint32_t _irLastCode{0};    // code of the last received frame
            uint16_t _irFrameCount{0};  // number of received frames
            uint16_t _irRepeatCount{0}; // number of repeat frames (the key is still held)

    #else

        private:
            void beginIRReceiver() {}
            void endIRReceiver() {}

            static constexpr uint32_t kStandbyLoopDelay = 50;

    #endif

    // ------------------------------------------------------------------------
    // Power consumption sensor
    // ------------------------------------------------------------------------
    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION || IOT_CLOCK_HAVE_POWER_LIMIT

    public:
        static void webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id);

    private:
        String _getPowerLevelStr();
        void _updatePowerLevelWebUI();
        void _powerLevelCallback(uint32_t total_mW, uint32_t requested_mW, uint32_t max_mW, uint8_t target_brightness, uint8_t recommended_brightness);
        void _webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id);
        static uint8_t calcPowerFunction(uint8_t scale, uint32_t data);
        uint8_t _calcPowerLevel(uint8_t brightness);
        float __getPowerLevel(float P_Watt) const;
        uint32_t _getPowerLevelLimit(uint32_t P_Watt) const;

        #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

            uint8_t _calcPowerFunction(uint8_t scale, uint32_t data);
            float _getPowerLevel() const;

        private:
            static constexpr uint32_t kPowerLevelUpdateRateMultiplier = 500000;

            struct PowerLevelType {
                float average_mW{0}; // mW
                uint32_t current_mW{0}; // mW
                uint32_t timer{0};
                uint32_t updateRate{kUpdateMQTTInterval * kPowerLevelUpdateRateMultiplier};

                void clear() {
                    average_mW = 0;
                    current_mW = 0;
                    timer = 0;
                }
            };

            PowerLevelType _powerLevel;
        #endif

    #endif

// ------------------------------------------------------------------------
// Enable/disable LEDs
// ------------------------------------------------------------------------
public:
    // blank the pixels during a reset, the loop task is not running any more
    static void clear() {
        _reset();
    }

    // if the system crashed, disable the LEDs before the loop task starts
    void lock()
    {
        Logger_warning("The LED subsystem has been locked cause of a crash or hard reset");
        _disable();
        _setBrightness(0);
    }

private:
    static void _reset();
    void _enable();
    void _disable();
    bool _getEnabledState() const {
        return _config.enabled && _isEnabled && _targetBrightness && _tempBrightness != -1;
    }

    // ------------------------------------------------------------------------
    // Save state
    // ------------------------------------------------------------------------

    // this method needs to be called if any changes in _config are supposed to be stored permanently
    // it delays the write operation to avoid to many writes and also checks if any changes have been made
    void _saveStateDeferred();
    Event::Timer _saveTimer;

    void _setStateDeferred(bool state, bool autoOff = false); // set a state and call _saveStateDeferred()

    // ------------------------------------------------------------------------
    // Button
    // ------------------------------------------------------------------------
    #if PIN_MONITOR

    public:
        using EventType = Clock::Button::EventType;
        using ButtonType = Clock::ButtonType;
        // called by PinMonitor, queues the action, see _buttonCallbackQueued()
        void buttonCallbackDeferred(ButtonType button, EventType eventType, uint16_t repeatCount);
        void _buttonCallbackQueued(ButtonType button, EventType eventType, uint16_t repeatCount);

        #if IOT_CLOCK_HAVE_ROTARY_ENCODER
            // called by PinMonitor, queues the action, see _rotaryCallbackQueued()
            void rotaryCallbackDeferred(bool decrease, uint32_t now);
            void _rotaryCallbackQueued(bool decrease, uint32_t now);
            void setRotaryAction(uint8_t action);

        private:
            // acceleration per step
            static constexpr int kRotaryAccelerationDivider = 1;
            // max. acceleration
            static constexpr int kRotaryMaxAcceleration = 10 * kRotaryAccelerationDivider;

            uint32_t _lastRotaryUpdate{0};
            uint8_t _rotaryAcceleration{kRotaryAccelerationDivider};
            uint8_t _rotaryAction{0};
            Event::Timer _rotaryActionTimer;
        #endif

    #endif

    // ------------------------------------------------------------------------
    // Alarm Plugin
    // ------------------------------------------------------------------------
    #if IOT_ALARM_PLUGIN_ENABLED

    public:
        using AlarmModeType = KFCConfigurationClasses::Plugins::AlarmConfigNS::ModeType;

        static void alarmCallback(AlarmModeType mode, uint16_t maxDuration);

    private:
        void _alarmCallback(AlarmModeType mode, uint16_t maxDuration);
        bool _resetAlarm();  // returns true if alarm was reset

        Event::Timer _alarmTimer;
        Event::Callback _resetAlarmFunc;
    #endif

        void _addLoop();
        void _removeLoop();

    #if !IOT_LED_MATRIX

    // ------------------------------------------------------------------------
    // Clock
    // ------------------------------------------------------------------------

    // private:
    //     void _setSevenSegmentDisplay();

    public:
        // request, see the "Requests" section. the loop task uses _setBlinkColon() directly
        void setBlinkColonDeferred(uint16_t value);

    private:
        // loop task only, called directly and by the queue
        void _setBlinkColon(uint16_t value);

    private:
        // std::array<SevenSegmentDisplay::PixelAddressType, IOT_CLOCK_PIXEL_ORDER_LEN * IOT_CLOCK_NUM_DIGITS> _pixelOrder;
        time_t _time{0};

    #endif

// ------------------------------------------------------------------------

public:
    void _broadcastWebUI();
    void _webUIUpdateColor(int color = -1);

private:
    bool isTempProtectionActive() const {
        return _tempBrightness == -1;
    }
    float getTempProtectionFactor() const {
        if (isTempProtectionActive()) {
            return 0;
        }
        return _tempBrightness;
    }

public:
    // void setAnimationCallback(Clock::AnimationCallback callback);
    // void setUpdateRate(uint16_t updateRate);
    // uint16_t getUpdateRate() const;
    void setColor(Color color);
    Color getColor() const;

private:
    friend WebServer::AsyncUpdateWebHandler;

    // set brightness
    // enable LEDs if disabled
    // store new level in config
    void _setBrightnessLevel(uint8_t brightness, bool useEnable = true);

    // update brightness settings savedBrightness, config.brightness and config.enabled
    void _updateBrightnessSettings();

    // returns display brightness using current brightness and auto brightness value
    uint8_t _getBrightness(bool temperatureProtection = true) const;

    // returns display brightness target with auto brightness/temperature protection adjustments (and if supported, the power limit of FastLED)
    uint8_t _getRealBrightnessTarget() const;

    // returns current brightness without auto brightness value
    // while fading between different levels it returns the current level
    float _getFadingBrightness() const;

    KFCConfigurationClasses::Plugins::ClockConfigNS::ColorType &_getColorVar();

    // set current color
    // store color in config as solid_color if no animation is active
    void _setColor(uint32_t color, bool updateAnimation = true);

    // get current color
    uint32_t _getColor() const;

    // blend the new animation into the current one, loop task only
    bool _setBlendAnimation(Clock::Animation *animation);

// ------------------------------------------------------------------------
// Singleton getter
// ------------------------------------------------------------------------
public:
    static ClockPlugin &getInstance();

    MQTT::Json::UnnamedObject getWLEDJson();

// ------------------------------------------------------------------------
// private variables
// ------------------------------------------------------------------------

private:
    friend Clock::LEDMatrixLoopOptions;
    friend Clock::ClockLoopOptions;
    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
        friend Clock::VisualizerAnimation;
    #endif

    // SevenSegmentDisplay _display;
    Clock::PublishedStateType _publishedValues;
    bool _schedulePublishState;
    bool _isFading;
    bool _isEnabled;
    bool _forceUpdate;
    bool _isRunning;
    bool _isLocked;

    using Animation = Clock::Animation;
    friend Animation;

    Clock::DisplayType _display;

    uint32_t _lastUpdateTime;
    uint8_t _tempOverride;
    float _tempBrightness;
    float _fps;
    String _overheatedInfo;

    ClockConfigType _config;
    Event::Timer _timer;
    uint32_t _timerCounter;

    #if IOT_LED_MATRIX_SHOW_UPDATE_PROGRESS
        // progress of an OTA update, -1 until the first update, see _showUpdateProgressQueued().
        // read by the OTA upload callback to skip values that are applied already
        volatile int _updateProgress{-1};
    #endif

    MillisTimer _fadeTimer;
    uint8_t _savedBrightness;
    uint8_t _startBrightness;
    uint8_t _fadingBrightness;
    uint8_t _targetBrightness;

    Clock::Animation *_animation;
    Clock::BlendAnimation *_blendAnimation;
    Clock::ShowMethodType _method;

    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2
        AmbientLightSensorHandler _lightSensor2;
    #endif

public:
    static Clock::ShowMethodType getShowMethod();
    static const __FlashStringHelper *getShowMethodStr();
    static const __FlashStringHelper *getShowMethodStr(Clock::ShowMethodType method);
    static void setShowMethod(Clock::ShowMethodType method);
    static void toggleShowMethod();

protected:
    void _setShowMethod(Clock::ShowMethodType method);
    void _toggleShowMethod();
};

inline void ClockPlugin::setColor(Color color)
{
    _setColor(color);
}

inline ClockPlugin::Color ClockPlugin::getColor() const
{
    return _getColor();
}

inline void ClockPlugin::standbyLoop()
{
    // execute the queued requests while the animation loop is disabled
    getInstance()._applyPendingChanges();
    ::delay(kStandbyLoopDelay); // energy saving mode
}

inline void ClockPlugin::_show()
{
    _display_show();
}

inline void ClockPlugin::_clear()
{
    _display.clear();
}

inline void ClockPlugin::_resetDisplay()
{
    _reset();
}

inline void ClockPlugin::enableLoop(bool enable)
{
    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        setAutoBrightness(enable ? (Plugins::Sensor::getConfig().ambient.auto_brightness != -1) : false);
    #endif
    _display.clear();
    _display_show();
    enableLoopNoClear(enable);
}

inline void ClockPlugin::enableLoopNoClear(bool enable)
{
    __LDBG_printf("enable loop=%u", enable);
    LoopFunctions::remove(standbyLoop);
    if (enable) {
        _addLoop();
        _fps = 0;
    }
    else {
        _removeLoop();
        _fps = NAN;
    }
}

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION || IOT_CLOCK_HAVE_POWER_LIMIT

    #if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION

        inline uint8_t ClockPlugin::calcPowerFunction(uint8_t scale, uint32_t data)
        {
            return getInstance()._calcPowerFunction(scale, data);
        }

        inline float ClockPlugin::_getPowerLevel() const
        {
            return __getPowerLevel(_powerLevel.average_mW / 1000.0);
        }

    #else

        inline uint8_t ClockPlugin::calcPowerFunction(uint8_t scale, uint32_t data)
        {
            return calculate_max_brightness_for_power_mW(scale, data);
        }

    #endif

    inline void ClockPlugin::webSocketCallback(WsClient::ClientCallbackType type, WsClient *client, AsyncWebSocket *server, WsClient::ClientCallbackId id)
    {
        if (getInstance()._isRunning) {
            getInstance()._webSocketCallback(type, client, server, id);
        }
    }

    inline float ClockPlugin::__getPowerLevel(float P) const
    {
        #define PF(f) (P - (P * P * (f / 1500.0)))
        return std::max<float>(0, IOT_CLOCK_POWER_CORRECTION_OUTPUT);
        #undef PF
    }

    inline uint32_t ClockPlugin::_getPowerLevelLimit(uint32_t P_Watt) const
    {
        if (P_Watt == 0) {
            return ~0U; // unlimited
        }
        const float diff = P_Watt - __getPowerLevel(P_Watt);
        return (P_Watt + diff) * 1000;
    }

    inline uint8_t ClockPlugin::_calcPowerLevel(uint8_t brightness)
    {
        const uint32_t timestamp = micros();
        const float diff = _powerLevel.timer ? _powerLevel.updateRate / static_cast<float>(get_time_since(_powerLevel.timer, timestamp)) : 0.0f;
        _powerLevel.average_mW = ((_powerLevel.average_mW * diff) + _powerLevel.current_mW) / (diff + 1.0);
        _powerLevel.timer = timestamp;
        return brightness;
    }

#endif

extern ClockPlugin ClockPlugin_plugin;

inline ClockPlugin &ClockPlugin::getInstance()
{
    return ClockPlugin_plugin;
}

#if IOT_ALARM_PLUGIN_ENABLED

    inline void ClockPlugin::alarmCallback(AlarmModeType mode, uint16_t maxDuration)
    {
        getInstance()._alarmCallback(mode, maxDuration);
    }

    inline bool ClockPlugin::_resetAlarm()
    {
        __LDBG_printf("alarm_func=%u alarm_state=%u", bool(_resetAlarmFunc), AlarmPlugin::getAlarmState());
        if (_resetAlarmFunc) {
            // reset prior clock settings
            _resetAlarmFunc(*_timer);
            AlarmPlugin::resetAlarm();
            _schedulePublishState = true;
            return true;
        }
        return false;
    }

#endif

#if IOT_SENSOR_HAVE_MOTION_SENSOR && IOT_SENSOR_HAVE_MOTION_AUTO_OFF

    inline void ClockPlugin::eventMotionDetected(bool motion)
    {
        #if defined(IOT_CLOCK_MOTION_SENSOR_OUTPUT_PIN) && IOT_CLOCK_MOTION_SENSOR_OUTPUT_PIN != -1
            digitalWrite(IOT_CLOCK_MOTION_SENSOR_OUTPUT_PIN, !motion);
        #endif
    }

    inline bool ClockPlugin::eventMotionAutoOff(bool state)
    {
        // state true = turn the display off
        if (state && _isEnabled) {
            _setState(false, true);
            return true;
        }
        // state false = turn on
        if (!state && !_isEnabled && _motionAutoOff) {
            _setState(true, false);
            return true;
        }
        return false;
    }

#endif

inline Clock::ShowMethodType ClockPlugin::getShowMethod()
{
    return getInstance()._method;
}

inline void ClockPlugin::_setShowMethod(Clock::ShowMethodType method)
{
    #if IOT_LED_MATRIX_FASTLED_ONLY
        // PixelDisplay::show() calls FastLED.show() for every show method, so the RMT driver has
        // to stay initialised and FastLED is the only method that can be used
        method = Clock::ShowMethodType::FASTLED;
    #endif
    _method = method;
    #if ESP32 && FASTLED_VERSION == 3004000 && !FASTLED_ESP32_I2S
        if (_method != Clock::ShowMethodType::FASTLED) {
            ESP32RMTController::deinit();
        }
    #endif
}

inline void ClockPlugin::setShowMethod(Clock::ShowMethodType method)
{
    getInstance()._setShowMethod(method);
}

inline void ClockPlugin::toggleShowMethod()
{
    getInstance()._toggleShowMethod();
}

inline void ClockPlugin::_toggleShowMethod()
{
    constexpr auto kFirst = static_cast<int>(Clock::ShowMethodType::NONE);
    constexpr auto kRange = static_cast<int>(Clock::ShowMethodType::MAX) - kFirst;
    auto method = static_cast<uint8_t>(_method);
    method -= kFirst + 1;
    if (method < 0) {
        method = 0;
    }
    else {
        method %= kRange;
    }
    _setShowMethod(static_cast<Clock::ShowMethodType>(method + kFirst));
}


#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR

    inline uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
    {
        return isAutoBrightnessEnabled() ?
            (_getFadingBrightness() * (getAutoBrightness()) * (temperatureProtection ? getTempProtectionFactor() : 1.0f)) :
            (temperatureProtection ?
                (_getFadingBrightness() * getTempProtectionFactor()) :
                _getFadingBrightness());
    }

#else

    inline uint8_t ClockPlugin::_getBrightness(bool temperatureProtection) const
    {
        return temperatureProtection ?
            (_getFadingBrightness() * getTempProtectionFactor()) :
            _getFadingBrightness();
    }

#endif

inline uint8_t ClockPlugin::_getRealBrightnessTarget() const
{
    return std::min(255.0f, roundf(
        #if FASTLED_VERSION == 3004000 && (IOT_CLOCK_HAVE_POWER_LIMIT || IOT_CLOCK_DISPLAY_POWER_CONSUMPTION)
            FastLED.getPowerLimitScale() *
        #endif
        #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
            getAutoBrightness() *
        #endif
        #if IOT_CLOCK_TEMPERATURE_PROTECTION
            getTempProtectionFactor() *
        #endif
        _targetBrightness) + 1);
}

inline float ClockPlugin::_getFadingBrightness() const
{
    float dly;
    return (_fadeTimer.isActive() && (dly = _fadeTimer.getDelay())) ?
        (_targetBrightness - (((int16_t)_targetBrightness - (int16_t)_startBrightness) / dly * _fadeTimer.getTimeLeft())) :
        _targetBrightness;
}

inline ClockPlugin::AnimationType ClockPlugin::_getAnimationType(const __FlashStringHelper *name) const
{
    return ClockConfigType::getAnimationType(name);
}

inline const __FlashStringHelper * ClockPlugin::_getAnimationName(AnimationType type) const
{
    return ClockConfigType::getAnimationName(type);
}

inline const __FlashStringHelper * ClockPlugin::_getAnimationNameSlug(AnimationType type) const
{
    return ClockConfigType::getAnimationNameSlug(type);
}

inline const __FlashStringHelper *ClockPlugin::_getAnimationTitle(AnimationType type) const
{
    return ClockConfigType::getAnimationTitle(type);
}

#if !IOT_LED_MATRIX

    inline void ClockPlugin::setBlinkColonDeferred(uint16_t value)
    {
        _enqueue([this, value] { _setBlinkColon(value); });
    }

    inline void ClockPlugin::_setBlinkColon(uint16_t value)
    {
        if (value < kMinBlinkColonSpeed) {
            value = 0;
        }
        _config.blink_colon_speed = value;
        _schedulePublishState = true;
        __LDBG_printf("blinkcolon=%u update_rate=%u", value, value);
    }

#endif

inline void ClockPlugin::setColorAndRefreshDeferred(Color color)
{
    __LDBG_printf("color=%s", color.toString().c_str());
    _enqueue([this, color] { _setColorAndRefresh(color); });
}

inline void ClockPlugin::_setColorAndRefresh(Color color)
{
    _setColor(color);
    _forceUpdate = true;
    _schedulePublishState = true;
}

inline KFCConfigurationClasses::Plugins::ClockConfigNS::ColorType &ClockPlugin::_getColorVar()
{
    switch(_config.getAnimation()) {
        case AnimationType::FLASHING:
            return _config.flashing_color;
        #if IOT_LED_MATRIX_ENABLE_VISUALIZER
            case AnimationType::VISUALIZER:
                return _config.visualizer.color;
        #endif
        default:
            break;
    }
    return _config.solid_color;
}

inline void ClockPlugin::_setColor(uint32_t color, bool updateAnimation)
{
    _getColorVar() = color;
    if (updateAnimation && _animation) {
        _animation->setColor(color);
    }
}

inline uint32_t ClockPlugin::_getColor() const
{
    return const_cast<ClockPlugin *>(this)->_getColorVar();
}

inline Clock::ClockConfigType &ClockPlugin::getWriteableConfig()
{
    return _config;
}

// publish an animation and destroy the one it replaces, loop task only
inline void ClockPlugin::_publishAnimation(Clock::Animation *animation, uint16_t blendTime)
{
    __LDBG_printf("animation=%p blend_time=%u _ani=%p _blend_ani=%p", animation, blendTime, _animation, _blendAnimation);
    if (!animation) {
        return;
    }
    // the loop task decides if the animation can be blended into the current one
    _blendTime = (_animation && _animation->hasBlendSupport()) ? blendTime : 0;

    if (_animation && _setBlendAnimation(animation)) {
        // blending started, the BlendAnimation owns the new animation as _target
        return;
    }
    // no animation set yet
    if (_animation) {
        delete _animation;
    }
    if ((_animation = animation) != nullptr) {
        _animation->begin();
    }
}

inline bool ClockPlugin::_setBlendAnimation(Clock::Animation *blendAnimation)
{
    __LDBG_printf("blend=%p", blendAnimation);
    // we do not support blending more than 2 animations
    // if switched to quickly it will stop and continue with the new one
    if (_blendAnimation) {
        delete _blendAnimation;
        _blendAnimation = nullptr;
    }
    if (!_blendTime) {
        return false;
    }
    _blendAnimation = new Clock::BlendAnimation(_animation, blendAnimation, _display, _blendTime);
    if (!_blendAnimation) {
        return false;
    }
    _blendAnimation->begin();
    return true;
}

inline void ClockPlugin::_updateBrightnessSettings()
{
    if (_targetBrightness != 0) {
        __LDBG_printf("saved=%u set=%u", _savedBrightness, _targetBrightness);
        _savedBrightness = _targetBrightness;
        _config.setBrightness(_targetBrightness);
        // __LDBG_assertf(_config.enabled, "_config.enabled not true");
        // _config.enabled = true;
    }
    else {
        // __LDBG_assertf(!_config.enabled, "_config.enabled not false");
        // _config.enabled = false;
    }
}

inline void ClockPlugin::_reset()
{
    // turn off all LEDs during restart or a crash
    // IOT_LED_MATRIX_FASTLED_ONLY: PixelDisplay::show() keeps using FastLED, so the RMT driver must
    // not be torn down here - the pixels are blanked by forceClear() below
    #if ESP32 && FASTLED_VERSION == 3004000 && !FASTLED_ESP32_I2S && !IOT_LED_MATRIX_FASTLED_ONLY
        ESP32RMTController::deinit();
    #endif
    NeoPixelEx::forceClear<IOT_LED_MATRIX_OUTPUT_PIN>(std::min<uint16_t>(IOT_CLOCK_NUM_PIXELS, 1024));
    #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
        NeoPixelEx::forceClear<IOT_LED_MATRIX_OUTPUT_PIN1>(std::min<uint16_t>(IOT_CLOCK_NUM_PIXELS, 1024));
    #endif
    #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
        NeoPixelEx::forceClear<IOT_LED_MATRIX_OUTPUT_PIN2>(std::min<uint16_t>(IOT_CLOCK_NUM_PIXELS, 1024));
    #endif
    #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
        NeoPixelEx::forceClear<IOT_LED_MATRIX_OUTPUT_PIN3>(std::min<uint16_t>(IOT_CLOCK_NUM_PIXELS, 1024));
    #endif
}

inline void ClockPlugin::loop()
{
    getInstance()._loop();
}

inline void ClockPlugin::_addLoop()
{
    LOOP_FUNCTION_ADD(loop);
}

inline void ClockPlugin::_removeLoop()
{
    LoopFunctions::remove(loop);
}

inline const __FlashStringHelper *ClockPlugin::getShowMethodStr()
{
    return getShowMethodStr(getShowMethod());
}

inline const __FlashStringHelper *ClockPlugin::getShowMethodStr(Clock::ShowMethodType method)
{
    switch(method) {
        case Clock::ShowMethodType::NONE:
            return F("None");
        case Clock::ShowMethodType::FASTLED:
            return F("FastLED");
        #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
            case Clock::ShowMethodType::NEOPIXEL_EX:
            return F("NeoPixelEx");
        #endif
        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
            case Clock::ShowMethodType::AF_NEOPIXEL:
                return F("Adafruit NeoPixel");
        #endif
        default:
            break;
    }
    return F("Unknown");
}

extern "C" uint8_t getNeopixelShowMethodInt();
extern "C" const __FlashStringHelper *getNeopixelShowMethodStr();

inline const __FlashStringHelper *getNeopixelShowMethodStr()
{
    return ClockPlugin::getShowMethodStr();
}

extern "C" void ClockPluginClearPixels();

inline Clock::LoopOptionsBase::LoopOptionsBase(ClockPlugin &plugin) :
    // _updateRate(plugin._updateRate),
    _forceUpdate(plugin._forceUpdate),
    _brightness(plugin._getBrightness()),
    _millis(millis()),
    _millisSinceLastUpdate(_millis - plugin._lastUpdateTime)
{
    if (plugin._isFading && plugin._fadeTimer.reached()) {
        __LDBG_printf("fading=done brightness=%u target_brightness=%u", plugin._getBrightness(), plugin._targetBrightness);
        plugin._setBrightnessLevel(plugin._targetBrightness);
        plugin._isFading = false;
    }
}

#if IOT_LED_MATRIX == 0

    inline Clock::ClockLoopOptions::ClockLoopOptions(ClockPlugin &plugin) :
        LoopOptionsBase(plugin),
        _time(plugin._time),
        _now(time(nullptr)),
        _tm({}),
        format_24h(plugin._config.time_format_24h)
    {
    }

    inline struct Clock::ClockLoopOptions::tm24 &Clock::ClockLoopOptions::getLocalTime(time_t *nowPtr)
    {
        if (!nowPtr) {
            nowPtr = &_now;
        }
        _tm = localtime(nowPtr);
        _tm.set_format_24h(format_24h);
        return _tm;
    }

#endif

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif
