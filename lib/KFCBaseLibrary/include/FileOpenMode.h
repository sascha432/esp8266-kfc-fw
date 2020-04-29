/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

namespace fs {
    class FileOpenMode {
    public:
        static const char *read;
        static const char *write;
        static const char *readplus;
        static const char *append;
        static const char *appendplus;
    };
};
