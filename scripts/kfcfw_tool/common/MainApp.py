#
# Author: sascha_lammers@gmx.de
#

from common.Plot import Plot
from common.Touchpad import Touchpad
import tkinter
import tkinter as tk
from tkinter import ttk
from threading import Lock, Thread
from libs import kfcfw as kfcfw
import importlib
import pages

import sys

class MainApp(tk.Tk, kfcfw.connection.Controller):

    def __init__(self, *args, **kwargs):

        tk.Tk.__init__(self, *args, **kwargs)
        kfcfw.connection.Controller.__init__(self, True)

        self.gui_settings = {
            'y_range': 0,
            'convert_units': tk.IntVar(),
            'noise': tk.IntVar(),
            'retention': tk.StringVar(),
            'compress': tk.StringVar(),
            'plot_type': '',
            'data_state': [ True, True, True, True ]
        }
        self.gui_settings['convert_units'].set(1)
        self.gui_settings['noise'].set(0)
        self.gui_settings['retention'].set('60')
        self.gui_settings['compress'].set('0.1')

        self.connection_name = None
        self.plot = Plot(self)
        self.touchpad = Touchpad(self)
        self.wsc = kfcfw.connection.WebSocket(self)
        self.set_plot_type('')

        tk.Tk.wm_title(self, "KFCFirmware Debug Tool")

        container = tk.Frame(self)
        container.pack(side="top", fill="both", expand = True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)

        self.frames = {}
        self.active = False

        module = importlib.import_module('pages')
        for frame in pages.base.PageBase.classes:
            inst = getattr(module, frame)(container, self)
            inst.grid(row=0, column=0, sticky="nsew")
            self.frames[frame] = inst

        self.show_frame(pages.base.PageBase.classes[0])

    def connection_status(self, conn):
        kfcfw.connection.Controller.connection_status(self, conn)

        title = ''
        if conn.is_authenticated():
            title = 'Connected to %s' % self.connection_name
        elif conn.is_connected():
            title = 'Authenticating %s' % self.connection_name
            self.connect_button.configure(text='Disconnect');
        elif not conn.is_connected():
            self.connect_button.configure(text='Connect');
            title = 'DISCONNECTED'
            if conn.has_error():
                title += ': %s' % conn.get_error()

        self.connect_label.config(text='Start Page - ' + title)
        self.set_plot_title()

    def set_plot_title(self):
        if not self.wsc.is_connected():
            title = 'Not connected'
        else:
            type = self.gui_settings['plot_type']
            if type=='I':
                title='Current'
            elif type=='P':
                title='Power'
            elif type=='U':
                title='Voltage'
            else:
                title='Paused'
        self.plot.title = title

    def toggle_data_state(self, num):
        self.gui_settings['data_state'][num] = not self.gui_settings['data_state'][num]

    def get_data_state(self, num):
        return self.gui_settings['data_state'][num]

    def set_y_range(self, val):
        self.gui_settings['y_range'] = val

    def get_y_range(self):
        return float(self.gui_settings['y_range']);

    def get_update_rate(self):
        return 250

    def set_plot_type(self, type):
        self.gui_settings['plot_type'] = type;

        if self.gui_settings['convert_units'].get()==1:
            convert_units = 'true'
        else:
            convert_units = '0'

        if type == 'U':
            self.wsc.send_cmd('SP_HLWMODE', 'u'); #, '500');
            self.wsc.send_cmd('SP_HLWPLOT', self.wsc.client_id, 'u', convert_units);
        elif type == 'I':
            self.wsc.send_cmd('SP_HLWMODE', 'i'); #, '1250');
            self.wsc.send_cmd('SP_HLWPLOT', self.wsc.client_id, 'i', convert_units);
        elif type == 'P':
            self.wsc.send_cmd('SP_HLWMODE', 'p');
            self.wsc.send_cmd('SP_HLWPLOT', self.wsc.client_id, 'p', convert_units);
        else:
            self.wsc.send_cmd('SP_HLWMODE', 'c');
            self.wsc.send_cmd('SP_HLWPLOT', self.wsc.client_id, '0')

        self.set_plot_title()

        if type=='':
            self.plot.plot_type = ''
            self.update_plot = False
        else:
            self.plot.init_plot(type)
            self.update_plot = True

    def show_frame(self, name):
        if self.active:
            self.frames[self.active].set_active(False)
        frame = self.frames[name]
        self.active = name;
        frame.set_active(True)
        frame.tkraise()

