/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <thread>
#include <mutex>
#include "ets_sys_win32.h"

//volatile bool __ets_is_running = true;
std::atomic_bool __ets_is_running = true;
std::chrono::steady_clock::time_point micros_start_time = std::chrono::high_resolution_clock::now();

uint64_t micros64()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - micros_start_time).count();
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - micros_start_time).count();
}

unsigned long millis(void)
{
    return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - micros_start_time).count());
}

unsigned long micros(void)
{
    return static_cast<unsigned long>(micros64());
}

void delay(unsigned long time_ms)
{
    ets_timer_delay(time_ms);
}

void delayMicroseconds(unsigned int us)
{
    ets_timer_delay_us(us);
}

bool __ets_is_loop_running()
{
    return __ets_is_running;
}

static std::thread ets_main_thread;

static void ets_main_thread_stop()
{
    if (ets_main_thread.joinable()) {
        __DBG_printf("ets_main_thread join");
        ets_main_thread.join();
    }
}

void __ets_end_loop()
{
    __ets_is_running = false;
    __DBG_printf("__ets_end_loop invoked=1");
    ets_timer_deinit();
    ets_main_thread_stop();
}

static bool __ets_console_handler(int signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
        __ets_end_loop();
        return true;
    }
    return false;
}

void __ets_post_loop_exec_loop_functions()
{
    auto &loopFunctions = LoopFunctions::getVector();
    bool cleanup = false;
    for (uint8_t i = 0; i < loopFunctions.size(); i++) { // do not use iterators since the vector can be modifed inside the callback
        if (loopFunctions[i].deleteCallback) {
            cleanup = true;
        }
        else {
            _Scheduler.run(Event::PriorityType::NORMAL); // check priority above NORMAL after every loop function
            _ASSERTE(_CrtCheckMemory());
            if (loopFunctions[i].callback) {
                loopFunctions[i].callback();
                _ASSERTE(_CrtCheckMemory());
            }
            else {
                loopFunctions[i].callbackPtr();
                _ASSERTE(_CrtCheckMemory());
            }
        }
    }
    if (cleanup) {
        loopFunctions.erase(std::remove_if(loopFunctions.begin(), loopFunctions.end(), [](const LoopFunctions::Entry &entry) { return entry.deleteCallback == true; }), loopFunctions.end());
        loopFunctions.shrink_to_fit();
    }
    _Scheduler.run(); // check all events
    _ASSERTE(_CrtCheckMemory());
    run_scheduled_functions();
}

static void start_main_thread();

void __ets_pre_setup()
{
    micros_start_time = std::chrono::high_resolution_clock::now();
#if !ETS_MAIN_NO_TIMERS
    ets_timer_init();
#endif
    SetConsoleOutputCP(CP_UTF8);
    ESP._enableMSVCMemdebug();
#if !ETS_MAIN_NO_TIMERS
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)__ets_console_handler, TRUE);
#endif
    DEBUG_HELPER_INIT();

#if !ETS_MAIN_NO_LOOP
    start_main_thread();
#endif
}

#if ETS_MAIN_NO_MAIN_FUNC

static void start_main_thread()
{
    ets_main_thread = std::thread([]() {
        while (__ets_is_running) {
            __ets_post_loop_exec_loop_functions();
            _ASSERTE(_CrtCheckMemory());
            __ets_post_loop_do_yield();
            _ASSERTE(_CrtCheckMemory());
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        __DBG_printf("ets_main_thread ended=1");
        ets_timer_deinit();
        _ASSERTE(_CrtCheckMemory());
    });
}

#else

static void start_main_thread()
{
}

void preinit() 
{
}

int main()
{
    __ets_pre_setup();
    preinit();
    setup();
    _ASSERTE(_CrtCheckMemory());
    do {
        loop();
        _ASSERTE(_CrtCheckMemory());
        __ets_post_loop_exec_loop_functions();
        _ASSERTE(_CrtCheckMemory());
        __ets_post_loop_do_yield();
        _ASSERTE(_CrtCheckMemory());
    }
    while (__ets_is_running);
    __DBG_printf("ets_main_loop ended=1");
    _ASSERTE(_CrtCheckMemory());
    ets_timer_deinit();
    _ASSERTE(_CrtCheckMemory());
    return 0;
}

#endif
