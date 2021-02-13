#
# Author: sascha_lammers@gmx.de
#

from . import *
import colorama
import sys
import os

colorama.init()

if os.name == 'nt':
    import msvcrt
    import ctypes

    class _CursorInfo(ctypes.Structure):
        _fields_ = [("size", ctypes.c_int),
                    ("visible", ctypes.c_byte)]

def hide_cursor():
    if os.name == 'nt':
        ci = _CursorInfo()
        handle = ctypes.windll.kernel32.GetStdHandle(-11)
        ctypes.windll.kernel32.GetConsoleCursorInfo(handle, ctypes.byref(ci))
        ci.visible = False
        ctypes.windll.kernel32.SetConsoleCursorInfo(handle, ctypes.byref(ci))
    elif os.name == 'posix':
        sys.stdout.write("\033[?25l")
        sys.stdout.flush()

def show_cursor():
    if os.name == 'nt':
        ci = _CursorInfo()
        handle = ctypes.windll.kernel32.GetStdHandle(-11)
        ctypes.windll.kernel32.GetConsoleCursorInfo(handle, ctypes.byref(ci))
        ci.visible = True
        ctypes.windll.kernel32.SetConsoleCursorInfo(handle, ctypes.byref(ci))
    elif os.name == 'posix':
        sys.stdout.write("\033[?25h")
        sys.stdout.flush()

class AbstractCanvas(object):

    def __init__(self, width, height):
        global Font
        from . import Font
        self._width = width
        self._height = height
        self._color = 0xffffff
        self._background = 0
        self._font = Font(0)

    @property
    def font(self):
        return self._font

    @font.setter
    def font(self, value):
        if not isinstance(value, Font):
            value = Font(value)
        self._font = value

    @property
    def width(self):
        return self._width

    @property
    def height(self):
        return self._height

    @property
    def color(self):
        return self._color

    @color.setter
    def color(self, value):
        self._color = value

    @property
    def bgcolor(self):
        return self._background

    @bgcolor.setter
    def bgcolor(self, value):
        self._background = value

    def print(self, x, y, text, color=None):
        if color!=None:
            _pcolor = self.color
            self.color = color
        self._font.print(x, y, text)
        if color!=None:
            self.color = _pcolor

    def clear(self):
        pass

    def set_pixel(self, x, y, color):
        pass

    def fill_rect(self, x, y, w, h, color):
        for xx in range(x, x + w + 1):
            for yy in range(y, y + h + 1):
                self.set_pixel(xx, yy, color)

class ConsoleCanvas(AbstractCanvas):

    def __init__(self, width, height, pixels = ' ', has_color=True):
        AbstractCanvas.__init__(self, width, height)
        self.pixels = pixels
        self.has_color = has_color
        self.clear()

    def cursor(self, state):
        if state:
            show_cursor()
        else:
            hide_cursor()

    def clear(self, flush=False):
        self.buffer = [self.bgcolor]*(self.width*self.height)
        if flush:
            print(colorama.ansi.clear_screen())
            self.flush()

    def set_pixel(self, x, y, color=None):
        if x>=self.width or y>=self.height or x<0 or y<0:
            return
        if color==None:
            color = self.color
        self.buffer[x + y * self.width] = color

    def get_pixel(self, x, y):
        if x>=self.width or y>=self.height or x<0 or y<0:
            return 0
        return self.buffer[x + y * self.width]

    def flush(self):
        print(\
            colorama.ansi.Cursor.POS() + \
            (self.has_color and '+' or '-') + \
            ('-'*(len(self.pixels)*self.width+1)))
        # print('\x1b[2J\x1b[0;0H' + ('-'*(len(self.pixels)*self.width+2)))
        color = -1
        for y in range(0, self.height):
            line = '|'
            for x in range(0, self.width):
                c = self.get_pixel(x, y)
                if color!=c:
                    color = c
                    if self.has_color:
                        line += colorama.ansi.CSI + '48;2;%u;%u;%um' % (c >> 16, (c >> 8) & 0xff, c & 0xff)
                if c:
                    line += self.pixels
                else:
                    line += len(self.pixels)*' '
            line += '|' + colorama.Style.RESET_ALL
            print(line)

        print(colorama.Style.RESET_ALL + ('-'*(len(self.pixels)*self.width+2)), flush=True, end='')
