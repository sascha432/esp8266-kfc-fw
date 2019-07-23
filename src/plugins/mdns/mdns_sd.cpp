
/**
 * Author: sascha_lammers@gmx.de
 */

// TODO the esp8266 framework is rewriting the mdns responder.
// I removed my own implementation and waiting till it has been finished to integrate it

#if MDNS_SUPPORT

#include "mdns_sd.h"
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include "progmem_data.h"
#include "kfc_fw_config.h"
#include "misc.h"
#include "plugins.h"

// bool MDNSResponder::asyncQueryService(const char *service, const char *proto) {

//     debug_printf_P(PSTR("asyncQueryService %s %s\n"), service, proto);
//     if (_newQuery) {
//         debug_println(F("Last MDNS query not sent. Use freeQueryService() to clean up"));
//         return false;
//     }
//     if (_waitingForAnswers) {
//         debug_println(F("MDNS query still waiting for answers. Use freeQueryService() to clean up"));
//         return false;
//     }
//     MDNSResponder::queryService(service, proto, MDNS_QUERY_SERVICE_ASYNC);
//     return true;
// }

// int MDNSResponder::waitForAnswer(int tenth_seconds) {
//     if (tenth_seconds) {
//         delay(100 * tenth_seconds);
//     }
//     return _getNumAnswers();
// }

void MDNS_setup() {
    char buf[64];
    char *ptr = buf;
    const char *kfcmdns_ptr, *http_ptr = "", *ws_ptr = "", *tcp_ptr, *udp_ptr;
    kfcmdns_ptr = strcpy_P(ptr, PSTR("kfcmdns"));
    ptr += 8;
    tcp_ptr = strcpy_P(ptr, PSTR("tcp"));
    ptr += 4;
    udp_ptr = strcpy_P(ptr, PSTR("udp"));
    ptr += 4;

    MDNS.begin(config._H_STR(Config().device_name));
    debug_printf_P(PSTR("Setting MDNS hostname %s\n"), config._H_STR(Config().device_name));
#if WEBSERVER_SUPPORT
#  if WEBSERVER_TLS_SUPPORT
    if (_Config.getOptions().isHttpServerTLS()) {
        http_ptr = strcpy_P(ptr, PSTR("https"));
        ptr += 6;
        MDNS.addService(http_ptr,  tcp_ptr, _Config.get().http_port);
#    if PING_MONITOR || HTTP2SERIAL
        // ws_ptr = strcpy_P(ptr, PSTR("wss"));
        ptr += 4;
        MDNS.addService(ws_ptr, tcp_ptr, _Config.get().http_port);
#    endif
    } else
#  endif
    if (config._H_GET(Config().flags).webServerMode != HTTP_MODE_DISABLED) {
        http_ptr = strcpy_P(ptr, PSTR("http"));
        ptr += 5;
        MDNS.addService(http_ptr, tcp_ptr, config._H_GET(Config().http_port));
#  if PING_MONITOR || HTTP2SERIAL
        ws_ptr = strcpy_P(ptr, PSTR("ws"));
        // ptr += 3:
        MDNS.addService(ws_ptr, tcp_ptr, config._H_GET(Config().http_port));
#  endif
    }
#endif
    const char *v_ptr = strcpy_P(ptr, PSTR("v"));
    ptr += 2;
    // PGM_P version_f = PSTR(FIRMWARE_VERSION_STR);
    // const char *vesion_str = strcpy_P(ptr, version_f);
    // ptr += strlen_P(version_f);
    MDNS.addService(kfcmdns_ptr, udp_ptr, 5353);
    MDNS.addServiceTxt(kfcmdns_ptr, udp_ptr, v_ptr, FIRMWARE_VERSION_STR);
    MDNS_setup_wifi_event_callback();
}

void MDNS_auto_discovery(Print &out) {
    String tmp = config._H_STR(Config().device_name);
    tmp.toLowerCase();
    out.printf_P(SPGM(auto_discovery_html), tmp.c_str(), WiFi.localIP().toString().c_str());
    // int n = MDNS.waitForAnswer(0);
    int n=0;
    if (n >= 0) {
        for (int i = 0; i < n; i++) {
            tmp = MDNS.hostname(i);
            tmp.toLowerCase();
            out.printf_P(SPGM(auto_discovery_html), tmp.c_str(), MDNS.IP(i).toString().c_str());
        }
    }
    // MDNS.freeQueryService();
}
// at+mdns=kfcmdns,udp

void MDNS_query_service(const char *service, const char *proto, Stream &stream) {
    // MDNS.asyncQueryService(service, proto);
    // int n = MDNS.waitForAnswer(19);
    //  if (n == MDNSResponder::MDNS_POLL_ERROR_NO_SERVICES_FOUND) {
        // stream.println(F("+MDNS No services found"));
    // } else if (n == MDNSResponder::MDNS_POLL_ERROR_NOT_SENT) {
        // stream.println(F("+MDNS Query has not been sent"));
    // } else if (n == MDNSResponder::MDNS_POLL_ERROR_NOT_WAITING) {
        // stream.println(F("+MDNS Service is not waiting for answers"));
    // } else if (n > 0) {
    //     for (int i = 0; i < n; i++) {
    //         stream.print(F("+MDNS "));
    //         stream.print(MDNS.hostname(i));
    //         stream.print(F(" ("));
    //         stream.print(MDNS.IP(i));
    //         stream.print(F(":"));
    //         stream.print(MDNS.port(i));
    //         stream.println(F(")"));
    //     }
    // }
    // MDNS.freeQueryService();
}

PrintHtmlEntitiesString mdns_html;
MillisTimer mdns_timer;

void MDNS_query_poll_loop(void *) {
//     if (!mdns_timer.isActive()) {
//         char *ptr = get_static_str_buffer();
//         strcpy_P(ptr, PSTR("kfcmdns"));
//         strcpy_P(ptr + 16, PSTR("udp"));
//         // MDNS.asyncQueryService(ptr, ptr + 16);
//         mdns_timer.set(1000);
//         // mdns_html = String();
//     } else if (mdns_timer.reached()) { // isActive() returns true until reached() is called and the time has expired
//         // if (mdns_html.length()) {
//             if(0) {
//             // debug_println(F("freeing MDNS service cache"));
            // LoopFunctions::remove(MDNS_query_poll_loop);
//             // mdns_html = String();
//         } else {
//             // MDNS_auto_discovery(mdns_html);
// #if DEBUG
//             // if (!mdns_html.length()) {
//                 debug_println(F("MDNS_auto_discovery didn't return a result"));
//             }
// #endif
//             // debug_printf_P(PSTR("Timer reached, MDNS service result length %d, html result %p\n"), tmp.length(), mdns_html);
//             mdns_timer.set(MDNS_QUERY_CACHE_TIMEOUT * 1000UL); // cache response
//         }
//     }
}

HttpSimpleHeader MDNS_get_cache_ttl_header() {
    PrintString tmp;
    // if (mdns_html.length()) {
    //     tmp.printf_P(PSTR("%lu ms"), mdns_timer.get());
    // } else {
    //     tmp.print(F("empty-cache"));
    // }
    return HttpSimpleHeader(F("MDNS-cache-ttl"), tmp);
}

void MDNS_async_query_service() {
    // if (mdns_html.length() && mdns_timer.get() < MDNS_QUERY_CACHE_MIN_TIME_LEFT * 1000L) { // start new query if the cache expires soon
    //     mdns_html = String();
    //     mdns_timer.disable();
    // }
    LoopFunctions::add(MDNS_query_poll_loop);
}

String &MDNS_get_html_result() {
    return mdns_html;
}

void MDNS_wifi_callback(uint8_t event, void *payload) {
    MDNS.notifyAPChange();
}

void MDNS_setup_wifi_event_callback() {

    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED|WiFiCallbacks::EventEnum_t::MODE_CHANGE, MDNS_wifi_callback);
}

const String MDNS_get_status() {
    PrintString message;
    message.printf_P(PSTR("Hostname '%s'"), config.getString(_H(Config().device_name)));
    return message;
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MDNS, "MDNS", "<service>,<proto>", "Query MDNS");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(MDNSBSD, "MDNSBSD", "Broadcast service discovery on selected interfaces");

bool mdns_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (command.length() == 0) {

        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNS));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MDNSBSD));

    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(MDNS))) {
        if (argc != 2) {
            serial.println(F("ERROR - Invalid arguments"));
        } else {
            MDNS_query_service(argv[0], argv[1], serial);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(MDNSBSD))) {
        serial.println(F("Not supported"));
        //MDNS.broadcastDnsSdDiscovery();
        return true;
    }
    return false;
}

#endif

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ mdns,
/* setupPriority            */ PLUGIN_PRIO_MDNS,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ MDNS_setup,
/* statusTemplate           */ MDNS_get_status,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ nullptr,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ mdns_at_mode_command_handler
);

#endif
