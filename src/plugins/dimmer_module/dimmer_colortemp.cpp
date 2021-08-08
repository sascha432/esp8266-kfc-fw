/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_HAS_COLOR_TEMP

#include "dimmer_colortemp.h"
#include "dimmer_channel.h"

namespace Dimmer {

    ColorTemperature::ColorTemperature(Base *base) : MQTTComponent(ComponentType::LIGHT), _base(*base)
    {
    }

    MQTT::AutoDiscovery::EntityPtr ColorTemperature::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
    {
        auto discovery = new MQTT::AutoDiscovery::Entity();
        switch(num) {
            case 0:
                if (discovery->createJsonSchema(this, FSPGM(main), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::MAIN_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::MAIN_SET));
                    discovery->addBrightnessScale(_base.getChannelCount() * Channel::getMaxLevel());
                    discovery->addParameter(F("brightness"), true);
                    discovery->addParameter(F("white"), true);
                }
                // discovery->addPayloadOnOff();
                // discovery->addBrightnessStateTopic(_data.brightness.state);
                // discovery->addBrightnessCommandTopic(_data.brightness.set);
                // discovery->addBrightnessScale(MAX_LEVEL_ALL_CHANNELS);
                // discovery->addColorTempStateTopic(_data.color.state);
                // discovery->addColorTempCommandTopic(_data.color.set);
                break;
            case 1:
                if (discovery->create(this, FSPGM(lock_channels, "lock_channels"), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::LOCK_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::LOCK_SET));
                }
                break;
        }
        return discovery;
    }

    uint8_t ColorTemperature::getAutoDiscoveryCount() const
    {
        return 2;
    }

    String ColorTemperature::_createTopics(TopicType type)
    {
        switch(type) {
            case TopicType::MAIN_SET:
                return MQTT::Client::formatTopic(String(FSPGM(main)), FSPGM(_set));
            case TopicType::MAIN_STATE:
                return MQTT::Client::formatTopic(String(FSPGM(main)), FSPGM(_state));
            case TopicType::LOCK_SET:
                return MQTT::Client::formatTopic(String(FSPGM(lock_channels)), F("/lock/set"));
            case TopicType::LOCK_STATE:
                return MQTT::Client::formatTopic(String(FSPGM(lock_channels)), F("/lock/state"));
            default:
                break;
        }
        return emptyString;
    }

}

#endif
