/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

namespace KFCConfigurationClasses {

    inline const void *loadBinaryConfig(HandleType handle, uint16_t &length)
    {
        auto data = config.getBinaryV(handle, length);
        // __CDBG_printf("handle=%04x data=%p len=%u", handle, data, length);
        return data;
    }

    inline void *loadWriteableBinaryConfig(HandleType handle, uint16_t length)
    {
        auto data = config.getWriteableBinary(handle, length);
        __CDBG_printf("handle=%04x data=%p len=%u", handle, data, length);
        return data;
    }

    inline void storeBinaryConfig(HandleType handle, const void *data, uint16_t length)
    {
        __CDBG_printf("handle=%04x data=%p len=%u", handle, data, length);
        config.setBinary(handle, data, length);
    }

    inline const char *loadStringConfig(HandleType handle)
    {
        auto str = config.getString(handle);
        // __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        return str;
    }

    inline char *loadWriteableStringConfig(HandleType handle, uint16_t size)
    {
        return config.getWriteableString(handle, size);
    }

    inline void storeStringConfig(HandleType handle, const char *str)
    {
        __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        config.setString(handle, str);
    }

    inline void storeStringConfig(HandleType handle, const __FlashStringHelper *str)
    {
        __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        config.setString(handle, str);
    }

    inline void storeStringConfig(HandleType handle, const String &str)
    {
        __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        config.setString(handle, str);
    }

}
