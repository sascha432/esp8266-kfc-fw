
# 4/1
split = {
    'sketch': 0x40200000,
    'empty': 0x402FEFF0,
    'fs': 0x40500000,
    'kfcfw': 0x405EA000,
    'savecrash': 0x405FA000,
    'eeprom': 0x405FB000,
    'rfcal': 0x405FC000,
    'wifi': 0x405FD000,
    'end': 0x40600000,
};


# 4/2
split = {
    'sketch': 0x40200000,
    'empty': 0x402FEFF0,
    'fs': 0x40400000,
    'kfcfw': 0x405EA000,
    'savecrash': 0x405FA000,
    'eeprom': 0x405FB000,
    'rfcal': 0x405FC000,
    'wifi': 0x405FD000,
    'end': 0x40600000,
};

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

print("/* Flash Layout */");
for key, values in split.items():
    print("/* %-*.*s @0x%08X (%dKB, %d) */" % (klen, klen, key, values[0], values[2] / 1024, values[2]));


print("\nMEMORY\n"
"{\n"
"  dport0_0_seg :                        org = 0x3FF00000, len = 0x10\n"
"  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000\n"
"  iram1_0_seg :                         org = 0x40100000, len = 0x8000\n"
"  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0\n"
"}\n"
);

provides = {
    '_FS_start': split['fs'][0],
    '_FS_end': split['fs'][1] - 4095,
    '_FS_page': 0x100,
    '_FS_block': 0x02000,
    '_KFCFW_start': split['kfcfw'][0],
    '_KFCFW_end': split['kfcfw'][1] - 4095,
    '_ESPSAVECRASH_start': split['savecrash'][0],
    '_EEPROM_start': split['eeprom'][0],
    '/* The following symbols are DEPRECATED and will be REMOVED in a future release */': -1,
    '_SPIFFS_start': split['fs'][0],
    '_SPIFFS_end': split['fs'][1] - 4095,
    '_SPIFFS_page': 0x100,
    '_SPIFFS_block': 0x02000,
}

for key, value in provides.items():
    if value == -1:
        print(key)
    else:
        print("PROVIDE ( %s = 0x%X );" % (key, value));


print('\nINCLUDE "local.eagle.app.v6.common.ld"')
