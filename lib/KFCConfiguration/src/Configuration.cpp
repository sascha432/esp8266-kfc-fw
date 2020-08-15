/**
* Author: sascha_lammers@gmx.de
*/

#include "Configuration.h"
#include "JsonConfigReader.h"
#include <Buffer.h>
#include <JsonTools.h>
#include "misc.h"
#include "DumpBinary.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if DEBUG_CONFIGURATION_GETHANDLE

#include <EventScheduler.h>

#ifndef __DBG__TYPE_NONE
#define __DBG__TYPE_NONE                                            0
#define __DBG__TYPE_GET                                             1
#define __DBG__TYPE_SET                                             2
#define __DBG__TYPE_W_GET                                           3
#define __DBG__TYPE_CONST                                           4
#define __DBG__TYPE_DEFINE_PROGMEM                                  5
#endif

class DebugHandle;

class DebugHandle
{
public:
    using HandleType = ConfigurationHelper::HandleType;
    using DebugHandleVector = std::vector<DebugHandle>;

    DebugHandle(const DebugHandle &) = delete;
    DebugHandle(DebugHandle &&handle) {
        *this = std::move(handle);
    }

    DebugHandle &operator=(const DebugHandle &) = delete;
    DebugHandle &operator=(DebugHandle &&handle) {
        _name = handle._name;
        handle._name = nullptr;
        _handle = handle._handle;
        _get = handle._get;
        _set = handle._set;
        _writeGet = handle._writeGet;
        _flashRead = handle._flashRead;
        _flashReadSize = handle._flashReadSize;
        _flashWrite = handle._flashWrite;
        return *this;
    }

    DebugHandle() : _name((char *)emptyString.c_str()), _get(0), _handle(~0), _set(0), _writeGet(0), _flashRead(0), _flashReadSize(0), _flashWrite(0) {}

    DebugHandle(const char *name, const HandleType handle) :  _name(nullptr), _get(0), _handle(handle), _set(0), _writeGet(0), _flashRead(0), _flashReadSize(0), _flashWrite(0) {
        if (isProgmem(name)) {
            // we can just store the pointer if inside PROGMEM
            _name = (char *)name;
        }
        else {
            _name = (char *)malloc(strlen_P(name) + 1);
            if (_name) {
                strcpy_P(_name, name);
            }
            else {
                _name = (char *)PSTR("out of memory");
            }
        }
    }

    ~DebugHandle() {
        if (_name && _name != emptyString.c_str() && !is_PGM_P(_name)) {
            __DBG_printf("free=%p", _name);
            free(_name);
        }
    }

    operator HandleType() const {
        return _handle;
    }

    // bool operator==(const HandleType handle) const {
    //     return _handle == handle;
    // }
    // bool operator!=(const HandleType handle) const {
    //     return _handle != handle;
    // }

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

    void print(Print &output) const {
        output.printf_P(PSTR("%04x: [get=%u,set=%u,write_get=%u,flash:read=#%u/%u,write=#%u/%u]: %s\n"), _handle, _get, _set, _writeGet, _flashRead, _flashReadSize, _writeGet, _writeGet * 4096, _name);
    }

    static DebugHandle *find(const char *name);
    static DebugHandle *find(HandleType handle);
    static void add(const char *name, HandleType handle);
    static void init();
    static void clear();
    static void logUsage();
    static void printAll(Print &output);
    static DebugHandleVector &getHandles();

public:
    struct __attribute__packed__ {
        char *_name;
        uint32_t _get;
        HandleType _handle;
        uint16_t _set;
        uint16_t _writeGet;
        uint16_t _flashRead;
        uint32_t _flashReadSize;
        uint16_t _flashWrite;
    };
    static DebugHandleVector *_handles;
};

DebugHandle::DebugHandleVector *DebugHandle::_handles = nullptr;


void DebugHandle::init()
{
    if (!_handles) {
        _handles = new DebugHandleVector();
        _handles->emplace_back(PSTR("<EEPROM>"), 0);
        _handles->emplace_back(PSTR("<INVALID>"), ~0);
#if DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL
        Scheduler.addTimer(DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL * 60000UL, true, [](EventScheduler::TimerPtr) {
            DebugHandle::logUsage();
        });
#endif
    }
}

DebugHandle::DebugHandleVector &DebugHandle::getHandles()
{
    init();
    return *_handles;
}

void DebugHandle::clear()
{
    if (_handles) {
        delete _handles;
        _handles = nullptr;
        init();
    }
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

const ConfigurationHelper::HandleType ConfigurationHelper::registerHandleName(const char *name, uint8_t type)
{
    auto debugHandle = DebugHandle::find(name);
    if (debugHandle) {
        // found the name, just return handle
        if (type == __DBG__TYPE_GET) {
            debugHandle->_get++;
        }
        else if (type == __DBG__TYPE_SET) {
            debugHandle->_set++;
        }
        else if (type == __DBG__TYPE_W_GET) {
            debugHandle->_writeGet++;
        }
        return debugHandle->getHandle();
    }
    else {
        // get handle for name
        auto handle = crc16_update_P(name, strlen_P(name));
        // check if this handle exists already
        debugHandle = DebugHandle::find(handle);
        if (debugHandle) {
            __DBG_printf("handle name=%s handle=%04x matches name=%s", name, handle, debugHandle->getName());
            ::printf(PSTR("handle name=%s handle=%04x matches name=%s"), name, handle, debugHandle->getName());
        }
        DebugHandle::add(name, handle);
        return handle;
    }
}

static bool _panicMode = false;

void ConfigurationHelper::setPanicMode(bool value)
{
    _panicMode = value;
}

void ConfigurationHelper::addFlashUsage(HandleType handle, size_t readSize, size_t writeSize)
{
    DebugHandle::init();
    auto debugHandle = DebugHandle::find(handle);
    if (!debugHandle) {
        if (_panicMode) {
            writeHandles();
            DebugHandle::printAll(DEBUG_OUTPUT);
            __DBG_panic("handle=%04x not found read=%u write=%u size=%u", handle, readSize, writeSize, DebugHandle::getHandles().size());
        }
        else {
            __DBG_printf("handle=%04x not found read=%u write=%u size=%u", handle, readSize, writeSize, DebugHandle::getHandles().size());
        }
        debugHandle = DebugHandle::find(~0);
    }
    if (debugHandle) {
        if (readSize) {
            debugHandle->_flashRead++;
            debugHandle->_flashReadSize += readSize;
        }
        else if (writeSize) {
            debugHandle->_flashWrite++;
        }
    }
}

const ConfigurationHelper::HandleType ConfigurationHelper::registerHandleName(const __FlashStringHelper *name, uint8_t type)
{
    return registerHandleName(RFPSTR(name), type);
}

bool registerHandleExists(ConfigurationHelper::HandleType crc)
{
    return DebugHandle::find(crc) != nullptr;
}

static const char __DBG_config_handle_storage[] PROGMEM = { "/.pvt/cfg_handles" };
static const char __DBG_config_handle_log_file[] PROGMEM = { "/.pvt/cfg_handles.log" };

void DebugHandle::logUsage()
{
    auto uptime = getSystemUptime();
    __DBG_printf("writing usage to log uptime=%u file=%s", uptime, __DBG_config_handle_log_file);
    auto file = SPIFFS.open(FPSTR(__DBG_config_handle_log_file), fs::FileOpenMode::append);
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
    for(const auto &handle: getHandles()) {
        handle.print(output);
    }
}

void ConfigurationHelper::readHandles()
{
    SPIFFS.begin();
    auto file = SPIFFS.open(FPSTR(__DBG_config_handle_storage), fs::FileOpenMode::read);
    if (file) {
        uint32_t count = 0;
        while (file.available()) {
            auto line = file.readStringUntil('\n');
            if (String_rtrim(line)) {
                ConfigurationHelper::registerHandleName(line.c_str(), __DBG__TYPE_NONE);
                count++;
            }
        }
        file.close();
        __DBG_printf("read %u handles from=%s", count, __DBG_config_handle_storage);
    }
}


void ConfigurationHelper::writeHandles(bool clear)
{
    if (clear) {
        DebugHandle::clear();
    }
    __DBG_printf("storing %u handles file=%s", DebugHandle::getHandles().size(), __DBG_config_handle_storage);
    auto file = SPIFFS.open(FPSTR(__DBG_config_handle_storage), fs::FileOpenMode::write);
    if (file) {
        for (const auto &handle : DebugHandle::getHandles()) {
            file.printf_P(PSTR("%s\n"), handle.getName());
        }
        file.close();
    }
    DebugHandle::logUsage();
}

// uint16_t getHandle(const char *name)
// {
//     Serial.printf("name=%s\n",name);
//     ConfigurationParameter::Handle_t crc = constexpr_crc16_update(name, constexpr_strlen(name));
//     auto iterator = std::find(handles->begin(), handles->end(), crc);
//     if (iterator == handles->end()) {
//         handles->emplace_back(name, crc);
//     }
//     else if (*iterator != name) {
//         __DBG_panic("getHandle(%s): CRC not unique: %x, %s", name, crc, iterator->getName());
//     }
//     return crc;
// }

const char *ConfigurationHelper::getHandleName(ConfigurationParameter::Handle_t crc)
{
    auto debugHandle = DebugHandle::find(crc);
    if (debugHandle) {
        return debugHandle->getName();
    }
    return "<Unknown>";
}

void ConfigurationHelper::dumpHandles(Print &output, bool log)
{
    DebugHandle::printAll(output);
    if (log) {
        writeHandles();
    }
}


#else

const char *ConfigurationHelper::getHandleName(ConfigurationParameter::Handle_t crc)
{
    return "N/A";
}

#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

Configuration::Configuration(uint16_t offset, uint16_t size) : _offset(offset), _dataOffset(0), _size(size), _eeprom(false, 4096), _readAccess(0)
{
}

Configuration::~Configuration()
{
    clear();
}

void Configuration::clear()
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return;
    }
    __LDBG_printf("params=%u", _params.size());
    discard();
    _params.clear();
}

void Configuration::discard()
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return;
    }
    __LDBG_printf("params=%u", _params.size());
    for (auto &parameter : _params) {
        if (parameter.isDirty()) {
            free(parameter._info.data);
            parameter._info = ConfigurationParameter::Info_t();
        }
        else {
            parameter.__release(this);
        }
    }
    _eeprom.end();
    _storage.clear();
    _readAccess = 0;
}


void Configuration::release()
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return;
    }
    _dumpPool(_storage);
    __LDBG_printf("params=%u last_read=%d dirty=%d", _params.size(), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    for (auto &parameter : _params) {
        if (!parameter.isDirty()) {
            parameter.__release(this);
        }
    }
    _eeprom.end();
    _shrinkStorage();
    _readAccess = 0;
}

bool Configuration::read()
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return false;
    }
    clear();
#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::readHandles();
#endif
    auto result = _readParams();
    if (!result) {
        __LDBG_printf("readParams()=false");
        clear();
    }
    return result;
}

bool Configuration::write()
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return false;
    }
    __LDBG_printf("params=%u", _params.size());

    _eeprom.begin();

    bool dirty = false;
    for (auto &parameter : _params) {
        //if (parameter.isDirty()) {
            if (parameter.hasDataChanged(this)) {
                __LDBG_printf("%s dirty", parameter.toString().c_str())
                dirty = true;
                //break;
            }
        //}
    }
    if (dirty == false) {
        __LDBG_printf("configuration did not change");
        return read();
    }

    Buffer buffer; // storage for parameter information and data

    _storage.clear();

    // store current data offset
    uint16_t dataOffset = _dataOffset;
    for (auto &parameter : _params) {
        auto length = parameter._param.length;

        if (parameter.isDirty()) {
            if (parameter._param.isString()) {
                parameter.updateStringLength();
            }
            else { //if (parameter._param.isBinary()) {
                parameter._param.length = parameter._info.size;
            }
        }
        else {
            // update EEPROM location for unmodified parameters
            parameter._info.data = const_cast<uint8_t *>(_eeprom.getConstDataPtr()) + dataOffset;
        }

        // write parameter headers
        __LDBG_printf("write_header: %s ofs=%d prev_data_offset=%u", parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)), dataOffset);
        buffer.write(reinterpret_cast<const uint8_t *>(&parameter._param), sizeof(parameter._param));
        dataOffset += length;
    }

    // new data offset is base offset + size of header + size of stored parameter information
    _dataOffset = _offset + (uint16_t)(buffer.length() + sizeof(Header_t));
#if DEBUG_CONFIGURATION
    auto calcOfs = _offset + (sizeof(ConfigurationParameter::Param_t) * _params.size()) + sizeof(Header_t);
    if (_dataOffset != calcOfs) {
        __DBG_panic("real_ofs=%u ofs=%u", _dataOffset, calcOfs);

    }
#endif

    // write data
    for (auto &parameter : _params) {
        __LDBG_printf("write_data: %s ofs=%d %s", parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)), __debugDumper(parameter, parameter._info.data, parameter._param.length).c_str());
        buffer.write(parameter._info.data, parameter._param.length);
        if (parameter.isDirty()) {
            parameter._free();
        }
        else {
            parameter._info = ConfigurationParameter::Info_t();
        }
    }

    if (buffer.length() > _size) {
        __DBG_panic("size exceeded: %u > %u", buffer.length(), _size);
    }

    auto len = (uint16_t)buffer.length();
    auto eepromSize = _offset + len + sizeof(Header_t);
    _eeprom.begin(eepromSize); // update size

    auto headerPtr = _eeprom.getDataPtr() + _offset;
    auto &header = *reinterpret_cast<Header_t *>(headerPtr);
    // update header
    header = Header_t({ CONFIG_MAGIC_DWORD, crc16_update(buffer.get(), len), len, _params.size() });

    // update parameter info and data
    memcpy(headerPtr + sizeof(header), buffer.get(), len);

    __LDBG_printf("CRC %04x, length %d", header.crc, len);

    _eeprom.commit();

#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::writeHandles();
#endif

    // _eeprom.dump(Serial, false, _offset, 160);

    return true;
}

void Configuration::makeWriteable(ConfigurationParameter &param, uint16_t size)
{
    param._makeWriteable(this, size ? size : param._info.size);
}

const char *Configuration::getString(Handle_t handle)
{
    uint16_t offset;
    auto param = _findParam(ConfigurationParameter::STRING, handle, offset);
    if (param == _params.end()) {
        return emptyString.c_str();
    }
    return param->getString(this, offset);
}

char *Configuration::getWriteableString(Handle_t handle, uint16_t maxLength)
{
    auto &param = getWritableParameter<char *>(handle, maxLength);
    return reinterpret_cast<char *>(param._info.data);
}

const uint8_t *Configuration::getBinary(Handle_t handle, uint16_t &length)
{
    uint16_t offset;
    auto param = _findParam(ConfigurationParameter::BINARY, handle, offset);
    if (param == _params.end()) {
        return nullptr;
    }
    return param->getBinary(this, length, offset);
}

void *Configuration::getWriteableBinary(Handle_t handle, uint16_t length)
{
    auto &param = getWritableParameter<void *>(handle, length);
    return reinterpret_cast<void *>(param._info.data);
}

void Configuration::setString(Handle_t handle, const char *string, uint16_t length)
{
    uint16_t offset;
    auto &param = _getOrCreateParam(ConfigurationParameter::STRING, handle, offset);
    param.setData(this, (const uint8_t *)string, length);
}

void Configuration::setBinary(Handle_t handle, const void *data, uint16_t length)
{
    uint16_t offset;
    auto &param = _getOrCreateParam(ConfigurationParameter::BINARY, handle, offset);
    param.setData(this, (const uint8_t *)data, length);
}

void Configuration::dump(Print &output, bool dirty, const String &name)
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return;
    }
    output.printf_P(PSTR("Configuration:\nofs=%d base_ofs=%d eeprom_size=%d params=%d len=%d\n"), _dataOffset, _offset, _eeprom.getSize(), _params.size(), _eeprom.getSize() - _offset);
    output.printf_P(PSTR("min_mem_usage=%d header_size=%d Param_t::size=%d, ConfigurationParameter::size=%d, Configuration::size=%d\n"), sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter), sizeof(Configuration::Header_t), sizeof(ConfigurationParameter::Param_t), sizeof(ConfigurationParameter), sizeof(Configuration));

    uint16_t offset = _dataOffset;
    for (auto &parameter : _params) {
        DEBUG_HELPER_SILENT();
        auto display = true;
        auto &param = parameter._param;
        if (name.length()) {
            if (!name.equals(ConfigurationHelper::getHandleName(param.handle))) {
                display = false;
            }
        }
        else if (dirty) {
            if (!parameter._info.dirty) {
                display = false;
            }
        }
        auto length = parameter.read(this, offset);
        DEBUG_HELPER_INIT();
        if (display) {
#if DEBUG_CONFIGURATION_GETHANDLE
            output.printf_P(PSTR("%s[%04x]: "), ConfigurationHelper::getHandleName(param.handle), param.handle);
#else
            output.printf_P(PSTR("%04x: "), param.handle);
#endif
            output.printf_P(PSTR("type=%s ofs=%d size=%d dirty=%u value: "), (const char *)parameter.getTypeString(parameter._param.getType()), offset, /*calculateOffset(param.handle),*/ length, parameter._info.dirty);
            parameter.dump(output);
        }
        offset += param.length;
    }
}

bool Configuration::isDirty() const
{
    for(auto &param: _params) {
        if (param.isDirty()) {
            return true;
        }
    }
    return false;
}

void Configuration::_writeAllocate(ConfigurationParameter &param, uint16_t size)
{
    //param._param.length = size;
    param._info.size = param._param.getSize(size);
    param._info.data = reinterpret_cast<uint8_t*>(calloc(param._info.size, 1));
    __LDBG_printf("calloc %s", param.toString().c_str());
}

uint8_t *Configuration::_allocate(uint16_t size, PoolVector *poolVector)
{
    if (!poolVector) {
        poolVector = &_storage;
    }
    Pool *poolPtr = nullptr;
    if (size < CONFIG_POOL_MAX_SIZE) {
        poolPtr = _findPool(size, poolVector);
        if (!poolPtr) {
            auto POOL_COUNT = poolVector->size() + 1;
            uint16_t size = (uint16_t)CONFIG_POOL_SIZE;
            poolVector->emplace_back(size);
            poolPtr = &poolVector->back();
            poolPtr->init();
        }
        poolPtr = _findPool(size, poolVector);
    }
    if (!poolPtr) {
        poolVector->emplace_back(size);
        poolPtr = &poolVector->back();
        poolPtr->init();
    }
    return poolPtr->allocate(size);
}

void Configuration::__release(const void *ptr)
{
    if (ptr) {
        auto pool = _getPool(ptr);
        if (pool) {
            pool->release(ptr);
        }
    }
}

Configuration::Pool *Configuration::_getPool(const void *ptr)
{
    if (ptr) {
        for (auto &pool: _storage) {
            if (pool.hasPtr(ptr)) {
                return &pool;
            }
        }
    }
    return nullptr;
}

Configuration::Pool *Configuration::_findPool(uint16_t length, PoolVector *poolVector) const
{
    for (auto &pool : *poolVector) {
        if (pool.space(length)) {
            return &pool;
        }
    }
    return nullptr;
}

void Configuration::_shrinkStorage()
{
    PoolVector newPool;
    for (auto &param: _params) {
        if (param._info.data && param._info.dirty == 0) {
            param._info.data = reinterpret_cast<uint8_t *>(memcpy(_allocate(param.getSize(), &newPool), param._info.data, param.getLength()));
        }
    }
    _storage = std::move(newPool);
    _dumpPool(_storage);
}

#if DEBUG_CONFIGURATION

void Configuration::_dumpPool(PoolVector &poolVector)
{
    PrintString str;
    size_t total = 0;
    for (const auto &pool : poolVector) {
        if (pool.getPtr()) {
            total += pool.size();
            str.printf_P(PSTR("[%u:%u:#%u] "), pool.size(), pool.available(), pool.count());
        }
        else {
            str.printf_P(PSTR("[%u:null] "), pool.size());
        }
    }
    size_t count_dirty = 0;
    size_t total_dirty = 0;
    for (const auto& param : _params) {
        if (param._info.dirty) {
            count_dirty++;
            total_dirty += param._info.size;
        }
    }
    debug_printf_P(PSTR("%ssize=%u[#%u] dirty=%u[#%u]\n"), str.c_str(), total, poolVector.size(), total_dirty, count_dirty);
}

#endif

void Configuration::exportAsJson(Print &output, const String &version)
{
    output.printf_P(PSTR("{\n\t\"magic\": \"%#08x\",\n\t\"version\": \"%s\",\n"), CONFIG_MAGIC_DWORD, version.c_str());
    output.print(F("\t\"config\": {\n"));

    uint16_t dataOffset = _dataOffset;
    for (auto &parameter : _params) {
        auto &param = parameter._param;

        if (dataOffset != _dataOffset) {
            output.print(F(",\n"));
        }

        output.printf_P(PSTR("\t\t\"%#04x\": {\n"), param.handle);
#if DEBUG_CONFIGURATION_GETHANDLE
        output.print(F("\t\t\t\"name\": \""));
        auto name = ConfigurationHelper::getHandleName(param.handle);
        JsonTools::printToEscaped(output, name, strlen(name), false);
        output.print(F("\",\n"));
#endif

        auto length = parameter.read(this, dataOffset);
        output.printf_P(PSTR("\t\t\t\"type\": %d,\n"), parameter._param.getType());
        output.printf_P(PSTR("\t\t\t\"type_name\": \"%s\",\n"), (const char *)parameter.getTypeString(parameter._param.getType()));
        output.printf_P(PSTR("\t\t\t\"length\": %d,\n"), length);
        output.print(F("\t\t\t\"data\": "));
        parameter.exportAsJson(output);
        output.print(F("\n"));
        dataOffset += param.length;

        output.print(F("\t\t}"));
    }

    output.print(F("\n\t}\n}\n"));
}

bool Configuration::importJson(Stream &stream, uint16_t *handles)
{
    JsonConfigReader reader(&stream, *this, handles);
    reader.initParser();
    return reader.parseStream();
}

void Configuration::__crashCallback()
{
    __LDBG_printf("clearing memory");
    _offset = kInvalidOffset;
    static_cast<ConfigurationHelper::EEPROMClassEx &>(EEPROM).clearAndEnd();
    _params.clear();
    _storage.clear();
    __LDBG_printf("done");
}

Configuration::ParameterList::iterator Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset)
{
    offset = _dataOffset;
    if (type == ConfigurationParameter::_ANY) {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle) {
                //__LDBG_printf("%s FOUND", it->toString().c_str());
                return it;
            }
            offset += it->_param.length;
        }
    }
    else {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle && it->_param.type == type) {
                //__LDBG_printf("%s FOUND", it->toString().c_str());
                return it;
            }
            offset += it->_param.length;
        }
    }
    __LDBG_printf("handle=%s[%04x] type=%s = NOT FOUND", ConfigurationHelper::getHandleName(handle), handle, (const char *)ConfigurationParameter::getTypeString(type));
    return _params.end();
}

ConfigurationParameter &Configuration::_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset)
{
    auto iterator = _findParam(ConfigurationParameter::_ANY, handle, offset);
    if (iterator == _params.end()) {
        _params.emplace_back(handle, type);
        __LDBG_printf("new param %s", _params.back().toString().c_str());
        __DBG__checkIfHandleExists("create", handle);
        return _params.back();
    }
    else if (type != iterator->_param.getType()) {
        __DBG_panic("%s: new_type=%s type=%s different", ConfigurationHelper::getHandleName(iterator->_param.handle), iterator->toString().c_str(), (const char *)ConfigurationParameter::getTypeString(type));
    }
    __DBG__checkIfHandleExists("find", iterator->_param.handle);
    return *iterator;
}

uint16_t Configuration::_readHeader(uint16_t offset, HeaderAligned_t &hdr)
{
#if defined(ESP8266)
    //|| defined(ESP32)
        // read header directly from flash since we do not know the size of the configuration
    _eeprom.read(hdr.headerBuffer, (uint16_t)offset, (uint16_t)sizeof(hdr.header), (uint16_t)sizeof(hdr.headerBuffer));
#else
    _eeprom.begin(_offset + sizeof(hdr.header));
    _eeprom.get(offset, hdr.header);
    _eeprom.end();
#endif
    return offset + sizeof(hdr.header);
}

bool Configuration::_readParams()
{
    if (_offset == kInvalidOffset) {
        __LDBG_printf("invalid offset");
        return false;
    }
    uint16_t offset = _offset;
    HeaderAligned_t hdr;

    _eeprom.end();
    offset = _readHeader(offset, hdr);

#if DEBUG_CONFIGURATION
    DumpBinary dump(F("Header:"), DEBUG_OUTPUT);
    dump.dump((const uint8_t *)&hdr, sizeof(hdr.header));
#endif

    for (;;) {

        if (hdr.header.magic != CONFIG_MAGIC_DWORD) {
            __LDBG_printf("invalid magic %08x", hdr.header.magic);
#if DEBUG_CONFIGURATION
            _eeprom.dump(Serial, false, _offset, 160);
#endif
            break;
        }
        else if (hdr.header.crc == -1) {
            __LDBG_printf("invalid CRC %04x", hdr.header.crc);
            break;
        }
        else if (hdr.header.length == 0 || hdr.header.length > _size - sizeof(hdr.header)) {
            __LDBG_printf("invalid length %d", hdr.header.length);
            break;
        }

        // now we know the required size, initialize EEPROM
        _eeprom.begin(_offset + sizeof(hdr.header) + hdr.header.length);

        uint16_t crc = crc16_update(_eeprom.getConstDataPtr() + _offset + sizeof(hdr.header), hdr.header.length);
        if (hdr.header.crc != crc) {
            __LDBG_printf("CRC mismatch %04x != %04x", crc, hdr.header.crc);
            break;
        }

        _dataOffset = (sizeof(ConfigurationParameter::Param_t) * hdr.header.params) + offset;
        auto dataOffset = _dataOffset;

        for(;;) {

            ConfigurationParameter::Param_t param;
            param.type = ConfigurationParameter::_INVALID;
            _eeprom.get(offset, param);
            if (param.type == ConfigurationParameter::_INVALID) {
                __LDBG_printf("invalid type");
                break;
            }
            offset += sizeof(param);

            _params.emplace_back(param);
            dataOffset += param.length;

            if (_params.size() == hdr.header.params) {
                _eeprom.end();
                return true;
            }
        }
        __LDBG_printf("failure before reading all parameters %d/%d", _params.size(), hdr.header.params);
        break;

    }

    clear();
    _eeprom.end();
    return false;
}

#if DEBUG_CONFIGURATION

uint16_t Configuration::calculateOffset(Handle_t handle) const
{
    uint16_t offset = _dataOffset;
    for (auto &parameter: _params) {
        if (parameter.getHandle() == handle) {
            return offset;
        }
        offset += parameter._param.length;
    }
    return 0;
}

String Configuration::__debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len, bool progmem)
{
    PrintString str = F("data");
progmem=true;// treat everything as PROGMEM
    if (param._param.isString()) {
        str.printf_P(PSTR("[%u]='"), len);
        if (progmem) {
            auto ptr = data;
            while (len--) {
                str += (char)pgm_read_byte(ptr++);
            }
        }
        else {
            auto ptr = data;
            while (len--) {
                str += (char)*ptr++;
            }
        }
        str += '\'';
    }
    else {
        str += '=';
        DumpBinary dump(str);
        dump.setPerLine((uint8_t)len);
        if (progmem) {
            auto pPtr = malloc(len);
            memcpy_P(pPtr, data, len);
            dump.dump((uint8_t *)pPtr, len);
            free(pPtr);
        }
        else {
            dump.dump(data, len);
        }
        str.rtrim();
    }
    return str;
}

#endif
