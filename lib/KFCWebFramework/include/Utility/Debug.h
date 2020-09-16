/**
* Author: sascha_lammers@gmx.de
*/

#ifndef DEBUG_KFC_FORMS
#define DEBUG_KFC_FORMS                             1
#endif

#ifndef DEBUG_KFC_FORMS_DISABLE_ASSERT
#define DEBUG_KFC_FORMS_DISABLE_ASSERT              0
#endif

#include <debug_helper.h>

#if DEBUG_KFC_FORMS
#include <MicrosTimer.h>
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if DEBUG_KFC_FORMS_DISABLE_ASSERT
#undef __LDBG_assert_printf
#define __LDBG_assert_printf(...)
#endif
