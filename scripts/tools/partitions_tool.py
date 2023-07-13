
import os
import sys
from os import path
import copy

# global config
SECTOR_SIZE = None
APP_OFS = None
CORE_SIZE = None
START_OFS = None
END_OFS = None
FLASH_SIZE = None
OTA_0 = None
OTA_1 = None
OTA_SIZE = None
NVS_SIZE = None
SECTOR_SIZE = 4096
KEEP_FREE_SIZE = SECTOR_SIZE * 4
APP_OFS = 0x10000
START_OFS = 0x9000
OUTPUT_FILE = None

EAGLE_DIR = path.abspath('./conf/ld');
if not os.path.isdir(EAGLE_DIR):
    raise Exception('cannot find dir: %s' % EAGLE_DIR)

# different files
FILES = (
    ('4M', '1M', False, 'Auto', '64K'),
    ('4M', '2M', False, 'Auto', '64K'),
    ('4M', '3M', False, '640K', 'Auto'),
    # ('4M', '1408K', True, 'Auto', '64K'),
    ('4M', '2M', False, None, '64K'),
    ('4M', '3M', False, None, '64K'),
    # ('4M', '1920K', True, None, '64K'),
)

#helpers

def openw(file):
    if not file or file is None or file=='-' or file=='stdout':
        return sys.stdout
    return open(os.path.join(EAGLE_DIR, file), "w")

def size_to_M(size):
    if size / 1024 % 1024 != 0:
        return size_to_K(size)
    return '%uM' % (size / 1024 / 1024)

def size_to_K(size):
    return '%uK' % (size / 1024)

# update list of partitions
# ready for output
def get_size_in_bytes(size):
    try:
        byte_size = 0

        if size is None or (type(size) in (int, float) and int(size)<0):
            return None
        if (type(size) in (int, float) and size == 0) or (type(size) is str and size.lower() == 'auto'):
            return 0
        if type(size) is str:
            size = size.lower()
            if size.endswith('m'): # MByte
                byte_size = int(size[0:-1]) * 1024 * 1024
            elif size.endswith('k'): # KByte
                byte_size = int(size[0:-1]) * 1024
            elif size.endswith('b'): # Bytes
                byte_size = int(size[0:-1])
            elif size.endswith('p') or size.endswith('s'): # Pages or sectors
                byte_size = int(size[0:-1]) * SECTOR_SIZE
            else:
                byte_size = int(size)
        elif type(size) in (int, float):
            byte_size = int(size)
        else:
            raise Exception('invalid type=%s' % type(size).__name__)

        if byte_size <= 0 or byte_size % SECTOR_SIZE != 0:
            raise Exception('invalid size %s (%d), multiple sectors of %u Byte required' % (str(size), byte_size, SECTOR_SIZE))

    except Exception as e:
        raise Exception('size=%s type=%s error=%s' % (size, type(size).__name__, e))

    return int(byte_size)

def get_total_size(start, end, partitions):
    name = None
    num = 0
    free_bytes = 0
    auto_size = None
    try:
        offset = start
        app_ofs = get_size_in_bytes(APP_OFS)
        for items in partitions:
            # check if it is a list
            if not type(items) in (list, tuple):
                partitions[num] = None
                num += 1
                continue

            # check length
            if not len(items) in (4, 5):
                partitions[num] = None
                num += 1
                continue

            if items[3] is None:
                partitions[num] = None
                num += 1
                continue

            # append flags if missing
            if len(items)==4:
                items.append(())

            # get all items
            (name, part_type, sub_type, size, flags) = items

            # flags
            if type(flags) in (list, tuple):
                flags = list(flags)
            else:
                l = []
                if type(flags) is str:
                    for flag in flags.split(','):
                        l.append(flag.strip(' \t\n\r'))
                flags = l

            # name
            if not type(name) is str:
                partitions[num] = None
                num += 1
                continue

            # check size
            byte_size = get_size_in_bytes(size)
            if byte_size is None:
                partitions[num] = None
                num += 1
                continue

            if byte_size == 0:
                if offset<app_ofs:
                    raise Exception('auto size cannot be used before the app at offset 0x%x' % app_ofs)
                if auto_size != None:
                    raise Exception('we already have one with auto size name %s' % (partitions[auto_size][0]))
                auto_size = num
            elif byte_size < SECTOR_SIZE:
                raise Exception('size 0x%x(%s) less than 0x%x' % (byte_size, str(size), SECTOR_SIZE))
            elif byte_size % SECTOR_SIZE != 0:
                raise Exception('size 0x%x is not a multiple of sector size 0x%x' % (byte_size, SECTOR_SIZE))

            partitions[num][3] = byte_size
            partitions[num].append(0)

            offset += byte_size
            num += 1

        free_bytes = end - offset
        if free_bytes<0 and auto_size is None:
            raise Exception('size=0x%x>0x%x' % (offset, end))

        num = 0
        offset = start
        for items in partitions:
            if items is None:
                num += 1
                continue

            (name, part_type, sub_type, size, flags, dummy) = items

            if 'app' in flags:
                if offset>app_ofs:
                    raise Exception('type app offset: %x>%x' % (offset, app_ofs))
                offset = app_ofs

            if auto_size == num:
                size = free_bytes - KEEP_FREE_SIZE
                partitions[num][3] = size

            partitions[num][5] = offset
            offset += size

            # check of we still have space
            if offset>end:
                raise Exception('size=0x%x offset=0x%x' % (end, offset))

            num += 1

        # final check
        free_bytes = end - offset
        if free_bytes<0:
            raise Exception('size=0x%x>0x%x' % (offset, end))

    except Exception as e:
        raise Exception('name=%s num=%u e=%s' % (name, num + 1, str(e)))

    return offset

#
# write partitions to file
#
def create_partitions(filename, start, end, partitions):

    free_bytes = 0
    offset = 0
    file = None
    try:
        offset = get_total_size(start, end, partitions)
        free_bytes = end - offset

        file = openw(filename)
        print('# ESP-IDF Partition Table', file=file);
        print('', file=file)
        for items in partitions:
            if items is None:
                continue
            part_flags = ''
            (name, part_type, sub_type, size, flags, offset) = items
            for value in flags:
                if value.startswith('flags='):
                    (tmp, part_flags) = value.split('=', 2)
            print('%s, %s, %s, 0x%x, %s, %s' % (name, part_type, sub_type, offset, size_to_K(size), part_flags), file=file)
            offset += size

        print('\n# end_addr=0x%x' % (offset - 1), file=file);

    except Exception as e:
        raise e

    finally:
        print('')
        print('# start=0x%x end=0x%x offset=0x%x size=%s' % (start, end - 1, offset, size_to_M(end)))
        print('# end=0x%x size=%s free=0x%x' % (offset - 1, size_to_K(offset), free_bytes))
        if file!=sys.stdout and file!=None:
            print("# created %s" % file.name)
            file.close()


#
# main
# loop trhough all settings
#

for items in FILES:

    (FLASH_SIZE, APP_SIZE, HAS_OTA, SPIFF_SIZE, CORE_SIZE) = items
    if HAS_OTA:
        OTA_0 = APP_SIZE
        OTA_1 = APP_SIZE
        APP_SIZE = None

    SECTOR_SIZE = get_size_in_bytes(SECTOR_SIZE)
    APP_OFS = get_size_in_bytes(APP_OFS)
    CORE_SIZE = get_size_in_bytes(CORE_SIZE)
    start_addr = get_size_in_bytes(START_OFS)
    FLASH_SIZE = get_size_in_bytes(FLASH_SIZE)
    end_addr = FLASH_SIZE
    OTA_0 = get_size_in_bytes(OTA_0)
    OTA_1 = get_size_in_bytes(OTA_1)
    APP_SIZE = get_size_in_bytes(APP_SIZE)
    NVS_SIZE = '5S'

    OTA_SIZE = (OTA_0!=None and OTA_0!=None) and '2S' or None
    if OTA_SIZE!=None:
        NVS_SIZE = '4S'
        APP_SIZE=None

    spiffs = ''
    if SPIFF_SIZE is None:
        spiffs = 'no_spiffs_'

    if OUTPUT_FILE is None:
        if APP_SIZE is None:
            OUTPUT_FILE = 'partitions_' + size_to_M(OTA_0) +  '_ota_' + spiffs + size_to_M(FLASH_SIZE) + '_flash.csv'
        else:
            OUTPUT_FILE = 'partitions_' + size_to_M(APP_SIZE) +  '_app_' + spiffs + size_to_M(FLASH_SIZE) + '_flash.csv'

    # M for MByte   '1M'
    # K for KByte   '4K'
    # B for Byte   '4096B'
    # S for pages/sectors   '1S'
    # type int/float with Bytes
    # 0 too use all remaining space
    # use name = None or size = None to skip the entry
    DEFAULT_PARTITIONS = [
        # Name,  Type,  SubType, Size, Flags
        [ 'nvs', 'data', 'nvs', NVS_SIZE ],
        [ 'phy_init', 'data', 'phy', '1S' ],
        [ 'otadata', 'data', 'ota', OTA_SIZE ],
        [ 'uf2', 'app', 'factory', APP_SIZE, ('app') ],
        [ 'app1', 'app', 'ota_0', OTA_0, ('app') ],
        [ 'app2', 'app', 'ota_1', OTA_1 ],
        [ 'eeprom', 'data', 'fat', '1S' ],
        [ 'nvs2', 'data', 'nvs', '40S' ],
        [ 'coredump', 'data', 'coredump', CORE_SIZE ],
        [ 'savecrash', 'data', 'fat', '4S' ],
        [ 'spiffs', 'data', 'spiffs',  SPIFF_SIZE ],
    ]


    create_partitions(OUTPUT_FILE, start_addr, end_addr, copy.deepcopy(DEFAULT_PARTITIONS));

    OUTPUT_FILE = None
