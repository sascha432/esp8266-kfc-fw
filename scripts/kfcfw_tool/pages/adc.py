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

from common import tk_form_var
from common import tk_ez_grid

class PageADC(tk.Frame, PageBase):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        PageBase.__init__(self, controller)

        self.lock = Lock()
        self.data_file = os.path.join(self.controller.config_dir, 'adc_data.json')

        self.read_config()

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        label = tk.Label(self, text="ADC Readings", font=self.LARGE_FONT)
        label.pack(pady=10, padx=10, in_=top)

        ttk.Button(self, text="Home", width=7, command=lambda: self.show_frame('PageStart')).pack(in_=top, side=tkinter.LEFT, padx=10)

        action = tk.Frame(self)
        action.pack(in_=top, side=tkinter.LEFT, padx=15)
        grid = tk_ez_grid.TkEZGrid(self, action, padx=1, pady=1)
        grid.first(ttk.Button(self, text="Start", command=self.send_adc_start_cmd))
        grid.next(ttk.Button(self, text="Clear", command=self.reset_data))
        grid.first(ttk.Button(self, text="Stop", command=self.send_adc_stop_cmd))
        grid.next(ttk.Button(self, text="Load Data", command=self.load_previous_data))

        # ttk.Button(self, text="Start", command=self.send_adc_start_cmd).grid(in_=action, row=1, column=1)
        # ttk.Button(self, text="Stop", command=self.send_adc_stop_cmd).grid(in_=action, row=2, column=1)
        # ttk.Button(self, text="Clear", command=self.load_previous_data).grid(in_=action, row=1, column=2)
        # ttk.Button(self, text="Load Data", command=self.load_previous_data).grid(in_=action, row=2, column=2)

        cfg = tk.Frame(self)
        cfg.pack(in_=top, side=tkinter.LEFT, padx=15)
        self.form = tk_form_var.TkFormVar(self.config)
        grid = tk_ez_grid.TkEZGrid(self, cfg, padx=[2, 4], pady=[0, 1], direction='h', sticky=tkinter.W)

        grid.first(ttk.Label(self, text="Interval (µs):"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['interval']))

        grid.first(ttk.Label(self, text="Duration (ms):"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['duration']))

        grid.first(ttk.Label(self, text="Display (ms):"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['display']))

        grid.first(ttk.Label(self, text="ADC unit:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['unit']))

        grid.first(ttk.Label(self, text="Multiplier:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['multiplier']))

        grid.first(ttk.Label(self, text="Current limit (mA):"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['dac']))

        grid.first(ttk.Label(self, text="PWM value:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['pwm']))

        grid.first(None);
        grid.next(ttk.Button(self, text="Save", command=self.write_config))


        action2 = tk.Frame(self)
        action2.pack(in_=top, side=tkinter.LEFT, padx=15)
        grid = tk_ez_grid.TkEZGrid(self, action2, padx=1, pady=1, direction='h')
        grid.first(ttk.Button(self, text="pin 14 HIGH", command=lambda: self.controller.wsc.send_pwm_cmd(14, 1023, 0)))
        grid.next(ttk.Button(self, text="pin 14 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(14, 0, 0)))

        grid.first(ttk.Button(self, text="pin 4 PWM", command=lambda: self.controller.wsc.send_pwm_cmd(4, self.config['pwm'], 0)))
        grid.next(ttk.Button(self, text="pin 4 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(4, 0, 0)))
        grid.next(ttk.Button(self, text="pin 5 PWM", command=lambda: self.controller.wsc.send_pwm_cmd(5, self.config['pwm'], 0)))
        grid.next(ttk.Button(self, text="pin 5 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(5, 0, 0)))

        grid.first(ttk.Button(self, text="pin 12 PWM", command=lambda: self.controller.wsc.send_pwm_cmd(12, self.config['pwm'], 0)))
        grid.next(ttk.Button(self, text="pin 12 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(12, 0, 0)))
        grid.next(ttk.Button(self, text="pin 13 PWM", command=lambda: self.controller.wsc.send_pwm_cmd(13, self.config['pwm'], 0)))
        grid.next(ttk.Button(self, text="pin 13 LOW", command=lambda: self.controller.wsc.send_pwm_cmd(13, 0, 0)))

        grid.first(ttk.Button(self, text="Set DAC", command=lambda: self.controller.wsc.send_pwm_cmd(16, int(1024 / 3.3 * (self.config['dac'] / 1000.0)), 0)))

        # .grid(in_=cfg, row=1, column=2)
        # ttk.Button(self, text="PWM pin 4 500", command=lambda: self.controller.wsc.send_pwm_cmd(12, 500, 0)).grid(in_=cfg, row=1, column=1)
        # ttk.Button(self, text="PWM pin 4 300", command=lambda: self.controller.wsc.send_pwm_cmd(12, 300, 0)).grid(in_=action, row=1, column=10)
        # ttk.Button(self, text="PWM pin 4 0", command=lambda: self.controller.wsc.send_pwm_cmd(12, 0, 0)).grid(in_=action, row=1, column=3)
        # .grid(in_=action, row=1, column=4)
        # ttk.Button(self, text="pin 14 HIGH", command=lambda: self.controller.wsc.send_pwm_cmd(14, 1023, 0)).grid(in_=action, row=1, column=5)
        # # //VREF/(10 * RS)
        # ttk.Button(self, text="DAC 0.1A", command=lambda: self.controller.wsc.send_pwm_cmd(16, int(1024 / 3.3 * 0.1), 0)).grid(in_=action, row=1, column=6)
        # ttk.Button(self, text="DAC 0.2A", command=lambda: self.controller.wsc.send_pwm_cmd(16, int(1024 / 3.3 * 0.2), 0)).grid(in_=action, row=1, column=7)
        # ttk.Button(self, text="DAC 1.3A", command=lambda: self.controller.wsc.send_pwm_cmd(16, int(1024 / 3.3 * 1.3), 0)).grid(in_=action, row=1, column=8)
        # ttk.Button(self, text="Seq #1", command=self.seq1).grid(in_=action, row=1, column=9)

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.fig.subplots_adjust(top=0.96, bottom=0.06, left=0.06, right=0.96)
        self.axis = self.fig.add_subplot(111)
        self.axis.set_title('ADC readings')
        self.axis.set_xlabel('time (ms)')
        if self.config['multiplier']==1:
            self.axis.set_ylabel('ADC value')
        else:
            self.axis.set_ylabel('ADC value converter to %s' % self.config['unit'])

        self.lines = []
        self.reset_data(1000)
        tmp, = self.axis.plot(self.values[0], self.values[1], lw=0.1, color='blue', alpha = 0.2)
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[2], lw=0.5, color='green')
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[3], lw=1, color='orange')
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[4], lw=2, color='red')
        self.lines.append(tmp)


        self.anim_running = False
        self.update_plot = False
        canvas = FigureCanvasTkAgg(self.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        self.anim = animation.FuncAnimation(self.fig, func=self.plot_values, interval=100)

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

    def read_config(self):
        # merge config with main config
        config = self.get_config()
        defaults = {
            'multiplier': 1,
            'offset': 0,
            'unit': 'raw',
            'interval': 2500,
            'duration': 15000,
            'display': 15000,
            'dac': 850,
            'pwm': 128,
        }
        try:
            tmp = defaults.copy()
            tmp.update(config['adc'])
        except:
            tmp = defaults.copy()
        config['adc'] = tmp.copy()
        self.set_config(config)
        self.config = self.controller.config['adc']
        self.console.debug('ADC config: %s' % self.config)

    def write_config(self):
        self.console.debug('write ADC config: %s' % self.config)
        self.controller.write_config()

    def load_previous_data(self):
        try:
            self.reset_data()
            with open(self.data_file) as file:
                self.data = json.loads(file.read())
            self.calc_data(True)
            self.set_data()
            self.update_plot = True
        except Exception as e:
            self.reset_data(1000)
            self.console.error(e)
            raise e

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

    def find_start(self, min_time, n):
        for i in range(n, 1, -1):
            if self.data['time'][i] < min_time:
                return i
        return 0

    def calc_data(self, last_packet = False):

        if self._calc['n']==-1:
            self._calc = {
                'n': 0,
                'avg1': 0.0,
                'avg2': 0.0,
                'avg3': 0.0,
                'diff': 0
            }

        if not 'time' in self.data:
            return
        num = len(self.data['time'])
        if num==0:
            return

        try:
            diff = self._calc['diff']
            avg1 = self._calc['avg1']
            avg2 = self._calc['avg2']
            avg3 = self._calc['avg3']

            for n in range(self._calc['n'], num):
                time = self.data['time'][n]
                raw_value = self.data['adc_raw'][n]
                value = (raw_value + self.config['offset']) * self.config['multiplier']

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

                # colors:
                # blue for value alpha 0.3
                # green
                # orange
                # red

                diff0 = time - diff
                if diff0>0:
                    diff = time
                    diff1 = diff0 * 10.0
                    diff2 = diff1 + 1.0
                    avg1 = ((avg1 * diff1) + value) / diff2

                    diff1 = diff0 * 25.0
                    diff2 = diff1 + 1.0
                    avg2 = ((avg2 * diff1) + value) / diff2

                    diff1 = diff0 * 50.0
                    diff2 = diff1 + 1.0
                    avg3 = ((avg3 * diff1) + value) / diff2

                val = 0
                if n>0:
                    # cover the last 100ms
                    start = self.find_start(time - 100, n)
                    val = np.median(self.values[1][start:n])

                self.values[0].append(time)
                self.values[1].append(value)
                self.values[2].append(avg1)
                self.values[3].append(avg2)
                self.values[4].append(val)

            # store values for next call
            self._calc['n'] = n
            self._calc['diff'] = diff
            self._calc['avg1'] = avg1
            self._calc['avg2'] = avg2
            self._calc['avg3'] = avg3

            if last_packet:
                info = {
                    'std': {
                        'adc_raw': float(np.std(self.data['adc_raw'])),
                        'values': float(np.std(self.values[1])),
                        'average': float(np.std(self.values[3])),
                    },
                    'mean': {
                        'adc_raw': float(np.mean(self.data['adc_raw'])),
                        'values': float(np.mean(self.values[1])),
                        'average': float(np.mean(self.values[3])),
                    },
                    'median': {
                        'adc_raw': float(np.median(self.data['adc_raw'])),
                        'values': float(np.median(self.values[1])),
                        'average': float(np.median(self.values[3])),
                    },
                    'min': {
                        'adc_raw': float(np.min(self.data['adc_raw'])),
                        'values': float(np.min(self.values[1])),
                        'average': float(np.min(self.values[3])),
                    },
                    'max': {
                        'adc_raw': float(np.max(self.data['adc_raw'])),
                        'values': float(np.max(self.values[1])),
                        'average': float(np.max(self.values[3])),
                    },
                }
                self.console.log('stats: %s' % json.dumps(info, indent=True))
                self._calc = {'n': -1}


        except Exception as e:
            self.console.log('calc_data failed: %s' % e)
            raise e

    def data_handler(self, data, flags):
        if not self.lock.acquire(True):
            self.console.error('data_handler: failed to lock plot')
            return
        try:
            self.data['packets'] += 1
            for packed in data:
                time_val = packed & 0x3fffff
                value = packed >> 22
                self.data['time'].append(time_val)
                self.data['adc_raw'].append(value)
            self.calc_data(flags & 0x0001)

            # last packet flag
            if flags & 0x0002:
                self.data['dropped'] = True
            if flags & 0x0001:
                self.console._debug = self._debug
                self.console.debug('last packet flag set')
                if self.data['dropped']:
                    self.console.error('packets lost flag set')
                try:
                    # find file that does not exist
                    # keep up to 100 files overwriting old ones first
                    n = 0
                    tmp = self.data_file
                    while os.path.exists(tmp):
                        tmp = self.data_file.replace('.json', '.%02u.json' % (n % 100))
                        if n>=100:
                            break
                        n += 1

                    with open(tmp, 'wt') as file:
                        file.write(json.dumps(self.data))
                except Exception as e:
                    self.console.error('write adc data: %s' % e)
        except Exception as e:
            self.console.error(e)
        finally:
            self.update_plot = True
            self.lock.release()

    def plot_values(self, i):
        if not self.anim_running:
            self.anim.event_source.stop()
        if not self.update_plot:
            return
        if not self.lock.acquire(False):
            return
        try:
            if self.values[0]:
                self.set_data()
                num = len(self.values[0])
                max_time_millis = max(self.values[0])

                # scroll if there is more data thasn 15 seconds
                if max_time_millis>self.config['display']:
                    self.axis.set_xlim(left=max_time_millis - self.config['display'], right=max_time_millis)

                max_time = max_time_millis / 1000.0
                self.data['ylim'] = int(max(self.values[1]) * 1.15 / 10) * 10 + 1
                self.axis.set_ylim(bottom=0, top=self.data['ylim'])
                if max_time>=1.0:
                    self.axis.set_title('ADC readings %.2f packets/s interval %.0fµs' % (self.data['packets'] / max_time, max_time_millis * 1000.0 / num))
        except Exception as e:
            self.console.log(e)

        finally:
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
        if len(self.lines):
            for n in range(len(self.lines)):
                self.lines[n].set_data(self.values[0], self.values[n + 1])

    def reset_data(self, xlim):
        if xlim>self.config['display']:
            xlim=self.config['display']
        self.data = { 'time': [], 'adc_raw': [], 'packets': 0, 'dropped': False, 'xlim': xlim, 'ylim': 0 }
        self.values = [ [],[],[],[],[] ]
        self._calc = {'n': -1}
        self.set_data()

    def send_adc_start_cmd(self):
        self.reset_data(self.config['duration'])

        # calculate packet size to get about 5 packets per second
        packets_per_second = 5.0
        packet_size = int((1000000 * 4 / self.config['interval']) / packets_per_second)
        if packet_size<128:
            packet_size=128
        elif packet_size>1024:
            packet_size=1024

        self.controller.wsc.send_cmd_adc_start(self.config['interval'], self.config['duration'], packet_size)
        self.console._debug = False

    def send_adc_stop_cmd(self):
        self.controller.wsc.send_cmd_adc_stop()
        self.console._debug = self._debug
