/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

#include "Arduino_compat.h"
#include "WString.h"

static char *dtostrf(double number, signed char width, unsigned char prec, char *s) {
    bool negative = false;

    if (isnan(number)) {
        strcpy(s, "nan");
        return s;
    }
    if (isinf(number)) {
        strcpy(s, "inf");
        return s;
    }

    char *out = s;

    int fillme = width; // how many cells to fill for the integer part
    if (prec > 0) {
        fillme -= (prec + 1);
    }

    // Handle negative numbers
    if (number < 0.0) {
        negative = true;
        fillme--;
        number = -number;
    }

    // Round correctly so that print(1.999, 2) prints as "2.00"
    // I optimized out most of the divisions
    double rounding = 2.0;
    for (uint8_t i = 0; i < prec; ++i)
        rounding *= 10.0;
    rounding = 1.0 / rounding;

    number += rounding;

    // Figure out how big our number really is
    double tenpow = 1.0;
    int digitcount = 1;
    double nextpow;
    while (number >= (nextpow = (10.0 * tenpow))) {
        tenpow = nextpow;
        digitcount++;
    }

    // minimal compensation for possible lack of precision (#7087 addition)
    number *= 1 + std::numeric_limits<decltype(number)>::epsilon();

    number /= tenpow;
    fillme -= digitcount;

    // Pad unused cells with spaces
    while (fillme-- > 0) {
        *out++ = ' ';
    }

    // Handle negative sign
    if (negative) *out++ = '-';

    // Print the digits, and if necessary, the decimal point
    digitcount += prec;
    int8_t digit = 0;
    while (digitcount-- > 0) {
        digit = (int8_t)number;
        if (digit > 9) digit = 9; // insurance
        *out++ = (char)('0' | digit);
        if ((digitcount == prec) && (prec > 0)) {
            *out++ = '.';
        }
        number -= digit;
        number *= 10.0;
    }

    // make sure the string is terminated
    *out = 0;
    return s;
}

LPWStr::LPWStr() : _str(nullptr) {
    lpw_str(String());
}

LPWStr::LPWStr(const String &str) : _str(nullptr) {
    lpw_str(str);
}

LPWStr::~LPWStr() {
    delete[] _str;
}

LPWSTR LPWStr::lpw_str(const String &str) {
    if (_str) {
        delete[] _str;
    }
    int slength = (int)str.length() + 1;
    int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), slength, 0, 0);
    _str = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), slength, _str, len);
    return _str;
}

LPWSTR LPWStr::lpw_str() {
    return _str;
}

String::String(bool value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    assign(buf);
}

String::String(int16_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    assign(buf);
}

String::String(uint16_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    assign(buf);
}

String::String(int64_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", value);
    assign(buf);
}

String::String(uint64_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", value);
    assign(buf);
}

String::String(float value, unsigned char decimals) {
    char buf[33];
    dtostrf(value, (decimals + 2), decimals, buf);
    assign(buf);
    //snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    //assign(buf);
}

String::String(double value, unsigned char decimals) {
    char buf[33];
    dtostrf(value, (decimals + 2), decimals, buf);
    assign(buf);
    //snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    //assign(buf);
}

String::String(const char ch) {
    assign(1, ch);
}

String::String(const char *str) {
    if (str) {
        assign(str);
    }
}

String::String(String &&str) noexcept {
    *this = std::move(str);
}

String::String(const String &str) {
    *this = str;
}

String::String(const __FlashStringHelper *str) : String(reinterpret_cast<const char *>(str)) {
}

String::~String() {
    CHECK_MEMORY();
}

String::String(int value, unsigned char base) {
    char buf[65]; // max 64bit + NUL
    _itoa(value, buf, base);
    assign(buf);
}

String::String(unsigned int value, unsigned char base) {
    char buf[65]; // max 64bit + NUL
    _itoa(value, buf, base);
    assign(buf);
}

String::String(long value, unsigned char base) {
    char buf[65]; // max 64bit + NUL
    _ltoa(value, buf, base);
    assign(buf);
}

String::String(unsigned long value, unsigned char base) {
    char buf[65]; // max 64bit + NUL
    _ltoa(value, buf, base);
    assign(buf);
}

int String::indexOf(const String &find, int index) const {
    return String::find(find, index);
}

int String::indexOf(char ch, int index) const {
    return String::find(ch, index);
}

String String::substring(size_t from) const {
    return String(substr(from).c_str());
}

String String::substring(size_t from, size_t to) const {
    return String(substr(from, to- from).c_str());
}

long String::toInt() const {
    return atol(c_str());
}

float String::toFloat() const {
    return (float)atof(c_str());
}

double String::toDouble() const {
    return atof(c_str());
}

unsigned char String::reserve(size_t size) {
    std::string::reserve(size);
    return (capacity() >= size);
}

bool String::equals(const String &value) const {
    return *this == value;
}

bool String::equals(const char *value) const {
    return strcmp(c_str(), value) == 0;
}

bool String::equalsIgnoreCase(const String &value) const noexcept {
    return _stricmp(c_str(), value.c_str()) == 0;
}

bool String::equalsIgnoreCase(const char *value) const noexcept {
    return _stricmp(c_str(), value) == 0;
}

void String::remove(int index, size_t count) {
    if (index < 0) {
        index = length() + index;
    }
    if (count == -1 || index + count >= length()) {
        assign(substr(0, index));
    }
    else {
        assign(substr(0, index) + substr(index + count));
    }
}

char *String::begin() {
    return const_cast<char *>(data());
}

void String::trim(void) {
    char *buffer = begin();
    size_t len = length();
    if (!buffer || len == 0) return;
    char *begin = buffer;
    while (isspace(*begin)) begin++;
    char *end = buffer + len - 1;
    while (isspace(*end) && end >= begin) end--;
    len = end + 1 - begin;
    if (begin > buffer) memmove(buffer, begin, len);
    buffer[len] = 0;
    assign(buffer);
}

void String::toLowerCase(void) {
    if (length()) {
        for (char *p = begin(); *p; p++) {
            *p = tolower(*p);
        }
    }
}

void String::toUpperCase(void) {
    if (length()) {
        for (char *p = begin(); *p; p++) {
            *p = toupper(*p);
        }
    }
}

void String::replace(char find, char replace) {
    if (length()) {
    }
    for (char *p = begin(); *p; p++) {
        if (*p == find) *p = replace;
    }
}

void String::replace(String fromStr, String toStr) {
    size_t pos = find(fromStr);
    while (pos != String::npos) {
        std::string::replace(pos, fromStr.length(), toStr);
        pos = find(fromStr, pos + toStr.length());
    }
}

unsigned char String::concat(const char *cstr, unsigned int length) {
    if (!cstr) {
        return 0;
    }
    if (length == 0) {
        return 1;
    }
    this->append(cstr, length);
    return length;
}

unsigned char String::concat(char c) {
    this->append(1, c);
    return 1;
}

bool String::startsWith(const String str) const {
    if (length() < str.length()) {
        return false;
    }
    return (strncmp(c_str(), str.c_str(), str.length()) == 0);
}

bool String::endsWith(const String str) const {
    if (length() < str.length()) {
        return false;
    }
    return strcmp(c_str() + length() - str.length(), str.c_str()) == 0;
}

char String::charAt(size_t idx) const {
    return at(idx);
}

LPWStr String::LPWStr() {
    return ::LPWStr(*this);
}

std::wstring String::w_str() {
    return std::wstring(std::string::begin(), std::string::end());
}

String &String::operator=(String &&str) noexcept {
    assign(str.c_str());
    str.remove(0, -1);
    return *this;
}

String &String::operator =(const String &str) {
    assign(str.c_str());
    return *this;
}

String String::operator + (const String &str) {
    String tmp = *this;
    tmp.concat(str.c_str(), str.length());
    return tmp;
}

String &String::operator += (const String &str) {
    concat(str.c_str(), str.length());
    return (*this);
}

String &String::operator += (char *str) {
    concat(str, strlen(str));
    return (*this);
}

String String::operator + (const char *str) {
    String tmp = *this;
    tmp.concat(str, strlen(str));
    return tmp;
}

String &String::operator += (const char *str) {
    concat(str, strlen(str));
    return (*this);
}

String String::operator + (const char ch) {
    String tmp = *this;
    tmp.concat(ch);
    return tmp;
}

String &String::operator += (const char ch) {
    concat(ch);
    return (*this);
}

String String::operator + (const __FlashStringHelper *str) {
    String tmp = *this;
    const char *ptr = reinterpret_cast<const char *>(str);
    tmp.concat(ptr, strlen(ptr));
    return tmp;
}

String &String::operator += (const __FlashStringHelper *str) {
    const char *ptr = reinterpret_cast<const char *>(str);
    concat(ptr, strlen(ptr));
    return (*this);
}


#endif
