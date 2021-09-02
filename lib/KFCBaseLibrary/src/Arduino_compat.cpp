/**
  Author: sascha_lammers@gmx.de
*/

#include "Arduino_compat.h"

// mode	description	starts..
// r	rb		open for reading (The file must exist)	beginning
// w	wb		open for writing (creates file if it doesn't exist). Deletes content and overwrites the file.	beginning
// a	ab		open for appending (creates file if it doesn't exist)	end
// r+	rb+	r+b	open for reading and writing (The file must exist)	beginning
// w+	wb+	w+b	open for reading and writing. If file exists deletes content and overwrites the file, otherwise creates an empty new file	beginning
// a+	ab+	a+b	open for reading and writing (append if file exists)	end

const char *fs::FileOpenMode::read = "r";
const char *fs::FileOpenMode::write = "w";
const char *fs::FileOpenMode::append = "a";
const char *fs::FileOpenMode::readplus = "r+";
const char *fs::FileOpenMode::writeplus = "w+";
const char *fs::FileOpenMode::appendplus = "a+";

int ___debugbreak_and_panic(const char *filename, int line, const char *function) {
#if DEBUG
    DEBUG_OUTPUT.printf_P(PSTR("___debugbreak_and_panic() called in %s:%u - %s\n"), filename, line, function);
    DEBUG_OUTPUT.flush();
#endif
#if _MSC_VER
    bool doPanic = false;
    __try {
        __debugbreak();
    }
    __except (GetExceptionCode() == EXCEPTION_BREAKPOINT ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        doPanic = true;
    }
    if (doPanic) {
        panic();
    }
#else
    panic();
#endif
    return 1;
}

#ifndef _MSC_VER

String __class_from_String(const char* str) {
    String name;
    size_t space = 0;
    while (str && *str) {
        if (*str == ':' || *str == '<') {
            break;
        }
        else {
            if (*str == ' ') {
                space = name.length();
            }
            name += *str;
        }
        str++;
    }
    if (space) {
        name.remove(0, space + 1);
    }
    return name;
}

#endif
