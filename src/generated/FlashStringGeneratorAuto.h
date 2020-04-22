// AUTO GENERATED FILE - DO NOT MODIFY
#pragma once
#include <avr/pgmspace.h>
#ifndef PROGMEM_STRING_DECL
#define PROGMEM_STRING_DECL(name)               extern const char _shared_progmem_string_##name[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)         const char _shared_progmem_string_##name[] PROGMEM = { value };
#endif
#ifndef SPGM
#define SPGM(name, ...)                         _shared_progmem_string_##name
#define FSPGM(name, ...)                        reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#endif
// lib/KFCTimezone/include/RemoteTimezone.h:41
// src/web_server.cpp:1124
// src/web_server.cpp:1128
// src/plugins/weather_station/weather_station.cpp:93
// src/plugins/weather_station/weather_station.cpp:95
// src/plugins/weather_station/weather_station.cpp:99
// src/plugins/weather_station/weather_station.cpp:123
PROGMEM_STRING_DECL(message);
// src/web_server.cpp:76
// src/web_server.cpp:662
// src/web_server.cpp:682
// src/web_server.cpp:786
// src/web_server.cpp:801
PROGMEM_STRING_DECL(_html);
// src/web_server.cpp:822
// src/web_server.cpp:831
// src/web_server.cpp:843
PROGMEM_STRING_DECL(cfg);
// src/web_server.cpp:822
PROGMEM_STRING_DECL(wifi);
// src/web_server.cpp:826
PROGMEM_STRING_DECL(_network_html);
// src/web_server.cpp:831
PROGMEM_STRING_DECL(network);
// src/web_server.cpp:843
PROGMEM_STRING_DECL(password);
// src/web_server.cpp:847
PROGMEM_STRING_DECL(_reboot_html);
// src/web_server.cpp:849
PROGMEM_STRING_DECL(safe_mode);
// src/web_server.cpp:856
// src/web_server.cpp:868
PROGMEM_STRING_DECL(_rebooting_html);
// src/web_server.cpp:858
// src/web_server.cpp:870
PROGMEM_STRING_DECL(_index_html);
// src/web_server.cpp:860
PROGMEM_STRING_DECL(_factory_html);
// src/plugins/home_assistant/home_assistant.cpp:170
// src/plugins/home_assistant/home_assistant.cpp:187
// src/plugins/weather_station/weather_station.cpp:413
// src/plugins/weather_station/weather_station.cpp:420
PROGMEM_STRING_DECL(status__u);
// src/plugins/weather_station/weather_station.cpp:97
PROGMEM_STRING_DECL(display);
// src/plugins/weather_station/weather_station.cpp:101
// src/plugins/weather_station/weather_station.cpp:103
PROGMEM_STRING_DECL(sensors);
// src/plugins/weather_station/weather_station.cpp:109
PROGMEM_STRING_DECL(name);
// src/plugins/weather_station/weather_station.cpp:110
PROGMEM_STRING_DECL(values);
