/**
  Author: sascha_lammers@gmx.de
*/

#include <functional>
#include <PrintString.h>
#include "misc.h"

const String _sharedEmptyString;

PROGMEM_STRING_DECL(slash);
PROGMEM_STRING_DECL(SPIFFS_tmp_dir);

#if _WIN32
PROGMEM_STRING_DEF(slash, "/");
PROGMEM_STRING_DEF(SPIFFS_tmp_dir, "c:/temp/");
#endif

String formatBytes(size_t bytes) {
    char buf[8];
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

String formatTime(unsigned long seconds, bool days_if_not_zero) {
    PrintString out;
    unsigned int days = (unsigned int)(seconds / 86400);
    if (!days_if_not_zero || days) {
        out.printf_P(PSTR("%u days "), days);
    }
    out.printf_P(PSTR("%02uh:%02um:%02us"), (unsigned)((seconds / 3600) % 24), (unsigned)((seconds / 60) % 60), (unsigned)(seconds % 60));
    return out;
}

String implode(const __FlashStringHelper *glue, const char **pieces, int count) {
    String tmp;
    if (count > 0) {
        tmp += *pieces++;
        for(int i = 1; i < count; i++) {
            tmp += glue;
            tmp += *pieces++;
        }
    }
    return tmp;
}

String url_encode(const String &str) {
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

String printable_string(const uint8_t *buffer, size_t length) {
    PrintString out_str;
    const char *ptr = (const char *)buffer;
    while(length--) {
        if (isprint(*ptr)) {
            out_str += *ptr;
        } else {
            out_str.printf_P(PSTR("\\%02X"), (int)(*ptr & 0xff));
        }
        ptr++;
    }
    return out_str;
}

String append_slash_copy(const String &dir) {
    if (dir.length() == 0 || (dir.length() != 0 && dir.charAt(dir.length() - 1) != '/')) {
        return dir;
    } else {
        String tmp = dir;
        tmp += '/';
        return tmp;
    }
}

void append_slash(String &dir) {
    if (dir.length() == 0 || (dir.length() != 0 && dir.charAt(dir.length() - 1) != '/')) {
        dir += '/';
    }
}

void remove_trailing_slash(String &dir) {
    if (dir.length() && dir.charAt(dir.length() - 1) == '/') {
        dir.remove(dir.length() - 1);
    }
}

String sys_get_temp_dir() {
    return FSPGM(SPIFFS_tmp_dir);
}

#if SPIFFS_TMP_FILES_TTL
File tmpfile(const String &dir, const String &prefix, long ttl) {
    String tmp = append_slash_copy(dir);
    tmp += String((millis() / 1000UL) + ttl, HEX);
    if (isxdigit(prefix.charAt(0))) {
        tmp += F("_"); // add separator if next character is a hex digit
    }
    tmp += prefix;
#else
File tmpfile(const String &dir, const String &prefix) {
    String tmp = append_slash_copy(dir);
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

    return SPIFFS.open(tmp, "w");
}

String WiFi_disconnect_reason(WiFiDisconnectReason reason) {
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
            out.printf_P(PSTR("UKNOWN_REASON_%d"), reason);
            return out;
    }
#elif defined(ESP8266)
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
            out.printf_P(PSTR("UKNOWN_REASON_%d"), reason);
            return out;
    }
#else
#error Platform not supported
#endif
}

int16_t stringlist_find_P_P(PGM_P list, PGM_P find, char separator) {
    char separator_str[2] = { separator, 0 };
    return stringlist_find_P_P(list, find, separator_str);
}

int16_t stringlist_find_P_P(PGM_P list, PGM_P find, const char *separator/*, int &position*/) {
    PGM_P ptr1 = list;
    PGM_P ptr2 = find;
    uint8_t ch;
    int16_t num = 0;
    // position = 0;
    do {
        ch = pgm_read_byte(ptr1);
        if (strchr(separator, ch)) {    // end of one string
            if (ptr2 && pgm_read_byte(ptr2) == 0) { // match
                return num;
            } else {
                // position = ptr1 - list;
                ptr2 = find; // compare with next string
                num++;
            }
        } else {
            if (ptr2 && pgm_read_byte(ptr2) != ch) { // mismatch, stop comparing
                ptr2 = nullptr;
            } else if (ptr2 && !ch) { // match
                return num;
            }
        }
    } while(ch);

    // position = -1;
    return -1;
}

int strcmp_P_P(PGM_P str1, PGM_P str2) {
    uint8_t ch;
    do {
        ch = pgm_read_byte(str1);
        if (ch != pgm_read_byte(str2)) {
            return -1;
        }
    } while (ch);
    return 0;
}

int strcasecmp_P_P(PGM_P str1, PGM_P str2) {
    uint8_t ch;
    do {
        ch = pgm_read_byte(str1);
        if (tolower(ch) != tolower(pgm_read_byte(str2))) {
            return -1;
        }
    } while (ch);
    return 0;
}

bool String_equals(const String &str1, PGM_P str2, size_t strlen2) {
    if (str1.length() != strlen2) {
        return false;
    }
    return !strcmp_P(str1.c_str(), str2);
}

bool String_equalsIgnoreCase(const String &str1, PGM_P str2, size_t strlen2) {
    if (str1.length() != strlen2) {
        return false;
    }
    return !strcasecmp_P(str1.c_str(), str2);
}

int  strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2) {
    if (len2 > len1) {
        return false;
    } else if (len2 == len1) {
        return strcmp_P(str1, str2);
    }
    return strcmp_P(str1 + len1 - len2, str2);
}

bool __while(uint32_t time_in_ms, std::function<bool()> loop, uint16_t interval_in_ms, std::function<bool()> intervalLoop) {
    unsigned long end = millis() + time_in_ms;
    //debug_printf_P(PSTR("__while(%d, %p, %d, %p)\n"), time_in_ms, &loop, interval_in_ms, &intervalLoop);
    while(millis() < end) {
        delay(1);
        if (interval_in_ms) {
            if (!__while(interval_in_ms, loop) || !intervalLoop()) {
                return false;
            }
        } else if (loop && !loop()) {
            return false;
        }
    }
    return true;
}

bool __while(uint32_t time_in_ms, uint16_t interval_in_ms, std::function<bool()> intervalLoop) {
    return __while(time_in_ms, nullptr, interval_in_ms, intervalLoop);
}

bool __while(uint32_t time_in_ms, std::function<bool()> loop) {
    return __while(time_in_ms, loop, 0, nullptr);
}

char nibble2hex(uint8_t nibble, char hex_char) {
    return nibble > 9 ? nibble + hex_char : nibble + '0';
}

const char *mac2char_s(char *dst, size_t size, const uint8_t *mac, char separator) {
    constexpr uint8_t len = 6;
    if (!mac) {
        strncpy_P(dst, PSTR("null"), size);
        return dst;
    }
    int8_t count = len;
    char *ptr = dst;
    while(count-- && size > 1) {
        *ptr++ = nibble2hex((*mac & 0xf0) >> 4);
        *ptr++ = nibble2hex(*mac & 0xf);
        size -= 2;
        if (count && separator && size--) {
            *ptr++ = separator;
        }
        mac++;
    }
    *ptr = 0;
    return dst;
}

String mac2String(const uint8_t *mac, char separator) {
    char buf[MAC2STRING_LEN];
    return mac2char_s(buf, sizeof(buf), mac, separator);
}

String inet_ntoString(uint32_t ip) {
    char buf[INET_NTOA_LEN];
    return inet_ntoa_s(buf, sizeof(buf), ip);
}

const char *inet_ntoa_s(char *dst, size_t size, uint32_t ip) {
    uint8_t *bip = (uint8_t *)&ip;
    snprintf_P(dst, size, PSTR("%u.%u.%u.%u"), bip[0], bip[1], bip[2], bip[3]);
    return dst;
}
