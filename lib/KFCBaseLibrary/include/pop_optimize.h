/**
 * Author: sascha_lammers@gmx.de
 */

#ifdef GCC_OPTIMIZE_LEVEL
#if GCC_OPTIMIZE_LEVEL == 0
#pragma GCC optimize ("O0")
#elif GCC_OPTIMIZE_LEVEL == 1
#pragma GCC optimize ("O1")
#elif GCC_OPTIMIZE_LEVEL == 2
#pragma GCC optimize ("O2")
#elif GCC_OPTIMIZE_LEVEL == 3
#pragma GCC optimize ("O3")
#elif GCC_OPTIMIZE_LEVEL == 4
#pragma GCC optimize ("Og")
#elif GCC_OPTIMIZE_LEVEL == 5
#pragma GCC optimize ("Os")
#else
#error Invalid optimize level
#endif
#else
#pragma GCC pop_options
#endif
