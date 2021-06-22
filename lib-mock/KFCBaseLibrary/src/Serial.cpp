/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if _MSC_VER

#include <Arduino_compat.h>
#include "Serial.h"

static DWORD console_mode;
static auto console_initialized = 0;
static auto console_last_ch = -1;

void console_init()
{
    if (console_initialized++) {
        return;
    }
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL) {
        __DBG_printf("console not available");
        console_initialized = 0;
        return;
    }
    GetConsoleMode(h, &console_mode);
    SetConsoleMode(h, console_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
    console_initialized = true;
}

void console_done()
{
    if (console_initialized <= 0) {
        return;
    }
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == NULL) {
        __DBG_printf("console not available");
        console_initialized = 0;
        return;
    }
    SetConsoleMode(h, console_mode);
    if (--console_initialized <= 0) {
        console_last_ch = -1;
    }
}

CHAR console_getch() 
{
    if (console_last_ch != -1) {
        console_last_ch = -1;
        return console_last_ch;
    }
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (!h) {
        return -1;
    }

    DWORD events;
    if (!GetNumberOfConsoleInputEvents(h, &events) || events == 0) {
        return -1;
    }

    INPUT_RECORD buffer[1];
    if (!ReadConsoleInput(h, buffer, 1, &events) || events == 0 || buffer->EventType != KEY_EVENT || buffer->Event.KeyEvent.bKeyDown == FALSE) {
        return -1;
    }
    console_last_ch = -1;
    return buffer->Event.KeyEvent.uChar.AsciiChar;
}

CHAR console_peekch()
{
    if (console_last_ch != -1) {
        return console_last_ch;
    }
    return (console_last_ch = console_getch());
}

HardwareSerial::HardwareSerial() : Stdout() 
{
    console_init();
}

HardwareSerial::~HardwareSerial()
{
    end();
}

void HardwareSerial::begin(int baud)
{
    console_init();
}

void HardwareSerial::end()
{
    console_done();
}

int HardwareSerial::available()
{
    return console_peekch() != -1;
}

int HardwareSerial::read()
{
    return console_getch();
}

int HardwareSerial::peek()
{
    return console_peekch();
}

void HardwareSerial::flush(void)
{
    fflush(stdout);
}

size_t FakeSerial::write(uint8_t c)
{
    return write(&c, 1);
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    ::printf("%*.*s", size, size, buffer);
    _RPTN(_CRT_WARN, "%*.*s", size, size, buffer);
    return size;
}


HardwareSerial fakeSerial;
HardwareSerial &Serial0 = fakeSerial;
Stream &Serial = fakeSerial;

#endif
