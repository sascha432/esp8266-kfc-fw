/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_SUPPORT

#include "mdns_sd.h"
#if defined(ESP8266)
#include <ESP8266mDNS.h>
#endif
#if defined(ESP32)
#include <ESPmDNS.h>
#endif
#include <PrintString.h>
#include <EventScheduler.h>
#include <WiFiCallbacks.h>
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "misc.h"
#include "plugins.h"

PROGMEM_STRING_DEF(kfcmdns, "kfcmdns");
PROGMEM_STRING_DEF(tcp, "tcp");
PROGMEM_STRING_DEF(udp, "udp");
PROGMEM_STRING_DEF(http, "http");
PROGMEM_STRING_DEF(https, "https");
PROGMEM_STRING_DEF(wss, "wss");
PROGMEM_STRING_DEF(ws, "ws");
PROGMEM_STRING_DEF(v, "v");

#if defined(ESP8266)

void MDNS_wifi_callback(uint8_t event, void *payload) {
    MDNS.notifyAPChange();
}

void MDNS_setup_wifi_event_callback() {
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::MODE_CHANGE, MDNS_wifi_callback);
}

#endif

void MDNS_query_service(const char *service, const char *proto, Stream *output) {
#if defined(ESP32)
    String serviceStr = service;
    String protoStr = proto;
    Scheduler.addTimer(10, false, [output, serviceStr, protoStr](EventScheduler::TimerPtr timer) {
        auto results = (uint8_t)MDNS.queryService(serviceStr, protoStr);
        if (results) {
            for(uint8_t i = 0; i < results; i++) {
                output->printf_P(PSTR("+MDNS: host=%s,ip=%s,port=%u,txts="), MDNS.hostname(i).c_str(), MDNS.IP(i).toString().c_str(), MDNS.port(i));
                auto numTxt = (uint8_t)MDNS.numTxt(i);
                for(uint8_t j = 0; j < numTxt; j++) {
                    if (j) {
                        output->print(',');
                    }
                    output->print(MDNS.txt(i, j));
                }
                output->println();
            }
        } else {
            output->println(F("+MDNS: Query did not return any results"));
        }
    });
#endif
}

class MDNSPlugin : public PluginComponent {
public:
    MDNSPlugin() {
        REGISTER_PLUGIN(this, "MDNSPlugin");
    }
    PGM_P getName() const;
    PluginPriorityEnum_t getSetupPriority() const override;
    void setup(PluginSetupMode_t mode) override;
    void restart() override {
        WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, MDNS_wifi_callback);
    }

    virtual bool hasStatus() const override;
    virtual void getStatus(Print &output) override;

#if AT_MODE_SUPPORTED
    bool hasAtMode() const override;
    void atModeHelpGenerator() override;
    bool atModeHandler(Stream &serial, const String &command, AtModeArgs &args) override;
#endif
};

static MDNSPlugin plugin;

PGM_P MDNSPlugin::getName() const {
    return PSTR("mdns");
}

MDNSPlugin::PluginPriorityEnum_t MDNSPlugin::getSetupPriority() const {
    return PRIO_MDNS;
}

void MDNSPlugin::setup(PluginSetupMode_t mode) {
    auto httpPort = config._H_GET(Config().http_port);

    MDNS.begin(config._H_STR(Config().device_name));
    debug_printf_P(PSTR("Setting MDNS hostname %s\n"), config._H_STR(Config().device_name));
#if WEBSERVER_SUPPORT
#  if WEBSERVER_TLS_SUPPORT
    auto flags = config._H_GET(Config().flags);
    if (flags.webServerMode == HTTP_MODE_SECURE) {
        MDNS.addService(FSPGM(https), FSPGM(tcp), httpPort);
#    if PING_MONITOR || HTTP2SERIAL
        MDNS.addService(FSPGM(wss), FSPGM(tcp), httpPort);
#    endif
    } else
#  endif
    if (config._H_GET(Config().flags).webServerMode != HTTP_MODE_DISABLED) {
        MDNS.addService(FSPGM(http), FSPGM(tcp), httpPort);
#  if PING_MONITOR || HTTP2SERIAL
        MDNS.addService(FSPGM(ws), FSPGM(tcp), httpPort);
#  endif
    }
#endif
    MDNS.addService(FSPGM(kfcmdns), FSPGM(udp), 5353);
    MDNS.addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), FSPGM(v), FIRMWARE_VERSION_STR);
#if defined(ESP8266)
    MDNS_setup_wifi_event_callback();
#endif
}

bool MDNSPlugin::hasStatus() const {
    return true;
}

void MDNSPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("Hostname '%s'"), config._H_STR(Config().device_name));
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNS, "MDNS", "<service>,<proto>", "Query MDNS");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(MDNSBSD, "MDNSBSD", "Broadcast service discovery on selected interfaces");

bool MDNSPlugin::hasAtMode() const {
    return true;
}

void MDNSPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNS), getName());
    // at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSBSD), getName());
}

bool MDNSPlugin::atModeHandler(Stream &serial, const String &command, AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNS))) {
        if (args.requireArgs(2, 2)) {
            MDNS_query_service(args.get(0), args.get(1), &serial);
            args.print(F("Querying..."));
        }
        return true;
    }
    // else if (String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(MDNSBSD))) {
    //     serial.println(F("Not supported"));
    //     return true;
    // }
    return false;
}

#endif

#endif
