/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationParameter.h"

#pragma push_macro("new")
#undef new

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

}

#pragma pop_macro("new")
