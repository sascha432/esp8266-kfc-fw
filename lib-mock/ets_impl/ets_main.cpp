/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "ets_sys_win32.h"
#include "LoopFunctions.h"

extern volatile bool ets_is_running;
volatile bool ets_is_running = true;

bool is_loop_running() {
    return ets_is_running;
}

void end_loop() {
    ets_is_running = false;
}

void __no_setup() {
}

void __no_loop() {
    end_loop();
}

#pragma comment(linker, "/alternatename:loop=__no_loop")

bool consoleHandler(int signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
        ets_is_running = false;
    }
    return true;
}

int main() {
    SetConsoleOutputCP(65001);
    ESP._enableMSVCMemdebug();
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)consoleHandler, TRUE);
    DEBUG_HELPER_INIT();
    _ASSERTE(_CrtCheckMemory());
    setup();
    _ASSERTE(_CrtCheckMemory());
    do {
        loop();
        _ASSERTE(_CrtCheckMemory());
        run_scheduled_functions();
        _ASSERTE(_CrtCheckMemory());
        yield();
        _ASSERTE(_CrtCheckMemory());
    } while (ets_is_running);
    return 0;
}
