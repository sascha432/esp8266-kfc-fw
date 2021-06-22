/**
  Author: sascha_lammers@gmx.de
*/

#include "ets_win_includes.h"
#include "ets_sys_win32.h"
#include "ets_timer_win32.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <LoopFunctions.h>
#include <thread>
#include <mutex>
#include <list>

#undef max

#pragma pack(push, 4)

static constexpr uint32_t __ETSTimerInitVal = 0xa23542f3;

ETSTimer *timer_list = nullptr;
using ETSTimerList = std::vector<ETSTimer *>;

static std::mutex __ets_timer_list_mutex;
static std::condition_variable __ets_timer_thread_wakeup;
static std::atomic_bool __ets_timer_thread_end;
static std::thread __ets_timer_thread;
static std::atomic_bool __ets_timer_callback;
static ETSTimer root;

bool can_yield()
{
    return (__ets_timer_callback == false);
}

void __ets_post_loop_do_yield()
{
    __ets_timer_list_mutex.lock();
    std::this_thread::yield();
    run_scheduled_functions();
    __ets_timer_list_mutex.unlock();
}

void yield()
{
    if (!can_yield()) {
        __debugbreak();
    }
    std::this_thread::yield();
}

static ETSTimer *find_timer(ETSTimer *timer)
{
    assert(timer_list);
    auto cur = timer_list;
    auto prev = cur;
    while (cur != timer && cur->timer_next) {
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
    assert(timer_list);
    ETSTimer *cur = timer_list;
    while (cur->timer_next) {
        cur = cur->timer_next;
    }
    cur->timer_next = timer;
    timer->timer_next = nullptr;
}

static void remove_timer(ETSTimer *timer)
{
    if (timer_list) {
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
}

static void dump_timers(const ETSTimerList &timers, Stream &output)
{
    output.printf_P("timer count=%u\n", timers.size());
    auto now = std::chrono::high_resolution_clock::now();
    for (auto timer : timers) {
        output.printf_P(PSTR("sort timer=%p next=%p expire=%lld period=%lld\n"), timer, timer->timer_next, std::chrono::duration_cast<std::chrono::milliseconds>(timer->timer_expire - now).count(), timer->timer_period);
    }
}

static void order_timers(ETSTimerList &_timers)
{
    _timers.clear();
    auto cur = timer_list;
    while (cur) {
        if (cur->timer_func && cur->timer_arg && cur->timer_expire != std::chrono::steady_clock::time_point::max()) {
            _timers.push_back(cur);
        }
        cur = cur->timer_next;
    }
    std::sort(_timers.begin(), _timers.end(), [](const ETSTimer *a, const ETSTimer *b) {
        return a->timer_expire < b->timer_expire;
    });
}

static void dump_order_timers(Stream &output = Serial)
{
    ETSTimerList timers;
    order_timers(timers);
    dump_timers(timers, output);
}

void ets_dump_timer(Stream &output)
{
    dump_order_timers(output);
}

static String this_thread_id()
{
    std::stringbuf buffer;
    std::ostream os(&buffer);
    os << "thread " << std::this_thread::get_id();
    return buffer.str().c_str();
}

static std::thread create_timer_thread()
{
    __ets_timer_thread_end = false;
    return std::thread([]() {
        while (!__ets_timer_thread_end) {
            std::chrono::steady_clock::time_point wait_until = std::chrono::steady_clock::time_point::max();
            {
                // lock timer_list
                std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);

                auto now = std::chrono::high_resolution_clock::now();
                // order by time
                ETSTimerList timers;
                order_timers(timers);
                if (timers.size()) {
                    auto &timer = timers.front();
                    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(timer->timer_expire - now).count();
                    if (diff > 0) { // nothing due
                        // unlock and wait for signal or the next timer is due
                        wait_until = timers.front()->timer_expire;
                        timers.clear();
                    }
                    else {
                        for (auto timer : timers) {
                            if (now >= timer->timer_expire) {
                                if (timer->timer_period) {
                                    // repeat in intervals
                                    timer->timer_expire += std::chrono::microseconds(timer->timer_period);
                                }
                                else {
                                    // one time call
                                    timer->timer_expire = std::chrono::steady_clock::time_point::max();
                                }
                                __ets_timer_callback = true;
                                timer->timer_func(timer->timer_arg);
                                __ets_timer_callback = false;
                            }
                        }
                        timers.clear();
                        continue;
                    }
                }
                // unlock
            }

            // wait for the next timer or modification of the list
            std::unique_lock<std::mutex> lock(__ets_timer_list_mutex);
            if (__ets_timer_thread_wakeup.wait_until(lock, wait_until, []() {
                return __ets_timer_thread_end == true;
            })) {
                // end timer thread
                break;
            }
        }
    });
}

void ets_timer_delay(uint32_t time_ms)
{
    auto end = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(time_ms);
    if (time_ms && can_yield()) {
        yield();
    }
    std::this_thread::sleep_until(end);
}

void ets_timer_delay_us(uint64_t time_us)
{
    auto end = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(time_us);
    if (time_us > 1000 && can_yield()) {
        yield();
    }
    std::this_thread::sleep_until(end);
}

static bool timer_initialized(ETSTimer *ptimer)
{
    return ptimer->init == __ETSTimerInitVal && find_timer(ptimer);
}

void ets_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg)
{
    std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);

    assert(timer_list);
    if (!timer_initialized(ptimer)) {
        *ptimer = {};
        ptimer->init = __ETSTimerInitVal;
        add_timer(ptimer);
    }
    ptimer->timer_func = pfunction;
    ptimer->timer_arg = parg;
}

void ets_timer_arm_us(ETSTimer *ptimer, uint32_t time_us, bool repeat_flag)
{
    std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);
    assert(timer_list);
    assert(timer_initialized(ptimer));

    ptimer->timer_period = repeat_flag ? time_us : 0;
    ptimer->timer_expire = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(time_us);

    __ets_timer_thread_wakeup.notify_all();
}

void ets_timer_arm(ETSTimer *ptimer, uint32_t time_ms, bool repeat_flag)
{
    std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);
    assert(timer_list);
    uint64_t time_us = time_ms * 1000ULL;

    assert(timer_initialized(ptimer));
    ptimer->timer_period = repeat_flag ? time_us : 0;
    ptimer->timer_expire = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(time_us);

    __ets_timer_thread_wakeup.notify_all();
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
    std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);
    assert(timer_list);
    if (timer_initialized(ptimer)) {
        ptimer->timer_expire = std::chrono::steady_clock::time_point::max();
        ptimer->timer_period = 0;
    }
    __ets_timer_thread_wakeup.notify_all();
}

void ets_timer_disarm(ETSTimer *ptimer)
{
    std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);
    assert(timer_list);
    if (timer_initialized(ptimer)) {
        remove_timer(ptimer);
    }
    __ets_timer_thread_wakeup.notify_all();
}

void ets_timer_init(void)
{
    std::lock_guard<std::mutex> lock(__ets_timer_list_mutex);
    if (!timer_list) {
        root.init = __ETSTimerInitVal;
        root.timer_func = nullptr;
        timer_list = &root;
        __ets_timer_thread = create_timer_thread();
        //__DBG_printf("ets_timer_thread created=1");
    }
}

void ets_timer_deinit(void)
{
    if (timer_list) {
        __ets_timer_thread_end = true;
        __ets_timer_thread_wakeup.notify_all();
        //__DBG_printf("__ets_timer_thread joinable=%u", __ets_timer_thread.joinable());
        __ets_timer_thread.join();
        timer_list = nullptr;
    }
}

esp_err_t esp_timer_init()
{
    ets_timer_init();
    return ESP_OK;
}

esp_err_t esp_timer_deinit()
{
    ets_timer_deinit();
    return ESP_OK;
}

void os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg) 
{
    ets_timer_setfn(ptimer, pfunction, parg);
}

void os_timer_disarm(ETSTimer *ptimer) 
{
    ets_timer_disarm(ptimer);
}

void os_timer_arm_us(ETSTimer *ptimer, uint32_t u_seconds, bool repeat_flag) 
{
    ets_timer_arm_us(ptimer, u_seconds, repeat_flag);
}

void os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag) 
{
    ets_timer_arm(ptimer, milliseconds, repeat_flag);
}

void os_timer_done(ETSTimer *ptimer) 
{
    ets_timer_done(ptimer);
}

#pragma pack(pop)
