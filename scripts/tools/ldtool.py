
import os
import sys
from os import path

def create_eagle_ld(file, split, layout):
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

        file.write('/* Flash Layout %s */\n' % layout);
        for key, values in split.items():
            file.write('/* %-*.*s @0x%08X (%dKB, %d, 0x%x) */\n' % (klen, klen, key, values[0], values[2] / 1024, values[2], values[2]));


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

nvs_size = 8 # 4K blocks, 8 blocks take about 16ms to initialize while 16 take 25ms.
# nvs_size = 1 # set to 1 to disable, init with less than 12KB will fail

savecrash_size = 8

sketch_addr = 0x40200000
empty_addr = 0x402FEFF0
end_addr = 0x40200000 + 0x400000 # 4MB
eeprom_addr = end_addr - 0x1000 - 0x1000 - 0x3000 # eeprom=4KB rfcal=4KB wifi=12KB

layout = 'flash=4M fs=1M (256 sectors)'
split = {
    'sketch': sketch_addr,
    'empty': empty_addr,
    'fs': eeprom_addr - ((nvs_size + savecrash_size + 256) * 0x1000),
    'nvs': eeprom_addr - ((nvs_size + savecrash_size) * 0x1000),
    'savecrash': eeprom_addr - (savecrash_size * 0x1000),
    'eeprom': eeprom_addr,
    'rfcal': eeprom_addr + 0x1000,
    'wifi': eeprom_addr + 0x2000,
    'end': end_addr,
};
create_eagle_ld(path.join(eagle_dir, 'eagle.flash.4m1m.ld'), split, layout);

layout = 'flash=4M fs=2M (480 sectors to keep "empty" bigger as the sketch for OTA)'
split = {
    'sketch': sketch_addr,
    'empty': empty_addr,
    'fs': eeprom_addr - ((nvs_size + savecrash_size + 480) * 0x1000),
    'nvs': eeprom_addr - ((nvs_size + savecrash_size) * 0x1000),
    'savecrash': eeprom_addr - (savecrash_size * 0x1000),
    'eeprom': eeprom_addr,
    'rfcal': eeprom_addr + 0x1000,
    'wifi': eeprom_addr + 0x2000,
    'end': end_addr,
};
create_eagle_ld(path.join(eagle_dir, 'eagle.flash.4m2m.ld'), split, layout);
