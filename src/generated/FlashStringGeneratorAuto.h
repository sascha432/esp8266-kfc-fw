// AUTO GENERATED FILE - DO NOT MODIFY
#pragma once
#if ESP32
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#ifndef PROGMEM_STRING_DECL
#define PROGMEM_STRING_DECL(name)               extern const char _shared_progmem_string_##name[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)         const char _shared_progmem_string_##name[] PROGMEM = { value };
#endif
#ifndef SPGM
#define SPGM(name, ...)                         _shared_progmem_string_##name
#define FSPGM(name, ...)                        reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#endif
// lib/KFCTimezone/include/RemoteTimezone.h:41
// src/web_server.cpp:1210
// src/web_server.cpp:1214
// src/plugins/weather_station/weather_station.cpp:105
// src/plugins/weather_station/weather_station.cpp:107
// src/plugins/weather_station/weather_station.cpp:109
// src/plugins/weather_station/weather_station.cpp:124
// src/plugins/weather_station/weather_station.cpp:128
// src/plugins/weather_station/weather_station.cpp:137
// src/plugins/weather_station/weather_station.cpp:147
// src/plugins/weather_station/weather_station.cpp:151
// src/plugins/weather_station/weather_station.cpp:174
// src/plugins/weather_station/weather_station.cpp:178
PROGMEM_STRING_DECL(message);
// src/plugins.cpp:126
// src/plugins.cpp:130
PROGMEM_STRING_DECL(Home);
// src/plugins.cpp:127
// src/plugins.cpp:130
// src/web_server.cpp:732
// src/web_server.cpp:941
// src/web_server.cpp:954
PROGMEM_STRING_DECL(index_html);
// src/plugins.cpp:131
// src/plugins.cpp:138
PROGMEM_STRING_DECL(Status);
// src/plugins.cpp:131
// src/plugins.cpp:139
// src/plugins.cpp:209
// src/web_server.cpp:735
PROGMEM_STRING_DECL(status_html);
// src/plugins.cpp:132
// src/plugins.cpp:142
// src/web_server.cpp:726
PROGMEM_STRING_DECL(wifi_html);
// src/plugins.cpp:133
// src/plugins.cpp:143
// src/web_server.cpp:723
PROGMEM_STRING_DECL(network_html);
// src/plugins.cpp:134
// src/plugins.cpp:150
PROGMEM_STRING_DECL(Change_Password);
// src/plugins.cpp:134
// src/plugins.cpp:150
PROGMEM_STRING_DECL(password_html);
// src/plugins.cpp:135
// src/plugins.cpp:151
PROGMEM_STRING_DECL(Reboot_Device);
// src/plugins.cpp:135
// src/plugins.cpp:151
PROGMEM_STRING_DECL(reboot_html);
// src/plugins.cpp:141
PROGMEM_STRING_DECL(Configuration);
// src/plugins.cpp:142
PROGMEM_STRING_DECL(WiFi);
// src/plugins.cpp:143
PROGMEM_STRING_DECL(Network);
// src/plugins.cpp:144
// src/plugins.cpp:147
PROGMEM_STRING_DECL(Device);
// src/plugins.cpp:144
// src/web_server.cpp:729
PROGMEM_STRING_DECL(device_html);
// src/plugins.cpp:145
PROGMEM_STRING_DECL(remote_html);
// src/plugins.cpp:149
PROGMEM_STRING_DECL(Admin);
// src/plugins.cpp:152
// src/web_server.cpp:944
PROGMEM_STRING_DECL(factory_html);
// src/plugins.cpp:193
// src/web_server.cpp:78
// src/web_server.cpp:708
// src/web_server.cpp:738
// src/web_server.cpp:844
// src/web_server.cpp:859
PROGMEM_STRING_DECL(_html);
// src/templates.cpp:426
// src/templates.cpp:451
// src/plugins/atomic_sun/atomic_sun_v2.cpp:678
// src/plugins/switch/switch.cpp:142
// src/plugins/weather_station/weather_station.cpp:108
PROGMEM_STRING_DECL(title);
// src/templates.cpp:465
// src/web_server.cpp:815
// src/web_server.cpp:818
// src/web_server.cpp:922
PROGMEM_STRING_DECL(password);
// src/web_server.cpp:815
// src/web_server.cpp:818
PROGMEM_STRING_DECL(username);
// src/web_server.cpp:883
// src/web_server.cpp:895
// src/web_server.cpp:907
// src/web_server.cpp:922
PROGMEM_STRING_DECL(cfg);
// src/web_server.cpp:883
PROGMEM_STRING_DECL(wifi);
// src/web_server.cpp:884
// src/web_server.cpp:896
// src/web_server.cpp:908
// src/web_server.cpp:923
PROGMEM_STRING_DECL(applying_html);
// src/web_server.cpp:931
PROGMEM_STRING_DECL(safe_mode);
// src/web_server.cpp:938
// src/web_server.cpp:952
PROGMEM_STRING_DECL(rebooting_html);
// src/plugins/home_assistant/home_assistant.cpp:194
// src/plugins/weather_station/weather_station.cpp:522
// src/plugins/weather_station/weather_station.cpp:529
PROGMEM_STRING_DECL(status__u);
// src/plugins/weather_station/weather_station.cpp:134
PROGMEM_STRING_DECL(display);
// src/plugins/weather_station/weather_station.cpp:156
// src/plugins/weather_station/weather_station.cpp:158
PROGMEM_STRING_DECL(sensors);
// src/plugins/weather_station/weather_station.cpp:164
PROGMEM_STRING_DECL(name);
// src/plugins/weather_station/weather_station.cpp:165
PROGMEM_STRING_DECL(values);
PROGMEM_STRING_DECL(network);
PROGMEM_STRING_DECL(device);
