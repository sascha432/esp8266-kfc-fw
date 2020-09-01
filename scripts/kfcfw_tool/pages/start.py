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
        self.controller.config_dir = os.path.normpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../.config'))
        self.controller.config = {}
        self.config_filename = os.path.join(self.controller.config_dir, 'kfcfw_tool.json')

        self.read_config()

        label = tk.Label(self, text="Start Page - DISCONNECTED", font=self.LARGE_FONT)
        label.pack(pady=10, padx=10)
        self.controller.connect_label = label

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

        self.form = { 'hostname': tk.StringVar(), 'username': tk.StringVar(), 'password': tk.StringVar(), 'save_credentials': tk.IntVar() }
        self.form['save_credentials'].set(1)

        ttk.Label(self, text="Hostname or IP:").grid(in_=top, row=0, column=0, padx=pad, pady=pad)

        values = ['']
        for conn in self.config['connections']:
            name = conn['username'] + '@' + conn['hostname']
            values.append(name)

        self.hostname = ttk.Combobox(self, width=40, values=values, textvariable=self.form['hostname'])
        self.hostname.grid(in_=top, row=0, column=1, padx=pad, pady=pad)
        self.hostname.bind('<<ComboboxSelected>>', self.on_select)

        ttk.Label(self, text="Username:").grid(in_=top, row=1, column=0, padx=pad, pady=pad)
        ttk.Entry(self, width=43, textvariable=self.form['username']).grid(in_=top, row=1, column=1, padx=pad, pady=pad)

        ttk.Label(self, text="Password:").grid(in_=top, row=2, column=0, padx=pad, pady=pad)
        ttk.Entry(self, width=43, show='*', textvariable=self.form['password']).grid(in_=top, row=2, column=1, padx=pad, pady=pad)

        ttk.Checkbutton(self, text="Save credentials", variable=self.form['save_credentials']).grid(in_=top, row=3, column=0, padx=pad, pady=pad, columnspan=2)

        self.controller.connect_button = ttk.Button(self, text="Connect", command=self.connect)
        self.controller.connect_button.grid(in_=top, row=4, column=0, padx=pad, pady=pad, columnspan=2)

        if self.config['last_conn']!=None:
            self.hostname.current(self.config['last_conn'] + 1)
            self._on_select(self.config['last_conn'] + 1)

    def connect(self):
        if self.controller.wsc.is_connected():
            self.controller.wsc.disconnect()
            return

        cur = self.hostname.current()
        if cur==0:
            self.console.show_message(message='Enter new connection details or select existing connection')
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
                save = self.form['save_credentials'].get()
                if hostname=='':
                    raise ValueError('hostname')
                if username=='':
                    raise ValueError('username')
                if new_conn:
                    if password=='':
                        raise ValueError('password')
            except ValueError as e:
                self.console.show_message(message='Enter %s' % str(e))
            else:
                session = kfcfw.session.Session()
                if new_conn:
                    sid = session.generate(username, password)
                    if save==1:
                        self.config['connections'].append({'hostname': hostname, 'username': username, 'sid': sid})
                        self.config['last_conn'] = len(self.config['connections']) - 1
                        self.write_config()
                        self.hostname['values'] = (*self.hostname['values'], username + '@' + hostname)
                        self.hostname.current(self.config['last_conn'] + 1)
                        self.form['password'].set('')
                else:
                    if password!='':
                        sid = session.generate(username, password)
                        if save==1:
                            self.config['connections'][cur - 1]['sid'] = sid
                            self.config['last_conn'] = cur - 1
                            self.write_config()
                            self.form['password'].set('')
                    else:
                        sid = self.config['connections'][cur - 1]['sid']
                        self.config['last_conn'] = cur - 1
                        self.write_config()

                self.controller.wsc.connect(hostname, sid)
                self.controller.connect_button.configure(text='Connecting...');


    def on_select(self, event):
        self._on_select(event.widget.current())

    def _on_select(self, cur):
        if cur>0 and cur<=len(self.config['connections']):
            conn = self.config['connections'][cur - 1]
            self.config['last_conn'] = cur - 1
            self.form['username'].set(conn['username'])
            self.form['password'].set(self.password_placeholder)
            self.form['save_credentials'].set(1)
        else:
            self.form['username'].set('')
            self.form['password'].set('')

    def read_config(self):
        last_conn = 'None'
        try:
            with open(self.config_filename) as file:
                self.config = json.loads(file.read())
            self.console.debug('loaded config: %s' % self.config_filename)
            # verify last_conn is valid
            try:
                n = int(self.config['last_conn'])
                conn = self.config['connections'][n]
                if conn['hostname'] and conn['username']:
                    self.config['last_conn'] = n
                    last_conn = '%s@%s' % (conn['hostname'], conn['username'])
                else:
                    self.config['last_conn'] = None
            except:
                self.config['last_conn'] = None
        except Exception as e:
            self.console.error('Failed to read config: %s' % e)
            self.config = {
                'connections': [],
                'last_conn': None
            }
            last_conn = 'None'
        self.controller.set_config(self.config)
        self.console.debug('last connection: %s' % last_conn)

    def write_config(self):
        try:
            with open(self.config_filename, 'wt') as file:
                file.write(json.dumps(self.config, indent=2))
            self.console.debug('stored config: %s' % self.config_filename)
        except Exception as e:
            self.console.error('Failed to write config: %s' % e)
