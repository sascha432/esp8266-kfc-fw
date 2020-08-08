/**
  Author: sascha_lammers@gmx.de
*/

// https://developers.home-assistant.io/docs/en/external_api_rest.html#post-api-services-lt-domain-lt-service

#include <KFCForms.h>
#include <KFCJson.h>
#include <LoopFunctions.h>
#include <EventTimer.h>
#include "kfc_fw_config.h"
#include "home_assistant.h"
#include "../include/templates.h"
#include "web_server.h"
#include "plugins.h"

#if DEBUG_HOME_ASSISTANT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

static HassPlugin plugin;

HassPlugin &HassPlugin::getInstance()
{
    return plugin;
}

void HassPlugin::getStatus(Print &output)
{
    output.print(F("RESTful API: "));
    uint8_t count = 0;
    for(uint8_t i = 0; i < 4; i++) {
        bool hasToken = strlen(Plugins::HomeAssistant::getApiToken(i)) > 100;
        auto endPoint = Plugins::HomeAssistant::getApiEndpoint(i);
        if (endPoint && hasToken) {
            output.printf_P(PSTR(HTML_S(br) "#%u %s"), i, endPoint);
            count++;
        }
    }
    if (!count) {
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
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HASSAC), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HASSGS), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HASSCS), name);
}

bool HassPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSAC))) {
        Plugins::HomeAssistant::ActionVector actions;
        Plugins::HomeAssistant::getActions(actions);
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
                    Plugins::HomeAssistant::setActions(actions);
                }
            }
            else {
                auto _action = (Plugins::HomeAssistant::ActionEnum_t)args.toInt(1);
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
                    actions.emplace_back(id, 0, _action, values, entityId);
                }
                args.printf_P(PSTR("modified id %u (use +STORE to save)"), id);
                Plugins::HomeAssistant::setActions(actions);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSEA))) {
        if (args.requireArgs(1)) {
            auto id = (uint16_t)args.toInt(0);
            auto action = Plugins::HomeAssistant::getAction(id);
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
            callService(args.toString(0), 0, json, [args](HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback) mutable {
                if (service) {
                    args.print(F("call service executed"));
                    statusCallback(true);
                }
                else {
                    args.printf_P(PSTR("error: http code %u, message %s"), request.getCode(), request.getMessage().c_str());
                    statusCallback(false);
                }
            }, [args](bool status) mutable {
                args.printf_P(SPGM(status__u, "status=%u"), status);
            });
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSGS))) {
        if (args.requireArgs(1, 1)) {
            getState(args.toString(0), 0, [args](HassJsonReader::GetState *state, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback) mutable {
                if (state) {
                    args.printf_P(PSTR("entity_id=%s, state=%s"), state->_entitiyId.c_str(), state->_state.c_str());
                    statusCallback(true);
                }
                else {
                    args.printf_P(PSTR("error: http code %u, message %s"), request.getCode(), request.getMessage().c_str());
                    statusCallback(false);
                }
            }, [args](bool status) mutable {
                args.printf_P(SPGM(status__u), status);
            });
        }
        return true;
    }
    return false;
}

#endif

void HassPlugin::onConnect(MQTTClient *client)
{
    Plugins::HomeAssistant::ActionVector actions;
    Plugins::HomeAssistant::getActions(actions);
    for(auto &action: actions) {
        switch(action.getAction()) {
            case ActionEnum_t::MQTT_TOGGLE:
            case ActionEnum_t::MQTT_INCR:
            case ActionEnum_t::MQTT_DECR:
                {
                    auto topic = action.getEntityId();
                    auto pos = topic.indexOf(';');
                    if (pos != -1) {
                        topic.remove(pos);
                        __LDBG_printf("subscribe %s", topic.c_str());
                        client->subscribe(this, topic);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void HassPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    auto iterator = std::find_if(_topics.begin(), _topics.end(), [topic](const TopicValue_t &tv){
        return tv.topic == topic;
    });
    if (iterator == _topics.end()) {
        __LDBG_printf("add=%s value=%u", topic, atoi(payload));
        _topics.push_back({ topic, atoi(payload) });
    }
    else {
        __LDBG_printf("change=%s value=%u", topic, atoi(payload));
        iterator->value = atoi(payload);
    }
}

bool HassPlugin::_mqttSplitTopics(String &state, String &set)
{
    auto pos = state.indexOf(';');
    if (pos != -1) {
        set = state.substring(pos + 1);
        state.remove(pos);
        __LDBG_printf("state=%s set=%s", state.c_str(), set.c_str());
        return true;
    }
    return false;
}

void HassPlugin::_mqttSet(const String &topic, int value)
{
    auto client = MQTTClient::getClient();
    __LDBG_printf("client=%p topic=%s value=%u", client, topic.c_str(), value);
    if (client) {
        client->publish(topic, true, String(value));
    }
}

void HassPlugin::_mqttGet(const String &topic, std::function<void(bool, int)> callback)
{
    int counter = 0;
    Scheduler.addTimer(10, true, [this, topic, callback, counter](EventScheduler::TimerPtr timer) mutable {
        if (counter++ == 200) {
            timer->detach();
            callback(false, 0);
        }
        else {
            auto iterator = std::find_if(_topics.begin(), _topics.end(), [topic](const TopicValue_t &tv){
                return tv.topic == topic;
            });
            __LDBG_printf("topic=%s i=%u found=%u", topic.c_str(), counter, iterator != _topics.end());
            if (iterator != _topics.end()) {
                timer->detach();
                __LDBG_printf("value=%u", iterator->value);
                callback(true, iterator->value);
            }
        }
    });
}

void HassPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    __LDBG_printf("url=%s method=%s", request->url().c_str(), request->methodToString());
    if (request->url().endsWith(F("hass.html"))) {

        using KFCConfigurationClasses::MainConfig;

        //FormUI::ZeroconfSuffix()

        form.add(F("api_endpoint"), _H_STR_VALUE(MainConfig().plugins.homeassistant.api_endpoint));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.api_endpoint) - 1));

        form.add(F("token"), _H_STR_VALUE(MainConfig().plugins.homeassistant.token));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.token) - 1));

        form.add(F("api_endpoint1"), _H_STR_VALUE(MainConfig().plugins.homeassistant.api_endpoint1));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.api_endpoint1) - 1));

        form.add(F("token1"), _H_STR_VALUE(MainConfig().plugins.homeassistant.token1));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.token1) - 1));

        form.add(F("api_endpoint2"), _H_STR_VALUE(MainConfig().plugins.homeassistant.api_endpoint2));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.api_endpoint2) - 1));

        form.add(F("token2"), _H_STR_VALUE(MainConfig().plugins.homeassistant.token2));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.token2) - 1));

        form.add(F("api_endpoint3"), _H_STR_VALUE(MainConfig().plugins.homeassistant.api_endpoint3));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.api_endpoint3) - 1));

        form.add(F("token3"), _H_STR_VALUE(MainConfig().plugins.homeassistant.token3));
        form.addValidator(FormLengthValidator(1, sizeof(MainConfig().plugins.homeassistant.token3) - 1));

    }
    else if (request->url().endsWith(F("actions.html"))) {

        JsonUnnamedObject &json = *new JsonUnnamedObject();
        {
            auto &actions = json.addObject(F("actions"));
            for(uint8_t i = 1; i < ActionEnum_t::__END; i++) {
                actions.add(String(i), Plugins::HomeAssistant::getActionStr(static_cast<Plugins::HomeAssistant::ActionEnum_t>(i)));
            }
        }
        auto &items = json.addArray(F("items"));
        Plugins::HomeAssistant::ActionVector actions;
        Plugins::HomeAssistant::getActions(actions);
        for(auto &action: actions) {
            auto &json = items.addObject();
            json.add(FSPGM(id), action.getId());
            json.add(F("action"), action.getAction());
            auto &values = json.addArray(F("values"));
            for(auto value: action.getValues()) {
                values.add(value);
            }
            json.add(F("entity_id"), action.getEntityId());
        }
        reinterpret_cast<SettingsForm &>(form).setJson(&json);
    }
    else {
        auto actionId = (uint16_t)request->arg(FSPGM(id)).toInt();
        Plugins::HomeAssistant::Action action = Plugins::HomeAssistant::getAction(actionId);
        if (action.getId()) { // edit action
        }
        else { // new action
            Plugins::HomeAssistant::ActionVector actions;
            Plugins::HomeAssistant::getActions(actions);
            actionId = 1;
            for(auto &action: actions) {
                actionId = std::max(actionId, (uint16_t)(action.getId() + 1));
            }
            action.setId(actionId);
            action.setAction(static_cast<Plugins::HomeAssistant::ActionEnum_t>(request->arg(F("action")).toInt()));
        }
        if (request->method() & WebRequestMethod::HTTP_POST) { // store changes
            //action.setId(actionId);
            action.setEntityId(request->arg(F("entity_id")));
            action.setApiId(request->arg(F("api_id")).toInt());

            Plugins::HomeAssistant::Action::ValuesVector values;
            for(uint8_t i = 0; i < 8; i++) {
                String name = PrintString(F("values[%u]"), i);
                if (!request->hasArg(name.c_str())) {
                    break;
                }
                values.push_back(request->arg(name).toInt());
            }
            action.setValues(values);

            Plugins::HomeAssistant::ActionVector actions;
            Plugins::HomeAssistant::getActions(actions);
            auto iterator = std::find(actions.begin(), actions.end(), actionId);
            if (iterator != actions.end()) {
                *iterator = action;
            }
            else {
                actions.push_back(action);
            }
            __LDBG_printf("storing actions, updated id=%u", actionId);
            Plugins::HomeAssistant::setActions(actions);
        }

        form.add(FSPGM(id), String(action.getId()), FormField::Type::TEXT)
            ->setFormUInew FormUI::UI(FormUI::HIDDEN, emptyString));

        form.add(F("action"), String(action.getAction()), FormField::Type::TEXT)
            ->setFormUInew FormUI::UI(FormUI::HIDDEN, emptyString));

        form.add(F("action_str"), String(action.getActionFStr()), FormField::Type::TEXT)
            ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Action")))->setReadOnly());

        if (action.getAction() >= ActionEnum_t::MQTT_SET) {
            form.add(F("entity_id"), action.getEntityId(), FormField::Type::TEXT)
                ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("MQTT Topic")));
        }
        else {
            form.add(F("entity_id"), action.getEntityId(), FormField::Type::TEXT)
                ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Entity Id")));

            form.add(F("api_id"), String(action.getApiId()), FormField::Type::TEXT)
                ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Api Id")));
        }

        switch(action.getAction()) {
            case ActionEnum_t::SET_BRIGHTNESS:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Brightness")));
                break;
            case ActionEnum_t::CHANGE_BRIGHTNESS:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Brightness")))->setSuffix(String(PRINTHTMLENTITIES_PLUSM[0])));
                form.add(F("values[1]"), String(action.getValue(1)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Min. Brightness")));
                form.add(F("values[2]"), String(action.getValue(2)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Max. Brightness")));
                form.add(F("values[3]"), String(action.getValue(3)), FormField::Type::SELECT)
                    ->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Below Min. Brightness")))->setBoolItems(F("Turn Off"), F("Min. Brightness")));
                break;
            case ActionEnum_t::SET_KELVIN:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Kelvin")));
                break;
            case ActionEnum_t::SET_MIREDS:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Mireds")));
                break;
            case ActionEnum_t::SET_RGB_COLOR:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Red")));
                form.add(F("values[1]"), String(action.getValue(1)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Green")));
                form.add(F("values[2]"), String(action.getValue(2)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Blue")));
                break;
            case ActionEnum_t::VOLUME_SET:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Volume")))->setSuffix(String('%')));
                break;
            case ActionEnum_t::MQTT_SET:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Value")));
                break;
            case ActionEnum_t::MQTT_TOGGLE:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("On Value")));
                form.add(F("values[1]"), String(action.getValue(1)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Off Value")));
                break;
            case ActionEnum_t::MQTT_INCR:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Value")));
                form.add(F("values[1]"), String(action.getValue(1)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Max. Value")));
                break;
            case ActionEnum_t::MQTT_DECR:
                form.add(F("values[0]"), String(action.getValue(0)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Value")));
                form.add(F("values[1]"), String(action.getValue(1)), FormField::Type::TEXT)
                    ->setFormUInew FormUI::UI(FormUI::Type::TEXT, F("Min. Value")));
                break;
            default:
                break;
        }

    }
    form.finalize();
}

void HassPlugin::setup(SetupModeType mode)
{
    _installWebhooks();
    dependsOn(FSPGM(mqtt), [this](const PluginComponent *plugin) {
        MQTTClient::safeRegisterComponent(this);
    });
}

void HassPlugin::reconfigure(PGM_P source)
{
    if (source != nullptr) {
        _installWebhooks();
    }
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
    url = Plugins::HomeAssistant::getApiEndpoint(_apiId);
    __LDBG_printf("url=%s api_id=%u", url.c_str(), _apiId);
}

void HassPlugin::getBearerToken(String &token) const
{
    token = Plugins::HomeAssistant::getApiToken(_apiId);
    __LDBG_printf("token=%s api_id=%u", token.c_str(), _apiId);
}

void HassPlugin::executeAction(const Action &action, StatusCallback_t statusCallback)
{
    JsonUnnamedObject json;
    json.add(F("entity_id"), action.getEntityId());
    __LDBG_printf("id=%u action=%s values=%s entity_id=%s", action.getId(), action.getActionFStr(), action.getValuesStr().c_str(), action.getEntityId().c_str());
    switch(action.getAction()) {
        case ActionEnum_t::TURN_ON:
            callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::TURN_OFF:
            callService(_getDomain(action.getEntityId()) + F("/turn_off"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::SET_BRIGHTNESS:
            json.add(FSPGM(brightness), action.getValue(0));
            callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::CHANGE_BRIGHTNESS:
            getState(action.getEntityId(), action.getApiId(), [this, action](HassJsonReader::GetState *state, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback) {
                __LDBG_printf("state=%p http=%u", state, request.getCode());
                JsonUnnamedObject json;
                json.add(F("entity_id"), action.getEntityId());
                if (state) {
                    int brightness = state->getBrightness();
                    if (brightness == 0 && action.getValue(0) > 0) {
                        __LDBG_printf("brightness=0, calling turn on", brightness);
                        callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
                        return;
                    } else if (action.getValue(3) && brightness <= action.getValue(0)) {
                        __LDBG_printf("brightness %u <= value %u, calling turn off", brightness, action.getValue(0));
                        callService(_getDomain(action.getEntityId()) + F("/turn_off"), action.getApiId(), json, _serviceCallback, statusCallback);
                        return;
                    }
                    else {
                        brightness += action.getValue(0);
                        if (brightness < action.getValue(1)) {
                            brightness = action.getValue(1);
                        }
                        if (brightness > action.getValue(2)) {
                            brightness = action.getValue(2);
                        }
                    }
                    json.add(FSPGM(brightness), brightness);
                    __LDBG_printf("new brightness=%u", brightness);
                    callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
                }
                else {
                    _serviceCallback(nullptr, request, statusCallback);
                }
            }, statusCallback);
            break;
        case ActionEnum_t::TOGGLE:
            callService(_getDomain(action.getEntityId()) + F("/toggle"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::TRIGGER:
            callService(_getDomain(action.getEntityId()) + F("/trigger"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::SET_KELVIN:
            json.add(F("kelvin"), action.getValue(0));
            callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::SET_MIREDS:
            json.add(F("color_temp"), action.getValue(0));
            callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::SET_RGB_COLOR:
            {
                auto &colors = json.addArray(F("rgb_color"));
                colors.add(action.getValue(0));
                colors.add(action.getValue(1));
                colors.add(action.getValue(2));
                callService(_getDomain(action.getEntityId()) + F("/turn_on"), action.getApiId(), json, _serviceCallback, statusCallback);
            } break;
        case ActionEnum_t::MEDIA_NEXT_TRACK:
            callService(_getDomain(action.getEntityId()) + F("/media_next_track"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::MEDIA_PAUSE:
            callService(_getDomain(action.getEntityId()) + F("/media_pause"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::MEDIA_PLAY:
            callService(_getDomain(action.getEntityId()) + F("/media_play"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::MEDIA_PLAY_PAUSE:
            callService(_getDomain(action.getEntityId()) + F("/media_play_pause"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::MEDIA_PREVIOUS_TRACK:
            callService(_getDomain(action.getEntityId()) + F("/media_previous_track"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::MEDIA_STOP:
            callService(_getDomain(action.getEntityId()) + F("/media_stop"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::VOLUME_DOWN:
            callService(_getDomain(action.getEntityId()) + F("/volume_down"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::VOLUME_MUTE:
            json.add(F("is_volume_muted"), true);
            callService(_getDomain(action.getEntityId()) + F("/volume_mute"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::VOLUME_UNMUTE:
            json.add(F("is_volume_muted"), false);
            callService(_getDomain(action.getEntityId()) + F("/volume_mute"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::VOLUME_SET:
            json.add(F("volume_level"), action.getValue(0) / 100.0f);
            callService(_getDomain(action.getEntityId()) + F("/volume_set"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::VOLUME_UP:
            callService(_getDomain(action.getEntityId()) + F("/volume_up"), action.getApiId(), json, _serviceCallback, statusCallback);
            break;
        case ActionEnum_t::MQTT_SET:
            _mqttSet(action.getEntityId(), action.getValue(0));
            statusCallback(true);
            break;
        case ActionEnum_t::MQTT_TOGGLE:
            {
                String stateTopic = action.getEntityId();
                String setTopic;
                if (_mqttSplitTopics(stateTopic, setTopic)) {
                    auto value0 = action.getValue(0);
                    auto value1 = action.getValue(1);
                    _mqttGet(stateTopic, [this, value0, value1, setTopic, statusCallback](bool status, int value) {
                        if (status) {
                            _mqttSet(setTopic, (value == value0) ? value1 : value0);
                        }
                        statusCallback(status);
                    });
                }
            }
            break;
        case ActionEnum_t::MQTT_INCR:
            {
                String stateTopic = action.getEntityId();
                String setTopic;
                if (_mqttSplitTopics(stateTopic, setTopic)) {
                    auto incr = action.getValue(0);
                    auto max = action.getValue(1);
                    _mqttGet(stateTopic, [this, incr, max, setTopic, statusCallback](bool status, int value) {
                        if (status) {
                            value += incr;
                            if (value > max) {
                                value = max;
                            }
                            _mqttSet(setTopic, value);
                        }
                        statusCallback(status);
                    });
                }
            }
            break;
        case ActionEnum_t::MQTT_DECR:
            {
                String stateTopic = action.getEntityId();
                String setTopic;
                if (_mqttSplitTopics(stateTopic, setTopic)) {
                    auto decr = action.getValue(0);
                    auto min = action.getValue(1);
                    _mqttGet(stateTopic, [this, decr, min, setTopic, statusCallback](bool status, int value) {
                        if (status) {
                            value -= decr;
                            if (value < min) {
                                value = min;
                            }
                            _mqttSet(setTopic, value);
                        }
                        statusCallback(status);
                    });
                }
            }
            break;
        case ActionEnum_t::NONE:
        default:
            statusCallback(false);
            break;
    }
}

void HassPlugin::getState(const String &entityId, uint8_t apiId, GetStateCallback_t callback, StatusCallback_t statusCallback)
{
    __LDBG_printf("entity=%s", entityId.c_str());
    auto jsonReader = new JsonVariableReader::Reader();
    auto groups = jsonReader->getElementGroups();
    groups->emplace_back(JsonString());
    HassJsonReader::GetState::apply(groups->back());
    _apiId = apiId;
    _createRestApiCall(String(F("states/")) + entityId, String(), jsonReader, [callback, statusCallback](int16_t code, KFCRestAPI::HttpRequest &request) {
        __LDBG_printf("code=%d msg=%s", code, request.getMessage().c_str());
        if (code == 200) {
            auto &results = request.getElementsGroup()->front().getResults<HassJsonReader::GetState>();
            if (results.size()) {
                callback(results.front(), request, statusCallback);
                return;
            }
        }
        callback(nullptr, request, statusCallback);
    });
}

void HassPlugin::callService(const String &service, uint8_t apiId, const JsonUnnamedObject &payload, ServiceCallback_t callback, StatusCallback_t statusCallback)
{
    __LDBG_printf("service=%s payload=%s", service.c_str(), payload.toString().c_str());
    auto jsonReader = new JsonVariableReader::Reader();
    auto groups = jsonReader->getElementGroups();
    groups->emplace_back(JsonString());
    HassJsonReader::CallService::apply(groups->back());
    _apiId = apiId;
    _createRestApiCall(String(F("services/")) + service, payload.toString(), jsonReader, [callback, statusCallback](int16_t code, KFCRestAPI::HttpRequest &request) {
        __LDBG_printf("code=%d msg=%s", code, request.getMessage().c_str());
        if (code == 200) {
            auto &results = request.getElementsGroup()->front().getResults<HassJsonReader::CallService>();
            if (results.size()) {
                callback(results.front(), request, statusCallback);
                return;
            }
        }
        callback(nullptr, request, statusCallback);
    });
}

void HassPlugin::_serviceCallback(HassJsonReader::CallService *service, HassPlugin::KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback)
{
    __LDBG_printf("service=%p http=%d", service, request.getCode());
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

void HassPlugin::removeAction(AsyncWebServerRequest *request)
{
    __LDBG_printf("is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {

        auto msg = String(0);
        auto id = (uint16_t)request->arg(FSPGM(id)).toInt();
        Plugins::HomeAssistant::ActionVector actions;
        Plugins::HomeAssistant::getActions(actions);
        auto iterator = std::remove(actions.begin(), actions.end(), id);
        if (iterator != actions.end()) {
            actions.erase(iterator);
            Plugins::HomeAssistant::setActions(actions);
            config.write();
            msg = FSPGM(OK);
        }

        AsyncWebServerResponse *response = request->beginResponse(200, FSPGM(mime_text_plain), msg);
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache(true);
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void HassPlugin::_installWebhooks()
{
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/hass_remove.html"), removeAction);
}
