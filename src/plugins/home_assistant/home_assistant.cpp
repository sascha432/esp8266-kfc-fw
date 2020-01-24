/**
  Author: sascha_lammers@gmx.de
*/

#if HOME_ASSISTANT_INTEGRATION

#include <KFCForms.h>
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

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(HASSRS, "HASSRS", "Home assistant read states");

void HassPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HASSRS), name);
}

bool HassPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HASSRS))) {
        args.print(F("requesting states"));
        _createRestApiCall(F("states"), [args](int16_t code, KFCRestAPI::HttpRequest &request) mutable {
            if (code == 200) {
                args.print(F("Hass states retrieved"));
            }
            else {
                args.printf_P(PSTR("HTTP error code %d, message %s"), code, request.getMessage().c_str());
            }
        });
        return true;
    }
    return false;
}

#endif

void HassPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {

}

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

void HassPlugin::getRestUrl(String &url) const
{
    url = Config_HomeAssistant::getApiEndpoint();
}

void HassPlugin::getBearerToken(String &token) const
{
    token = Config_HomeAssistant::getApiToken();
}


// void home_assistant_rest_call(const String &endPoint, const String &body, JsonReaderCallback jsonCallback, HomeAssistantErrorCallback errorCallback, int timeout) {

//     static BufferStream stream;//TODO static just for testing
//     static JsonCallbackReader json = JsonCallbackReader(stream, jsonCallback);

//     _debug_printf_P(PSTR("homeassistant rest call endpoint %s, body %s\n"), endPoint.c_str(), body.c_str());

//     json.initParser();

//     asyncHTTPrequest *_home_assistant_rest_api = new asyncHTTPrequest();
//     _home_assistant_rest_api->onData([](void *, asyncHTTPrequest *request, size_t length) {
//         char buf[length];
//         size_t read = request->responseRead((uint8_t *)buf, length);
//         _debug_printf_P(PSTR("feeding %d\n"), read);
//         stream.write((uint8_t *) buf, read);
//         json.parseStream();
//     });
//     _home_assistant_rest_api->onReadyStateChange([jsonCallback, errorCallback](void *, asyncHTTPrequest *request, int readyState) {
//         if (readyState == 4) {
//             // String response = request->responseText();
//             _debug_printf_P(PSTR("HTTP remote request: HTTP code %d, length %d\n"), request->responseHTTPcode(), request->responseLength());
//             if (request->responseHTTPcode() == 200) {
//                 // stream = StreamString(response);
//                 // json = JsonCallbackReader(stream, jsonCallback);
//                 // json.parse();
//             } else {
//                 _debug_printf_P(PSTR("ERROR: asyncHTTPrequest closed with http code %d\n"), request->responseHTTPcode());
//                 errorCallback(request->responseHTTPcode());
//             }
//             Scheduler.addTimer(300 * 1000, false, [request](EventScheduler::TimerPtr timer) {//TODO
//                 delete request;
//             });
//         }
//     });
//     _home_assistant_rest_api->setTimeout(timeout);
//     String token = F("Bearer ");
//     token += config._H_STR(Config().homeassistant.token);
//     String url = config._H_STR(Config().homeassistant.api_endpoint) + endPoint;

//     // _home_assistant_rest_api->setDebug(true);

//     const char *method = body.length() ? "POST" : "GET";
//     if (!_home_assistant_rest_api->open(method, url.c_str())) {
//         _debug_printf_P(PSTR("asyncHTTPrequest open failed, method %s, url %s\n"), method, url.c_str());
//         errorCallback(-1);
//         delete _home_assistant_rest_api;
//     } else {
//         _home_assistant_rest_api->setReqHeader("Authorization", token.c_str());
//         _home_assistant_rest_api->setReqHeader("Content-Type", "application/json");

//         if (!_home_assistant_rest_api->send(body.c_str())) {
//             _debug_printf_P(PSTR("asyncHTTPrequest body %s\n"), body.c_str());
//             errorCallback(-2);
//             delete _home_assistant_rest_api;
//         }
//     }

// }

// void home_assistant_read_states() {

//     home_assistant_rest_call(F("states"), _sharedEmptyString, [](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) -> bool {
//         _debug_printf_P(PSTR("JSON %s=%s\n"), json.getPath().c_str(), value.c_str());
//         return true;
//     }, [](int httpCode) {
//         _debug_printf_P(PSTR("error callback %d\n"), httpCode);
//     });

// }

// void home_assistant_change_light(const String &entityId, bool state, int brightness) {

//     home_assistant_rest_call(F("states"), _sharedEmptyString, [](const String &key, const String &value, size_t partialLength, JsonBaseReader &json) -> bool {
//         _debug_printf_P(PSTR("JSON %s=%s\n"), json.getPath().c_str(), value.c_str());
//         return true;
//     }, [](int httpCode) {
//         _debug_printf_P(PSTR("error callback %d\n"), httpCode);
//     });

// }

// void home_assistant_create_settings_form(AsyncWebServerRequest *request, Form &form) {

//     form.add<sizeof Config().homeassistant.api_endpoint>(F("api_endpoint"), config._H_W_STR(Config().homeassistant.api_endpoint));

//     form.add<sizeof Config().homeassistant.token>(F("token"), config._H_W_STR(Config().homeassistant.token));

//     form.finalize();


/**
curl -X GET -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJiMmNlNjkwZDBhMTY0ZDI2YWY4MWUxYzJiNjgzMjM3NCIsImlhdCI6MTU0ODY0MzczMywiZXhwIjoxODY0MDAzNzMzfQ.h1287xhv5nY5Fvu2GMSzIMnP51IsyFtKg9RFCS8qMBQ"     -H "Content-Type: application/json"     https://smart-home.d0g3.space:8123/api/error/all -v

curl -X GET -H "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJiMmNlNjkwZDBhMTY0ZDI2YWY4MWUxYzJiNjgzMjM3NCIsImlhdCI6MTU0ODY0MzczMywiZXhwIjoxODY0MDAzNzMzfQ.h1287xhv5nY5Fvu2GMSzIMnP51IsyFtKg9RFCS8qMBQ"     -H "Content-Type: application/json"     https://smart-home.d0g3.space:8123/api/states -v

[{"attributes": {"entity_id": ["switch.front_door", "light.entrance_light"], "friendly_name": "Unlock Door"},
"context": {"id": "459c342a25044f249e4f46ababddcc0a", "user_id": null}, "entity_id": "scene.unlock_door",
"last_changed": "2019-01-27T14:50:24.360535+00:00", "last_updated": "2019-01-27T14:50:24.360535+00:00", "state": "scening"},
{"attributes": {"Battery Level": 32, "Battery State": "Unplugged", "Device Name": "sascha's iPhone", "Device Type": "iPhone 6",
"Device Version": "12.1", "friendly_name": "sascha's iPhone Battery Level", "icon": "mdi:battery-30", "unit_of_measurement": "%"}, "context": {"i
d": "c0651818f2224f7c81f6a1e1a9cf822c", "user_id": null}, "entity_id": "sensor.saschas_iphone_battery_level", "last_changed": "2019-01-27T14:50:28.686722+00:00", "last_updated": "2019-01-27T14:50:28.686722+00:00", "state": "32"},
{"attributes": {"entity_id": ["sensor.bed", "light.floor_lamp_3", "light.floor_lamp_low_2", "light.floor_lamp_2", "sensor.saschas_ipad_battery_state", "sensor.octoprint_target_bed_temp", "sensor.yr_symbol_2", "light.window_light_3", "sensor.yr_temperature_2", "light.floor_lamp_top", "sensor.octoprint_job_percentage", "binary_sensor.gang", "switch.fluxer", "group.living_room_and_kitchen_and_desk", "sensor.octoprint_target_tool0_temp", "binary_sensor.wink_relay_living_rooms_bottom_button", "group.all_lights", "light.floor_lamp_upper_2", "binary_sensor.wink_relay_living_room_presence", "camera.door_cam", "binary_sensor.octoprint_printing", "switch.spotlightdim", "sensor.altermist_iphone_battery_state", "light.desk_light_2", "light.floor_lamp_upper", "sensor.ipad_battery_level", "light.floor_lamp_middle_2", "sensor.wink_relay_living_room_proximity", "light.desk_light", "group.all_devices", "sensor.octoprint_actual_bed_temp", "binary_sensor.d01acidservices", "binary_sensor.wink_relay_living_rooms_top_button", "group.scenes", "light.window_light_2", "updater.updater", "group.tv_lights", "binary_sensor.octoprint_printing_error", "persistent_notification.invalid_config", "zone.homebeacons", "group.x_default_view", "group.all_switches", "group.untracked", "sensor.octoprint_time_remaining", "switch.tv_spot", "sensor.yr_temperature", "group.battery_level", "zone.home", "sensor.hotend", "switch.couch_light", "zone.work", "sensor.saschas_iphone_battery_state", "light.floor_lamp_low", "sensor.octoprint_current_state", "light.floor_lamp_middle", "sensor.octoprint_time_elapsed", "sensor.octoprint_actual_tool0_temp", "sensor.ipad_battery_state", "switch.acidpi"], "friendly_name": "untracked", "hidden": true, "order": 13, "view": true},
 "context": {"id": "47358039f8f84a6e810a6690fd5a5a52", "user_id": null}, "entity_id": "group.untracked", "last_changed": "2019-01-27T14:50:27.990873+00:00", "last_updated": "2019-01-27T14:50:27.990873+00:00", "state": "on"}, {"attributes": {"friendly_name": "Floor lamp top", "supported_features": 41}, "context": {"id": "b2d1eaac84064ca5993afa643d656331", "user_id": null}, "entity_id": "light.floor_lamp_top", "last_changed": "2019-01-27T14:50:53.328630+00:00", "last_updated": "2019-01-27T14:50:53.328630+00:00", "state": "unavailable"}, {"attributes": {"entity_id": ["sensor.saschas_iphone_battery_level", "sensor.acids_ipad_battery_level", "sensor.saschas_ipad_battery_level", "sensor.altermist_iphone_battery_level"], "friendly_name": "Battery Level", "hidden": true, "order": 3, "view": true}, "context": {"id": "3b485fab62cc4dabbe3717ec0476d485", "user_id": null}, "entity_id": "group.battery_level", "last_changed": "2019-01-27T14:50:24.418518+00:00", "last_updated": "2019-01-27T14:50:24.418518+00:00", "state": "unknown"}, {"attributes": {"auto": true, "entity_id": ["light.atomic_sun", "light.bedroom_light_1", "light.bedroom_light_2", "light.desk_light", "light.entrance_light", "light.floor_lamp_2", "light.floor_lamp", "light.floor_lamp_low", "light.floor_lamp_middle", "light.floor_lamp_top", "light.floor_lamp_upper", "light.hallway_light", "light.rgbw_light_1", "light.rgbww_strip_1", "light.window_light", "light.window_light_2"], "friendly_name": "all lights", "hidden": true, "order": 15}, "context": {"id": "fa16fa30e26c47c99e7ced15285f61b6", "user_id": null}, "entity_id": "group.all_lights", "last_changed": "2019-01-27T14:50:28.438145+00:00", "last_updated": "2019-01-27T14:50:53.416015+00:00", "state": "on"}, {"attributes": {"brightness": 255, "color_temp": 153, "friendly_name": "RGBW Light 1", "hs_color": [344.941, 100.0], "max_mireds": 500, "min_mireds": 153, "rgb_color": [255, 0, 64], "supported_features": 147, "white_value": 0.0, "xy_color": [0.666, 0.284]}, "context": {"id": "b1b33f30eec34592a72d58fd9b741e2d", "user_id": null}, "entity_id": "light.rgbw_light_1", "last_changed": "2019-01-27T14:50:28.398219+00:00", "last_updated": "2019-01-27T14:50:28.540792+00:00", "state": "on"}, {"attributes": {"friendly_name": "Acidpi"}, "context": {"id": "e898cd25325d4a6c98f354bcd2ce68bb", "user_id": null}, "entity_id": "switch.acidpi", "last_changed": "2019-01-28T02:41:47.836287+00:00", "last_updated": "2019-01-28T02:41:47.836287+00:00", "state": "off"}, {"attributes": {"brightness": 254, "color_temp": 366, "friendly_name": "Floor lamp upper", "max_mireds": 500, "min_mireds": 153, "supported_features": 43}, "context": {"id": "c6daf624d078492d8a0ac106cf7ce9b6", "user_id": "be7bc3c5a2e043658ceec99b7fa51cb1"}, "entity_id": "light.floor_lamp_upper", "last_changed": "2019-01-27T19:49:37.006341+00:00", "last_updated": "2019-01-27T19:49:37.006341+00:00", "state": "on"}, {"attributes": {"friendly_name": "Spotlightdim"}, "context": {"id": "a815e64c958d42339a730d7e666ccc85", "user_id": null}, "entity_id": "switch.spotlightdim", "last_changed": "2019-01-27T14:50:28.644755+00:00", "last_updated": "2019-01-27T14:50:28.644755+00:00", "state": "off"}, {"attributes": {"access_token": "3c1631ceb2f6dc39a4a8088adf178e84af0e8c8c9bd816553f5a02f31d2da3b1", "entity_picture": "/api/camera_proxy/camera.yoosee_ipcam?token=3c1631ceb2f6dc39a4a8088adf178e84af0e8c8c9bd816553f5a02f31d2da3b1", "friendly_name": "Yoosee-Ipcam", "supported_features": 0}, "context": {"id": "430ff98061d543edb61580f8d3548561", "user_id": null}, "entity_id": "camera.yoosee_ipcam", "last_changed": "2019-01-27T14:50:24.376205+00:00", "last_updated": "2019-01-28T02:52:47.013752+00:00", "state": "idle"}, {"attributes": {"brightness": 255, "friendly_name": "Atomic Sun", "supported_features": 1}, "context": {"id": "c6daf624d078492d8a0ac106cf7ce9b6", "user_id": "be7bc3c5a2e043658ceec99b7fa51cb1"}, "entity_id": "light.atomic_sun", "last_changed": "2019-01-27T19:49:36.700463+00:00", "last_updated": "2019-01-27T19:49:36.700463+00:00", "state": "on"}, {"attributes": {"Battery Level": 44, "Battery State": "Unplugged", "Device Name": "iPad", "Device Type": "iPad 3", "Device Version": "9.3.5", "friendly_name": "iPad Battery Level", "icon": "mdi:battery-40", "unit_of_measurement": "%"}, "context": {"id": "6335f884483343a08872b25ded2a1185", "user_id": null}, "entity_id": "sensor.ipad_battery_level", "last_changed": "2019-01-27T14:50:28.605210+00:00", "last_updated": "2019-01-27T14:50:28.605210+00:00", "state": "44"}, {"attributes": {"friendly_name": "altermist iphone", "source_type": null}, "context": {"id": "276c3fe7c304491a8724a9faba2a3404", "user_id": null}, "entity_id": "device_tracker.altermist_iphone", "last_changed": "2019-01-27T14:50:28.359257+00:00", "last_updated": "2019-01-27T14:50:28.359257+00:00", "state": "not_home"}, {"attributes": {"friendly_name": "sascha's iPhone", "source_type": null}, "context": {"id": "71a790f32e5d4766a03aa549f5dda7e4", "user_id": null}, "entity_id": "device_tracker.saschasiphone", "last_changed": "2019-01-27T14:50:28.347085+00:00", "last_updated": "2019-01-27T14:50:28.347085+00:00", "state": "not_home"}, {"attributes": {"friendly_name": "Bedroom Light 2", "supported_features": 1}, "context": {"id": "019814ea9f6d4e8d8ee83da988cb1c35", "user_id": null}, "entity_id": "light.bedroom_light_2", "last_changed": "2019-01-27T17:17:05.704907+00:00", "last_updated": "2019-01-27T17:17:05.704907+00:00", "state": "unavailable"}, {"attributes": {"attribution": "Weather forecast from met.no, delivered by the Norwegian Meteorological Institute.", "friendly_name": "yr Temperature", "unit_of_measurement": "\u00b0C"}, "context": {"id": "686a28b1b148488193a64b0a9da0e288", "user_id": null}, "entity_id": "sensor.yr_temperature_2", "last_changed": "2019-01-28T02:31:00.227711+00:00", "last_updated": "2019-01-28T02:31:00.227711+00:00", "state": "4.3"}, {"attributes": {"attribution": "Data provided by Digital Ocean", "created_at": "2017-10-30T16:07:33Z", "device_class": "moving", "droplet_id": 68584806, "droplet_name": "d01.acid.services", "features": ["private_networking", "ipv6", "monitoring"], "friendly_name": "d01.acid.services", "ipv4_address": "165.227.29.150", "ipv6_address": "2604:A880:0002:00D0:0000:0000:0088:8001", "memory": 512, "region": "San Francisco 2", "vcpus": 1}, "context": {"id": "a27b22b6fc3f47fb9cab2078a1942cb1", "user_id": null}, "entity_id": "binary_sensor.d01_acid_services", "last_changed": "2019-01-27T14:50:27.779568+00:00", "last_updated": "2019-01-27T14:50:27.779568+00:00", "state": "on"}, {"attributes": {"friendly_name": "saschas iphone", "source_type": null}, "context": {"id": "0d74cbfef2084e1a98308f84687963f7", "user_id": null}, "entity_id": "device_tracker.saschas_iphone", "last_changed": "2019-01-27T14:50:28.369263+00:00", "last_updated": "2019-01-27T14:50:28.369263+00:00", "state": "not_home"}, {"attributes": {"attribution": "Weather forecast from met.no, delivered by the Norwegian Meteorological Institute.", "entity_picture": "https://api.met.no/weatherapi/weathericon/1.1/?symbol=1;content_type=image/png", "friendly_name": "yr Symbol"}, "context": {"id": "1dac828977a948ccaa243af7bcbb28bd", "user_id": null}, "entity_id": "sensor.yr_symbol_2", "last_changed": "2019-01-28T01:31:00.233829+00:00", "last_updated": "2019-01-28T01:31:00.233829+00:00", "state": "1"}, {"attributes": {"entity_id": ["group.bedroom", "switch.front_door", "switch.bathroom_fan", "light.hallway_light", "light.entrance_light", "group.tv_lights", "switch.lock_aciddell", "switch.bathroom_light", "group.living_room_and_kitchen_and_desk", "switch.spotlight"], "friendly_name": "Leaving"}, "context": {"id": "a3f3a6a6d8424732b7b8d0248691e1a0", "user_id": null}, "entity_id": "scene.leaving", "last_changed": "2019-01-27T14:50:24.362944+00:00", "last_updated": "2019-01-27T14:50:24.362944+00:00", "state": "scening"}, {"attributes": {"friendly_name": "Bedroom  Speaker", "supported_features": 21437}, "context": {"id": "1350185a428647e5baba5cd859369a64", "user_id": null}, "entity_id": "media_player.bedroom", "last_changed": "2019-01-27T14:50:27.859584+00:00", "last_updated": "2019-01-27T14:50:27.859584+00:00", "state": "off"}, {"attributes": {"friendly_name": "Countertop Light", "icon": "mdi:lightbulb"}, "context": {"id": "4db397a33f584687a5f63846a28c4045", "user_id": null}, "entity_id": "switch.countertop_light", "last_changed": "2019-01-27T14:50:26.988355+00:00", "last_updated": "2019-01-27T14:50:26.988355+00:00", "state": "on"}, {"attributes": {"Battery Level": 100, "Battery State": "Full", "Device Name": "sascha's iPad", "Device Type": "iPad Pro (12.9-inch)", "Device Version": "11.2.1", "friendly_name": "sascha's iPad Battery Level", "icon": "mdi:battery", "unit_of_measurement": "%"}, "context": {"id": "01f1a97238b2471cb77147cecbe5e4bb", "user_id": null}, "entity_id": "sensor.saschas_ipad_battery_level", "last_changed": "2019-01-27T14:50:28.626229+00:00", "last_updated": "2019-01-27T14:50:28.626229+00:00", "state": "100"}, {"attributes": {"entity_id": ["light.desk_light_3", "light.desk_light_top", "light.600194913b6b_1921680172"], "friendly_name": "Desk", "order": 8}, "context": {"id": "047fc97cef7b445f9fe2d7d161707dab", "user_id": null}, "entity_id": "group.desk", "last_changed": "2019-01-27T14:50:24.446789+00:00", "last_updated": "2019-01-27T14:50:24.446789+00:00", "state": "unknown"}, {"attributes": {"brightness": 254, "color_temp": 366, "friendly_name": "Desk Light", "max_mireds": 500, "min_mireds": 153, "supported_features": 43}, "context": {"id": "c6daf624d078492d8a0ac106cf7ce9b6", "user_id": "be7bc3c5a2e043658ceec99b7fa51cb1"}, "entity_id": "light.desk_light", "last_changed": "2019-01-27T19:49:36.991358+00:00", "last_updated": "2019-01-27T19:49:36.991358+00:00", "state": "on"}, {"attributes": {"entity_id": ["light.window_light", "light.floor_lamp", "switch.spotlight", "switch.top_light_load", "switch.bottom_light_load", "switch.fireplace", "switch.tv", "switch.tv_light"], "friendly_name": "Living Room", "order": 5}, "context":: "2019-01-27T14:50:27.888297+00:00", "state": "on"}, {"attributes": {"friendly_name": "RGBWW Strip 1", "max_mireds": 500, "min_mireds": 153, "supported_features": 147}, "context": {"id": "d042af4102a44642927606c091cf58c6", "user_id": null}, "entity_id": "light.rgbww_strip_1", "last_changed": "2019-01-27T14:50:28.519411+00:00", "last_updated": "2019-01-27T14:50:28.519411+00:00", "state": "off"}, {"attributes": {"entity_id": ["group.bedroom", "group.kitchen", "switch.front_door", "switch.lock_aciddell", "group.living_room", "group.desk", "group.tv_lights", "group.bathroom", "light.hallway_light", "light.entrance_light"], "friendly_name": "Good Night"}, "context": {"id": "1d9ae93e474d4c0483602021449aeee8", "user_id": null}, "entity_id": "scene.good_night", "last_changed": "2019-01-27T14:50:24.370740+00:00", "last_updated": "2019-01-27T14:50:24.370740+00:00", "state": "scening"}, {"attributes": {"entity_id": ["light.floor_lamp_2", "switch.bathroom_fan", "light.desk_light_top", "light.entrance_light", "light.kitchen_light", "light.hallway_light", "switch.top_light_load", "switch.spotlight"], "friendly_name": "Arriving"}, "context": {"id": "8fe8f0723eb44d48b4f41f1b815678b4", "user_id": null}, "entity_id": "scene.arriving", "last_changed": "2019-01-27T14:50:24.365470+00:00", "last_updated": "2019-01-27T14:50:24.365470+00:00", "state": "scening"}, {"attributes": {"attribution": "Weather forecast from met.no, delivered by the Norwegian Meteorological Institute.", "entity_picture": "https://api.met.no/weatherapi/weathericon/1.1/?symbol=1;content_type=image/png", "friendly_name": "yr Symbol"}, "context": {"id": "e526a56170f64669988f8493503463d1", "user_id": null}, "entity_id": "sensor.yr_symbol", "last_changed": "2019-01-28T01:31:00.226943+00:00", "last_updated": "2019-01-28T01:31:00.226943+00:00", "state": "1"}, {"attributes": {"auto": true, "entity_id": ["device_tracker.saschasmacbookpro", "device_tracker.acids_ipad", "device_tracker.saschasiphone", "device_tracker.aciddell", "device_tracker.user_1a30d370359743649279321398cd0574", "device_tracker.altermist_iphone", "device_tracker.user_1a30d370_3597_4364_9279_321398cd0574", "device_tracker.saschas_ipad", "device_tracker.saschas_iphone", "device_tracker.saschasipad"], "friendly_name": "all devices", "hidden": true, "order": 16}, "context": {"id": "f19aa9736df1477b877a4951e5c08426", "user_id": null}, "entity_id": "group.all_devices", "last_changed": "2019-01-27T14:50:28.401501+00:00", "last_updated": "2019-01-27T14:50:28.401501+00:00", "state": "not_home"}, {"attributes": {"friendly_name": "Home", "hidden": true, "icon": "mdi:school", "latitude": 49.3100001, "longitude": -123.077698, "radius": 100.0}, "context": {"id": "6037cdff160b4f2fa9b1768d3d3b3581", "user_id": null}, "entity_id": "zone.home", "last_changed": "2019-01-27T14:50:22.706396+00:00", "last_updated": "2019-01-27T14:50:22.706396+00:00", "state": "zoning"}, {"attributes": {"friendly_name": "Bathroom Light", "icon": "mdi:lightbulb"}, "context": {"id": "171cab253cfe478d8b35d7542d5aeb42", "user_id": null}, "entity_id": "switch.bathroom_light", "last_changed": "2019-01-27T23:46:41.943383+00:00", "last_updated": "2019-01-27T23:46:41.943383+00:00", "state": "off"}, {"attributes": {"friendly_name": "Hallway Light", "supported_features": 1}, "context": {"id": "53481bfa0ea8479ea99b4a85fb29b5d2", "user_id": null}, "entity_id": "light.hallway_light", "last_changed": "2019-01-28T02:55:27.642552+00:00", "last_updated": "2019-01-28T02:55:27.642552+00:00", "state": "unavailable"}, {"attributes": {"Battery Level": 44, "Battery State": "Unplugged", "Device Name": "iPad", "Device Type": "iPad 3", "Device Version": "9.3.5", "friendly_name": "iPad Battery State", "icon": "mdi:power-plug-off"}, "context": {"id": "a3bbda1ceb5545c4be4b679472de019e", "user_id": null}, "entity_id": "sensor.ipad_battery_state", "last_changed": "2019-01-27T14:50:28.654218+00:00", "last_updated": "2019-01-27T14:50:28.654218+00:00", "state": "Unplugged"}, {"attributes": {"azimuth": 263.37, "elevation": -18.61, "friendly_name": "Sun", "next_dawn": "2019-01-28T15:15:04+00:00", "next_dusk": "2019-01-29T01:35:09+00:00", "next_midnight": "2019-01-28T08:25:16+00:00", "next_noon": "2019-01-28T20:25:06+00:00", "next_rising": "2019-01-28T15:49:54+00:00", "next_setting": "2019-01-29T01:00:18+00:00"}, "context": {"id": "b99493c9a9b54c579cb2ce97d28e948e", "user_id": null}, "entity_id": "sun.sun", "last_changed": "2019-01-28T00:58:41.019220+00:00", "last_updated": "2019-01-28T02:55:30.010520+00:00", "state": "below_horizon"}, {"attributes": {"friendly_name": "living room ", "supported_features": 21437}, "context": {"id": "b8c2ac73501b4d56bd5e17624b417e77", "user_id": null}, "entity_id": "media_player.living_room", "last_changed": "2019-01-27T14:50:27.866181+00:00", "last_updated": "2019-01-27T14:50:27.866181+00:00", "state": "off"}, {"attributes": {"Battery Level": 32, "Battery State": "Unplugged", "Device Name": "sascha's iPhone", "Device Type": "iPhone 6", "Device Version": "12.1", "friendly_name": "sascha's iPhone Battery State", "icon": "mdi:power-plug-off"}, "context": {"id": "a7201906049444ec94835e1e397c9d33", "user_id": null}, "entity_id": "sensor.saschas_iphone_battery_state", "last_changed": "2019-01-27T14:50:28.696224+00:00", "last_updated": "2019-01-27T14:50:28.696224+00:00", "state": "Unplugged"}, {"attributes": {"friendly_name": "Bedroom Light 1", "supported_features": 1}, "context": {"id": "4258d44a7a444d489a59e8b1c760a981", "user_id": null}, "entity_id": "light.bedroom_light_1", "last_changed": "2019-01-27T17:16:24.734596+00:00", "last_updated": "2019-01-27T17:16:24.734596+00:00", "state": "unavailable"}, {"attributes": {"friendly_name": "sascha", "source_type": null}, "context": {"id": "8fbce14ef941414ab465310f252bafc9", "user_id": null}, "entity_id": "device_tracker.user_1a30d370359743649279321398cd0574", "last_changed": "2019-01-27T14:50:28.355649+00:00", "last_updated": "2019-01-27T14:50:28.355649+00:00", "state": "not_home"}, {"attributes": {"entity_id": ["group.desk", "light.floor_lamp_2", "light.desk_light", "light.hallway_light", "light.entrance_light", "light.kitchen_light", "light.desk_light_top", "group.tv_lights", "switch.spotlight", "light.window_light", "switch.top_light_load"], "friendly_name": "Living Room Bright"}, "context": {"id": "3473df3a17c246e2a56408ffae708a2f", "user_id": null}, "entity_id": "scene.living_room_bright", "last_changed": "2019-01-27T14:50:24.368174+00:00", "last_updated": "2019-01-27T14:50:24.368174+00:00", "state": "scening"}, {"attributes": {"friendly_name": "saschas ipad", "source_type": null}, "context": {"id": "29fe211678384346b27177a68373193e", "user_id": null}, "entity_id": "device_tracker.saschas_ipad", "last_changed": "2019-01-27T14:50:28.365947+00:00", "last_updated": "2019-01-27T14:50:28.365947+00:00", "state": "not_home"}, {"attributes": {"friendly_name": "Atomic sun"}, "context": {"id": "08ec266e6f194b92897bab10f6d1d05a", "user_id": null}, "entity_id": "switch.atomic_sun", "last_changed": "2019-01-27T14:50:37.595924+00:00", "last_updated": "2019-01-27T14:50:37.595924+00:00", "state": "unavailable"}, {"attributes": {"brightness": 254, "color_temp": 366, "friendly_name": "Floor lamp middle", "max_mireds": 500, "min_mireds": 153, "supported_features": 43}, "context": {"id": "c6daf624d078492d8a0ac106cf7ce9b6", "user_id": "be7bc3c5a2e043658ceec99b7fa51cb1"}, "entity_id": "light.floor_lamp_middle", "last_changed": "2019-01-27T19:49:36.975062+00:00", "last_updated": "2019-01-27T19:49:36.975062+00:00", "state": "on"}, {"attributes": {"friendly_name": "Fluxer"}, "context": {"id": "bb06d7731c7e4b38b02bdb0730a1a67c", "user_id": null}, "entity_id": "switch.fluxer", "last_changed": "2019-01-27T14:50:53.250425+00:00", "last_updated": "2019-01-27T14:50:53.250425+00:00", "state": "off"}, {"attributes": {"entity_id": ["light.kitchen_light", "switch.countertop_light"], "friendly_name": "Kitchen", "order": 4}, "context": {"id": "3e2c08a2537f48c6bd5bbccf3ebbedd2", "user_id": null}, "entity_id": "group.kitchen", "last_changed": "2019-01-27T14:50:27.810695+00:00", "last_updated": "2019-01-27T14:50:27.810695+00:00", "state": "on"}, {"attributes": {"friendly_name": "Tv", "icon": "mdi:monitor"}, "context": {"id": "b36cf9061bb647aba188311fab70f49b", "user_id": null}, "entity_id": "switch.tv", "last_changed": "2019-01-27T22:27:38.351331+00:00", "last_updated": "2019-01-27T22:27:38.351331+00:00", "state": "off"}, {"attributes": {"Battery Level": 100, "Battery State": "Charging", "Device Name": "Altermist iPhone", "Device Type": "iPhone 5", "Device Version": "10.3.3", "friendly_name": "Altermist iPhone Battery Level", "icon": "mdi:bat

 $ curl -X GET \
    -H "Authorization: Bearer ABCDEFGH" \
    -H "Content-Type: application/json" \
    http://IP_ADDRESS:8123/ENDPOINT


    https://developers.home-assistant.io/docs/en/external_api_rest.html

GET /api/discovery_info
Returns basic information about the Home Assistant instance as JSON.

{
    "base_url": "http://192.168.0.2:8123",
    "location_name": "Home",
    "requires_api_password": true,
    "version": "0.56.2"
}

$ curl -X POST -H "Authorization: Bearer ABCDEFGH" \
       -H "Content-Type: application/json" \
       -d '{"entity_id": "switch.christmas_lights"}' \
       http://localhost:8123/api/services/switch/turn_on

 * */

#endif
