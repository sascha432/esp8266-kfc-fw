/**
* Author: sascha_lammers@gmx.de
*/

// String with fixed size buffer and minimal String compatibility

#include <Arduino_compat.h>

template<size_t _MaxLength>
class FixedString
{
public:
    FixedString() : _str{0} {}
    FixedString(char str) : _str{str, 0} {}
    FixedString(const String &str) {
        strncpy(_str, str.c_str(), _MaxLength)[_MaxLength] = 0;
    }
    FixedString(const char *str) {
        strncpy_P(_str, str, _MaxLength)[_MaxLength] = 0;
    }

    inline void clear() {
        _str[0] = 0;
    }

    FixedString &operator=(const char str) {
        _str[0] = str;
        _str[1] = 0;
        return *this;
    }

    FixedString &operator=(const String &str) {
        strncpy(_str, str.c_str(), _MaxLength)[_MaxLength] = 0;
        return *this;
    }

    FixedString &operator=(const char *str) {
        strncpy_P(_str, str, _MaxLength)[_MaxLength] = 0;
        return *this;
    }

    bool operator==(const char *str) {
        return strcmp(_str, str) == 0;
    }

    bool operator==(const String &str) {
        return strcmp(_str, str.c_str()) == 0;
    }

    operator const char *() const {
        return c_str();
    }

    operator char *() {
        return begin();
    }

    const char *c_str() const {
        return _str;
    }

    size_t length() const {
        return strlen(_str);
    }

    constexpr size_t capacity() const {
        return _MaxLength;
    }

    char *begin() {
        return _str;
    }

    char *end() {
        return _str + length() + 1;
    }

    const char *begin() const {
        return _str;
    }

    const char *end() const {
        return _str + length() + 1;
    }

private:
    char _str[_MaxLength + 1];
};
