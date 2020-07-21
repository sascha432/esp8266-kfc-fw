/**
  Author: sascha_lammers@gmx.de
*/

#include <functional>
#include <PrintString.h>
#include "misc.h"

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

String formatTime(unsigned long seconds, bool days_if_not_zero)
{
    PrintString out;
    unsigned int days = (unsigned int)(seconds / 86400);
    if (!days_if_not_zero || days) {
        out.printf_P(PSTR("%u days "), days);
    }
    out.printf_P(PSTR("%02uh:%02um:%02us"), (unsigned)((seconds / 3600) % 24), (unsigned)((seconds / 60) % 60), (unsigned)(seconds % 60));
    return out;
}

String url_encode(const String &str)
{
    PrintString out_str;
    const char *ptr = str.c_str();
    while(*ptr) {
        if (isalnum(*ptr)) {
            out_str += *ptr;
        } else {
            out_str.printf_P(PSTR("%%%02X"), (int)(*ptr & 0xff));
        }
        ptr++;
    }
    return out_str;
}

String printable_string(const uint8_t *buffer, size_t length, size_t maxLength)
{
    PrintString str;
    printable_string(str, buffer, length, maxLength);
    return str;
}

void printable_string(Print &output, const uint8_t *buffer, size_t length, size_t maxLength, const char *extra)
{
    if (maxLength) {
        length = std::min(maxLength, length);
    }
    auto ptr = buffer;
    while(length--) {
        if (isprint(*ptr) || (extra && strchr(extra, *ptr))) {
            output.print((char)*ptr);
        } else {
            output.printf_P(PSTR("\\x%02X"), (int)(*ptr & 0xff));
        }
        ptr++;
    }
}

void append_slash(String &dir)
{
    if (dir.length() == 0 || (dir.length() != 0 && dir.charAt(dir.length() - 1) != '/')) {
        dir += '/';
    }
}

void remove_trailing_slash(String &dir)
{
    if (dir.length() && dir.charAt(dir.length() - 1) == '/')
    {
        dir.remove(dir.length() - 1);
    }
}

String sys_get_temp_dir()
{
    return F("/tmp/");
}

#if SPIFFS_TMP_FILES_TTL
File tmpfile(const String &dir, const String &prefix, long ttl)
{
    String tmp = dir;
    append_slash(tmp);
    tmp += String((millis() / 1000UL) + ttl, HEX);
    if (isxdigit(prefix.charAt(0))) {
        tmp += '_'; // add separator if next character is a hex digit
    }
    tmp += prefix;
#else
File tmpfile(const String &dir, const String &prefix) {
    String tmp = dir;
    append_slash(tmp);
    tmp += prefix;
#endif
    do {
        char ch = rand() % (26 + 26 + 10); // add random characters, [a-zA-Z0-9]
        if (ch < 26) {
            ch += 'A';
        } else if (ch < 26 + 26) {
            ch += 'a' - 26;
        } else {
            ch += '0' - 26 - 26;
        }
        if (tmp.length() > 31) {
            tmp = tmp.substring(0, 24);
        }
        tmp += ch;
    } while (SPIFFS.exists(tmp));

    return SPIFFS.open(tmp, fs::FileOpenMode::write);
}

String WiFi_disconnect_reason(WiFiDisconnectReason reason)
{
#if defined(ESP32)
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
            PrintString out;
            out.printf_P(PSTR("UNKNOWN_REASON_%d"), reason);
            return out;
    }
#elif defined(ESP8266) || _WIN32 || _WIN64
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
            PrintString out;
            out.printf_P(PSTR("UNKNOWN_REASON_%d"), reason);
            return out;
    }
#else
#error Platform not supported
#endif
}

#ifndef DEBUG_STRING_CHECK_NULLPTR
#define DEBUG_STRING_CHECK_NULLPTR          DEBUG
#endif

int stringlist_find_P_P(PGM_P list, PGM_P find, char separator)
{
#if DEBUG_STRING_CHECK_NULLPTR
    if (!list || !find) {
        debug_printf_P(PSTR("list=%p find=%p separator=%c\n"), list, find, separator);
        return -1;
    }
#endif
    const char separator_str[2] = { separator, 0 };
    return stringlist_find_P_P(list, find, separator_str);
}

int stringlist_find_P_P(PGM_P list, PGM_P find, PGM_P separator/*, int &position*/)
{
    if (!list || !find || !separator) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("list=%p find=%p separator=%p\n"), list, find, separator);
#endif
        return -1;
    }
    PGM_P ptr1 = list;
    PGM_P ptr2 = find;
    uint8_t ch;
    int16_t num = 0;
    // position = 0;
    do {
        ch = pgm_read_byte(ptr1);
        if (strchr_P(separator, ch)) {    // end of one string
            if (ptr2 && pgm_read_byte(ptr2++) == 0) { // match
                return num;
            } else {
                // position = ptr1 - list;
                ptr2 = find; // compare with next string
                num++;
            }
        } else {
            if (ptr2 && pgm_read_byte(ptr2++) != ch) { // mismatch, stop comparing
                ptr2 = nullptr;
            } else if (ptr2 && !ch) { // match
                return num;
            }
        }
        ptr1++;
    } while(ch);

    // position = -1;
    return -1;
}

int strcmp_P_P(PGM_P str1, PGM_P str2)
{
    if (!str1 || !str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%p str2=%p\n"), str1, str2);
#endif
        return -1;
    }
    uint8_t ch;
    do {
        ch = pgm_read_byte(str1++);
        if (ch != pgm_read_byte(str2++)) {
            return -1;
        }
    } while (ch);
    return 0;
}

int strcasecmp_P_P(PGM_P str1, PGM_P str2)
{
    if (!str1 || !str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%p str2=%p\n"), str1, str2);
#endif
        return -1;
    }
    uint8_t ch;
    do {
        ch = pgm_read_byte(str1++);
        if (tolower(ch) != tolower(pgm_read_byte(str2++))) {
            return -1;
        }
    } while (ch);
    return 0;
}

size_t String_rtrim(String &str)
{
    size_t len = str.length();
    while (len && isspace(str.charAt(len - 1))) {
        len--;
    }
    str.remove(len, -1);
    return len;
}

size_t String_ltrim(String &str)
{
    size_t remove = 0;
    while (isspace(str.charAt(remove))) {
        remove++;
    }
    str.remove(0, remove);
    return str.length();
}

size_t String_trim(String &str)
{
    str.trim();
    return str.length();
}

size_t String_rtrim(String &str, const char *chars, uint16_t minLength)
{
    if (!chars) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str=%s chars=%p\n"), str.c_str(), chars);
#endif
        return str.length();
    }
    size_t len = str.length();
    minLength = len - minLength;
    while (minLength-- && len && strchr(chars, str.charAt(len - 1))) {
        len--;
    }
    str.remove(len, -1);
    return len;
}

size_t String_ltrim(String &str, const char *chars)
{
   if (!chars) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str=%s chars=%p\n"), str.c_str(), chars);
#endif
        return str.length();
    }
    size_t remove = 0;
    while (strchr(chars, str.charAt(remove))) {
        remove++;
    }
    str.remove(0, remove);
    return str.length();
}

size_t String_trim(String &str, const char *chars)
{
    String_rtrim(str, chars);
    return String_ltrim(str, chars);
}

size_t String_rtrim_P(String &str, PGM_P chars, uint16_t minLength)
{
    if (!chars) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str=%s chars=%p\n"), str.c_str(), chars);
#endif
        return str.length();
    }
    auto buf = new char[strlen_P(chars) + 1];
    strcpy_P(buf, chars);
    auto len = String_rtrim(str, buf, minLength);
    delete [] buf;
    return len;
}

size_t String_ltrim_P(String &str, PGM_P chars)
{
    if (!chars) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str=%s chars=%p\n"), str.c_str(), chars);
#endif
        return str.length();
    }
    auto buf = new char[strlen_P(chars) + 1];
    strcpy_P(buf, chars);
    auto len = String_ltrim(str, buf);
    delete[] buf;
    return len;
}

size_t String_trim_P(String &str, PGM_P chars)
{
    if (!chars) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str=%s chars=%p\n"), str.c_str(), chars);
#endif
        return str.length();
    }
    auto buf = new char[strlen_P(chars) + 1];
    strcpy_P(buf, chars);
    auto len = String_trim(str, buf);
    delete[] buf;
    return len;
}

size_t String_trim(String &str, char ch)
{
    char chars[2] = { ch, 0 };
    return String_trim(str, chars);
}

size_t String_ltrim(String &str, char ch)
{
    char chars[2] = { ch, 0 };
    return String_ltrim(str, chars);
}

size_t String_rtrim(String &str, char ch, uint16_t minLength)
{
    char chars[2] = { ch, 0 };
    return String_rtrim(str, chars, minLength);
}


#if 0

bool String_equals(const String &str1, PGM_P str2)
{
    if (!str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%s str2=%p\n"), str1.c_str(), str2);
#endif
        return false;
    }
    const size_t strlen2 = strlen_P(str2);
    return (str1.length() == strlen2) && !strcmp_P(str1.c_str(), str2);
}

bool String_equalsIgnoreCase(const String &str1, PGM_P str2)
{
    if (!str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%s str2=%p\n"), str1.c_str(), str2);
#endif
        return false;
    }
    const size_t strlen2 = strlen_P(str2);
    return (str1.length() == strlen2) && !strcasecmp_P(str1.c_str(), str2);
}

#endif

bool String_startsWith(const String &str1, PGM_P str2)
{
    if (!str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%s str2=%p\n"), str1.c_str(), str2);
#endif
        return false;
    }
    const size_t strlen2 = strlen_P(str2);
    return (str1.length() >= strlen2) && !strncmp_P(str1.c_str(), str2, strlen2);
}

bool String_endsWith(const String &str1, PGM_P str2)
{
    if (!str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%s str2=%p\n"), str1.c_str(), str2);
#endif
        return false;
    }
    const size_t strlen2 = strlen_P(str2);
    auto len = str1.length();
    return (len >= strlen2) && !strcmp_P(str1.c_str() + len - strlen2, str2);
}

bool String_endsWith(const String &str1, char ch)
{
    auto len = str1.length();
    return (len != 0) && (str1.charAt(len - 1) == ch);
}

int strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2)
{
    if (!str1 || !str2) {
#if DEBUG_STRING_CHECK_NULLPTR
        debug_printf_P(PSTR("str1=%p len1=%u str2=%p len2=%u\n"), str1, len1, str2, len2);
#endif
        return -1;
    }
    if (len2 > len1) {
        return -1;
    } else if (len2 == len1) {
        return strcmp_P(str1, str2);
    }
    return strcmp_P(str1 + len1 - len2, str2);
}

#if defined(ESP8266) || defined(ESP32)

const char *strchr_P(const char *str, int c)
{
    if (str) {
        char ch;
        while(0 != (ch = pgm_read_byte(str))) {
            if (ch == c) {
                return str;
            }
            str++;
        }
    }
#if DEBUG_STRING_CHECK_NULLPTR
    else {
        debug_printf_P(PSTR("str=%p int=%u\n"), str, c);
    }
#endif
    return nullptr;
}

#endif

void bin2hex_append(String &str, const void *data, size_t length)
{
    char hex[3];
    auto ptr = reinterpret_cast<const uint8_t *>(data);
    while (length--) {
        snprintf(hex, sizeof(hex), "%02x", *ptr++);
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

const char *mac2char_s(char *dst, size_t size, const uint8_t *mac, char separator)
{
    constexpr uint8_t len = 6;
    if (!mac) {
        strncpy_P(dst, PSTR("null"), size);
        return dst;
    }
    int8_t count = len;
    char *ptr = dst;
    while(count-- && size > 1) {
        snprintf_P(ptr, 3, PSTR("%02x"), *mac);
        size -= 2;
        if (count && separator && size--) {
            *ptr++ = separator;
        }
        mac++;
    }
    *ptr = 0;
    return dst;
}

String mac2String(const uint8_t *mac, char separator)
{
    char buf[MAC2STRING_LEN];
    return mac2char_s(buf, sizeof(buf), mac, separator);
}

String inet_ntoString(uint32_t ip)
{
    char buf[INET_NTOA_LEN];
    return inet_ntoa_s(buf, sizeof(buf), ip);
}

const char *inet_ntoa_s(char *dst, size_t size, uint32_t ip)
{
    auto bip = reinterpret_cast<const uint8_t *>(&ip);
    snprintf_P(dst, size, PSTR("%u.%u.%u.%u"), bip[0], bip[1], bip[2], bip[3]);
    return dst;
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

// void explode(char *str, const char *separator, std::vector<char*>& vector, const char *trim)
// {
//     char *nextCommand;
//     TokenizerArgsVector<char *> args(vector);
//     tokenizer(str, args, false, &nextCommand, [separator, trim](char ch, int type) {
//         return ((type == 5 && strchr(separator, ch)) || ((trim && (type == 7 || type == 8)) && strchr(trim, ch)));
//     });
// }

void split::vector_callback(const char *sptr, size_t len, void *ptr, int flags)
{
    StringVector &container = *reinterpret_cast<StringVector *>(ptr);
    String str = (flags & SplitFlagsType::_PROGMEM) ? PrintString(reinterpret_cast<const __FlashBufferHelper *>(sptr), len) : PrintString(reinterpret_cast<const uint8_t *>(sptr), len);
    if (flags & SplitFlagsType::TRIM) {
        str.trim();
    }
    if ((flags & SplitFlagsType::EMPTY) || str.length()) {
        std::back_inserter(container) = str;
    }
}

void split::split(const char *str, char sep, callback fun, void *data, int flags, uint16_t limit)
{
    unsigned int start = 0, stop;
    for (stop = 0; str[stop]; stop++) {
        if (limit > 0 && str[stop] == sep) {
            limit--;
            fun(str + start, stop - start, data, flags);
            start = stop + 1;
        }
    }
    fun(str + start, stop - start, data, flags);
}

void split::split_P(PGM_P str, char sep, callback fun, void *data, int flags, uint16_t limit)
{
    unsigned int start = 0, stop;
    char ch;
    flags |= SplitFlagsType::_PROGMEM;
    for (stop = 0; (ch = pgm_read_byte(&str[stop])); stop++) {
        if (limit > 0 && ch == sep) {
            limit--;
            fun(str + start, stop - start, data, flags);
            start = stop + 1;
        }
    }
    fun(str + start, stop - start, data, flags);
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

#if 0

// old function for time zone support

tm *timezone_localtime(const time_t *timer)
{
    struct tm *_tm;
    time_t now;
    if (!timer) {
        time(&now);
    } else {
        now = *timer;
    }
    if (default_timezone.isValid()) {
        now += default_timezone.getOffset();
        _tm = gmtime(&now);
        _tm->tm_isdst = default_timezone.isDst() ? 1 : 0;
    } else {
        _tm = localtime(&now);
    }
    return _tm;
}

size_t timezone_strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm)
{
	String _z = F("%z");
    String fmt = FPSTR(format);
    if (default_timezone.isValid()) {
        if (fmt.indexOf(_z) != -1) {
            int32_t ofs = default_timezone.getOffset();
            char buf[8];
            snprintf_P(buf, sizeof(buf), PSTR("%c%02u:%02u"), ofs < 0 ? '-' : '+', (int)(std::abs(ofs) / 3600) % 24, (int)(std::abs(ofs) % 60));
            fmt.replace(_z, buf);
        }
		_z.toUpperCase();
        fmt.replace(_z, default_timezone.getAbbreviation());
        return strftime(buf, size, fmt.c_str(), tm);
    } else {
		fmt.replace(_z, emptyString);
        _z.toUpperCase();
        fmt.replace(_z, emptyString);
        return strftime(buf, size, fmt.c_str(), tm);
    }
}

size_t timezone_strftime(char *buf, size_t size, const char *format, const struct tm *tm)
{
    return timezone_strftime_P(buf, size, format, tm);
}

#endif

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
