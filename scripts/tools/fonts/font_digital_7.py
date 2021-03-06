#
# Author: sascha_lammers@gmx.de
#

# from . import fonts
from . import font_data, FontStyle

# typedef struct {
# 	uint16_t bitmapOffset;     ///< Pointer into GFXfont->bitmap
# 	uint8_t  width;            ///< Bitmap dimensions in pixels
#     uint8_t  height;           ///< Bitmap dimensions in pixels
# 	uint8_t  xAdvance;         ///< Distance to advance cursor (x axis)
# 	int8_t   xOffset;          ///< X dist from cursor pos to UL corner
#     int8_t   yOffset;          ///< Y dist from cursor pos to UL corner
# } GFXglyph;

class FontDigital7:
    FONT_DATA = {
        'digital-7_8pt': {
            'first': 0x20,
            'last': 0x7e,
            'advance': 9,
            'bitmap': (
                0xFB, 0x40, 0xB6, 0x80, 0x28, 0x53, 0x59, 0x42, 0x9F, 0xCA, 0x00, 0x20,
                0x0F, 0xE0, 0xA2, 0x80, 0x1E, 0x28, 0xA0, 0xBE, 0x00, 0x80, 0xE7, 0x4B,
                0xB0, 0x40, 0x84, 0x08, 0x37, 0x4B, 0x9C, 0xF9, 0x24, 0x80, 0x7E, 0x18,
                0x61, 0x87, 0xF0, 0x40, 0xE0, 0xF2, 0x41, 0x24, 0x9C, 0xC4, 0x90, 0x49,
                0x38, 0x25, 0x5C, 0xEA, 0x90, 0x21, 0x3E, 0x42, 0x00, 0xE0, 0xF8, 0x80,
                0x0C, 0x21, 0x84, 0x10, 0x04, 0x10, 0x82, 0x00, 0xFC, 0x63, 0x18, 0xC6,
                0x31, 0x8F, 0xC0, 0xFF, 0xC0, 0xF8, 0x42, 0x1F, 0xC2, 0x10, 0x87, 0x80,
                0xFC, 0x10, 0x41, 0x7C, 0x10, 0x41, 0x07, 0xF0, 0x0C, 0x63, 0x1F, 0x84,
                0x21, 0x08, 0x40, 0xF4, 0x21, 0x0F, 0x84, 0x21, 0x0F, 0xC0, 0xF4, 0x21,
                0x0F, 0xC6, 0x31, 0x8F, 0xC0, 0xFC, 0x63, 0x18, 0x84, 0x21, 0x08, 0x40,
                0xFC, 0x63, 0x1F, 0xC6, 0x31, 0x8F, 0xC0, 0xFC, 0x63, 0x1F, 0x84, 0x21,
                0x0F, 0xC0, 0x84, 0x81, 0xC0, 0x12, 0x44, 0x21, 0xF8, 0x01, 0xF0, 0xC6,
                0x33, 0x6C, 0xF8, 0x42, 0x1F, 0xC2, 0x10, 0x04, 0x00, 0xFD, 0x15, 0xD5,
                0x45, 0x15, 0x57, 0x41, 0xE0, 0xFC, 0x63, 0x1F, 0xC6, 0x31, 0x8C, 0x40,
                0xFC, 0x63, 0x1F, 0xC6, 0x31, 0x8F, 0xC0, 0xFC, 0x21, 0x08, 0x42, 0x10,
                0x87, 0xC0, 0xFC, 0x63, 0x18, 0x46, 0x31, 0x8F, 0xC0, 0xFC, 0x21, 0x0F,
                0xC2, 0x10, 0x87, 0xC0, 0xFC, 0x21, 0x0F, 0xC2, 0x10, 0x84, 0x00, 0xF4,
                0x21, 0x0B, 0xC6, 0x31, 0x8F, 0xC0, 0x8C, 0x63, 0x1F, 0xC6, 0x31, 0x8C,
                0x40, 0xFF, 0xC0, 0x08, 0x42, 0x10, 0xC6, 0x31, 0x8F, 0xC0, 0x8C, 0xA9,
                0x8F, 0xC6, 0x31, 0x8C, 0x40, 0x82, 0x08, 0x20, 0x82, 0x08, 0x20, 0x83,
                0xF0, 0xFD, 0x6B, 0x5A, 0xC6, 0x31, 0x8C, 0x40, 0x8E, 0x6B, 0x38, 0xC6,
                0x31, 0x8C, 0x40, 0xFC, 0x63, 0x10, 0x46, 0x31, 0x8F, 0xC0, 0xFC, 0x63,
                0x1F, 0xC2, 0x10, 0x84, 0x00, 0xFA, 0x28, 0xA2, 0x8A, 0x28, 0xA6, 0x9B,
                0xB0, 0xFC, 0x63, 0x1F, 0xC3, 0x14, 0x94, 0x40, 0xFC, 0x21, 0x0F, 0x84,
                0x21, 0x0F, 0xC0, 0xFC, 0x82, 0x08, 0x20, 0x82, 0x08, 0x20, 0x80, 0x8C,
                0x63, 0x18, 0xC6, 0x31, 0x8F, 0xC0, 0x8C, 0x63, 0x18, 0xC6, 0x2A, 0x50,
                0x00, 0x8C, 0x63, 0x18, 0xD6, 0xB5, 0xAF, 0xC0, 0xC6, 0x89, 0xB1, 0x42,
                0x85, 0x0A, 0x36, 0x45, 0x8C, 0x0C, 0x63, 0x1F, 0x84, 0x21, 0x0F, 0xC0,
                0xFC, 0x21, 0x84, 0x10, 0x84, 0x10, 0x83, 0xE0, 0xD2, 0x49, 0x24, 0x98,
                0x82, 0x04, 0x10, 0x40, 0x41, 0x06, 0x08, 0x30, 0xE4, 0x92, 0x49, 0x3C,
                0x01, 0x4C, 0xA1, 0xF8, 0x88, 0x80, 0xFC, 0x63, 0x1F, 0xC6, 0x31, 0x8C,
                0x40, 0xFC, 0x63, 0x1F, 0xC6, 0x31, 0x8F, 0xC0, 0xFC, 0x21, 0x08, 0x42,
                0x10, 0x87, 0xC0, 0xFC, 0x63, 0x18, 0x46, 0x31, 0x8F, 0xC0, 0xFC, 0x21,
                0x0F, 0xC2, 0x10, 0x87, 0xC0, 0xFC, 0x21, 0x0F, 0xC2, 0x10, 0x84, 0x00,
                0xF4, 0x21, 0x0B, 0xC6, 0x31, 0x8F, 0xC0, 0x8C, 0x63, 0x1F, 0xC6, 0x31,
                0x8C, 0x40, 0xFF, 0xC0, 0x08, 0x42, 0x10, 0xC6, 0x31, 0x8F, 0xC0, 0x8C,
                0xA9, 0x8F, 0xC6, 0x31, 0x8C, 0x40, 0x82, 0x08, 0x20, 0x82, 0x08, 0x20,
                0x83, 0xF0, 0xFD, 0x6B, 0x5A, 0xC6, 0x31, 0x8C, 0x40, 0x8E, 0x6B, 0x38,
                0xC6, 0x31, 0x8C, 0x40, 0xFC, 0x63, 0x10, 0x46, 0x31, 0x8F, 0xC0, 0xFC,
                0x63, 0x1F, 0xC2, 0x10, 0x84, 0x00, 0xFA, 0x28, 0xA2, 0x8A, 0x28, 0xA6,
                0x9B, 0xB0, 0xFC, 0x63, 0x1F, 0xC3, 0x14, 0x94, 0x40, 0xFC, 0x21, 0x0F,
                0x84, 0x21, 0x0F, 0xC0, 0xFC, 0x82, 0x08, 0x20, 0x82, 0x08, 0x20, 0x80,
                0x8C, 0x63, 0x18, 0xC6, 0x31, 0x8F, 0xC0, 0x8C, 0x63, 0x18, 0xC6, 0x2A,
                0x50, 0x00, 0x8C, 0x63, 0x18, 0xD6, 0xB5, 0xAF, 0xC0, 0xC6, 0x89, 0xB1,
                0x42, 0x85, 0x0A, 0x36, 0x45, 0x8C, 0x0C, 0x63, 0x1F, 0x84, 0x21, 0x0F,
                0xC0, 0xFC, 0x21, 0x84, 0x10, 0x84, 0x10, 0x83, 0xE0, 0x39, 0x08, 0x4E,
                0x10, 0x84, 0x21, 0xC0, 0xFF, 0xC0, 0xE1, 0x08, 0x43, 0x90, 0x84, 0x27,
                0x00, 0x21, 0xAC, 0x20),
            'glyphs': (
                (0,   0,   0,   7,    0,    1),   # 0x20 ' '
                (0,   1,  10,   7,    3,   -9),   # 0x21 '!'
                (2,   3,   3,   7,    2,   -9),   # 0x22 '"'
                (4,   7,   7,   7,    0,   -8),   # 0x23 '#'
                (11,   6,  14,   7,    1,  -11),   # 0x24 '$'
                (22,   7,  10,   7,    0,   -9),   # 0x25 '%'
                (31,   6,  11,   7,    1,   -9),   # 0x26 '&'
                (40,   1,   3,   7,    3,   -9),   # 0x27 '''
                (41,   3,  10,   7,    1,   -9),   # 0x28 '('
                (45,   3,  10,   7,    3,   -9),   # 0x29 ')'
                (49,   5,   6,   7,    1,   -7),   # 0x2A '*'
                (53,   5,   5,   7,    1,   -7),   # 0x2B '+'
                (57,   1,   3,   7,    3,    0),   # 0x2C ','
                (58,   5,   1,   7,    1,   -5),   # 0x2D '-'
                (59,   1,   1,   7,    3,    0),   # 0x2E '.'
                (60,   6,  10,   7,    1,   -9),   # 0x2F '/'
                (68,   5,  10,   7,    1,   -9),   # 0x30 '0'
                (75,   1,  10,   7,    5,   -9),   # 0x31 '1'
                (77,   5,  10,   7,    1,   -9),   # 0x32 '2'
                (84,   6,  10,   7,    0,   -9),   # 0x33 '3'
                (92,   5,  10,   7,    1,   -9),   # 0x34 '4'
                (99,   5,  10,   7,    1,   -9),   # 0x35 '5'
                (106,   5,  10,   7,    1,   -9),   # 0x36 '6'
                (113,   5,  10,   7,    1,   -9),   # 0x37 '7'
                (120,   5,  10,   7,    1,   -9),   # 0x38 '8'
                (127,   5,  10,   7,    1,   -9),   # 0x39 '9'
                (134,   1,   6,   7,    3,   -7),   # 0x3A ':'
                (135,   1,  10,   7,    3,   -7),   # 0x3B ';'
                (137,   4,   6,   7,    1,   -7),   # 0x3C '<'
                (140,   5,   4,   7,    1,   -6),   # 0x3D '='
                (143,   4,   6,   7,    1,   -7),   # 0x3E '>'
                (146,   5,  10,   7,    1,   -9),   # 0x3F '?'
                (153,   6,  10,   7,    0,   -9),   # 0x40 '@'
                (161,   5,  10,   7,    1,   -9),   # 0x41 'A'
                (168,   5,  10,   7,    1,   -9),   # 0x42 'B'
                (175,   5,  10,   7,    1,   -9),   # 0x43 'C'
                (182,   5,  10,   7,    1,   -9),   # 0x44 'D'
                (189,   5,  10,   7,    1,   -9),   # 0x45 'E'
                (196,   5,  10,   7,    1,   -9),   # 0x46 'F'
                (203,   5,  10,   7,    1,   -9),   # 0x47 'G'
                (210,   5,  10,   7,    1,   -9),   # 0x48 'H'
                (217,   1,  10,   7,    3,   -9),   # 0x49 'I'
                (219,   5,  10,   7,    1,   -9),   # 0x4A 'J'
                (226,   5,  10,   7,    1,   -9),   # 0x4B 'K'
                (233,   6,  10,   7,    1,   -9),   # 0x4C 'L'
                (241,   5,  10,   7,    1,   -9),   # 0x4D 'M'
                (248,   5,  10,   7,    1,   -9),   # 0x4E 'N'
                (255,   5,  10,   7,    1,   -9),   # 0x4F 'O'
                (262,   5,  10,   7,    1,   -9),   # 0x50 'P'
                (269,   6,  10,   7,    1,   -9),   # 0x51 'Q'
                (277,   5,  10,   7,    1,   -9),   # 0x52 'R'
                (284,   5,  10,   7,    1,   -9),   # 0x53 'S'
                (291,   6,  10,   7,    1,   -9),   # 0x54 'T'
                (299,   5,  10,   7,    1,   -9),   # 0x55 'U'
                (306,   5,  10,   7,    1,   -9),   # 0x56 'V'
                (313,   5,  10,   7,    1,   -9),   # 0x57 'W'
                (320,   7,  10,   7,    0,   -9),   # 0x58 'X'
                (329,   5,  10,   7,    1,   -9),   # 0x59 'Y'
                (336,   6,  10,   7,    1,   -9),   # 0x5A 'Z'
                (344,   3,  10,   7,    1,   -9),   # 0x5B '['
                (348,   6,  10,   7,    1,   -9),   # 0x5C '\'
                (356,   3,  10,   7,    3,   -9),   # 0x5D ']'
                (360,   6,   4,   7,    1,   -9),   # 0x5E '^'
                (363,   6,   1,   7,    1,    0),   # 0x5F '_'
                (364,   3,   3,   7,    2,  -10),   # 0x60 '`'
                (366,   5,  10,   7,    1,   -9),   # 0x61 'a'
                (373,   5,  10,   7,    1,   -9),   # 0x62 'b'
                (380,   5,  10,   7,    1,   -9),   # 0x63 'c'
                (387,   5,  10,   7,    1,   -9),   # 0x64 'd'
                (394,   5,  10,   7,    1,   -9),   # 0x65 'e'
                (401,   5,  10,   7,    1,   -9),   # 0x66 'f'
                (408,   5,  10,   7,    1,   -9),   # 0x67 'g'
                (415,   5,  10,   7,    1,   -9),   # 0x68 'h'
                (422,   1,  10,   7,    3,   -9),   # 0x69 'i'
                (424,   5,  10,   7,    1,   -9),   # 0x6A 'j'
                (431,   5,  10,   7,    1,   -9),   # 0x6B 'k'
                (438,   6,  10,   7,    1,   -9),   # 0x6C 'l'
                (446,   5,  10,   7,    1,   -9),   # 0x6D 'm'
                (453,   5,  10,   7,    1,   -9),   # 0x6E 'n'
                (460,   5,  10,   7,    1,   -9),   # 0x6F 'o'
                (467,   5,  10,   7,    1,   -9),   # 0x70 'p'
                (474,   6,  10,   7,    1,   -9),   # 0x71 'q'
                (482,   5,  10,   7,    1,   -9),   # 0x72 'r'
                (489,   5,  10,   7,    1,   -9),   # 0x73 's'
                (496,   6,  10,   7,    1,   -9),   # 0x74 't'
                (504,   5,  10,   7,    1,   -9),   # 0x75 'u'
                (511,   5,  10,   7,    1,   -9),   # 0x76 'v'
                (518,   5,  10,   7,    1,   -9),   # 0x77 'w'
                (525,   7,  10,   7,    0,   -9),   # 0x78 'x'
                (534,   5,  10,   7,    1,   -9),   # 0x79 'y'
                (541,   6,  10,   7,    1,   -9),   # 0x7A 'z'
                (549,   5,  10,   7,    0,   -9),   # 0x7B '('
                (556,   1,  10,   7,    1,   -9),   # 0x7C '|'
                (558,   5,  10,   7,    1,   -9),   # 0x7D ')'
                (565,   7,   3,   7,    0,   -7)),  # 0x7E '~'
        },
        'digital-7_9pt': {
            'first': 0x20,
            'last': 0x7e,
            'advance': 10,
            'bitmap': (
                0xF9, 0xD0, 0x99, 0x99, 0x24, 0x24, 0xDB, 0x24, 0x24, 0xDB, 0x24, 0x24,
                0x10, 0x07, 0xA4, 0x92, 0x48, 0x3F, 0x04, 0x51, 0x41, 0xFC, 0x11, 0x00,
                0xF2, 0x96, 0x94, 0xFC, 0x08, 0x08, 0x10, 0x10, 0x3F, 0x29, 0x69, 0x4F,
                0x7C, 0x89, 0x12, 0x20, 0x0F, 0xE0, 0xC1, 0x83, 0x03, 0xF8, 0x10, 0xF0,
                0xF8, 0x88, 0x80, 0x88, 0x88, 0x70, 0xF1, 0x11, 0x10, 0x11, 0x11, 0xE0,
                0x25, 0x7F, 0xFA, 0x90, 0x21, 0x3E, 0x42, 0x00, 0xE0, 0xFC, 0x80, 0x04,
                0x30, 0x86, 0x10, 0x42, 0x08, 0x61, 0x0C, 0x20, 0xFE, 0x18, 0x61, 0x84,
                0x08, 0x61, 0x86, 0x1F, 0xC0, 0xF9, 0xF0, 0xFC, 0x10, 0x41, 0x07, 0xF8,
                0x20, 0x82, 0x0F, 0x80, 0xFC, 0x10, 0x41, 0x07, 0xF0, 0x41, 0x04, 0x1F,
                0xC0, 0x06, 0x18, 0x61, 0x84, 0x0F, 0xC1, 0x04, 0x10, 0x41, 0xFA, 0x08,
                0x20, 0x83, 0xF0, 0x41, 0x04, 0x1F, 0xC0, 0xFA, 0x08, 0x20, 0x83, 0xF8,
                0x61, 0x86, 0x1F, 0xC0, 0xFE, 0x18, 0x61, 0x84, 0x00, 0x41, 0x04, 0x10,
                0x40, 0xFE, 0x18, 0x61, 0x87, 0xF8, 0x61, 0x86, 0x1F, 0xC0, 0xFE, 0x18,
                0x61, 0x87, 0xF0, 0x41, 0x04, 0x1F, 0xC0, 0x84, 0x80, 0xE0, 0x36, 0xC0,
                0xC6, 0x30, 0xFC, 0x00, 0x3F, 0x8C, 0x60, 0x6C, 0x80, 0xFC, 0x10, 0x41,
                0x07, 0xF8, 0x20, 0x00, 0x08, 0x00, 0xFE, 0x1B, 0xE9, 0xA4, 0x8A, 0x6F,
                0x82, 0x0F, 0x80, 0xFE, 0x18, 0x61, 0x87, 0xF8, 0x61, 0x86, 0x18, 0x40,
                0xFE, 0x18, 0x61, 0x87, 0xF8, 0x61, 0x86, 0x1F, 0x80, 0xFA, 0x08, 0x20,
                0x80, 0x08, 0x20, 0x82, 0x0F, 0xC0, 0xFE, 0x18, 0x61, 0x84, 0x08, 0x61,
                0x86, 0x1F, 0x80, 0xFA, 0x08, 0x20, 0x83, 0xF8, 0x20, 0x82, 0x0F, 0xC0,
                0xFA, 0x08, 0x20, 0x83, 0xF8, 0x20, 0x82, 0x08, 0x00, 0xFA, 0x08, 0x20,
                0x80, 0x78, 0x61, 0x86, 0x1F, 0xC0, 0x06, 0x18, 0x61, 0x86, 0x0F, 0xE1,
                0x86, 0x18, 0x61, 0xF9, 0xF0, 0x04, 0x10, 0x41, 0x04, 0x00, 0x21, 0x86,
                0x18, 0x7F, 0x8E, 0x6B, 0x38, 0xC0, 0x0F, 0xE1, 0x86, 0x18, 0x61, 0x82,
                0x08, 0x20, 0x80, 0x00, 0x20, 0x82, 0x08, 0x3F, 0xFF, 0x26, 0x4C, 0x99,
                0x20, 0x20, 0xC1, 0x83, 0x06, 0x08, 0x87, 0x9B, 0x67, 0x84, 0x08, 0x61,
                0x86, 0x18, 0x40, 0xFE, 0x18, 0x61, 0x84, 0x08, 0x61, 0x86, 0x17, 0x80,
                0xFE, 0x18, 0x61, 0x87, 0xF8, 0x20, 0x82, 0x08, 0x00, 0xFD, 0x0A, 0x14,
                0x28, 0x40, 0x21, 0x42, 0x9D, 0x1B, 0xD8, 0xFE, 0x18, 0x61, 0x87, 0xFC,
                0x38, 0xB2, 0x68, 0xC0, 0xFE, 0x08, 0x20, 0x83, 0xF0, 0x41, 0x04, 0x1F,
                0xC0, 0xFC, 0x82, 0x08, 0x20, 0x82, 0x08, 0x20, 0x82, 0x00, 0x06, 0x18,
                0x61, 0x86, 0x00, 0x21, 0x86, 0x18, 0x7F, 0x86, 0x18, 0x61, 0x84, 0x08,
                0x73, 0x48, 0xC0, 0x00, 0x83, 0x06, 0x0C, 0x18, 0x20, 0x04, 0x49, 0x93,
                0x26, 0x0F, 0xF0, 0x83, 0x8D, 0x13, 0x62, 0x85, 0x0A, 0x14, 0x6C, 0x8B,
                0x1C, 0x10, 0x06, 0x18, 0x61, 0x86, 0x0F, 0xC1, 0x04, 0x10, 0x7F, 0xF4,
                0x30, 0x82, 0x18, 0xC2, 0x18, 0x43, 0x0B, 0x80, 0xF2, 0x48, 0x24, 0x93,
                0x80, 0x83, 0x04, 0x18, 0x20, 0x81, 0x04, 0x18, 0x20, 0xC1, 0xE4, 0x92,
                0x01, 0x24, 0xF0, 0x30, 0xC4, 0xB3, 0x84, 0xFC, 0xB4, 0xFE, 0x18, 0x61,
                0x87, 0xF8, 0x61, 0x86, 0x18, 0x40, 0xFE, 0x18, 0x61, 0x87, 0xF8, 0x61,
                0x86, 0x1F, 0x80, 0xFA, 0x08, 0x20, 0x80, 0x08, 0x20, 0x82, 0x0F, 0xC0,
                0xFE, 0x18, 0x61, 0x84, 0x08, 0x61, 0x86, 0x1F, 0x80, 0xFA, 0x08, 0x20,
                0x83, 0xF8, 0x20, 0x82, 0x0F, 0xC0, 0xFA, 0x08, 0x20, 0x83, 0xF8, 0x20,
                0x82, 0x08, 0x00, 0xFA, 0x08, 0x20, 0x80, 0x78, 0x61, 0x86, 0x1F, 0xC0,
                0x06, 0x18, 0x61, 0x86, 0x0F, 0xE1, 0x86, 0x18, 0x61, 0xF9, 0xF0, 0x04,
                0x10, 0x41, 0x04, 0x00, 0x21, 0x86, 0x18, 0x7F, 0x8E, 0x6B, 0x38, 0xC0,
                0x0F, 0xE1, 0x86, 0x18, 0x61, 0x82, 0x08, 0x20, 0x80, 0x00, 0x20, 0x82,
                0x08, 0x3F, 0xFF, 0x26, 0x4C, 0x99, 0x20, 0x20, 0xC1, 0x83, 0x06, 0x08,
                0x87, 0x9B, 0x67, 0x84, 0x08, 0x61, 0x86, 0x18, 0x40, 0xFE, 0x18, 0x61,
                0x84, 0x08, 0x61, 0x86, 0x17, 0x80, 0xFE, 0x18, 0x61, 0x87, 0xF8, 0x20,
                0x82, 0x08, 0x00, 0xFD, 0x0A, 0x14, 0x28, 0x40, 0x21, 0x42, 0x9D, 0x1B,
                0xD8, 0xFE, 0x18, 0x61, 0x87, 0xFC, 0x38, 0xB2, 0x68, 0xC0, 0xFE, 0x08,
                0x20, 0x83, 0xF0, 0x41, 0x04, 0x1F, 0xC0, 0xFC, 0x82, 0x08, 0x20, 0x82,
                0x08, 0x20, 0x82, 0x00, 0x06, 0x18, 0x61, 0x86, 0x00, 0x21, 0x86, 0x18,
                0x7F, 0x86, 0x18, 0x61, 0x84, 0x08, 0x73, 0x48, 0xC0, 0x00, 0x83, 0x06,
                0x0C, 0x18, 0x20, 0x04, 0x49, 0x93, 0x26, 0x0F, 0xF0, 0x83, 0x8D, 0x13,
                0x62, 0x85, 0x0A, 0x14, 0x6C, 0x8B, 0x1C, 0x10, 0x06, 0x18, 0x61, 0x86,
                0x0F, 0xC1, 0x04, 0x10, 0x7F, 0xF4, 0x30, 0x82, 0x18, 0xC2, 0x18, 0x43,
                0x0B, 0x80, 0x38, 0x41, 0x04, 0x13, 0x81, 0x04, 0x10, 0x43, 0x80, 0xFF,
                0xF0, 0x70, 0x82, 0x08, 0x20, 0x72, 0x08, 0x20, 0x87, 0x00, 0x30, 0xDB,
                0x00, 0x0C),
            'glyphs': (
                (0,   0,   0,   8,    0,    1),   # 0x20 ' '
                (0,   1,  12,   8,    3,  -11),   # 0x21 '!'
                (2,   4,   4,   8,    2,  -11),   # 0x22 '"'
                (4,   8,   8,   8,    0,   -9),   # 0x23 '#'
                (12,   6,  15,   8,    1,  -12),   # 0x24 '$'
                (24,   8,  12,   9,    0,  -11),   # 0x25 '%'
                (36,   7,  12,   9,    1,  -10),   # 0x26 '&'
                (47,   1,   4,   8,    4,  -11),   # 0x27 '''
                (48,   4,  11,   8,    1,  -10),   # 0x28 '('
                (54,   4,  11,   8,    3,  -10),   # 0x29 ')'
                (60,   5,   6,   8,    1,   -8),   # 0x2A '*'
                (64,   5,   5,   8,    1,   -7),   # 0x2B '+'
                (68,   1,   3,   8,    3,    0),   # 0x2C ','
                (69,   6,   1,   8,    1,   -5),   # 0x2D '-'
                (70,   1,   1,   8,    3,    0),   # 0x2E '.'
                (71,   6,  12,   8,    1,  -11),   # 0x2F '/'
                (80,   6,  11,   8,    1,  -10),   # 0x30 '0'
                (89,   1,  12,   8,    6,  -11),   # 0x31 '1'
                (91,   6,  11,   8,    1,  -10),   # 0x32 '2'
                (100,   6,  11,   8,    1,  -10),   # 0x33 '3'
                (109,   6,  12,   8,    1,  -11),   # 0x34 '4'
                (118,   6,  11,   8,    1,  -10),   # 0x35 '5'
                (127,   6,  11,   8,    1,  -10),   # 0x36 '6'
                (136,   6,  11,   8,    1,  -10),   # 0x37 '7'
                (145,   6,  11,   8,    1,  -10),   # 0x38 '8'
                (154,   6,  11,   8,    1,  -10),   # 0x39 '9'
                (163,   1,   6,   8,    3,   -8),   # 0x3A ':'
                (164,   1,  11,   8,    3,   -8),   # 0x3B ';'
                (166,   4,   7,   8,    2,   -8),   # 0x3C '<'
                (170,   6,   4,   8,    1,   -7),   # 0x3D '='
                (173,   4,   7,   8,    2,   -8),   # 0x3E '>'
                (177,   6,  11,   8,    1,  -10),   # 0x3F '?'
                (186,   6,  11,   8,    1,  -10),   # 0x40 '@'
                (195,   6,  11,   8,    1,  -10),   # 0x41 'A'
                (204,   6,  11,   8,    1,  -10),   # 0x42 'B'
                (213,   6,  11,   8,    1,  -10),   # 0x43 'C'
                (222,   6,  11,   8,    1,  -10),   # 0x44 'D'
                (231,   6,  11,   8,    1,  -10),   # 0x45 'E'
                (240,   6,  11,   8,    1,  -10),   # 0x46 'F'
                (249,   6,  11,   8,    1,  -10),   # 0x47 'G'
                (258,   6,  12,   8,    1,  -11),   # 0x48 'H'
                (267,   1,  12,   8,    3,  -11),   # 0x49 'I'
                (269,   6,  12,   8,    1,  -11),   # 0x4A 'J'
                (278,   6,  12,   8,    1,  -11),   # 0x4B 'K'
                (287,   6,  12,   8,    1,  -11),   # 0x4C 'L'
                (296,   7,  11,   9,    1,  -10),   # 0x4D 'M'
                (306,   6,  11,   8,    1,  -10),   # 0x4E 'N'
                (315,   6,  11,   8,    1,  -10),   # 0x4F 'O'
                (324,   6,  11,   8,    1,  -10),   # 0x50 'P'
                (333,   7,  11,   8,    1,  -10),   # 0x51 'Q'
                (343,   6,  11,   8,    1,  -10),   # 0x52 'R'
                (352,   6,  11,   8,    1,  -10),   # 0x53 'S'
                (361,   6,  11,   8,    1,  -10),   # 0x54 'T'
                (370,   6,  12,   8,    1,  -11),   # 0x55 'U'
                (379,   6,  11,   8,    1,  -10),   # 0x56 'V'
                (388,   7,  12,   9,    1,  -11),   # 0x57 'W'
                (399,   7,  12,   8,    1,  -11),   # 0x58 'X'
                (410,   6,  12,   8,    1,  -11),   # 0x59 'Y'
                (419,   6,  11,   8,    1,  -10),   # 0x5A 'Z'
                (428,   3,  11,   8,    1,  -10),   # 0x5B '['
                (433,   6,  12,   8,    1,  -11),   # 0x5C '\'
                (442,   3,  12,   8,    4,  -11),   # 0x5D ']'
                (447,   6,   5,   8,    1,  -11),   # 0x5E '^'
                (451,   6,   1,   8,    1,    0),   # 0x5F '_'
                (452,   2,   3,   8,    3,  -11),   # 0x60 '`'
                (453,   6,  11,   8,    1,  -10),   # 0x61 'a'
                (462,   6,  11,   8,    1,  -10),   # 0x62 'b'
                (471,   6,  11,   8,    1,  -10),   # 0x63 'c'
                (480,   6,  11,   8,    1,  -10),   # 0x64 'd'
                (489,   6,  11,   8,    1,  -10),   # 0x65 'e'
                (498,   6,  11,   8,    1,  -10),   # 0x66 'f'
                (507,   6,  11,   8,    1,  -10),   # 0x67 'g'
                (516,   6,  12,   8,    1,  -11),   # 0x68 'h'
                (525,   1,  12,   8,    3,  -11),   # 0x69 'i'
                (527,   6,  12,   8,    1,  -11),   # 0x6A 'j'
                (536,   6,  12,   8,    1,  -11),   # 0x6B 'k'
                (545,   6,  12,   8,    1,  -11),   # 0x6C 'l'
                (554,   7,  11,   9,    1,  -10),   # 0x6D 'm'
                (564,   6,  11,   8,    1,  -10),   # 0x6E 'n'
                (573,   6,  11,   8,    1,  -10),   # 0x6F 'o'
                (582,   6,  11,   8,    1,  -10),   # 0x70 'p'
                (591,   7,  11,   8,    1,  -10),   # 0x71 'q'
                (601,   6,  11,   8,    1,  -10),   # 0x72 'r'
                (610,   6,  11,   8,    1,  -10),   # 0x73 's'
                (619,   6,  11,   8,    1,  -10),   # 0x74 't'
                (628,   6,  12,   8,    1,  -11),   # 0x75 'u'
                (637,   6,  11,   8,    1,  -10),   # 0x76 'v'
                (646,   7,  12,   9,    1,  -11),   # 0x77 'w'
                (657,   7,  12,   8,    1,  -11),   # 0x78 'x'
                (668,   6,  12,   8,    1,  -11),   # 0x79 'y'
                (677,   6,  11,   8,    1,  -10),   # 0x7A 'z'
                (686,   6,  11,   8,    0,  -10),   # 0x7B '('
                (695,   1,  12,   8,    1,  -11),   # 0x7C '|'
                (697,   6,  11,   8,    1,  -10),   # 0x7D ')'
                (706,   8,   4,   8,    0,   -8)),  # 0x7E '~'
        }
    }

font_data.FONT_DATA.update(FontDigital7.FONT_DATA)
font_data.FONT_LIST.append(('digital-7_8pt', 'Digital-7', 8, FontStyle.MONO))
font_data.FONT_LIST.append(('digital-7_9pt', 'Digital-7', 9, FontStyle.MONO))


