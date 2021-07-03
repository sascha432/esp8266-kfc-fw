/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationHelper.h"

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

        bool read(void *dst, uint16_t offset, uint16_t length);

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
