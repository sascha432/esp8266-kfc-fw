#
# Author: sascha_lammers@gmx.de
#

from .base import PageBase
from .start import PageStart
import tkinter
import tkinter as tk
from tkinter import ttk
import time
import os
import json

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure
import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib import style

import numpy as np
import _thread as thread
from threading import Lock

class PageADC(tk.Frame, PageBase):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        PageBase.__init__(self, controller)

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        label = tk.Label(self, text="ADC Readings", font=self.LARGE_FONT)
        label.pack(pady=10, padx=10, in_=top)

        ttk.Button(self, text="Home", width=7, command=lambda: self.show_frame('PageStart')).pack(in_=top, side=tkinter.LEFT, padx=10)

        action = tk.Frame(self)
        action.pack(in_=top, side=tkinter.LEFT, padx=15)

        ttk.Button(self, text="Start", command=lambda: self.send_adc_start_cmd()).grid(in_=action, row=1, column=1)
        ttk.Button(self, text="PWM pin 4 1023", command=lambda: self.controller.wsc.send_pwm_cmd(4, 1023, 0)).grid(in_=action, row=1, column=2)
        ttk.Button(self, text="PWM pin 4 0", command=lambda: self.controller.wsc.send_pwm_cmd(4, 0, 0)).grid(in_=action, row=1, column=3)
        ttk.Button(self, text="pin 14 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(14, 0, 0)).grid(in_=action, row=1, column=4)
        ttk.Button(self, text="pin 14 HIGH", command=lambda: self.controller.wsc.send_pwm_cmd(14, 1023, 0)).grid(in_=action, row=1, column=5)
        # //VREF/(10 * RS)
        ttk.Button(self, text="DAC 0.7A", command=lambda: self.controller.wsc.send_pwm_cmd(16, int(1024 / 3.3 * 0.7), 0)).grid(in_=action, row=1, column=6)
        ttk.Button(self, text="DAC 1.5A", command=lambda: self.controller.wsc.send_pwm_cmd(16, int(1024 / 3.3 * 1.5), 0)).grid(in_=action, row=1, column=7)
        ttk.Button(self, text="DAC 3.3A", command=lambda: self.controller.wsc.send_pwm_cmd(16, 1023, 0)).grid(in_=action, row=1, column=8)
        ttk.Button(self, text="Seq #1", command=self.seq1).grid(in_=action, row=1, column=9)

        self.lock = Lock()
        self.data_file = os.path.join(self.controller.config_dir, 'adc_data.json')

        self.adc_multiplier = 9.765625 / 1.3
        self.adc_offset = -2;
        self.adc_unit = 'mA'
        # self.adc_multiplier = 1

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.axis = self.fig.add_subplot(111)
        self.axis.set_title('ADC readings')
        self.axis.set_xlabel('time (ms)')
        if self.adc_multiplier==1:
            self.axis.set_ylabel('ADC value')
        else:
            self.axis.set_ylabel('ADC value converter to %s' % self.adc_unit)
        self.line, = self.axis.plot([0, 1000], [0, 1023], lw=1, color='blue', alpha = 0.2)
        self.line2, = self.axis.plot([0, 1000], [0, 1023], lw=2, color='green')
        self.line3, = self.axis.plot([0, 1000], [0, 1023], lw=2, color='red')
        self.line4, = self.axis.plot([0, 1000], [0, 1023], lw=2, color='orange')


        try:
            with open(self.data_file) as file:
                self.data = json.loads(file.read())

            if 'value' in self.data and self.data['value']:
                self.data['values'] = [ self.data['value'] ]
                del self.data['value']
                del self.data['avg']
                del self.data['stall']
                del self.data['diff']
                del self.data['avg_value']
            n = len(self.data['values'][0])
            self.data['values'] = [ self.data['values'][0], np.arange(n), np.arange(n), np.arange(n) ]

            self.calc_data()
            self.set_data()
        except Exception as e:
            self.reset_data(1000)
            self.console.error(e)
            raise e



        self.anim_running = False
        self.update_plot = False
        canvas = FigureCanvasTkAgg(self.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        self.anim = animation.FuncAnimation(self.fig, func=self.plot_values, interval=100)

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

    def seq1(self):
        def run(*args):
            for i in range(5, 0, -1):
                self.console.log('countdown %u' % i)
                time.sleep(1)
            run_time = 7000
            self.send_adc_start_cmd(1500, run_time + 200 * 2)
            time.sleep(0.2)
            self.controller.wsc.send_pwm_cmd(14, 1023, run_time + 50)
            self.controller.wsc.send_pwm_cmd(4, 450, run_time)
        thread.start_new_thread(run, ())

    def calc_data(self):
        n = 0
        avg = 0.0
        avg1 = 0.0
        avg2 = 0.0
        diff = 0
        for time in self.data['time']:
            value = self.data['values'][0][n]
            # self.data['values'][1].append(0)
            # self.data['values'][2].append(0)
            # self.data['values'][3].append(0)

            # end = n - 1
            # # start = 0
            # start = end - 10
            # stall = 0
            # if end>0:
            #     if start<0:
            #         start = 0
            #     arr = self.data['values'][0][start:end]
                # stall = np.std(arr)
                # stall = np.percentile(arr, 5)
                # stall = np.mean(arr)
                # stall = np.median(arr)
            # self.data['stall'][n] = stall

            diff0 = time - diff
            if diff0>0:
                diff = time
                diff1 = diff0 * 10.0
                diff2 = diff1 + 1.0
                avg = ((avg * diff1) + value) / diff2

                diff1 = diff0 * 150.0
                diff2 = diff1 + 1.0
                avg1 = ((avg1 * diff1) + value) / diff2

                diff1 = diff0 * 50.0
                diff2 = diff1 + 1.0
                avg2 = ((avg2 * diff1) + value) / diff2
            self.data['values'][1][n] = avg
            self.data['values'][2][n] = avg1
            self.data['values'][3][n] = avg2

            n += 1

    def data_handler(self, data, flags):
        if not self.lock.acquire(True):
            self.console.error('failed to lock plot')
            return
        self.data['packets'] += 1
        for packed in data:
            time = packed & 0x3fffff
            value = packed >> 22
            value = (value + self.adc_offset) * self.adc_multiplier
            self.data['time'].append(time)
            self.data['values'][0].append(value)
            self.data['values'][1].append(0)
            self.data['values'][2].append(0)
            self.data['values'][3].append(0)
        self.calc_data()

        # last packet flag
        if flags & 0x0002:
            self.data['dropped'] = True
        if flags & 0x0001:
            self.console._debug = self._debug
            self.console.debug('last packet flag set')
            if self.data['dropped']:
                self.console.error('packets lost flag set')
            try:
                with open(self.data_file, 'wt') as file:
                    file.write(json.dumps(self.data))
            except Exception as e:
                self.console.error(e)

        self.update_plot = True
        self.lock.release()

    def plot_values(self, i):
        if not self.anim_running:
            self.anim.event_source.stop()
        if not self.update_plot:
            return
        if not self.lock.acquire(False):
            return
        # self.line.set_data(self.data.values())
        if len(self.data['time']):
            self.line.set_data(self.data['time'], self.data['values'][0])
            self.line2.set_data(self.data['time'], self.data['values'][1])
            self.line3.set_data(self.data['time'], self.data['values'][2])
            self.line4.set_data(self.data['time'], self.data['values'][3])
            max_time_millis = max(self.data['time'])
            max_time = max_time_millis / 1000.0
            self.data['ylim'] = int(max(self.data['values'][0]) * 1.15 / 10) * 10
            self.axis.set_ylim(bottom=0, top=self.data['ylim'])
            if max_time>=1.0:
                self.axis.set_title('ADC readings %.2f packets/s interval %.0fµs' % (self.data['packets'] / max_time, max_time_millis * 1000.0 / len(self.data['values'][0])))
        self.update_plot = False
        self.lock.release()

    def set_animation(self, state):
        if self.anim_running!=state:
            self.anim_running = state
            self.update_plot = state
            if state:
                self.anim.event_source.start()

    def set_data(self):
        self.axis.set_xlim(left=0, right=self.data['xlim'])
        self.line.set_data(self.data['time'], self.data['values'][0])
        self.line2.set_data(self.data['time'], self.data['values'][1])
        self.line3.set_data(self.data['time'], self.data['values'][2])
        self.line4.set_data(self.data['time'], self.data['values'][3])

    def reset_data(self, xlim):
        self.data = {'time': [], 'values': [ [], [], [], [] ], 'packets': 0, 'dropped': False, 'xlim': xlim, 'ylim': 0 }
        self.set_data()

    def send_adc_start_cmd(self, interval_micros = 1500, duration_millis = 15000, packet_size = 1536):
        self.reset_data(duration_millis)
        # calculate packet size to get about 5 packets per second
        packets_per_second = 5.0
        packet_size = int((1000000 * 4 / interval_micros) / packets_per_second)
        if packet_size<128:
            packet_size=128
        self.controller.wsc.send_cmd_adc_start(interval_micros, duration_millis, packet_size)
        self.console._debug = False

    def send_adc_stop_cmd(self):
        self.controller.wsc.send_cmd_adc_stop()
        self.console._debug = self._debug
