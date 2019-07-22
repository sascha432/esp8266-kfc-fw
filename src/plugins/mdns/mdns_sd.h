/**
 * Author: sascha_lammers@gmx.de
 */

#if MDNS_SUPPORT

#pragma once

#include <Arduino_compat.h>
#include <HttpHeaders.h>
#include "ESP8266mDNS.h"

#define MDNS_QUERY_CACHE_MIN_TIME_LEFT  10                                          // time left before cache expires and a new query is started
#define MDNS_QUERY_CACHE_TIMEOUT        (30 + MDNS_QUERY_CACHE_MIN_TIME_LEFT)       // clean cache after n seconds

class AsyncWebServerResponse;

void MDNS_auto_discovery(Print &out);
void MDNS_query_service(const char *service, const char *proto, Stream &stream);
String &MDNS_get_html_result();
HttpSimpleHeader MDNS_get_cache_ttl_header();
void MDNS_query_poll_loop();
void MDNS_async_query_service();
void MDNS_setup_wifi_event_callback();

#endif
