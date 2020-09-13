/**
* Author: sascha_lammers@gmx.de
*/

#if HAVE_MEMDEBUG == 0

#undef __LMDBG_IF
#undef __LMDBG_N_IF
#define __LMDBG_IF(...)
#define __LMDBG_N_IF(...)                           __VA_ARGS__

#endif
