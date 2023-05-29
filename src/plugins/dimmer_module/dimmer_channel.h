/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <EventScheduler.h>
#include "dimmer_base.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/mqtt/mqtt_client.h"

namespace Dimmer {

    class ChannelsArray;
    class ColorTemperature;

    class Channel : public MQTTComponent {
    public:
        static constexpr int16_t kMaxLevel = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
        static constexpr uint32_t kMaxLevelSum = IOT_DIMMER_MODULE_MAX_BRIGHTNESS * IOT_DIMMER_MODULE_CHANNELS;
        static constexpr int16_t kMinLevel = kMaxLevel / 100;           // 1%
        static constexpr int16_t kDefaultLevel = kMaxLevel / 10;        // 10%
        static constexpr uint16_t kWebUIMaxUpdateRate = 150;
        static constexpr uint16_t kMQTTMaxUpdateRate = 600;
        static constexpr uint8_t kMQTTUpdateRateMultiplier = kMQTTMaxUpdateRate / kWebUIMaxUpdateRate;

        static constexpr uint8_t kWebUIUpdateFlag = 0x01;
        static constexpr uint8_t kMQTTUpdateFlag = 0x02;
        static constexpr uint8_t kUpdateAllFlag = kWebUIUpdateFlag|kMQTTUpdateFlag;
        static constexpr uint8_t kStartTimerFlag = 0xff;

    public:
        enum class TopicType : uint8_t {
            COMMAND_SET,
            COMMAND_STATE,
        };

    public:
        Channel(Module *dimmer = nullptr, uint8_t channel = 0);

        void setup(Module *dimmer, uint8_t channel)
        {
            _dimmer = dimmer;
            _channel = channel;
            _topic = _createTopics(TopicType::COMMAND_SET, false);
        }

        void end()
        {
            _Timer(_publishTimer).remove();
        }

        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            Channel(const Channel &) = delete;
            Channel &operator=(const Channel &) = delete;

            ~Channel() {
                if (_delayTimer) {
                    delete _delayTimer;
                }
            }
        #endif

        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
        virtual uint8_t getAutoDiscoveryCount() const override;
        virtual void onConnect() override;
        virtual void onMessage(const char *topic, const char *payload, size_t len) override final;

        bool on(float transition = NAN);
        bool off(ConfigType *config = nullptr, float transition = NAN);
        void publishState();

        bool getOnState() const;
        int16_t getLevel() const;
        void setLevel(int32_t level, float transition = NAN);
        void setStoredBrightness(int32_t store);
        uint16_t getStoredBrightness() const;
        void stopFading();

    protected:
        void onJsonMessage(const MQTT::Json::Reader &json);
        virtual void _publishMQTT();
        virtual void _publishWebUI();

    private:
        friend ChannelsArray;
        friend ColorTemperature;
        friend Module;

        void _publish();
        bool _set(int32_t level, float transition = NAN);
        String _createTopics(TopicType type, bool full = true) const;

        Module *_dimmer;
        String _topic;
        Event::Timer _publishTimer;
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            Event::Timer *_delayTimer;
        #endif
        uint16_t _storedBrightness;
        uint16_t _brightness;
        uint8_t _channel;
    };

    inline bool Channel::getOnState() const
    {
        return _brightness != 0;
    }

    inline int16_t Channel::getLevel() const
    {
        return _brightness;
    }

    inline void Channel::setLevel(int32_t level, float transition)
    {
        _set(level, transition);
    }

    inline void Channel::setStoredBrightness(int32_t store)
    {
        // do not store 0
        if (store > 1) {
            _storedBrightness = store;
        }
    }

    inline uint16_t Channel::getStoredBrightness() const
    {
        return _storedBrightness;
    }

    inline void Channel::_publish()
    {
        __LDBG_printf("publish channel=%u", _channel);
        _publishMQTT();
        _publishWebUI();
    }

    inline void Channel::publishState()
    {
        _Timer(_publishTimer).throttle(333, [this](Event::CallbackTimerPtr) {
            _publish();
        });
    }

}
