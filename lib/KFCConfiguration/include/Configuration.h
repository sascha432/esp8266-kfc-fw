/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <crc16.h>
#include <Buffer.h>
#include <DumpBinary.h>
#include <JsonTools.h>
#include <type_traits>
#include <list>
#include <vector>
#include <stl_ext/chunked_list.h>
#include <stl_ext/is_trivially_copyable.h>
#if ESP8266
#include <coredecls.h>
#endif
#if ESP32 || HAVE_NVS_FLASH
#include <nvs.h>
#endif

#include "ConfigurationHelper.h"
#include "ConfigurationParameter.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

namespace ConfigurationHelper {

    inline uint32_t getFlashAddress() {
        return SECTION_EEPROM_START_ADDRESS - SECTION_FLASH_START_ADDRESS;
    }

    inline uint32_t getFlashAddress(size_t offset) {
        return getFlashAddress() + offset;
    }

    inline uint32_t getOffsetFromFlashAddress(uint32_t address) {
        return address - getFlashAddress();
    }

    struct Header {
        Header() :
            _magic(CONFIG_MAGIC_DWORD),
            _crc(~0U)
        {
        }

        Header(uint32_t version, uint16_t length, uint16_t numParams) :
            _magic(CONFIG_MAGIC_DWORD),
            _crc(~0U),
            _data(version, length, numParams)
        {
        }

        // length does not include the Header
        uint16_t length() const {
            return _data.length;
        }

        uint16_t numParams() const {
            return _data.params;
        }

        uint32_t &version() {
            return _data.version;
        }

        void update(uint32_t version, uint16_t numParams, uint16_t length) {
            _data.version = version;
            _data.params = numParams;
            _data.length = length;
        }

        // update crc from data passed
        void calcCrc(const uint8_t *buffer, size_t len) {
            _crc = crc32(buffer, len, _data.crc());
        }

        // calculate crc of header
        uint32_t initCrc() const {
            return _data.crc();
        }

        uint16_t getParamsLength() const {
            return sizeof(ParameterHeaderType) * numParams();
        };

        static uint16_t getParamsLength(uint16_t num) {
            return sizeof(ParameterHeaderType) * num;
        };

        // compare magic and crc
        bool validateCrc(uint32_t crc) const {
            return (CONFIG_MAGIC_DWORD == _magic) && (_crc == crc);
        }

        // check if the length fits into the sector
        bool isLengthValid(size_t maxSize) const{
            return (_data.length >= sizeof(*this) && _data.length < (maxSize - sizeof(*this)));
        }

        uint32_t magic() const {
            return _magic;
        }

        uint32_t crc() const {
            return _crc;
        }

        operator bool() const {
            return CONFIG_MAGIC_DWORD == _magic;
        }

        #if ESP8266

            explicit operator uint8_t *() {
                return reinterpret_cast<uint8_t *>(this);
            }

            operator uint32_t *() {
                return reinterpret_cast<uint32_t *>(this);
            }

        #elif ESP32

            operator void *() {
                return reinterpret_cast<void *>(this);
            }

            operator const void *() const {
                return reinterpret_cast<const void *>(this);
            }

        #endif

        void setMagic() {
            _magic = CONFIG_MAGIC_DWORD;
        }

    private:
        uint32_t _magic;
        uint32_t _crc;
        struct Data {
            uint32_t version;
            uint16_t length;
            uint16_t params;


            Data() : version(0), length(0), params(0) {}
            Data(uint32_t _version, uint16_t _length, uint16_t _params) : version(_version), length(_length), params(_params) {}

            uint32_t crc() const {
                return crc32(this, sizeof(*this));
            }
        } _data;
    };

    static_assert((sizeof(Header) & 3) == 0, "not dword aligned");

}

class Configuration {
public:
    using Handle_t = ConfigurationHelper::HandleType;
    using HandleType = ConfigurationHelper::HandleType;
    using TypeEnum_t = ConfigurationHelper::ParameterType;
    using ParameterType = ConfigurationHelper::ParameterType;
    using Param_t = ConfigurationHelper::ParameterInfo;
    using ParameterInfo = ConfigurationHelper::ParameterInfo;
    using ParameterHeaderType = ConfigurationHelper::ParameterHeaderType;
    using size_type = ConfigurationHelper::size_type;
    using Header = ConfigurationHelper::Header;

    // iterators must not change when the list is modified
    using ParameterList = std::list<ConfigurationParameter>;

    // using ParameterList = stdex::chunked_list<ConfigurationParameter, 6>;
    // static constexpr size_t ParameterListChunkSize = ParameterList::chunk_element_count * 8 + sizeof(uint32_t); ; //ParameterList::chunk_size;

    // Offset 0
    //  some data...
    // kHeaderOffset
    //  Header
    // kParamsOffset
    //  param 1 (ParameterHeaderType)
    //  param 2 ...
    // getDataOffset(numParam)
    //  data record 1
    //  data record 2 ...

    #define CONFIGURATION_HEADER_OFFSET 0

    static constexpr uint16_t kHeaderOffset = CONFIGURATION_HEADER_OFFSET;
    static_assert((kHeaderOffset & 3) == 0, "not dword aligned");
    #if ESP32 || HAVE_NVS_FLASH
        static_assert(kHeaderOffset == 0, "offset not supported");
    #endif

    static constexpr uint16_t kParamsOffset = kHeaderOffset + sizeof(Header);
    static_assert((kParamsOffset & 3) == 0, "not dword aligned");

    static constexpr uint16_t getDataOffset(uint16_t numParams) {
        return kParamsOffset + (sizeof(ParameterHeaderType) * numParams);
    }

    enum class WriteResultType {
        SUCCESS = 0,
        READING_HEADER_FAILED,
        READING_CONF_FAILED,
        OUT_OF_MEMORY,
        READING_PREV_CONF_FAILED,
        MAX_SIZE_EXCEEDED,
        #if ESP8266
            FLASH_READ_ERROR,
            FLASH_ERASE_ERROR,
            FLASH_WRITE_ERROR,
        #endif
        #if ESP32 || HAVE_NVS_FLASH
            NVS_COMMIT_ERROR,
            NVS_SET_BLOB_ERROR,
            NVS_ERASE_ALL,
        #endif
    };

    static const __FlashStringHelper *getWriteResultTypeStr(WriteResultType result) {
        switch(result) {
            case WriteResultType::SUCCESS:
                return F("SUCCESS");
            case WriteResultType::READING_HEADER_FAILED:
                return F("READING_HEADER_FAILED");
            case WriteResultType::READING_CONF_FAILED:
                return F("READING_CONF_FAILED");
            case WriteResultType::OUT_OF_MEMORY:
                return F("OUT_OF_MEMORY");
            case WriteResultType::READING_PREV_CONF_FAILED:
                return F("READING_PREV_CONF_FAILED");
            case WriteResultType::MAX_SIZE_EXCEEDED:
                return F("MAX_SIZE_EXCEEDED");
            #if ESP8266
                case WriteResultType::FLASH_READ_ERROR:
                    return F("FLASH_READ_ERROR");
                case WriteResultType::FLASH_ERASE_ERROR:
                    return F("FLASH_ERASE_ERROR");
                case WriteResultType::FLASH_WRITE_ERROR:
                    return F("FLASH_WRITE_ERROR");
            #endif
            #if ESP32 || HAVE_NVS_FLASH
                case WriteResultType::NVS_COMMIT_ERROR:
                    return F("NVS_COMMIT_ERROR");
                case WriteResultType::NVS_SET_BLOB_ERROR:
                    return F("NVS_SET_BLOB_ERROR");
                case WriteResultType::NVS_ERASE_ALL:
                    return F("NVS_ERASE_ALL");
            #endif
            // default:
            //     break;
        }
        // this is causing a memory leak for invalid result codes, which should not occur
        auto str = new String();
        *str = F("INVALID RESULT #");
        *str += String(static_cast<int>(result));
        return reinterpret_cast<const __FlashStringHelper *>(str->c_str());
    }


public:
    Configuration(uint16_t size);
    ~Configuration();

    // clear data and parameters
    void clear();

    // free data and discard modifications
    void discard();

    // free data and keep modifications
    void release();

    // read data from EEPROM
    bool read();

    // erase NVS data or EEPROM
    WriteResultType erase();

    // write data to EEPROM
    WriteResultType write();

    template <typename _Ta>
    ConfigurationParameter &getWritableParameter(HandleType handle, size_type maxLength = sizeof(_Ta)) {
        __LDBG_printf("handle=%04x max_len=%u", handle, maxLength);
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        if (param._param.isString()) {
            param.getString(*this, offset);
        }
        else {
            size_type length;
            auto ptr = param.getBinary(*this, length, offset);
            if (ptr) {
                __DBG_assert_printf(length == maxLength, "%04x: resizing binary blob=%u to %u maxLength=%u type=%u", handle, length, sizeof(_Ta), maxLength, ConfigurationParameter::getType<_Ta>());
            }
        }
        makeWriteable(param, maxLength);
        return param;
    }

    template <typename _Ta>
    ConfigurationParameter *getParameter(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        size_type offset;
        auto iterator = _findParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        if (iterator == _params.end()) {
            return nullptr;
        }
        return &(*iterator);
    }

    void makeWriteable(ConfigurationParameter &param, size_type length);

    const char *getString(HandleType handle);
    char *getWriteableString(HandleType handle, size_type maxLength);

    // uint8_t for compatibility
    const uint8_t *getBinary(HandleType handle, size_type &length);
    const void *getBinaryV(HandleType handle, size_type &length) {
        return reinterpret_cast<const void *>(getBinary(handle, length));
    }
    void *getWriteableBinary(HandleType handle, size_type length);

    // PROGMEM safe
    inline void setString(HandleType handle, const char *str, size_type length, size_type maxLength) {
        _setString(handle, str, length, maxLength);
    }

    inline void setString(HandleType handle, const char *str, size_type maxLength) {
        if (!str) {
            _setString(handle, emptyString.c_str(), 0);
            return;
        }
        _setString(handle, str, (size_type)strlen_P(str), maxLength);
    }

    inline void setString(HandleType handle, const char *str) {
        if (!str) {
            _setString(handle, emptyString.c_str(), 0);
            return;
        }
        _setString(handle, str, (size_type)strlen_P(str));
    }

    inline void setString(HandleType handle, const __FlashStringHelper *fstr, size_type maxLength) {
        setString(handle, RFPSTR(fstr), maxLength);
    }

    inline void setString(HandleType handle, const __FlashStringHelper *fstr) {
        setString(handle, RFPSTR(fstr));
    }

    inline void setString(HandleType handle, const String &str, size_type maxLength) {
        _setString(handle, str.c_str(), (size_type)str.length(), maxLength);
    }

    inline void setString(HandleType handle, const String &str) {
        _setString(handle, str.c_str(), (size_type)str.length());
    }

    // PROGMEM safe
    void setBinary(HandleType handle, const void *data, size_type length);

    bool getBool(HandleType handle) {
        return get<uint8_t>(handle) ? true : false;
    }

    void setBool(HandleType handle, const bool flag) {
        set<bool>(handle, flag ? true : false);
    }

    template <typename _Ta>
    bool exists(HandleType handle) {
        uint16_t offset;
        return _findParam(ConfigurationParameter::getType<_Ta>(), handle, offset) != _params.end();
    }

    template <typename _Ta>
    // static_assert(std::is_<>);
    const _Ta get(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        uint16_t offset;
        auto param = _findParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        if (param == _params.end()) {
            return _Ta();
        }
        size_type length;
        auto ptr = reinterpret_cast<const _Ta *>(param->getBinary(*this, length, offset));
        if (!ptr || length != sizeof(_Ta)) {
            #if DEBUG_CONFIGURATION
                if (ptr && length != sizeof(_Ta)) {
                    __LDBG_printf("size does not match, type=%s handle %04x (%s)",
                        (const char *)ConfigurationParameter::getTypeString(param->getType()),
                        handle,
                        ConfigurationHelper::getHandleName(handle)
                    );
                }
            #endif
            return _Ta();
        }
        return *ptr;
    }

    template <typename _Ta>
    _Ta &getWriteable(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        auto &param = getWritableParameter<_Ta>(handle);
        return *reinterpret_cast<_Ta *>(param._getParam().data());
    }

    template <typename _Ta>
    const _Ta &set(HandleType handle, const _Ta &data) {
        __LDBG_printf("handle=%04x data=%p len=%u", handle, std::addressof(data), sizeof(_Ta));
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        param.setData(*this, (const uint8_t *)&data, (size_type)sizeof(_Ta));
        return data;
    }

    void dump(Print &output, bool dirty = false, const String &name = String());
    bool isDirty() const;

    void exportAsJson(Print& output, const String &version);
    bool importJson(Stream& stream, HandleType *handles = nullptr);

private:
    friend ConfigurationParameter;

private:
    void _setString(HandleType handle, const char *str, size_type length);
    void _setString(HandleType handle, const char *str, size_type length, size_type maxLength);

    // find a parameter, type can be _ANY or a specific type
    ParameterList::iterator _findParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset);
    ConfigurationParameter &_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset);

    // read parameter headers
    bool _readParams();

    // ------------------------------------------------------------------------
    // EEPROM

    #if ESP8266

        // Flash implementation

        bool flashWrite(uint32_t offset, const uint8_t *data, size_t size)
        {
            return ESP.flashWrite(offset, data, size);
        }

        bool flashRead(uint32_t offset, uint8_t *data, size_t size)
        {
            return ESP.flashRead(offset, data, size);
        }

        bool flashWrite(uint32_t offset, const uint32_t *data, size_t size)
        {
            return ESP.flashWrite(offset, const_cast<uint32_t *>(data), size);
        }

        bool flashRead(uint32_t offset, uint32_t *data, size_t size)
        {
            return ESP.flashRead(offset, data, size);
        }

        bool flashEraseSector(uint32_t sector)
        {
            return ESP.flashEraseSector(sector);
        }

    #elif ESP32 || HAVE_NVS_FLASH

        // NVS implementation
        static uint32_t _nvs_key_handle(ConfigurationParameter::TypeEnum_t type, HandleType handle) {
            return (static_cast<uint32_t>(type) << 16) | handle;
        }

        static char *_nvs_key_handle_name(ConfigurationParameter::TypeEnum_t type, HandleType handle) {
            static char buffer[9];
            snprintf_P(buffer, sizeof(buffer), PSTR("%08x"), _nvs_key_handle(type, handle));
            return buffer;
        }

        void _nvs_open() {
            if (_handle) {
                return;
            }
            esp_err_t err = nvs_open(_name, NVS_READWRITE, &_handle);
            if (err != ESP_OK) {
                __DBG_printf_E("cannot open NVS name=%s err=%08x", _name, err);
            }
            else {
                __LDBG_printf_N("NVS name=%s handle=%08x", _name, _handle);
            }
        }

        nvs_handle _handle; // NV handle
        const char *_name; // NVS namespace

    #endif

protected:
    ParameterList _params;
    uint32_t _readAccess;
    uint16_t _size;

// ------------------------------------------------------------------------
// last access

public:
    // return time of last readData call that allocated memory
    unsigned long getLastReadAccess() const {
        return _readAccess;
    }

    void setLastReadAccess() {
        _readAccess = millis();
    }

// ------------------------------------------------------------------------
// DEBUG

    static String __debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len);

    static String __debugDumper(ConfigurationParameter &param, const __FlashStringHelper *data, size_t len) {
        return __debugDumper(param, reinterpret_cast<const uint8_t *>(data), len);
    }

};

#if _MSC_VER
#pragma warning(pop)
#endif

#include <debug_helper_disable.h>
