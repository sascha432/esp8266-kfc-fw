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

#if DEBUG_GETHANDLE

class DebugHandle_t
{
public:
    DebugHandle_t() = default;

    DebugHandle_t(const String& name, const ConfigurationParameter::Handle_t handle) : _handle(handle), _name(name) {
    }

    bool operator==(const ConfigurationParameter::Handle_t handle) const {
        return _handle == handle;
    }
    bool operator!=(const ConfigurationParameter::Handle_t handle) const {
        return !(*this == handle);
    }

    bool operator==(const char* name) const {
        return _name.equals(name);
    }
    bool operator!=(const char* name) const {
        return !(*this == name);
    }

    const char* getName() const {
        return _name.c_str();
    }

private:
    friend void readHandles();
    friend void writeHandles();

    ConfigurationParameter::Handle_t _handle;
    String _name;
};

static std::vector<DebugHandle_t> handles;


void readHandles()
{
    File file = SPIFFS.open(F("config_debug"), fs::FileOpenMode::read);
    if (file) {
        while (file.available()) {
            auto line = file.readStringUntil('\n');
            if (String_rtrim(line)) {
                getHandle(line.c_str());
            }

        }
        file.close();
    }
}


void writeHandles()
{
    File file = SPIFFS.open(F("config_debug"), fs::FileOpenMode::write);
    if (file) {
        for (auto &handle : handles) {
            file.println(handle._name);
        }
        file.close();
    }
}

uint16_t getHandle(const char *name)
{
    ConfigurationParameter::Handle_t crc = constexpr_crc16_update(name, constexpr_strlen(name));
    auto iterator = std::find(handles.begin(), handles.end(), crc);
    if (iterator == handles.end()) {
        handles.emplace_back(name, crc);
    }
    else if (*iterator != name) {
        __debugbreak_and_panic_printf_P(PSTR("getHandle(%s): CRC not unique: %x, %s\n"), name, crc, iterator->getName());
    }
    return crc;
}

const char *getHandleName(ConfigurationParameter::Handle_t crc)
{
    auto iterator = std::find(handles.begin(), handles.end(), crc);
    if (iterator == handles.end()) {
        return "<Unknown>";
    }
    return iterator->getName();
}
#else
const char *getHandleName(ConfigurationParameter::Handle_t crc) {
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
    _debug_printf_P(PSTR("params=%u\n"), _params.size());
    discard();
    _params.clear();
}

void Configuration::discard()
{
    _debug_printf_P(PSTR("params=%u\n"), _params.size());
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
    _dumpPool(_storage);
    _debug_printf_P(PSTR("params=%u last_read=%d dirty=%d\n"), _params.size(), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
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
    clear();
#if DEBUG_GETHANDLE
    readHandles();
#endif
    auto result = _readParams();
    if (!result) {
        _debug_printf_P(PSTR("readParams()=false\n"));
        clear();
    }
    return result;
}

bool Configuration::write()
{
    _debug_printf_P(PSTR("params=%u\n"), _params.size());

    Buffer buffer; // storage for parameter information and data

    _storage.clear();
    _eeprom.begin();

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
        _debug_printf_P(PSTR("write_header: %s ofs=%d prev_data_offset=%u\n"), parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)), dataOffset);
        buffer.write(reinterpret_cast<const uint8_t *>(&parameter._param), sizeof(parameter._param));
        dataOffset += length;
    }

    // new data offset is base offset + size of header + size of stored parameter information
    _dataOffset = _offset + (uint16_t)(buffer.length() + sizeof(Header_t));
#if DEBUG_CONFIGURATION
    auto calcOfs = _offset + (sizeof(ConfigurationParameter::Param_t) * _params.size()) + sizeof(Header_t);
    if (_dataOffset != calcOfs) {
        __debugbreak_and_panic_printf_P(PSTR("real_ofs=%u ofs=%u\n"), _dataOffset, calcOfs);

    }
#endif

    // write data
    for (auto &parameter : _params) {
        _debug_printf_P(PSTR("write_data: %s ofs=%d %s\n"), parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)), __debugDumper(parameter, parameter._info.data, parameter._param.length).c_str());
        buffer.write(parameter._info.data, parameter._param.length);
        if (parameter.isDirty()) {
            parameter._free();
        }
        else {
            parameter._info = ConfigurationParameter::Info_t();
        }
    }

    if (buffer.length() > _size) {
        __debugbreak_and_panic_printf_P(PSTR("size exceeded: %u > %u\n"), buffer.length(), _size);
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

    _debug_printf_P(PSTR("CRC %04x, length %d\n"), header.crc, len);

    _eeprom.commit();

#if DEBUG_GETHANDLE
    writeHandles();
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
    output.printf_P(PSTR("Configuration:\nofs=%d base_ofs=%d eeprom_size=%d params=%d len=%d\n"), _dataOffset, _offset, _eeprom.getSize(), _params.size(), _eeprom.getSize() - _offset);
    output.printf_P(PSTR("min_mem_usage=%d header_size=%d Param_t::size=%d, ConfigurationParameter::size=%d, Configuration::size=%d\n"), sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter), sizeof(Configuration::Header_t), sizeof(ConfigurationParameter::Param_t), sizeof(ConfigurationParameter), sizeof(Configuration));

    uint16_t offset = _dataOffset;
    for (auto &parameter : _params) {
        DEBUG_HELPER_SILENT();
        auto display = true;
        auto &param = parameter._param;
        if (name.length()) {
            if (!name.equals(getHandleName(param.handle))) {
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
#if DEBUG_GETHANDLE
            output.printf_P(PSTR("%s[%04x]: "), getHandleName(param.handle), param.handle);
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
    _debug_printf_P(PSTR("calloc %s\n"), param.toString().c_str());
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
#if DEBUG_GETHANDLE
        output.print(F("\t\t\t\"name\": \""));
        auto name = getHandleName(param.handle);
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

Configuration::ParameterList::iterator Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset)
{
    offset = _dataOffset;
    if (type == ConfigurationParameter::_ANY) {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle) {
                //_debug_printf_P(PSTR("%s FOUND\n"), it->toString().c_str());
                return it;
            }
            offset += it->_param.length;
        }
    }
    else {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle && it->_param.type == type) {
                //_debug_printf_P(PSTR("%s FOUND\n"), it->toString().c_str());
                return it;
            }
            offset += it->_param.length;
        }
    }
    _debug_printf_P(PSTR("handle=%s[%04x] type=%s = NOT FOUND\n"), getHandleName(handle), handle, (const char *)ConfigurationParameter::getTypeString(type));
    return _params.end();
}

ConfigurationParameter &Configuration::_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset)
{
    auto iterator = _findParam(ConfigurationParameter::_ANY, handle, offset);
    if (iterator == _params.end()) {
        _params.emplace_back(handle, type);
        _debug_printf_P(PSTR("new param %s\n"), _params.back().toString().c_str());
        return _params.back();
    }
    else if (type != iterator->_param.getType()) {
        __debugbreak_and_panic_printf_P(PSTR("%s: new_type=%s type=%s different\n"), getHandleName(iterator->_param.handle), iterator->toString().c_str(), (const char *)ConfigurationParameter::getTypeString(type));
    }
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
            _debug_printf_P(PSTR("invalid magic %08x\n"), hdr.header.magic);
#if DEBUG_CONFIGURATION
            _eeprom.dump(Serial, false, _offset, 160);
#endif
            break;
        }
        else if (hdr.header.crc == -1) {
            _debug_printf_P(PSTR("invalid CRC %04x\n"), hdr.header.crc);
            break;
        }
        else if (hdr.header.length == 0 || hdr.header.length > _size - sizeof(hdr.header)) {
            _debug_printf_P(PSTR("invalid length %d\n"), hdr.header.length);
            break;
        }

        // now we know the required size, initialize EEPROM
        _eeprom.begin(_offset + sizeof(hdr.header) + hdr.header.length);

        uint16_t crc = crc16_update(_eeprom.getConstDataPtr() + _offset + sizeof(hdr.header), hdr.header.length);
        if (hdr.header.crc != crc) {
            _debug_printf_P(PSTR("CRC mismatch %04x != %04x\n"), crc, hdr.header.crc);
            break;
        }

        _dataOffset = (sizeof(ConfigurationParameter::Param_t) * hdr.header.params) + offset;
        auto dataOffset = _dataOffset;

        for(;;) {

            ConfigurationParameter::Param_t param;
            param.type = ConfigurationParameter::_INVALID;
            _eeprom.get(offset, param);
            if (param.type == ConfigurationParameter::_INVALID) {
                _debug_printf_P(PSTR("invalid type\n"));
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
        _debug_printf_P(PSTR("failure before reading all parameters %d/%d\n"), _params.size(), hdr.header.params);
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
