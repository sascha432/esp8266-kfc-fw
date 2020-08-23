/**
  Author: sascha_lammers@gmx.de
*/

#include "ets_win_includes.h"
#include "ets_sys_win32.h"
#include "ets_timer_win32.h"

bool can_yield()
{
	return (__ets_in_timer_isr == false);
}

void __panic_func(const char *file, int line, const char *func)
{
	__debugbreak();

	ets_printf("PANIC in %s:%u function %s\n", file, line, func);

	while (ets_is_running) {
		Sleep(10);
	}
}

void yield()
{
	if (__ets_in_timer_isr) {
		panic();
	}
	Sleep(1);
}

int ets_printf(const char *fmt, ...)
{
	char buffer[2048];
	va_list arg;
	va_start(arg, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, arg);
	va_end(arg);

	::printf("%s", buffer);
	_RPTN(_CRT_WARN, "%s", buffer);

	return len;
}


#define ETS_INTR_LOCK_NEST_MAX 7
static uint16_t ets_intr_lock_stack[ETS_INTR_LOCK_NEST_MAX];
static byte     ets_intr_lock_stack_ptr=0;


int __set_xt_rsil(int level)
{
	return 0;
}

int __xt_wsr_ps(int state)
{
	return 0;
}

// Replace ets_intr_(un)lock with nestable versions
void ets_intr_lock() {
	if (ets_intr_lock_stack_ptr < ETS_INTR_LOCK_NEST_MAX)
		ets_intr_lock_stack[ets_intr_lock_stack_ptr++] = xt_rsil(3);
	else
		xt_rsil(3);
}

void ets_intr_unlock() {
	if (ets_intr_lock_stack_ptr > 0)
		xt_wsr_ps(ets_intr_lock_stack[--ets_intr_lock_stack_ptr]);
	else
		xt_rsil(0);
}

