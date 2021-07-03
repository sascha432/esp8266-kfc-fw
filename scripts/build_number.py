#
# Author: sascha_lammers@gmx.de
#

import sys
import os
import re
import argparse

parser = argparse.ArgumentParser(description="Build Number Tool")
parser.add_argument("buildfile", help="build.h", type=argparse.FileType("r+"))
parser.add_argument("--print-contents", help="print build instead of writing to file", action="store_true", default=False)
parser.add_argument("--print-number", help="print build number", action="store_true", default=False)
parser.add_argument("--name", help="constant name", default="__BUILD_NUMBER")
parser.add_argument("--add", help="add value to build number", default=0, type=int)
parser.add_argument("--format", help="format for print number", default="{0:d}")
parser.add_argument("-v", "--verbose", help="verbose output", action="store_true", default=False)
args = parser.parse_args()

lines = args.buildfile.read().split("\n");
number = 0

output = ""
for line in lines:
    empty = line.strip()
    line = re.sub("#define\s*%s_INT\s*[0-9]+" % args.name, '', line)
    if line.strip()=='' and not empty:
        continue

    m = re.search("#define\s*%s\s*\"(.*?)\"" % args.name, line)
    try:
        number = int(m.group(1).strip());
        number = number + args.add
        line = "#define %s \"%d\"\n#define %s_INT %d" % (args.name, number + 1, args.name, number + 1)
    except:
        pass
    if args.print_number:
        if number!=0:
            print(args.format.format(number))
            break
    elif args.print_contents:
        print(line)
    else:
        output += line + "\n"

if number==0:
    raise Exception("Cannot find build number " + args.name)

if args.verbose:
    print("New build number: " + str(number + 1))

if output!="":
    args.buildfile.seek(0)
    args.buildfile.truncate(0)
    args.buildfile.write(re.sub("\n\n+", "\n\n", output.replace("\r", "")))

