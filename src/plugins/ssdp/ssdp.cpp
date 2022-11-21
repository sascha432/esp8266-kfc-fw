/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SSDP_SUPPORT

#include "ssdp.h"
#include "templates.h"
#include "WiFiCallbacks.h"

#if DEBUG_SSDP
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

static SSDPPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SSDPPlugin,
    "ssdp",             // name
    "SSDP",             // friendly name
    "",                 // web_templates
    "",                 // config_forms
    // reconfigure_dependencies
    "wifi,network,device",
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
    __LDBG_printf("running=%u", _running);
    PrintString tmp;
    __LDBG_printf("schema=%s", SPGM(description_xml));
    SSDP.setSchemaURL(FSPGM(description_xml));
    __LDBG_printf("port=%u", System::WebServer::getConfig().getPort());
    SSDP.setHTTPPort(System::WebServer::getConfig().getPort());
    SSDP.setDeviceType(F("upnp:rootdevice"));
    __LDBG_printf("name=%s", System::Device::getName());
    SSDP.setName(System::Device::getName());
    WebTemplate::printUniqueId(tmp, FSPGM(kfcfw));
    __LDBG_printf("serial=%s", __S(tmp));
    SSDP.setSerialNumber(tmp);
    tmp = PrintString();
    WebTemplate::printModel(tmp);
    __LDBG_printf("model=%s", __S(tmp));
    SSDP.setModelName(tmp);
    __LDBG_printf("version=%s", __S(config.getShortFirmwareVersion()));
    SSDP.setModelNumber(config.getShortFirmwareVersion());
    tmp = PrintString();
    WebTemplate::printWebInterfaceUrl(tmp);
    __LDBG_printf("weburl=%s", __S(tmp));
    SSDP.setModelURL(tmp);
    __LDBG_printf("manufacturer=%s", SPGM(KFCLabs));
    SSDP.setManufacturer(FSPGM(KFCLabs, "KFCLabs"));
    __LDBG_printf("manuf_url=%s", PSTR("https://github.com/sascha432"));
    SSDP.setManufacturerURL(F("https://github.com/sascha432"));
    __LDBG_printf("url=/");
    SSDP.setURL(String('/'));
    __LDBG_printf("SSDP.begin()");
    _running = SSDP.begin();
    __LDBG_printf("SSDP=%u", _running);
}

void SSDPPlugin::_end()
{
    __LDBG_printf("running=%u", _running);
    SSDP.end();
    _running = false;
}

void SSDPPlugin::setup(SetupModeType mode, const DependenciesPtr &dependencies)
{
    if (System::Flags::getConfig().is_ssdp_enabled) {
        WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, wifiCallback);
        if (WiFi.isConnected()) {
            wifiCallback(WiFiCallbacks::EventType::CONNECTED, nullptr);
        }
    }
}

void SSDPPlugin::reconfigure(const String &source)
{
    shutdown();
    setup(PluginComponent::SetupModeType::DEFAULT, nullptr);
}

void SSDPPlugin::shutdown()
{
    _end();
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
}


void SSDPPlugin::getStatus(Print &output)
{
    __LDBG_printf("enabled=%u running=%", System::Flags::getConfig().is_ssdp_enabled, _running);
    if (System::Flags::getConfig().is_ssdp_enabled && _running) {
        WebTemplate::printWebInterfaceUrl(output);
        output.print(FSPGM(description_xml, "description.xml"));
        output.print(HTML_S(br));
        WebTemplate::printSSDPUUID(output);
    }
    else {
        output.print(FSPGM(Disabled));
    }
}

#endif
