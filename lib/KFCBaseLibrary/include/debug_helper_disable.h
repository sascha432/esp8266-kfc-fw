/**
* Author: sascha_lammers@gmx.de
*/

// disable debug output (compile time)
// in order to use the macros, the file should be included after the last #include statement before using the macros, since the included file might have defined/undefined them

#ifdef _debug_print
#undef _debug_println_notempty
#undef _debug_print
#undef _debug_println
#undef _debug_printf
#undef _debug_printf_P
#undef _debug_print_result
#undef _debug_dump_args
#undef _IF_DEBUG
#undef _debug_resolve_lambda
#endif
#define _debug_println_notempty(...)                ;
#define _debug_print(...)							;
#define _debug_println(...)							;
#define _debug_printf(...)							;
#define _debug_printf_P(...)						;
#define _debug_print_result(result)					result
#define _IF_DEBUG(...)
#define _debug_resolve_lambda(ptr)                  ptr

#ifdef __LDBG_panic
#undef __LDBG_panic
#endif
#define __LDBG_panic(...)                           __DBG_panic(__VA_ARGS__)
