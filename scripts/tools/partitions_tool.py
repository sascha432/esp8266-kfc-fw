
import os
import sys
from os import path

def openw(file):
    if not file or file=='-' or file=='stdout':
        return sys.stdout
    return open(file, "w")

start_addr = 0x9000
end_addr = 4096 * 1024 - 1      # 4Mbyte
# 'size' is in KB and must be a multiple of 4KB/4096Byte, size 0 mens fill up to the end

partitions1 = [
    # Name          Type        SubType      Size   Flags
    [ 'nvs',        'data',     'nvs',          20, '' ],
    [ 'otadata',    'data',     'ota',           8, '' ],
    [ 'uf2',        'app',      'factory',    2048, '' ],
    # [ 'uf2',        'app',      'factory',    2432, '' ],
    [ 'eeprom',     'data',     'fat',           4, '' ],
    [ 'savecrash',  'data',     'fat',          16, '' ],
    # [ 'savecrash',  'data',     'fat',          48, '' ],
    [ 'kfcfw',      'data',     'nvs',          64, '' ],
    # [ 'coredump',   'data',     'coredump',     16, '' ],
    [ 'spiffs',     'data',     'spiffs',        0, '' ],
]

def create_partitions(filename, start, end, partitions):
    try:
        file = openw(filename)
        print('# ESP-IDF Partition Table', file=file);
        print('# Name,   Type, SubType, Offset,  Size, Flags', file=file);
        print('# bootloader.bin,,          0x1000, 32K,', file=file);
        print('# partition table,          0x8000, 4K,', file=file);
        print('', file=file)
        offset = start
        for items in partitions:
            (name, type, sub_type, size, flags) = items
            if size==0:
                size = (end - offset) * 4
            else:
                size = size * 4096
            size = int(size / 4096)
            if size < 4:
                print('name=%s size=%u < 4' % (name, size))
                exit(1)
            print('%s, %s, %s, 0x%x, %uK, %s' % (name, type, sub_type, offset, size, flags), file=file)
            offset += size * 1024
    finally:
        print('')
        print('# start=0x%x end=0x%x size=%uMByte' % (start, end, (end + 1) / 1024 / 1024))
        print('# end=0x%x size=%uMB' % (offset, offset / 1024))
        if file!=sys.stdout:
            print("# created %s" % file.name)
            file.close()


#ota_0,     0,    ota_0,    0x10000,   1408K,
#ota_1,     0,    ota_1,   0x170000,   1408K,
#uf2,       app,  factory, 0x2d0000,    256K,
#eeprom,    data, fat,     0x310000,      4K,
#savecrash, data, fat,     0x311000,      8K,
#data,      data, spiffs,  0x313000,    952K,


eagle_dir = path.abspath('./conf/ld');
if not os.path.isdir(eagle_dir):
    print('cannot find: %s' % eagle_dir)
    sys.exit(-1)

# create_partitions('-', start_addr, end_addr, partitions1);
create_partitions(path.join(eagle_dir, 'partitions.csv'), start_addr, end_addr, partitions1);
# create_partitions(path.join(eagle_dir, 'partitions_maxapp.csv'), start_addr, end_addr, partitions1);
