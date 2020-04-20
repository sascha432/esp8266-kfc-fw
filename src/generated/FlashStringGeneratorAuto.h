// AUTO GENERATED FILE - DO NOT MODIFY
#pragma once
#include <avr/pgmspace.h>
#ifndef PROGMEM_STRING_DECL
#define PROGMEM_STRING_DECL(name)               extern const char _shared_progmem_string_##name[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)         const char _shared_progmem_string_##name[] PROGMEM = { value };
#endif
#ifndef SPGM
#define SPGM(name)                              _shared_progmem_string_##name
#define FSPGM(name)                             reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#endif
// lib/KFCTimezone/include/RemoteTimezone.h:41
// src/web_server.cpp:1045
PROGMEM_STRING_DECL(message);
// src/web_server.cpp:76
// src/web_server.cpp:662
// src/web_server.cpp:682
// src/web_server.cpp:785
// src/web_server.cpp:800
PROGMEM_STRING_DECL(_html);
// src/plugins/home_assistant/home_assistant.cpp:170
// src/plugins/home_assistant/home_assistant.cpp:187
// src/plugins/weather_station/weather_station.cpp:376
// src/plugins/weather_station/weather_station.cpp:383
PROGMEM_STRING_DECL(status__u);
