/**
* Author: sascha_lammers@gmx.de
*/

// in order to use the macros, the file should be included after the last #include statement before using the macros, since the 
// included file might have defined/undefined them

// enable local functions (__LDBG_ prefix)

#undef __LDBG_IF
#undef __LDBG_N_IF
#define __LDBG_IF(...)                              __VA_ARGS__
#define __LDBG_N_IF(...)
