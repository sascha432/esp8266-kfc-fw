/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_PLUGIN && AT_MODE_SUPPORTED

#include <Arduino_compat.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <Mutex.h>
#include "mdns_plugin.h"
#include "at_mode.h"

using namespace KFCJson;

#if DEBUG_MDNS_SD
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSQ, "MDNSQ", "<service>,<proto>,[<wait=3000ms>]", "Query MDNS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNSR, "MDNSR", "<stop|start|enable|disable|zeroconf>", "Configure MDNS");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr MDNSPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(MDNSQ),
        PROGMEM_AT_MODE_HELP_COMMAND(MDNSR)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

#if ESP8266

void MDNSPlugin::serviceCallback(Output &output, MDNSResponder::MDNSServiceInfo &mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent)
{
    if (!output._timeout || !p_bSetContent) {
        return;
    }

    MUTEX_LOCK_BLOCK(output._lock) {

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
                        ip.printTo(output._output);
                        // output._output.print(ip.toString());
                        output._output.print(F("\","));
                    }
                    output._output.rtrim(',');
                    output._output.print(F("],"));
                }
                break;
            case MDNSResponder::AnswerType::Txt: {
                    auto keys = PSTR("vbtd");
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
            __DBG_printf("out of memory len=%u", output._output.length());
        }
        // __DBG_printf("%s", output._output.c_str());
    }
}

#elif ESP32

bool MDNSPlugin::Output::poll(uint32_t timeout, bool lock)
{
    auto outputLen = _output.length();
    if (_timeout && _serviceQuery) {
        mdns_result_t *results = nullptr;
        if (!mdns_query_async_get_results(_serviceQuery, timeout, &results)) {
            return false;
        }
        __LDBG_printf("results=%p", results);
        if (!results) {
            return false;
        }

        {
            MutexLock mLock(_lock, lock);
            auto cur = results;
            while(cur) {
                next();
                if (cur->ip_protocol == mdns_ip_protocol_t::MDNS_IP_PROTOCOL_V4) {
                    uint32_t ipCount = 0;
                    auto addr = cur->addr;
                    while(addr) {
                        if (addr->addr.type == ESP_IPADDR_TYPE_V4) {
                            __LDBG_printf("ipv4=%u", addr->addr.u_addr.ip4);
                            if (ipCount++ == 0) {
                                _output.print(F("\"a\":["));
                            }
                            _output.print('"');
                            IPAddress(addr->addr.u_addr.ip4.addr).printTo(_output);
                            _output.print(F("\","));
                        }
                        addr = addr->next;
                    }
                    if (ipCount) {
                        _output.rtrim(',');
                        _output.print(F("],"));
                    }

                    JsonTools::Utf8Buffer buffer;
                    _output.print(F("\"s\":\""));
                    JsonTools::printToEscaped(_output, cur->instance_name, strlen(cur->instance_name), &buffer);
                    _output.print(F("\","));

                    buffer = JsonTools::Utf8Buffer();
                    _output.print(F("\"h\":\""));
                    JsonTools::printToEscaped(_output, cur->hostname, strlen(cur->hostname), &buffer);
                    if (!strchr(cur->hostname, '.')) {
                        _output.print(F(".local"));
                    }
                    _output.printf_P(PSTR("\",\"p\":%u,"), cur->port);

                    __LDBG_printf("instance=%s if=%u hostname=%s port=%u txt_count=%u",
                        __S(cur->instance_name), cur->tcpip_if, __S(cur->hostname), cur->port, cur->txt_count
                    );

                    // collect data from txt records
                    auto count = cur->txt_count;
                    auto txt = cur->txt;
                    auto len = cur->txt_value_len;
                    auto keys = PSTR("vbtd");
                    while(count--) {
                        __LDBG_printf("key=%s value=%*.*s", txt->key, *len, *len, txt->value);
                        auto ptr = keys;
                        char ch;
                        while((ch = pgm_read_byte(ptr++)) != 0) {
                            String key(ch);
                            if (key.equals(txt->key)) {
                                _output.printf_P(PSTR("\"%s\":\""), key.c_str());
                                buffer = JsonTools::Utf8Buffer();
                                JsonTools::printToEscaped(_output, txt->value, *len, &buffer);
                                _output.print(F("\","));
                            }
                        }
                        txt++;
                        len++;
                    }
                }
                cur = cur->next;
            }
        }
        mdns_query_results_free(results);
    }
    __LDBG_printf("output=%u old=%u", _output.length(), outputLen);
    return _output.length() != outputLen;
}

#endif

bool MDNSPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSQ))) {
        if (!_isRunning()) {
            args.print(F("MDNS service is not running"));
        }
        else if (args.requireArgs(2, 3)) {
            auto timeout = args.toMillis(2, 100, ~0, 3000);
            auto query = PrintString(F("service=%s proto=%s wait=%ums"), args.toString(0).c_str(), args.toString(1).c_str(), timeout);
            args.print(F("Querying: %s"), query.c_str());

            // +MDNSQ=_kfcmdns,_udp
            // +MDNSQ=kfcmdns,udp

            auto output = new MDNSPlugin::Output(timeout);
            if (output) {
                #if ESP8266
                    output->_serviceQuery = MDNS.installServiceQuery(args.toString(0).c_str(), args.toString(1).c_str(), [this, output](MDNSResponder::MDNSServiceInfo mdnsServiceInfo, MDNSResponder::AnswerType answerType, bool p_bSetContent) {
                        serviceCallback(*output, mdnsServiceInfo, answerType, p_bSetContent);
                    });
                #elif ESP32
                    output->_serviceQuery = mdns_query_async_new(emptyString.c_str(), args.toString(0).c_str(), args.toString(1).c_str(), MDNS_TYPE_PTR, timeout, 64, NULL);
                #endif

                if (output->_serviceQuery) {
                    _Scheduler.add(timeout, false, [args, output, this](Event::CallbackTimerPtr timer) {
                        #if ESP32
                            if (esp_task_wdt_add(nullptr) != ESP_OK) {
                                __DBG_printf_E("esp_task_wdt_add failed");
                            }
                            while(output->poll(1000)) {
                                esp_task_wdt_reset();
                                __LDBG_printf("poll repeat");
                            }
                            esp_task_wdt_delete(NULL);
                        #endif
                        MUTEX_LOCK_BLOCK(output->_lock) {
                            output->end();
                        }

                        if (output->_output.length() == 0) {
                            args.print(F("No response"));
                        }
                        else {
                            args.getStream().println(output->_output);
                        }

                        delete output;
                    });
                }
                else {
                    args.print(F("Failed to create service query"));
                }
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MDNSR))) {

        enum class MDNSCommand : int8_t {
            INVALID = -1,
            STOP = 0,
            START,
            ENABLE,
            DISABLE,
            ZEROCONF,
        };

        if (args.requireArgs(1)) {
            auto action = static_cast<MDNSCommand>(stringlist_find_P_P(PSTR("stop|start|enable|disable|zeroconf"), args.get(0), '|'));
            switch(action) {
                case MDNSCommand::STOP: {
                        args.print(F("Stopping MDNS"));
                        end();
                    }
                    break;
                case MDNSCommand::START: {
                        args.printf_P(PSTR("Restarting MDNS, interface IP %s..."), WiFi.localIP().toString().c_str());
                        end();
                        begin();
                    }
                    break;
                case MDNSCommand::ENABLE: {
                        args.print(F("MDNS enabled. Requires restart..."));
                        auto &flags = System::Flags::getWriteableConfig();
                        flags.is_mdns_enabled = true;
                        config.write();
                    }
                    break;
                case MDNSCommand::DISABLE: {
                        args.print(F("MDNS disabled. Requires restart..."));
                        auto &flags = System::Flags::getWriteableConfig();
                        flags.is_mdns_enabled = false;
                        config.write();
                    }
                    break;
                case MDNSCommand::ZEROCONF:
                    if (args.requireArgs(2, 2)) {
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
                    }
                    break;
                default:
                    args.printf_P(PSTR("Invalid argument: %s"), args.get(0));
                    break;
            }
        }
        return true;
    }
    return false;
}

#endif

