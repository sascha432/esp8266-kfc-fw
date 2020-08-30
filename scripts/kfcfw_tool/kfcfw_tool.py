#
# Author: sascha_lammers@gmx.de
#

# pip install matplotlib websocket-client

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
import numpy as np
from threading import Lock, Thread

from statistics import mean
import os
import json
import time

from libs import kfcfw as kfcfw

import pages.adc


# import serial
# import atexit

LARGE_FONT= ("Verdana", 16)

def var_dump(o):
    pprint(vars(o))

class TouchpadEventType:

    def __init__(self, value):
        self.value = value
        self.TOUCH       = 0x0001
        self.RELEASED    = 0x0002
        self.MOVE        = 0x0004
        self.TAP         = 0x0008
        self.PRESS       = 0x0010
        self.HOLD        = 0x0020
        self.SWIPE       = 0x0040
        self.DRAG        = 0x0080

    def __repr__(self):
        types = []
        if self.isTouch():
            types.append("TOUCH")
        if self.isReleased():
            types.append("RELEASED")
        if self.isMove():
            types.append("MOVE")
        if self.isTap():
            types.append("TAP")
        if self.isPress():
            types.append("PRESS")
        if self.isHold():
            types.append("HOLD")
        if self.isSwipe():
            types.append("SWIPE")
        if self.isDrag():
            types.append("DRAG")
        return "|".join(types)

    def getValue(self):
        return self.value

    def isTouch(self):
        return self.value & self.TOUCH != 0
    def isReleased(self):
        return self.value & self.RELEASED != 0
    def isMove(self):
        return self.value & self.MOVE != 0
    def isTap(self):
        return self.value & self.TAP != 0
    def isPress(self):
        return self.value & self.PRESS != 0
    def isHold(self):
        return self.value & self.HOLD != 0
    def isSwipe(self):
        return self.value & self.SWIPE != 0
    def isDrag(self):
        return self.value & self.DRAG != 0

class Touchpad:

    def __init__(self, controller):
        self.controller = controller
        self.ani = None
        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.ax = []
        if False:
            self.ax.append(self.fig.subplots(1, 1))
        else:
            for col in self.fig.subplots(2, 2):
                for row in col:
                    self.ax.append(row)
        self.fig.suptitle("Touchpad Movements")
        self.data = []
        self.init_axis()
        self.update_plot = False

    def init_axis(self):
        n = 0
        titles = ["Start/Stop", "Gestures", "Move", "All"]
        for ax in self.ax:
            ax.clear();
            ax.set_title(titles[n])
            self.cols = 14
            cols = self.cols
            self.rows = 8
            rows = self.rows
            sx = 1
            sy = 1
            min_y = 0
            min_x = 0
            max_y = rows + sy
            if len(self.ax)==1:
                max_x = cols + sx # + 4
            else:
                max_x = cols + sx
            ax.hlines(y=sy, xmin=min_x, xmax=max_x, linestyle='dashed')
            ax.hlines(y=rows, xmin=min_x, xmax=max_x, linestyle='dashed')
            ax.vlines(x=sx, ymin=min_y, ymax=max_y, linestyle='dashed')
            ax.vlines(x=cols, ymin=min_y, ymax=max_y, linestyle='dashed')
            ax.grid(True);
            ax.set_xticks(np.arange(1, cols, 1))
            # ax.set_xticks(minor_ticks, minor=True)
            ax.set_yticks(np.arange(1, rows, 1))
            # ax.set_yticks(minor_ticks, minor=True)
            ax.set_ylim(top=min_y, bottom=max_y)
            ax.set_xlim(left=min_x, right=max_x)

            n = n + 1

    def data_handler(self, data):
        data["type"] = TouchpadEventType(int.from_bytes(data["raw_type"], byteorder="big"))
        print(data);
        if data["type"].isTouch():
            self.data = []
            self.init_axis()

        self.data.append(data)
        self.update_plot = True

    def add_coords(self, ax, data, label = False, color=None, scale = 1, alpha = 0.3):
        x = data["x"]
        y = data["y"]
        if x==0 and data["px"]>0:
            x = data["px"]
            alpha *= 0.5
            scale += 15
        if y==0 and data["py"]>0:
            y = data["py"]
            alpha *= 0.5
            scale += 15

        ax.scatter(x, y, c=color, s=scale * 150, label='tab:'+color, alpha=alpha, edgecolors='none')

    def fill(self, data):
        # list2 = []
        # if data["x"]==0 or data["y"]==0:
        #     if data["px"]>0 and data["x"]==0:
        #         data["x"] = data["px"]
        #     if data["py"]>0 and data["y"]==0:
        #         data["y"] = data["py"]
        #     if data["x"]==0 or data["y"]==0:
        #         list2 = []
        #         time = data["time"]
        #         for data2 in list2:
        #             if (data["x"]==0 and data2["x"]!=0) or (data["y"]==0 and data2["y"]!=0):
        #                 list2.append([data2["x"] * 1.0, data2["y"] * 1.0, abs(data2["time"] - time)])
        #         sorted(list2, key=lambda item: item[2])
        #         for data2 in list2:
        #             if data["x"]==0 and data2["x"]!=0:
        #                 data["x"] = data2["x"]
        #             if data["y"]==0 and data2["y"]!=0:
        #                 data["y"] = data2["y"]
        return data

    def plot_values(self, i):
        if not self.ani.running:
            self.ani.event_source.stop()
        elif self.update_plot and len(self.data)>0:
            start = self.data[0]["time"]
            end = self.data[-1]["time"]
            diff = end - start
            timings = []
            last_time = start
            for data in self.data:
                timings.append([data["time"], data["time"] - last_time])
                last_time = data["time"]
                n = 0
                if len(self.ax)==1:
                    n = 100
                time = (data["time"] - start) / (diff + 1.0)
                time_color = (min(1.0, .2 + time * 0.5), .0, .0)
                # print(str(diff)+" "+str(len(self.data))+" "+str(len(self.ax)))
                data = self.fill(data)
                # print(str(data["x"]) + " " + str(data["y"]) + " " + str(data["type"]) + " " + str(data["time"] - start))
                for ax in self.ax:
                    if n==0 or n>=100:
                        if data["type"].isTouch():
                            self.add_coords(ax, data, label="touch", color="green", scale=12)
                        if data["type"].isReleased():
                            self.add_coords(ax, data, label="release", color="red", scale=12, alpha=0.8)
                    elif n==1 or n>=100:
                        if data["type"].isTap():
                            self.add_coords(ax, data, label="tap", color="red", scale=20, alpha=0.8)
                        if data["type"].isPress():
                            self.add_coords(ax, data, label="press", color="green", scale=20, alpha=0.8)
                        if data["type"].isSwipe():
                            self.add_coords(ax, data, label="swipe", color="yellow", scale=10, alpha=0.8)
                    elif n==2 or n>=100:
                        if data["type"].isDrag():
                            self.add_coords(ax, data, label="drag", color="red", scale=7)
                        if data["type"].isHold():
                            self.add_coords(ax, data, label="hold", color="green", scale=12)
                    elif n==3 or n>=100:
                            self.add_coords(ax, data, label="any", color="green", scale=2, alpha=0.8)

                    n = n + 1

            self.update_plot = False

    def set_animation(self, state = False):
        if self.ani==None:
            self.ani = animation.FuncAnimation(self.fig, self.plot_values, interval=200)
        self.ani.running = state
        if state:
            self.update_plot = True
            self.ani.event_source.start()

class Plot:

    def __init__(self, controller):
        self.controller = controller
        self.lock = Lock()
        self.update_plot = False
        self.label = { 'x': 'time in seconds', 'y': '' }
        self.title = ''
        self.ani = None
        self.sensor_config = None
        self.incoming_data = { "start": 0.0, "end": 0.0, "data": 0 }

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.ax1 = self.fig.add_subplot(111)
        self.ax2 = False

    def set_animation(self, state):
        if self.ani==None:
            self.ani = animation.FuncAnimation(self.fig, self.plot_values, interval=self.controller.get_update_rate())
        self.running = state
        if state:
            self.update_plot = True
            self.ani.event_source.start()

    def init_plot(self, type):
        self.stats = [ '', '', '', '' ]
        self.incoming_data["start"] = time.time()
        self.incoming_data["data"] = 0
        self.voltage = 0
        self.current = 0
        self.power = 0
        self.pf = 0
        self.energy = 0
        self.noiseLevel = 0
        self.dimLevel = 0
        self.values = [[], [], [], [], [], [] ]
        self.start_time = 0
        self.max_time = 0
        self.plot_type = self.controller.gui_settings['plot_type']

        self.compress_start = 0
        self.data_retention = int(self.controller.gui_settings['retention'].get())
        if self.data_retention<5:
            self.data_retention = 5
            self.controller.gui_settings['retention'].set('5')
        # keep 10% or max. 5 seconds uncompressed
        self.data_retention_keep = self.data_retention * 0.1;
        if self.data_retention_keep>5:
            self.data_retention_keep = 5
        # min. 10ms max 1s
        self.data_retention_compress = float(self.controller.gui_settings['compress'].get())
        if self.data_retention_compress!=0:
            if self.data_retention_compress<0.01:
                self.data_retention_compress=0.01
                self.controller.gui_settings['compress'].set('0.01')
            elif self.data_retention_compress>1:
                self.data_retention_compress=1
                self.controller.gui_settings['compress'].set('1')

        self.convert_units = self.controller.gui_settings['convert_units'].get()
        self.label['y'] = "µs"
        if self.convert_units==1:
            if self.plot_type=='I':
                self.label['y'] = "A"
            elif self.plot_type=='U':
                self.label['y'] = "V"
            elif self.plot_type=='P':
                self.label['y'] = "W"

        self.fig.clear()
        if self.controller.gui_settings['noise'].get()==1:
            (self.ax1, self.ax2) = self.fig.subplots(2, 1)
        else:
            self.ax1 = self.fig.subplots(1, 1)
            self.ax2 = False

    def plot_values(self, i):
        if not self.ani.running:
            self.ani.event_source.stop()
        elif self.update_plot:
            if not self.lock.acquire(False):
                print("plot_values() could not aquire lock")
                return

            try:
                max_time = self.max_time
                x_left = max_time - self.data_retention
                x_right = max_time
                values = [
                    self.values[0].copy(),
                    self.values[1].copy(),
                    self.values[2].copy(),
                    self.values[3].copy(),
                    self.values[4].copy(),
                    self.values[5].copy()
                ]

            finally:
                self.lock.release()

            # print(str(len(values[0])) + ' ' + str(len(values[1])) + ' ' + str(len(values[2])) + ' ' + str(len(values[3])) + ' ' + str(len(values[4])) + ' ' + str(len(values[5])))

            self.update_plot = False
            self.ax1.clear()

            label_x = ''
            if self.sensor_config!=None:
                fmt = " - Imin. {0:}A Imax {1:}A Rshunt {2:} - Calibration U/I/P {3:} {4:} {5:}"
                label_x = fmt.format(self.sensor_config["Imin"], self.sensor_config["Imax"], self.sensor_config["Rs"], *self.sensor_config["UIPc"])

            self.ax1.set_title('HLW8012 - ' + self.title)
            self.ax1.set_xlabel(self.label['x'] + label_x)
            self.ax1.set_ylabel(self.label['y'])
            self.ax1.set_xlim(left=x_left, right=x_right)

            if len(values[0])==0:
                return

            fmt = "{0:.2f}V {1:.4f}A {2:.2f}W pf {3:.2f} {4:.3f}kWh"
            if self.noiseLevel!=0:
                fmt = fmt + " noise {5:.3f}"
            fmt = fmt + " data {6:d} ({7:.2f}/s)"
            if self.dimLevel!=-1:
                fmt = fmt + " level {8:.1f}%"
            try:
                dpps = self.incoming_data["data"] / (self.incoming_data["end"] - self.incoming_data["start"])
                self.fig.suptitle(fmt.format(self.voltage, self.current, self.power, self.pf, self.energy, self.noiseLevel / 1000.0, len(values[0]), dpps, self.dimLevel * 100.0), fontsize=16)
            except:
                pass

            try:
                stats = []
                n = max(values[3]) / 10
                digits = 2
                while n<1 and digits<4:
                    n = n * 10
                    digits = digits + 1
                fmt = '.' + str(digits) + 'f'
                for i in range(1, 5):
                    tmp = ' min/max ' + format(min(values[i]), fmt) + '/' + format(max(values[i]), fmt) + ' ' + u"\u2300" + ' ' + format(mean(values[i]), fmt)
                    stats.append(tmp)
                self.stats = stats
            except:
                pass

            y_max = 0
            if self.controller.get_data_state(0):
                self.ax1.plot(values[0], values[1], 'g', label='sensor' + self.stats[0], linewidth=0.1)
                y_max = max(y_max, max(values[1]))
            if self.controller.get_data_state(1):
                self.ax1.plot(values[0], values[2], 'b', label='avg' + self.stats[1])
                y_max = max(y_max, max(values[2]))
            if self.controller.get_data_state(2):
                self.ax1.plot(values[0], values[3], 'r', label='integral' + self.stats[2])
                y_max = max(y_max, max(values[3]))
            if self.controller.get_data_state(3):
                self.ax1.plot(values[0], values[4], 'c', label='display' + self.stats[3])
                y_max = max(y_max, max(values[4]))

            i_min = 0
            if self.sensor_config!=None and self.plot_type=='I':
                if self.convert_units==1:
                    i_min = self.sensor_config["Imin"]
                    self.ax1.hlines(y=self.sensor_config["Imax"], xmin=x_left, xmax=x_right, linestyle='dashed')
                else:
                    i_min = self.sensor_config["Ipmax"]
                    self.ax1.hlines(y=self.sensor_config["Ipmin"], xmin=x_left, xmax=x_right, linestyle='dashed')

                y_max = max(y_max, i_min)
                self.ax1.hlines(y=i_min, xmin=x_left, xmax=x_right, linestyle='dashed')

            y_min = y_max * 0.98 * self.controller.get_y_range() / 100.0
            y_max = y_max * 1.02
            self.ax1.set_ylim(top=y_max, bottom=y_min)
            self.ax1.legend(loc = 'lower left')

            if self.ax2!=False:
                self.ax2.clear()
                self.ax2.set_xlim(left=x_left, right=x_right)
                self.ax2.hlines(y=40, xmin=x_left, xmax=x_right, linestyle='dashed')
                self.ax2.plot(values[0], values[5], 'r', label='noise')

    def get_time(self, value):
        return (value - self.start_time) / 1000000.0

    def clean_old_data(self):
        if len(self.values[0])>0:
            min_time = self.values[0][-1] - self.data_retention
            for i in range(len(self.values[0]) - 1, 1, -1):
                if self.values[0][i]<min_time:
                    if i>=self.compress_start:
                        self.compress_start = 0
                    for n in range(0, len(self.values)):
                        del self.values[n][0:i]
                    break

    def data_handler(self, header, data):

        self.incoming_data["end"] = time.time()
        self.incoming_data["data"] = self.incoming_data["data"] + len(data)

        if not self.lock.acquire(True):
            print("data_handler() could not aquire lock")
            return
        try:

            if chr(header[2])==self.plot_type:
                (packet_id, output_mode, data_type, self.voltage, self.current, self.power, self.energy, self.pf, self.noiseLevel, self.dimLevel) = header
                if self.plot_type=='I':
                    display_value = self.current
                elif self.plot_type=='U':
                    display_value = self.voltage
                elif self.plot_type=='P':
                    display_value = self.power

                self.clean_old_data()

                if self.data_retention_compress!=0 and len(self.values[0])>100:
                    keep_time = self.values[0][-1] - self.data_retention_keep
                    while True:
                        compress_time = self.values[0][self.compress_start] + self.data_retention_compress
                        if compress_time>keep_time:
                            break
                        n = 0
                        for t in self.values[0]:
                            if compress_time<t:
                                # print("compress " + str(self.compress_start) + " " + str(n) + " " + str(len(self.values[0])) + " t " + str(compress_time) + " " + str(t))
                                for i in range(0, len(self.values)):
                                    minVal = min(self.values[i][self.compress_start:n])
                                    maxVal = max(self.values[i][self.compress_start:n])
                                    self.values[i][self.compress_start] = minVal
                                    self.values[i][self.compress_start + 1] = maxVal
                                    del self.values[i][self.compress_start + 2:n]
                                self.compress_start = self.compress_start + 2
                                compress_time = self.values[0][self.compress_start] + self.data_retention_compress
                                break
                            n = n + 1
                        if compress_time==self.values[0][self.compress_start] + self.data_retention_compress:
                            break

                if self.start_time==0:
                    self.start_time = data[0]

                # copy data
                for pos in range(0, len(data), 4):
                    self.values[0].append(self.get_time(data[pos]))
                    self.values[1].append(data[pos + 1])
                    self.values[2].append(data[pos + 2])
                    self.values[3].append(data[pos + 3])
                    self.values[4].append(display_value)
                    self.values[5].append(self.noiseLevel / 1000.0)

                self.max_time = self.values[0][-1]
                self.update_plot = True

        finally:
            self.lock.release()


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

        for F in (StartPage, PageEnergyMonitor, PageTouchpad, pages.adc.ReadADC):
            frame = F(container, self)
            self.frames[F] = frame
            frame.grid(row=0, column=0, sticky="nsew")
        self.show_frame(StartPage)

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

    def show_frame(self, cont):
        if self.active:
            self.frames[self.active].set_active(False)
        frame = self.frames[cont]
        self.active = cont;
        frame.set_active(True)
        frame.tkraise()

class StartPage(tk.Frame):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)

        self.controller = controller
        self.password_placeholder = '********'
        self.config_filename = os.path.dirname(os.path.realpath(__file__)) + '/.config/kfcfw_tool.json'
        self.read_config()

        label = tk.Label(self, text="Start Page - DISCONNECTED", font=LARGE_FONT)
        label.pack(pady=10,padx=10)
        self.controller.connect_label = label
        self.controller.connection_name = None

        menu = tk.Frame(self)
        menu.pack(side=tkinter.TOP)

        pad = 8
        button = ttk.Button(self, text="HLW8012 Energy Monitor", command=lambda: controller.show_frame(PageEnergyMonitor))
        button.pack(in_=menu, side=tkinter.LEFT, padx=pad)

        button = ttk.Button(self, text="MPR121 Touchpad", command=lambda: controller.show_frame(PageTouchpad))
        button.pack(in_=menu, side=tkinter.LEFT, padx=pad)

        button = ttk.Button(self, text="ADC", command=lambda: controller.show_frame(pages.adc.PageADC))
        button.pack(in_=menu, side=tkinter.LEFT, padx=pad)

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

        self.controller.connect_button = ttk.Button(self, text="Connect", command=self.connect)
        self.controller.connect_button.grid(in_=top, row=4, column=0, padx=pad, pady=pad, columnspan=2)

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
                file.write(json.dumps(self.config, indent=2))
        except:
            print('Failed to write: ' + self.config_filename)

    def set_active(self, state):
        print("StartPage active " + str(state))
        # self.controller.touchpad.set_animation(False)
        # self.controller.plot.set_animation(False)

class PageEnergyMonitor(tk.Frame):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        self.controller = controller

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        label = tk.Label(self, text="HLW8012 Live Stats", font=LARGE_FONT)
        label.pack(pady=10, padx=10, in_=top)

        ttk.Button(self, text="Home", width=7, command=lambda: controller.show_frame(StartPage)).pack(in_=top, side=tkinter.LEFT, padx=10)

        action = tk.Frame(self)
        action.pack(in_=top, side=tkinter.LEFT, padx=15)

        ttk.Button(self, text="Current", command=lambda: controller.set_plot_type('I')).grid(in_=action, row=1, column=1)
        ttk.Button(self, text="Voltage", command=lambda: controller.set_plot_type('U')).grid(in_=action, row=1, column=2)
        ttk.Button(self, text="Power", command=lambda: controller.set_plot_type('P')).grid(in_=action, row=2, column=1)
        ttk.Button(self, text="Pause", command=lambda: controller.set_plot_type('')).grid(in_=action, row=2, column=2)

        options = tk.Frame(self)
        options.pack(in_=top, side=tkinter.LEFT, padx=15)
        ttk.Label(self, text="Options:").grid(in_=options, row=1, column=1, columnspan=2, padx=5)
        ttk.Checkbutton(self, text="Convert Units", variable=controller.gui_settings['convert_units']).grid(in_=options, row=2, column=1)
        check2 = ttk.Checkbutton(self, text="Noise", variable=controller.gui_settings['noise']).grid(in_=options, row=2, column=2)

        ylimit = tk.Frame(self)
        ylimit.pack(in_=top, side=tkinter.LEFT, padx=15)
        ttk.Label(self, text="Lower Y Limit:").grid(in_=ylimit, row=1, column=1, padx=5)
        scale1 = ttk.Scale(self, from_=0, to=100, command=controller.set_y_range)
        scale1.grid(in_=ylimit, row=2, column=1, padx=10)

        retention = tk.Frame(self)
        retention.pack(in_=top, side=tkinter.LEFT, padx=15)
        ttk.Label(self, text="Data Retention:").grid(in_=retention, row=1, column=1, columnspan=2, padx=5)
        ttk.Entry(self, width=5, textvariable=controller.gui_settings['retention']).grid(in_=retention, row=2, column=1)
        ttk.Label(self, text="seconds").grid(in_=retention, row=2, column=2)
        ttk.Entry(self, width=5, textvariable=controller.gui_settings['compress']).grid(in_=retention, row=3, column=1)
        ttk.Label(self, text="seconds").grid(in_=retention, row=3, column=2)

        data = tk.Frame(self)
        data.pack(in_=top, side=tkinter.LEFT, padx=15)
        ttk.Label(self, text="Display Data:").grid(in_=data, row=1, column=1, columnspan=2, padx=5)
        ttk.Button(self, text="Sensor", width=7, command=lambda: controller.toggle_data_state(0)).grid(in_=data, row=2, column=1)
        ttk.Button(self, text="Avg", width=7, command=lambda: controller.toggle_data_state(1)).grid(in_=data, row=2, column=2)
        ttk.Button(self, text="Integral", width=7, command=lambda: controller.toggle_data_state(2)).grid(in_=data, row=3, column=1)
        ttk.Button(self, text="Display", width=7, command=lambda: controller.toggle_data_state(3)).grid(in_=data, row=3, column=2)

        canvas = FigureCanvasTkAgg(controller.plot.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        self.controller.plot.set_animation(False)

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

    def set_active(self, state):
        print("PageEnergyMonitor active " + str(state))
        self.controller.plot.set_animation(state)


class PageTouchpad(tk.Frame):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        self.controller = controller

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        label = tk.Label(self, text="MPR121 Touch Pad", font=LARGE_FONT)
        label.pack(pady=10, padx=10, in_=top)

        ttk.Button(self, text="Home", width=7, command=lambda: controller.show_frame(StartPage)).pack(in_=top, side=tkinter.LEFT, padx=10)

        canvas = FigureCanvasTkAgg(controller.touchpad.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        self.controller.touchpad.set_animation(False)

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

    def set_active(self, state):
        print("PageTouchpad active " + str(state))
        self.controller.touchpad.set_animation(state)


app = MainApp()
app.mainloop()

