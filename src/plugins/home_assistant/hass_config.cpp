/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::HomeAssistant::defaults()
    {
        setApiEndpoint(F("${zeroconf:_home-assistant._tcp,base_url}/api/"), 0);
    }

    void Plugins::HomeAssistant::setApiEndpoint(const String &endpoint, uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint, endpoint);
                break;
            case 1:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint1, endpoint);
                break;
            case 2:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint2, endpoint);
                break;
            case 3:
                config._H_SET_STR(MainConfig().plugins.homeassistant.api_endpoint3, endpoint);
                break;
            default:
                __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        }
    }

    void Plugins::HomeAssistant::setApiToken(const String &token, uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token, token);
                break;
            case 1:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token1, token);
                break;
            case 2:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token2, token);
                break;
            case 3:
                config._H_SET_STR(MainConfig().plugins.homeassistant.token3, token);
                break;
            default:
                __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        }
    }

    const char *Plugins::HomeAssistant::getApiEndpoint(uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint);
            case 1:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint1);
            case 2:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint2);
            case 3:
                return config._H_STR(MainConfig().plugins.homeassistant.api_endpoint3);

        }
        __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        return nullptr;
    }

    const char *Plugins::HomeAssistant::getApiToken(uint8_t apiId)
    {
        switch(apiId) {
            case 0:
                return config._H_STR(MainConfig().plugins.homeassistant.token);
            case 1:
                return config._H_STR(MainConfig().plugins.homeassistant.token1);
            case 2:
                return config._H_STR(MainConfig().plugins.homeassistant.token2);
            case 3:
                return config._H_STR(MainConfig().plugins.homeassistant.token3);

        }
        __debugbreak_and_panic_printf_P(PSTR("invalid api_id=%u\n"), apiId);
        return nullptr;
    }

    void Plugins::HomeAssistant::getActions(ActionVector &actions)
    {
        uint16_t length;
        auto data = config.getBinary(_H(MainConfig().plugins.homeassistant.actions), length);
        if (data) {
            auto endPtr = data + length;
            while(data + sizeof(ActionHeader_t) <= endPtr) {
                auto header = reinterpret_cast<const ActionHeader_t *>(data);
                data += sizeof(ActionHeader_t);
                auto valuesLen = header->valuesLen * sizeof(int32_t);
                if (data + header->entityLen + valuesLen > endPtr) {
                    break;
                }
                String str = PrintString(data, header->entityLen);
                data += header->entityLen;
                Action::ValuesVector values(header->valuesLen);
                memcpy(values.data(), data, valuesLen);
                data += valuesLen;
                actions.emplace_back(header->id, header->apiId, header->action, values, str);
            }
        }
    }

    void Plugins::HomeAssistant::setActions(ActionVector &actions)
    {
        Buffer buffer;
        for(auto &action: actions) {
            ActionHeader_t header;
            header.entityLen = action.getEntityId().length();
            if (action.getId() && header.entityLen) {
                header.id = action.getId();
                header.action = action.getAction();
                header.valuesLen = action.getNumValues();
                header.apiId = action.getApiId();
                buffer.writeObject(header);
                buffer.write(action.getEntityId());
                buffer.writeVector(action.getValues());
            }
        }
        config.setBinary(_H(MainConfig().plugins.homeassistant.actions), buffer.get(), buffer.length());
    }

    Plugins::HomeAssistant::Action Plugins::HomeAssistant::getAction(uint16_t id)
    {
        ActionVector actions;
        getActions(actions);
        for(auto &action: actions) {
            if (id == action.getId()) {
                return action;
            }
        }
        return Action();
    }

    const __FlashStringHelper *Plugins::HomeAssistant::getActionStr(ActionEnum_t action)
    {
        switch(action) {
            case TURN_ON:
                return F("Turn On");
            case TURN_OFF:
                return F("Turn Off");
            case SET_BRIGHTNESS:
                return F("Set Brightness");
            case CHANGE_BRIGHTNESS:
                return F("Change Brightness");
            case TOGGLE:
                return F("Toggle");
            case TRIGGER:
                return F("Trigger");
            case SET_KELVIN:
                return F("Set Kelvin");
            case SET_MIREDS:
                return F("Set Mireds");
            case SET_RGB_COLOR:
                return F("Set RGB Color");
            case MEDIA_NEXT_TRACK:
                return F("Next Track");
            case MEDIA_PAUSE:
                return F("Media Pause");
            case MEDIA_PLAY:
                return F("Media Play");
            case MEDIA_PLAY_PAUSE:
                return F("Media Play/Pause");
            case MEDIA_PREVIOUS_TRACK:
                return F("Previous Track");
            case MEDIA_STOP:
                return F("Media Stop");
            case VOLUME_DOWN:
                return F("Volume Down");
            case VOLUME_MUTE:
                return F("Volume Mute");
            case VOLUME_UNMUTE:
                return F("Volume Unmute");
            case VOLUME_SET:
                return F("Volume Set");
            case VOLUME_UP:
                return F("Volume Up");
            case MQTT_SET:
                return F("MQTT Set");
            case MQTT_TOGGLE:
                return F("MQTT Toggle");
            case MQTT_INCR:
                return F("MQTT Increase");
            case MQTT_DECR:
                return F("MQTT Decrease");
            default:
                return F("None");
        }
    }

}
