/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ets_sys_win32.h"

#pragma pack(push, 4)

/* timer related */
typedef uint32_t ETSHandle;
typedef void ETSTimerFunc(void *timer_arg);


typedef struct _ETSTIMER_ {
    struct _ETSTIMER_ *timer_next;
    std::chrono::steady_clock::time_point timer_expire;
    uint64_t timer_period;
    ETSTimerFunc *timer_func;
    void *timer_arg;
    uint32_t init;
} ETSTimer;

extern ETSTimer *timer_list;


void ets_timer_delay(uint32_t time_ms);
void ets_timer_delay_us(uint64_t time_us);

void ets_timer_init(void);
void ets_timer_deinit(void);

void ets_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
void ets_timer_arm_new(ETSTimer *ptimer, int time_ms, int repeat_flag, int isMillis);
void ets_timer_disarm(ETSTimer *ptimer);
void ets_timer_done(ETSTimer *ptimer);

esp_err_t esp_timer_init();
esp_err_t esp_timer_deinit();

void os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
void os_timer_disarm(ETSTimer *ptimer);
void os_timer_arm_us(ETSTimer *ptimer, uint32_t u_seconds, bool repeat_flag);
void os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag);
void os_timer_done(ETSTimer *ptimer);

#pragma pack(pop)
