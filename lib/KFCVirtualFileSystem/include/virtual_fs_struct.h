/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <push_pack.h>

#include <stdint.h>

#define VFS_MAX_PATH                            160

#define VFS_OFFSET_FILE_ENTRIES                 sizeof(vfs_image_header)
#define VFS_OFFSET_LOOKUP_TABLE(entries)        (VFS_OFFSET_FILE_ENTRIES + sizeof(vfs_file_entry) * entries)
#define VFS_OFFSET_DATA_BLOCK(header)           header.data_block_offset
#define VFS_OFFSET_FILE_ENTRY(header, num)      (VFS_OFFSET_FILE_ENTRIES + sizeof(vfs_file_entry) * num)
#define VFS_OFFSET_VIRTUAL_PATH(header, ofs)    header.data_block_offset + ofs
#define VFS_OFFSET_INLINE_CONTENT(header)       header.content_offset + sizeof(vfs_content_header)

typedef struct __attribute__packed__ {
    uint16_t entries;                           // number of file entries
    uint16_t data_block_offset;                 // location of the data block
    uint32_t content_offset;
} vfs_image_header;

#define VFS_FILE_ENTRY_FLAGS_MD5_HASH           0x0001
#define VFS_FILE_ENTRY_FLAGS_SHA1_HASH          0x0002
#define VFS_FILE_ENTRY_FLAGS_GZIPPED            0x0100      // file is gzipped. for example fpor sending it directly to a web client that supports gzipped encoding
#define VFS_FILE_ENTRY_FLAGS_INLINE_CONTENT     0x0200      // data is stored in the same file

typedef uint16_t vfs_crc_t;
typedef uint16_t vfs_offset_t;
typedef uint32_t vfs_long_offset_t;

typedef struct __attribute__packed__ {
    vfs_crc_t crc;                                      // crc of the path
    vfs_offset_t virtual_path_offset;                   // offset of the virtual path int he data block
    vfs_offset_t hash_offset;
    uint16_t flags;
    uint32_t file_size;
    uint32_t modification_time;
} vfs_file_entry;

typedef struct __attribute__packed__ {
    vfs_crc_t crc;                                      // crc of the path
    uint16_t path_offset;                               // offset to the path
} vfs_lookup_table_entry;

typedef struct __attribute__packed__ {
    uint16_t file_entry_num;
    uint8_t dir_len;                                     // length of the directory of the virutal file
    uint8_t extra_len;
    vfs_long_offset_t content_offset;                    // offset to the files content if it isn't stored in an external file
} vfs_data_header;

typedef struct __attribute__packed__ {
    uint16_t file_entry_num;
    uint16_t ___reserved;
} vfs_content_header;

/*

data block:

vfs_data_header*1           see struct
null terminated string      virtual path
byte*N                      fixed size hash (md5 20 bytes, sha1 32 byte, etc...)
byte*extra_len              preserved for the future

*/

#include <pop_pack.h>
