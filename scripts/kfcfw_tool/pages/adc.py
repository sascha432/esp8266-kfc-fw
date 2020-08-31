#
# Author: sascha_lammers@gmx.de
#

from .base import PageBase
from .start import PageStart
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
        ttk.Button(self, text="PWM pin 4 512 for 2000ms", command=lambda: self.controller.wsc.send_pwm_cmd(4, 512, 2000)).grid(in_=action, row=1, column=2)
        ttk.Button(self, text="PWM pin 12 512 for 2000ms", command=lambda: self.controller.wsc.send_pwm_cmd(12, 512, 2000)).grid(in_=action, row=1, column=3)
        ttk.Button(self, text="pin 14 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(14, 0, 0)).grid(in_=action, row=1, column=4)
        ttk.Button(self, text="pin 14 HIGH", command=lambda: self.controller.wsc.send_pwm_cmd(14, 1023, 0)).grid(in_=action, row=1, column=5)

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.axis = self.fig.add_subplot(111)
        self.axis.set_title('ADC readings')
        self.axis.set_xlabel('time (µs)')
        self.axis.set_ylabel('ADC value')
        self.line, = self.axis.plot([0, 1000], [0, 1023], lw=2)
        self.reset_data(1000)

        self.anim_running = False
        self.update_plot = False
        canvas = FigureCanvasTkAgg(self.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        self.anim = animation.FuncAnimation(self.fig, func=self.plot_values, interval=100)

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

    def data_handler(self, data, flags):
        self.data['packets'] += 1
        for packed in data:
            time = packed & 0x3fffff
            value = packed >> 22
            self.data['time'].append(time)
            self.data['value'].append(value)
        self.update_plot = True
        # last packet flag
        if flags & 0x0002:
            self.data['dropped'] = True
        if flags & 0x0001:
            self.console._debug = self._debug
            self.console.debug('last packet flag set')
            if self.data['dropped']:
                self.console.error('packets lost flag set')

    def plot_values(self, i):
        if not self.anim_running:
            self.anim.event_source.stop()
        if not self.update_plot:
            return
        # self.line.set_data(self.data.values())
        if len(self.data['time']):
            self.line.set_data(self.data['time'], self.data['value'])
            max_time_millis = max(self.data['time'])
            max_time = max_time_millis / 1000.0
            self.axis.set_ylim(bottom=0, top=int(max(self.data['value']) / 10) * 10 + 10)
            if max_time>=1.0:
                self.axis.set_title('ADC readings %.2f packets/s interval %.0fµs' % (self.data['packets'] / max_time, max_time_millis * 1000.0 / len(self.data['value'])))
        self.update_plot = False

    def set_animation(self, state):
        if self.anim_running!=state:
            self.anim_running = state
            self.update_plot = state
            if state:
                self.anim.event_source.start()

    def reset_data(self, xlim):
        self.data = {'time': [], 'value': [], 'packets': 0, 'dropped': False }
        self.axis.set_xlim(left=0, right=xlim)
        self.line.set_data(self.data['time'], self.data['value'])

    def send_adc_start_cmd(self, interval_micros = 250, duration_millis = 5000, packet_size = 1536):
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
