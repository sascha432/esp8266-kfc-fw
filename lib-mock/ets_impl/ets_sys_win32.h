/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdint.h>
#include <chrono>
#include <pgmspace.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ETS_MAIN_NO_MAIN_FUNC
#    define ETS_MAIN_NO_MAIN_FUNC 0
#endif

#pragma pack(push, 4)

#ifndef IRAM_ATTR
#    define IRAM_ATTR
#endif

#ifndef ICACHE_RAM_ATTR
#    define ICACHE_RAM_ATTR
#endif

#ifndef ICACHE_FLASH_ATTR
#    define ICACHE_FLASH_ATTR
#endif

typedef int32_t esp_err_t;

/* Definitions for error constants. */

#define ESP_OK   0
#define ESP_FAIL -1

#define ESP_ERR_NO_MEM           0x101
#define ESP_ERR_INVALID_ARG      0x102
#define ESP_ERR_INVALID_STATE    0x103
#define ESP_ERR_INVALID_SIZE     0x104
#define ESP_ERR_NOT_FOUND        0x105
#define ESP_ERR_NOT_SUPPORTED    0x106
#define ESP_ERR_TIMEOUT          0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC      0x109
#define ESP_ERR_INVALID_VERSION  0x10A
#define ESP_ERR_INVALID_MAC      0x10B

typedef uint8_t byte;

extern bool can_yield();
extern void yield();

void __panic_func(const char *file, int line, const char *func);
uint64_t micros64();
unsigned long millis(void);
unsigned long micros(void);
void delay(unsigned long time_ms);

#define panic() __panic_func(PSTR(__FILE__), __LINE__, __func__)

int __set_xt_rsil(int level);
int __xt_wsr_ps(int state);

#define xt_wsr_ps(state) __xt_wsr_ps(state)
#define xt_rsil(level)   __set_xt_rsil(level)
#define interrupts()     xt_rsil(0)
#define noInterrupts()   xt_rsil(15)

void ets_intr_lock(void);
void ets_intr_unlock(void);

void ets_timer_init(void);
void ets_timer_deinit(void);

#define ETS_INTR_LOCK() ets_intr_lock()

#define ETS_INTR_UNLOCK() ets_intr_unlock()

int ets_printf(const char *fmt, ...);

extern std::atomic_bool __ets_is_running;
//extern volatile bool __ets_is_running;
extern bool __ets_is_loop_running();
extern void __ets_end_loop();

extern void __ets_post_loop_do_yield();
extern void __ets_post_loop_exec_loop_functions();
extern void __ets_pre_setup();
extern void preinit();
extern void loop();
extern void setup();

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
