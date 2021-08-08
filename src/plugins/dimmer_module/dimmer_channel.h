/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <EventScheduler.h>
#include "dimmer_base.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/mqtt/mqtt_client.h"

namespace Dimmer {

    class Channel : public MQTTComponent {
    public:
        static constexpr int16_t MAX_LEVEL = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
        // static constexpr int16_t MIN_LEVEL = MAX_LEVEL / 100;
        // static constexpr int16_t DEFAULT_LEVEL = MAX_LEVEL / 2;
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
        bool off(ConfigType *config = nullptr, float transition = NAN, int32_t level = -1);
        void publishState();

        static constexpr int16_t getMaxLevel() {
            return MAX_LEVEL;
        }

        bool getOnState() const;
        int16_t getLevel() const;
        void setLevel(int32_t level, float transition = NAN, bool publish = true);
        void setStoredBrightness(int32_t store);
        uint16_t getStorededBrightness() const;

    protected:
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            int _offDelayPrecheck(int16_t level, ConfigType *config = nullptr, int16_t storeLevel = -1);
        #endif

        void onJsonMessage(const MQTT::Json::Reader &json);
        virtual void _publishMQTT();
        virtual void _publishWebUI();

    private:
        void _publish();
        bool _set(int32_t level, float transition = NAN, bool publish = true);
        String _createTopics(TopicType type, bool full = true) const;

        Module *_dimmer;
        String _topic;
        Event::Timer _publishTimer;
        uint32_t _publishLastTime;
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            Event::Timer *_delayTimer;
        #endif
        uint16_t _storedBrightness;
        uint16_t _brightness;
        uint8_t _channel;
        uint8_t _publishFlag;
        uint8_t _mqttCounter;
    };

    // inline int16_t Channel::getMaxLevel() const {
    //     return _maxLevel;
    // }

    inline bool Channel::getOnState() const
    {
        return _brightness != 0;
    }

    inline int16_t Channel::getLevel() const
    {
        return _brightness;
    }

    inline void Channel::setStoredBrightness(int32_t store)
    {
        if (store > 1) {
            _storedBrightness = store;
        }
    }

    inline uint16_t Channel::getStorededBrightness() const
    {
        return _storedBrightness;
    }

    using ChannelPtr = Channel *;

    template<size_t _Channels>
    class GroupArray : public std::array<ChannelPtr, _Channels>
    {
    public:
        void setAll(int16_t value) {
            for (auto channel: *this) {
                channel->setLevel(value);
            }
        }

        int32_t getSum() const {
            int32_t sum = 0;
            for (const auto channel: *this) {
                sum += channel->getLevel();
            }
            return sum;
        }

        int32_t getSum(int16_t minLevel) const {
            int32_t sum = 0;
            for (const auto channel: *this) {
                auto level = channel->getLevel();
                if (level > minLevel) {
                    sum += level;
                }

            }
            return sum;
        }
    };

}
