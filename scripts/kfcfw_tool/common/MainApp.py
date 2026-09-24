#
# Author: sascha_lammers@gmx.de
#

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

        self.config = {}

        self.touchpad = Touchpad(self)
        self.wsc = kfcfw.connection.WebSocket(self)

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

    def get_config(self):
        return self.config

    def set_config(self, config):
        self.config = config

    def write_config(self):
        self.frames['PageStart'].write_config()

    def get_connection_name(self):
        conn = ''
        try:
            conn = self.config['connections'][self.config['last_conn'] - 1]
            conn = '%s@%s' % (conn['username'], conn['hostname'])
        except Exception as e:
            self.console.error(e)
        return conn

    def connection_status(self, conn):
        kfcfw.connection.Controller.connection_status(self, conn)

        font = self.frames['PageStart'].LARGE_FONT
        title = ''
        if conn.is_authenticated():
            title = 'Connected to %s' % self.get_connection_name()
        elif conn.is_connected():
            title = 'Authenticating %s' % self.get_connection_name()
            self.connect_button.configure(text='Disconnect');
        elif not conn.is_connected():
            self.connect_button.configure(text='Connect');
            title = 'DISCONNECTED'
            if conn.has_error():
                font = self.frames['PageStart'].SMALL_FONT
                title += '\n%s' % conn.get_error()

        self.connect_label.config(text='Start Page - %s' % title, wraplength=840, font=font)

    def show_frame(self, name):
        if self.active:
            self.frames[self.active].set_active(False)
        frame = self.frames[name]
        self.active = name;
        frame.set_active(True)
        frame.tkraise()
