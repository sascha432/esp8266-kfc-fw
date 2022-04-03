/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include "PluginComponent.h"
#include "kfc_fw_config.h"
#include "build.h"
#include "misc.h"
#include "web_server.h"
#include "status.h"
#include "reset_detector.h"
#include "plugins_menu.h"
#include "../src/plugins/plugins.h"
#include  "spgm_auto_def.h"
#include "save_crash.h"
#if ESP32
#include <sdkconfig.h>
#endif

#if DEBUG_TEMPLATES
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

String WebTemplate::_aliveRedirection;

void WebTemplate::printSystemTime(time_t now, PrintHtmlEntitiesString &output)
{
    auto format = PSTR("%a, %d %b %Y " HTML_SA(span, HTML_A("id", "system_time")) "%H:%M:%S" HTML_E(span) " %Z");
    output.printf_P(PSTR(HTML_SA(span, HTML_A("id", "system_date") HTML_A("format", "%s"))), PrintHtmlEntitiesString(FPSTR(format)).c_str());
    output.strftime_P(format, localtime(&now));
    output.print(F(HTML_E(span)));
}

void WebTemplate::printUniqueId(Print &output, const String &name, int8_t dashPos)
{
    #if defined(ESP8266)

        struct __attribute__packed__ {
            uint32_t chip_id;
            uint32_t flash_chip_id;
            union __attribute__packed__ {
                uint8_t mac[2 * WL_MAC_ADDR_LENGTH];
                struct __attribute__packed__ {
                    uint8_t mac_station[WL_MAC_ADDR_LENGTH];
                    uint8_t mac_softap[WL_MAC_ADDR_LENGTH];
                };
            };
        } info = {
            system_get_chip_id(),
            ESP.getFlashChipId() /* cached version of spi_flash_get_id() */
        };
        wifi_get_macaddr(STATION_IF, info.mac_station);
        wifi_get_macaddr(SOFTAP_IF, info.mac_softap);

    #elif defined(ESP32)

        struct __attribute__packed__ {
            esp_chip_info_t chip_id;
            uint32_t flash_chip_id;
            union __attribute__packed__ {
                uint8_t mac[4 * WL_MAC_ADDR_LENGTH];
                struct __attribute__packed__ {
                    uint8_t mac_wifi_sta[WL_MAC_ADDR_LENGTH];
                    uint8_t mac_wifi_soft_ap[WL_MAC_ADDR_LENGTH];
                    uint8_t mac_bt[WL_MAC_ADDR_LENGTH];
                    uint8_t mac_eth[WL_MAC_ADDR_LENGTH];
                };
            };
        } info;

        esp_chip_info(&info.chip_id);
        info.flash_chip_id = ESP.getFlashChipSize();
        esp_read_mac(info.mac_wifi_sta, ESP_MAC_WIFI_STA);
        esp_read_mac(info.mac_wifi_soft_ap, ESP_MAC_WIFI_SOFTAP);
        esp_read_mac(info.mac_bt, ESP_MAC_BT);
        esp_read_mac(info.mac_eth, ESP_MAC_ETH);

    #else

        #error Platform not supported

    #endif

    uint16_t crc[4];

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

void WebTemplate::printVersion(Print &output, bool full)
{
    output.print(F("KFC FW "));
    output.print(config.getFirmwareVersion());
    #if ESP32
        if (full) {
            output.printf_P(PSTR(HTML_S(br) "ESP-IDF Version %s"), esp_get_idf_version());
        }
    #endif
}

void WebTemplate::printWebInterfaceUrl(Print &output)
{
    auto cfg = System::WebServer::getConfig();
    output.print(cfg.is_https ? F("https://") : F("http://"));
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
    output.print('/');
    output.print(KFCFWConfiguration::getChipModel());
}

void WebTemplate::printFileSystemInfo(Print &output)
{
    #if USE_LITTLEFS
        FSInfo info;
        KFCFS.info(info);
        output.printf_P(PSTR("Block size %d" HTML_S(br)
                "Max. open files %d" HTML_S(br)
                "Max. path length %d" HTML_S(br)
                "Page size %d" HTML_S(br)
                "Total bytes %d" HTML_S(br)
                "Used bytes %d (%.2f%%)" HTML_S(br)
            ), info.blockSize, info.maxOpenFiles, info.maxPathLength, info.pageSize, info.totalBytes, info.usedBytes, (info.usedBytes * 100) / static_cast<float>(info.totalBytes)
        );
    #else
        output.print(F("SPIFFS is deprecated." HTML_S(br) "Please consider to upgrade to LittleFS or another file system"));
    #endif
}

#if IOT_SSDP_SUPPORT

void WebTemplate::printSSDPUUID(Print &output)
{
    auto &ssdp = static_cast<SSDPClassEx &>(SSDP);
    output.print(ssdp.getUUID());
}

#endif

void WebTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    #if DEBUG_TEMPLATES
        struct OnReturn {
            OnReturn(const String &key) : _key(key) {
            }
            ~OnReturn() {
                __DBG_printf("key=%s", __S(_key));
            }
            const String &_key;
        } onReturn(key);
    #endif
    __DBG_validatePointer(&key, VP_HPS);
    __DBG_validatePointer(&output, VP_HPS);
    __DBG_validatePointer(&_form, VP_HPS);
    __DBG_validatePointer(_form, VP_NHPS);
    __LDBG_printf("key=%s %p form=%p auth=%u", __S(key), std::addressof(key), _form, isAuthenticated());
    // ------------------------------------------------------------------------------------
    // public variables
    // ------------------------------------------------------------------------------------
    if (key == F("HOSTNAME")) {
        output.print(System::Device::getName());
    }
    else if (key == F("LOGIN_USERNAME")) {
        output.print(System::Device::getUsername());
    }
    else if (key == F("TITLE")) {
        output.print(System::Device::getTitle());
    }
    else if (key == F("SELF_URI")) {
        output.print(_selfUri);
    }
    else if (key == F("ALIVE_REDIRECTION")) {
        if (_aliveRedirection.length()) {
            output.print(_aliveRedirection);
        }
        else {
            output.print(FSPGM(index_html));
        }
    }
    #if DEBUG_ASSETS
        else if (key == F("DEBUG_ASSETS_URL1")) {
            output.print(F(DEBUG_ASSETS_URL1));
        }
        else if (key == F("DEBUG_ASSETS_URL2")) {
            output.print(F(DEBUG_ASSETS_URL2));
        }
    #endif
    // ------------------------------------------------------------------------------------
    // SSDP public info
    // ------------------------------------------------------------------------------------
    else if (key == F("WIFI_IP_ADDRESS")) {
        WiFi.localIP().printTo(output);
    }
    else if (key == F("WEB_INTERFACE_URL")) {
        printWebInterfaceUrl(output);
    }
    else if (key == F("VERSION")) {
        printVersion(output);
    }
    else if (key == F("VERSION_ONLY")) {
        printVersion(output, false);
    }
    else if (key == F("MODEL")) {
        printModel(output);
    }
    else if (key == F("UNIQUE_ID")) {
        printUniqueId(output, FSPGM(kfcfw), -1);
    }
    else if (key == F("SSDP_UUID")) {
    #if IOT_SSDP_SUPPORT
            printSSDPUUID(output);
    #else
            output.print(F("<SSDP support disabled>"));
    #endif
    }
    // ------------------------------------------------------------------------------------
    // requires to be authenticated after the next block
    // ------------------------------------------------------------------------------------
    else if (!isAuthenticated()) {
        // replacements for unauthenticated clients
        // everything else will be replaced with an empty string
        if (key == F("IS_CONFIG_DIRTY")) {
            output.print(FSPGM(_hidden));
        }
        else if (key == F("IS_CONFIG_DIRTY_CLASS")) {
            output.print(FSPGM(hidden));
        }
        // __DBG_printf("return key='%s'", key.c_str());
        return;
    }
    // ------------------------------------------------------------------------------------
    // private variables
    // ------------------------------------------------------------------------------------
    else if (key == F("HARDWARE")) {
        #if ESP8266
            output.printf_P(PSTR("ESP8266 %s Flash, %d Mhz, Free RAM %s"),
                formatBytes(ESP.getFlashChipRealSize()).c_str(),
                system_get_cpu_freq(),
                formatBytes(ESP.getFreeHeap()).c_str()
            );
        #elif ESP32
            output.printf_P(PSTR("ESP32 %s Flash, %d Mhz, Free RAM %s, "),
                formatBytes(ESP.getFlashChipSize()).c_str(),
                ESP.getCpuFreqMHz(),
                formatBytes(ESP.getFreeHeap()).c_str()
            );
            #if CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM
                if (psramFound()) {
                    output.printf_P(PSTR("Free PSRam %s, "),
                        formatBytes(ESP.getFreePsram()).c_str()
                    );
                }
            #endif
            output.printf_P(PSTR("Temperature %.1f%s"),
                temperatureRead(),
                SPGM(UTF8_degreeC)
            );
        #else
            #error Platform not supported
        #endif
    }
    else if (key == F("SOFTWARE")) {
        printVersion(output);
        if (System::Flags::getConfig().is_default_password) {
            output.printf_P(PSTR(HTML_S(br) HTML_S(strong) "%s" HTML_E(strong)), SPGM(default_password_warning));
        }
        auto cfg = System::Device::getConfig();
        if (cfg.config_magic) {
            output.printf_P(PSTR(HTML_S(br) "Last Factory Reset (Version %s-%08x): "), SaveCrash::Data::FirmwareVersion(cfg.config_version).toString().c_str(), cfg.config_magic);
            auto timestamp = cfg.getLastFactoryResetTimestamp();
            if (timestamp) {
                output.strftime_P(SPGM(strftime_date_time_zone), timestamp);
            }
            else {
                output.print(F("No time available"));
            }
        }
    }
    #if NTP_CLIENT || RTC_SUPPORT
        else if (key == F("TIME")) {
            auto now = time(nullptr);
            if (isTimeValid(now)) {
                printSystemTime(now, output);
            }
            else {
                output.print(F("No time available"));
            }
        }
    #endif
    #if HAVE_IOEXPANDER
        else if (key == F("IOEXPANDER_STATUS")) {
            IOExpander::config.printStatus<true>(output);
        }
    #endif
    else if (key == F("RTC_STATUS")) {
        config.printRTCStatus(output, false);
    }
    else if (key == F("SAFEMODE")) {
        if (config.isSafeMode()) {
            output.print(F(" - Running in SAFE MODE"));
        }
    }
    else if (key == F("UPTIME")) {
        output.print(formatTime(getSystemUptime()));
    }
    else if (key == F("WIFI_UPTIME")) {
        auto wifiUp = KFCFWConfiguration::getWiFiUp();
        output.print(System::Flags::getConfig().is_station_mode_enabled ? (wifiUp == 0 ? FSPGM(Offline, "Offline") : formatTime(wifiUp / 1000)) : F("Client mode disabled"));
    }
    else if (key == F("IP_ADDRESS")) {
        WiFi_get_address(output);
    }
    else if (key == F("FILE_SYSTEM_INFO")) {
        WebTemplate::printFileSystemInfo(output);
    }
    else if (key == F("RANDOM")) {
        uint8_t buf[8];
        ESP.random(buf, sizeof(buf));
        for(auto n: buf) {
            n %= 36;
            output.print((char)(n < 26 ? (n + 'a') : (n + ('0' - 26))));
        }
    }
    else if (key == F("IS_CONFIG_DIRTY")) {
        if (!config.isConfigDirty()) {
            output.print(FSPGM(_hidden));
        }
    }
    else if (key == F("IS_CONFIG_DIRTY_CLASS")) {
        if (config.isConfigDirty()) {
            output.print(F("alert alert-dismissible alert-danger fade show"));
        }
        else {
            output.print(FSPGM(hidden));
        }
    }
    else if (key == F("PIN_MONITOR_STATUS")) {
    #if PIN_MONITOR
            PinMonitor::pinMonitor.printStatus(output);
    #else
            output.print(F("Pin monitor disabled"));
    #endif
    }
    #if IOT_ALARM_PLUGIN_ENABLED
        else if (key.startsWith(F("ALARM_TIMESTAMP_"))) {
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
    else if (key.endsWith(F("_STATUS"))) {
        uint8_t cmp_length = key.length() - 7;
        for(auto plugin: PluginComponents::Register::getPlugins()) {
            if (plugin->hasStatus() && strncasecmp_P(key.c_str(), plugin->getName_P(), cmp_length) == 0) {
                plugin->getStatus(output);
                return;
            }
        }
        output.print(FSPGM(Not_supported, "Not supported"));
    }
    else {
        __DBG_printf("strlist check key='%s'", key.c_str());
        if (stringlist_find_P_P(PSTR("PCF8574_STATUS,PCF8575_STATUS,TINYPWM_STATUS,PCA9685_STATUS,MCP23017_STATUS,RTC_STATUS"), key.c_str(), ',') != -1) {
            // strings that have not been replaced yet
            __DBG_printf("return strlist check key='%s'", key.c_str());
            return;
        }
        else {
            __DBG_printf("return assert failed check key='%s'", key.c_str());
            __DBG_assert_printf(F("key not found") == nullptr, "key not found '%s'", key.c_str());
        }
    }
}

void LoginTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (key == F("LOGIN_ERROR_MESSAGE")) {
        output.print(_errorMessage);
    }
    else if (key == F("LOGIN_ERROR_CLASS")) {
        if (_errorMessage.length() == 0) {
            output.print(FSPGM(_hidden));
        }
    }
    else if (key == F("LOGIN_KEEP_DAYS")) {
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

void MessageTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (key == F("TPL_MESSAGE")) {
        if (_containsHtml & kHtmlMessage) {
            auto mode = output.setMode(PrintHtmlEntities::Mode::RAW);
            output.print(_message);
            output.setMode(mode);
        }
        else {
            output.print(_message);
        }
    }
    else if (key == F("TPL_TITLE")) {
        if (_containsHtml & kHtmlTitle) {
            auto mode = output.setMode(PrintHtmlEntities::Mode::RAW);
            output.print(_title);
            output.setMode(mode);
        }
        else {
            output.print(_title);
        }
    }
    else if (key == F("TPL_TITLE_CLASS")) {
        if (_titleClass) {
            output.print(' ');
            output.print(_titleClass);
        }
        else {
            output.print(F(" text-white bg-primary"));
        }
    }
    else if (key == F("TPL_MESSAGE_CLASS")) {
        if (_messageClass) {
            output.print(' ');
            output.print(_messageClass);
        }
    }
    else {
        WebTemplate::process(key, output);
    }
}

void MessageTemplate::checkForHtml()
{
    auto pos = _title.indexOf('<');
    if (pos != -1) {
        if (_title.indexOf('>') > pos) {
            _containsHtml |= kHtmlTitle;
        }
    }
    pos = _message.indexOf('<');
    if (pos != -1) {
        if (_message.indexOf('>') > pos) {
            _containsHtml |= kHtmlMessage;
        }
    }
}

void NotFoundTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (key == F("STATUS_CODE")) {
        output.print(_code);
        return;
        // if (_code >= 200 && _code < 600) {
        //     output.print(_code);
        //     return;
        // }
        // return;
    }
    else if (key == F("TPL_TITLE")) {
        output.printf_P("Status Code: %u", _code);
        return;
        // if (_code >= 200 && _code < 600) {
        //     output.printf_P("Status Code: %u", _code);
        //     return;
        // }
        // return;
    }
    else if (_titleClass == nullptr && key == F("TPL_TITLE_CLASS")) {
        if (_code >= 400) {
        }
        else if (_code >= 300) {
            output.print(F(" text-white bg-info"));
            return;
        }
        else if (_code >= 200) {
            output.print(F(" text-white bg-success"));
            return;
        }
        output.print(F(" text-white bg-danger"));
        return;
    }
    return MessageTemplate::process(key, output);
}

void ConfigTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (!isAuthenticated()) {
        WebTemplate::process(key, output);
        return;
    }
    if (_form) {
        if (getForm()->process(key, output)) {
            return;
        }
    }

    if (key ==  F("NETWORK_MODE")) {
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
    else if (key.startsWith(F("MAX_CHANNELS"))) {
        output.print(config.getMaxWiFiChannels());
    }
    else if (key.startsWith(F("MODE_"))) {
        if (System::Flags::getConfig().getWifiMode() == key.substring(5).toInt()) {
            output.print(FSPGM(_selected, " selected"));
        }
    }
    else if (key == F("SSL_CERT")) {
    #if WEBSERVER_TLS_SUPPORT
            File file = KFCFS.open(FSPGM(server_crt, "/.pvt/server.crt"), fs::FileOpenMode::read);
            if (file) {
                output.print(file.readString());
            }
    #endif
    }
    else if (key == F("SSL_KEY")) {
    #if WEBSERVER_TLS_SUPPORT
            File file = KFCFS.open(FSPGM(server_key, "/.pvt/server.key"), fs::FileOpenMode::read);
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
    if (!isAuthenticated()) {
        WebTemplate::process(key, output);
        return;
    }
    if (key == F("GATEWAY")) {
        WiFi.gatewayIP().printTo(output);
    }
    else if (key == F("DNS")) {
        WiFi.dnsIP().printTo(output);
        if (WiFi.dnsIP(1)) {
            output.print(FSPGM(comma_));
            WiFi.dnsIP(1).printTo(output);
        }
    }
    else if (key == F("SSL_STATUS")) {
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
    else if (key == F("WIFI_MODE")) {
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
    else if (key == F("WIFI_SSID")) {
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
    else if (key == F("WIFI_STATUS")) {
        WiFi_get_status(output);
    }
    else {
        WebTemplate::process(key, output);
    }
}

void PasswordTemplate::process(const String &key, PrintHtmlEntitiesString &output)
{
    if (key == F("PASSWORD_ERROR_MESSAGE")) {
        output.print(_errorMessage);
    }
    else if (key == F("PASSWORD_ERROR_CLASS")) {
        if (_errorMessage.length() == 0) {
            output.print(FSPGM(_hidden));
        }
    }
    else {
        WebTemplate::process(key, output);
    }
}

SettingsForm::SettingsForm(AsyncWebServerRequest *request) : BaseForm(static_cast<FormUI::Form::Data *>(request)) //, _json(nullptr)
{
}


// void EmptyTemplate::process(const String &key, PrintHtmlEntitiesString &output) {
// }

#include <TemplateDataProvider.h>

class PluginStatusStream {
public:
    PluginStatusStream() : _iterator(PluginComponents::RegisterEx::getPlugins().begin()) {
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
        while (_iterator != PluginComponents::RegisterEx::getPlugins().end()) {
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
    PluginComponents::PluginsVector::iterator _iterator;
};


bool TemplateDataProvider::callback(const String& name, DataProviderInterface& provider, WebTemplate &webTemplate) {

    enum class FillBufferMethod {
        NONE = 0,
        PRINT_ARGS,
        BUFFER_STREAM
    };

    auto &printArgs = webTemplate.getPrintArgs();
    auto value = PrintHtmlEntitiesString();
    auto fbMethod = FillBufferMethod::NONE;

    // menus
    if (name == F("MENU_HTML_MAIN")) {
        bootstrapMenu.html(printArgs);
        fbMethod = FillBufferMethod::PRINT_ARGS;
    }
    else if (name.startsWith(F("MENU_HTML_MAIN_"))) {
        bootstrapMenu.html(printArgs, bootstrapMenu.findMenuByLabel(name.substring(15)));
        fbMethod = FillBufferMethod::PRINT_ARGS;
    }
    else if (name.startsWith(F("MENU_HTML_SUBMENU_"))) {
        bootstrapMenu.htmlSubMenu(printArgs, bootstrapMenu.findMenuByLabel(name.substring(18)), 0);
        fbMethod = FillBufferMethod::PRINT_ARGS;
    }
    // forms
    else if (name == F("FORM_HTML")) {
        auto form = webTemplate.getForm();
        if (form) {
            form->createHtml(printArgs);
            fbMethod = FillBufferMethod::PRINT_ARGS;
        }
    }
    else if (name == F("FORM_VALIDATOR")) {
        auto form = webTemplate.getForm();
        if (form) {
            form->createJavascript(printArgs);
            fbMethod = FillBufferMethod::PRINT_ARGS;
        }
    }
    // else if (name == F("FORM_JSON")) {
    //     auto json = webTemplate.getJson();
    //     if (json) {
    //         auto stream = std::shared_ptr<JsonBuffer>(new JsonBuffer(*json));
    //         provider.setFillBuffer([stream](uint8_t *buffer, size_t size) {
    //             return stream->fillBuffer(buffer, size);
    //         });
    //         return true;
    //     }
    // }
    // plugin status
    else if (name == F("PLUGIN_STATUS")) {
        auto stream = std::shared_ptr<PluginStatusStream>(new PluginStatusStream());
        if (!stream) {
            __DBG_printf_E("memory allocation failed");
            return false;
        }
        provider.setFillBuffer([stream](uint8_t *buffer, size_t size) {
            return stream->readBytes(buffer, size);
        });
        return true;
    }
    // other variables
    else {
        webTemplate.process(name, value);
        fbMethod = FillBufferMethod::BUFFER_STREAM;
    }

    // if (value.length() == 0) {
    //     // value.print(' ');
    //     __DBG_printf("empty output for %s", name.c_str());
    // }

    switch(fbMethod)  {
        case FillBufferMethod::PRINT_ARGS: {
                provider.setFillBuffer([&printArgs](uint8_t *buffer, size_t size) {
                    return printArgs.fillBuffer(buffer, size);
                });
                return true;
            }
        case FillBufferMethod::BUFFER_STREAM: {
                auto stream = std::shared_ptr<BufferStream>(new BufferStream(std::move(value)));
                if (!stream) {
                    __DBG_printf_E("memory allocation failed");
                    return false;
                }
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
