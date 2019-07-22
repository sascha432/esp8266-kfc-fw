/**
 * Author: sascha_lammers@gmx.de
 */

#include "templates.h"
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

#if DEBUG_TEMPLATE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if false && DEBUG
static String display_result(const char *file, int line, const char *func, const String &key, String str) {
    DEBUG_OUTPUT.printf(_debugPrefix.c_str(), basename(file), line, func);
    DEBUG_OUTPUT.printf_P(PSTR("Macro %s = %s\n"), key.c_str(), str.c_str());
    return str;
}
#define _return(str)                        return display_result(__FILE__, __LINE__, __FUNCTION__, key, str);
#define debug_display_return(key, result)   display_result(__FILE__, __LINE__, __FUNCTION__, key, result)
#else
#define _return(str)                       return str
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

String WebTemplate::process(const String &key) {
    if (key.equals(F("HOSTNAME"))) {
        PrintHtmlEntitiesString out(config._H_STR(Config().device_name));
        _return(out);
#if ESP8266
    } else if (key == F("HARDWARE")) {
        PrintHtmlEntitiesString out;
        out.printf_P(PSTR("ESP8266 %s Flash, %d Mhz, Free RAM %s"), formatBytes(ESP.getFlashChipRealSize()).c_str(), system_get_cpu_freq(), formatBytes(ESP.getFreeHeap()).c_str());
        _return(out);
#else
#error Platform not supported
#endif
    } else if (key == F("SOFTWARE")) {
        PrintHtmlEntitiesString out = F("KFC FW ");
        out += config.getFirmwareVersion();
        if (config._H_GET(Config().flags).isDefaultPassword) {
            out.printf_P(PSTR(HTML_S(br) HTML_S(strong) "%s" HTML_E(strong)), SPGM(default_password_warning));
        }
        _return(out);
#  if NTP_CLIENT || RTC_SUPPORT
    } else if (key == F("TIME")) {
        time_t now = time(nullptr);
        if (IS_TIME_VALID(now)) {
            _return(F("No time available"));
        } else {
            char buf[32];
            timezone_strftime_P(buf, sizeof(buf), PSTR("%a, %d %b %Y %H:%M:%S %Z"), timezone_localtime(&now));
            PrintHtmlEntitiesString out(buf);
            _return(out);
        }
#  endif
    } else if (key == F("SAFEMODE")) {
        if (resetDetector.getSafeMode()) {
            _return(F(" - Running in SAFE MODE"));
        }
        _return(_sharedEmptyString);
    } else if (key == F("UPTIME")) {
        _return(formatTime((long)(millis() / 1000)));
    } else if (key == F("WIFI_UPTIME")) {
        _return(config._H_GET(Config().flags).wifiMode & WIFI_STA ? (!KFCFWConfiguration::isWiFiUp() ? F("Offline") : formatTime((millis() - KFCFWConfiguration::getWiFiUp()) / 1000)) : F("Client mode disabled"));
    } else if (key == F("IP_ADDRESS")) {
        PrintHtmlEntitiesString out;
        WiFi_get_address(out);
        _return(out);
    } else if (key == F("FIRMWARE_UPGRADE_FAILURE")) {
        return _sharedEmptyString;
    } else if (key == F("FIRMWARE_UPGRADE_FAILURE_CLASS")) {
        return SPGM(_hidden);
    } else if (key == F("CONFIG_DIRTY_CLASS")) {
        _return(config.isConfigDirty() ? _sharedEmptyString : SPGM(_hidden));
    } else if (key == F("FORM_VALIDATOR")) {
        if (_form) {
            PrintString response;
            _form->createJavascript(response);
            return response;
        }
        return _sharedEmptyString;
    } else if (_form) {
        const char *str = _form->process(key);
        if (str) {
            _return(str);
        }
    } else if (key.endsWith(F("_STATUS"))) {
        uint8_t cmp_length = key.length() - 7;
        for(auto &plugin: plugins) {
            auto callback = plugin.getStatusTemplate();
            if (callback && strncasecmp_P(key.c_str(), plugin.getPluginNamePSTR(), cmp_length) == 0) {
                return callback();
            }
        }
        _return(SPGM(Not_supported));
    }

    PrintHtmlEntitiesString out;
    out.print(F("KEY NOT FOUND: %"));
    out.print(key);
    out.print('%');
#if DEBUG
    if (out.length() > 15) {
        debug_printf_P(PSTR("Template macro %s not defined\n"), out.c_str() + 15);
    }
#endif
    _return(out);
}

UpgradeTemplate::UpgradeTemplate() {
}

UpgradeTemplate::UpgradeTemplate(const String &errorMessage) {
    _errorMessage = errorMessage;
}

String UpgradeTemplate::process(const String &key) {
    if (key == F("FIRMWARE_UPGRADE_FAILURE_CLASS")) {
        _return(_sharedEmptyString);
    } else if (key == F("FIRMWARE_UPGRADE_FAILURE")) {
        _return(_errorMessage);
    }
    _return(WebTemplate::process(key));
}

void UpgradeTemplate::setErrorMessage(const String &errorMessage) {
    _errorMessage = errorMessage;
}

LoginTemplate::LoginTemplate() {
}

LoginTemplate::LoginTemplate(const String &errorMessage) {
    _errorMessage = errorMessage;
}

String LoginTemplate::process(const String &key) {
    if (key == F("LOGIN_ERROR_MESSAGE")) {
        _return(_errorMessage);
    } else if (key == F("LOGIN_ERROR_CLASS")) {
        _return((_errorMessage.length() != 0) ? _sharedEmptyString : SPGM(_hidden));
    }
    _return(WebTemplate::process(key));
}

void LoginTemplate::setErrorMessage(const String &errorMessage) {
    _errorMessage = errorMessage;
}

String ConfigTemplate::process(const String &key) {
    if (_form) {
        const char *str = getForm()->process(key);
        if (str) {
            _return(str);
        }
    }
    if (key == F("NETWORK_MODE")) {
        String out;
        if (config._H_GET(Config().flags).wifiMode & WIFI_AP) {
            out = F("#station_mode");
        }
        if (config._H_GET(Config().flags).wifiMode & WIFI_STA) {
            if (out.length()) {
                out += FSPGM(comma_);
            }
            out += F("#ap_mode");
        }
        _return(out);
    } else if (key.startsWith(F("MAX_CHANNELS"))) {
        _return(String(config.getMaxWiFiChannels()));
    } else if (key.startsWith(F("MODE_"))) {
        _return((config._H_GET(Config().flags).wifiMode == key.substring(5).toInt()) ? SPGM(_selected) : _sharedEmptyString);
    } else if (key.startsWith(F("LED_TYPE_"))) {
        uint8_t type = key.substring(9).toInt();
        _return(config._H_GET(Config().flags).ledMode == type ? SPGM(_selected) : _sharedEmptyString);
    } else if (key == F("LED_PIN")) {
        _return(String(config._H_GET(Config().led_pin)));
    } else if (key == F("SSL_CERT")) {
#if SPIFFS_SUPPORT
        File file = SPIFFS.open(FSPGM(server_crt), "r");
        if (file) {
            _return(file.readString());
        }
#endif
        _return(_sharedEmptyString);
    } else if (key == F("SSL_KEY")) {
#if SPIFFS_SUPPORT
        File file = SPIFFS.open(SPGM(server_key), "r");
        if (file) {
            _return(file.readString());
        }
#endif
        _return(_sharedEmptyString);
    }
    _return(WebTemplate::process(key));
}

String StatusTemplate::process(const String &key) {
    if (key == F("GATEWAY")) {
        _return(WiFi.gatewayIP().toString());
    } else if (key == F("DNS")) {
        PrintHtmlEntitiesString out;
        out.print(WiFi.dnsIP());
        if (WiFi.dnsIP(1)) {
            out.print(FSPGM(comma_));
            out.print(WiFi.dnsIP(1));
        }
        _return(out);
    } else if (key == F("SSL_STATUS")) {
        #if ASYNC_TCP_SSL_ENABLED
            PrintHtmlEntitiesString out;
            #if WEBSERVER_TLS_SUPPORT
                out.printf_P(PSTR("TLS enabled, HTTPS %s"), _Config.getOptions().isHttpServerTLS() ? PSTR("enabled") : SPGM(Disabled));
            #else
                out.printf_P(PSTR("TLS enabled, HTTPS not supported"));
            #endif
                _return(out);
        #else
            _return(SPGM(Not_supported));
        #endif
    } else if (key == F("WIFI_MODE")) {
        int n;
        switch (n = WiFi.getMode()) {
            case WIFI_STA:
                _return(F("Station mode"));
            case WIFI_AP:
                _return(F("Access Point"));
            case WIFI_AP_STA:
                _return(F("Access Point and Station Mode"));
            case WIFI_OFF:
                _return(F("Off"));
            default:
                PrintHtmlEntitiesString out;
                out.printf_P(PSTR("Invalid mode %d"), n);
                _return(out);
        }
    } else if (key == F("WIFI_SSID")) {
        PrintHtmlEntitiesString out;
        if (WiFi.getMode() == WIFI_AP_STA) {
            out.print(F("Station connected to " HTML_S(strong)));
            WiFi_Station_SSID(out);
            out.print(F(HTML_E(strong) HTML_S(br) "Access Point " HTML_S(strong)));
            WiFi_SoftAP_SSID(out);
            out.print(F(HTML_E(strong)));
        } else if (WiFi.getMode() & WIFI_STA) {
            WiFi_Station_SSID(out);
        } else if (WiFi.getMode() & WIFI_AP) {
            WiFi_SoftAP_SSID(out);
        }
        _return(out);
    } else if (key == F("WIFI_HOSTNAME")) {
        PrintHtmlEntitiesString out;
        out.print(wifi_station_get_hostname());
        _return(out);
    } else if (key == F("WIFI_STATUS")) {
        PrintHtmlEntitiesString out;
        WiFi_get_status(out);
        _return(out);
    }
    _return(WebTemplate::process(key));
}

PasswordTemplate::PasswordTemplate(const String &errorMessage) {
    _errorMessage = errorMessage;
}

String PasswordTemplate::process(const String &key) {
    if (key == F("PASSWORD_ERROR_MESSAGE")) {
        _return(PrintHtmlEntitiesString(_errorMessage));
    } else if (key == F("PASSWORD_ERROR_CLASS")) {
        _return((_errorMessage.length() != 0) ? _sharedEmptyString : SPGM(_hidden));
    }
    _return(WebTemplate::process(key));
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
    addValidator(new FormEnumValidator<uint8_t, 5>(F("Invalid encryption"), array_of<uint8_t>(ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO)));

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
    addValidator(new FormMatchValidator(F("The entered password is not correct"), [&](FormField *field) {
        return field->getValue().equals(config._H_STR(Config().device_pass));
    }));

    add<sizeof Config().device_pass>(F("password2"), config._H_W_STR(Config().device_pass))
        ->setValue(_sharedEmptyString);

    addValidator(new FormRangeValidator(F("The password has to be at least %min% characters long"), 6, 0));
    addValidator(new FormRangeValidator(6, sizeof(Config().device_pass) - 1))
        ->setValidateIfValid(false);

    add(F("password3"), _sharedEmptyString);
    addValidator(new FormMatchValidator(F("The password confirmation does not match"), [](FormField *field) {
        return field->equals(field->getForm()->getField(F("password2")));
    }))
        ->setValidateIfValid(false);

    finalize();
}

SettingsForm::SettingsForm(AsyncWebServerRequest * request) : Form(&_data) {
    _data.setCallbacks([request](const String &name) {
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

String EmptyTemplate::process(const String & key) {
    return String();
}
