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

using KFCConfigurationClasses::System;

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSQ, "MDNSQ", "<service>,<proto>,[<wait=2000ms>]", "Query MDNS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSR, "MDNSR", "<stop|start|enable|disable|zeroconf>", "Configure MDNS");

ATModeCommandHelpArrayPtr MDNSPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(MDNSQ),
        PROGMEM_AT_MODE_HELP_COMMAND(MDNSR)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#if ESP8266

void MDNSPlugin::serviceCallback(Output &output, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
{
    if (!output._timeout || !p_bSetContent) {
        return;
    }

    noInterrupts();

    if (output._current != mdnsServiceInfo.serviceDomain()) {
        output.next();
        output._current = mdnsServiceInfo.serviceDomain();
    }

    switch (answerType) {
        case MDNSResponder::AnswerType::ServiceDomain: {
                JsonTools::Utf8Buffer buffer;
                output._output.print(F("\"s\":\""));
                JsonTools::printToEscaped(output._output, mdnsServiceInfo.serviceDomain(), strlen(mdnsServiceInfo.serviceDomain()), &buffer);
                output._output.print(F("\","));
            }
            break;
        case MDNSResponder::AnswerType::HostDomainAndPort: {
                JsonTools::Utf8Buffer buffer;
                output._output.print(F("\"h\":\""));
                JsonTools::printToEscaped(output._output, mdnsServiceInfo.hostDomain(), strlen(mdnsServiceInfo.hostDomain()), &buffer);
                output._output.printf_P(PSTR("\",\"p\":%u,"), mdnsServiceInfo.hostPort());
            }
            break;
        case MDNSResponder::AnswerType::IP4Address: {
                output._output.print(F("\"a\":["));
                for (const IPAddress &ip: mdnsServiceInfo.IP4Adresses()) {
                    output._output.print('"');
                    output._output.print(ip.toString());
                    output._output.print(F("\","));
                }
                output._output.rtrim(',');
                output._output.print(F("],"));
            }
            break;
        case MDNSResponder::AnswerType::Txt: {
                auto keys = PSTR("vbt");
                auto ptr = keys;
                char ch;
                while((ch = pgm_read_byte(ptr++)) != 0) {
                    String key(ch);
                    auto value = mdnsServiceInfo.value(key.c_str());
                    if (value) {
                        output._output.printf_P(PSTR("\"%s\":\""), key.c_str());
                        JsonTools::Utf8Buffer buffer;
                        JsonTools::printToEscaped(output._output, value, strlen(value), &buffer);
                        output._output.print(F("\","));
                    }
                }
            }
            break;
        default:
            break;
    }
    if (output._output.length() + 1024 > ESP.getFreeHeap()) {
        output.end();
        interrupts();
        __DBG_printf("out of memory len=%u", output._output.length());
    }
    interrupts();
    // __DBG_printf("%s", output._output.c_str());
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

            Output *output = new MDNSPlugin::Output(millis() + timeout);

            output->_serviceQuery = MDNS.installServiceQuery(args.toString(0).c_str(), args.toString(1).c_str(), [this, output](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                serviceCallback(*output, mdnsServiceInfo, answerType, p_bSetContent);
            });

            _Scheduler.add(timeout, false, [args, output, this](Event::CallbackTimerPtr timer) mutable {
                _IF_DEBUG(auto result = ) MDNS.removeServiceQuery(output->_serviceQuery);
                output->_serviceQuery = nullptr;
                output->end();

                __LDBG_printf("removeServiceQuery=%u", result);
                if (output->_output.length() == 0) {
                    args.print(F("No response"));
                }
                else {
                    args.getStream().println(output->_output);
                }
                delete output;
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
                    auto &flags = System::Flags::getWriteableConfig();
                    // flags.setMDNSEnabled(true);
                    flags.is_mdns_enabled = true;
                    config.write();
                } break;
                case 3: { // disable
                    args.print(F("MDNS disabled. Requires restart..."));
                    auto &flags = System::Flags::getWriteableConfig();
                    //flags.setMDNSEnabled(false);
                    flags.is_mdns_enabled = false;
                    config.write();
                } break;
                case 4:
                    if (args.requireArgs(2, 2)) { // zeroconf
                        auto conf = args.toString(1);
                        auto result = config.resolveZeroConf(getFriendlyName(), conf, 0, [args](const String &hostname, const IPAddress &address, uint16_t port, const String &resolved, MDNSResolver::ResponseType type) mutable {
                            args.printf_P(PSTR("ZeroConf response: host=%s ip=%s port=%u type=%u resolved=%s"), hostname.c_str(), address.toString().c_str(), port, type, resolved.c_str());
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

