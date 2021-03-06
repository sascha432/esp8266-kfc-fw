/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "ConfigurationHelper.h"
#include "DebugHandle.h"
#include <crc16.h>
#include <ConfigurationParameter.h>

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// #if defined(ESP32)
// PROGMEM_STRING_DEF(EEPROM_partition_name, "eeprom");
// #endif

#if defined(ESP8266) || defined(_MSC_VER)
#include "spi_flash.h"
#endif

#ifdef NO_GLOBAL_EEPROM
EEPROMClass EEPROM((SECTION_EEPROM_START_ADDRESS) / SPI_FLASH_SEC_SIZE);
#endif

namespace ConfigurationHelper {

    uint8_t *allocate(size_t size, size_t *realSize)
    {
        __LDBG_assert_printf(size != 0, "allocating size=%u", size);
        size = std::max(1U, size);
        size = (size + 7) & ~7;
        auto ptr = reinterpret_cast<uint32_t *>(malloc(size));
        __LDBG_assert_printf(ptr != nullptr, "malloc=%u returned=%p", size, ptr);
        if (!ptr) {
            if (realSize) {
                *realSize = 0;
            }
            return nullptr;
        }
        if (realSize) {
            *realSize = size;
        }
        std::fill_n(ptr, size / sizeof(uint32_t), 0);
        return (uint8_t *)ptr;
    }

    void allocate(size_t size, ConfigurationParameter &parameter)
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
        param._readable = allocate(size, nullptr);
    }

    void deallocate(ConfigurationParameter &parameter)
    {
        auto &param = parameter._getParam();
        if (param.isWriteable()) {
            //param._length = param._writeable->length();
            delete param._writeable;
            param._writeable = nullptr;
            param._is_writeable = false;
        }
        if (param._readable) {
            free(param._readable);
            param._readable = nullptr;
        }
        // param._usage._counter2 = param._usage._counter + 1;
    }

    size_type getParameterLength(ParameterType type, size_t length)
    {
        switch (type) {
        case ParameterType::BYTE:
            return sizeof(uint8_t);
        case ParameterType::WORD:
            return sizeof(uint16_t);
        case ParameterType::DWORD:
            return sizeof(uint32_t);
        case ParameterType::QWORD:
            return sizeof(uint64_t);
        case ParameterType::FLOAT:
            return sizeof(float);
        case ParameterType::DOUBLE:
            return sizeof(double);
        case ParameterType::BINARY:
        case ParameterType::STRING:
            return (size_type)length;
        default:
            break;
        }
        __DBG_panic("invalid type=%u length=%u", type, length);
        return 0;
    }

#if DEBUG_CONFIGURATION_GETHANDLE

    static bool _panicMode = false;

    const HandleType registerHandleName(const char *name, uint8_t type)
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


    void setPanicMode(bool value)
    {
        _panicMode = value;
    }

    void addFlashUsage(HandleType handle, size_t readSize, size_t writeSize)
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

    const HandleType registerHandleName(const __FlashStringHelper *name, uint8_t type)
    {
        return registerHandleName(RFPSTR(name), type);
    }

    void readHandles()
    {
        KFCFS.begin();
        auto file = KFCFS.open(FSPGM(__DBG_config_handle_storage), fs::FileOpenMode::read);
        if (file) {
            uint32_t count = 0;
            while (file.available()) {
                auto line = file.readStringUntil('\n');
                if (line.trim().length()) {
                    ConfigurationHelper::registerHandleName(line.c_str(), __DBG__TYPE_NONE);
                    count++;
                }
            }
            file.close();
            __DBG_printf("read %u handles from=%s", count, SPGM(__DBG_config_handle_storage));
        }
    }

    void writeHandles(bool clear)
    {
        if (clear) {
            DebugHandle::clear();
        }
        __DBG_printf("storing %u handles file=%s", DebugHandle::getHandles().size(), SPGM(__DBG_config_handle_storage));
        auto file = KFCFS.open(FSPGM(__DBG_config_handle_storage), fs::FileOpenMode::write);
        if (file) {
            for (const auto &handle : DebugHandle::getHandles()) {
                file.printf_P(PSTR("%s\n"), handle.getName());
            }
            file.close();
        }
        DebugHandle::logUsage();
    }


    const char *getHandleName(HandleType crc)
    {
        auto debugHandle = DebugHandle::find(crc);
        if (debugHandle) {
            return debugHandle->getName();
        }
        return "<Unknown>";
    }

    void dumpHandles(Print &output, bool log)
    {
        DebugHandle::printAll(output);
        if (log) {
            writeHandles();
        }
    }

    bool registerHandleExists(HandleType crc)
    {
        return DebugHandle::find(crc) != nullptr;
    }

#else

    const char *getHandleName(HandleType crc)
    {
        return "N/A";
    }

#endif

}
