/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "virtual_fs_struct.h"

extern File _vfs_file;

uint16_t crc16_calc(const uint8_t *data, size_t len);

int vfs_get_cached_header(vfs_image_header *headerPtr);
int vfs_get_lookup_entry(uint16_t num, vfs_lookup_table_entry *lookupPtr, vfs_image_header *headerPtr);
int vfs_get_entry_from_lookup(vfs_lookup_table_entry *lookupPtr, vfs_file_entry *entryPtr, vfs_image_header *headerPtr, vfs_data_header *dataHeaderPtr);
int vfs_get_virtual_path(vfs_offset_t offset, String &filename, vfs_image_header *headerPtr);
int vfs_find_entry_by_name(const char *filename, int filename_len, vfs_file_entry *entryPtr, vfs_data_header *dataHeaderPtr);
String vfs_flags2string(uint16_t flags);

class VFSFile;

VFSFile __VFS_open(const String filename, const char *mode);

class VFSFile : public File {
public:
	static const size_t IS_INVALID = (size_t)~0;

    VFSFile(vfs_file_entry &entry, vfs_data_header &dataHeader, File &file, const char *mode);
    virtual ~VFSFile();

	operator bool() const;
    bool seek(long pos, int mode);
    size_t read(uint8_t *buffer, size_t len);
    size_t write(uint8_t *data, size_t len);
	int available();
    size_t position() const;
    size_t size() const;
	void flush() override;
	void close();

private:
	size_t _position;
	size_t _size;
	size_t _offset;
	File &_file;
};
