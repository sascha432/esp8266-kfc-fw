#
# Author: sascha_lammers@gmx.de
#

from pprint import pprint
import tkinter
import tkinter as tk
from tkinter import ttk

import matplotlib
matplotlib.use("TkAgg")

from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure

import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib import style

from statistics import mean

import os
import websocket
import _thread as thread
import json
import time

import kfcfw_session

# import serial
# import atexit

LARGE_FONT= ("Verdana", 16)

def var_dump(o):
    pprint(vars(o))

class WSConsole:

    def __init__(self, controller):
        self.controller = controller
        self.set_disconnected()

    def set_disconnected(self):
        self.client_id = '???'
        self.auth = False
        self.connected = False

    def send_cmd(self, command, *args):
        if self.connected:
            sep = ','
            cmd_str = '+' + command + '=' + sep.join(args)
            print('Sending cmd: ' + cmd_str)
            self.ws.send(cmd_str + '\n')
            return True
        return False

    def on_message(self, data):
        if isinstance(data, bytes):
            print("on_message: bytes " + str(len(data)))
        elif isinstance(data, bytearray):
            print("on_message: bytearray " + str(len(data)))
        else:
            lines = data.split("\n")
            for message in lines:
                if message != "":
                    # print(message)
                    if not self.auth:
                        if message == "+AUTH_OK":
                            self.auth = True
                            self.send_cmd('H2SBUFDLY', '500')
                            self.send_cmd('H2SBUFSZ', '256')
                        elif message == "+AUTH_ERROR":
                            self.on_error('Authentication failed')
                        elif message == "+REQ_AUTH":
                            print("Sending authentication " + self.sid)
                            self.ws.send("+SID " + self.sid)
                    else:
                        if message[0:11] == "+CLIENT_ID=":
                            self.client_id = message[11:]
                            print("ClientID: " + self.client_id)
                        elif message[0:7] == "+PLOTM=":
                            self.controller.plot.add_marker(message[7:])
                        elif message[0:6] == "+PLOT=":
                            try:
                                data = json.loads(message[6:])
                            except:
                                print("Failed to load JSON")
                                print(message[6:])
                            else:
                                self.controller.data_handler(data)

    def on_error(self, error):
        tk.messagebox.showerror(title='Error', message='Connection failed: ' + str(error))
        self.set_disconnected()
        self.controller.set_connection_status(False, str(error))
        try:
            self.ws.close()
        except:
            self.connected = False

    def on_close(self):
        if self.connected:
            self.set_disconnected()
            tk.messagebox.showerror(title='Error', message='Connection closed')
        self.controller.set_connection_status(False)

    def on_open(self):
        self.connected = True
        self.controller.set_connection_status(True)
        print("Web Socket connected")

    def url(self):
        return 'ws://' + self.hostname + '/serial_console'

    def connect(self, hostname, sid):
        if self.connected:
            self.set_disconnected()
            try:
                self.ws.close()
            except:
                self.connected = False

        self.hostname = hostname
        self.sid = sid

        # websocket.enableTrace(True)

        self.ws = websocket.WebSocketApp(self.url(),
            on_message = self.on_message,
            on_error = self.on_error,
            on_close = self.on_close,
            on_open = self.on_open)

        def run(*args):
            self.ws.run_forever()
        thread.start_new_thread(run, ())

    def close(self):
        if self.connected:
            self.connected = False
            self.ws.close()
        self.set_disconnected()
        self.controller.set_connection_status(False)

class Plot:

    def __init__(self, controller):
        self.controller = controller
        self.values = []
        self.markers = []
        self.start_time = 0
        self.update_plot = False
        self.label = { 'x': 'time in seconds', 'y': '' }

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.ax1 = self.fig.add_subplot(111)
        self.ax2 = False

    def init_animation(self):
        self.ani = animation.FuncAnimation(self.fig, self.plot_values, interval=self.controller.get_update_rate())

    def get_max_value(self):
        return max([max(self.values[1][1:]), max(self.values[2][1:]), max(self.values[3][1:]), max(self.values[4][1:])])

    def init_plot(self, type):
        self.stats = [ '', '', '', '' ]
        self.levels = []
        self.markers = []
        self.values = [[], [], [], [], [], [], [], ]
        self.start_time = 0

        self.fig.clear()
        if self.controller.gui_settings['noise'].get()==1:
            (self.ax1, self.ax2) = self.fig.subplots(2, 1)
        else:
            self.ax1 = self.fig.subplots(1, 1)
            self.ax2 = False

        self.controller.update_plot_title()

    def plot_values(self, i):
        if self.update_plot or i == -1:
            self.update_plot = False
            self.ax1.clear()

            self.ax1.set_xlabel(self.label['x'])
            self.ax1.set_ylabel(self.label['y'])

            try:
                levels = self.levels
                fmt = '{0:.2f}V {1:.3f}A {2:.2f}W pf {3:.2f} {4:.3f}kWh noise {5:.3f}'
                self.fig.suptitle(fmt.format(levels[0], levels[1], levels[2], levels[4], levels[3], levels[5] / 1000.0), fontsize=16)
            except:
                dummy = 0

            try:
                stats = []
                n = max(self.values[3]) / 10
                digits = 2
                while n<1 and digits<4:
                    n = n * 10
                    digits = digits + 1
                fmt = '.' + str(digits) + 'f'
                for i in range(1, 5):
                    tmp = ' min/max ' + format(min(self.values[i]), fmt) + '/' + format(max(self.values[i]), fmt) + ' ' + u"\u2300" + ' ' + format(mean(self.values[i]), fmt)
                    stats.append(tmp)
                self.stats = stats
            except:
                dummy = 0

            # noise = ''
            # try:
            #     _min = min(self.values[1][-5:])
            #     _max = max(self.values[1][-5:])
            #     _mean = mean(self.values[1][-5:])
            #     noise = ((_mean - _min) + (_max - _mean)) * 50 / _mean
            #     noise = format(noise, '.2f')
            # except:
            #     dummy = 0

            if self.controller.get_data_state(0):
                self.ax1.plot(self.values[0], self.values[1], 'g', label='diff ' + self.stats[0], linewidth=0.1)
            if self.controller.get_data_state(1):
                self.ax1.plot(self.values[0], self.values[2], 'b', label='avg' + self.stats[1])
            if self.controller.get_data_state(2):
                self.ax1.plot(self.values[0], self.values[3], 'r', label='integral' + self.stats[2])
            if self.controller.get_data_state(3):
                self.ax1.plot(self.values[0], self.values[4], 'c', label='display' + self.stats[3])
            if len(self.values[0])>1:
                y_range_limit = self.get_max_value() * self.controller.get_y_range() / 100.0
                self.ax1.plot(self.values[0][0], y_range_limit)

            for marker in self.markers:
                if marker!=0:
                    self.ax1.axvline(self.get_time(marker), color='k')

            self.ax1.legend(loc = 'upper left')

            if self.ax2!=False and len(self.values[0])>1:
                self.ax2.clear()
                self.ax2.hlines(y=40, xmin=self.values[0][0], xmax=self.values[0][-1], linestyle='dashed')
                self.ax2.plot(self.values[0], self.values[5], 'r', label='noise')


    def get_time(self, value):
        return (value - self.start_time) / 1000000.0

    def add_marker(self, data):
        value = int(data)
        self.markers.append(value)

    def update_plot_data(self, data):

        if self.start_time==0:
            self.data_retention = self.controller.get_data_retention()

        self.levels = data['l']

        # remove old data
        if len(self.values[0])>10:
            self.label['y'] = data['u']
            min_time = self.values[0][-1] - self.data_retention
            n = 0
            for value in self.markers:
                if self.get_time(value)<min_time:
                    self.markers[n] = 0
                n = n + 1
            for i in range(len(self.values[0]) - 1, 1, -1):
                if self.values[0][i]<min_time:
                    for n in range(0, len(self.values) - 1):
                        del self.values[n][0:i]
                    break

        # prefill
        if self.start_time==0:
            self.start_time = data['d']['x'][0]
            for i in range(0, (self.data_retention * 10) - 1, 1):
                value = (i - (self.data_retention * 10)) / 10.0
                self.values[0].append(value)
                self.values[1].append(data['d']['y1'][0])
                self.values[2].append(data['d']['y2'][0])
                self.values[3].append(data['d']['y3'][0])
                self.values[4].append(0)
                self.values[5].append(0)

        # copy data
        for value in data['d']['x']:
            self.values[0].append(self.get_time(value))
            type = self.controller.gui_settings['plot_type']
            if type=='I':
                self.values[4].append(data['l'][1])
            elif type=='U':
                self.values[4].append(data['l'][0])
            elif type=='P':
                self.values[4].append(data['l'][2])
            self.values[5].append(data['l'][5] / 1000.0)

        for value in data['d']['y1']:
            self.values[1].append(value)
        for value in data['d']['y2']:
            self.values[2].append(value)
        n = 0
        for value in data['d']['y3']:
            if value==0:
                value = data['d']['y2'][n]
            n = n + 1
            self.values[3].append(value)

        self.update_plot = True


class MainApp(tk.Tk):

    def __init__(self, *args, **kwargs):

        tk.Tk.__init__(self, *args, **kwargs)

        self.gui_settings = { 'y_range': 0, 'convert_units': tk.IntVar(), 'noise': tk.IntVar(), 'retention': tk.StringVar(), 'plot_type': '', 'data_state': [ True, True, True, True ] }
        self.gui_settings['convert_units'].set(1)
        self.gui_settings['noise'].set(0)
        self.gui_settings['retention'].set('60')

        self.connection_name = 'NOT CONNECTED'
        self.plot = Plot(self)
        self.wsc = WSConsole(self)
        self.set_plot_type('')

        tk.Tk.wm_title(self, "HLW8012 live view")

        container = tk.Frame(self)
        container.pack(side="top", fill="both", expand = True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)

        self.frames = {}

        for F in (StartPage, PageLiveStats):
            frame = F(container, self)
            self.frames[F] = frame
            frame.grid(row=0, column=0, sticky="nsew")
        self.show_frame(StartPage)

    def set_connection_status(self, state, error = ''):
        self.update_plot_title()
        if state==False:
            self.connection_name = 'Disconnected'
            if error!='':
                self.connection_name = self.connection_name + ': ' + error
        self.connect_label.config(text='Start Page - ' + self.connection_name)

    def set_connection(self, name):
        self.connection_name = name

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

    def get_data_retention(self):
        retention = int(self.gui_settings['retention'].get())
        if retention<5:
            retention=5
            self.gui_settings['retention'].set('5')
        return retention

    def update_plot_title(self):
        if not self.wsc.connected:
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
        self.plot.ax1.set_title('HLW8012 - ' + title)

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
            self.wsc.send_cmd('SP_HLWPLOT', self.wsc.client_id, '0')

        self.plot.init_plot(type)
        if type != '':
            self.update_plot = True

    def data_handler(self, data):
        if data['n'] == self.gui_settings['plot_type']:
            try:
                self.plot.update_plot_data(data)
            except Exception as e:
                print('Update plot data error: ' + str(e))

    def show_frame(self, cont):
        frame = self.frames[cont]
        frame.tkraise()

class StartPage(tk.Frame):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self,parent)

        self.controller = controller
        self.password_placeholder = '********'
        self.config_filename = os.path.dirname(os.path.realpath(__file__)) + '/hlw8012_config.json'
        self.read_config()

        connection_name = 'NOT CONNECTED'
        controller.set_connection(connection_name)
        label = tk.Label(self, text="Start Page - " + connection_name, font=LARGE_FONT)
        label.pack(pady=10,padx=10)
        self.controller.connect_label = label

        button = ttk.Button(self, text="HLW8012 Live Stats", command=lambda: controller.show_frame(PageLiveStats))
        button.pack()

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP, pady=20)
        pad = 2

        self.form = { 'hostname': tk.StringVar(), 'username': tk.StringVar(), 'password': tk.StringVar(), 'safe_credentials': tk.IntVar() }
        self.form['safe_credentials'].set(1)

        ttk.Label(self, text="Hostname or IP:").grid(in_=top, row=0, column=0, padx=pad, pady=pad)

        values = ['']
        for conn in self.config['connections']:
            values.append(conn['username'] + '@' + conn['hostname'])
        self.hostname = ttk.Combobox(self, width=40, values=values, textvariable=self.form['hostname'])
        self.hostname.grid(in_=top, row=0, column=1, padx=pad, pady=pad)
        self.hostname.bind('<<ComboboxSelected>>', self.on_select)

        ttk.Label(self, text="Username:").grid(in_=top, row=1, column=0, padx=pad, pady=pad)
        ttk.Entry(self, width=43, textvariable=self.form['username']).grid(in_=top, row=1, column=1, padx=pad, pady=pad)

        ttk.Label(self, text="Password:").grid(in_=top, row=2, column=0, padx=pad, pady=pad)
        ttk.Entry(self, width=43, show='*', textvariable=self.form['password']).grid(in_=top, row=2, column=1, padx=pad, pady=pad)

        ttk.Checkbutton(self, text="Safe credentials", variable=self.form['safe_credentials']).grid(in_=top, row=3, column=0, padx=pad, pady=pad, columnspan=2)

        ttk.Button(self, text="Connect", command=self.connect).grid(in_=top, row=4, column=0, padx=pad, pady=pad, columnspan=2)

    def connect(self):
        self.controller.wsc.close()

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
                session = kfcfw_session.Session()
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
                        print(username)
                        print(password)
                        print(sid)
                        if safe==1:
                            self.config['connections'][cur - 1]['sid'] = sid
                            self.write_config()
                            self.form['password'].set('')
                    else:
                        sid = self.config['connections'][cur - 1]['sid']

                self.controller.set_connection('Connected to ' + username + '@' + hostname)
                self.controller.wsc.connect(hostname, sid)


    def on_select(self, event):
        cur = event.widget.current()
        if cur>0:
            conn = self.config['connections'][cur - 1]
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
            print('Failed to read: ' + self.config_filename)
            self.config = { 'connections': [] }

    def write_config(self):
        try:
            with open(self.config_filename, 'wt') as file:
                file.write(json.dumps(self.config))
        except:
            print('Failed to write: ' + self.config_filename)


class PageLiveStats(tk.Frame):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        label = tk.Label(self, text="HLW8012 Live Stats", font=LARGE_FONT)
        label.pack(pady=10, padx=10, in_=top)

        ttk.Button(self, text="Home", width=7, command=lambda: controller.show_frame(StartPage)).pack(in_=top, side=tkinter.LEFT, padx=10)
        ttk.Button(self, text="Current", command=lambda: controller.set_plot_type('I')).pack(in_=top, side=tkinter.LEFT)
        ttk.Button(self, text="Voltage", command=lambda: controller.set_plot_type('U')).pack(in_=top, side=tkinter.LEFT)
        ttk.Button(self, text="Power", command=lambda: controller.set_plot_type('P')).pack(in_=top, side=tkinter.LEFT)
        ttk.Button(self, text="Pause", command=lambda: controller.set_plot_type('')).pack(in_=top, side=tkinter.LEFT)

        check2 = ttk.Checkbutton(self, text="Convert Units", variable=controller.gui_settings['convert_units'])
        check2.pack(in_=top, side=tkinter.LEFT, padx=10)

        check2 = ttk.Checkbutton(self, text="Noise", variable=controller.gui_settings['noise'])
        check2.pack(in_=top, side=tkinter.LEFT, padx=10)

        ttk.Label(self, text="Lower Y Limit:").pack(in_=top, side=tkinter.LEFT, padx=5)
        scale1 = ttk.Scale(self, from_=0, to=100, command=controller.set_y_range)
        scale1.pack(in_=top, side=tkinter.LEFT, padx=10)

        ttk.Label(self, text="Data Retention:").pack(in_=top, side=tkinter.LEFT, padx=5)
        ttk.Entry(self, width=5, textvariable=controller.gui_settings['retention']).pack(in_=top, side=tkinter.LEFT)
        ttk.Label(self, text="seconds").pack(in_=top, side=tkinter.LEFT, padx=2)

        ttk.Button(self, text="Data#1", width=7, command=lambda: controller.toggle_data_state(0)).pack(in_=top, side=tkinter.LEFT)
        ttk.Button(self, text="Data#2", width=7, command=lambda: controller.toggle_data_state(1)).pack(in_=top, side=tkinter.LEFT)
        ttk.Button(self, text="Data#3", width=7, command=lambda: controller.toggle_data_state(2)).pack(in_=top, side=tkinter.LEFT)
        ttk.Button(self, text="Data#4", width=7, command=lambda: controller.toggle_data_state(3)).pack(in_=top, side=tkinter.LEFT)


        canvas = FigureCanvasTkAgg(controller.plot.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        controller.plot.init_animation()

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)


app = MainApp()
app.mainloop()

