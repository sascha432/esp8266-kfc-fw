/**
 * Author: sascha_lammers@gmx.de
 */

// work around for broken GCC
// define GCC_OPTIMIZE_LEVEL
// 0    O0
// 1    O1
// 2    O2
// 3    O3
// 4    Og
// 5    Os

#ifndef GCC_OPTIMIZE_LEVEL
#pragma GCC push_options
#endif
