/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintString.h"

size_t PrintString::print(double n, int digits, bool trimTrailingZeros)
{
    if (trimTrailingZeros) {
        char buf[64];
        snprintf_P(buf, sizeof(buf), PSTR("%.*f"), digits, n);
        auto endPtr = strchr(buf, '.');
        if (endPtr) {
            endPtr += 2;
            auto ptr = endPtr;
            while(*ptr) {
                if (*ptr++ != '0') {
                    endPtr = ptr;
                }
            }
            return Print::write(buf, endPtr - buf);
        }
        else {
            return Print::write(buf, strlen(buf));
        }
    }
    else {
        return Print::print(n, digits);
    }
}
