/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <vector>
#include <EventScheduler.h>
#include "SevenSegmentPixel.h"
#include "WebUIComponent.h"
#include "plugins.h"
#include "kfc_fw_config.h"
#include "./plugins/mqtt/mqtt_component.h"

#ifndef DEBUG_IO1T_CLOCK
#define DEBUG_IOT_CLOCK                 1
#endif

#if SPEED_BOOSTER_ENABLED
#warning The speed booster causes timing issues and should be deactivated
#endif

#if !NTP_CLIENT || !NTP_HAVE_CALLBACKS
#error NTP_CLIENT=1 and NTP_HAVE_CALLBACKS=1 required
#endif

// number of digits
#ifndef IOT_CLOCK_NUM_DIGITS
#define IOT_CLOCK_NUM_DIGITS            4
#endif

// pixels per segment
#ifndef IOT_CLOCK_NUM_PIXELS
#define IOT_CLOCK_NUM_PIXELS            2
#endif

// number of colons
#ifndef IOT_CLOCK_NUM_COLONS
#if IOT_CLOCK_NUM_DIGITS == 4
#define IOT_CLOCK_NUM_COLONS            1
#else
#define IOT_CLOCK_NUM_COLONS            2
#endif
#endif

// pixels per colon
#ifndef IOT_CLOCK_NUM_COLON_PIXELS
#define IOT_CLOCK_NUM_COLON_PIXELS      4
#endif




#ifndef IOT_CLOCK_NUM_PX_PER_DOT
#define IOT_CLOCK_NUM_PX_PER_DOT        2
#endif

// order of the segments (a-g)
#ifndef IOT_CLOCK_SEGMENT_ORDER
#define IOT_CLOCK_SEGMENT_ORDER         { 0, 1, 3, 4, 5, 6, 2 }
#endif

// digit order, 30=colon #1,31=#2, etc...
#ifndef IOT_CLOCK_DIGIT_ORDER
#define IOT_CLOCK_DIGIT_ORDER           { 0, 1, 30, 2, 3, 31, 4, 5 }
#endif

// pixel order of pixels that belong to digits, 0 to use pixel addresses of the 7 segment class
#ifndef IOT_CLOCK_PIXEL_ORDER
#define IOT_CLOCK_PIXEL_ORDER           { 0 }
#endif

#ifndef IOT_CLOCK_BUTTON_PIN
#define IOT_CLOCK_BUTTON_PIN            14
#endif

#if IOT_CLOCK_BUTTON_PIN
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>
#endif

class ClockPlugin : public PluginComponent, public MQTTComponent, public WebUIInterface {

// WebUIInterface
public:
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

private:
    uint8_t _ui_colon;
    uint8_t _ui_animation;
    uint8_t _ui_color;

// PluginComponent
public:
    typedef std::function<void()> UpdateCallback_t;

    typedef enum : int8_t {
        NONE = -1,
        BLINK_COLON = 0,
        FAST_BLINK_COLON,
        SOLID_COLON,
        RAINBOW,
        FLASHING,
        FADE,
    } AnimationEnum_t;

    typedef enum : uint8_t {
        SOLID = 0,
        NORMAL,
        FAST,                   // twice per second
    } BlinkColonEnum_t;

    class Color {
    public:
        Color() {
        }
        Color(uint8_t *values) : _red(values[0]), _green(values[1]), _blue(values[2]) {
        }
        Color(uint8_t red, uint8_t green, uint8_t blue) : _red(red), _green(green), _blue(blue) {
        }
        Color (uint32_t value, bool bgr = false) {
            if (bgr) {
                _blue = value >> 16;
                _green = value >> 8;
                _red = value;
            } else {
                *this = value;
            }
        }
        uint32_t rnd() {
            do {
                *this = Color((rand() % 4) * (255 / 4), (rand() % 4) * (255 / 4), (rand() % 4) * (255 / 4));
            } while(_red == 0 && _green == 0 && _blue == 0);
            return get();
        }
        inline uint32_t get() {
            return ((uint32_t)_red << 16) | ((uint32_t)_green <<  8) | _blue;
        }
        inline Color &operator =(uint32_t value) {
            _red = value >> 16;
            _green = value >> 8;
            _blue = value;
            return *this;
        }
        inline operator int() {
            return get();
        }
        inline static uint32_t get(uint8_t red, uint8_t green, uint8_t blue) {
            return ((uint32_t)red << 16) | ((uint32_t)green <<  8) | blue;
        }
        inline uint8_t red() const {
            return _red;
        }
        inline uint8_t green() const {
            return _green;
        }
        inline uint8_t blue() const {
            return _blue;
        }
    private:
        uint8_t _red;
        uint8_t _green;
        uint8_t _blue;
    };

    ClockPlugin();

    virtual PGM_P getName() const {
        return PSTR("clock");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Clock");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return MIN_PRIORITY;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    //virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual bool hasWebUI() const override {
        return true;
    }
    virtual WebUIInterface *getWebUIInterface() override {
        return this;
    }
    virtual void createWebUI(WebUI &webUI) override;

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }

    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector);
    virtual uint8_t getAutoDiscoveryCount() const {
        return 1;
    }
    virtual void onConnect(MQTTClient *client);
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len);

    void publishState(MQTTClient *client);

private:
    void _setColor();

private:
    uint8_t _qos;
    uint8_t _colors[3];
    uint16_t _brightness;

public:
#if IOT_CLOCK_BUTTON_PIN
    static void onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
    static void onButtonReleased(Button& btn, uint16_t duration);
    void _onButtonReleased(uint16_t duration);
#endif
    static void loop();
    static void wifiCallback(uint8_t event, void *payload);
    static void ntpCallback(time_t now);

public:
    void enable(bool enable);
    void setSyncing(bool sync);
    void setBlinkColon(BlinkColonEnum_t value);
    void setAnimation(AnimationEnum_t animation);
    Clock updateConfig();

private:
    void _loop();
    void _setSevenSegmentDisplay(Clock &cfg);
    void _off() {
        _display.clear();
    }
    void setBrightness(uint16_t brightness);

private:
    typedef struct {
        union {
            struct {
                uint32_t fromColor;
                uint32_t toColor;
                uint16_t progress;
                uint16_t speed;
            } fade;
            struct {
                uint32_t color;
            } flashing;
            struct {
                uint16_t movementSpeed;
            } rainbow;
        };
        UpdateCallback_t callback;
    } AnimationData_t;

#if IOT_CLOCK_BUTTON_PIN
    PushButton _button;
    uint8_t _buttonCounter;
#endif
    using SevenSegmentDisplay = SevenSegmentPixel<uint8_t, IOT_CLOCK_NUM_DIGITS, IOT_CLOCK_NUM_PIXELS, IOT_CLOCK_NUM_COLONS, IOT_CLOCK_NUM_COLON_PIXELS>;

    SevenSegmentDisplay _display;
    std::array<SevenSegmentDisplay::pixel_address_t, SevenSegmentDisplay::getTotalPixels()> _pixelOrder;

    Color _color;
    BlinkColonEnum_t _blinkColon;
    uint32_t _updateTimer;
    time_t _time;
    uint16_t _updateRate;
    uint8_t _isSyncing;
    bool _timeFormat24h;
    AnimationData_t _animationData;
};
