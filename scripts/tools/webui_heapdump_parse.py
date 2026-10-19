# webui_heapdump_parse.py <dumpA> <dumpB> - walks the umm_malloc block chain of two +HEAPDUMP outputs and
# compares the allocated blocks by size and content signature (umm block = 8 bytes, header next/prev uint16, bit
# 15 of next = free block, block 0 = free list head, the data of a used block starts 4 bytes into the block).
# The dumps are the serial output of +HEAPDUMP (DEBUG builds, ESP8266). A flash address in a block that grew is
# resolved with xtensa-lx106-elf-nm -C -n firmware.elf (the first word of a C++ object is its vtable + 8)
import re, sys, collections, struct

def load(path):
    mem = {}
    base = None
    for line in open(path):
        m = re.match(r'\+HD ([0-9a-f]{8}) ([0-9a-f]+)', line)
        if m:
            addr = int(m.group(1), 16)
            if base is None:
                base = addr
            mem[addr] = bytes.fromhex(m.group(2))
    data = bytearray()
    for addr in sorted(mem):
        data += mem[addr]
    return base, bytes(data)

def walk(base, data):
    blocks = []
    nblocks = len(data) // 8
    cur = 0
    seen = set()
    while True:
        nxt, prv = struct.unpack_from('<HH', data, cur * 8)
        free = bool(nxt & 0x8000)
        nxt &= 0x7fff
        if cur != 0:
            if nxt == 0 or nxt <= cur or nxt > nblocks:
                end = nblocks
            else:
                end = nxt
            payload = data[cur * 8 + 4: end * 8]
            blocks.append((cur, base + cur * 8 + 4, (end - cur) * 8 - 4, free, payload))
        if nxt == 0 or nxt in seen or nxt > nblocks:
            break
        seen.add(nxt)
        cur = nxt
    return blocks

def signature(payload, size):
    # strings: printable prefix; otherwise the first 8 bytes with heap pointers masked
    p = payload[:size]
    text = re.match(rb'[\x20-\x7e]{4,}', p)
    if text:
        return 'S:' + text.group(0)[:40].decode()
    words = []
    for i in range(0, min(16, len(p)) - 3, 4):
        w = struct.unpack_from('<I', p, i)[0]
        if 0x3ffe8000 <= w < 0x40000000:
            words.append('heap')
        elif 0x40200000 <= w < 0x40300000:
            words.append('f%08x' % w)   # flash address (vtable, function, PROGMEM string)
        else:
            words.append('%08x' % w)
    return 'B:' + ' '.join(words)

result = {}
for name in sys.argv[1:3]:
    base, data = load(name)
    blocks = walk(base, data)
    used = [b for b in blocks if not b[3]]
    free = sum(b[2] + 4 for b in blocks if b[3])
    print('%s: base=0x%08x blocks=%u used=%u used_bytes=%u free_bytes~%u' % (name.split('/')[-1], base, len(blocks), len(used), sum(b[2] + 4 for b in used), free))
    c = collections.Counter()
    for b in used:
        c[(b[2], signature(b[4], b[2]))] += 1
    result[name] = (c, used)

(ca, ua), (cb, ub) = result[sys.argv[1]], result[sys.argv[2]]
print('\nblock kinds that grew from A to B (count A -> B, size, signature):')
rows = []
for k in set(ca) | set(cb):
    d = cb[k] - ca[k]
    if d > 0:
        rows.append((d * (k[0] + 4), d, k))
for total, d, (size, sig) in sorted(rows, reverse=True)[:40]:
    print('  +%3u x %4u bytes = %5u  (%u -> %u)  %s' % (d, size, total, ca[(size, sig)], cb[(size, sig)], sig))

print('\nmost frequent used blocks in B:')
for (size, sig), n in cb.most_common(25):
    print('  %4u x %4u bytes  %s' % (n, size, sig))
