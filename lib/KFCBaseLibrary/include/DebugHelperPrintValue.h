/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "TypeName.h"

class DebugHelperPrintValue : public Print
{
public:
    operator bool() const {
        return DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE;
    }

    virtual void printPrefix() = 0;

    virtual size_t write(uint8_t ch) override;
    virtual size_t write(const uint8_t* buffer, size_t size) override;
};
