/**
  Author: sascha_lammers@gmx.de
*/

#include <stdint.h>
#include <string.h>
#include <pgmspace.h>
#include <memory>
#include <PrintString.h>
#include "misc_string.h"

// extern void progmem_str_func_test();

// // #if DEBUG
// // // test C library against PROGMEM to avoid any surprises if certain functions cannot handle PROGMEM
// // extern void progmem_str_func_test();
// // progmem_str_func_test();
// // #endif





// void progmem_str_func_test() {

//     auto test1 = PSTR("test string test string test!string test string test string ");
//     auto len1 = strlen_P(test1);
//     auto test2 = PSTR("test string");
//     auto len2 = strlen_P(test2);
//     auto test3Str = String(FPSTR(test1));
//     auto test3 = test3Str.c_str();

//     {
//         __DBG_printf("testing strrchr()");
//         auto res = strrchr(test1, '!');
//         res = strrchr(test1 + 1, '!');
//         res = strrchr(test1 + 2, '!');
//         res = strrchr(test1 + 3, '!');
//         res = strrchr(test1 + 4, '!') ;
//         res = strrchr(test1 + 5, '!');
//         __DBG_printf("strrchr() = PROGMEM safe %p", res);
//     }

//     {
//         __DBG_printf("testing strchr()");
//         auto res = strchr(test1, '!');
//         res = strchr(test1 + 1, '!');
//         res = strchr(test1 + 2, '!');
//         res = strchr(test1 + 3, '!');
//         res = strchr(test1 + 4, '!') ;
//         res = strchr(test1 + 5, '!');
//         __DBG_printf("strchr() = PROGMEM safe %p", res);
//     }

//     // cannot handle PROGMEM
//     // {
//     //     __DBG_printf("testing strcmp_P(PGM_P, PGM_P)");
//     //     auto res = strcmp_P(test1, test2 + 5);
//     //     res = strcmp_P(test1 + 1, test2 + 4);
//     //     res = strcmp_P(test1 + 2, test2 + 3);
//     //     res = strcmp_P(test1 + 3, test2 + 2);
//     //     res = strcmp_P(test1 + 4, test2 + 1);
//     //     res = strcmp_P(test1 + 5, test2);
//     //     __DBG_printf("strcmp_P() = PROGMEM safe %p", res);
//     // }

//     {
//         __DBG_printf("testing strcmp_PP(PGM_P, PGM_P)");
//         auto res = strcmp_PP(test1, test2 + 5);
//         res = strcmp_PP(test1 + 1, test2 + 4);
//         res = strcmp_PP(test1 + 2, test2 + 3);
//         res = strcmp_PP(test1 + 3, test2 + 2);
//         res = strcmp_PP(test1 + 4, test2 + 1);
//         res = strcmp_PP(test1 + 5, test2);
//         __DBG_printf("strcmp_PP() = PROGMEM safe %p", res);
//     }

//     // {
//     //     __DBG_printf("testing memcmp_P(PGM_P, PGM_P)");
//     //     auto res = memcmp_P(test1, test2 + 5, len2 - 5);
//     //     res = memcmp_P(test1 + 1, test2 + 4, len2 - 4);
//     //     res = memcmp_P(test1 + 2, test2 + 3, len2 - 3);
//     //     res = memcmp_P(test1 + 3, test2 + 2, len2 - 2);
//     //     res = memcmp_P(test1 + 4, test2 + 1, len2 - 1);
//     //     res = memcmp_P(test1 + 5, test2, len2);
//     //     __DBG_printf("memcmp_P() = PROGMEM safe %p", res);
//     // }

//     // {
//     //     __DBG_printf("testing strcasecmp_P(PGM_P, PGM_P)");
//     //     auto res = strcasecmp_P(test1, test2 + 5);
//     //     res = strcasecmp_P(test1 + 1, test2 + 4);
//     //     res = strcasecmp_P(test1 + 2, test2 + 3);
//     //     res = strcasecmp_P(test1 + 3, test2 + 2);
//     //     res = strcasecmp_P(test1 + 4, test2 + 1);
//     //     res = strcasecmp_P(test1 + 5, test2);
//     //     __DBG_printf("strcasecmp_P() = PROGMEM safe %p", res);
//     // }

//     // {
//     //     __DBG_printf("testing strncasecmp_P(PGM_P, PGM_P)");
//     //     auto res = strncasecmp_P(test1, test2 + 5, 10);
//     //     res = strncasecmp_P(test1 + 1, test2 + 4, 10);
//     //     res = strncasecmp_P(test1 + 2, test2 + 3, 10);
//     //     res = strncasecmp_P(test1 + 3, test2 + 2, 10);
//     //     res = strncasecmp_P(test1 + 4, test2 + 1, 10);
//     //     res = strncasecmp_P(test1 + 5, test2, 10);
//     //     __DBG_printf("strcasecmp_P() = PROGMEM safe %p", res);
//     // }

//     {
//         __DBG_printf("testing memchr_P()");
//         auto res2 = memchr_P(test1, '!', len1);
//         res2 = memchr_P(test1 + 1, '!', len1 - 1);
//         res2 = memchr_P(test1 + 2, '!', len1 - 2);
//         res2 = memchr_P(test1 + 3, '!', len1 - 3);
//         res2 = memchr_P(test1 + 4, '!', len1 - 4) ;
//         res2 = memchr_P(test1 + 5, '!', len1 * 5);
//         __DBG_printf("memchr_P() = PROGMEM safe %p", res2);

//     }





//     // strcasestr cannot take progmem for either argument
//     // __DBG_printf("testing strcasestr(PGM_P, char *)");
//     // res = strcasestr(test1, "!");
//     // res = strcasestr(test1 + 1, "!");
//     // res = strcasestr(test1 + 3, "!");
//     // res = strcasestr(test1 + 4, "!") ;
//     // res = strcasestr(test1 + 5, "!");
//     // __DBG_printf("testing strcasestr(PGM_P, PGM_P)");
//     // res = strcasestr(test1, test2 + 5);
//     // res = strcasestr(test1 + 1, test2 + 4);
//     // res = strcasestr(test1 + 3, test2 + 3);
//     // res = strcasestr(test1 + 4, test2 + 2) ;
//     // res = strcasestr(test1 + 5, test2 + 1);
//     // res = strcasestr(test1 + 6, test2);
//     // __DBG_printf("testing strcasestr(char *, PGM_P)");
//     // res = strcasestr(test3, test2 + 5);
//     // res = strcasestr(test3 + 1, test2 + 4);
//     // res = strcasestr(test3 + 3, test2 + 3);
//     // res = strcasestr(test3 + 4, test2 + 2) ;
//     // res = strcasestr(test3 + 5, test2 + 1);
//     // res = strcasestr(test3 + 6, test2);
//     // __DBG_printf("strcasestr() = PROGMEM safe %p", res);


// }

int stringlist_find_P_P(PGM_P list, PGM_P find, PGM_P separator/*, int &position*/)
{
    if (!list || !find || !separator) {
        return -1;
    }
    PGM_P ptr1 = list;
    PGM_P ptr2 = find;
    uint8_t ch;
    int16_t num = 0;
    // position = 0;
    do {
        ch = pgm_read_byte(ptr1);
        if (ch == '*') { // end of one string, expanding with wildcard
            if (ptr2) { // stop comparing here, match
                return num;
            }
            else {
                ptr2 = find; // reset to compare with next string
                num++;
                // move to next string in list
                while ((ch = pgm_read_byte(++ptr1)) != 0 && !strchr_P(separator, ch)) {
                }
            }
        }
        else if (strchr_P(separator, ch)) {    // end of one string
            if (ptr2 && pgm_read_byte(ptr2++) == 0) { // match
                return num;
            }
            else {
                // position = ptr1 - list;
                ptr2 = find; // reset to compare with next string
                num++;
                // ptr1 points to the separator already, advancing to the next string in the list
            }
        }
        else {
            if (ptr2 && pgm_read_byte(ptr2++) != ch) { // mismatch, stop comparing
                ptr2 = nullptr;
            }
            else if (ptr2 && !ch) { // match
                return num;
            }
        }
        ptr1++;
    }
    while(ch);

    // position = -1;
    return -1;
}


int stringlist_ifind_P_P(PGM_P list, PGM_P find, PGM_P separator/*, int &position*/)
{
    if (!list || !find || !separator) {
        return -1;
    }
    PGM_P ptr1 = list;
    PGM_P ptr2 = find;
    uint8_t ch;
    int16_t num = 0;
    // position = 0;
    do {
        ch = tolower(pgm_read_byte(ptr1));
        if (strichr_P(separator, ch)) {    // end of one string
            if (ptr2 && pgm_read_byte(ptr2++) == 0) { // match
                return num;
            } else {
                // position = ptr1 - list;
                ptr2 = find; // compare with next string
                num++;
            }
        } else {
            if (ptr2 && tolower(pgm_read_byte(ptr2++)) != ch) { // mismatch, stop comparing
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

char *strichr(char *str, int c)
{
    if (!str) {
        return nullptr;
    }
    if (!c) {
        // special case: find end of string
        return str + strlen(str);
    }
    if (toupper(c) == c) {
        // special case: lower and upercase are the same
        return strchr(str, c);
    }
    c = tolower(static_cast<uint8_t>(c)); // cast to uint8_t since pgm_read_byte returns unsigned
    int ch;
    while((ch = tolower(static_cast<uint8_t>(*str))) != c) {
        if (ch == 0) {
            return nullptr;
        }
        str++;
    }
    return str;
}

PGM_P strichr_P(PGM_P str, int c)
{
    if (!str) {
        return nullptr;
    }
    if (!c) {
        return str + strlen_P(str);
    }
    c = tolower(static_cast<uint8_t>(c)); // cast to uint8_t since pgm_read_byte returns unsigned
    int ch;
    while ((ch = tolower(pgm_read_byte(str))) != c) {
        if (ch == 0) {
            return nullptr;
        }
        str++;
    }
    return str;
}

#if defined(ESP8266) || defined(ESP32)

PGM_P strrchr_P(PGM_P str, int c)
{
    if (!str) {
        return nullptr;
    }
    c = static_cast<uint8_t>(c); // cast to uint8_t since pgm_read_byte returns unsigned
    PGM_P found = nullptr;
    while (true) {
        auto ch = pgm_read_byte(str);
        if (ch == c) {
            found = str;
        }
        if (ch == 0) {
            return found;
        }
        str++;
    }
}

PGM_P strchr_P(PGM_P str, int c)
{
    uint8_t ch;
    c = static_cast<uint8_t>(c); // cast to uint8_t since pgm_read_byte returns unsigned
    while ((ch = pgm_read_byte(str)) != c) {
        if (ch == 0) {
            return nullptr;
        }
        str++;
    }
    return str;
}

#endif


int strncmp_P_P(PGM_P str1, PGM_P str2, size_t size)
{
    if (!size) {
        return ESZEROL;
    }
    if (!str1) {
        return ESNULLP;
    }
    if (!str2) {
        return -ESNULLP;
    }
    if (str1 == str2) {
        return 0;
    }
    uint8_t ch1, ch2;
    do {
        ch1 = pgm_read_byte(str1++);
        ch2 = pgm_read_byte(str2++);
        if (ch1 < ch2) {
            return -1;
        }
        else if (ch1 > ch2) {
            return 1;
        }
        else if (ch1 == 0 || --size == 0) {
            return 0;
        }
    } while (true);
}

int strncasecmp_P_P(PGM_P str1, PGM_P str2, size_t size)
{
    if (!size) {
        return ESZEROL;
    }
    if (!str1) {
        return ESNULLP;
    }
    if (!str2) {
        return -ESNULLP;
    }
    if (str1 == str2) {
        return 0;
    }
    uint8_t ch1, ch2;
    do {
        ch1 = tolower(pgm_read_byte(str1++));
        ch2 = tolower(pgm_read_byte(str2++));
        if (ch1 < ch2) {
            return -1;
        }
        else if (ch1 > ch2) {
            return 1;
        }
        else if (ch1 == 0 || --size == 0) {
            return 0;
        }
    } while (true);
}


PGM_P strstr_P_P(PGM_P str, PGM_P find, size_t findLen) {
    if (!str || !find) {
        return nullptr;
    }
    if (findLen == ~0U) {
        findLen = strlen_P(find);
    }
    if (str == find || (findLen == 0)) {
        return str;
    }
    size_t strLen =  strlen_P(str);
    while (strLen >= findLen) {
        if (strncmp_P_P(find, str, findLen) == 0) {
            return str;
        }
        strLen--;
        str++;
    }
    return nullptr;
}

PGM_P strcasestr_P_P(PGM_P str, PGM_P find, size_t findLen) {
    if (!str || !find) {
        return nullptr;
    }
    if (findLen == ~0U) {
        findLen = strlen_P(find);
    }
    if (str == find || (findLen == 0)) {
        return str;
    }
    size_t strLen =  strlen_P(str);
    while (strLen >= findLen) {
        if (strncasecmp_P_P(find, str, findLen) == 0) {
            return str;
        }
        strLen--;
        str++;
    }
    return nullptr;
}

char *strcasestr_P(char *str, PGM_P find, size_t findLen) {
    if (!str || !find) {
        return nullptr;
    }
    if (findLen == ~0U) {
        findLen = strlen_P(find);
    }
    if (str == find || (findLen == 0)) {
        return str;
    }
    size_t strLen =  strlen(str);
    while (strLen >= findLen) {
        if (strncasecmp_P(str, find, findLen) == 0) {
            return str;
        }
        strLen--;
        str++;
    }
    return nullptr;
}


char *__strrstr(char *str, size_t stringLen, const char *find, size_t findLen)
{
	if (findLen > stringLen)
		return nullptr;

	for (auto ptr = str + stringLen - findLen; ptr >= str; ptr--) {
		if (strncmp(ptr, find, findLen) == 0) {
			return ptr;
        }
    }
	return nullptr;
}

char *__strrstr_P(char *str, size_t stringLen, PGM_P find, size_t findLen)
{
	if (findLen > stringLen) {
        return nullptr;
    }

	for (auto ptr = str + stringLen - findLen; ptr >= str; ptr--) {
		if (strncmp_P(ptr, find, findLen) == 0) {
			return ptr;
        }
    }
	return nullptr;
}

PGM_P __strrstr_P_P(PGM_P  str, size_t stringLen, PGM_P  find, size_t findLen)
{
	if (findLen > stringLen) {
        return nullptr;
    }

	for (auto ptr = str + stringLen - findLen; ptr >= str; ptr--) {
		if (strncmp_P_P(ptr, find, findLen) == 0) {
			return ptr;
        }
    }
	return nullptr;
}

inline static size_t String_rtrim_zeros(String &str, size_t minLength)
{
    auto len = str.length();
    if (len) {
        minLength = len - minLength;
        auto cStr = str.c_str();
        while (minLength-- && len && cStr[len - 1] == '0') {
            len--;
        }
        str.remove(len);
    }
    return len;
}

size_t printTrimmedDouble(Print *output, double value, int digits)
{
    auto str = PrintString(F("%.*f"), digits, value);
    auto size = String_rtrim_zeros(str, str.indexOf('.') + 2); // min. length dot + 1 char to avoid getting "1." for "1.0000"
    if (output) {
        return output->print(str);
    }
    return size;
}

#if ESP32

bool str_endswith(const char *str, char ch)
{
    if (!str) {
        return false;
    }
    auto len = strlen(str);
    return (len != 0) && (str[len - 1] == ch);
}

#elif ESP8266

bool str_endswith_P(PGM_P str, char ch)
{
    if (!str) {
        return false;
    }
    auto len = strlen_P(str);
    return (len != 0) && (pgm_read_byte(str + len - 1) == ch);
}

#endif

int strcmp_end(const char *str1, size_t len1, const char *str2, size_t len2)
{
    if (!str1) {
        return ESNULLP;
    }
    if (!str2) {
        return -ESNULLP;
    }
    if (len2 == len1) {
        return strcmp(str1, str2);
    }
    return strcmp(str1 + len1 - len2, str2);
}

int strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2)
{
    if (!str1) {
        return ESNULLP;
    }
    if (!str2) {
        return -ESNULLP;
    }
    if (len2 == len1) {
        return strcmp_P(str1, str2);
    }
    return strcmp_P(str1 + len1 - len2, str2);
}

int strcmp_end_P_P(PGM_P str1, size_t len1, PGM_P str2, size_t len2)
{
    if (!str1) {
        return ESNULLP;
    }
    if (!str2) {
        return -ESNULLP;
    }
    if (len2 > len1) {
        return -1;
    }
    if (len2 == len1) {
        return strcmp_P_P(str1, str2);
    }
    return strcmp_P_P(str1 + len1 - len2, str2);
}

#if defined(ESP8266) || defined(ESP32)

char *strdup_P(PGM_P src)
{
    if (!src) {
        return nullptr;
    }
    auto len = strlen_P(src) + 1;
    char *dst = reinterpret_cast<char *>(malloc(len));
    if (!dst) {
        return nullptr;
    }
    memcpy_P(dst, src, len);
    return dst;
}


#endif
