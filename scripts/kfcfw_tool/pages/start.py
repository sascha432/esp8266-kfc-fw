#
# Author: sascha_lammers@gmx.de
#

from .base import PageBase
import tkinter
import tkinter as tk
from tkinter import ttk
import os
import json
from libs import kfcfw as kfcfw

# import matplotlib
# matplotlib.use("TkAgg")
# from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
# from matplotlib.figure import Figure
# import matplotlib.animation as animation
# import matplotlib.pyplot as plt
# from matplotlib import style
# import numpy as np
# from threading import Lock, Thread

class PageStart(tk.Frame, PageBase):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        PageBase.__init__(self, controller)

        self.password_placeholder = '********'
        self.config_filename = os.path.normpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../.config/kfcfw_tool.json'))
        self.read_config()

        label = tk.Label(self, text="Start Page - DISCONNECTED", font=self.LARGE_FONT)
        label.pack(pady=10,padx=10)
        self.controller.connect_label = label
        self.controller.connection_name = None

        menu = tk.Frame(self)
        menu.pack(side=tkinter.TOP)

        pad = 8
        button = ttk.Button(self, text="HLW8012 Energy Monitor", command=lambda: self.show_frame('PageEnergyMonitor'))
        button.pack(in_=menu, side=tkinter.LEFT, padx=pad)

        button = ttk.Button(self, text="MPR121 Touchpad", command=lambda: self.show_frame('PageTouchpad'))
        button.pack(in_=menu, side=tkinter.LEFT, padx=pad)

        button = ttk.Button(self, text="ADC", command=lambda: self.show_frame('PageADC'))
        button.pack(in_=menu, side=tkinter.LEFT, padx=pad)

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP, pady=20)
        pad = 2

        self.form = { 'hostname': tk.StringVar(), 'username': tk.StringVar(), 'password': tk.StringVar(), 'safe_credentials': tk.IntVar() }
        self.form['safe_credentials'].set(1)

        ttk.Label(self, text="Hostname or IP:").grid(in_=top, row=0, column=0, padx=pad, pady=pad)

        values = ['']
        n = 1
        select_hostname = None
        select_item = 0
        for conn in self.config['connections']:
            name = conn['username'] + '@' + conn['hostname']
            values.append(name)
            if 'last_conn' in self.config.keys() and self.config['last_conn']==n:
                select_hostname = name
                select_item = n
            n += 1

        self.hostname = ttk.Combobox(self, width=40, values=values, textvariable=self.form['hostname'])
        self.hostname.grid(in_=top, row=0, column=1, padx=pad, pady=pad)
        self.hostname.bind('<<ComboboxSelected>>', self.on_select)

        ttk.Label(self, text="Username:").grid(in_=top, row=1, column=0, padx=pad, pady=pad)
        ttk.Entry(self, width=43, textvariable=self.form['username']).grid(in_=top, row=1, column=1, padx=pad, pady=pad)

        ttk.Label(self, text="Password:").grid(in_=top, row=2, column=0, padx=pad, pady=pad)
        ttk.Entry(self, width=43, show='*', textvariable=self.form['password']).grid(in_=top, row=2, column=1, padx=pad, pady=pad)

        ttk.Checkbutton(self, text="Safe credentials", variable=self.form['safe_credentials']).grid(in_=top, row=3, column=0, padx=pad, pady=pad, columnspan=2)

        self.controller.connect_button = ttk.Button(self, text="Connect", command=self.connect)
        self.controller.connect_button.grid(in_=top, row=4, column=0, padx=pad, pady=pad, columnspan=2)

        if select_item:
            self.form['hostname'].set(select_hostname)
            self._on_select(select_item)

    def connect(self):
        if self.controller.wsc.is_connected():
            self.controller.wsc.disconnect()
            return

        cur = self.hostname.current()
        if cur==0:
            tk.messagebox.showerror(title='Error', message='Enter new connection details or select existing connection')
        else:
            try:
                new_conn = True
                username = self.form['username'].get().strip()
                password = self.form['password'].get()
                if cur==-1:
                    hostname = self.form['hostname'].get().strip()
                else:
                    hostname = self.config['connections'][cur - 1]['hostname']
                    if username==self.config['connections'][cur - 1]['username']:
                        new_conn = False
                if password==self.password_placeholder:
                    password = ''
                safe = self.form['safe_credentials'].get()
                if hostname=='':
                    raise Exception('hostname')
                if username=='':
                    raise Exception('username')
                if new_conn:
                    if password=='':
                        raise Exception('password')
            except Exception as e:
                tk.messagebox.showerror(title='Error', message='Enter ' + str(e))
            else:
                session = kfcfw.session.Session()
                if new_conn:
                    sid = session.generate(username, password)
                    if safe==1:
                        self.config['connections'].append({'hostname': hostname, 'username': username, 'sid': sid})
                        self.write_config()
                        self.hostname['values'] = (*self.hostname['values'], username + '@' + hostname)
                        self.hostname.current(len(self.config['connections']))
                        self.form['password'].set('')
                else:
                    if password!='':
                        sid = session.generate(username, password)
                        if safe==1:
                            self.config['connections'][cur - 1]['sid'] = sid
                            self.write_config()
                            self.form['password'].set('')
                    else:
                        sid = self.config['connections'][cur - 1]['sid']

                self.controller.connection_name =  username + '@' + hostname
                self.controller.wsc.connect(hostname, sid)


    def on_select(self, event):
        self._on_select(event.widget.current())

    def _on_select(self, cur):
        if cur>0 and cur<=len(self.config['connections']):
            conn = self.config['connections'][cur - 1]
            self.config['last_conn'] = cur
            self.form['username'].set(conn['username'])
            self.form['password'].set(self.password_placeholder)
            self.form['safe_credentials'].set(1)
        else:
            self.form['username'].set('')
            self.form['password'].set('')

    def read_config(self):
        try:
            with open(self.config_filename) as file:
                self.config = json.loads(file.read())
        except Exception as e:
            print('Failed to read config: %s' % e)
            self.config = { 'connections': [], 'last_conn': 0 }

    def write_config(self):
        try:
            with open(self.config_filename, 'wt') as file:
                file.write(json.dumps(self.config, indent=2))
        except:
            print('Failed to write: ' + self.config_filename)
