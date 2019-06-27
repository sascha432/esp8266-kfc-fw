/**
 * Author: sascha_lammers@gmx.de
 */

#include "virtual_fs_impl.h"

// platform indepent core implementation

File _vfs_file;

int vfs_get_cached_header(vfs_image_header *headerPtr) {
    if (!headerPtr) {
        return -1;
    }
    if (!_vfs_file.seek(0)) {
        return -2;
    }
    if (_vfs_file.read((uint8_t *)headerPtr, sizeof(vfs_image_header)) != sizeof(vfs_image_header)) {
        return -3;
    }
    printf("image header %d\n", headerPtr->entries);
    return 0;
}

int vfs_get_lookup_entry(uint16_t num, vfs_lookup_table_entry *lookupPtr, vfs_image_header *headerPtr) {
    //int result;
    if (!lookupPtr || !headerPtr) {
        return -1;
    } else {
        if (num >= headerPtr->entries) {
            return -4;
        } else if (!_vfs_file.seek(VFS_OFFSET_LOOKUP_TABLE(headerPtr->entries) + num * sizeof(vfs_lookup_table_entry))) {
            return -2;
        } else if (_vfs_file.read((uint8_t *)lookupPtr, sizeof(vfs_lookup_table_entry)) != sizeof(vfs_lookup_table_entry)) {
            return -3;
        }
    }
    printf("lookup entry #%d %04x %d\n", num, lookupPtr->crc, lookupPtr->path_offset);
    return 0;
}

int vfs_get_entry_from_lookup(vfs_lookup_table_entry *lookupPtr, vfs_file_entry *entryPtr, vfs_image_header *headerPtr, vfs_data_header *dataHeaderPtr) {
    if (!lookupPtr || !entryPtr || !headerPtr || !dataHeaderPtr) {
        return -1;
    } else {
        if (!_vfs_file.seek(VFS_OFFSET_VIRTUAL_PATH((*headerPtr), lookupPtr->path_offset - sizeof(vfs_data_header)))) {
			return -2;
        }
        if (_vfs_file.read((uint8_t *)dataHeaderPtr, sizeof(vfs_data_header)) != sizeof(vfs_data_header)) {
			return -3;
		}
        if (!_vfs_file.seek(VFS_OFFSET_FILE_ENTRY((*headerPtr), dataHeaderPtr->file_entry_num))) {
			return -2;
		}
        if (_vfs_file.read((uint8_t *)entryPtr, sizeof(vfs_file_entry)) != sizeof(vfs_file_entry)) {
			return -3;
		}
		return dataHeaderPtr->file_entry_num + 1;
    }
    return 0;
}

int vfs_get_virtual_path(vfs_offset_t offset, String &filename, vfs_image_header *headerPtr) {
    if (!headerPtr) {
        return -1;
    }
    offset = VFS_OFFSET_VIRTUAL_PATH((*headerPtr), offset);
    if (offset < headerPtr->data_block_offset || (offset + 1) >= (int)headerPtr->content_offset) {
        return -4;
    } else if (!_vfs_file.seek(offset)) {
        return -1;
    } else {
        filename = _vfs_file.readStringUntil(0);
        if (filename.length() < 1 || filename.length() >= VFS_MAX_PATH) {
            filename = String();
            return -5;
        }
        printf("virtual path %u: %s\n", offset, filename.c_str());
        return 0;
    }
}


/**
 *  0 no error, incidcating not found or something similar
 * -1 nullptr
 * -2 seek error
 * -3 read error
 * -4 out of bounds
 * -5 invalid/corrupted data
 **/

int vfs_find_entry_by_name(const char *filename, int filename_len, vfs_file_entry *entryPtr, vfs_data_header *dataHeaderPtr) {

    vfs_image_header header;
    vfs_file_entry entry;
    vfs_lookup_table_entry lookup;
    int result;

    result = vfs_get_cached_header(&header);
    if (result < 0) {
        return result;
    }
    if (header.entries == 0) {
        return 0;
    }

    uint16_t header_num = 0;
    uint16_t crc = crc16_calc((uint8_t *)filename, filename_len == -1 ? strlen(filename) : filename_len);

    result = vfs_get_lookup_entry(header_num, &lookup, &header);
    if (result < 0) {
        return result;
    }

    uint16_t count = header.entries;
    bool isMatch = false;
    while(count > 0) {
        uint16_t middle = count / 2;
        bool greaterOrEqual = crc > lookup.crc;
        printf("cmd %04x < %04x, header_num %d, middle %d, count %d, isMatch %d\n", crc, lookup.crc, header_num, middle, count, isMatch);
        if (crc == lookup.crc) {
            String resultString;
            result = vfs_get_virtual_path(lookup.path_offset, resultString, &header);
            if (result < 0) {
                return result;
            }
            if (resultString.equals(filename)) {
                isMatch = true;
                break;
            }
            greaterOrEqual = true; // not a match, check next record. ideally there isn't any duplicate crcs
        }
        if (greaterOrEqual) {
            header_num++;
            count -= middle + 1;
        } else {
            count = middle;
            header_num += count;
        }
        result = vfs_get_lookup_entry(header_num, &lookup, &header);
        if (result < 0) {
            return result;
        }
    }
    if (isMatch) {
        if (entryPtr) {
			vfs_data_header dataHeader;
            result = vfs_get_entry_from_lookup(&lookup, &entry, &header, &dataHeader);
            if (result < 0) {
                return result;
            }
            if (dataHeaderPtr) {
				*dataHeaderPtr = dataHeader;
			}
            *entryPtr = entry;
        }
        return header_num + 1;
    }
    return 0;
}

String vfs_flags2string(uint16_t flags) {
	String out;
    if (flags & VFS_FILE_ENTRY_FLAGS_MD5_HASH) {
        out += "MD5,";
	}
    if (flags & VFS_FILE_ENTRY_FLAGS_SHA1_HASH) {
        out += "SHA1,";
	}
    if (flags & VFS_FILE_ENTRY_FLAGS_GZIPPED) {
        out += "Gzipped,";
	}
    if (flags & VFS_FILE_ENTRY_FLAGS_INLINE_CONTENT) {
        out += "Inline,";
	}
    out.remove(-1, 1);
	return out;
}

VFSFile __VFS_open(const String filename, const char *mode) {
    vfs_file_entry entry;
	vfs_data_header data_header;
    int result;

    if ((result = vfs_find_entry_by_name(filename.c_str(), filename.length(), &entry, &data_header)) <= 0) {
		entry.file_size = -1;
        printf("error %d\n", result);
    }
    printf("Find file %s, result %d, file size %d, flags %s, mod. time %u, content ofs %u\n", filename.c_str(), result, entry.file_size, vfs_flags2string(entry.flags).c_str(), entry.modification_time, data_header.content_offset);

    return VFSFile(entry, data_header, _vfs_file, mode);
}

VFSFile::VFSFile(vfs_file_entry &entry, vfs_data_header &dataHeader, File &file, const char *mode) : _file(file) {
	_position = 0;
	_size = entry.file_size;
    if (_size != IS_INVALID) {
		if ((entry.flags & VFS_FILE_ENTRY_FLAGS_INLINE_CONTENT) == 0) {
			_offset = 0;
			char buf[64];
			snprintf(buf, sizeof(buf), "/vfs/%02x", dataHeader.file_entry_num);
			_file = SPIFFS.open(buf, mode);
		} else {
			_offset = VFS_OFFSET_INLINE_CONTENT(dataHeader);
		}
	}
}
VFSFile::~VFSFile() {
    close();
}

VFSFile::operator bool() const {
	return _size != IS_INVALID;
}

bool VFSFile::seek(long pos, int mode) {
    if (mode == SeekSet) {
        if (pos >= 0 && pos < (long)_size) {
            _position = (uint16_t)pos;
            return _file.seek(_position + _offset, SeekSet);
        }
    } else if (mode == SeekCur) {
        return seek(_position + pos, SeekSet);
    } else if (mode == SeekEnd) {
        return seek(_size - pos, SeekSet);
    }
    return false;
}

size_t VFSFile::read(uint8_t *buffer, size_t len) {
    if (_position + len >= _size) {
        len = _size - _position;
    }
    if (_offset != 0) { // if using a shared _vfs_file, always use seek
        if (!_vfs_file.seek(_position + _offset)) {
			return 0;
		}
    }
	size_t read = _vfs_file.read(buffer, len);
	_position += read;
    return read;
}

size_t VFSFile::write(uint8_t *data, size_t len) {
    return 0;
}

int VFSFile::available() {
	return _size -_position;
}

size_t VFSFile::position() const {
	return _position;
}

size_t VFSFile::size() const {
	return _size;
}

void VFSFile::flush() {
    //_file.flush(); // read only, doesnt need flush
}

void VFSFile::close() {
    if (_size != IS_INVALID && _offset == 0) {
        _file.close();
    }
	_size = IS_INVALID;
}
