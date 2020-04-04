/**
 * Author: sascha_lammers@gmx.de
 */

#include "mdns_sd.h"
#include <PrintString.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include <misc.h>
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "misc.h"

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if MDNS_PLUGIN

bool MDNSService::addService(const String &service, const String &proto, uint16_t port)
{
    _debug_printf_P(PSTR("service=%s proto=%s port=%u\n"), service.c_str(), proto.c_str(), port);
#if ESP8266
    return _debug_print_result(MDNS.addService(service, proto, port));
#else
    MDNS.addService(service, proto, port);
    return true;;
#endif
}

bool MDNSService::addServiceTxt(const String &service, const String &proto, const String &key, const String &value)
{
    _debug_printf_P(PSTR("service=%s proto=%s key=%s value=%s\n"), service.c_str(), proto.c_str(), key.c_str(), value.c_str());
#if ESP8266
    return _debug_print_result(MDNS.addServiceTxt(service, proto, key, value));
#else
    MDNS.addServiceTxt(service, proto, key, value);
    return true;
#endif
}

bool MDNSService::removeService(const String &service, const String &proto)
{
    _debug_printf_P(PSTR("service=%s proto=%s\n"), service.c_str(), proto.c_str());
#if ESP8266
    return _debug_print_result(MDNS.removeService(nullptr, service.c_str(), proto.c_str()));
#else
    return true;
#endif
}

bool MDNSService::removeServiceTxt(const String &service, const String &proto, const String &key)
{
    _debug_printf_P(PSTR("service=%s proto=%s key=%s\n"), service.c_str(), proto.c_str(), key.c_str());
#if ESP8266
    return _debug_print_result(MDNS.removeServiceTxt(nullptr, service.c_str(), proto.c_str(), key.c_str()));
#else
    return true;
#endif
}

void MDNSService::announce()
{
    _debug_println();
#if ESP8266
    MDNS.announce();
#endif
}

PROGMEM_STRING_DEF(kfcmdns, "kfcmdns");

static MDNSPlugin plugin;

void MDNSPlugin::setup(PluginSetupMode_t mode)
{
    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & WIFI_STA) {
#if ESP8266
        if (config.isWiFiUp() && WiFi.localIP().isSet()) { // already online
            start(WiFi.localIP());
        }
        else {
            start(INADDR_ANY);
            WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED, [this](uint8_t event, void *payload) {
                _debug_printf_P(PSTR("MDNS.begin interface=%s\n"), WiFi.localIP().toString().c_str());
                MDNS.end();
                MDNS.begin(config.getDeviceName(), WiFi.localIP());
                MDNS.announce();
                WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, this);
            }, this);
        }
#else
        if (config.isWiFiUp() && WiFi.localIP() != INADDR_NONE) { // already online
            start(WiFi.localIP());
        }
        else {
            start(INADDR_NONE);
            WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED, [this](uint8_t event, void *payload) {
                _debug_printf_P(PSTR("MDNS.begin\n"));
                MDNS.end();
                MDNS.begin(config.getDeviceName());
                WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, this);
            }, this);
        }
#endif
    }
}

void MDNSPlugin::loop()
{
    plugin._loop();
}

void MDNSPlugin::start(const IPAddress &address)
{
    stop();

    _debug_printf_P(PSTR("hostname=%s address=%s\n"), config.getDeviceName(), address.toString().c_str());

#if ESP8266
    if (!MDNS.begin(config.getDeviceName(), address)) {
        _debug_println(F("MDNS failed"));
        return;
    }
#else
    if (!MDNS.begin(config.getDeviceName())) {
        _debug_println(F("MDNS failed"));
        return;
    }
#endif

    if (MDNSService::addService(FSPGM(kfcmdns), FSPGM(udp), 5353)) {
        MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('v'), FIRMWARE_VERSION_STR);
    }
    _running = true;
    LoopFunctions::add(loop);
}

void MDNSPlugin::stop()
{
    _debug_printf_P(PSTR("running=%u\n"), _running);
    if (_running) {
        MDNSService::removeService(FSPGM(kfcmdns), FSPGM(udp));
        MDNSService::announce();

        MDNS.end();
        _running = false;
        LoopFunctions::remove(loop);
    }
}

void MDNSPlugin::_loop()
{
#if ESP8266
    MDNS.update();
#endif
}

void MDNSPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("Service for hostname %s "), config.getDeviceName());
    if (!_running) {
        output.print(F("not "));
    }
    output.print(F("running"));
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSQ, "MDNSQ", "<service>,<proto>,[<wait=2000ms>]", "Query MDNS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSR, "MDNSR", "<interface address>", "Restart MDNS and run on selected interface");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(MDNSBSD, "MDNSBSD", "Broadcast service discovery on selected interfaces");

void MDNSPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSQ), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSR), getName());
    // at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSBSD), getName());
}

#if ESP8266

void MDNSPlugin::serviceCallback(MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
{
    _debug_printf_P(PSTR("answerType=%u p_bSetContent=%u\n"), answerType, p_bSetContent)
    PrintString str;

    auto iterator = std::find_if(_services.begin(), _services.end(), [&mdnsServiceInfo](const ServiceInfo &info) {
        return info.serviceDomain.equals(mdnsServiceInfo.serviceDomain());
    });
    ServiceInfo *infoPtr;
    if (iterator != _services.end()) {
        infoPtr = &(*iterator);
    }
    else {
        _services.emplace_back(mdnsServiceInfo.serviceDomain());
        infoPtr = &_services.back();
    }
    auto &info = *infoPtr;
    if (mdnsServiceInfo.hostDomainAvailable()) {
        info.domain = mdnsServiceInfo.hostDomain();
    }
    if (mdnsServiceInfo.IP4AddressAvailable()) {
        info.addresses = mdnsServiceInfo.IP4Adresses();
    }
    if (mdnsServiceInfo.hostPortAvailable()) {
        info.port = mdnsServiceInfo.hostPort();
    }
    if (mdnsServiceInfo.txtAvailable()) {
        info.txts = mdnsServiceInfo.strKeyValue();
    }
}

#endif

bool MDNSPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSQ))) {
        if (!_running) {
            args.print(F("MDNS service is not running"));
        }
        else if (args.requireArgs(2, 3)) {
#if ESP8266
            auto timeout = args.toMillis(2, 100, ~0, 2000);
            auto query = PrintString(F("service=%s proto=%s wait=%ums"), args.toString(0).c_str(), args.toString(1).c_str(), timeout);
            args.printf_P(PSTR("Querying: %s"), query.c_str());

            auto serviceQuery = MDNS.installServiceQuery(args.toString(0).c_str(), args.toString(1).c_str(), [this](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                serviceCallback(mdnsServiceInfo, answerType, p_bSetContent);
            });

            Scheduler.addTimer(timeout, false, [args, serviceQuery, this](EventScheduler::TimerPtr) mutable {
                _IF_DEBUG(auto result = ) MDNS.removeServiceQuery(serviceQuery);
                _debug_printf_P(PSTR("removeServiceQuery=%u\n"), result);
                if (_services.empty()) {
                    args.print(F("No response"));
                }
                else {
                    for(auto &svc: _services) {
                        args.printf_P(PSTR("domain=%s port=%u ips=%s txts=%s"), svc.domain.c_str(), svc.port, implode_cb(',', svc.addresses, [](const IPAddress &addr) {
                            return addr.toString();
                        }).c_str(), svc.txts.c_str());
                    }
                }
                _services.clear();
            });
#endif
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSR))) {
        IPAddress addr;
        addr.fromString(args.toString(0));
        args.printf_P(PSTR("Restarting MDNS, interface IP %s..."), addr.toString().c_str());
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

#else

bool MDNSService::addService(const String &service, const String &proto, uint16_t port)
{
    return false;
}

bool MDNSService::addServiceTxt(const String &service, const String &proto, const String &key, const String &value)
{
    return false;
}

bool MDNSService::removeService(const String &service, const String &proto)
{
    return false;
}

bool MDNSService::removeServiceTxt(const String &service, const String &proto, const String &key)
{
    return false;
}

void MDNSService::announce()
{
}

#endif
