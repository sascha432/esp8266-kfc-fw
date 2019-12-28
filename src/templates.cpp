/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "build.h"
#include "misc.h"
#include "timezone.h"
#include "web_server.h"
#include "status.h"
#include "reset_detector.h"
#include "plugins.h"

#if DEBUG_TEMPLATES
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if DEBUG_TEMPLATES
static const String &display_result(const String &key, const String &str) {
    debug_printf_P(PSTR("Macro '%s' = '%s'\n"), key.c_str(), str.c_str());
    return str;
}
#define _return(str)                        return display_result(key, str);
#else
#define _return(str)                        return str
#endif

PROGMEM_STRING_DEF(_hidden, " hidden");
PROGMEM_STRING_DEF(_selected, " selected");
PROGMEM_STRING_DEF(_checked, " checked");
PROGMEM_STRING_DEF(Not_supported, "Not supported");

WebTemplate::WebTemplate() {
    _form = nullptr;
}

WebTemplate::~WebTemplate() {
    if (_form) {
        delete _form;
    }
}

void WebTemplate::setForm(Form * form) {
    _form = form;
}

Form * WebTemplate::getForm() {
    return _form;
}

void WebTemplate::process(const String &key, PrintHtmlEntities &output)
{
    if (key.equals(F("HOSTNAME"))) {
        output.print(config._H_STR(Config().device_name));
    }
    else if (key == F("HARDWARE")) {
#if defined(ESP8266)
        output.printf_P(PSTR("ESP8266 %s Flash, %d Mhz, Free RAM %s"), formatBytes(ESP.getFlashChipRealSize()).c_str(), system_get_cpu_freq(), formatBytes(ESP.getFreeHeap()).c_str());
#elif defined(ESP32)
        output.printf_P(PSTR("ESP32 %s Flash, %d Mhz, Free RAM %s"), formatBytes(ESP.getFlashChipSize()).c_str(), ESP.getCpuFreqMHz(), formatBytes(ESP.getFreeHeap()).c_str());
#else
#error Platform not supported
#endif
    }
    else if (key == F("SOFTWARE")) {
        output.print(F("KFC FW "));
        output.print(config.getFirmwareVersion());
        if (config._H_GET(Config().flags).isDefaultPassword) {
            output.printf_P(PSTR(HTML_S(br) HTML_S(strong) "%s" HTML_E(strong)), SPGM(default_password_warning));
        }
#  if NTP_CLIENT || RTC_SUPPORT
    }
    else if (key == F("TIME")) {

        time_t now = time(nullptr);
        if (!IS_TIME_VALID(now)) {
            output.print(F("No time available"));
        } else {
            char buf[32];
            timezone_strftime_P(buf, sizeof(buf), PSTR("%a, %d %b %Y %H:%M:%S %Z"), timezone_localtime(&now));
            output.print(buf);
        }
#  endif
    }
    else if (key == F("SAFEMODE")) {
        if (config.isSafeMode()) {
            output.print(F(" - Running in SAFE MODE"));
        }
    }
    else if (key == F("UPTIME")) {
        output.print(formatTime((long)(millis() / 1000)));
    }
    else if (key == F("WIFI_UPTIME")) {
        output.print(config._H_GET(Config().flags).wifiMode & WIFI_STA ? (!KFCFWConfiguration::isWiFiUp() ? F("Offline") : formatTime((millis() - KFCFWConfiguration::getWiFiUp()) / 1000)) : F("Client mode disabled"));
    }
    else if (key == F("IP_ADDRESS")) {
        WiFi_get_address(output);
    }
    else if (key == F("FIRMWARE_UPGRADE_FAILURE")) {
    }
    else if (key == F("FIRMWARE_UPGRADE_FAILURE_CLASS")) {
        output.print(FSPGM(_hidden));
    }
    else if (key == F("CONFIG_DIRTY_CLASS")) {
        if (!config.isConfigDirty()) {
            output.print(FSPGM(_hidden));
        }
    }
    else if (key.equals(F("MENU_HTML_MAIN"))) {
        output.setRawOutput(true);
        bootstrapMenu.html(output);
    }
    else if (key.startsWith(F("MENU_HTML_SUBMENU_"))) {
        output.setRawOutput(true);
        auto id = bootstrapMenu.findMenuByLabel(key.substring(18));
        if (id != BootstrapMenu::INVALID_ID) {
            bootstrapMenu.htmlSubMenu(output, id, 0);
        }
    }
    else if (key == F("FORM_HTML")) {
        if (_form) {
            output.setRawOutput(true);
            _form->createHtml(output);
        }
    }
    else if (key == F("FORM_VALIDATOR")) {
        if (_form) {
            output.setRawOutput(true);
            _form->createJavascript(output);
        }
    }
    else if (_form) {
        const char *str = _form->process(key);
        if (str) {
            output.print(str);
        }
    }
    else if (key.endsWith(F("_STATUS"))) {
        uint8_t cmp_length = key.length() - 7;
        for(auto plugin: plugins) {
            if (plugin->hasStatus() && strncasecmp_P(key.c_str(), plugin->getName(), cmp_length) == 0) {
                plugin->getStatus(output);
                return;
            }
        }
        output.print(FSPGM(Not_supported));
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

void UpgradeTemplate::process(const String &key, PrintHtmlEntities &output)
{
    if (key == F("FIRMWARE_UPGRADE_FAILURE_CLASS")) {
    }
    else if (key == F("FIRMWARE_UPGRADE_FAILURE")) {
        output.print(_errorMessage);
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

LoginTemplate::LoginTemplate(const String &errorMessage) {
    _errorMessage = errorMessage;
}

void LoginTemplate::process(const String &key, PrintHtmlEntities &output)
{
    if (key == F("LOGIN_ERROR_MESSAGE")) {
        output.print(_errorMessage);
    }
    else if (key == F("LOGIN_ERROR_CLASS")) {
        if (_errorMessage.length() == 0) {
            output.print(FSPGM(_hidden));
        }
    }
    else {
        WebTemplate::process(key, output);
    }
}

void LoginTemplate::setErrorMessage(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

void ConfigTemplate::process(const String &key, PrintHtmlEntities &output)
{
    if (_form) {
        const char *str = getForm()->process(key);
        if (str) {
            output.print(str);
            return;
        }
        SettingsForm *form = static_cast<SettingsForm *>(getForm());
        auto &tokens = form->getTokens();
        if (tokens.size()) {
            for(const auto &token: tokens) {
                if (token.first.equals(key)) {
                    output.print(token.second);
                    return;
                }
            }
        }
    }
    if (key == F("NETWORK_MODE")) {
        if (config._H_GET(Config().flags).wifiMode & WIFI_AP) {
            output.print(F("#station_mode"));
        }
        if (config._H_GET(Config().flags).wifiMode & WIFI_STA) {
            output.print(F("#ap_mode"));
        }
    }
    else if (key.startsWith(F("MAX_CHANNELS"))) {
        output.print(config.getMaxWiFiChannels());
    }
    else if (key.startsWith(F("MODE_"))) {
        if (config._H_GET(Config().flags).wifiMode == key.substring(5).toInt()) {
            output.print(FSPGM(_selected));
        }
    }
    else if (key.startsWith(F("LED_TYPE_"))) {
        uint8_t type = key.substring(9).toInt();
        if (config._H_GET(Config().flags).ledMode == type) {
            output.print(FSPGM(_selected));
        }
    }
    else if (key == F("LED_PIN")) {
        output.print(config._H_GET(Config().led_pin));
    }
    else if (key == F("SSL_CERT")) {
#if SPIFFS_SUPPORT
        File file = SPIFFS.open(FSPGM(server_crt), "r");
        if (file) {
            output.print(file.readString());
        }
#endif
    }
    else if (key == F("SSL_KEY")) {
#if SPIFFS_SUPPORT
        File file = SPIFFS.open(FSPGM(server_key), "r");
        if (file) {
            output.print(file.readString());
        }
#endif
    }
    else {
        WebTemplate::process(key, output);
    }
}

void StatusTemplate::process(const String &key, PrintHtmlEntities &output)
{
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
                output.printf_P(PSTR("TLS enabled, HTTPS %s"), _Config.getOptions().isHttpServerTLS() ? F("enabled") : FSPGM(Disabled));
            #else
                output.printf_P(PSTR("TLS enabled, HTTPS not supported"));
            #endif
        #else
            output.print(FSPGM(Not_supported));
        #endif
    }
    else if (key == F("WIFI_MODE")) {
        switch (WiFi.getMode()) {
            case WIFI_STA:
                output.print(F("Station mode"));
                break;
            case WIFI_AP:
                output.print(F("Access Point"));
                break;
            case WIFI_AP_STA:
                output.print(F("Access Point and Station Mode"));
                break;
            case WIFI_OFF:
                output.print(F("Off"));
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
            output.print(F(HTML_E(strong) HTML_S(br) "Access Point " HTML_S(strong)));
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

PasswordTemplate::PasswordTemplate(const String &errorMessage)
{
    _errorMessage = errorMessage;
}

void PasswordTemplate::process(const String &key, PrintHtmlEntities &output)
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

WifiSettingsForm::WifiSettingsForm(AsyncWebServerRequest *request) : SettingsForm(request) {

    add<uint8_t>(F("mode"), _H_STRUCT_FORMVALUE(Config().flags, uint8_t, wifiMode));
    addValidator(new FormRangeValidator(F("Invalid mode"), WIFI_OFF, WIFI_AP_STA));

    add<sizeof Config().wifi_ssid>(F("wifi_ssid"), config._H_W_STR(Config().wifi_ssid));
    addValidator(new FormLengthValidator(1, sizeof(Config().wifi_ssid) - 1));

    add<sizeof Config().wifi_pass>(F("wifi_password"), config._H_W_STR(Config().wifi_pass));
    addValidator(new FormLengthValidator(8, sizeof(Config().wifi_pass) - 1));

    add<sizeof Config().soft_ap.wifi_ssid>(F("ap_wifi_ssid"), config._H_W_STR(Config().soft_ap.wifi_ssid));
    addValidator(new FormLengthValidator(1, sizeof(Config().soft_ap.wifi_ssid) - 1));

    add<sizeof Config().soft_ap.wifi_pass>(F("ap_wifi_password"), config._H_W_STR(Config().soft_ap.wifi_pass));
    addValidator(new FormLengthValidator(8, sizeof(Config().soft_ap.wifi_pass) - 1));

    add<uint8_t>(F("channel"), &config._H_W_GET(Config().soft_ap.channel));
    addValidator(new FormRangeValidator(1, config.getMaxWiFiChannels()));

    add<uint8_t>(F("encryption"), &config._H_W_GET(Config().soft_ap.encryption));
#if defined(ESP32)
    addValidator(new FormEnumValidator<uint8_t, 6>(F("Invalid encryption"), array_of<uint8_t>(WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE)));
#elif defined(ESP8266)
    addValidator(new FormEnumValidator<uint8_t, 5>(F("Invalid encryption"), array_of<uint8_t>(ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO)));
#else
#error Platform not supported
#endif


    add<bool>(F("ap_hidden"), _H_STRUCT_FORMVALUE(Config().flags, bool, hiddenSSID), FormField::INPUT_CHECK);

    finalize();
}

NetworkSettingsForm::NetworkSettingsForm(AsyncWebServerRequest *request) : SettingsForm(request) {

    add<sizeof Config().device_name>(F("hostname"), config.getWriteableString(_H(Config().device_name), sizeof Config().device_name));

    add<bool>(F("dhcp_client"), _H_STRUCT_FORMVALUE(Config().flags, bool, stationModeDHCPEnabled));

    add(new FormObject<IPAddress>(F("ip_address"), _H_IP_FORM_OBJECT(Config().local_ip)));
    add(new FormObject<IPAddress>(F("subnet"), _H_IP_FORM_OBJECT(Config().subnet)));
    add(new FormObject<IPAddress>(F("gateway"), _H_IP_FORM_OBJECT(Config().gateway)));
    add(new FormObject<IPAddress>(F("dns1"), _H_IP_FORM_OBJECT(Config().dns1)));
    add(new FormObject<IPAddress>(F("dns2"), _H_IP_FORM_OBJECT(Config().dns2)));

    add<bool>(F("softap_dhcpd"), _H_STRUCT_FORMVALUE(Config().flags, bool, softAPDHCPDEnabled));

    add(new FormObject<IPAddress>(F("dhcp_start"), _H_IP_FORM_OBJECT(Config().soft_ap.dhcp_start)));
    add(new FormObject<IPAddress>(F("dhcp_end"), _H_IP_FORM_OBJECT(Config().soft_ap.dhcp_end)));
    add(new FormObject<IPAddress>(F("ap_ip_address"), _H_IP_FORM_OBJECT(Config().soft_ap.address)));
    add(new FormObject<IPAddress>(F("ap_subnet"), _H_IP_FORM_OBJECT(Config().soft_ap.subnet)));

    finalize();
}

PasswordSettingsForm::PasswordSettingsForm(AsyncWebServerRequest *request) : SettingsForm(request) {

    add(new FormField(F("password"), config._H_STR(Config().device_pass)));
    addValidator(new FormMatchValidator(F("The entered password is not correct"), [](FormField &field) {
        return field.getValue().equals(config._H_STR(Config().device_pass));
    }));

    add<sizeof Config().device_pass>(F("password2"), config._H_W_STR(Config().device_pass))
        ->setValue(_sharedEmptyString);

    addValidator(new FormRangeValidator(F("The password has to be at least %min% characters long"), 6, 0));
    addValidator(new FormRangeValidator(6, sizeof(Config().device_pass) - 1))
        ->setValidateIfValid(false);

    add(F("password3"), _sharedEmptyString);
    addValidator(new FormMatchValidator(F("The password confirmation does not match"), [](FormField &field) {
            return field.equals(field.getForm().getField(F("password2")));
        }))
        ->setValidateIfValid(false);

    finalize();
}

SettingsForm::SettingsForm(AsyncWebServerRequest * request) : Form(&_data) {
    _data.setCallbacks(
        [request](const String &name) {
            if (!request) {
                return String();
            }
            return request->arg(name);
        },
        [request](const String &name) {
        if (!request) {
            return false;
        }
        return request->hasArg(name.c_str());
    });
}

File2String::File2String(const String & filename) {
    _filename = filename;
}

String File2String::toString() {
    return SPIFFS.open(_filename, "r").readString();
}

void File2String::fromString(const String & value) {
    SPIFFS.open(_filename, "w").write((const uint8_t *)value.c_str(), value.length());
}

void EmptyTemplate::process(const String & key, PrintHtmlEntities &output) {
}
