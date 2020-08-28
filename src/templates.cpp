/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include "kfc_fw_config.h"
#include "build.h"
#include "misc.h"
#include "web_server.h"
#include "status.h"
#include "reset_detector.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "WebUIAlerts.h"
#if IOT_ALARM_PLUGIN_ENABLED
#include "../src/plugins/alarm/alarm.h"
#include "../src/plugins/ntp/ntp_plugin.h"
#endif

#if DEBUG_TEMPLATES
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include "../src/plugins/ssdp/ssdp.h"

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

String WebTemplate::_aliveRedirection;

WebTemplate::WebTemplate() : _form(nullptr), _json(nullptr)
{
}

WebTemplate::~WebTemplate()
{
    if (_json) {
        __LDBG_delete(_json);
    }
    if (_form) {
        __LDBG_delete(_form);
    }
}

void WebTemplate::setSelfUri(const String &selfUri)
{
    _selfUri = selfUri;
}

void WebTemplate::setForm(Form *form)
{
    _json = nullptr; //static_cast<SettingsForm *>(form)->_json;
    _form = form;
}

Form *WebTemplate::getForm()
{
    return _form;
}

JsonUnnamedObject *WebTemplate::getJson()
{
    __DBG_print("called");
    return _json;
}

PrintArgs &WebTemplate::getPrintArgs()
{
    return _printArgs;
}

void WebTemplate::printSystemTime(time_t now, PrintHtmlEntitiesString &output)
{
    //char buf[80];
    auto format = PSTR("%a, %d %b %Y " HTML_SA(span, HTML_A("id", "system_time")) "%H:%M:%S" HTML_E(span) " %Z");
//    strftime_P(buf, sizeof(buf), format, );
    output.printf_P(PSTR(HTML_SA(span, HTML_A("id", "system_date") HTML_A("format", "%s"))), PrintHtmlEntitiesString(FPSTR(format)).c_str());
    output.strftime_P(format, localtime(&now));
    output.print(F(HTML_E(span)));
}

void WebTemplate::printUniqueId(Print &output, const String &name, int8_t dashPos)
{
    uint16_t crc[4];

#if defined(ESP8266)

    typedef struct __attribute__packed__ {
        uint32_t chip_id;
        uint32_t flash_chip_id;
        uint8_t mac[2 * 6];
    } unique_device_info_t;
    unique_device_info_t info = { system_get_chip_id(), ESP.getFlashChipId()/* cached version of spi_flash_get_id() */ };
    wifi_get_macaddr(STATION_IF, info.mac);
    wifi_get_macaddr(SOFTAP_IF, info.mac + 6);

#elif defined(ESP32)

    typedef struct __attribute__packed__ {
        esp_chip_info_t chip_id;
        uint32_t flash_chip_id;
        uint8_t mac[4 * 6];
    } unique_device_info_t;
    unique_device_info_t info;

    esp_chip_info(&info.chip_id);
    info.flash_chip_id = ESP.getFlashChipSize();
    esp_read_mac(info.mac, ESP_MAC_WIFI_STA);
    esp_read_mac(info.mac + 6, ESP_MAC_WIFI_SOFTAP);
    esp_read_mac(info.mac + 12, ESP_MAC_BT);
    esp_read_mac(info.mac + 18, ESP_MAC_ETH);

#else
#error Platform not supported
#endif

    crc[0] = crc16_update(~0, &info, sizeof(info));
    crc[1] = crc16_update(crc[0], &info.chip_id, sizeof(info.chip_id));
    crc[1] = crc16_update(crc[1], &info.flash_chip_id, sizeof(info.flash_chip_id));
    crc[2] = crc16_update(crc[1], info.mac, sizeof(info.mac));
    crc[3] = crc16_update(~0, name.c_str(), name.length());

    for(int8_t i = 0; i < 4; i++) {
        if (i == dashPos) {
            output.print('-');
        }
        output.printf_P(PSTR("%04x"), crc[i]);
    }
}

void WebTemplate::printVersion(Print &output)
{
    output.print(F("KFC FW "));
    output.print(config.getFirmwareVersion());
}

void WebTemplate::printWebInterfaceUrl(Print &output)
{
    auto cfg = System::WebServer::getConfig();
    output.print(cfg.is_https ? FSPGM(https) : FSPGM(http));
    output.print(F("://"));
    WiFi.localIP().printTo(output);
    output.printf_P(PSTR(":%u/"), cfg.getPort());
}

void WebTemplate::printModel(Print &output)
{
#if defined(MQTT_AUTO_DISCOVERY_MODEL)
        output.print(F(MQTT_AUTO_DISCOVERY_MODEL));
#elif IOT_SWITCH
    #if IOT_SWITCH_CHANNEL_NUM>1
        output.print(F(_STRINGIFY(IOT_SWITCH_CHANNEL_NUM) " Channel Switch"));
    #else
        output.print(F("Switch"));
    #endif
#elif IOT_DIMMER_MODULE
    #if IOT_DIMMER_MODULE_CHANNELS > 1
        output.print(F(_STRINGIFY(IOT_DIMMER_MODULE_CHANNELS) " Channel MOSFET Dimmer"));
    #else
        output.print(F("MOSFET Dimmer"));
    #endif
#else
        output.print(F("Generic"));
#endif
#if defined(ESP8266)
        output.print(F("/ESP8266"));
#elif defined(ESP32)
        output.print(F("/ESP32"));
#elif defined(__AVR__) || defined(__avr__)
        output.print(F("/AVR"));
#else
        output.print(F("/Unknown"));
#endif
}

void WebTemplate::printSSDPUUID(Print &output)
{
    auto &ssdp = static_cast<SSDPClassEx &>(SSDP);
    output.print(ssdp.getUUID());
}

void WebTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (String_equals(key, PSTR("HOSTNAME"))) {
        output.print(System::Device::getName());
    }
    else if (String_equals(key, F("TITLE"))) {
        output.print(System::Device::getTitle());
    }
    else if (String_equals(key, PSTR("HARDWARE"))) {
#if defined(ESP8266)
        output.printf_P(PSTR("ESP8266 %s Flash, %d Mhz, Free RAM %s"), formatBytes(ESP.getFlashChipRealSize()).c_str(), system_get_cpu_freq(), formatBytes(ESP.getFreeHeap()).c_str());
#elif defined(ESP32)
        output.printf_P(PSTR("ESP32 %s Flash, %d Mhz, Free RAM %s"), formatBytes(ESP.getFlashChipSize()).c_str(), ESP.getCpuFreqMHz(), formatBytes(ESP.getFreeHeap()).c_str());
#else
#error Platform not supported
#endif
#if LOAD_STATISTICS
        output.printf_P(PSTR(HTML_S(br) "Load Average %.2f %.2f %.2f"), LOOP_COUNTER_LOAD(load_avg[0]), LOOP_COUNTER_LOAD(load_avg[1]), LOOP_COUNTER_LOAD(load_avg[2]));
#endif
    }
    else if (String_equals(key, PSTR("VERSION"))) {
        printVersion(output);
    }
    else if (String_equals(key, PSTR("SOFTWARE"))) {
        printVersion(output);
        if (System::Flags::getConfig().is_default_password) {
            output.printf_P(PSTR(HTML_S(br) HTML_S(strong) "%s" HTML_E(strong)), SPGM(default_password_warning));
        }
    }
#if RTC_SUPPORT
    else if (String_equals(key, PSTR("RTC_STATUS"))) {
        config.printRTCStatus(output, false);
    }
#endif
#if NTP_CLIENT || RTC_SUPPORT
    else if (String_equals(key, PSTR("TIME"))) {
        auto now = time(nullptr);
        if (!IS_TIME_VALID(now)) {
            output.print(F("No time available"));
        } else {
            printSystemTime(now, output);
        }
    }
#endif
    else if (String_equals(key, PSTR("SELF_URI"))) {
        output.print(_selfUri);
    }
    else if (String_equals(key, PSTR("SAFEMODE"))) {
        if (config.isSafeMode()) {
            output.print(F(" - Running in SAFE MODE"));
        }
    }
    else if (String_equals(key, PSTR("UPTIME"))) {
        output.print(formatTime((long)(millis() / 1000)));
    }
    else if (String_equals(key, PSTR("WIFI_UPTIME"))) {
        output.print(System::Flags::getConfig().is_station_mode_enabled ? (!KFCFWConfiguration::isWiFiUp() ? FSPGM(Offline, "Offline") : formatTime((millis() - KFCFWConfiguration::getWiFiUp()) / 1000)) : F("Client mode disabled"));
    }
    else if (String_equals(key, PSTR("IP_ADDRESS"))) {
        WiFi_get_address(output);
    }
    else if (String_equals(key, PSTR("WIFI_IP_ADDRESS"))) {
        WiFi.localIP().printTo(output);
    }
    else if (String_equals(key, PSTR("WEB_INTERFACE_URL"))) {
        WebTemplate::printWebInterfaceUrl(output);
    }
    else if (String_equals(key, PSTR("MODEL"))) {
        WebTemplate::printModel(output);
    }
    else if (String_equals(key, PSTR("UNIQUE_ID"))) {
        WebTemplate::printUniqueId(output, FSPGM(kfcfw), -1);
    }
    else if (String_equals(key, PSTR("SSDP_UUID"))) {
        WebTemplate::printSSDPUUID(output);
    }
    else if (String_equals(key, PSTR("FIRMWARE_UPGRADE_FAILURE"))) {
    }
    else if (String_equals(key, PSTR("FIRMWARE_UPGRADE_FAILURE_CLASS"))) {
        output.print(FSPGM(_hidden));
    }
    else if (String_equals(key, PSTR("ALIVE_REDIRECTION"))) {
        if (_aliveRedirection.length()) {
            output.print(_aliveRedirection);
        }
        else {
            output.print(FSPGM(index_html));
        }
    }
    else if (String_equals(key, PSTR("IS_CONFIG_DIRTY"))) {
        if (!config.isConfigDirty()) {
            output.print(FSPGM(_hidden));
        }
    }
    else if (String_equals(key, PSTR("IS_CONFIG_DIRTY_CLASS"))) {
        if (config.isConfigDirty()) {
            output.print(F(" fade show"));
        } else {
            output.print(FSPGM(_hidden));
        }
    }
    else if (String_equals(key, PSTR("WEBUI_ALERTS_STATUS"))) {
#if WEBUI_ALERTS_ENABLED
        if (System::Flags::getConfig().disableWebAlerts) {
            output.print(F("Disabled"));
        } else {
            output.printf_P(PSTR("Storage %s, rewrite size %d, poll interval %.2fs, WebUI max. height %s"), SPGM(alerts_storage_filename), WEBUI_ALERTS_REWRITE_SIZE, WEBUI_ALERTS_POLL_INTERVAL / 1000.0, WEBUI_ALERTS_MAX_HEIGHT);
        }
#else
        output.print(F("Send to logger"));
#endif
    }
    else if (String_equals(key, PSTR("WEBUI_ALERTS_JSON"))) {
#if WEBUI_ALERTS_ENABLED
        WebUIAlerts_printAsJson(output, 1);
#endif
    }
#if IOT_ALARM_PLUGIN_ENABLED
    else if (String_startsWith(key, PSTR("ALARM_TIMESTAMP_"))) {
        uint8_t num = atoi(key.c_str() + 16);
        if (num < Plugins::Alarm::MAX_ALARMS) {
            auto cfg = Plugins::Alarm::getConfig().alarms[num];
            if (cfg.is_enabled) {
                char buf[32];
                time_t _now = (time_t)cfg.time.timestamp;
                strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), localtime(&_now));
                output.print(buf);
            }
        }
    }
#endif
    else if (_form) {
        _form->process(key, output);
    }
    else if (String_endsWith(key, PSTR("_STATUS"))) {
        uint8_t cmp_length = key.length() - 7;
        for(auto plugin: plugins) {
            if (plugin->hasStatus() && strncasecmp_P(key.c_str(), plugin->getName_P(), cmp_length) == 0) {
                plugin->getStatus(output);
                return;
            }
        }
        output.print(FSPGM(Not_supported, "Not supported"));
    }
    else {
        output.print(F("KEY NOT FOUND: %"));
        output.print(key);
        output.print('%');
    }
}

UpgradeTemplate::UpgradeTemplate() {
}

UpgradeTemplate::UpgradeTemplate(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

void UpgradeTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (String_equals(key, F("FIRMWARE_UPGRADE_FAILURE_CLASS"))) {
    }
    else if (String_equals(key, F("FIRMWARE_UPGRADE_FAILURE"))) {
        output.printRaw(_errorMessage);
    }
    else {
        WebTemplate::process(key, output);
    }
}

void UpgradeTemplate::setErrorMessage(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

LoginTemplate::LoginTemplate() {
}

LoginTemplate::LoginTemplate(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

void LoginTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (String_equals(key, F("LOGIN_ERROR_MESSAGE"))) {
        output.print(_errorMessage);
    }
    else if (String_equals(key, F("LOGIN_ERROR_CLASS"))) {
        if (_errorMessage.length() == 0) {
            output.print(FSPGM(_hidden));
        }
    }
    else if (String_equals(key, F("LOGIN_KEEP_DAYS"))) {
        output.print(System::Device::getConfig().getWebUICookieLifetime());
    }
    else {
        WebTemplate::process(key, output);
    }
}

void LoginTemplate::setErrorMessage(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

void ConfigTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (_form) {
        if (getForm()->process(key, output)) {
            return;
        }
    }

    if (String_equals(key, F("NETWORK_MODE"))) {
        bool m = false;
        auto flags = System::Flags::getConfig();
        if (flags.is_softap_enabled) {
            output.print(F("#ap_mode"));
            m = true;
        }
        if (flags.is_station_mode_enabled) {
            if (m) {
                output.print(',');
            }
            output.print(F("#station_mode"));
        }
    }
    else if (String_startsWith(key, F("MAX_CHANNELS"))) {
        output.print(config.getMaxWiFiChannels());
    }
    else if (String_startsWith(key, F("MODE_"))) {
        if (System::Flags::getConfig().getWifiMode() == key.substring(5).toInt()) {
            output.print(FSPGM(_selected, " selected"));
        }
    }
    else if (String_equals(key, F("SSL_CERT"))) {
#if WEBSERVER_TLS_SUPPORT
        File file = SPIFFS.open(FSPGM(server_crt, "/.pvt/server.crt"), fs::FileOpenMode::read);
        if (file) {
            output.print(file.readString());
        }
#endif
    }
    else if (String_equals(key, F("SSL_KEY"))) {
#if WEBSERVER_TLS_SUPPORT
        File file = SPIFFS.open(FSPGM(server_key, "/.pvt/server.key"), fs::FileOpenMode::read);
        if (file) {
            output.print(file.readString());
        }
#endif
    }
    else {
        WebTemplate::process(key, output);
    }
}

void StatusTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (String_equals(key, F("GATEWAY"))) {
        WiFi.gatewayIP().printTo(output);
    }
    else if (String_equals(key, F("DNS"))) {
        WiFi.dnsIP().printTo(output);
        if (WiFi.dnsIP(1)) {
            output.print(FSPGM(comma_));
            WiFi.dnsIP(1).printTo(output);
        }
    }
    else if (String_equals(key, F("SSL_STATUS"))) {
        #if ASYNC_TCP_SSL_ENABLED
            #if WEBSERVER_TLS_SUPPORT
                output.printf_P(PSTR("TLS enabled, HTTPS %s"), _Config.getOptions().isHttpServerTLS() ? SPGM(enabled, "enabled") : SPGM(Disabled));
            #else
                output.printf_P(PSTR("TLS enabled, HTTPS not supported"));
            #endif
        #else
            output.print(FSPGM(Not_supported));
        #endif
    }
    else if (String_equals(key, F("WIFI_MODE"))) {
        switch (WiFi.getMode()) {
            case WIFI_STA: {
                    output.print(FSPGM(Station_Mode, "Station Mode"));
                    if (System::Flags::getConfig().is_softap_standby_mode_enabled) {
                        output.printf_P(PSTR(" (%s in stand-by mode)"), SPGM(Access_Point));
                    }
                } break;
            case WIFI_AP:
                output.print(FSPGM(Access_Point, "Access Point"));
                break;
            case WIFI_AP_STA:
                output.printf_P(PSTR("%s and %s"), SPGM(Access_Point), SPGM(Station_Mode));
                break;
            case WIFI_OFF:
                output.print(FSPGM(Off, "Off"));
                break;
            default:
                output.printf_P(PSTR("Invalid mode %d"), WiFi.getMode());
                break;
        }
    }
    else if (String_equals(key, F("WIFI_SSID"))) {
        if (WiFi.getMode() == WIFI_AP_STA) {
            output.print(F("Station connected to " HTML_S(strong)));
            WiFi_Station_SSID(output);
            output.printf(PSTR(HTML_E(strong) HTML_S(br) "%s " HTML_S(strong)), SPGM(Access_Point));
            WiFi_SoftAP_SSID(output);
            output.print(F(HTML_E(strong)));
        }
        else if (WiFi.getMode() & WIFI_STA) {
            WiFi_Station_SSID(output);
        }
        else if (WiFi.getMode() & WIFI_AP) {
            WiFi_SoftAP_SSID(output);
        }
    }
    else if (String_equals(key, F("WIFI_STATUS"))) {
        WiFi_get_status(output);
    }
    else {
        WebTemplate::process(key, output);
    }
}

PasswordTemplate::PasswordTemplate(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

void PasswordTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (String_equals(key, F("PASSWORD_ERROR_MESSAGE"))) {
        output.print(_errorMessage);
    }
    else if (String_equals(key, F("PASSWORD_ERROR_CLASS"))) {
        if (_errorMessage.length() == 0) {
            output.print(FSPGM(_hidden));
        }
    }
    else {
        WebTemplate::process(key, output);
    }
}

SettingsForm::SettingsForm(AsyncWebServerRequest *request) : Form(static_cast<FormData *>(request)) //, _json(nullptr)
{
}


File2String::File2String(const String &filename)
{
    _filename = filename;
}

String File2String::toString()
{
    return SPIFFS.open(_filename, fs::FileOpenMode::read).readString();
}

void File2String::fromString(const String &value)
{
    SPIFFS.open(_filename, fs::FileOpenMode::write).print(value);
    //write((const uint8_t *)value.c_str(), value.length());
}

// void EmptyTemplate::process(const String &key, PrintHtmlEntitiesString &output) {
// }

#include <TemplateDataProvider.h>

class PluginStatusStream {
public:
    PluginStatusStream() : _iterator(plugins.begin()) {
        _fillBuffer();
    }

    size_t readBytes(uint8_t *data, size_t size) {
        if (_buffer.length() == 0) {
            if (_fillBuffer() == 0) {
                return 0;
            }
        }
        if (size > _buffer.length()) {
            size = _buffer.length();
        }
        memcpy(data, _buffer.c_str(), size);
        _buffer.remove(0, size);
        return size;
    }

private:
    size_t _fillBuffer() {
        while (_iterator != plugins.end()) {
            auto &plugin = **_iterator;
            if (plugin.hasStatus()) {
                _buffer += F("<div class=\"row\"><div class=\"col-lg-3 col-lg-auto\">");
                _buffer += FPSTR(plugin.getFriendlyName());
                _buffer += "</div><div class=\"col\">";
                plugin.getStatus(_buffer);
                _buffer += "</div></div>";
                ++_iterator;
                break;
            }
            ++_iterator;
        }
        return _buffer.length();
    }

private:
    PrintHtmlEntitiesString _buffer;
    PluginsVector::iterator _iterator;
};


bool TemplateDataProvider::callback(const String& name, DataProviderInterface& provider, WebTemplate &webTemplate) {

    enum class FillBufferMethod {
        NONE = 0,
        PRINT_ARGS,
        BUFFER_STREAM
    };

    auto &printArgs = webTemplate.getPrintArgs();
    PrintHtmlEntitiesString value;
    FillBufferMethod fbMethod = FillBufferMethod::NONE;

    // menus
    if (String_equals(name, PSTR("MENU_HTML_MAIN"))) {
        bootstrapMenu.html(printArgs);
        fbMethod = FillBufferMethod::PRINT_ARGS;
    }
    else if (String_startsWith(name, PSTR("MENU_HTML_MAIN_"))) {
        bootstrapMenu.html(printArgs, bootstrapMenu.findMenuByLabel(name.substring(15)));
        fbMethod = FillBufferMethod::PRINT_ARGS;
    }
    else if (String_startsWith(name, PSTR("MENU_HTML_SUBMENU_"))) {
        bootstrapMenu.htmlSubMenu(printArgs, bootstrapMenu.findMenuByLabel(name.substring(18)), 0);
        fbMethod = FillBufferMethod::PRINT_ARGS;
    }
    // forms
    else if (String_equals(name, PSTR("FORM_HTML"))) {
        auto form = webTemplate.getForm();
        if (form) {
            form->createHtml(printArgs);
            fbMethod = FillBufferMethod::PRINT_ARGS;
        }
    }
    else if (String_equals(name, PSTR("FORM_VALIDATOR"))) {
        auto form = webTemplate.getForm();
        if (form) {
            form->createJavascript(printArgs);
            fbMethod = FillBufferMethod::PRINT_ARGS;
        }
    }
    else if (String_equals(name, PSTR("FORM_JSON"))) {
        auto json = webTemplate.getJson();
        if (json) {
            auto stream = std::shared_ptr<JsonBuffer>(new JsonBuffer(*json));
            provider.setFillBuffer([stream](uint8_t *buffer, size_t size) {
                return stream->fillBuffer(buffer, size);
            });
            return true;
        }
    }
    // plugin status
    else if (String_equals(name, PSTR("PLUGIN_STATUS"))) {
        auto stream = std::shared_ptr<PluginStatusStream>(new PluginStatusStream());
        provider.setFillBuffer([stream](uint8_t *buffer, size_t size) {
            return stream->readBytes(buffer, size);
        });
        return true;
    }
    // other variables
    else {
        webTemplate.process(name, value);
        // debug_printf_P(PSTR("%s=%s\n"), name.c_str(), value.c_str());
        fbMethod = FillBufferMethod::BUFFER_STREAM;
    }

    switch(fbMethod)  {
        case FillBufferMethod::PRINT_ARGS: {
                provider.setFillBuffer([&printArgs](uint8_t *buffer, size_t size) {
                    return printArgs.fillBuffer(buffer, size);
                });
                return true;
            }
        case FillBufferMethod::BUFFER_STREAM: {
                auto stream = std::shared_ptr<BufferStream>(new BufferStream(std::move(value)));
                provider.setFillBuffer([stream](uint8_t *buffer, size_t size) {
                    return stream->readBytes(buffer, size);
                });
                return true;
            }
        default:
        case FillBufferMethod::NONE:
            break;
    }
    return false;
}
