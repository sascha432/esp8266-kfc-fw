/**
* Author: sascha_lammers@gmx.de
*/

// enable local memory debugging

#undef __LMDBG_IF
#undef __LMDBG_N_IF
#define __LMDBG_IF(...)                             __VA_ARGS__
#define __LMDBG_N_IF(...)
