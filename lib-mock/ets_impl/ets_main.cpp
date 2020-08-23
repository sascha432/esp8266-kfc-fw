/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "ets_sys_win32.h"
#include "LoopFunctions.h"

void setup();
void loop();

extern volatile bool ets_is_running;
volatile bool ets_is_running = true;

bool consoleHandler(int signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        ets_is_running = false;
    }
    return true;
}

int main() {
    SetConsoleOutputCP(65001);
    ESP._enableMSVCMemdebug();
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)consoleHandler, TRUE);
    DEBUG_HELPER_INIT();
    setup();
    do {
        loop();
        run_scheduled_functions();
        yield();
    } while (ets_is_running);
    return 0;
}
