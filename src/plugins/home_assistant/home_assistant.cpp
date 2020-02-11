/**
  Author: sascha_lammers@gmx.de
*/

#if HOME_ASSISTANT_INTEGRATION

// https://developers.home-assistant.io/docs/en/external_api_rest.html#post-api-services-lt-domain-lt-service

/*

+hassgs=light.kfc4f22d0_0
+hasscs=light/turn_on,light.kfc4f22d0_0
+hasscs=light/turn_off,light.kfc4f22d0_0
+hasscs=light/turn_on,light.kfc4f22d0_0,brightness=85
+hasscs=light/turn_on,light.atomic_sun,brightness=60


+hassreq=services/light/turn_on,{"entity_id":"light.kfc4f22d0_0", "attributes":{"brightness": 20}}
+hassreq=services/light/turn_on,"{\"entity_id\":\"light.kfc4f22d0_0\"}"
+hassreq=services/light/turn_off,"{\"entity_id\":\"light.kfc4f22d0_0\"}"
+hassreq=services/light/turn_on,{"entity_id":"light.floor_lamp_top",",\"brightness\": 120}"
+hassreq=services/light/turn_on,{"entity_id":"light.floor_lamp_top",",\"brightness\":20}"
+hassreq=services/light/turn_off,{"entity_id":"light.floor_lamp_top"}
*/

#include <KFCForms.h>
#include <KFCJson.h>
#include "kfc_fw_config.h"
#include "home_assistant.h"
#include "../include/templates.h"
#include "plugins.h"

#if DEBUG_HOME_ASSISTANT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static HassPlugin plugin;

void HassPlugin::getStatus(Print &output)
{
    bool hasToken = strlen(Config_HomeAssistant::getApiToken()) > 100;
    auto endPoint = Config_HomeAssistant::getApiEndpoint();
    output.printf_P(PSTR("RESTful API: "));
    if (endPoint && hasToken) {
        output.print(endPoint);
    }
    else {
        output.print(F("Not configured"));
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX         "HASS"


PROGMEM_AT_MODE_HELP_COMMAND_DEF(HASSAC, "AC", "<id>,[<action>,<entity_id>[,<value>,...]]", "Modify home assistant action", "List home assistant actions");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HASSEA, "EA", "<id>", "Execute action");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HASSGS, "GS", "<domain.entity_id>", "Get state");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HASSCS, "CS", "<service>,<domain.entity_id>,[attribute:value,attribute:\"string\",...]", "Call service");

void HassPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HASSAC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HASSGS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HASSCS), name);
}

bool HassPlugin::atModeHandler(AtModeArgs &args)
{
    /*
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSREQ))) {
        auto uri = args.toString(0);
        auto body = args.toString(1);
        for(int i = 2; i < args.size(); i++) {
            body += args.toString(i);
        }

        args.printf_P(PSTR("calling API %s"), uri.c_str());
        args.printf_P(PSTR("body=%s"), body.c_str());

        auto jsonReader = new JsonVariableReader::Reader();

        _createRestApiCall(uri.c_str(), body, jsonReader, [args](int16_t code, KFCRestAPI::HttpRequest &request) mutable {
            if (code == 200) {
                args.print(F("Hass request retrieved"));
            }
            else {
                args.printf_P(PSTR("HTTP error code %d, message %s"), code, request.getMessage().c_str());
            }
        });
        return true;
    }
    else
*/
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSAC))) {
        Config_HomeAssistant::ActionVector actions;
        Config_HomeAssistant::getActions(actions);
        if (args.isQueryMode()) {
            if (actions.size()) {
                for(auto &action: actions) {
                    args.printf_P(PSTR("id=%u action=%s values=%s entity_id=%s"), action.getId(), action.getActionFStr(), action.getValuesStr().c_str(), action.getEntityId().c_str());
                }
            } else {
                args.print(F("no actions defined"));
            }
        }
        else if (args.size() >= 3 || args.requireArgs(1, 1)) {
            bool noMatch = true;
            auto id = args.toInt(0);
            if (args.size() == 1) {
                for(auto &action: actions) {
                    if (action.getId() == id) {
                        action.setEntityId(String());
                        noMatch = false;
                        break;
                    }
                }
                if (noMatch) {
                    args.printf_P(PSTR("could not find id %u"), id);
                }
                else {
                    args.printf_P(PSTR("removed %u (use +STORE to save)"), id);
                    Config_HomeAssistant::setActions(actions);
                }
            }
            else {
                auto _action = (Config_HomeAssistant::ActionEnum_t)args.toInt(1);
                String entityId = args.toString(2);
                Action::ValuesVector values;
                for(uint8_t i = 3; i < args.size(); i++) {
                    values.push_back(args.toInt(i));
                }
                for(auto &action: actions) {
                    if (action.getId() == id) {
                        action.setAction(_action);
                        action.setValues(values);
                        action.setEntityId(entityId);
                        noMatch = false;
                        break;
                    }
                }
                if (noMatch) {
                    actions.emplace_back(id, _action, values, entityId);
                }
                args.printf_P(PSTR("modified id %u (use +STORE to save)"), id);
                Config_HomeAssistant::setActions(actions);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSEA))) {
        if (args.requireArgs(1)) {
            auto id = (uint16_t)args.toInt(0);
            auto action = Config_HomeAssistant::getAction(id);
            if (action.getId()) {
                args.printf_P(PSTR("id=%u, action=%s, values=%s, entity=%s"), id, action.getActionFStr(), action.getValuesStr().c_str(), action.getEntityId().c_str());
                executeAction(action, [args, id](bool status) mutable {
                    args.printf_P(PSTR("id=%u, status=%u"), id, status);
                });
            }
            else {
                args.printf_P(PSTR("id=%u not found"), id);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSCS))) {
        if (args.requireArgs(2)) {
            JsonUnnamedObject json;
            json.add(F("entity_id"), args.toString(1));
            for (uint8_t i = 2; i < args.size(); i++) {
                auto attribute = args.toString(i);
                auto pos = attribute.indexOf('=');
                if (pos != -1) {
                    JsonString name = attribute.substring(0, pos);
                    if (attribute.charAt(pos + 1) == '"') {
                        String str = attribute.substring(pos + 2);
                        if (String_endsWith(str, '"')) {
                            str.remove(str.length() - 1, 1);
                        }
                        json.add(name, str);
                    }
                    else if (attribute.indexOf('.') != -1) {
                        json.add(name, atof(attribute.c_str() + pos + 1));
                    }
                    else {
                        json.add(name, atoi(attribute.c_str() + pos + 1));
                    }
                }
            }
            callService(args.toString(0), json, [args](HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback) mutable {
                if (service) {
                    args.print(F("call service executed"));
                    statusCallback(true);
                }
                else {
                    args.printf_P(PSTR("error: http code %u, message %s"), request.getCode(), request.getMessage().c_str());
                    statusCallback(false);
                }
            }, [args](bool status) mutable {
                args.printf_P(PSTR("status=%u"), status);
            });
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSGS))) {
        if (args.requireArgs(1, 1)) {
            getState(args.toString(0), [args](HassJsonReader::GetState *state, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback) mutable {
                if (state) {
                    args.printf_P(PSTR("entity_id=%s, state=%s"), state->_entitiyId.c_str(), state->_state.c_str());
                    statusCallback(true);
                }
                else {
                    args.printf_P(PSTR("error: http code %u, message %s"), request.getCode(), request.getMessage().c_str());
                    statusCallback(false);
                }
            }, [args](bool status) mutable {
                args.printf_P(PSTR("status=%u"), status);
            });
        }
        return true;
    }
    return false;
}

#endif

void HassPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    form.finalize();

    JsonUnnamedArray array;
    Config_HomeAssistant::ActionVector actions;
    Config_HomeAssistant::getActions(actions);
    for(auto &action: actions) {
        auto &json = array.addObject();
        json.add(F("id"), action.getId());
        json.add(F("action"), action.getAction());
        auto &values = json.addArray(F("values"));
        for(auto value: action.getValues()) {
            values.add(value);
        }
        json.add(F("entity_id"), action.getEntityId());
    }
    PrintString str;
    array.printTo(str);
    reinterpret_cast<SettingsForm &>(form).getTokens().push_back(std::make_pair<String, String>(F("HASS_ITEMS"), str.c_str()));
}

/*
void HassPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Home Assistant"), false);

    // row = &webUI.addRow();
}

void HassPlugin::getValues(JsonArray &array)
{
}

void HassPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
}
*/

void HassPlugin::getRestUrl(String &url) const
{
    url = Config_HomeAssistant::getApiEndpoint();
}

void HassPlugin::getBearerToken(String &token) const
{
    token = Config_HomeAssistant::getApiToken();
}

void HassPlugin::executeAction(const Action &action, StatusCallback_t statusCallback)
{
    JsonUnnamedObject json;
    json.add(F("entity_id"), action.getEntityId());
    _debug_printf_P(PSTR("id=%u action=%s values=%s entity_id=%s\n"), action.getId(), action.getActionFStr(), action.getValuesStr().c_str(), action.getEntityId().c_str());
    switch(action.getAction()) {
        case ActionEnum_t::TURN_ON:
            callService(_getDomain(action.getEntityId()) + F("/turn_on"), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::TURN_OFF:
            callService(_getDomain(action.getEntityId()) + F("/turn_off"), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::SET_BRIGHTNESS:
            json.add(F("brightness"), action.getValue(0));
            callService(_getDomain(action.getEntityId()) + F("/turn_on"), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::CHANGE_BRIGHTNESS:
            getState(action.getEntityId(), [this, action](HassJsonReader::GetState *state, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback) {
                _debug_printf_P(PSTR("state=%p http=%u\n"), state, request.getCode());
                JsonUnnamedObject json;
                json.add(F("entity_id"), action.getEntityId());
                if (state) {
                    int brightness = state->getBrightness();
                    if (action.getValue(3)) {
                        if (brightness == 0 && action.getValue(0) > 0) {
                            _debug_printf_P(PSTR("brightness=0, calling turn on\n"), brightness);
                            callService(_getDomain(action.getEntityId()) + F("/turn_on"), json, _serviceCallback, statusCallback);
                            return;
                        } else if (action.getValue(0) <= brightness) {
                            _debug_printf_P(PSTR("brightness<=value, calling turn off\n"), brightness);
                            callService(_getDomain(action.getEntityId()) + F("/turn_off"), json, _serviceCallback, statusCallback);
                            return;
                        }
                    }
                    brightness += action.getValue(0);
                    if (brightness > action.getValue(1)) {
                        brightness = action.getValue(1);
                    }
                    if (brightness < action.getValue(2)) {
                        brightness = action.getValue(2);
                    }
                    json.add(F("brightness"), brightness);
                    _debug_printf_P(PSTR("new brightness=%u\n"), brightness);
                    callService(_getDomain(action.getEntityId()) + F("/turn_on"), json, _serviceCallback, statusCallback);
                }
                else {
                    _serviceCallback(nullptr, request, statusCallback);
                }
            }, statusCallback);
            break;
        case ActionEnum_t::NONE:
        default:
            statusCallback(false);
            break;
    }
}

void HassPlugin::getState(const String &entityId, GetStateCallback_t callback, StatusCallback_t statusCallback)
{
    auto jsonReader = new JsonVariableReader::Reader();
    auto groups = jsonReader->getElementGroups();
    groups->emplace_back(JsonString());
    HassJsonReader::GetState::apply(groups->back());
    _createRestApiCall(String(F("states/")) + entityId, String(), jsonReader, [callback, statusCallback](int16_t code, KFCRestAPI::HttpRequest &request) {
        if (code == 200) {
            for(auto &element: *request.getElementsGroup()) {
                auto result = element.getResult<HassJsonReader::GetState>();
                if (result) {
                    callback(result, request, statusCallback);
                    return;
                }
            }
        }
        callback(nullptr, request, statusCallback);
    });
}

void HassPlugin::callService(const String &service, const JsonUnnamedObject &payload, ServiceCallback_t callback, StatusCallback_t statusCallback)
{
    auto jsonReader = new JsonVariableReader::Reader();
    auto groups = jsonReader->getElementGroups();
    groups->emplace_back(JsonString());
    HassJsonReader::CallService::apply(groups->back());
    PrintString payloadStr;
    payload.printTo(payloadStr);
    _createRestApiCall(String(F("services/")) + service, payloadStr, jsonReader, [callback, statusCallback](int16_t code, KFCRestAPI::HttpRequest &request) {
        if (code == 200) {
            for(auto &element: *request.getElementsGroup()) {
                auto result = element.getResult<HassJsonReader::CallService>();
                if (result) {
                    callback(result, request, statusCallback);
                    return;
                }
            }
        }
        callback(nullptr, request, statusCallback);
    });
}

void HassPlugin::_serviceCallback(HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback)
{
    _debug_printf_P(PSTR("service=%p http=%u\n"), service, request.getCode());
    statusCallback(service != nullptr);
}

String HassPlugin::_getDomain(const String &entityId)
{
    auto pos = entityId.indexOf('.');
    if (pos != -1) {
        return entityId.substring(0, pos);
    }
    return String();
}

#endif
