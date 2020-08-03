/**
 * Author: sascha_lammers@gmx.de
 */

#if _MSC_VER

#ifndef __attribute__packed__
#define __attribute__packed__
#define __attribute__unaligned__
#endif
#pragma pack(push, 1)

#else

#ifndef __attribute__packed__
#define __attribute__packed__           __attribute__((packed))
#define __attribute__unaligned__        __attribute__((__aligned__(1)))
#define PSTR1(str)                      PSTRN(str, 1)
#endif

#endif
