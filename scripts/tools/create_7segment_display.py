#!/usr/bin/env python3
#
# Author: sascha_lammers@gmx.de
#

# script to create the PROGMEM data for the pixel to 7 segment mapping
# class SevenSegmentDisplay holds all the different types identified by a unique name
# see "clockv2" for a more detailed explanation of the data

import json
import argparse
import enum
import re
import sys
import time
from os import path
from fonts import font_data, Font, ConsoleCanvas

project_dir = path.abspath(path.join(path.dirname(__file__), '..', '..'))
header_dir = path.abspath(path.join(project_dir, 'src/plugins/clock'))

parser = argparse.ArgumentParser(description='7 Segment Tool')
parser.add_argument('action', help='action to execute', choices=['list', 'dump', 'dumpcode', 'json', 'fonts', 'testgfx'])
parser.add_argument('-n', '--name', help='name of the display to create', type=str)
parser.add_argument('-O', '--output', help='export to header file', default=path.join(header_dir, 'display_{name}.h'))
parser.add_argument('--verbose', help='Enable verbose output', action="store_true", default=False)
parser.add_argument('--clear-font-cache', help='Clear fonts cache', action="store_true", default=False)
args = parser.parse_args()

font_data.verbose = args.verbose
font_data.clear_font_cache = args.clear_font_cache

if args.action in ('dump', 'dumpcode') and not args.name:
    parser.error('dump requires --name')

class Segments(str, enum.Enum):
    A = 'a'
    B = 'b'
    C = 'c'
    D = 'd'
    E = 'e'
    F = 'f'
    G = 'g'

    def __str__(self):
        return str(self.value).upper()

SEGMENTS_LIST = [Segments.A, Segments.B, Segments.C, Segments.D, Segments.E, Segments.F, Segments.G]

class Colons(str, enum.Enum):
    TOP = 'top'
    BOTTOM = 'bottom'

    def __str__(self):
        return str(self.value)[0].upper()

COLONS_LIST = [Colons.TOP, Colons.BOTTOM]

# different mappings for the pixels
class SevenSegmentDisplay(object):
    displays = {
        # ---
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
        # ---
        'clock_4': {
            'digits': (4, 2),
            'colons': (1, 1),
            'segment_order': { Segments.A: 0, Segments.B: 1, Segments.C: 3, Segments.D: 4, Segments.E: 5, Segments.F: 6, Segments.G: 2 },
            'order': (
                ('digit', 0),
                ('digit', 1),
                ('colon', 0),
                ('digit', 2),
                ('digit', 3),
            ),
            'pixel_animation_order': list(range(0, 7 * 2)),
        },
        # ---
        'clockv2': {
            # (number of digits, pixels per segment)
            'digits': (4, 4),
            # (number of colons, pixels per dot)
            'colons': (1, 2),
            # (order of the segments)
            'segment_order': { Segments.A: 6, Segments.B: 5, Segments.C: 2, Segments.D: 3, Segments.E: 4, Segments.F: 0, Segments.G: 1 },
            # (order of digits and colons)
            'order': (
                ('colon', 0),
                ('digit', 2),
                ('digit', 3),
                ('digit', 1),
                ('digit', 0),
            ),
            # (order of the pixels per digit for animations, should be a continuous loop through the 8 )
            'pixel_animation_order': (28, 29, 30, 31, 24, 25, 26, 27, 11, 10, 9, 8, 20, 21, 22, 23, 16, 17, 18, 19, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4),
        },
        # ---
        # test for using a LED matrix displaying a 7 segment clock using a font
        'matrix_clock': {
            'digits': (6, 1),
            'colons': (2, 1),
            'segment_order': { Segments.A: 0, Segments.B: 1, Segments.C: 2, Segments.D: 3, Segments.E: 4, Segments.F: 5, Segments.G: 6 },
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
            'pixel_mapping': (
                {'000': ' ', '001': ' ', '002': ' ', '003': ' ', '004': ' ', '005': ' ', '006': ' ', '007': ' ', '008': ' ', '009': ' ', '010': ' ', '011': ' ', '012': ' ', '013': ' ', '014': ' ', '015': ' ', '016': ' ', '017': ' ', '018': ' ', '019': ' ',  '020': ' ', '021': ' ', '022': ' ', '023': ' ', '024': ' ', '025': ' ', '026': ' ', '027': ' ', '028': ' ', '029': ' ', '030': ' ', '031': ' ', },
                {'063': ' ', '062': ' ', '061': ' ', '060': ' ', '059': ' ', '058': ' ', '057': ' ', '056': ' ', '055': ' ', '054': ' ', '053': ' ', '052': ' ', '051': ' ', '050': ' ', '049': ' ', '048': ' ', '047': ' ', '046': ' ', '045': ' ', '044': ' ',  '043': ' ', '042': ' ', '041': ' ', '040': ' ', '039': ' ', '038': ' ', '037': ' ', '036': ' ', '035': ' ', '034': ' ', '033': ' ', '032': ' ', },
                {'064': ' ', '065': ' ', '066': ' ', '067': ' ', '068': ' ', '069': ' ', '070': ' ', '071': ' ', '072': ' ', '073': ' ', '074': ' ', '075': ' ', '076': ' ', '077': ' ', '078': ' ', '079': ' ', '080': ' ', '081': ' ', '082': ' ', '083': ' ',  '084': ' ', '085': ' ', '086': ' ', '087': ' ', '088': ' ', '089': ' ', '090': ' ', '091': ' ', '092': ' ', '093': ' ', '094': ' ', '095': ' ', },
                {'127': ' ', '126': ' ', '125': ' ', '124': ' ', '123': ' ', '122': ' ', '121': ' ', '120': ' ', '119': ' ', '118': ' ', '117': ' ', '116': ' ', '115': ' ', '114': ' ', '113': ' ', '112': ' ', '111': ' ', '110': ' ', '109': ' ', '108': ' ',  '107': ' ', '106': ' ', '105': ' ', '104': ' ', '103': ' ', '102': ' ', '101': ' ', '100': ' ', '099': ' ', '098': ' ', '097': ' ', '096': ' ', },
                {'128': ' ', '129': ' ', '130': ' ', '131': ' ', '132': ' ', '133': ' ', '134': ' ', '135': ' ', '136': ' ', '137': ' ', '138': ' ', '139': ' ', '140': ' ', '141': ' ', '142': ' ', '143': ' ', '144': ' ', '145': ' ', '146': ' ', '147': ' ',  '148': ' ', '149': ' ', '150': ' ', '151': ' ', '152': ' ', '153': ' ', '154': ' ', '155': ' ', '156': ' ', '157': ' ', '158': ' ', '159': ' ', },
                {'191': ' ', '190': ' ', '189': ' ', '188': ' ', '187': ' ', '186': ' ', '185': ' ', '184': ' ', '183': ' ', '182': ' ', '181': ' ', '180': ' ', '179': ' ', '178': ' ', '177': ' ', '176': ' ', '175': ' ', '174': ' ', '173': ' ', '172': ' ',  '171': ' ', '170': ' ', '169': ' ', '168': ' ', '167': ' ', '166': ' ', '165': ' ', '164': ' ', '163': ' ', '162': ' ', '161': ' ', '160': ' ', },
                {'192': ' ', '193': ' ', '194': ' ', '195': ' ', '196': ' ', '197': ' ', '198': ' ', '199': ' ', '200': ' ', '201': ' ', '202': ' ', '203': ' ', '204': ' ', '205': ' ', '206': ' ', '207': ' ', '208': ' ', '209': ' ', '210': ' ', '211': ' ',  '212': ' ', '213': ' ', '214': ' ', '215': ' ', '216': ' ', '217': ' ', '218': ' ', '219': ' ', '220': ' ', '221': ' ', '222': ' ', '223': ' ', },
                {'255': ' ', '254': ' ', '253': ' ', '252': ' ', '251': ' ', '250': ' ', '249': ' ', '248': ' ', '247': ' ', '246': ' ', '245': ' ', '244': ' ', '243': ' ', '242': ' ', '241': ' ', '240': ' ', '239': ' ', '238': ' ', '237': ' ', '236': ' ',  '235': ' ', '234': ' ', '233': ' ', '232': ' ', '231': ' ', '230': ' ', '229': ' ', '228': ' ', '227': ' ', '226': ' ', '225': ' ', '224': ' ', },
            )
        },
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
            address += self.num_pixels_colon // 2
        return [(i + address) for i in range(0, self.num_pixels_colon // 2) ]

    def get_digit_pixels(self, address, segment):
        for key, val in self.segment_order.items():
            if segment==key:
                start = address + (val * self.display['digits'][1])
                end = start + self.display['digits'][1]
                return list(range(start, end))





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

elif args.action=='fonts':

    for idx, font in enumerate(Font.get_available_fonts().values()):
        print('#%u %s' % (idx, font))

elif args.action=='testgfx':

    canvas = ConsoleCanvas(50, 11, '██', True)

    cfg = ('Tiny3x3a_2pt', 1, 5, None)
    cfg = ('Picopixel_5pt', 2, 6, None)
    cfg = ('simple_small_pixels_4pt', 0, 8, 0)
    cfg = ('Dialog_8pt', 2, 6, 0)
    cfg = ('digital_7_mono_8pt', 0, 6, 6)

    cfg = list(cfg)

    font = Font(cfg[0], canvas)
    font.wrap = False
    font.set_text_size(1)
    glyph = font.get_glyph('0')
    cfg[2] = glyph.height
    if cfg[3]==None:
        xAdvance = 0
    elif cfg[3]:
        xAdvance = cfg[3]
    else:
        xAdvance = glyph.xAdvance

    canvas.font = font
    canvas.font.set_text_size(1)
    canvas.bgcolor = 0
    canvas.color = 0x0000ff
    canvas.cursor(False)
    n=0
    canvas.clear(True)
    while True:
        canvas.clear()
        # canvas.set_pixel(n % canvas.width, n % canvas.height, 0xff0000)
        # n+=1
        if True:
            s = time.strftime('%H:%M:%S')
            if xAdvance:
                x = cfg[1]
                for c in s:
                    font.print(x, cfg[2], c)
                    x += xAdvance
            else:
                font.print(cfg[1], cfg[2], s)
        canvas.flush()
        print('\n%s' % font.font)
        time.sleep(0.5)

elif args.action=='json':

    display = SevenSegmentDisplay(args.name)

    def json_dumps(obj, indent=None, cls=None):
        sout = json.dumps(obj, indent=indent, cls=cls)
        if indent==None:
            return sout
        sre = r'( [\[\{])[\r\n]+([^\[\{]+)([\]\}],{0,1}[\r\n]+)'
        g = re.search(sre, sout, re.I|re.M|re.DOTALL)
        while g:
            i = re.match(r'(\s+)%s' % indent, g.group(2), re.DOTALL)
            sout = sout.replace(g.group(0), \
                re.sub(r'([\]\}])\s+([\]\}])', '\\1\n' + (i and i.group(1)[0:-len(indent)] or '') + '\\2', g.group(1) + re.sub(r'((^)?\s+(\Z)?)', ' ', g.group(2), re.DOTALL) + \
                g.group(3)))
            g = re.search(sre, sout, re.I|re.M|re.DOTALL)
        return sout;

    print(json_dumps({args.name: display.display}, indent=' '*2))

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

    flatten_arrays = True
    disclaimer = '// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY\n// GENERATOR: ./scripts/tools/create_7segment_display.py dumpcode --name %s\n' % args.name

    display = SevenSegmentDisplay(args.name)
    headerFile = args.output.format(name=args.name)

    print('Output: %s' % headerFile)

    with open(headerFile, 'wt') as file:

        if display.num_pixels_digits<=255:
            pixel_type = 'uint8_t'
            pixel_read_func = 'pgm_read_byte'
        else:
            pixel_type = 'uint16_t'
            pixel_read_func = 'pgm_read_word'

        file.write(disclaimer)
        file.write('\n')
        file.write('#pragma once\n')
        file.write('\n')
        file.write('#include <Arduino_compat.h>\n')
        file.write('#include <stdint.h>\n')
        file.write('#include <array>\n')
        file.write('\n')
        file.write('using PixelAddressType = %s;\n' % pixel_type)
        file.write('using PixelAddressPtr = const PixelAddressType *;\n')
        file.write('\n')
        file.write('inline static PixelAddressType readPixelAddress(PixelAddressPtr ptr) {\n')
        file.write('    return %s(ptr);\n' % pixel_read_func)
        file.write('}\n')
        file.write('\n')
        file.write('static constexpr PixelAddressType kNumDigits = %u;\n' % display.num_digits)
        file.write('static constexpr PixelAddressType kNumColons = %u;\n' % display.num_colons)
        file.write('\n')
        file.write('static constexpr PixelAddressType kNumPixels = %u;\n' % display.num_pixels)
        file.write('static constexpr PixelAddressType kNumPixelsDigits = %u;\n' % display.num_pixels_digits)
        file.write('static constexpr PixelAddressType kNumPixelsColons = %u;\n' % display.num_pixels_colons)
        file.write('static constexpr PixelAddressType kNumPixelsPerSegment = %u;\n' % display.num_digits)
        file.write('static constexpr PixelAddressType kNumPixelsPerDigit = kNumPixelsPerSegment * 7;\n')
        file.write('static constexpr PixelAddressType kNumPixelsPerColon = %u;\n' % display.colons_len)
        file.write('\n')

        file.write('#define SEVEN_SEGMENT_DIGITS_TRANSLATION_TABLE ')

        n = display.num_digits
        for i in range(0, n):
            m = display.segments_len
            for j in range(0, m):
                address = display.get_digit_start(i)
                o = display.digits_pixels
                px = display.get_digit_pixels(address, SEGMENTS_LIST[j])
                for l in range(0, o):
                    file.write('%u' % px[l])
                    if l<o-1:
                        file.write(', ')
                if j<m-1:
                    file.write(', \\\n    ')
            if i<n-1:
                file.write(', \\\n    ')
        file.write('\n')
        file.write('\n')

        file.write('#define SEVEN_SEGMENT_COLONTRANSLATIONTABLE ')
        n = display.num_colons
        for i in range(0, n):
            m = display.colons_len
            for j in range(0, m):
                address = display.get_colon_start(i)
                o = display.colons_pixels
                px = display.get_colon_pixels(address, COLONS_LIST[j])
                for l in range(0, o):
                    file.write('%u' % px[l])
                    if l<o-1:
                        file.write(', ')
                if j<m-1:
                    file.write(', \\\n    ')
            if i<n-1:
                file.write(', \\\n    ')
        file.write('\n')
        file.write('\n')

        file.write('#define SEVEN_SEGMENT_PIXEL_ANIMATION_ORDER %s\n' % display.animation_order_array_str)
        file.write('\n')


    filename = 'display.js'
    print('Output: %s' % filename)
    print('This file needs to be included in "Resources\html\serial-console.html"');

    with open(filename, 'wt') as file:

        def write_digit(i):
            address = display.get_digit_start(i)
            file.write("var d = $('<div class=\"digit-container\" id=\"digit_%u\">' + $('#digit-prototype').html() + '</div>');\n" % i)
            for j in range(0, display.segments_len):
                px = display.get_digit_pixels(address, SEGMENTS_LIST[j])
                file.write("ss(d, '%s', %s); " % (SEGMENTS_LIST[j].value.upper(), str(px)[1:-1]))
            file.write("\n$('.digits-container').append(d);\n")

        def write_colon(i):
            address = display.get_colon_start(i)
            file.write("var c = $('<div class=\"colon-container\" id=\"colon_%u\">' + $('#colon-prototype').html() + '</div>');\n" % i)
            for j in range(0, display.colons_len):
                px = display.get_colon_pixels(address, COLONS_LIST[j])[0:display.colons_pixels]
                file.write("ss(c, '%s', %s); " % (COLONS_LIST[j].value[0].upper(), str(px)[1:-1]))
            file.write("\n$('.digits-container').append(c);\n")

        write_digit(0)
        write_digit(1)
        write_colon(0)
        write_digit(2)
        write_digit(3)
        if display.num_digits>4:
            write_colon(1)
            write_digit(4)
            write_digit(5)
