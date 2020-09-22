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
        if (is_PGM_P(name)) {
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
    struct {
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
        _Scheduler.add(Event::minutes(DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL), true, [](Event::CallbackTimerPtr timer) {
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
        String tmp = FPSTR(name);
        auto handle = crc16_update(tmp.c_str(), tmp.length());
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
    auto file = KFCFS.open(FPSTR(__DBG_config_handle_log_file), fs::FileOpenMode::append);
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
    KFCFS.begin();
    auto file = KFCFS.open(FPSTR(__DBG_config_handle_storage), fs::FileOpenMode::read);
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
    auto file = KFCFS.open(FPSTR(__DBG_config_handle_storage), fs::FileOpenMode::write);
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

const char *ConfigurationHelper::getHandleName(ConfigurationParameter::HandleType crc)
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

const char *ConfigurationHelper::getHandleName(ConfigurationParameter::HandleType crc)
{
    return "N/A";
}

#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

Configuration::Configuration(uint16_t offset, uint16_t size) :
    _eeprom(false, 4096),
    _readAccess(0),
    _offset(offset),
    _dataOffset(offset + sizeof(Header_t)),
    _size(size)
{
}

Configuration::~Configuration()
{
    clear();
}

void Configuration::clear()
{
    __LDBG_printf("params=%u", _params.size());
    _params.clear();
    _eeprom.discard();
    _dataOffset = _offset + sizeof(Header_t);
}

void Configuration::discard()
{
    __LDBG_printf("discard params=%u", _params.size());
    std::for_each(_params.begin(), _params.end(), [](ConfigurationParameter &parameter) {
        ConfigurationHelper::_allocator.deallocate(parameter);
    });
    ConfigurationHelper::_allocator.release();
    _eeprom.discard();
    _readAccess = 0;
}

void Configuration::release()
{
    __LDBG_printf("params=%u last_read=%d dirty=%d", _params.size(), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    std::for_each(_params.begin(), _params.end(), [this](ConfigurationParameter &parameter) {
        if (!parameter.isWriteable()) {
            ConfigurationHelper::_allocator.deallocate(parameter);
        }
        else if (parameter.hasDataChanged(*this) == false) {
            ConfigurationHelper::_allocator.deallocate(parameter);
        }
    });
    _eeprom.discard();
    _readAccess = 0;
}

bool Configuration::read()
{
    __LDBG_printf("params=%u", _params.size());
    clear();
#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::readHandles();
#endif
    if (!_readParams()) {
        __LDBG_printf("readParams()=false");
        clear();
        return false;
    }
    return true;
}

bool Configuration::write()
{
    __LDBG_printf("params=%u", _params.size());

    _eeprom.begin();

    bool dirty = false;
    for (auto &parameter : _params) {
        if (parameter.hasDataChanged(*this)) {
            dirty = true;
            break;
        }
    }
    if (dirty == false) {
        __LDBG_printf("configuration did not change");
        return read();
    }

    Buffer buffer; // storage for parameter information and data
    uint16_t dataOffset = _offset + sizeof(Header_t);

    for (auto &parameter : _params) {
        // create copy
        auto param = parameter._getParam();
        if (parameter._getParam().isWriteable()) {
            // update length and remove writeable flag
            __LDBG_printf("writable: len=%u next_offset=%u", param._writeable->length(),  parameter._getParam().next_offset());
            param._length = param._writeable->length();
            param._is_writeable = false;
        }
        // write parameter headers
        __LDBG_printf("write_header: %s ofs=%d", parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)));
        buffer.push_back(param._header);
        dataOffset += parameter._getParam().next_offset() ? sizeof(ParameterHeaderType) : 0;
    }

    __LDBG_assert_printf(dataOffset == _dataOffset, "wrong data_offset=%u _data_offset=%u", dataOffset, _dataOffset);

    // new data offset is base offset + size of header + size of stored parameter information
    _dataOffset = _offset + (uint16_t)(buffer.length() + sizeof(Header_t));
#if DEBUG_CONFIGURATION
    auto calcOfs = _offset + (sizeof(ParameterHeaderType) * _params.size()) + sizeof(Header_t);
    if (_dataOffset != calcOfs) {
        __DBG_panic("real_ofs=%u ofs=%u", _dataOffset, calcOfs);

    }
#endif

    // write data
    for (auto &parameter : _params) {
        const auto &param = parameter._getParam();
        __LDBG_printf("write_data: %s ofs=%d %s", parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)), __debugDumper(parameter, parameter._getParam().data(), parameter._param.length()).c_str());
        if (param.isWriteable()) {
            // write new data
            buffer.write(param._writeable->begin(), param._writeable->length());
        }
        else {
            // data did not change, read from eeprom into buffer
            buffer.write(_eeprom.getConstDataPtr() + dataOffset, param.next_offset());
        }
        dataOffset += param.next_offset();
        ConfigurationHelper::_allocator.deallocate(parameter);
        parameter._getParam() = {};
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

    _params.clear();
    ConfigurationHelper::_allocator.release();
    _dataOffset = _offset + sizeof(Header_t);

    return _readParams();
}

void Configuration::makeWriteable(ConfigurationParameter &param, size_type length)
{
    param._makeWriteable(*this, length);
}

const char *Configuration::getString(HandleType handle)
{
    __LDBG_printf("handle=%04x", handle);
    uint16_t offset;
    auto param = _findParam(ParameterType::STRING, handle, offset);
    if (param == _params.end()) {
        return emptyString.c_str();
    }
    return param->getString(*this, offset);
}

char *Configuration::getWriteableString(HandleType handle, size_type maxLength)
{
    __LDBG_printf("handle=%04x max_len=%u", handle, maxLength);
    auto &param = getWritableParameter<char *>(handle, maxLength);
    return param._getParam().string();
}

const uint8_t *Configuration::getBinary(HandleType handle, size_type &length)
{
    __LDBG_printf("handle=%04x", handle);
    uint16_t offset;
    auto param = _findParam(ParameterType::BINARY, handle, offset);
    if (param == _params.end()) {
        return nullptr;
    }
    return param->getBinary(*this, length, offset);
}

void *Configuration::getWriteableBinary(HandleType handle, size_type length)
{
    __LDBG_printf("handle=%04x len=%u", handle, length);
    auto &param = getWritableParameter<void *>(handle, length);
    return param._getParam().data();
}

void Configuration::_setString(HandleType handle, const char *string, size_type length, size_type maxLength)
{
    __LDBG_printf("handle=%04x length=%u max_len=%u", handle, length, maxLength);
    if (maxLength != (size_type)~0U) {
        if (length >= maxLength - 1) {
            length = maxLength - 1;
        }
    }
    _setString(handle, string, length);;
}

void Configuration::_setString(HandleType handle, const char *string, size_type length)
{
    __LDBG_printf("handle=%04x length=%u", handle, length);
    uint16_t offset;
    auto &param = _getOrCreateParam(ParameterType::STRING, handle, offset);
    param.setData(*this, (const uint8_t *)string, length);
}

void Configuration::setBinary(HandleType handle, const void *data, size_type length)
{
    __LDBG_printf("handle=%04x data=%p length=%u", handle, data, length);
    uint16_t offset;
    auto &param = _getOrCreateParam(ParameterType::BINARY, handle, offset);
    param.setData(*this, (const uint8_t *)data, length);
}

void Configuration::dump(Print &output, bool dirty, const String &name)
{
    output.printf_P(PSTR("Configuration:\nofs=%d base_ofs=%d eeprom_size=%d params=%d len=%d\n"), _dataOffset, _offset, _eeprom.getSize(), _params.size(), _eeprom.getSize() - _offset);
    output.printf_P(PSTR("min_mem_usage=%d header_size=%d Param_t::size=%d, ConfigurationParameter::size=%d, Configuration::size=%d\n"), sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter), sizeof(Configuration::Header_t), sizeof(ConfigurationParameter::Param_t), sizeof(ConfigurationParameter), sizeof(Configuration));

    uint16_t dataOffset = (uint16_t)(_offset + sizeof(Header_t) + (_params.size() * sizeof(ParameterHeaderType)));
    output.printf_P(PSTR("_dataOffset=%u _offset=%u dataOffset=%u\n"), _dataOffset, _offset, dataOffset);
    for (auto &parameter : _params) {
        DEBUG_HELPER_SILENT();
        auto display = true;
        auto &param = parameter._param;
        if (name.length()) {
            if (!name.equals(ConfigurationHelper::getHandleName(param.getHandle()))) {
                display = false;
            }
        }
        else if (dirty) {
            if (!parameter.isWriteable()) {
                display = false;
            }
        }
        //auto length =
        parameter.read(*this, dataOffset);
        DEBUG_HELPER_INIT();
        if (display) {
#if DEBUG_CONFIGURATION_GETHANDLE
            output.printf_P(PSTR("%s[%04x]: "), ConfigurationHelper::getHandleName(param.getHandle()), param.getHandle());
#else
            output.printf_P(PSTR("%04x: "), param.getHandle());
#endif
            output.printf_P(PSTR("type=%s ofs=%d len=%d dirty=%u value: "), (const char *)parameter.getTypeString(parameter.getType()), param.next_offset() ? dataOffset : -1, /*calculateOffset(param.handle),*/ parameter.getLength(), parameter.isWriteable());
            parameter.dump(output);
        }
        dataOffset += param.next_offset();
    }
}

bool Configuration::isDirty() const
{
    for(auto &param: _params) {
        if (param.isWriteable()) {
            return true;
        }
    }
    return false;
}

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

        output.printf_P(PSTR("\t\t\"%#04x\": {\n"), param.getHandle());
#if DEBUG_CONFIGURATION_GETHANDLE
        output.print(F("\t\t\t\"name\": \""));
        auto name = ConfigurationHelper::getHandleName(param.getHandle());
        JsonTools::printToEscaped(output, name, strlen(name), false);
        output.print(F("\",\n"));
#endif

        auto length = parameter.read(*this, dataOffset);
        output.printf_P(PSTR("\t\t\t\"type\": %d,\n"), parameter.getType());
        output.printf_P(PSTR("\t\t\t\"type_name\": \"%s\",\n"), (const char *)parameter.getTypeString(parameter.getType()));
        output.printf_P(PSTR("\t\t\t\"length\": %d,\n"), length);
        output.print(F("\t\t\t\"data\": "));
        parameter.exportAsJson(output);
        output.print(F("\n"));
        dataOffset += param.next_offset();

        output.print(F("\t\t}"));
    }

    output.print(F("\n\t}\n}\n"));
}

bool Configuration::importJson(Stream &stream, HandleType *handles)
{
    JsonConfigReader reader(&stream, *this, handles);
    reader.initParser();
    return reader.parseStream();
}

Configuration::ParameterList::iterator Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset)
{
    offset = _dataOffset;
    if (type == ParameterType::_ANY) {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle) {
                //__LDBG_printf("%s FOUND", it->toString().c_str());
                return it;
            }
            offset += it->_param.next_offset();
        }
    }
    else {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle && it->_param.type() == type) {
                //__LDBG_printf("%s FOUND", it->toString().c_str());
                return it;
            }
            offset += it->_param.next_offset();
        }
    }
    __LDBG_printf("handle=%s[%04x] type=%s = NOT FOUND", ConfigurationHelper::getHandleName(handle), handle, (const char *)ConfigurationParameter::getTypeString(type));
    return _params.end();
}

ConfigurationParameter &Configuration::_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset)
{
    auto iterator = _findParam(ParameterType::_ANY, handle, offset);
    if (iterator == _params.end()) {
        _params.emplace_back(handle, type);
        __LDBG_printf("new param %s", _params.back().toString().c_str());
        __DBG__checkIfHandleExists("create", handle);
        return _params.back();
    }
    else if (type != iterator->_param.type()) {
        __DBG_panic("%s: new_type=%s type=%s different", ConfigurationHelper::getHandleName(iterator->getHandle()), iterator->toString().c_str(), (const char *)ConfigurationParameter::getTypeString(type));
    }
    __DBG__checkIfHandleExists("find", iterator->getHandle());
    return *iterator;
}

uint16_t Configuration::_readHeader(uint16_t offset, HeaderAligned_t &hdr)
{
    _eeprom.read(hdr.headerBuffer, (uint16_t)offset, (uint16_t)sizeof(hdr.header), (uint16_t)sizeof(hdr.headerBuffer));
    return offset + sizeof(hdr.header);
}

bool Configuration::_readParams()
{
    uint16_t offset = _offset;
    HeaderAligned_t hdr;

    _eeprom.discard();
    offset = _readHeader(offset, hdr);
    __LDBG_printf("offset=%u params_ofs=%u", _offset, offset);

#if DEBUG_CONFIGURATION
    DumpBinary dump(F("Header:"), DEBUG_OUTPUT);
    dump.dump((const uint8_t *)&hdr, sizeof(hdr.header));
#endif

    while(true) {

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

        //offset += (sizeof(param._header) * hdr.header.params);
        _dataOffset =  offset + (sizeof(ParameterHeaderType) * hdr.header.params);

        for(size_t i = 0; i <= hdr.header.params; i++) {

            if (_params.size() == hdr.header.params) {
                __LDBG_printf("_dataOffset=%u params=%u", _dataOffset, _params.size());
                __LDBG_assert_printf(offset == _dataOffset, "offset=%u mismatch _dataOffset=%u", offset, _dataOffset);
                _eeprom.discard();
                return true;
            }

            ParameterInfo param;
            if (
                (_eeprom.read((uint8_t *)&param._header, offset, sizeof(param._header), sizeof(param._header)) != sizeof(param._header)) ||
                (param.type() == ParameterType::_INVALID)
            ) {
                __LDBG_assert_printf(false, "read error type=%u", param.type());
                break;
            }
            __LDBG_assert_panic(param.isWriteable() == false, "writeable");

            offset += sizeof(param._header);
            _params.emplace_back(param._header);
            if (_params.size() != i + 1) {
                __LDBG_assert_printf(false, "emplace_back failed");
                break;
            }
        }

        break;
    }

    clear();
    return false;
}

#if DEBUG_CONFIGURATION

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
