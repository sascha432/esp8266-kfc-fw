/**
 * Author: sascha_lammers@gmx.de
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include <memory>
#include "virtual_fs_struct.h"

uint16_t _crc16_update(uint16_t crc = ~0x0, const uint8_t a = 0) {
    crc ^= a;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 1) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc = (crc >> 1);
        }
    }
    return crc;
}

uint16_t crc16(uint8_t const *data, size_t length) {
    uint16_t crc = ~0;
    for (size_t index = 0; index < length; ++index) {
        crc = _crc16_update(crc, data[index]);
    }
    return crc;
}

#include <push_pack.h>

#define FS_MAPPINGS_HASH_LENGTH 20
typedef unsigned char item_counter_t;

typedef struct __attribute__packed {
    uint16_t path_offset;
    uint16_t mapped_path_offset;
    uint8_t flags;
    uint32_t mtime;
    uint32_t file_size;
    uint8_t hash[FS_MAPPINGS_HASH_LENGTH];
} mapping_t;

typedef struct {
    mapping_t header;
    char *virtual_path;
    char *mapped_path;
    vfs_crc_t crc;
    uint16_t file_num;
    uint16_t org_file_num;
} mapping_entry;

#include <pop_pack.h>

typedef std::vector<mapping_entry> MappingsList;

void free_mappings(MappingsList &list) {

    for(auto item: list) {
        if (item.org_file_num == 0) {
            free(item.virtual_path);
            break;
        }
    }
    list.clear();
}

MappingsList read_mappings(const char *path) {

    MappingsList list;
    FILE *fp;

    errno_t err = fopen_s(&fp, path, "rb");
    if (err) {
        printf("Failed to open %s\n", path);
        exit(-1);
    }

    fseek(fp, 0, SEEK_END);
    uint16_t file_size = (uint16_t)ftell(fp);
    rewind(fp);

    item_counter_t item_count;
    if (fread(&item_count, 1, sizeof(item_count), fp) != 1) {
        printf("Failed to read item count from %s\n", path);
        exit(-1);
    }

    list.reserve(item_count);

    item_counter_t i;
    for(i = 0; i < item_count; i++) {
        mapping_entry entry;
        memset(&entry, 0, sizeof(entry));
        entry.org_file_num = i;
        if (fread(&entry.header, sizeof(mapping_t), 1, fp) != 1) {
            printf("Failed to read header #%u from %s\n", i + 1, path);
            exit(-1);
        }
        list.push_back(entry);
    }
    uint16_t data_size = (uint16_t)(file_size - ftell(fp));
    char *data = (char *)calloc(data_size + 1, 1); // add nul byte to prevent read out of bounds
    if (!data) {
        printf("Failed to allocate %u bytes\n", data_size + 1);
        exit(1);
    }

    if (fread(data, data_size, 1, fp) != 1) {
        printf("Failed to read data block from %s\n", path);
        exit(1);
    }

    uint16_t n = 1;
    for(auto &entry: list) {
        entry.virtual_path = data + entry.header.path_offset;
        if ((n == 1 && entry.header.path_offset != 0) || entry.header.path_offset >= data_size) { // range check
            printf("%s: Invalid virtual path offset in header #%u\n", path, n);
            exit(1);
        }
        size_t len;
        if ((len = strlen(entry.virtual_path)) > 128) {
            printf("NOTE: long filename %s (%zd)\n", entry.virtual_path, len);
        }
        if (len >= VFS_MAX_PATH) {

            printf("Filenames are limited to %d charachters\n", VFS_MAX_PATH);
            exit(1);
        }

        entry.mapped_path = data + entry.header.mapped_path_offset;
        if (entry.header.path_offset >= data_size) { // range check
            printf("%s: Invalid mapped path offset in header #%u\n", path, n);
            exit(1);
        }
        n++;
    }

    fclose(fp);

    printf("%s: entries %d, data size %u, file size %u\n", path, item_count, data_size, file_size);
    // for(auto entry: list) {
    //     printf("%s => %s, %u\n", entry.virtual_path, entry.mapped_path, entry.header.file_size);
    // }

    return list;
}

bool copy_file(const char *dst, const char *src, uint32_t file_size) {

    printf("Would copy %s to %s\n", src, dst);
    return true;

}

bool copy_file(FILE *fp, const char *src, size_t file_size) {

    char buf[0xffff];
    size_t read, copied = 0;

    FILE *fp_src;
	errno_t err = fopen_s(&fp_src,  src, "rb");
    if (err) {
        printf("cannot open %s\n", src);
        return false;
    }

    while(!feof(fp_src)) {
        read = fread(buf, 1, sizeof(buf), fp_src);
        if (ferror(fp_src)) {
            printf("read error %zd bytes, copied %zd\n", read, copied);
        }
        if (read) {
            if (fwrite(buf, 1, read, fp) != read || ferror(fp)) {
                printf("write error %zd byes, %zd copied\n", read, copied);
                return false;
            }
            copied += read;
        }
    };

    fclose(fp_src);

    if (copied != file_size) {
        printf("file size mismatch %zd (src) != %zd (dst)\n", file_size, copied);
        return false;
    }

    return true;

}

void convert_mappings_vfs(MappingsList &list, char *output_file, char *data_path) {

    bool inline_content = true;
    uint16_t n;

    FILE *fp;
	errno_t err = fopen_s(&fp, output_file, "wb");
    if (err) {
        printf("Failed to write %s\n", output_file);
        exit(-1);
    }

    vfs_image_header header = { (uint16_t)list.size() };
    std::vector<vfs_file_entry> entries;
    std::vector<vfs_lookup_table_entry> lookup_virtual_path;

    memset(&header, 0, sizeof(header));

    for(auto &item: list) {
        item.crc = crc16((uint8_t *)item.virtual_path, strlen(item.virtual_path));
    }
    for(const auto &item1: list) {
        for(const auto &item2: list) {
            if (item1.file_num != item2.file_num) {
                if (item1.crc == item2.crc) {
                    printf("duplicate crc for filename of %d and %d (%s / %s)\n", item1.file_num, item2.file_num, item1.virtual_path, item2.virtual_path);
                }
            }
        }
    }

    std::sort(list.begin(), list.end(), [](const mapping_entry &a, const mapping_entry &b) {
        return a.crc > b.crc;
    });

    uint32_t data_size = 0;
    uint32_t extra_len = 0;
    n = 0;
    for(auto &entry: list) {
        data_size += (uint32_t)(strlen(entry.virtual_path) + 1 + sizeof(entry.header.hash) + sizeof(vfs_data_header) + extra_len);
        entry.file_num = n++;
    }
    header.entries = (uint16_t)list.size();
    printf("data block size: %u, items %zd, file entries %zd\n",
        data_size, list.size(), list.size());

    printf("header size: %zd\n", sizeof(header));
    if (sizeof(header) & 0x3) {
        printf("WARNING: size isn't 32bit padded\n");
    }
    printf("size of vfs_file_entry: %zd\n", sizeof(vfs_file_entry));
    if (sizeof(vfs_file_entry) & 0x3) {
        printf("WARNING: size isn't 32bit padded\n");
    }
    printf("size of vfs_lookup_table_entry: %zd\n", sizeof(vfs_lookup_table_entry));
    if (sizeof(vfs_lookup_table_entry) & 0x3) {
        printf("WARNING: size isn't 32bit padded\n");
    }
    vfs_offset_t lookup_ofs = (uint16_t)(sizeof(header) + sizeof(vfs_file_entry) * list.size());
    printf("lookup table offset: %u\n", lookup_ofs);
    if (header.data_block_offset & 0x3) {
        printf("WARNING: offset isn't 32bit padded\n");
    }
    header.data_block_offset = uint16_t(sizeof(header) + ((sizeof(vfs_lookup_table_entry) + sizeof(vfs_file_entry)) * list.size()));
    printf("data block offset: %u\n", header.data_block_offset);
    if (header.data_block_offset  & 0x3) {
        printf("WARNING: offset isn't 32bit padded\n");
    }

    char *data = (char *)calloc(data_size, 1);
    if (!data) {
        printf("Failed to allocate %u bytes\n", data_size);
        exit(1);
    }

    uint32_t content_offset = header.data_block_offset + data_size;
    header.content_offset = content_offset;

    n = 0;
    char *ptr = data;
    for(auto &item: list) {
        vfs_file_entry entry;
        vfs_data_header data_hdr;
        vfs_lookup_table_entry lookup;

        memset(&lookup, 0, sizeof(lookup));
        memset(&data_hdr, 0, sizeof(data_hdr));
        memset(&entry, 0, sizeof(entry));

        data_hdr.content_offset = 0;
        data_hdr.file_entry_num = item.file_num;
        data_hdr.extra_len = (uint8_t)extra_len;

        char *s;
        if ((s = strrchr(item.virtual_path, '/')) != nullptr) {
            data_hdr.dir_len = (uint8_t)(s - item.virtual_path);
        } else {
            printf("virtual path does not contain a slash, header #%u\n", n + 1);
            exit(1);
        }

        entry.crc = item.crc;
        entry.flags = VFS_FILE_ENTRY_FLAGS_MD5_HASH | ((item.header.flags & 0x01) ? VFS_FILE_ENTRY_FLAGS_GZIPPED : 0);
        entry.file_size = item.header.file_size;
        entry.modification_time = item.header.mtime;
        entry.virtual_path_offset = (vfs_offset_t)((ptr - data) + sizeof(data_hdr));
        printf("%s ofs %lu\n", item.virtual_path, entry.virtual_path_offset);
        entry.hash_offset = (uint16_t)(entry.virtual_path_offset + (strlen(item.virtual_path) + 1));

        if (inline_content) {
            entry.flags |= VFS_FILE_ENTRY_FLAGS_INLINE_CONTENT;
            item.file_num = 0xffff;
            data_hdr.content_offset = content_offset;
            content_offset += entry.file_size + sizeof(vfs_content_header);
        }

        entries.push_back(entry);

        lookup.crc = entry.crc;
        lookup.path_offset = entry.virtual_path_offset;
        lookup_virtual_path.push_back(lookup);

        memcpy(ptr, &data_hdr, sizeof(data_hdr));
        ptr += sizeof(data_hdr);
        if (ptr != data + entry.virtual_path_offset) {
            printf("header #%u, invalid path offset, %p != %p\n", n + 1, ptr, data + entry.virtual_path_offset);
            exit(1);
        }
        strcpy_s(ptr, strlen(item.virtual_path) + 1, item.virtual_path);
        ptr += strlen(item.virtual_path);
        *ptr++ = 0;
        if (ptr != data + entry.hash_offset) {
            printf("header #%u, invalid hash offset\n", n + 1);
            exit(1);
        }
        memcpy(ptr, item.header.hash, sizeof(item.header.hash));
        ptr += sizeof(item.header.hash);
        if (data_hdr.extra_len) {
            printf("vfs_data_header.extra_len must be 0, header +%u\n", n +1 );
            exit(1);
        }
        ptr += data_hdr.extra_len;
        printf("data size %d ptr %zd, %d\n", (int)data_size, (size_t)(ptr - data), (int)n);
        n++;
    }

    if (data_size != ptr - data) {
        printf("Data size %d does not match %zd\n", data_size, (size_t)(ptr - data));
        exit(1);
    }

    int written_records = 0;
    written_records += fwrite(&header, 1, sizeof(header), fp) != 1;

    printf("file offset %lu after header\n", ftell(fp));

    n = 0;
    for(auto entry: entries) {
        mapping_entry &mapping = list.at(n);
        vfs_data_header *data_hdr = reinterpret_cast<vfs_data_header *>(&data[entry.virtual_path_offset - sizeof(vfs_data_header)]);
        printf("file_entry %d %08x %s, content ofs %u => %s=%s\n", n, entry.crc, &data[entry.virtual_path_offset], data_hdr->content_offset, mapping.virtual_path, mapping.mapped_path);
        written_records += fwrite(&entry, 1, sizeof(entry), fp) != 1;
        n++;
    }

    printf("file offset %lu after file entries\n", ftell(fp));

    n = 0;
    for(auto entry: lookup_virtual_path) {
        // printf("lookup_table %d %08x %s\n", n, entry.crc, &data[entry.path_offset]);
        written_records += fwrite(&entry, 1, sizeof(entry), fp) != 1;
        n++;
    }
    printf("file offset %lu after lookup table\n", ftell(fp));

    int data_offset = ftell(fp);
    written_records += fwrite(data, 1, data_size, fp) != 1;

    size_t expected_records = 2 + 2 * list.size();

    printf("file offset %lu after data block\n", ftell(fp));

    printf("data offset %u file offset %u, written size %u, expected %zd\n", header.data_block_offset, data_offset, written_records, expected_records);

    if (written_records != expected_records) {
        printf("Failed to write %s\n", output_file);
        exit(-1);
    }

    n = 0;
    for(auto entry: entries) {
        char bsrc[2048];
        const auto &item = list.at(n);
        snprintf(bsrc, sizeof(bsrc), "%s/../%s", data_path, item.mapped_path);
        if (item.file_num != 0xffff) {
            char bdst[2048];
            snprintf(bdst, sizeof(bdst), "./vfs_files/%04x", item.file_num);
            if (!copy_file(bdst, bsrc, entry.file_size)) {
                printf("Failed to copy %s to %s\n", bsrc, bdst);
                exit(1);
            }
        } else {
            vfs_offset_t ofs = entry.virtual_path_offset - sizeof(vfs_data_header);
            vfs_data_header *data_hdr = reinterpret_cast<vfs_data_header *>(&data[ofs]);
            vfs_content_header content_hdr;
            memset(&content_hdr, 0, sizeof(content_hdr));

            if (ftell(fp) != data_hdr->content_offset) {
                printf("Content offset does not match ftell: %d != %d\n", ftell(fp), data_hdr->content_offset);
                exit(1);
            }
            content_hdr.file_entry_num = n;
            if (fwrite(&content_hdr, sizeof(content_hdr), 1, fp) != 1) {
                printf("Failed to write content header %u\n", n + 1);
                exit(1);
            }
            if (!copy_file(fp, bsrc, item.header.file_size)) {
                printf("Failed to copy %s as inline content\n", bsrc);
                exit(1);
            }
        }
        n++;
    }

    if (ftell(fp) != content_offset) {
        printf("Content size does not match ftell: %d != %d\n", ftell(fp), content_offset);
        exit(1);
    }
    printf("file offset %lu after inline content.\n", ftell(fp));
    fclose(fp);

    printf("Done!\n");

    free(data);
}


int main(int argc, char **argv) {

    if (argc != 3) {
        printf("usage: vfs-builder <mappings_file> <vfs_file>\n");
        exit(255);
    }

#if _DEBUG && _MSC_VER // msvs memory debug
	// Get current flag
	int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

	// Turn on leak-checking bit.
	tmpFlag |= _CRTDBG_LEAK_CHECK_DF|_CRTDBG_ALLOC_MEM_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_CHECK_CRT_DF|_CRTDBG_LEAK_CHECK_DF;

	// Turn off CRT block checking bit.
	//tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;

	// Set flag to the new value.
	_CrtSetDbgFlag(tmpFlag);
#endif

    MappingsList mappings_list;
    mappings_list = read_mappings(argv[1]);

	char *path = _strdup(argv[1]);
	char *p = strrchr(path, '/');
    if (!p) {
		p = strrchr(path, '\\');
    }
    if (p) {
		p++;
		*p = 0;
    } else {
		*path = 0;
    }

    convert_mappings_vfs(mappings_list, argv[2], path);

	free(path);
    free_mappings(mappings_list);

    return 0;
}
