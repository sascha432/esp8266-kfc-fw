
import os
import sys
from os import path

def create_eagle_ld(file, split):
    klen = 0
    prev_key = None
    for key in split:
        if prev_key:
            start = split[prev_key]
            end = split[key] - 1
            klen = max(len(prev_key), klen)
            split[prev_key] = [ start, end, end - start + 1 ]
        prev_key = key

    del split['end']

    def openw(file):
        if not file or file=='-' or file=='stdout':
            return sys.stdout
        return open(file, "w")


    try:
        file = openw(file)

        file.write('/* Flash Layout */\n');
        for key, values in split.items():
            file.write('/* %-*.*s @0x%08X (%dKB, %d) */\n' % (klen, klen, key, values[0], values[2] / 1024, values[2]));


        file.write('\nMEMORY\n'
            '{\n'
            '  dport0_0_seg :                        org = 0x3FF00000, len = 0x10\n'
            '  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000\n'
            '  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0\n'
            '}\n\n'
        );

        provides = {
            '_FS_start': split['fs'][0],
            '_FS_end': split['fs'][1] - 4095,
            '_FS_page': 0x100,
            '_FS_block': 0x02000,
            '_NVS_start': split['nvs'][0],
            '_NVS_end': split['nvs'][1] - 4095,
            '_NVS2_start': split['nvs2'][0],
            '_NVS2_end': split['nvs2'][1] - 4095,
            '_SAVECRASH_start': split['savecrash'][0],
            '_SAVECRASH_end': split['savecrash'][1] - 4095,
            '_EEPROM_start': split['eeprom'][0],
            '_EEPROM_end': split['eeprom'][1] - 4095,
            '/* The following symbols are DEPRECATED and will be REMOVED in a future release */': -1,
            '_SPIFFS_start': split['fs'][0],
            '_SPIFFS_end': split['fs'][1] - 4095,
            '_SPIFFS_page': 0x100,
            '_SPIFFS_block': 0x02000,
        }

        for key, value in provides.items():
            if value == -1:
                file.write(key)
                file.write('\n');
            else:
                file.write('PROVIDE ( %s = 0x%X );\n' % (key, value));

        file.write('\nINCLUDE "local.eagle.app.v6.common.ld"\n')

    finally:
        if file!=sys.stdout:
            print("created %s" % file.name)
            file.close()


eagle_dir = path.abspath('./conf/ld');
if not os.path.isdir(eagle_dir):
    print('cannot find: %s' % eagle_dir)
    sys.exit(-1)

# 4/1
split = {
    'sketch': 0x40200000,
    'empty': 0x402FEFF0,
    'fs': 0x40500000,
    'nvs': 0x405FB000 - ((8 + 40 + 32) * 0x1000),        # 8 x 4096 byte = 32K
    'nvs2': 0x405FB000 - ((40 + 32) * 0x1000),         # 40 x 4096 byte = 160K
    'savecrash': 0x405FB000 - (32 * 0x1000),           # 32 x 4096 byte = 128K
    'eeprom': 0x405FB000,
    'rfcal': 0x405FC000,
    'wifi': 0x405FD000,
    'end': 0x40600000,
};
create_eagle_ld(path.join(eagle_dir, 'eagle.flash.4m1m.ld'), split);

# 4/2
split = {
    'sketch': 0x40200000,
    'empty': 0x402FEFF0,
    'fs': 0x40400000,
    'nvs': 0x405FB000 - ((8 + 40 + 32) * 0x1000),        # 8 x 4096 byte = 32K
    'nvs2': 0x405FB000 - ((40 + 32) * 0x1000),         # 40 x 4096 byte = 160K
    'savecrash': 0x405FB000 - (32 * 0x1000),           # 32 x 4096 byte = 128K
    'eeprom': 0x405FB000,
    'rfcal': 0x405FC000,
    'wifi': 0x405FD000,
    'end': 0x40600000,
};
create_eagle_ld(path.join(eagle_dir, 'eagle.flash.4m2m.ld'), split);

