/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef __FS_MAPPING_INSIDE_INCLUDE
#define  __FS_MAPPING_INLINE__
#define  __FS_MAPPING_INLINE_AWLAYS__
#define __FS_MAPPING_INLINE_INLINE_DEFINED__
#else
#define  __FS_MAPPING_INLINE__ inline
#define  __FS_MAPPING_INLINE_AWLAYS__ inline __attribute__((__always_inline__))
#define __FS_MAPPING_INLINE_INLINE_DEFINED__
#include "fs_mapping.h"
#endif

__FS_MAPPING_INLINE_AWLAYS__
bool SPIFFSWrapper::remove(const char *path)
{
    return KFCFS.remove(path);
}

__FS_MAPPING_INLINE_AWLAYS__
File SPIFFSWrapper::open(Dir dir, const char *mode)
{
#if ESP8266
    return dir.openFile(mode);
#else
    return File();
    // return dir.open(mode);
#endif
}


__FS_MAPPING_INLINE_AWLAYS__
bool SPIFFSWrapper::exists(const char *path)
{
    if (KFCFS.exists(path)) {
        return true;
    }
    return FileMapping(path).exists();
}

__FS_MAPPING_INLINE_AWLAYS__
bool SPIFFSWrapper::rename(const char* pathFrom, const char* pathTo)
{
    if (exists(pathTo)) {
        return false;
    }
    return KFCFS.rename(pathFrom, pathTo);
}

#ifdef __FS_MAPPING_INSIDE_INCLUDE
#undef __FS_MAPPING_INSIDE_INCLUDE
#endif
