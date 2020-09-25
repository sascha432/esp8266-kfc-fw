/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationHelper.h"

#if _MSC_VER
#define ESP8266 1
#include "spi_flash.h"
#endif

namespace ConfigurationHelper {

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
        static constexpr uint16_t kTempReadBufferSize = 224;

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

#if defined(ESP8266) || 1

        //// allocate memory on stack
        //uint16_t read_with_temporary_on_stack(const uint32_t start_address, const uint16_t readSize, uint8_t *dst, const uint16_t size, const uint16_t maxSize, const uint8_t alignment);

        // temp and dst can be the same buffer
        // if alignment is not zero, data gets moved and padded with zeros
        // if the buffers are different, data is copied and padded with zeros
        uint16_t read_with_temporary(const uint32_t start_address, uint8_t *temp, const uint16_t readSize, uint8_t *dst, const uint16_t size, const uint16_t maxSize, const uint8_t alignment);

#endif

        uint16_t read(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t maxSize);// __DBG_IF_flashUsage(, size_t &));

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
