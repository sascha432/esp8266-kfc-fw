/**
 * Author: sascha_lammers@gmx.de
 */

#include "mdns_sd.h"
#include <PrintString.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "misc.h"
#include "plugins.h"

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(kfcmdns, "kfcmdns");
PROGMEM_STRING_DEF(tcp, "tcp");
PROGMEM_STRING_DEF(udp, "udp");
PROGMEM_STRING_DEF(https, "https");
PROGMEM_STRING_DEF(wss, "wss");
PROGMEM_STRING_DEF(ws, "ws");


void MDNS_query_service(const char *service, const char *proto, Stream *output)
{
#if defined(ESP32)
    String serviceStr = service;
    String protoStr = proto;
    Scheduler.1(10, false, [output, serviceStr, protoStr](EventScheduler::TimerPtr timer) {
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
    MDNSPlugin() : _running(false) {
        REGISTER_PLUGIN(this);
    }
    PGM_P getName() const {
        return PSTR("mdns");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("MDNS");
    }
    PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_MDNS;
    }
    void setup(PluginSetupMode_t mode) override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

#if AT_MODE_SUPPORTED
    bool hasAtMode() const override {
        return true;
    }
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void loop();
    void start(const IPAddress &address);
    void stop();

private:
    void _loop();

private:
    bool _running;
};

static MDNSPlugin plugin;

void MDNSPlugin::setup(PluginSetupMode_t mode)
{
    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & WIFI_STA) { // if station mode is active, wait for the interface to become online
        if (config.isWiFiUp() && WiFi.localIP().isSet()) { // already online
            start(WiFi.localIP());
        }
        else { // wait for event
            WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED, [this](uint8_t event, void *payload) {
                start(WiFi.localIP());
                LoopFunctions::callOnce([this]() {
                    WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, this);
                });
            }, this);
        }
    }
    else {
        start(INADDR_ANY);
    }
}

void MDNSPlugin::loop()
{
    plugin._loop();
}

void MDNSPlugin::start(const IPAddress &address)
{
    stop();

    _debug_printf_P(PSTR("hostname=%s address=%s\n"), config._H_STR(Config().device_name), address.toString().c_str());

    if (!MDNS.begin(config._H_STR(Config().device_name), address)) {
        _debug_println(F("MDNS failed"));
        return;
    }

#if WEBSERVER_SUPPORT
    auto httpPort = config._H_GET(Config().http_port);
#  if WEBSERVER_TLS_SUPPORT
    auto flags = config._H_GET(Config().flags);
    if (flags.webServerMode == HTTP_MODE_SECURE) {
        MDNS.addService(FSPGM(https), FSPGM(tcp), httpPort);
        MDNS.addService(FSPGM(wss), FSPGM(tcp), httpPort);
    } else
#  endif
    if (config._H_GET(Config().flags).webServerMode != HTTP_MODE_DISABLED) {
        MDNS.addService(FSPGM(http), FSPGM(tcp), httpPort);
        MDNS.addService(FSPGM(ws), FSPGM(tcp), httpPort);
    }
#endif

    if (MDNS.addService(FSPGM(kfcmdns), FSPGM(udp), 5353)) {
        MDNS.addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('v'), FIRMWARE_VERSION_STR);
    }
    else {
        _debug_printf_P(PSTR("failed to add kfcmdns\n"));
    }
    _running = true;
    LoopFunctions::add(loop);
}

void MDNSPlugin::stop()
{
    _debug_printf_P(PSTR("running=%u\n"), _running);
    if (_running) {
        MDNS.removeService(nullptr, String(FSPGM(kfcmdns)).c_str(), String(FSPGM(udp)).c_str());
        MDNS.removeServiceTxt(nullptr, String(FSPGM(kfcmdns)).c_str(), String(FSPGM(udp)).c_str(), String('v').c_str());

        MDNS.end();
        _running = false;
        LoopFunctions::remove(loop);
    }
}

void MDNSPlugin::_loop()
{
    MDNS.update();
}

void MDNSPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("Hostname '%s' "), config._H_STR(Config().device_name));
    if (!_running) {
        output.print(F("not "));
    }
    output.print(F("running"));
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNS, "MDNS", "<service>,<proto>", "Query MDNS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSR, "MDNSR", "<interface address>", "Restart MDNS");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(MDNSBSD, "MDNSBSD", "Broadcast service discovery on selected interfaces");

void MDNSPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNS), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSR), getName());
    // at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSBSD), getName());
}

bool MDNSPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNS))) {
        if (args.requireArgs(2, 2)) {
            MDNS_query_service(args.get(0), args.get(1), &args.getStream());
            args.print(F("Querying..."));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSR))) {
        IPAddress addr;
        addr.fromString(args.toString(0));
        args.printf_P(PSTR("MDNS on IP %s..."), addr.toString().c_str());
        plugin.start(addr);
        return true;
    }
    // else if (IsCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSBSD))) {
    //     serial.println(F("Not supported"));
    //     return true;
    // }
    return false;
}

#endif
