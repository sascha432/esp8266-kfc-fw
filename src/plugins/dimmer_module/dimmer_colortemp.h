/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_HAS_COLOR_TEMP

#include <EventScheduler.h>
#include "dimmer_def.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/mqtt/mqtt_client.h"

namespace Dimmer {

    class Base;
    class Plugin;

    class ColorTemperature : public MQTTComponent {
    public:
        // color in Mired * 100
        static constexpr uint16_t kColorMin = 15300;
        static constexpr uint16_t kColorMax = 50000;
        static constexpr uint16_t kColorRange = (kColorMax - kColorMin);
        static constexpr float kColorRangeFloat = kColorRange;

        enum class TopicType : uint8_t {
            MAIN_SET,
            MAIN_STATE,
            LOCK_SET,
            LOCK_STATE,
        };

    public:
        ColorTemperature(Base *base);

    // MQTT
    public:
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
        virtual uint8_t getAutoDiscoveryCount() const override;
        virtual void onConnect() override;
        virtual void onMessage(const char *topic, const char *payload, size_t len) final;

    protected:
        void onJsonMessage(const MQTT::Json::Reader &json);

    // WebUI
    public:
        // void _setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);
        // void _getValues(JsonArray &array);
        void getValues(WebUINS::Events &array);
        void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    private:
        String _createTopics(TopicType type);

    public:
        void _channelsToBrightness();
        void _brightnessToChannels();
        void _publish();
        void _publishMQTT();
        void _publishWebUI();

    private:
        void _setLockChannels(bool value);
        void _calcRatios();
        Base &_getBase();

        void begin();
        void end();

    private:
        friend Base;
        friend Plugin;

        Base *_base;
        Event::Timer _mqttTimer;
        Event::Timer _webuiTimer;
        float _color;
        float _colorPublished;
        float _ratio[2];
        int32_t _brightness;
        int32_t _brightnessPublished;
        bool _channelLock;
        bool _channelLockPublished;
        #if IOT_ATOMIC_SUN_V2
            // warm white
            int8_t _channel_ww1;
            int8_t _channel_ww2;
            // cold white
            int8_t _channel_cw1;
            int8_t _channel_cw2;
        #else
            // currently there is no generic methods to calculate brightness and color values
            // supported are 2 WW (3000K or 2700K) and 2 CW (6000K or 5000K) channels
            // the mireds shift is ~174, a little bit on the cold white side since those appear to be brighter for the human eye
            #error not implemented
        #endif
    };

    inline Base &ColorTemperature::_getBase()
    {
        return *_base;
    }

}

#endif
