/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationParameter.h"

#if DEBUG_CONFIGURATION_GETHANDLE

#include "JsonConfigReader.h"
#include <Buffer.h>
#include <JsonTools.h>
#include "misc.h"
#include "DumpBinary.h"
#include "DebugHandle.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(__DBG_config_handle_storage, "/.pvt/cfg_handles");
PROGMEM_STRING_DEF(__DBG_config_handle_log_file, "/.pvt/cfg_handles.log");

DebugHandle::DebugHandleVector *DebugHandle::_handles = nullptr;

class Cleanup {
public:
    Cleanup() {}
    ~Cleanup() {
        if (DebugHandle::_handles) {
            delete DebugHandle::_handles;
            DebugHandle::_handles = nullptr;
        }
    }
};
static Cleanup _cleanUp;

DebugHandle::DebugHandle(DebugHandle &&handle) noexcept : 
    _name(std::exchange(handle._name, nullptr)),
    _flashReadSize(std::exchange(handle._flashReadSize, 0)),
    _handle(std::exchange(handle._handle, 0)),
    _get(std::exchange(handle._get, 0)),
    _set(std::exchange(handle._set, 0)),
    _writeGet(std::exchange(handle._writeGet, 0)),
    _flashRead(std::exchange(handle._flashRead, 0)),
    _flashWrite(std::exchange(handle._flashWrite, 0))
{
}

#pragma push_macro("new")
#undef new

DebugHandle &DebugHandle::operator=(DebugHandle &&handle) noexcept 
{
    ::new(static_cast<void *>(this)) DebugHandle(std::move(handle));
    return *this;
}

#pragma pop_macro("new")

DebugHandle::DebugHandle() : 
    _name((char *)emptyString.c_str()), 
    _get(0),
    _flashReadSize(0),
    _handle(~0),
    _set(0), 
    _writeGet(0), 
    _flashRead(0), 
    _flashWrite(0) 
{
}

DebugHandle::DebugHandle(const char *name, const HandleType handle) : 
    _name(nullptr), 
    _get(0), 
    _flashReadSize(0),
    _handle(handle), 
    _set(0), 
    _writeGet(0), 
    _flashRead(0), 
    _flashWrite(0) 
{
    if (is_PGM_P(name)) {
        // we can just store the pointer if in PROGMEM
        _name = (char *)name;
    }
    else {
        _name = strdup_P(name);
        if (!_name) {
            _name = (char *)PSTR("out of memory");
        }
    }
}

DebugHandle::~DebugHandle() 
{
    if (_name && _name != emptyString.c_str() && !is_PGM_P(_name)) {
        __DBG_printf("free=%p", _name);
        free(_name);
    }
}

void DebugHandle::init()
{
    if (!_handles) {
        _handles = new DebugHandleVector();
        _handles->emplace_back(PSTR("<EEPROM>"), 0);
        _handles->emplace_back(PSTR("<INVALID>"), ~0);

#if DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL && !_MSC_VER
        _Scheduler.add(Event::minutes(DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL), true, [](Event::CallbackTimerPtr timer) {
            DebugHandle::logUsage();
        });
#endif
    }
}

void DebugHandle::clear()
{
    if (_handles) {
        delete _handles;
        _handles = nullptr;
        init();
    }
}

DebugHandle::DebugHandleVector &DebugHandle::getHandles()
{
    init();
    return *_handles;
}

void DebugHandle::print(Print &output) const 
{
    output.printf_P(PSTR("%04x: [get=%u,set=%u,write_get=%u,flash:read=#%u/%u,write=#%u/%u]: %s\n"), _handle, _get, _set, _writeGet, _flashRead, _flashReadSize, _writeGet, _writeGet * 4096, _name);
}

DebugHandle *DebugHandle::find(const char *name)
{
    if (_handles) {
        auto iterator = std::find(_handles->begin(), _handles->end(), name);
        if (iterator != _handles->end()) {
            return &(*iterator);
        }
    }
    return nullptr;
}

DebugHandle *DebugHandle::find(HandleType handle)
{
    if (_handles) {
        auto iterator = std::find(_handles->begin(), _handles->end(), handle);
        if (iterator != _handles->end()) {
            return &(*iterator);
        }
    }
    return nullptr;
}

void DebugHandle::add(const char *name, HandleType handle)
{
    init();
    __LDBG_printf("adding handle=%04x name=%s", handle, name);
    _handles->emplace_back(name, handle);
}

void DebugHandle::logUsage()
{
    auto uptime = getSystemUptime();
    __DBG_printf("writing usage to log uptime=%u file=%s", uptime, SPGM(__DBG_config_handle_log_file));
    auto file = KFCFS.open(FSPGM(__DBG_config_handle_log_file), fs::FileOpenMode::append);
    if (file) {
        PrintString str;
        str.strftime_P(SPGM(strftime_date_time_zone), time(nullptr));

        file.print(F("--- "));
        file.print(str);
        file.print(F(" uptime "));
        file.println(formatTime(uptime));
        printAll(file);
    }
}

void DebugHandle::printAll(Print &output)
{
    for (const auto &handle : getHandles()) {
        handle.print(output);
    }
}

// uint16_t getHandle(const char *name)
// {
//     Serial.printf("name=%s\n",name);
//     ConfigurationParameter::HandleType crc = constexpr_crc16_update(name, constexpr_strlen(name));
//     auto iterator = std::find(handles->begin(), handles->end(), crc);
//     if (iterator == handles->end()) {
//         handles->emplace_back(name, crc);
//     }
//     else if (*iterator != name) {
//         __DBG_panic("getHandle(%s): CRC not unique: %x, %s", name, crc, iterator->getName());
//     }
//     return crc;
// }


#endif
