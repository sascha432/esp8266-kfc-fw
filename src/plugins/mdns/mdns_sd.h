/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_MDNS_SD
#define DEBUG_MDNS_SD                   0
#endif

#include <Arduino_compat.h>
#if defined(ESP8266)
#include <ESP8266mDNS.h>
#endif
#if defined(ESP32)
#include <ESPmDNS.h>
#endif

#define MDNS_QUERY_CACHE_MIN_TIME_LEFT  10                                          // time left before cache expires and a new query is started
#define MDNS_QUERY_CACHE_TIMEOUT        (30 + MDNS_QUERY_CACHE_MIN_TIME_LEFT)       // clean cache after n seconds

// class AsyncWebServerResponse;

// void MDNS_auto_discovery(Stream &out);
void MDNS_query_service(const char *service, const char *proto, Stream &stream);
