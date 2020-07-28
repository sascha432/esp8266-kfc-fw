/**
 * Author: sascha_lammers@gmx.de
 */

#include "ssdp.h"
#include "templates.h"
#include "WiFiCallbacks.h"

using KFCConfigurationClasses::System;

static SSDPPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SSDPPlugin,
    "ssdp",             // name
    "SSDP",             // friendly name
    "",                 // web_templates
    "",                 // config_forms
    // reconfigure_dependencies
    "wifi,network,device,http",
    PluginComponent::PriorityType::SSDP,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

SSDPPlugin::SSDPPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SSDPPlugin)), _running(false)
{
    REGISTER_PLUGIN(this, "SSDPPlugin");
}

static void wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        plugin._begin();
    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        plugin._end();
    }
}

void SSDPPlugin::_begin()
{
    PrintString tmp;
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(System::WebServer::getConfig().port);
    SSDP.setDeviceType(F("upnp:rootdevice"));
    SSDP.setName(System::Device::getName());
    WebTemplate::printUniqueId(tmp, FSPGM(kfcfw));
    SSDP.setSerialNumber(tmp);
    tmp = PrintString();
    WebTemplate::printModel(tmp);
    SSDP.setModelName(tmp);
    tmp = PrintString();
    WebTemplate::printVersion(tmp);
    SSDP.setModelNumber(tmp);
    tmp = PrintString();
    WebTemplate::printWebInterfaceUrl(tmp);
    SSDP.setModelURL(tmp);
    SSDP.setManufacturer(F("KFCLabs"));
    SSDP.setManufacturerURL(F("https://github.com/sascha432"));
    SSDP.setURL(String('/'));
    _running = SSDP.begin();
    __DBG_printf("SSDP=%u", _running);
}

void SSDPPlugin::_end()
{
    SSDP.end();
    _running = false;
}

void SSDPPlugin::setup(SetupModeType mode)
{
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, wifiCallback);
}

void SSDPPlugin::reconfigure(const String &source)
{
    shutdown();
    setup(PluginComponent::SetupModeType::DEFAULT);
}

void SSDPPlugin::shutdown()
{
    SSDP.end();
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
}

void SSDPPlugin::getStatus(Print &output)
{
    if (_running) {
        WebTemplate::printWebInterfaceUrl(output);
        output.print(F("description.xml" HTML_S(br)));
        WebTemplate::printSSDPUUID(output);
        SSDP.schema(output);
    }
    else {
        output.print(FSPGM(Disabled));

    }
}
