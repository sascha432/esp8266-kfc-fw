/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <Form.h>
#include <algorithm>
#include <crc16.h>
#include <Buffer.h>
#include "plugins.h"
#ifndef DISABLE_EVENT_SCHEDULER
#include <EventScheduler.h>
#endif

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

PluginsVector plugins;

void register_plugin(PGM_PLUGIN_CONFIG_P config) {
    Plugin_t plugin;
    plugin.config = config;
    _debug_printf_P(PSTR("register_plugin %s priority %d\n"), plugin.getPluginName().c_str(), plugin.getSetupPriority());
    plugins.push_back(plugin);
}

// PROGMEM_PLUGIN_CONFIG_DEF(
// /* pluginName               */ NAME,
// /* setupPriority            */ 15,
// /* allowSafeMode            */ false,
// /* autoSetupWakeUp          */ false,
// /* rtcMemoryId              */ 0,
// /* setupPlugin              */ nullptr,
// /* statusTemplate           */ nullptr,
// /* configureForm            */ nullptr,
// /* reconfigurePlugin        */ nullptr,
// /* prepareDeepSleep         */ nullptr,
// /* atModeCommandHandler     */ nullptr
// );

PROGMEM_PLUGIN_CONFIG_DECL(rd);
PROGMEM_PLUGIN_CONFIG_DECL(dpslp);
PROGMEM_PLUGIN_CONFIG_DECL(mdns);
PROGMEM_PLUGIN_CONFIG_DECL(esp8266atc);
PROGMEM_PLUGIN_CONFIG_DECL(http);
PROGMEM_PLUGIN_CONFIG_DECL(syslog);
PROGMEM_PLUGIN_CONFIG_DECL(ntp);
PROGMEM_PLUGIN_CONFIG_DECL(mqtt);
PROGMEM_PLUGIN_CONFIG_DECL(pingmon);
PROGMEM_PLUGIN_CONFIG_DECL(http2ser);
PROGMEM_PLUGIN_CONFIG_DECL(ser2tcp);
PROGMEM_PLUGIN_CONFIG_DECL(hass);
PROGMEM_PLUGIN_CONFIG_DECL(hue);
PROGMEM_PLUGIN_CONFIG_DECL(atomicsun);
PROGMEM_PLUGIN_CONFIG_DECL(filemgr);
PROGMEM_PLUGIN_CONFIG_DECL(I2Cscan);

void register_all_plugins() {

    register_plugin(SPGM_PLUGIN_CONFIG_P(rd));
    register_plugin(SPGM_PLUGIN_CONFIG_P(dpslp));

#if MDNS_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(mdns));
#endif
#if ESP8266_AT_MODE_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(esp8266atc));
#endif
#if WEBSERVER_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(http));
#endif
#if SYSLOG
    register_plugin(SPGM_PLUGIN_CONFIG_P(syslog));
#endif
#if NTP_CLIENT
    register_plugin(SPGM_PLUGIN_CONFIG_P(ntp));
#endif
#if MQTT_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(mqtt));
#endif
#if PING_MONITOR
    register_plugin(SPGM_PLUGIN_CONFIG_P(pingmon));
#endif
#if HTTP2SERIAL
    register_plugin(SPGM_PLUGIN_CONFIG_P(http2ser));
#endif
#if SERIAL2TCP
    register_plugin(SPGM_PLUGIN_CONFIG_P(ser2tcp));
#endif
#if HOME_ASSISTANT_INTEGRATION
    register_plugin(SPGM_PLUGIN_CONFIG_P(hass));
#endif
#if HUE_EMULATION
    register_plugin(SPGM_PLUGIN_CONFIG_P(hue));
#endif
#if IOT_ATOMIC_SUN_V2
    register_plugin(SPGM_PLUGIN_CONFIG_P(atomicsun));
#endif
#if FILE_MANAGER
    register_plugin(SPGM_PLUGIN_CONFIG_P(filemgr));
#endif
#if I2CSCANNER_PLUGIN
    register_plugin(SPGM_PLUGIN_CONFIG_P(I2Cscan));
#endif

#if DEBUG
    uint8_t i = 0;
    for(auto &plugin: plugins) {
        if (plugin.getRtcMemoryId() > PLUGIN_RTC_MEM_MAX_ID) {
            _debug_printf_P(PSTR("WARNING! Plugin %s is using an invalid id %d\n"), plugin.getPluginName().c_str(), plugin.getRtcMemoryId());
        }
        if (plugin.getRtcMemoryId()) {
            uint8_t j = 0;
            for(auto &plugin2: plugins) {
                if (i != j && plugin2.getRtcMemoryId() && plugin.getRtcMemoryId() == plugin2.getRtcMemoryId()) {
                    _debug_printf_P(PSTR("WARNING! Plugin %s and %s use the same id (%d) to identify the RTC memory data\n"), plugin.getPluginName().c_str(), plugin2.getPluginName().c_str(), plugin.getRtcMemoryId());
                }
                j++;
            }
        }
        i++;
    }
#endif
}

void dump_plugin_list(Print &output) {
    for(auto plugin : plugins) {
        output.printf_P(PSTR("name %s priority %d allow_safe_mode %d setup %p\n"),
            plugin.getPluginName().c_str(),
            plugin.getSetupPriority(),
            plugin.isAllowSafeMode(),
            plugin.getSetupPlugin()
            //TOOD all missing items
        );
    }
}

void setup_plugins(PluginSetupMode_t mode) {

    if (mode != PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP) {

        _debug_printf_P(PSTR("setup_plugins(%d) counter %d\n"), mode, plugins.size());

        std::sort(plugins.begin(), plugins.end(), [](const Plugin_t &a, const Plugin_t &b) {
            return b.getSetupPriority() >= a.getSetupPriority();
        });
    }

    for(auto &plugin : plugins) {
        bool runSetup =
            (mode == PLUGIN_SETUP_DEFAULT) ||
            (mode == PLUGIN_SETUP_SAFE_MODE && plugin.isAllowSafeMode()) ||
            (mode == PLUGIN_SETUP_AUTO_WAKE_UP && plugin.isAutoSetupAfterDeepSleep()) ||
            (mode == PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP && !plugin.isAutoSetupAfterDeepSleep());
        _debug_printf_P(PSTR("setup_plugins(%d) %s priority %d run setup %d\n"), mode, plugin.getPluginName().c_str(), plugin.getSetupPriority(), runSetup);
        if (runSetup) {
            plugin.callSetupPlugin();
        }
    }

#ifndef DISABLE_EVENT_SCHEDULER
    if (mode == PLUGIN_SETUP_AUTO_WAKE_UP) {
        Scheduler.addTimer(30000, false, [](EventScheduler::TimerPtr timer) {
            setup_plugins(PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP);
        });
    }
#endif
}

Plugin_t *get_plugin_by_name(PGM_P name) {
    for(auto &plugin: plugins) {
        if (plugin.pluginNameEquals(FPSTR(name))) {
            _debug_printf_P(PSTR("get_plugin_by_name(%s) = %p\n"), String(FPSTR(name)).c_str(), &plugin);
            return &plugin;
        }
    }
    _debug_printf_P(PSTR("get_plugin_by_name(%s) = nullptr\n"), String(FPSTR(name)).c_str());
    return nullptr;
}

Plugin_t *get_plugin_by_name(const String &name) {
    for(auto &plugin: plugins) {
        if (plugin.pluginNameEquals(name)) {
            _debug_printf_P(PSTR("get_plugin_by_name(%s) = %p\n"), name.c_str(), &plugin);
            return &plugin;
        }
    }
    _debug_printf_P(PSTR("get_plugin_by_name(%s) = nullptr\n"), name.c_str());
    return nullptr;
}

static Plugin_t *get_plugin_by_rtc_memory_id(uint8_t id) {
    if (id <= 0 || id > PLUGIN_RTC_MEM_MAX_ID) {
        _debug_printf_P(PSTR("get_plugin_by_rtc_memory_id(%d): invalid id\n"), id);
        return nullptr;
    }
    for(auto &plugin: plugins) {
        if (plugin.getRtcMemoryId() == id) {
            return &plugin;
        }
    }
    return nullptr;
}

static bool __plugin_read_rtc_memory_header(rtc_memory_header &header, uint32_t &offset, uint16_t &length) {

    bool result = false;
    offset = PLUGIN_RTC_MEM_OFFSET;

    if (ESP.rtcUserMemoryRead(offset / PLUGIN_RTC_MEM_BLK_SIZE, (uint32_t *)&header, sizeof(header))) {
        if (header.length == 0) {
            _debug_printf_P(PSTR("RTC memory: Empty\n"));
        } else if (header.length & (PLUGIN_RTC_MEM_BLK_SIZE - 1)) {
            _debug_printf_P(PSTR("RTC memory: Stored length not DWORD aligned: %d\n"), header.length);
        } else {
            offset = PLUGIN_RTC_MEM_SIZE - PLUGIN_RTC_MEM_DATA_SIZE - header.length;
            if (offset > (uint32_t)PLUGIN_RTC_MEM_OFFSET) {
                _debug_printf_P(PSTR("RTC memory: Invalid length, start offset %d\n"), offset);
            } else {
                length = header.length;
                result = true;
            }
        }
    }
    return result;
}

static uint32_t *__plugin_read_rtc_memory(uint16_t &length) {

    uint32_t *rtc_memory = nullptr;
    rtc_memory_header header;
    uint32_t offset;

    if (__plugin_read_rtc_memory_header(header, offset, length)) {
        uint16_t size = PLUGIN_RTC_MEM_SIZE - offset; // header.length + sizeof(rtc_memory_header) + possibly padding for rtc_memory_header
        rtc_memory = (uint32_t *)calloc(1, size);
        uint16_t crc = -1;
        if (!ESP.rtcUserMemoryRead(offset / PLUGIN_RTC_MEM_BLK_SIZE, rtc_memory, size) || ((crc = crc16_calc((const uint8_t *)rtc_memory, header.length + sizeof(header) - sizeof(header.crc))) != header.crc)) {
            debug_printf(PSTR("RTC memory: CRC mismatch %04x != %04x, length %d\n"), crc, header.crc, size);
            free(rtc_memory);
            return nullptr;
        }
    }
    return rtc_memory;
}

// read RTC memory
bool plugin_read_rtc_memory(uint8_t id, void *dataPtr, uint8_t maxSize) {
    memset(dataPtr, 0, maxSize);
    _debug_printf_P(PSTR("plugin_read_rtc_memory(%d, %d)\n"), id, maxSize);
    if (id <= 0 || id > PLUGIN_RTC_MEM_MAX_ID) {
        _debug_printf_P(PSTR("plugin_read_rtc_memory(%d): invalid id\n"), id);
    }
    uint16_t length;

// TODO this function could save memory by reading from RTC and storing the result directly in dataPtr instead of using __plugin_read_rtc_memory()
#if 0
    // not well tested, variable "data" seems to get corrupted
    rtc_memory_header header;
    uint32_t offset;
    uint16_t crc = ~0;
    bool result = true;

    if (__plugin_read_rtc_memory_header(header, offset, length)) {
        uint8_t *ptr = (uint8_t *)dataPtr;
        uint8_t entryOffset = 0;
        offset /= PLUGIN_RTC_MEM_BLK_SIZE;
        while(length) {
            uint8_t data[4];
            if (!ESP.rtcUserMemoryRead(offset++, (uint32_t *)&data, sizeof(data))) {
                result = false;
                break;
            }
            length -= 4;  // check length?
            bool match = false;
            auto entry = (rtc_memory_entry_header *)(&data[entryOffset]);
            if (!entry->mem_id) {
                break; // done
            }
            else if (entry->mem_id == id) {
                match = true;
            }

            // here it gets a bit complicated
            // it needs to calculate the number of DWORDS to read and the offset for the next entry, since it isn't aligned

            uint16_t bytes = entry->length + sizeof(*entry);
            uint8_t dwords = bytes / 4;
            uint8_t copyOffset = entryOffset + sizeof(*entry);
            if (match && copyOffset < sizeof(data)) {
                memcpy(ptr, data, sizeof(data) - copyOffset);
                ptr += sizeof(data) - copyOffset;
            }

            entryOffset = bytes % 4; // new offset inside the 4 byte data block
            if (!entryOffset) { // no offset for the next DWORD, read it here
                dwords++;
            }
            while(--dwords) { // we got the first DWORD already
                if (!ESP.rtcUserMemoryRead(offset++, (uint32_t *)&data, sizeof(data))) {
                    result = false;
                    break;
                }
                length -= 4; // check length?
                crc = crc16_update(crc, (const uint8_t *)&data, sizeof(data));
                if (match) {
                    memcpy(ptr, &data, sizeof(data));
                    ptr += sizeof(data);
                }
            }
            if (match && entryOffset) { // copy the missing bytes from the beginning of the next DWORD
                if (!ESP.rtcUserMemoryRead(offset, (uint32_t *)ptr, sizeof(data) - entryOffset)) {
                    result = false;
                    break;
                }
                ptr += sizeof(data) - entryOffset;
            }
        }

        // update the CRC with the partial header
        crc = crc16_update(crc, (const uint8_t *)&header, sizeof(header) - sizeof(header.crc));

    }
#endif

    auto memPtr = __plugin_read_rtc_memory(length);
    if (!memPtr) {
        return false;
    }

    auto ptr = (uint8_t *)memPtr;
    auto endPtr = ((uint8_t *)memPtr) + length;
    while(ptr < endPtr) {
        auto entry = (rtc_memory_entry_header *)ptr;
        ptr += sizeof(*entry);
        if (ptr + entry->length > endPtr) {
            _debug_printf_P(PSTR("RTC memory: entry length exceeds total size. id %d, length %d\n"), entry->mem_id, entry->length);
            break;
        }
        if (entry->mem_id == id) {
            if (entry->length > maxSize) {
                entry->length = maxSize;
            }
            memcpy(dataPtr, ptr, entry->length);
            free(memPtr);
            return true;
        }
        ptr += entry->length;
    }
    free(memPtr);
    return false;
}

// write data to RTC memory
bool plugin_write_rtc_memory(uint8_t id, void *dataPtr, uint8_t dataLength) {

    _debug_printf_P(PSTR("plugin_write_rtc_memory(%d, %d)\n"), id, dataLength);

    if (id <= 0 || id > PLUGIN_RTC_MEM_MAX_ID) {
        _debug_printf_P(PSTR("plugin_read_rtc_memory(%d): invalid id\n"), id);
        return false;
    }
    // uint16_t block_count = maxSize / PLUGIN_RTC_MEM_ALIGNMENT;
    // if (block_count & ~0x4) {
    if (dataLength > PLUGIN_RTC_MEM_MAX_SIZE) {
        _debug_printf_P(PSTR("plugin_read_rtc_memory(%d, %d): max size exceeded\n"), id, dataLength);
        return false;
    }

    uint16_t length;
    Buffer newData;

    auto memPtr = __plugin_read_rtc_memory(length);
    if (memPtr) {
        // copy existing data
        auto ptr = (uint8_t *)memPtr;
        auto endPtr = ((uint8_t *)memPtr) + length;
        while(ptr < endPtr) {
            auto entry = (rtc_memory_entry_header *)ptr;
            if (!entry->mem_id) { // invalid id, probably aligned data
                break;
            }
            ptr += sizeof(*entry);
            if (entry->mem_id != id) {
                newData.write((const uint8_t *)entry, sizeof(*entry) + entry->length);
            }
            ptr += entry->length;
        }
        free(memPtr);
    }

    // append new data
    rtc_memory_entry_header newEntry;
    newData.reserve(newData.length() + sizeof(newEntry) + dataLength);
    newEntry.mem_id = id;
    newEntry.length = dataLength;

    newData.write((const uint8_t *)&newEntry, sizeof(newEntry));
    newData.write((const uint8_t *)dataPtr, dataLength);

    // align before adding header
    rtc_memory_header header;
    header.length = (uint16_t)newData.length();
    if (!__plugin_is_rtc_memory_header_aligned()) {
        uint8_t alignment = PLUGIN_RTC_MEM_BLK_SIZE - (header.length % PLUGIN_RTC_MEM_BLK_SIZE);
        while (alignment--) {
            header.length++;
            newData.write(0);
        }
    }

    // append header and align
    uint16_t crcPosition = (uint16_t)(newData.length() + sizeof(header) - sizeof(header.crc));
    newData.write((const uint8_t *)&header, sizeof(header));

    if ((sizeof(rtc_memory_header) & (PLUGIN_RTC_MEM_BLK_SIZE - 1)) != 0) {
        uint8_t alignment = PLUGIN_RTC_MEM_DATA_SIZE - sizeof(rtc_memory_header);
        while (alignment--) {
            newData.write(0);
        }
    }

    // update CRC in newData and store
    *(uint16_t *)(newData.get() + crcPosition) = crc16_calc(newData.get(), header.length + sizeof(header) - sizeof(header.crc));
    auto result = ESP.rtcUserMemoryWrite((PLUGIN_RTC_MEM_SIZE - newData.length()) / PLUGIN_RTC_MEM_BLK_SIZE, (uint32_t *)newData.get(), newData.length());

    //ESP.rtcMemDump();

    return result;
}

bool plugin_clear_rtc_memory() {
    uint8_t blocks = PLUGIN_RTC_MEM_DATA_BLK_SIZE;
    uint32_t offset = PLUGIN_RTC_MEM_ADDRESS;
    uint32_t data = 0;
    while (blocks--) {
        if (!ESP.rtcUserMemoryWrite(offset++, &data, sizeof(data))) {
            return false;
        }
    }
    return true;
}

#if DEBUG

#include <DumpBinary.h>

void plugin_debug_dump_rtc_memory(Print &output) {

    uint16_t length;
    auto memPtr = __plugin_read_rtc_memory(length);
    if (!memPtr) {
        debug_printf_P(PSTR("plugin_debug_dump_rtc_memory(): read returned nullptr\n"));
        return;
    }

    DumpBinary dumper(output);
    auto ptr = (uint8_t *)memPtr;
    auto endPtr = ((uint8_t *)memPtr) + length;
    while(ptr < endPtr) {
        auto entry = (rtc_memory_entry_header *)ptr;
        if (!entry->mem_id) {
            break;
        }
        ptr += sizeof(*entry);
        if (ptr + entry->length > endPtr) {
            _debug_printf_P(PSTR("RTC memory: entry length exceeds total size. id %d, block count %d\n"), entry->mem_id, entry->length);
            break;
        }
        auto plugin = get_plugin_by_rtc_memory_id(entry->mem_id);
        output.printf_P(PSTR("id: %d (%s), length %d "), entry->mem_id, plugin ? plugin->getPluginName().c_str() : String(F("<no plugin found>")).c_str(), entry->length);
        dumper.dump(ptr, entry->length);
        ptr += entry->length;
    }

    free(memPtr);
}
#endif
