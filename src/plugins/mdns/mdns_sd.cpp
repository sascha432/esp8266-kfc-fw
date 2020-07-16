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
#include <ESP8266NetBIOS.h>
#include "kfc_fw_config.h"
#include "kfc_fw_config_classes.h"
#include "web_server.h"
#include "async_web_response.h"
#include "misc.h"

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include "build.h"

#if MDNS_PLUGIN

PROGMEM_STRING_DEF(kfcmdns, "kfcmdns");

static MDNSPlugin plugin;


bool MDNSService::addService(const String &service, const String &proto, uint16_t port)
{
    _debug_printf_P(PSTR("service=%s proto=%s port=%u running=%u\n"), service.c_str(), proto.c_str(), port, plugin._isRunning());
    if (!plugin._isRunning()) {
        return false;
    }
#if ESP8266
    return _debug_print_result(MDNS.addService(service, proto, port));
#else
    MDNS.addService(service, proto, port);
    return true;
#endif
}

bool MDNSService::addServiceTxt(const String &service, const String &proto, const String &key, const String &value)
{
    _debug_printf_P(PSTR("service=%s proto=%s key=%s value=%s running=%u\n"), service.c_str(), proto.c_str(), key.c_str(), value.c_str(), plugin._isRunning());
    if (!plugin._isRunning()) {
        return false;
    }
#if ESP8266
    return _debug_print_result(MDNS.addServiceTxt(service, proto, key, value));
#else
    MDNS.addServiceTxt(service, proto, key, value);
    return true;
#endif
}

bool MDNSService::removeService(const String &service, const String &proto)
{
    _debug_printf_P(PSTR("service=%s proto=%s running=%u\n"), service.c_str(), proto.c_str(), plugin._isRunning());
    if (!plugin._isRunning()) {
        return false;
    }
#if ESP8266
    return _debug_print_result(MDNS.removeService(nullptr, service.c_str(), proto.c_str()));
#else
    return true;
#endif
}

bool MDNSService::removeServiceTxt(const String &service, const String &proto, const String &key)
{
    _debug_printf_P(PSTR("service=%s proto=%s key=%s running=%u\n"), service.c_str(), proto.c_str(), key.c_str(), plugin._isRunning());
    if (!plugin._isRunning()) {
        return false;
    }
#if ESP8266
    return _debug_print_result(MDNS.removeServiceTxt(nullptr, service.c_str(), proto.c_str(), key.c_str()));
#else
    return true;
#endif
}

void MDNSService::announce()
{
    _debug_printf_P(PSTR("running=%u\n"), plugin._isRunning());
#if ESP8266
    if (plugin._isRunning()) {
        MDNS.announce();
    }
#endif
}

void MDNSPlugin::_installWebServerHooks()
{
#if ESP8266
    auto server = WebServerPlugin::getWebServerObject();
    if (server) {
        _debug_println();
        WebServerPlugin::addHandler(F("/mdns_discovery"), mdnsDiscoveryHandler);
    }
#endif
}

#if ESP8266

void MDNSPlugin::mdnsDiscoveryHandler(AsyncWebServerRequest *request)
{
    debug_printf_P(PSTR("running=%u\n"), plugin._isRunning());
    if (plugin._isRunning()) {
        if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
            auto timeout = request->arg(F("timeout")).toInt();
            if (timeout == 0) {
                timeout = 2000;
            }
            ServiceInfoVector *services = new ServiceInfoVector();
            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();

            auto serviceQuery = MDNS.installServiceQuery(String(FSPGM(kfcmdns)).c_str(), String(FSPGM(udp)).c_str(), [services](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                plugin.serviceCallback(*services, true, mdnsServiceInfo, answerType, p_bSetContent);
            });
            auto response = new AsyncMDNSResponse(serviceQuery, services, timeout);
            httpHeaders.setAsyncBaseResponseHeaders(response);
            request->send(response);
        } else {
            request->send(403);
        }
    } else {
        request->send(500);
    }
}

#endif

void MDNSPlugin::_wifiCallback(uint8_t event, void *payload)
{
    _debug_printf_P(PSTR("event=%u, running=%u\n"), event, _running);
    if (WiFiCallbacks::EventEnum_t::CONNECTED) {
        _end();
#if MDNS_DELAYED_START_AFTER_WIFI_CONNECT
        _delayedStart.add(MDNS_DELAYED_START_AFTER_WIFI_CONNECT, false, [this](EventScheduler::TimerPtr) {
            _begin();
        });
#else
        _begin();
#endif
#if DEBUG_MDNS_SD
        auto result =
#endif
        NBNS.begin(config.getDeviceName());
        _debug_printf_P(PSTR("NetBIOS result=%u\n"), result);

    }
    else if (WiFiCallbacks::EventEnum_t::DISCONNECTED) {
        NBNS.end();
        _end();
    }
}

void MDNSPlugin::setup(SetupModeType mode)
{
    auto flags = config._H_GET(Config().flags);
    if (flags.enableMDNS && flags.wifiMode & WIFI_STA) {
        _enabled = true;
        begin();

        LoopFunctions::callOnce([this]() {
            _running = false;
            // add wifi handler after all plugins have been initialized
            WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::DISCONNECTED, wifiCallback);
            if (config.isWiFiUp()) {
                plugin._wifiCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr); // simulate event if already connected
            }
            _installWebServerHooks();
        });
    }
}

void MDNSPlugin::reconfigure(PGM_P source)
{
    _debug_printf_P(PSTR("running=%u source=%s\n"), _running, String(FPSTR(source)).c_str());
    if (!strcmp_P_P(source, WebServerPlugin::getPSTRName())) {
        _installWebServerHooks();
    }
    _end();
    _begin();
}

void MDNSPlugin::shutdown()
{
    debug_println();
    end();
}

void MDNSPlugin::wifiCallback(uint8_t event, void *payload)
{
    plugin._wifiCallback(event, payload);
}

void MDNSPlugin::loop()
{
    plugin._loop();
}

void MDNSPlugin::begin()
{
    _debug_println();

    if (_MDNS_begin()) {
        _running = true;
        if (MDNSService::addService(FSPGM(kfcmdns), FSPGM(udp), 5353)) {
            MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('v'), FIRMWARE_VERSION_STR);
            MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('b'), F(__BUILD_NUMBER));
            MDNSService::addServiceTxt(FSPGM(kfcmdns), FSPGM(udp), String('t'), config._H_STR(Config().device_title));
        }
    }
}

void MDNSPlugin::end()
{
    _debug_printf_P(PSTR("running=%u\n"), _running);
    if (_running) {
        MDNSService::removeService(FSPGM(kfcmdns), FSPGM(udp));
        MDNSService::announce();
        _end();
    }
}

bool MDNSPlugin::_isRunning() const
{
    return _running;
}

bool MDNSPlugin::_MDNS_begin()
{
#if ESP8266
    auto address = config.isWiFiUp() ? WiFi.localIP() : INADDR_ANY;
    _debug_printf_P(PSTR("hostname=%s address=%s\n"), config.getDeviceName(), address.toString().c_str());
    return _debug_print_result(MDNS.begin(config.getDeviceName(), address));
#else
    _debug_printf_P(PSTR("hostname=%s\n"), config.getDeviceName());
    return _debug_print_result(MDNS.begin(config.getDeviceName()));
#endif
}

void MDNSPlugin::_begin()
{
    _debug_printf_P(PSTR("running=%u\n"), _running);
    if (_MDNS_begin()) {
        _running = true;
        LoopFunctions::add(loop);
        MDNSService::announce();
    }
    else {
        _end();
    }
}

void MDNSPlugin::_end()
{
#if MDNS_DELAYED_START_AFTER_WIFI_CONNECT
    _delayedStart.remove();
#endif
    _debug_printf_P(PSTR("running=%u\n"), _running);
    _debug_println();
    MDNS.end();
    _running = false;
    LoopFunctions::remove(loop);
}

void MDNSPlugin::_loop()
{
// #if DEBUG_MDNS_SD
//     static unsigned long _update_timer = 0;
//     if (millis() < _update_timer) {
//         return;
//     }
//     _update_timer = millis() + 1000;
//     _debug_println(F("MDNS.update()"));
// #endif
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
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSR, "MDNSR", "<stop|start|enable|disable>", "Configure MDNS");
// PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(MDNSBSD, "MDNSBSD", "Broadcast service discovery on selected interfaces");

void MDNSPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSQ), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSR), getName());
    // at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSBSD), getName());
}

#if ESP8266

void MDNSPlugin::serviceCallback(ServiceInfoVector &services, bool map, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
{
    _debug_printf_P(PSTR("answerType=%u p_bSetContent=%u\n"), answerType, p_bSetContent)
    PrintString str;

    auto iterator = std::find_if(services.begin(), services.end(), [&mdnsServiceInfo](const ServiceInfo &info) {
        return info.serviceDomain.equals(mdnsServiceInfo.serviceDomain());
    });
    ServiceInfo *infoPtr;
    if (iterator != services.end()) {
        infoPtr = &(*iterator);
    }
    else {
        services.emplace_back(mdnsServiceInfo.serviceDomain());
        infoPtr = &services.back();
    }
    auto &info = *infoPtr;
    if (mdnsServiceInfo.hostDomainAvailable()) {
        _debug_printf_P(PSTR("domain=%s\n"), mdnsServiceInfo.hostDomain());
        info.domain = mdnsServiceInfo.hostDomain();
    }
    if (mdnsServiceInfo.IP4AddressAvailable()) {
        info.addresses = mdnsServiceInfo.IP4Adresses();
        _debug_printf_P(PSTR("addresses=%s\n"), implode_cb(',', info.addresses, [](const IPAddress &addr) { return addr.toString(); }).c_str());
    }
    if (mdnsServiceInfo.hostPortAvailable()) {
        _debug_printf_P(PSTR("port=%u\n"), mdnsServiceInfo.hostPort());
        info.port = mdnsServiceInfo.hostPort();
    }
    if (mdnsServiceInfo.txtAvailable()) {
        if (map) {
            for(const auto &item: mdnsServiceInfo.keyValues()) {
                _debug_printf_P(PSTR("txt=%s=%s\n"), item.first, item.second);
                info.map[item.first] = item.second;
            }
        }
        else {
            _debug_printf_P(PSTR("txt=%s\n"), mdnsServiceInfo.strKeyValue());
            info.txts = mdnsServiceInfo.strKeyValue();
        }
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

            ServiceInfoVector *services = new ServiceInfoVector();

            auto serviceQuery = MDNS.installServiceQuery(args.toString(0).c_str(), args.toString(1).c_str(), [this, services](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                serviceCallback(*services, false, mdnsServiceInfo, answerType, p_bSetContent);
            });

            Scheduler.addTimer(timeout, false, [args, serviceQuery, services, this](EventScheduler::TimerPtr) mutable {
                _IF_DEBUG(auto result = ) MDNS.removeServiceQuery(serviceQuery);
                _debug_printf_P(PSTR("removeServiceQuery=%u\n"), result);
                if (services->empty()) {
                    args.print(F("No response"));
                }
                else {
                    for(auto &svc: *services) {
                        args.printf_P(PSTR("domain=%s port=%u ips=%s txts=%s"), svc.domain.c_str(), svc.port, implode_cb(',', svc.addresses, [](const IPAddress &addr) {
                            return addr.toString();
                        }).c_str(), svc.txts.c_str());
                    }
                }
                delete services;
            });
#endif
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSR))) {
        if (args.equalsIgnoreCase(0, F("enable"))) {
            args.print(F("MDNS enabled. Requires restart..."));
            auto flags = KFCConfigurationClasses::System::Flags::read();
            flags.setMDNSEnabled(true);
            flags.write();
            config.write();
        }
        else if (args.equalsIgnoreCase(0, F("disable"))) {
            args.print(F("MDNS disabled. Requires restart..."));
            auto flags = KFCConfigurationClasses::System::Flags::read();
            flags.setMDNSEnabled(false);
            flags.write();
            config.write();
        }
        else if (args.isTrue(0)) {
            args.printf_P(PSTR("Restarting MDNS, interface IP %s..."), WiFi.localIP().toString().c_str());
            _end();
            _begin();
        }
        else {
            args.print(F("Stopping MDNS"));
            _end();
        }
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
