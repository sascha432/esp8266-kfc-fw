#
# Author: sascha_lammers@gmx.de
#

#
# GFXGlyph, GFXFont and Font.write() ported from Adafruit_GFX
#
# Copyright (c) 2013 Adafruit Industries.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#

import enum
from genericpath import getmtime
import glob
from os import fstat, path
import os
import fnmatch
import re
import sys
import time
import json
from . import font_data, FontData, AbstractCanvas
from typing import List, Union, Any, Tuple

class FontStyle(enum.Enum):
    DEFAULT = 'Default'
    BOLD = 'Bold'
    MONO = 'Mono'
    ITALIC = 'Italic'
    SMALL = 'Small'
    PIXEL = 'Pixel'
    LCD = 'LCD'

    def __str__(self):
        return str(self.value)

    def str(self, items: List[enum.Enum]):
        return ','.join([str(item) for item in items])


class FontStyles(tuple):
    def __new__(cls, items: List[FontStyle]):
        if not isinstance(items, (list, set, tuple, FontStyle)):
            raise RuntimeError('Invalid type: %s' % type(items));
        if isinstance(items, FontStyle):
            items = [items]
        obj = tuple.__new__(cls, list(items))
        return obj

    def __str__(self):
        return ','.join([str(item) for item in self])

    def __repr__(self):
        return '<%s(%s) @ %08x>' % (self.__class__.__qualname__, ','.join((str(item) for item in self)), id(self))


class GFXGlyph(object):
    def __init__(self, *arg, **kwargs):
        self.bitmapOffset, self.width, self.height, self.xAdvance, self.xOffset, self.yOffset = arg
        if 'char' in kwargs:
            self.char = kwargs['char']
        else:
            self.char = None

        if 'next_offset' in kwargs:
            self.next_offset = kwargs['next_offset']
        else:
            self.next_offset = max(self.xAdvance, self.width) * self.height;

    def __str__(self):
        return '[%u:%u] size=%ux%u adv=%u ofs=%ux%u char=%s' % (self.bitmapOffset, self.next_offset, self.width, self.height, self.xAdvance, self.xOffset, self.yOffset, self.char)

class FontBitmap(object):
    def __init__(self, font_object, bitmap):
        self.font_object = font_object # type: FontObject
        self.bitmap = bitmap

    def get(self, start, length) -> List[int]:
        return list(self.bitmap[start:start+length])


class FontGlyphs(object):
    def __init__(self, font_object, glyphs):
        self.font_object = font_object
        self.glyphs = glyphs

    def get(self, character) -> GFXGlyph:
        character = self.font_object.validate_character(character)
        if character:
            return GFXGlyph(*self.glyphs[character - self.font_object.first], next_offset=self.glyphs[character - self.font_object.first + 1][0], char=chr(character))
        return None


class GFXFont(object):
    def __init__(self, range: range, yAdvance: int, bitmap: tuple, glyphs: tuple):
        self.range = range
        self.advance = yAdvance
        self.bitmap = bitmap
        self.glyphs = glyphs

    @property
    def yAdvance(self):
        return self.advance

    @property
    def first(self):
        return self.range.start

    @property
    def last(self):
        return self.range.stop

class FontObject(GFXFont):
    def __init__(self, id:str, name:str, font_size:int, styles:FontStyles=FontStyle.DEFAULT, first:int=None, last:int=None, advance:int=None, bitmap:tuple=None, glyphs:tuple=None):
        if id not in FontData.FONT_DATA:
            if not (first and last and advance and bitmap and glyphs):
                raise RuntimeError('Font %s does not exist' % id)
            FontData.FONT_DATA[id] = {
                'name': name,
                'styles': styles,
                'first': first,
                'last': last,
                'advance': advance,
                'bitmap': bitmap,
                'glyphs': glyphs
            }
        font = FontData.FONT_DATA[id]
        GFXFont.__init__(self, range(font['first'] or first, font['last'] or last), advance or font['advance'], FontBitmap(self, bitmap or font['bitmap']), FontGlyphs(self, glyphs or font['glyphs']))
        self.id = id
        self.name = name or font['name']
        self.font_size_pt = font_size
        self.styles = FontStyles(styles)

    @property
    def yAdvance(self):
        return self.advance

    @property
    def char_range(self):
        return range(self.first, self.last)

    # returns None if the character is not available
    # otherwise the ascii value as integer
    def validate_character(self, character):
        if isinstance(character, str):
            if len(character)!=1:
                raise RuntimeError('Invalid length: %u' % len(character))
            character = ord(character[0])
        elif not isinstance(character, int):
            raise RuntimeError('Invalid type: %s' % type(character))
        if character>=self.first and character<=self.last:
            return character
        return None

    def __str__(self):
        return 'name=%s styles=%s (id=%s) size=%u chars=0x%02x-0x%02x' % (self.name, list(self.styles), self.id, self.advance, self.first, self.last)

    def __repr__(self):
        return 'name=%s styles=%s (id=%s) size=%u characters=%02x-%02x (%u) hash=%08x' % (self.name, list(self.styles), self.id, self.advance, self.first, self.last, self.last - self.first, self.__hash__())

    def __hash__(self) -> int:
        return hash(self.bitmap.bitmap) + hash(self.glyphs.glyphs) + hash('%u,%u,%u' % (self.advance, self.first, self.last))

class Fonts(object):

    FONTS = ()  # type: Tuple[FontObject]

    def init():
        for idx, data in enumerate(font_data.FONT_LIST):
            if isinstance(data, tuple):
                font_data.FONT_LIST[idx] = FontObject(*data)
        Fonts.FONTS = tuple(font_data.FONT_LIST)

        FontData.read_cache()
        for dir in font_data.INCLUDE_HEADER_DIRS:
            if font_data.verbose:
                print('Scanning %s for *.h' % dir.rstrip('/\\*'))
            for file in glob.glob(dir, recursive=True):
                if fnmatch.fnmatch(file, '*.h') and path.isfile(file):
                    Fonts.read_font(file)

        FontData.write_cache()

    def _read_font_header(filename, contents):
        start = time.monotonic()
        bitmap_expr = 'const\s+uint8_t\s+(\w+)Bitmaps\s*\[\s*\].*?=.*?\{'
        m = re.search(bitmap_expr, contents, re.I|re.M|re.DOTALL)
        if not m:
            if font_data.verbose:
                print('cannot find bitmap')
            return
        fullname = m.group(1)
        font_id = fullname
        if fullname.endswith('7b') or fullname.endswith('8b'):
            font_id = font_id[0:-2]
        m_font_size = re.search(r'([_-]?)[1-9]{1,}[0-9]{0,}pt([_-]?)', font_id);
        font_size_pt = 0
        if m_font_size:
            try:
                font_size_pt = int(re.sub(r'[^\d]*', '', m_font_size.group(0)))
            except:
                font_size_pt = 0
            font_id = font_id[0:-len(m_font_size.group(0))]
        if font_data.verbose:
            print('fullname=%s font-name=%s size=%u' % (fullname, font_id, font_size_pt))

        if font_id in Fonts.FONTS:
            raise RuntimeError('%s already exists' % font_id)
        expr = bitmap_expr +  r'([^\}]+)\};'
        m_bitmap = re.search(expr, contents, re.I|re.M)
        if not m_bitmap:
            if font_data.verbose:
                print(expr)
            raise RuntimeError('bitmap incomplete')

        bitmap = eval('(' + m_bitmap.group(2) + ')')

        if font_data.verbose:
            print("bitmap size=%u" % len(bitmap))

        expr = r'const\s+GFXglyph\s+%sGlyphs\s*\[\s*\].*?=.*?(\{.+?\})\s*;' % re.escape(fullname)
        m_glyphs = re.search(expr, contents, re.I|re.M|re.DOTALL)
        if not m_glyphs:
            raise RuntimeError('glyphs not found')
        glyphs = m_glyphs.group(1)
        for comment in re.finditer(r'(//.*?)$', glyphs, re.I|re.M):
            glyphs = glyphs.replace(comment.group(1), '')
        glyphs = glyphs.replace('{', '(').replace('}', ')')
        glyphs = eval(glyphs);

        if font_data.verbose:
            print("glyphs size=%u" % len(glyphs))

        m_info = re.search(r'GFXglyph.{1,100}Glyphs\s*,\s*([^,]+?),([^,]+?),(.+?)\}', contents, re.I|re.M|re.DOTALL)
        first = eval(m_info.group(1).strip())
        last = eval(m_info.group(2).strip())
        advance = eval(m_info.group(3).strip())

        name = []
        for idx, part in enumerate(font_id.split('_')):
            if idx==0 or not re.match(r'(bold|italic|pixel|small)', part, re.I):
                name.append(part)
        name = ' '.join(name)

        if font_size_pt==0:
            font_size_pt = int(advance / 1.33)


        font_id += '_%upt' % font_size_pt

        styles = []
        tmp = path.basename(filename).lower() + ':' + font_id.lower()
        if 'mono' in tmp:
            styles.append(FontStyle.MONO)
        if 'bold' in tmp:
            styles.append(FontStyle.BOLD)
        if 'italic' in tmp:
            styles.append(FontStyle.ITALIC)
        if 'small' in tmp:
            styles.append(FontStyle.SMALL)
        if 'pixel' in tmp:
            styles.append(FontStyle.PIXEL)
        if 'lcd' in tmp:
            styles.append(FontStyle.LCD)


        if font_data.verbose:
            print("styles=%s first=0x%02x last=0x%02x advance=%u" % (FontStyles(styles), first, last, advance))


        filename, modified = FontData.update_cached_files(filename)

        FontData.FONT_DATA[font_id] = {
            'source': filename,
            'modified': modified,
            'createFontObject': (font_id, name, font_size_pt, str(FontStyles(styles)) or '[]', first, last, advance, 'FontData.FONT_DATA["%s"]["bitmap"]' % font_id, 'FontData.FONT_DATA["%s"]["glyphs"]' % font_id),
            'first': first,
            'last': last,
            'advance': advance,
            'bitmap': bitmap,
            'glyphs': glyphs,
        }

        list = Fonts.list()
        list[font_id] = FontObject(font_id, name, font_size_pt, styles, first, last, advance, bitmap, glyphs)
        Fonts.FONTS = tuple(list.values())

        # print('time %.3f / id %s' % ((time.monotonic() - start)*1000, font_id))

        # DejaVuSans_12pt8

    def read_font(filename):
        filename = path.abspath(filename)
        if fnmatch.fnmatch(filename, '*.h'):
            if FontData.is_cached(filename):
                if font_data.verbose:
                    print('%s: from cache' % filename)
                return
            if font_data.verbose:
                print('reading %s' % filename)
            with open(filename, 'rt', errors='ignore') as file:
                contents = file.read()
                try:
                    Fonts._read_font_header(filename, contents)
                except RuntimeError as e:
                    print('Warning: %s' % e)

        # else:
        #     with open(filename, 'rb') as file:
        #         pass

    def list(*arg):
        if not Fonts.FONTS:
            Fonts.init()
        fonts = {}
        for font in Fonts.FONTS:
            fonts[font.id] = font
        return fonts


class Font(object):

    def __init__(self, font=None, canvas=None):
        try:
            fonts = Fonts.list()
            if isinstance(font, str):
                # if font in fonts:
                self.font = fonts[font]
            elif isinstance(font, int):
                l = list(fonts.values())
                self.font = list(fonts.values())[font]
            elif isinstance(font, FontObject):
                self.font = font
            else:
                self.font = fonts[font]
        except Exception as e:
            raise RuntimeError('Invalid font %s: %u' % (isinstance(name_or_id, int) and 'id' or 'name', name_or_id))
        self.canvas = canvas # type: AbstractCanvas
        self.clear()

    @staticmethod
    def get_available_fonts(*arg):
        return Fonts.list(*arg)

    @property
    def width(self):
        return self.canvas.width

    @property
    def height(self):
        return self.canvas.height

    @property
    def color(self):
        return self.canvas.color

    @property
    def bgcolor(self):
        return self.canvas.bgcolor

    def clear(self):
        self.cursor_x = 0
        self.cursor_y = 0
        self.wrap = True
        self.textsize_x = 1;
        self.textsize_y = 1;

    def get_glyph(self, c):
        return self.font.glyphs.get(c)

    def draw_char(self, x, y, c, color, bg, size_x, size_y=None):
        size_y = size_y or size_x

        x = int(x)
        y = int(y)

        glyth = self.get_glyph(c)
        if glyth:
            bitmap = self.font.bitmap.get(glyth.bitmapOffset, glyth.next_offset)

            bits = bit = 0
            if size_x>1 or size_y>1:
                xo16 = int(glyth.xOffset)
                yo16 = int(glyth.yOffset)
            else:
                xo16 = yo16 = 0

            for yy in range(0, glyth.height):
                for xx in range(0, glyth.width):
                    if not (bit & 7):
                        bits = bitmap.pop(0)
                    bit += 1
                    if bits & 0x80:
                        if size_x==1 and size_y==1:
                            self.canvas.set_pixel(x + glyth.xOffset + xx, y + glyth.yOffset + yy, color)
                        else:
                            self.canvas.fill_rect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y, size_x, size_y, color)
                    bits <<= 1



        # startWrite();
        # for(yy=0; yy<h; yy++) {
        #     for(xx=0; xx<w; xx++) {
        #         if(!(bit++ & 7)) {
        #             bits = pgm_read_byte(&bitmap[bo++]);
        #         }
        #         if(bits & 0x80) {
        #             if(size_x == 1 && size_y == 1) {
        #                 writePixel(x+xo+xx, y+yo+yy, color);
        #             } else {
        #                 writeFillRect(x+(xo16+xx)*size_x, y+(yo16+yy)*size_y,
        #                   size_x, size_y, color);
        #             }
        #         }
        #         bits <<= 1;
        #     }
        # }
        # endWrite();

    def set_text_size(self, size_x=1, size_y=None):
        self.textsize_x = size_x
        self.textsize_x = size_y or size_x

    def print(self, x, y, text, color=None):
        if x!=None:
            self.cursor_x = x
        if y!=None:
            self.cursor_y = y
        if color!=None:
            self.canvas.color = color
        for c in text:
            self.write(c)

    def write(self, c):
        if c=='\n':
            self.cursor_x = 0
            self.cursor_y += self.textsize_y * self.font.yAdvance
        elif c!='\r':
            glyph = self.get_glyph(c)
            if glyph:
                if glyph.width>0 and glyph.height>0:
                    if self.wrap and (self.cursor_x + (self.textsize_x * (glyph.xOffset + glyph.width))) > self.width:
                        self.cursor_x = 0
                        self.cursor_y += self.textsize_y * self.font.yAdvance
                    self.draw_char(self.cursor_x, self.cursor_y, c, self.color, self.bgcolor, self.textsize_x, self.textsize_y);
                self.cursor_x += glyph.xAdvance * self.textsize_x
        return 1


# /**************************************************************************/
# /*!
#     @brief    Helper to determine size of a character with current font/size.
#        Broke this out as it's used by both the PROGMEM- and RAM-resident getTextBounds() functions.
#     @param    c     The ascii character in question
#     @param    x     Pointer to x location of character
#     @param    y     Pointer to y location of character
#     @param    minx  Minimum clipping value for X
#     @param    miny  Minimum clipping value for Y
#     @param    maxx  Maximum clipping value for X
#     @param    maxy  Maximum clipping value for Y
# */
# /**************************************************************************/
# void Adafruit_GFX::charBounds(char c, int16_t *x, int16_t *y,
#   int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy) {

#     if(gfxFont) {

#         if(c == '\n') { // Newline?
#             *x  = 0;    // Reset x to zero, advance y by one line
#             *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
#         } else if(c != '\r') { // Not a carriage return; is normal char
#             uint8_t first = pgm_read_byte(&gfxFont->first),
#                     last  = pgm_read_byte(&gfxFont->last);
#             if((c >= first) && (c <= last)) { // Char present in this font?
#                 GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c - first);
#                 uint8_t gw = pgm_read_byte(&glyph->width),
#                         gh = pgm_read_byte(&glyph->height),
#                         xa = pgm_read_byte(&glyph->xAdvance);
#                 int8_t  xo = pgm_read_byte(&glyph->xOffset),
#                         yo = pgm_read_byte(&glyph->yOffset);
#                 if(wrap && ((*x+(((int16_t)xo+gw)*textsize_x)) > _width)) {
#                     *x  = 0; // Reset x to zero, advance y by one line
#                     *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
#                 }
#                 int16_t tsx = (int16_t)textsize_x,
#                         tsy = (int16_t)textsize_y,
#                         x1 = *x + xo * tsx,
#                         y1 = *y + yo * tsy,
#                         x2 = x1 + gw * tsx - 1,
#                         y2 = y1 + gh * tsy - 1;
#                 if(x1 < *minx) *minx = x1;
#                 if(y1 < *miny) *miny = y1;
#                 if(x2 > *maxx) *maxx = x2;
#                 if(y2 > *maxy) *maxy = y2;
#                 *x += xa * tsx;
#             }
#         }

#     } else { // Default font

#         if(c == '\n') {                     // Newline?
#             *x  = 0;                        // Reset x to zero,
#             *y += textsize_y * 8;           // advance y one line
#             // min/max x/y unchaged -- that waits for next 'normal' character
#         } else if(c != '\r') {  // Normal char; ignore carriage returns
#             if(wrap && ((*x + textsize_x * 6) > _width)) { // Off right?
#                 *x  = 0;                    // Reset x to zero,
#                 *y += textsize_y * 8;       // advance y one line
#             }
#             int x2 = *x + textsize_x * 6 - 1, // Lower-right pixel of char
#                 y2 = *y + textsize_y * 8 - 1;
#             if(x2 > *maxx) *maxx = x2;      // Track max x, y
#             if(y2 > *maxy) *maxy = y2;
#             if(*x < *minx) *minx = *x;      // Track min x, y
#             if(*y < *miny) *miny = *y;
#             *x += textsize_x * 6;             // Advance x one char
#         }
#     }
# }

# /**************************************************************************/
# /*!
#     @brief    Helper to determine size of a string with current font/size. Pass string and a cursor position, returns UL corner and W,H.
#     @param    str     The ascii string to measure
#     @param    x       The current cursor X
#     @param    y       The current cursor Y
#     @param    x1      The boundary X coordinate, set by function
#     @param    y1      The boundary Y coordinate, set by function
#     @param    w      The boundary width, set by function
#     @param    h      The boundary height, set by function
# */
# /**************************************************************************/
# void Adafruit_GFX::getTextBounds(const char *str, int16_t x, int16_t y,
#         int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
#     uint8_t c; // Current character

#     *x1 = x;
#     *y1 = y;
#     *w  = *h = 0;

#     int16_t minx = _width, miny = _height, maxx = -1, maxy = -1;

#     while((c = *str++))
#         charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);

#     if(maxx >= minx) {
#         *x1 = minx;
#         *w  = maxx - minx + 1;
#     }
#     if(maxy >= miny) {
#         *y1 = miny;
#         *h  = maxy - miny + 1;
#     }
# }

# /**************************************************************************/
# /*!
#     @brief    Helper to determine size of a string with current font/size. Pass string and a cursor position, returns UL corner and W,H.
#     @param    str    The ascii string to measure (as an arduino String() class)
#     @param    x      The current cursor X
#     @param    y      The current cursor Y
#     @param    x1     The boundary X coordinate, set by function
#     @param    y1     The boundary Y coordinate, set by function
#     @param    w      The boundary width, set by function
#     @param    h      The boundary height, set by function
# */
# /**************************************************************************/
# void Adafruit_GFX::getTextBounds(const String &str, int16_t x, int16_t y,
#         int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
#     if (str.length() != 0) {
#         getTextBounds(const_cast<char*>(str.c_str()), x, y, x1, y1, w, h);
#     }
# }


# /**************************************************************************/
# /*!
#     @brief    Helper to determine size of a PROGMEM string with current font/size. Pass string and a cursor position, returns UL corner and W,H.
#     @param    str     The flash-memory ascii string to measure
#     @param    x       The current cursor X
#     @param    y       The current cursor Y
#     @param    x1      The boundary X coordinate, set by function
#     @param    y1      The boundary Y coordinate, set by function
#     @param    w      The boundary width, set by function
#     @param    h      The boundary height, set by function
# */
# /**************************************************************************/
# void Adafruit_GFX::getTextBounds(const __FlashStringHelper *str,
#         int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
#     uint8_t *s = (uint8_t *)str, c;

#     *x1 = x;
#     *y1 = y;
#     *w  = *h = 0;

#     int16_t minx = _width, miny = _height, maxx = -1, maxy = -1;

#     while((c = pgm_read_byte(s++)))
#         charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);

#     if(maxx >= minx) {
#         *x1 = minx;
#         *w  = maxx - minx + 1;
#     }
#     if(maxy >= miny) {
#         *y1 = miny;
#         *h  = maxy - miny + 1;
#     }
# }
