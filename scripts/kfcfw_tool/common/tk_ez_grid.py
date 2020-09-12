#
# Author: sascha_lammers@gmx.de
#

import tkinter
import tkinter as tk
from tkinter import ttk

class TkEZGrid(object):
    def __init__(self, frame, in_frame, padx=0, pady=0, sticky=None, direction='v'):
        self.frame = frame
        self.in_frame = in_frame
        self.direction = direction
        if self.direction=='v':
            self.row = -1
            self.column = 0
        else:
            self.row = 0
            self.column = -1
        self.padx = padx
        self.pady = pady
        self.sticky = sticky

    def _set_grid(self, obj, span, padx=None, pady=None):
        columnspan = 1
        rowspan = 1
        if self.direction=='v':
            n = self.column
            columnspan = span
        else:
            rowspan = span
            n = self.row
        if padx==None:
            padx = self.padx
            if isinstance(padx, list):
                padx = padx[n % len(padx)]
        if pady==None:
            pady = self.pady
            if isinstance(pady, list):
                pady = pady[n % len(pady)]
        obj.grid(in_=self.in_frame, row=self.row, column=self.column, padx=padx, pady=pady, sticky=self.sticky, columnspan=columnspan, rowspan=rowspan)

    def label(self, text):
        label = ttk.Label(self.frame, text=text)
        return self.first(label)

    def first(self, obj, span=1, padx=None, pady=None):
        if self.direction=='v':
            self.row += 1
            self.column = 0
        else:
            self.column += 1
            self.row = 0
        if obj:
            self._set_grid(obj, span, padx, pady)
        if self.direction=='v':
            self.column += span
        else:
            self.row += span
        return obj

    def next(self, obj, span=1, padx=None, pady=None):
        self._set_grid(obj, span, padx, pady)
        if self.direction=='v':
            self.column += span
        else:
            self.row += span
        return obj
