/**
  Author: sascha_lammers@gmx.de
*/

#include "int64_to_string.h"

String to_string(const int64_t value)
{
    String str;
    concat_to_string(str, value);
    return str;
}

String to_string(const uint64_t value)
{
    String str;
    concat_to_string(str, value);
    return str;
}

size_t concat_to_string(String &str, const int64_t value)
{
    char buffer[21];
    auto len = Integral_to_string::_Integral_to_string<int64_t, uint64_t>(value, buffer);
    str.concat(buffer + 21 - len, len);
    return len;
}

size_t concat_to_string(String &str, const uint64_t value) 
{
    char buffer[21];
    auto len = Integral_to_string::_Integral_to_string<uint64_t, uint64_t>(value, buffer);
    str.concat(buffer + 21 - len, len);
    return len;
}

char *ulltoa(unsigned long long value, char *buffer)
{
    auto len = Integral_to_string::_Integral_to_string<unsigned long long, unsigned long long>(value, buffer);
    buffer[21] = 0;
    return buffer + 21 - len;
}

char *lltoa(long long value, char *buffer)
{
    auto len = Integral_to_string::_Integral_to_string<long long, unsigned long long>(value, buffer);
    buffer[21] = 0;
    return buffer + 21 - len;
}


