/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EEPROM.h>
#include <type_traits>
#include <algorithm>
#include <stl_ext/type_traits.h>

#ifndef DEBUG_CONFIGURATION
#define DEBUG_CONFIGURATION                 1
#endif

#ifndef DEBUG_CONFIGURATION_GETHANDLE
#define DEBUG_CONFIGURATION_GETHANDLE       1
//#define DEBUG_CONFIGURATION_GETHANDLE       0
#endif

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class Buffer;
class Configuration;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

class ConfigurationParameter;

namespace ConfigurationHelper {

    class Allocator {
    public:
        static constexpr uint8_t kDefaultAlignment = 0;
        static constexpr uint8_t kAllocBlockSize = (1 << 3) - 1;            // match mallocs 8 byte block size

    public:
        Allocator() {}
        ~Allocator() {
            release();
        }

        inline uint8_t *allocate(size_t size, size_t *realSize, uint8_t alignment = kDefaultAlignment) {
            __LDBG_assert_printf(size != 0, "allocating size=%u", size);
            size = std::max(1U, size);
            auto ptr = (uint32_t *)__LDBG_malloc(size = _get_aligned_size(size, alignment));
            if (realSize) {
                *realSize = size;
            }
            static_assert(kAllocBlockSize == 3 || kAllocBlockSize == 7 || kAllocBlockSize == 15, "cannot fill as uint32_t");
            std::fill_n(ptr, size / sizeof(uint32_t), 0);
            return (uint8_t *)ptr;
        }

        inline uint8_t *allocate(size_t size, uint8_t alignment = kDefaultAlignment) {
            return allocate(size, nullptr, alignment);
        }

        inline void deallocate(void *ptr) {
            __LDBG_free(ptr);
        }

        inline void allocate(size_t size, ConfigurationParameter &parameter, uint8_t alignment = kDefaultAlignment);

        inline void deallocate(ConfigurationParameter &parameter);

        inline void release() {
        }

    private:
        inline size_t _get_aligned_size(size_t size, uint8_t alignment) const {
            uint8_t tmp;
            if (alignment > (kAllocBlockSize + 1) && (tmp = (size % alignment))) {
                size += alignment - tmp;
            }
            return (size + kAllocBlockSize) & ~kAllocBlockSize;
        }
    };

    // direct access to EEPROM if supported
    class EEPROMAccess {
    public:
        static constexpr uint16_t kTempReadBufferSize = 128;

        EEPROMAccess(bool isInitialized, uint16_t size) :
            _size(size),
            _isInitialized(isInitialized)
        {
            //#if defined(ESP32)
            //            _partition = nullptr;
            //#endif
        }

        operator bool() const {
            return _isInitialized;
        }

        uint16_t getSize() const {
            return _size;
        }

        void begin(uint16_t size = 0);

        // end() has auto commit
        void end();

        // discard = end() without auto commit()
        void discard();

        void commit();

        uint16_t getReadSize(uint16_t offset, uint16_t length) const;

        uint16_t read(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t maxSize);

        void dump(Print &output, bool asByteArray = true, uint16_t offset = 0, uint16_t length = 0);

        uint8_t *getDataPtr() {
#if DEBUG_CONFIGURATION
            if (!_isInitialized) {
                __DBG_panic("EEPROM is not initialized");
            }
#endif
#if _MSC_VER
            return const_cast<uint8_t *>(EEPROM.getConstDataPtr());
#else
            return EEPROM.getDataPtr();
#endif
        }

        const uint8_t *getConstDataPtr() const {
#if DEBUG_CONFIGURATION
            if (!_isInitialized) {
                __DBG_panic("EEPROM is not initialized");
            }
#endif
#if defined(ESP32)
            return EEPROM.getDataPtr();
#else
            return EEPROM.getConstDataPtr();
#endif
        }

        template<typename T>
        inline T &get(int const address, T &t) {
#if DEBUG_CONFIGURATION
            if (!_isInitialized) {
                __DBG_panic("EEPROM is not initialized");
            }
            if (address + sizeof(T) > _size) {
                __DBG_panic("address=%u size=%u eeprom_size=%u", address, sizeof(T), _size);
            }
#endif
            return EEPROM.get(address, t);
        }

    private:
        uint16_t _size : 15;
        uint16_t _isInitialized : 1;

        //#if defined(ESP32)
 //       const esp_partition_t *_partition;
 //#endif
    };

}

namespace ConfigurationHelper {

    using HandleType = uint16_t;
    using Handle_t = HandleType;
    using size_type = uint16_t;
    using ParameterHeaderType = uint32_t;

    class ParameterInfo;
    class Allocator;
    class WriteableData;

    extern Allocator _allocator;

    enum class ParameterType : uint8_t { // 4 bit, 0-13 available as type
        _INVALID = 0,
        STRING,
        BINARY,
        BYTE,
        WORD,
        DWORD,
        QWORD,
        FLOAT,
        DOUBLE,
        MAX,
        _ANY = 0b1111,
    };
    using TypeEnum_t = ParameterType;

    static_assert((int)ParameterType::MAX < 15, "size exceeded");

    size_type getParameterLength(ParameterType type, size_t length = 0);

    class WriteableData {
    public:

        WriteableData(size_type length, ConfigurationParameter &parameter, Configuration &conf);
        ~WriteableData();

        void resize(size_type length, ConfigurationParameter &parameter, Configuration &conf);

        size_type size() const {
            return _length + _is_string;
        }

        size_type length() const {
            return _length;
        }

        uint8_t *data() {
            return size() <= sizeof(_buffer) ? _buffer : _data;
        }

        const uint8_t *data() const {
            return size() <= sizeof(_buffer) ? _buffer : _data;
        }

        const uint8_t *begin() const {
            return data();
        }

        uint8_t *begin() {
            return data();
        }

        const uint8_t *end() const {
            return data() + size();
        }

        uint8_t *end() {
            return data() + size();
        }

        void setData(uint8_t *ptr, size_type length);

        struct {
            union {
                uint8_t *_data;
                uint8_t _buffer[12];
            };
            uint16_t _length: 11;
            uint16_t _is_string : 1;
        };

    };

    static constexpr size_t WriteableDataSize = sizeof(WriteableData);

    class ParameterInfo {
    public:
        ParameterInfo() : _header(0), _readable(nullptr) {
        }

        ParameterInfo(HandleType handle, ParameterType type) : _handle(handle), _type(static_cast<uint16_t>(type)), _length(0), _is_writeable(false), _readable(nullptr) {
        }

        ParameterInfo(ParameterHeaderType header) : _header(header), _readable(nullptr) {
        }

        inline uint8_t *data() {
            return _is_writeable ? _writeable->data() : _readable;
        }

        inline const uint8_t *data() const {
            return _is_writeable ? _writeable->data() : _readable;
        }

        inline const char *string() const {
            return (const char *)data();
        }
        inline char *string() {
            return (char *)data();
        }

        inline ParameterType type() const {
            return static_cast<ParameterType>(_type);
        }

        inline bool hasData() const {
            return _readable != nullptr;
        }

        inline bool isWriteable() const {
            __LDBG_assert_printf((_is_writeable == true && _writeable) || (_is_writeable == false), "is_writeable=%u _writeable=%p", _is_writeable, _writeable);
            return _is_writeable;
        }

        void resizeWriteable(size_type length, ConfigurationParameter &param, Configuration &conf) {
            __LDBG_assert_printf(_is_writeable == true, "is_writeable=%u", _is_writeable);
            _writeable->resize(length, param, conf);
        }

        void setWriteable(WriteableData *writeable) {
            __LDBG_assert_printf(_is_writeable == false, "is_writeable=%u writeable=%p _writeable=%p", _is_writeable, writeable, _writeable);
            _is_writeable = true;
            _writeable = writeable;
        }

        void setReadable(void *ptr) {
            __LDBG_assert_printf(_is_writeable == false, "is_writeable=%u", _is_writeable);
            _readable = (uint8_t *)ptr;
        }

        inline bool isString() const {
            return type() == ParameterType::STRING;
        }

        inline bool isBinary() const {
            return type() == ParameterType::BINARY;
        }

        // size of data(), for STRING it includes the NULL byte = strlen(data()) + 1
        inline size_type size() const {
            return _is_writeable ? sizeOf(_writeable->length()) : sizeOf(_length);
        };

        // size of data(), for STRING it is strlen(data())
        // returns 0 if it has no data
        inline size_type length() const {
            return _is_writeable ? _writeable->length() : _readable ? _length : 0;
        }

        // required capacity for length
        inline size_type sizeOf(size_type length) const {
            return type() == ParameterType::STRING ? length + 1 : length;
        };

        // return length of data stored in eeprom
        inline size_type next_offset() const {
            return _length;
        }

        inline void setHandle(HandleType handle) {
            _handle = handle;
        }

        inline void setHandle(int handle) {
            _handle = static_cast<HandleType>(handle);
        }

        inline HandleType getHandle() const {
            return _handle;
        }


    public:
        struct {
            union {
                struct {
                    HandleType _handle;
                    uint16_t _type : 4;                              // type of data
                    uint16_t _length : 11;                           // stores the length of the data currently stored in the EEPROM
                    uint16_t _is_writeable: 1;
                };
                ParameterHeaderType _header;
            };
            union {
                uint8_t *_readable;
                WriteableData *_writeable;
            };
        };
    };

    static constexpr size_t ParameterInfoSize = sizeof(ParameterInfo);
    // static_assert(ParameterInfoSize == 8, "invalid size");

}

class ConfigurationParameter {
public:
    using Handle_t = ConfigurationHelper::HandleType;
    using HandleType = ConfigurationHelper::HandleType;
    using TypeEnum_t = ConfigurationHelper::ParameterType;
    using ParameterType = ConfigurationHelper::ParameterType;
    using Param_t = ConfigurationHelper::ParameterInfo;
    using ParameterInfo = ConfigurationHelper::ParameterInfo;
    using WriteableData = ConfigurationHelper::WriteableData;
    using ParameterHeaderType = ConfigurationHelper::ParameterHeaderType;
    using size_type = ConfigurationHelper::size_type;


    ConfigurationParameter(const ConfigurationParameter &) = delete;
    ConfigurationParameter &operator=(const ConfigurationParameter &) = delete;

    ConfigurationParameter(const ParameterInfo &param) : _param(param) {}

    ConfigurationParameter(HandleType handle, ParameterType type) : _param(handle, type) {}

    ConfigurationParameter(ParameterHeaderType header) : _param(header) {}

    ~ConfigurationParameter() {
        if (_param.isWriteable()) {
            delete _param._writeable;
        }
        else if (_param._readable) {
            ConfigurationHelper::_allocator.deallocate(_param._readable);
        }
    }

    bool operator==(const HandleType handle) const {
        return _param.getHandle() == handle;
    }

    template <typename _Ta>
    static ParameterType constexpr getType() {
        return
            (std::is_same<_Ta, char *>::value ? ParameterType::STRING :
                ((std::is_same<_Ta, uint8_t *>::value || std::is_same<_Ta, void *>::value) ? ParameterType::BINARY :
                    (std::is_same<_Ta, float>::value ? ParameterType::FLOAT :
                        (std::is_same<_Ta, double>::value ? ParameterType::DOUBLE :
                            ((std::is_same<_Ta, char>::value || std::is_same<_Ta, signed char>::value || std::is_same<_Ta, unsigned char>::value) ? ParameterType::BYTE :
                                ((std::is_same<_Ta, int16_t>::value || std::is_same<_Ta, uint16_t>::value) ? ParameterType::WORD :
                                    ((std::is_same<_Ta, int32_t>::value || std::is_same<_Ta, uint32_t>::value) ? ParameterType::DWORD :
                                        ((std::is_same<_Ta, int64_t>::value || std::is_same<_Ta, uint64_t>::value) ? ParameterType::QWORD :
                                            ParameterType::BINARY
            ))))))));
    }

    // PROGMEM safe
    void setData(Configuration &conf, const uint8_t *data, size_type length);

    const char *getString(Configuration &conf, uint16_t offset);

    const uint8_t *getBinary(Configuration &conf, size_type &length, uint16_t offset);

    uint16_t read(Configuration &conf, uint16_t offset);

    void dump(Print &output);
    void exportAsJson(Print& output);

    String toString() const;

    inline Handle_t getHandle() const {
        return _param.getHandle();
    }

    inline ParameterType getType() const {
        return _param.type();
    }

    inline uint16_t getLength() const {
        return _param.length();
    }

    inline bool hasData() const {
        return _param.hasData();
    }

    inline bool isWriteable() const {
        return _param.isWriteable();
    }

    // compare data with EEPROM content if isWriteable()
    bool hasDataChanged(Configuration &conf) const;

    static const __FlashStringHelper *getTypeString(ParameterType type);

    inline static PGM_P getTypeString_P(ParameterType type) {
        return reinterpret_cast<PGM_P>(getTypeString(type));
    }


private:
    friend Configuration;
    friend WriteableData;

    bool _readData(Configuration &conf, uint16_t offset);
    bool _readDataTo(Configuration &conf, uint16_t offset, uint8_t *ptr, size_type maxSize) const;
    void _makeWriteable(Configuration &conf, size_type length);

    // PROGMEM safe
    bool _compareData(const uint8_t *data, size_type length) const;

protected:
    ParameterInfo _param;

public:
    inline ParameterInfo &_getParam() {
        return _param;
    }
};

template<class T>
class ConfigurationParameterT : public ConfigurationParameter
{
public:
    using ConfigurationParameter::ConfigurationParameter;

    operator bool() const {
        return _param.hasData();
    }

    T &get() {
        return *reinterpret_cast<T *>(_param.data());
    }
    void set(const T &value) {
        __LDBG_assert_panic(_param.isWriteable(), "not writable");
        *reinterpret_cast<T *>(_param._writeable->begin()) = value;
    }
};

template<>
class ConfigurationParameterT <char *> : public ConfigurationParameter
{
public:
    using ConfigurationParameter::ConfigurationParameter;

    operator bool() const {
        return _param.hasData();
    }

    void set(const char *value) {
        __LDBG_assert_panic(_param.isWriteable(), "not writable");
        auto maxLength = _param._writeable->size() - 1;
        strncpy(_param.string(), value, maxLength)[maxLength] = 0;
    }
    void set(const __FlashStringHelper *value) {
        __LDBG_assert_panic(_param.isWriteable(), "not writable");
        auto maxLength = _param._writeable->size() - 1;
        strncpy_P(_param.string(), reinterpret_cast<PGM_P>(value), maxLength)[maxLength] = 0;
    }
    void set(const String &value) {
        set(value.c_str());
    }
};

using namespace ConfigurationHelper;

inline void Allocator::allocate(size_t size, ConfigurationParameter &parameter, uint8_t alignment)
{
    __LDBG_assert_printf(size != 0, "allocating size=%u", size);
#if 1
    __LDBG_assert_printf(parameter.hasData() == false, "has_data=%u is_writable=%u _readable=%p", parameter.hasData(), parameter.isWriteable(), parameter._getParam()._readable);
    if (parameter.hasData()) {
        deallocate(parameter);
    }
#else
    __LDBG_assert_panic(parameter.hasData() == false, "has_data=%", parameter.hasData());
#endif
    auto &param = parameter._getParam();
    __LDBG_assert_printf(size >= param.size(), "size=%u too small param.size=%u", size, param.size());
    param._readable = allocate(size, alignment);
}

inline void Allocator::deallocate(ConfigurationParameter &parameter)
{
    auto &param = parameter._getParam();
    if (param.isWriteable()) {
        //param._length = param._writeable->length();
        delete param._writeable;
        param._writeable = nullptr;
        param._is_writeable = false;
    }
    if (param._readable) {
        deallocate(param._readable);
        param._readable = nullptr;
    }
}


#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <debug_helper_disable.h>
