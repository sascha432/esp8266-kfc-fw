/**
  Author: sascha_lammers@gmx.de
*/

#include "ets_win_includes.h"
#include "ets_sys_win32.h"
#include "ets_timer_win32.h"
#include <chrono>
#include <thread>
#include <mutex>

#pragma pack(push, 4)

constexpr auto etstimer_zerotime = std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds>(std::chrono::microseconds(0));
static constexpr uint32_t __ETSTimerInitVal = 0xa23542f3;

static ETSTimer *init_ets_timers() {
	static ETSTimer root;
    root.init = __ETSTimerInitVal;
    root.timer_expire = etstimer_zerotime;
	return &root;
}

ETSTimer *timer_list = init_ets_timers();
static std::mutex __ets_timer_list_mutex;
static std::mutex __ets_timer_callback_mutex;
static bool __ets_timer_callback;

bool can_yield()
{
    return (__ets_timer_callback == false);
}

void __loop_do_yield() 
{
    __ets_timer_callback_mutex.lock();
    yield();
    __ets_timer_callback_mutex.unlock();
}

void yield()
{
    if (__ets_timer_callback) {
        //panic();
        __debugbreak();
    }
    Sleep(1);
}

void __start_timer_thread_func()
{
    auto thread = std::thread([]() {
        while (ets_is_running) {
            __ets_timer_list_mutex.lock();
            ETSTimer *cur = timer_list;
            do {
                if (can_yield() && cur->timer_func && cur->timer_arg && cur->timer_expire != etstimer_zerotime) {
                    if (std::chrono::high_resolution_clock::now() > cur->timer_expire) {
                        if (cur->timer_period) {
                            cur->timer_expire += std::chrono::microseconds(cur->timer_period);
                        }
                        else {
                            cur->timer_expire = etstimer_zerotime;
                        }
                        __ets_timer_list_mutex.unlock();
                        __ets_timer_callback_mutex.lock();
                        __ets_timer_callback = true;
                        cur->timer_func(cur->timer_arg);
                        __ets_timer_callback = false;
                        __ets_timer_callback_mutex.unlock();
                        __ets_timer_list_mutex.lock();
                    }
                }
                cur = cur->timer_next;
            } while (cur);

            __ets_timer_list_mutex.unlock();
            Sleep(1);
        }
    });
    thread.detach();
}

static ETSTimer *find_timer(ETSTimer *timer)
{
    ETSTimer *cur = timer_list;
    ETSTimer *prev = timer_list;
    while(cur != timer && cur->timer_next) {
        prev = cur;
        cur = cur->timer_next;
    }
    if (cur == timer && prev->timer_next == timer) {
        return prev;
    }
    return nullptr;
}

static void add_timer(ETSTimer *timer)
{
    ETSTimer *cur = timer_list;
    while(cur->timer_next) {
        cur = cur->timer_next;
    }
    cur->timer_next = timer;
    timer->timer_next = nullptr;
}

static void remove_timer(ETSTimer *timer)
{
    ETSTimer *cur = timer_list;
    ETSTimer *prev = find_timer(timer);
    if (prev) {
        if (prev->timer_next == timer) {
            prev->timer_next = timer->timer_next;
            timer->timer_next = nullptr;
            timer->init = 0;
        }
    }
}

void ets_timer_delay(uint32_t time_ms)
{
    Sleep(time_ms);
}

void ets_timer_delay_us(uint64_t time_us)
{
    auto end = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(time_us);
    auto ms = time_us / 1000LL;
    auto us = time_us % 1000LL;
    if (ms > 5) {
        ms -= 5;
    }
    if (ms) {
        ets_timer_delay((uint32_t)ms);
    }
    while (std::chrono::high_resolution_clock::now() < end) {
    }
}

/*
*/

static bool timer_initialized(ETSTimer *ptimer)
{
    return ptimer->init == __ETSTimerInitVal && find_timer(ptimer);
}

void ets_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
{
    __ets_timer_list_mutex.lock();
    if (!timer_initialized(ptimer)) {
        *ptimer = {};
        ptimer->init = __ETSTimerInitVal;
        add_timer(ptimer);
    }
    ptimer->timer_func = pfunction;
    ptimer->timer_arg = parg;
    __ets_timer_list_mutex.unlock();
}

void ets_timer_arm_us(ETSTimer *ptimer, uint32_t time_us, bool repeat_flag)
{
    __ets_timer_list_mutex.lock();
    assert(timer_initialized(ptimer));

    ptimer->timer_period = repeat_flag ? time_us : 0;
    ptimer->timer_expire = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(time_us);
    __ets_timer_list_mutex.unlock();
}

void ets_timer_arm(ETSTimer *ptimer, uint32_t time_ms, bool repeat_flag)
{
    __ets_timer_list_mutex.lock();
    uint64_t time_us = 1000ULL * (uint64_t)time_ms;

    assert(timer_initialized(ptimer));
    ptimer->timer_period = repeat_flag ? (uint32_t)time_us : 0;
    ptimer->timer_expire = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(ptimer->timer_period);
    __ets_timer_list_mutex.unlock();
}

void ets_timer_arm_new(ETSTimer *ptimer, int time_ms, int repeat_flag, int isMillis)
{
    if (isMillis) {
        ets_timer_arm(ptimer, time_ms, repeat_flag);
    }
    else {
        ets_timer_arm_us(ptimer, time_ms, repeat_flag);
    }
}

void ets_timer_done(ETSTimer *ptimer)
{
    __ets_timer_list_mutex.lock();
    if (timer_initialized(ptimer)) {
        remove_timer(ptimer);
        ptimer->timer_arg = nullptr;
    }
    __ets_timer_list_mutex.unlock();
}

void ets_timer_disarm(ETSTimer *ptimer)
{
    __ets_timer_list_mutex.lock();
    if (timer_initialized(ptimer)) {
        auto prev = find_timer(ptimer);
        if (prev) {
            ptimer->timer_expire = etstimer_zerotime;
            ptimer->timer_period = 0;
        }
    }
    __ets_timer_list_mutex.unlock();
}

void ets_timer_init(void)
{
}

void ets_timer_deinit(void)
{
}

esp_err_t esp_timer_init()
{
    return ESP_OK;
}

esp_err_t esp_timer_deinit()
{
    return ESP_OK;
}

void os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg) {
    ets_timer_setfn(ptimer, pfunction, parg);
}

void os_timer_disarm(ETSTimer *ptimer) {
    ets_timer_disarm(ptimer);
}

void os_timer_arm_us(ETSTimer *ptimer, uint32_t u_seconds, bool repeat_flag) {
    ets_timer_arm_us(ptimer, u_seconds, repeat_flag);
}

void os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag) {
    ets_timer_arm(ptimer, milliseconds, repeat_flag);
}
void os_timer_done(ETSTimer *ptimer) {
    ets_timer_done(ptimer);
}

#pragma pack(pop)
