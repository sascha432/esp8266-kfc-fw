/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if DEBUG_CONFIGURATION_GETHANDLE

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "ConfigurationHelper.h"

PROGMEM_STRING_DECL(__DBG_config_handle_storage);
PROGMEM_STRING_DECL(__DBG_config_handle_log_file);

#define __DBG__TYPE_NONE                                            0
#define __DBG__TYPE_GET                                             1
#define __DBG__TYPE_SET                                             2
#define __DBG__TYPE_W_GET                                           3
#define __DBG__TYPE_CONST                                           4
#define __DBG__TYPE_DEFINE_PROGMEM                                  5

class DebugHandle
{
public:
    using HandleType = ConfigurationHelper::HandleType;
    using DebugHandleVector = std::vector<DebugHandle>;

    DebugHandle(const DebugHandle &) = delete;
    DebugHandle(DebugHandle &&handle) noexcept;

    DebugHandle &operator=(const DebugHandle &) = delete;
    DebugHandle &operator=(DebugHandle &&handle) noexcept;

    DebugHandle();

    DebugHandle(const char *name, const HandleType handle);

    ~DebugHandle();

    operator HandleType() const {
        return _handle;
    }

    bool operator==(const char *name) const {
        return strcmp_P_P(_name, name) == 0;
    }

    bool operator!=(const char *name) const {
        return strcmp_P_P(_name, name) != 0;
    }

    const char *getName() const {
        return _name;
    }

    const HandleType getHandle() const {
        return _handle;
    }

    bool equals(const char *name) const {
        return strcmp_P_P(_name, name) == 0;
    }

    void print(Print &output) const;

    static DebugHandle *find(const char *name);
    static DebugHandle *find(HandleType handle);
    static void add(const char *name, HandleType handle);
    static void init();
    static void clear();
    static void logUsage();
    static void printAll(Print &output);
    static DebugHandleVector &getHandles();

public:
    char *_name;
    uint32_t _get;
    uint32_t _flashReadSize;
    HandleType _handle;
    uint16_t _set;
    uint16_t _writeGet;
    uint16_t _flashRead;
    uint16_t _flashWrite;

    static DebugHandleVector *_handles;
};

#include <debug_helper_disable.h>

#endif
