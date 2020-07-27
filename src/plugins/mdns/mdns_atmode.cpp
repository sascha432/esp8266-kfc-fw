/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_PLUGIN && AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include "mdns_plugin.h"
#include "at_mode.h"

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using Flags = KFCConfigurationClasses::System::Flags;

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSQ, "MDNSQ", "<service>,<proto>,[<wait=2000ms>]", "Query MDNS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSR, "MDNSR", "<stop|start|enable|disable|zeroconf>", "Configure MDNS");

void MDNSPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSQ), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSR), name);
    // at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSBSD), name);
}

#if ESP8266

void MDNSPlugin::serviceCallback(ServiceInfoVector &services, bool map, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
{
    __LDBG_printf("answerType=%u p_bSetContent=%u", answerType, p_bSetContent)
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
        __LDBG_printf("domain=%s", mdnsServiceInfo.hostDomain());
        info.domain = mdnsServiceInfo.hostDomain();
    }
    if (mdnsServiceInfo.IP4AddressAvailable()) {
        info.addresses = mdnsServiceInfo.IP4Adresses();
        __LDBG_printf("addresses=%s", implode_cb(',', info.addresses, [](const IPAddress &addr) { return addr.toString(); }).c_str());
    }
    if (mdnsServiceInfo.hostPortAvailable()) {
        __LDBG_printf("port=%u", mdnsServiceInfo.hostPort());
        info.port = mdnsServiceInfo.hostPort();
    }
    if (mdnsServiceInfo.txtAvailable()) {
        if (map) {
            for(const auto &item: mdnsServiceInfo.keyValues()) {
                __LDBG_printf("txt=%s=%s", item.first, item.second);
                info.map[item.first] = item.second;
            }
        }
        else {
            __LDBG_printf("txt=%s", mdnsServiceInfo.strKeyValue());
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
                __LDBG_printf("removeServiceQuery=%u", result);
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

        if (args.requireArgs(1)) {
            auto action = stringlist_find_P_P(PSTR("stop|start|enable|disable|zeroconf"), args.get(0), '|');
            switch(action) {
                case 0: { // stop
                    args.print(F("Stopping MDNS"));
                    _end();
                } break;
                case 1: { // start
                    args.printf_P(PSTR("Restarting MDNS, interface IP %s..."), WiFi.localIP().toString().c_str());
                    _end();
                    _begin();
                } break;
                case 2: { // enable
                    args.print(F("MDNS enabled. Requires restart..."));
                    auto flags = Flags(true);
                    flags.setMDNSEnabled(true);
                    flags.write();
                    config.write();
                } break;
                case 3: { // disable
                    args.print(F("MDNS disabled. Requires restart..."));
                    auto flags = Flags(true);
                    flags.setMDNSEnabled(false);
                    flags.write();
                    config.write();
                } break;
                case 4:
                    if (args.requireArgs(2, 2)) { // zeroconf
                        auto conf = args.toString(1);
                        auto result = config.resolveZeroConf(conf, 0, [args](const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type) mutable {
                            args.printf_P(PSTR("ZeroConf response: host=%s ip=%s port=%u type=%u"), hostname.c_str(), address.toString().c_str(), port, type);
                        });
                        if (result) {
                            args.print(F("Request pending"));
                        }
                        else {
                            args.printf_P(PSTR("Invalid config: %s"), conf.c_str());
                        }

                    } break;
                default:
                    args.printf_P(PSTR("Invalid argument: %s"), args.get(0));
                    break;
            }
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

