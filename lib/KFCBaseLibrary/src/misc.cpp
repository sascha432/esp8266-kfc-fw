/**
  Author: sascha_lammers@gmx.de
*/

#include <string.h>
#include <functional>
#include <PrintString.h>
#include "misc.h"

// extern "C" {
//     const char SPGM_null[] PROGMEM = { "null" };
//     const char SPGM_0x_08x[] PROGMEM = { "0x%08x" };
// }

String formatBytes(size_t bytes)
{
    char buf[16];
    if (bytes < 1024) {
        snprintf_P(buf, sizeof(buf), PSTR("%dB"), bytes);
        return buf;
    } else if (bytes < (1024 * 1024)) {
        snprintf_P(buf, sizeof(buf), PSTR("%.2fKB"), bytes / 1024.0);
        return buf;
    } else if (bytes < (1024 * 1024 * 1024)) {
        snprintf_P(buf, sizeof(buf), PSTR("%.2fMB"), bytes / 1024.0 / 1024.0);
        return buf;
    }
    snprintf_P(buf, sizeof(buf), PSTR("%.2fGB"), bytes / 1024.0 / 1024.0 / 1024.0);
    return buf;
}

String urlEncode(const __FlashStringHelper *str, const __FlashStringHelper *set)
{
    PrintString out;
    auto ptr = reinterpret_cast<PGM_P>(str);
    if (!out.reserve(strlen_P(ptr))) {
        return String();
    }
    uint8_t ch;
    while((ch = pgm_read_byte(ptr)) != 0) {
        if (
            (set && (!isprint(ch) || strchr_P(reinterpret_cast<PGM_P>(set), ch))) ||
            (!set && !isalnum(ch))
        ) {
            out.printf_P(PSTR("%%%02X"), ch);
        }
        else {
            out += static_cast<char>(ch);
        }
        ptr++;
    }
    return out;
}

String printable_string(const uint8_t *buffer, size_t length, size_t maxLength, PGM_P extra, bool crlfAsText)
{
    PrintString str;
    printable_string(str, buffer, length, maxLength, extra, crlfAsText);
    return str;
}

void printable_string(Print &output, const uint8_t *buffer, size_t length, size_t maxLength, PGM_P extra, bool crlfAsText)
{
    if (maxLength) {
        length = std::min(maxLength, length);
    }
    auto ptr = buffer;
    while(length--) {
        char ch = pgm_read_byte(ptr);
        if (crlfAsText && (ch == '\n' || ch == '\r')) {
            switch(ch) {
                case '\r':
                    output.print(F("<<CR>>"));
                    break;
                case '\n':
                    output.println(F("<<LF>>"));
                    break;
            }
        }
        else if (isprint(ch) || (extra && strchr_P(extra, ch))) {
            output.print(ch);
        }
        else {
            output.print('\\');
            switch(ch) {
            case '\n':
                output.print('n');
                break;
            case '\r':
                output.print('r');
                break;
            case '\t':
                output.print('t');
                break;
            case '\b':
                output.print('b');
                break;
            default:
                output.printf_P(PSTR("x%02x"), (uint8_t)ch);
                break;
            }
        }
        ptr++;
    }
}

int countDecimalPlaces(double value, uint8_t maxPrecision)
{
    // NaN, inf, -inf ...
    if (maxPrecision == 0 || !std::isnormal(value)) {
        return 0;
    }
    // check if we have any decimal places
    if (maxPrecision == 1 || value - static_cast<int>(value) == 0) {
        return 1;
    }
    char buf[std::numeric_limits<double>::digits + 1];
    auto len = snprintf_P(buf, sizeof(buf), PSTR("%.*f"), maxPrecision, value);
    // check if the data fit into the buffer
    if (len == sizeof(buf)) {
        return maxPrecision;
    }
    auto endPtr = strchr(buf, '.');
    if (!endPtr) {
        return 0;
    }
    auto dot = endPtr;
    // keep 1 digit
    endPtr += 2;
    auto ptr = endPtr;
    while(*ptr) {
        if (*ptr++ != '0') {
            endPtr = ptr;
        }
    }
    return (endPtr - dot) - 1;
}

void append_slash(String &dir)
{
    auto len = dir.length();
    if (len == 0 || (len != 0 && dir.charAt(len - 1) != '/')) {
        dir += '/';
    }
}

void remove_trailing_slash(String &dir)
{
    auto len = dir.length();
    if (len && dir.charAt(len - 1) == '/')
    {
        dir.remove(len - 1);
    }
}

const __FlashStringHelper *sys_get_temp_dir()
{
    return F("/.tmp/");
}

File tmpfile(String /*dir*/tmpfile, const String &prefix)
{
    append_slash(tmpfile);
    const char *ptr = strrchr(prefix.c_str(), '/');
    ptr = ptr ? ptr + 1 : prefix.c_str();
    while(*ptr) {
        if (isalnum(*ptr)) {
            tmpfile += *ptr;
        }
        ptr++;
    }
    static constexpr uint8_t kRandomLen = 6;  // minimum number of random characters to add
    if (tmpfile.length() + kRandomLen > KFCFS_MAX_FILE_LEN) {
        tmpfile.remove(KFCFS_MAX_FILE_LEN - kRandomLen);
    }
    uint8_t len = kRandomLen;
    while(true) {
        char ch = rand() % (26 + 26 + 10); // add random characters, [a-zA-Z0-9]
        if (ch < 26) {
            ch += 'A';
        } else if (ch < 26 + 26) {
            ch += 'a' - 26;
        } else {
            ch += '0' - 26 - 26;
        }
        tmpfile += ch;
        if (--len == 0) {
            if (!KFCFS.exists(tmpfile)) {
                break;
            }
            len = kRandomLen;
            tmpfile.remove(tmpfile.length() - kRandomLen);
        }
    } while (len-- > 1 || KFCFS.exists(tmpfile));

    return KFCFS.open(tmpfile, fs::FileOpenMode::write);
}

const __FlashStringHelper *WiFi_disconnect_reason(WiFiDisconnectReason reason)
{
    #if ESP32
        switch(reason) {
            case WIFI_REASON_UNSPECIFIED:
                return F("UNSPECIFIED");
            case WIFI_REASON_AUTH_EXPIRE:
                return F("AUTH_EXPIRE");
            case WIFI_REASON_AUTH_LEAVE:
                return F("AUTH_LEAVE");
            case WIFI_REASON_ASSOC_EXPIRE:
                return F("ASSOC_EXPIRE");
            case WIFI_REASON_ASSOC_TOOMANY:
                return F("ASSOC_TOOMANY");
            case WIFI_REASON_NOT_AUTHED:
                return F("NOT_AUTHED");
            case WIFI_REASON_NOT_ASSOCED:
                return F("NOT_ASSOCED");
            case WIFI_REASON_ASSOC_LEAVE:
                return F("ASSOC_LEAVE");
            case WIFI_REASON_ASSOC_NOT_AUTHED:
                return F("ASSOC_NOT_AUTHED");
            case WIFI_REASON_DISASSOC_PWRCAP_BAD:
                return F("DISASSOC_PWRCAP_BAD");
            case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
                return F("DISASSOC_SUPCHAN_BAD");
            case WIFI_REASON_IE_INVALID:
                return F("IE_INVALID");
            case WIFI_REASON_MIC_FAILURE:
                return F("MIC_FAILURE");
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
                return F("4WAY_HANDSHAKE_TIMEOUT");
            case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
                return F("GROUP_KEY_UPDATE_TIMEOUT");
            case WIFI_REASON_IE_IN_4WAY_DIFFERS:
                return F("IE_IN_4WAY_DIFFERS");
            case WIFI_REASON_GROUP_CIPHER_INVALID:
                return F("GROUP_CIPHER_INVALID");
            case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
                return F("PAIRWISE_CIPHER_INVALID");
            case WIFI_REASON_AKMP_INVALID:
                return F("AKMP_INVALID");
            case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
                return F("UNSUPP_RSN_IE_VERSION");
            case WIFI_REASON_INVALID_RSN_IE_CAP:
                return F("INVALID_RSN_IE_CAP");
            case WIFI_REASON_802_1X_AUTH_FAILED:
                return F("802_1X_AUTH_FAILED");
            case WIFI_REASON_CIPHER_SUITE_REJECTED:
                return F("CIPHER_SUITE_REJECTED");
            case WIFI_REASON_BEACON_TIMEOUT:
                return F("BEACON_TIMEOUT");
            case WIFI_REASON_NO_AP_FOUND:
                return F("NO_AP_FOUND");
            case WIFI_REASON_AUTH_FAIL:
                return F("AUTH_FAIL");
            case WIFI_REASON_ASSOC_FAIL:
                return F("ASSOC_FAIL");
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                return F("HANDSHAKE_TIMEOUT");
            default:
                break;
        }
    #elif ESP8266 || _WIN32 || _WIN64
        switch(reason) {
            case WIFI_DISCONNECT_REASON_UNSPECIFIED:
                return F("UNSPECIFIED");
            case WIFI_DISCONNECT_REASON_AUTH_EXPIRE:
                return F("AUTH_EXPIRE");
            case WIFI_DISCONNECT_REASON_AUTH_LEAVE:
                return F("AUTH_LEAVE");
            case WIFI_DISCONNECT_REASON_ASSOC_EXPIRE:
                return F("ASSOC_EXPIRE");
            case WIFI_DISCONNECT_REASON_ASSOC_TOOMANY:
                return F("ASSOC_TOOMANY");
            case WIFI_DISCONNECT_REASON_NOT_AUTHED:
                return F("NOT_AUTHED");
            case WIFI_DISCONNECT_REASON_NOT_ASSOCED:
                return F("NOT_ASSOCED");
            case WIFI_DISCONNECT_REASON_ASSOC_LEAVE:
                return F("ASSOC_LEAVE");
            case WIFI_DISCONNECT_REASON_ASSOC_NOT_AUTHED:
                return F("ASSOC_NOT_AUTHED");
            case WIFI_DISCONNECT_REASON_DISASSOC_PWRCAP_BAD:
                return F("DISASSOC_PWRCAP_BAD");
            case WIFI_DISCONNECT_REASON_DISASSOC_SUPCHAN_BAD:
                return F("DISASSOC_SUPCHAN_BAD");
            case WIFI_DISCONNECT_REASON_IE_INVALID:
                return F("IE_INVALID");
            case WIFI_DISCONNECT_REASON_MIC_FAILURE:
                return F("MIC_FAILURE");
            case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT:
                return F("4WAY_HANDSHAKE_TIMEOUT");
            case WIFI_DISCONNECT_REASON_GROUP_KEY_UPDATE_TIMEOUT:
                return F("GROUP_KEY_UPDATE_TIMEOUT");
            case WIFI_DISCONNECT_REASON_IE_IN_4WAY_DIFFERS:
                return F("IE_IN_4WAY_DIFFERS");
            case WIFI_DISCONNECT_REASON_GROUP_CIPHER_INVALID:
                return F("GROUP_CIPHER_INVALID");
            case WIFI_DISCONNECT_REASON_PAIRWISE_CIPHER_INVALID:
                return F("PAIRWISE_CIPHER_INVALID");
            case WIFI_DISCONNECT_REASON_AKMP_INVALID:
                return F("AKMP_INVALID");
            case WIFI_DISCONNECT_REASON_UNSUPP_RSN_IE_VERSION:
                return F("UNSUPP_RSN_IE_VERSION");
            case WIFI_DISCONNECT_REASON_INVALID_RSN_IE_CAP:
                return F("INVALID_RSN_IE_CAP");
            case WIFI_DISCONNECT_REASON_802_1X_AUTH_FAILED:
                return F("802_1X_AUTH_FAILED");
            case WIFI_DISCONNECT_REASON_CIPHER_SUITE_REJECTED:
                return F("CIPHER_SUITE_REJECTED");
            case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT:
                return F("BEACON_TIMEOUT");
            case WIFI_DISCONNECT_REASON_NO_AP_FOUND:
                return F("NO_AP_FOUND");
            case WIFI_DISCONNECT_REASON_AUTH_FAIL:
                return F("AUTH_FAIL");
            case WIFI_DISCONNECT_REASON_ASSOC_FAIL:
                return F("ASSOC_FAIL");
            case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT:
                return F("HANDSHAKE_TIMEOUT");
            default:
                break;
        }
    #else
        #error Platform not supported
    #endif
    return F("UNKNOWN_REASON");
}

void bin2hex_append(String &str, const void *data, size_t length)
{
    char hex[3];
    auto ptr = reinterpret_cast<const uint8_t *>(data);
    while (length--) {
        snprintf_P(hex, sizeof(hex), PSTR("%02x"), *ptr++);
        str += hex;
    }
}

size_t hex2bin(void *buf, size_t length, const char *str)
{
    char hex[3];
    auto ptr = reinterpret_cast<uint8_t *>(buf);
    hex[2] = 0;
    while (length--) {
        if ((hex[0] = *str++) == 0 || (hex[1] = *str++) == 0) {
            break;
        }
        *ptr++ = static_cast<uint8_t>(strtoul(hex, nullptr, HEX));
    }
    return ptr - reinterpret_cast<uint8_t *>(buf);
}

size_t _printMacAddress(const uint8_t *mac, Print &output, char separator)
{
    if (!mac) {
        return 0;
    }
    return output.printf_P(PSTR("%02x%c%02x%c%02x%c%02x%c%02x%c%02x"), mac[0], separator, mac[1], separator, mac[2], separator, mac[3], separator, mac[4], separator, mac[5]);
}

String _mac2String(const uint8_t *mac, char separator)
{
    PrintString str;
    printMacAddress(mac, str, separator);
    return str;
}

static bool tokenizerCmpFunc(char ch, int type)
{
    if (type == 1) { // command separator
        if (ch == '=' || ch == ' ') {
            return true;
        }
    }
    else if (type == 2 && ch == ';') { // new command / new line separator
        return true;
    }
    else if (type == 3 && ch == '"') { // quotes
        return true;
    }
    else if (type == 4 && ch == '\\') { // escape character
        return true;
    }
    else if (type == 5 && ch == ',') { // token separator
        return true;
    }
    else if (type == 6 && isspace(ch)) { // leading whitespace outside quotes
        return true;
    }
    //else if (type == 7 && isspace(ch)) { // token leading whitespace
    //    return true;
    //}
    //else if (type == 8 && isspace(ch)) { // token trailing whitespace
    //    return true;
    //}
    return false;
}

static void tokenizer_trim_trailing(char *str, char *endPtr, TokenizerSeparatorCallback cmp)
{
    if (str) {
        while (endPtr-- >= str && cmp(*endPtr, 8)) {
            *endPtr = 0;
        }
    }
}

uint16_t tokenizer(char *str, TokenizerArgs &args, bool hasCommand, char **nextCommand, TokenizerSeparatorCallback cmp)
{
    if (!cmp) {
        cmp = tokenizerCmpFunc;
    }
    char* lastStr = nullptr;
    uint16_t argc = 0;
    *nextCommand = nullptr;
    if (hasCommand) {
        while(*str && !cmp(*str, 1)) {     // find end of command
            if (cmp(*str, 2)) {
                *str = 0;
                *nextCommand = str + 1;
                break;
            }
            str++;
        }
    }
    if (*str) {  // tokenize arguments into args
        bool quoted = false;
        if (hasCommand) {
            *str++ = 0;
        }
        while(*str) {
            if (!args.hasSpace()) {
                break;
            }
            // trim white space for quoted arguments only
            quoted = false;
            char *wptr = str;
            while (cmp(*wptr, 6)) {
                wptr++;
            }
            if (cmp(*wptr, 3)) {
                quoted = true;
                str = wptr + 1;
            }

            // trim leading characters
            while (*str && cmp(*str, 7)) {
                str++;
            }
            lastStr = str;
            args.add(str);
            argc++;
            if (quoted) {
                while (*str) {  // find end of argument
                    // handle escaped characters
                    if (cmp(*str, 3)) {
                        str++;
                        if (cmp(*str, 3)) { // double quote, keep one
                            strcpy(str, str + 1);
                            continue;
                        }
                        *(str - 1) = 0; // end quote, find next token
                        while (*str && !cmp(*str, 5)) {
                            if (cmp(*str, 2)) {
                                *str = 0;
                                tokenizer_trim_trailing(lastStr, str, cmp);
                                *nextCommand = str + 1;
                                break;
                            }
                            str++;
                        }
                        break;
                    }
                    else if (cmp(*str, 4)) {
                        str++;
                        if (cmp(*str, 4)) { // double backslash
                            strcpy(str, str + 1);
                        }
                        else if (cmp(*str, 3)) { // escaped quote
                            strcpy(str - 1, str);
                        }
                        continue; // single backslash
                    }
                    str++;
                }
                if (!*str) {
                    break;
                }
            }
            else {
                while (*str && !cmp(*str, 5)) {
                    if (cmp(*str, 2)) {
                        *str = 0;
                        tokenizer_trim_trailing(lastStr, str, cmp);
                        *nextCommand = str + 1;
                        break;
                    }
                    str++;
                }
                if (!*str) {
                    break;
                }
                *str = 0;
                tokenizer_trim_trailing(lastStr, str, cmp);
            }
            str++;
        }
    }
    return argc;
}

void split::split(const char *str, const char *sep, AddItemCallback callback, int flags, uint16_t limit)
{
    unsigned int start = 0, stop;
    for (stop = 0; str[stop]; stop++) {
        if (limit > 0 && strchr(sep, str[stop])) {
            limit--;
            callback(str + start, stop - start, flags);
            start = stop + 1;
        }
    }
    callback(str + start, stop - start, flags | SplitFlagsType::LAST);
}

void split::split_P(const FPStr &str, const FPStr &sep, AddItemCallback callback, int flags, uint16_t limit)
{
    unsigned int start = 0, stop;
    char ch;
    flags |= SplitFlagsType::_PROGMEM;
    for (stop = 0; (ch = str[stop]); stop++) {
        if (limit > 0 && strchr_P(sep.c_str(), ch)) {
            limit--;
            callback(str.c_str() + start, stop - start, flags);
            start = stop + 1;
        }
    }
    callback(str.c_str() + start, stop - start, flags | SplitFlagsType::LAST);
}

// timezone support
size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm)
{
#if defined(ESP8266)
    // strftime does not support PROGMEM
    char fmt[strlen_P(format) + 1];
    strcpy_P(fmt, format);
#else
    auto fmt = format;
#endif
    return strftime(buf, size, fmt, tm);
}

#if defined(ESP8266) || defined(ESP32) || defined(_MSC_VER)

uint32_t getSystemUptime()
{
    return (uint32_t)(micros64() / 1000000UL);
}

uint64_t getSystemUptimeMillis()
{
    return (uint64_t)(micros64() / 1000UL);
}

#else

#include <push_pack.h>

typedef union {
    uint64_t value;
    struct __attribute__packed__ {
        uint32_t millis;
        uint16_t overflow;
    };
} UptimeInfo_t;

static UptimeInfo_t uptimeInfo = { 0 };

#include <pop_pack.h>

// fallback if micros64() is not available
#warning these functions do not work properly if not called at least once every 49 days

uint32_t getSystemUptime()
{
    Serial.println(sizeof(UptimeInfo_t));
    return (uint32_t)(getSystemUptimeMillis() / 1000UL);
}

uint64_t getSystemUptimeMillis()
{
    uint32_t ms;
    if ((ms = millis()) < uptimeInfo.millis) {
        uptimeInfo.overflow++;
    }
    uptimeInfo.millis = ms;
    return uptimeInfo.value;
}

#endif

IPAddress convertToIPAddress(const char *hostname)
{
    IPAddress addr;
    if (addr.fromString(hostname) && IPAddress_isValid(addr)) {
        return addr;
    }
    return IPAddress();
}

uint8_t numberOfSetBits(uint32_t i)
{
     i = i - ((i >> 1) & 0x55555555);
     i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
     return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

uint8_t numberOfSetBits(uint16_t i)
{
     i = i - ((i >> 1) & 0x5555);
     i = (i & 0x3333) + ((i >> 2) & 0x3333);
     return (((i + (i >> 4)) & 0x0F0F) * 0x0101) >> 8;
}

// class _bitsToStr_String : public String {
// public:
//     using String::String;

//     char *reserve(size_t len, char *&begin, char *&end) {
//         if (String::reserve(len)) {
//             setLen(len);
//             begin = wbuffer();
//             end = begin + len;
//             *end = 0;
//             return begin;
//         }
//         return nullptr;
//     }
// };

// String _bitsToStr(const uint8_t *data, uint8_t bits, bool reverse)
// {
//     _bitsToStr_String str;
//     char *begin, *end;
//     if (str.reserve(bits, begin, end)) {
//         if (!reverse) {
//             begin = end - 1;
//         }
//         for(uint8_t i = 0; i < bits; i++) {
//             uint8_t bit = i & 0x07;
//             *begin = (*data & _BV(bit)) ? '1' : '0';
//             reverse ? ++begin : --begin;
//             if (bit == 7) {
//                 data++;
//             }
//         }
//     }
//     return str;
// }
