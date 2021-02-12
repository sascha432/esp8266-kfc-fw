#!/usr/bin/env python3
#
# Author: sascha_lammers@gmx.de
#
#

import sys
import argparse
import enum
from os import path

project_dir = path.abspath(path.join(path.dirname(__file__), '..', '..'))
header_dir = path.abspath(path.join(project_dir, 'src/plugins/clock'))

parser = argparse.ArgumentParser(description='OTA for KFC Firmware')
parser.add_argument('action', help='action to execute', choices=['list', 'dump', 'dumpcode'])
parser.add_argument('-n', '--name', help='name of the display to create', type=str)
parser.add_argument('-O', '--output', help='export to header file', default=path.join(header_dir, 'display_{name}.h'))
args = parser.parse_args()

if args.action in ('dump', 'dumpcode') and not args.name:
    parser.error('dump requires --name')

class Segments(enum.Enum):
    A = 'a'
    B = 'b'
    C = 'c'
    D = 'd'
    E = 'e'
    F = 'f'
    G = 'g'

SEGMENTS_LIST = [Segments.A, Segments.B, Segments.C, Segments.D, Segments.E, Segments.F, Segments.G]

class Colons(enum.Enum):
    TOP = 'top'
    BOTTOM = 'bottom'

COLONS_LIST =[Colons.TOP, Colons.BOTTOM]

class SevenSegmentDisplay(object):
    displays = {
        'clock': {
            'digits': (6, 2),
            'colons': (2, 1),
            'segment_order': { Segments.A: 0, Segments.B: 1, Segments.C: 3, Segments.D: 4, Segments.E: 5, Segments.F: 6, Segments.G: 2 },
            'order': (
                ('digit', 0),
                ('digit', 1),
                ('colon', 0),
                ('digit', 2),
                ('digit', 3),
                ('colon', 1),
                ('digit', 4),
                ('digit', 5),
            ),
            'pixel_animation_order': list(range(0, 7 * 2)),
        },
        'clockv2': {
            'digits': (4, 4),
            'colons': (1, 2),
            'segment_order': { Segments.A: 6, Segments.B: 5, Segments.C: 2, Segments.D: 3, Segments.E: 4, Segments.F: 0, Segments.G: 1 },
            'order': (
                ('colon', 0),
                ('digit', 2),
                ('digit', 3),
                ('digit', 1),
                ('digit', 0),
            ),
            'pixel_animation_order': (28, 29, 30, 31, 24, 25, 26, 27, 11, 10, 9, 8, 20, 21, 22, 23, 16, 17, 18, 19, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4),
        }
    }

    def names():
        return SevenSegmentDisplay.displays.keys()

    def __init__(self, name):
        self._name = name
        if name not in SevenSegmentDisplay.displays:
            raise RuntimeError('name %s does not exist' % name)
        self.display = SevenSegmentDisplay.displays[name]

    @property
    def name(self):
        return self._name

    @property
    def digits(self):
        return self.display['digits'][0]

    @property
    def num_digits(self):
        return self.display['digits'][0]

    @property
    def digits_pixels(self):
        return self.display['digits'][1]

    @property
    def colons(self):
        return self.display['colons'][0]

    @property
    def num_colons(self):
        return self.display['colons'][0]

    @property
    def colons_pixels(self):
        return self.display['colons'][1]

    @property
    def segments_len(self):
        return len(SEGMENTS_LIST)

    @property
    def colons_len(self):
        return len(COLONS_LIST)

    @property
    def segment_order(self):
        return self.display['segment_order']

    @property
    def order(self):
        return self.display['order']

    @property
    def segment_order_str(self):
        parts = []
        for key, val in self.display['segment_order'].items():
            parts.append('%s=%u' % (key, val))
        return ' '.join(parts)

    @property
    def num_pixels(self):
        return self.num_pixels_digits + self.num_pixels_colons

    @property
    def num_pixels_digit(self):
        return self.digits_pixels * self.segments_len

    @property
    def num_pixels_digits(self):
        return self.num_pixels_digit * self.num_digits

    @property
    def num_pixels_colon(self):
        return self.display['colons'][1] * len(COLONS_LIST)

    @property
    def num_pixels_colons(self):
        return self.num_pixels_colon * self.num_colons

    @property
    def animation_order_len(self):
        return len(self.display['pixel_animation_order'])

    @property
    def animation_order_array_str(self):
        parts = []
        for pixel in self.display['pixel_animation_order']:
            parts.append('%u,' % pixel)
        return (' '.join(parts)).rstrip(', ')

    def get_num_pixels(self, type):
        if type=='colon':
            return self.num_pixels_colon
        return self.num_pixels_digit

    def get_digit_start(self, digit):
        address = 0
        for type, num in self.order:
            if type=='digit' and num==digit:
                return address
            address += self.get_num_pixels(type)
        raise RuntimeError('digit %u does not exists' % digit)

    def get_colon_start(self, colon):
        address = 0
        for type, num in self.order:
            if type=='colon' and num==colon:
                return address
            address += self.get_num_pixels(type)
        raise RuntimeError('digit %u does not exists' % colon)

    def get_colon_pixels(self, address, type):
        if type==Colons.BOTTOM:
            address += self.num_pixels_colon
        return [(i + address) for i in range(0, self.num_pixels_colon) ]

    def get_digit_pixels(self, address, segment):
        for key, val in self.segment_order.items():
            if segment==key:
                start = address + (val * self.display['digits'][1])
                end = start + self.display['digits'][1]
                return list(range(start, end))


    def get_segment(self, digit, segment):
        pass



if args.action=='list':
    for name in SevenSegmentDisplay.names():
        display = SevenSegmentDisplay(name)

        print('-'*76)
        print('name: %s' % display.name)
        print('number of digits: %u (pixels %u)' % (display.digits, display.digits_pixels))
        print('number of colons: %u (pixels %u)' % (display.colons, display.colons_pixels))
        print('segment order: %s' % display.segment_order_str)
        print('digit pixels: %u' % display.num_pixels_digits)
        print('colon pixels: %u' % display.num_pixels_colons)
        print('total pixels: %u' % display.num_pixels)

elif args.action=='dump':

    display = SevenSegmentDisplay(args.name)
    j = 0
    for i in range(0, display.digits):
        n = display.get_digit_start(i)
        print('digit %u: %u-%u' % (i, n, n + display.num_pixels_digit - 1))
        for segment in SEGMENTS_LIST:
            print('  %s: %s' % (segment, display.get_digit_pixels(n, segment)))
        if i%2==1 and j<display.colons:
            n = display.get_colon_start(j)
            print('colon %u: %u-%u' % (j, n, n + display.num_pixels_colon - 1))
            for type in COLONS_LIST:
                print('  %s: %s' % (type, display.get_colon_pixels(n, type)))

            j += 1

elif args.action=='dumpcode':

    display = SevenSegmentDisplay(args.name)
    filename = args.output.format(name=args.name)

    print('Output: %s' % filename)

    with open(filename, 'wt') as file:

        file.write('// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY\n')
        file.write('// GENERATOR: ./scripts/tools/create_7segment_display.py dumpcode --name %s\n' % args.name)
        file.write('\n')
        file.write('#pragma once\n')
        file.write('\n')
        file.write('#include <stdint.h>\n')
        file.write('#include <array>\n')
        file.write('\n')
        file.write('using PixelAddressType = uint8_t;\n')
        file.write('\n')
        file.write('using SegmentPixelArray = std::array<PixelAddressType, %u>;\n' % display.digits_pixels)
        file.write('using SegmentArray = std::array<SegmentPixelArray, %u>;\n' % display.segments_len)
        file.write('using DigitArray = std::array<SegmentArray, %u>;\n' % display.num_digits)
        file.write('\n')
        file.write('using ColonPixelsArray = std::array<PixelAddressType, %u>;\n' % display.colons_pixels)
        file.write('using ColonPixelArray = std::array<ColonPixelsArray, %u>;\n' % display.colons_len)
        file.write('using ColonArray = std::array<ColonPixelArray, %u>;\n' % display.num_colons)
        file.write('\n')
        file.write('using AnimationOrderArray = std::array<PixelAddressType, %u>;\n' % display.animation_order_len)
        file.write('\n')
        file.write('static constexpr uint8_t kNumDigits = %u;\n' % display.num_digits)
        file.write('#define IOT_CLOCK_NUM_DIGITS %u\n' % display.num_digits)
        file.write('static constexpr uint8_t kNumColons = %u;\n' % display.num_colons)
        file.write('#define IOT_CLOCK_NUM_COLONS %u\n' % display.num_colons)
        file.write('static constexpr uint8_t kNumPixels = %u;\n' % display.num_pixels)
        file.write('static constexpr uint8_t kNumPixelsDigits = %u;\n' % display.num_pixels_digits)
        file.write('\n')
        file.write('static constexpr auto kDigitsTranslationTable = DigitArray({\n')
        n = display.num_digits
        for i in range(0, n):
            file.write('    SegmentArray({\n')
            m = display.segments_len
            for j in range(0, m):
                address = display.get_digit_start(i)
                file.write('        SegmentPixelArray({ ')
                o = display.digits_pixels
                px = display.get_digit_pixels(address, SEGMENTS_LIST[j])
                for l in range(0, o):
                    file.write('%u' % px[l])
                    if l<o-1:
                        file.write(', ')
                    else:
                        file.write(' })')
                if j<m-1:
                    file.write(',\n')
                else:
                    file.write('\n')
            file.write('    })')
            if i<n-1:
                file.write(',\n')
            else:
                file.write('\n')
        file.write('});\n')
        file.write('\n')
        file.write('static constexpr auto kColonTranslationTable = ColonArray({\n')
        n = display.num_colons
        for i in range(0, n):
            file.write('    ColonPixelArray({\n')
            m = display.colons_len
            for j in range(0, m):
                address = display.get_colon_start(i)
                file.write('        ColonPixelsArray({ ')
                o = display.colons_pixels
                px = display.get_colon_pixels(address, COLONS_LIST[j])
                for l in range(0, o):
                    file.write('%u' % px[l])
                    if l<o-1:
                        file.write(', ')
                    else:
                        file.write(' })')
                if j<m-1:
                    file.write(',\n')
                else:
                    file.write('\n')
            file.write('    })')
            if i<n-1:
                file.write(',\n')
            else:
                file.write('\n')
        file.write('});\n')
        file.write('\n')
        file.write('static constexpr auto kPixelAnimationOrder = AnimationOrderArray({%s});\n' % display.animation_order_array_str)
        file.write('\n')

