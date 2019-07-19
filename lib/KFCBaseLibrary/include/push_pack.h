/**
 * Author: sascha_lammers@gmx.de
 */

#if _WIN32 || _WIN64

#ifndef __attribute__packed__
#define __attribute__packed__
#endif
#pragma pack(push, 1)

#else

#ifndef __attribute__packed__
#define __attribute__packed__           __attribute__((packed))
#endif

#endif
