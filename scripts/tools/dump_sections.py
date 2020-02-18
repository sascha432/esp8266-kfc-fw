#
# Author: sascha_lammers@gmx.de
#

import subprocess
import re
import os
import json
import sys
import argparse

def create_sym(symbol, addr = 0):
    result = {"s": symbol["section"]["name"], "n": symbol["name"], "a": hex(symbol["value"]), "l": symbol["size"]}
    if addr!=0:
        result["m"] = addr
    return result

def print_output(output):
    for result in output:
        try:
            src = hex(result["m"])
        except:
            src = "0x0"
        print(args.output_format.format(src_address=src, address=result["a"], size=result["l"], name=result["n"], section=result["s"]))

if sys.platform[0:1]=="w":
    readelf_bin = "C:/Users/sascha/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-readelf.exe"
    addr2line_bin = "C:/Users/sascha/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-addr2line.exe"
else:
    readelf_bin = "/usr/bin/readelf"
    addr2line_bin = "/usr/bin/addr2line"

parser = argparse.ArgumentParser(description="readelf/addr2line dumper")
parser.add_argument("env", help="environment")
parser.add_argument("-n", "--name", help="names+dumpt to dump", nargs='*', default=[])
parser.add_argument("-a", "--addr", help="addresses to dump", nargs='*', default=[])
parser.add_argument("--addr2line", help="use addr2line", action="store_true", default=False)
parser.add_argument("--readelf", help="path to readelf binary", default=readelf_bin)
parser.add_argument("--elf", help="path to elf binary. {0} is the environment placeholder", default="./.pio/build/{0}/firmware.elf")
parser.add_argument("-v", "--verbose", help="verbose output", action="store_true", default=False)
parser.add_argument("-f", "--output-format", help="output format or json", default="{src_address} {address}:{size:5d} @{section:12.12s} {name}")
args = parser.parse_args()

elf = args.elf.format(args.env)
readelf = args.readelf.split(" ")
readelf.append(elf)
readelf.append("-a")
readelf.append("-W")
if args.verbose:
    print("elf: " + elf)
    print("readelf: " + str(readelf))

if args.addr2line:

    addresses = []
    addr2line = [addr2line_bin, "-s", "-f", "-C", "-e", elf]
    for addr in args.addr:
        if addr[0:2]!="0x":
            addr = hex(int(addr))
        addr2line.append(addr)
        addresses.append(addr)

    addr2line = subprocess.Popen(addr2line, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output, errors = addr2line.communicate()

    if addr2line.wait()==0:
        lines = output.decode("utf-8").splitlines()

    output = []

    num = 0
    for line in lines:
        if num%2==0:
            line = line.strip()
            if line!="??":
                addr = addresses[int(num / 2)]
                output.append({"s": "", "n": line, "a": addr, "l": 0, "m": addr});
        num = num + 1

else:

    readelf = subprocess.Popen(readelf, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output, errors = readelf.communicate()

    if readelf.wait()==0:
        lines = output.decode("utf-8").splitlines()
        sections = {}
        symbols_by_address = {}
        symbols = {}

        cre = {
            "hdr": re.compile("Section Headers:"),
            "sym": re.compile("Symbol table '\.symtab' contains"),
            "hdrs": re.compile("\[([ 0-9]+)\]\s+([^ ]+)\s+([^ ]+)\s+([^ ]+)\s+([^ ]+)\s+([^ ]+)"),
            "syms": re.compile("\s*([0-9]+):\s+([0-9a-z]+)\s+([0-9]+)\s+([^ ]+)\s+([^ ]+)\s+([^ ]+)\s+([^ ]+)\s+([^ ]+)")
        }

        type = 0
        for line in lines:
            if len(line)>3:
                if cre["hdr"].search(line):
                    type = 1
                elif cre["sym"].search(line):
                    type = 2
                elif line[0]!=' ':
                    type = 0

                if type==1:
                    m = cre["hdrs"].search(line)
                    if m:
                        section = int(m.group(1))
                        name = m.group(2)
                        if name[0:4]!=".xt.":
                            sections[section] = { "name": name, "type": m.group(3), "addr": int(m.group(4), base=16), "off": int(m.group(5), base=16), "size": int(m.group(6), base=16) }

                elif type==2:
                    m = cre["syms"].search(line)
                    if m:
                        try:
                            section = int(m.group(7))
                        except:
                            section = 0
                        if section in sections.keys():
                            if section not in symbols.keys():
                                symbols[section] = []
                            value = int(m.group(2), base=16)
                            if value!=0:
                                section_addr = "0"
                                for sec in sections.values():
                                    start = sec["addr"]
                                    end = start + sec["size"]
                                    if value>=start and value<=end:
                                        section_addr = sec["name"]
                                symbol = {
                                    "section": sections[section],
                                    "value": value,
                                    "size": int(m.group(3)),
                                    "type": m.group(4),
                                    "bind": m.group(5),
                                    "vis": m.group(6),
                                    "ndx": m.group(7),
                                    "name": m.group(8),
                                }
                                symbols[section].append(symbol);
                                if section_addr not in symbols_by_address.keys():
                                    symbols_by_address[section_addr] = []
                                symbols_by_address[section_addr].append(symbol);

        output = []

        for addr in args.addr:
            if addr[0:2]=="0x":
                addr = int(addr, base=16)
                for section in symbols:
                    for symbol in symbols[section]:
                        try:
                            if symbol["value"]>=addr and symbol["value"]<addr+symbol["size"]:
                                output.append(create_sym(symbol, addr))
                        except:
                            pass

        for name in args.name:
            for section in symbols:
                for symbol in symbols[section]:
                    try:
                        m = re.search(name, symbol["name"])
                        if m:
                            output.append(create_sym(symbol))
                    except:
                        pass



if args.output_format=="json":
    print(json.dumps(output))
else:
    print_output(output)
