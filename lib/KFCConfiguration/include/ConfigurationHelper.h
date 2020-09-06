/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <EEPROM.h>
#include "ConfigurationParameter.h"

#pragma push_macro("new")
#undef new

namespace ConfigurationHelper {

    using HandleType = ConfigurationParameter::Handle_t;

    class EEPROMClassEx : public EEPROMClass {
    public:
        using EEPROMClass::EEPROMClass;

        size_t getWriteSize() const {
            if (!_size || !_dirty || !_data) {
                return 0;
            }
            return _size;
        }
        bool getDirty() const {
            return _dirty;
        }
        void clearAndEnd() {
            _dirty = false;
            end();
        }
    };


    // direct access to EEPROM if supported
    class EEPROMAccess {
    public:
        EEPROMAccess(bool isInitialized, uint16_t size) : _isInitialized(isInitialized), _size(size) {
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
        void end();
        void commit();

        uint16_t read(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size);
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
        bool _isInitialized;
        uint16_t _size;
 //#if defined(ESP32)
 //       const esp_partition_t *_partition;
 //#endif
    };




#if DEBUG_CONFIGURATION_GETHANDLE
    const HandleType registerHandleName(const char *name, uint8_t type);
    const HandleType registerHandleName(const __FlashStringHelper *name, uint8_t type);
    bool registerHandleExists(HandleType handle);
    void setPanicMode(bool value);
    void addFlashUsage(HandleType handle, size_t readSize, size_t writeSize);

    void readHandles();
    void writeHandles(bool clear = false);
    void dumpHandles(Print &output, bool log);
#endif
    const char *getHandleName(HandleType crc);

    // memory pool for configuration data
    class Pool {
    public:
        Pool(uint16_t size);
        ~Pool();
        Pool(const Pool &pool) = delete;
        Pool(Pool &&pool) noexcept;
        Pool &operator=(Pool &&pool) noexcept;

        // call space_unaligned(align(length))
        bool space_unaligned(uint16_t length) const;

        uint16_t available() const;
        uint16_t size() const;

        // number of pointers
        uint8_t count() const;

        uint8_t *allocate(uint16_t length);
        void release(const void *ptr);

        // returns length dword aligned
        static uint16_t align(uint16_t length);

        // returns true if ptr belongs to this pool
        bool hasPtr(const void *ptr) const;

        void *getPtr() const;

    private:
        uint8_t *_end() const {
            return _ptr + _size;
        }

        uint8_t *_ptr;
        union {
            struct {
                uint32_t _length: 12;
                uint32_t _size: 12;
                uint32_t _count: 8;
            };
            uint32_t _val;
        };
    };

    inline Pool &Pool::operator=(Pool &&pool) noexcept
    {
        this->~Pool();
        ::new(static_cast<void *>(this)) Pool(std::move(pool));
        return *this;
    }

    inline uint16_t Pool::available() const
    {
        return _size - _length;
    }

    inline uint16_t Pool::size() const
    {
        return _size;
    }

    inline uint8_t Pool::count() const
    {
        return _count;
    }

    inline bool Pool::space_unaligned(uint16_t length) const
    {
        return length <= (_size - _length);
    }

    inline uint16_t Pool::align(uint16_t length)
    {
        return (length + 4) & ~3;
    }

    inline bool Pool::hasPtr(const void *ptr) const
    {
        return (ptr >= _ptr && ptr < _ptr + _length);
    }

    inline void *Pool::getPtr() const
    {
        return _ptr;
    }

};

#pragma pop_macro("new")
