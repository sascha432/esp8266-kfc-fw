/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_HAS_COLOR_TEMP

#include <EventScheduler.h>
#include "dimmer_base.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/mqtt/mqtt_client.h"

namespace Dimmer {

    class ColorTemperature : public MQTTComponent {
    public:
        static constexpr uint16_t kColorMin = 15300;
        static constexpr uint16_t kColorMax = 50000;
        static constexpr uint16_t kColorRange = (kColorMax - kColorMin);

        enum class TopicType : uint8_t {
            MAIN_SET,
            MAIN_STATE,
            LOCK_SET,
            LOCK_STATE
        };

    public:
        ColorTemperature(Base *base);

    // MQTT
    public:
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
        virtual uint8_t getAutoDiscoveryCount() const override;
        virtual void onConnect() override;
        virtual void onMessage(const char *topic, const char *payload, size_t len) final;

    // WebUI
    public:
        void _setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);
        // void _getValues(JsonArray &array);

    private:
        String _createTopics(TopicType type);

    // // channels are displayed in this order in the web ui
    // // warm white
    // int8_t channel_ww1;
    // int8_t channel_ww2;
    // // cold white
    // int8_t channel_cw1;
    // int8_t channel_cw2;


        // static constexpr uint8_t MAX_CHANNELS = 4;
        // static constexpr int16_t MAX_LEVEL = IOT_ATOMIC_SUN_MAX_BRIGHTNESS;
        // static constexpr int16_t MIN_LEVEL = (MAX_LEVEL / 100);    // below is treat as off
        // static constexpr int16_t DEFAULT_LEVEL = MAX_LEVEL / 3;     // set this level if a channel is turned on without a brightness value
        // static constexpr int32_t MAX_LEVEL_ALL_CHANNELS = MAX_LEVEL * MAX_CHANNELS;
    private:
        Base &_base;
    };

}

#endif
