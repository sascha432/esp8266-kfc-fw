/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <FSImpl.h>

/**
 * Virtual file system with long filenames and modification time, stored in PROGMEM or on SPIFFS
 **/

typedef std::unique_ptr<fs::FileImpl> VirtualFileImplPtr;

class VirtualFS {
public:
    VirtualFS(fs::FileImplPtr impl = VirtualFileImplPtr());

    void begin(const fs::FS *spiffs);
    void begin(); // use globally defined SPIFFS
    void end();

private:
    fs::FS *_spiffs;
    fs::FSImplPtr impl;
};

class VirtualFileImpl : public fs::FileImpl {

};

class VirtualDirImpl : public fs::DirImpl {

};

class VirtualFSImpl : public fs::FSImpl {

};

class ProgmemStream : public Stream {

};

class ProgmemFileImpl : public ProgmemStream, public VirtualFS {

};
