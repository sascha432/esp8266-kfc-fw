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
from pathlib import Path

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure
import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib import style

import numpy as np
import threading
from threading import Lock

from common import tk_form_var
from common import tk_ez_grid

class PageADC(tk.Frame, PageBase):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        PageBase.__init__(self, controller)

        self.lock = Lock()
        self.data_file = os.path.join(self.controller.config_dir, 'adc_data.json')

        self.thread_id = 1
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
        grid.first(ttk.Button(self, text="Stop", command=lambda: self.send_cmd_adc_stop_threaded(0.1)))
        grid.next(ttk.Button(self, text="Load Data", command=self.load_previous_data))

        # ttk.Button(self, text="Start", command=self.send_adc_start_cmd).grid(in_=action, row=1, column=1)
        # ttk.Button(self, text="Clear", command=self.load_previous_data).grid(in_=action, row=1, column=2)
        # ttk.Button(self, text="Load Data", command=self.load_previous_data).grid(in_=action, row=2, column=2)

        cfg = tk.Frame(self)
        cfg.pack(in_=top, side=tkinter.LEFT, padx=15)
        self.form = tk_form_var.TkFormVar(self.config)
        self.form.trace('m', callback=self.form_modified_callback)

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
        grid.next(self.form.set_textvariable(ttk.Entry(self, width=10), 'dac', True))

        grid.first(ttk.Label(self, text="Stop current (mA):"))
        grid.next(self.form.set_textvariable(ttk.Entry(self, width=10), 'stop_current', True))

        grid.first(ttk.Label(self, text="PWM value:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['pwm']))

        grid.first(ttk.Label(self, text="frequency:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['frequency']))

        grid.first(None);
        grid.next(ttk.Button(self, text="Save", command=self.write_config))


        action2 = tk.Frame(self)
        action2.pack(in_=top, side=tkinter.LEFT, padx=15)
        grid = tk_ez_grid.TkEZGrid(self, action2, padx=1, pady=1, direction='h')
        grid.first(ttk.Button(self, text="pin 14 HIGH", command=lambda: self.send_pwm_cmd_threaded(14, True)))
        grid.next(ttk.Button(self, text="pin 14 LOW", command=lambda: self.send_pwm_cmd_threaded(14, False)))

        grid.first(ttk.Button(self, text="pin 4 PWM", command=lambda: self.set_motor_pin_threaded(4, True)))
        grid.next(ttk.Button(self, text="pin 4 LOW", command=lambda: self.set_motor_pin_threaded(4, False)))
        grid.next(ttk.Button(self, text="pin 5 PWM", command=lambda: self.set_motor_pin_threaded(5, True)))
        grid.next(ttk.Button(self, text="pin 5 LOW", command=lambda: self.set_motor_pin_threaded(5, False)))

        grid.first(ttk.Button(self, text="pin 12 PWM", command=lambda: self.set_motor_pin_threaded(12, True)))
        grid.next(ttk.Button(self, text="pin 12 LOW", command=lambda: self.set_motor_pin_threaded(12, False)))
        grid.next(ttk.Button(self, text="pin 13 PWM", command=lambda: self.set_motor_pin_threaded(13, True)))
        grid.next(ttk.Button(self, text="pin 13 LOW", command=lambda: self.set_motor_pin_threaded(13, False)))

        # .grid(in_=cfg, row=1, column=2)
        # ttk.Button(self, text="PWM pin 4 500", command=lambda: self.send_pwm_cmd(12, 500, 0)).grid(in_=cfg, row=1, column=1)
        # ttk.Button(self, text="PWM pin 4 300", command=lambda: self.send_pwm_cmd(12, 300, 0)).grid(in_=action, row=1, column=10)
        # ttk.Button(self, text="PWM pin 4 0", command=lambda: self.send_pwm_cmd(12, 0, 0)).grid(in_=action, row=1, column=3)
        # .grid(in_=action, row=1, column=4)
        # ttk.Button(self, text="pin 14 HIGH", command=lambda: self.send_pwm_cmd(14, 1023, 0)).grid(in_=action, row=1, column=5)
        # # //VREF/(10 * RS)
        # ttk.Button(self, text="DAC 0.1A", command=lambda: self.send_pwm_cmd(16, int(1024 / 3.3 * 0.1), 0)).grid(in_=action, row=1, column=6)
        # ttk.Button(self, text="DAC 0.2A", command=lambda: self.send_pwm_cmd(16, int(1024 / 3.3 * 0.2), 0)).grid(in_=action, row=1, column=7)
        # ttk.Button(self, text="DAC 1.3A", command=lambda: self.send_pwm_cmd(16, int(1024 / 3.3 * 1.3), 0)).grid(in_=action, row=1, column=8)
        # ttk.Button(self, text="Seq #1", command=self.seq1).grid(in_=action, row=1, column=9)

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.fig.subplots_adjust(top=0.96, bottom=0.06, left=0.06, right=0.96)
        self.axis = self.fig.add_subplot(111)
        self.axis.set_title('ADC readings')
        self.axis.set_xlabel('time (ms)')
        self.hlines = {}
        if self.config['multiplier']==1:
            self.axis.set_ylabel('ADC value')
        else:
            self.axis.set_ylabel('ADC value converter to %s' % self.config['unit'])

        self.lines = []
        self.reset_data()
        tmp, = self.axis.plot(self.values[0], self.values[1], lw=0.1, color='blue', alpha = 0.5)
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
        self.anim = animation.FuncAnimation(self.fig, func=self.plot_values, interval=200)

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
            'frequency': 40000,
            'dac': 850,
            'stop_current': 650,
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

            dir = os.path.dirname(self.data_file)
            patt = 'adc_data.*.json'
            files = sorted(Path(dir).iterdir(), key=lambda key: key.is_file() and key.match(patt) and key.stat().st_mtime or 0, reverse=True)
            if files and files[0].is_file():
                with open(files[0].resolve()) as file:
                    self.data = json.loads(file.read())
                self.calc_data(True)
                self.set_data()
                self.current_data_file = files[0].name
                self.update_plot = True
            else:
                self.console.error('cannot find %s' % os.path.join(dir, patt))

        except Exception as e:
            self.reset_data()
            self.console.error(e)
            raise e

    def find_start(self, min_time, n):
        for i in range(n, 1, -1):
            if self.data['time'][i] < min_time:
                return i
        return 0

    def adc_cvt_raw_value(self, raw_value):
        return (raw_value + self.config['offset']) * self.config['multiplier']

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
                value = self.adc_cvt_raw_value(raw_value)
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

                    diff1 = diff0 * 75.0
                    diff2 = diff1 + 1.0
                    avg3 = ((avg3 * diff1) + value) / diff2

                val = 0
                # if n>0:
                    # cover the last 100ms
                    # start = self.find_start(time - 5000, n)
                    # start=0
                    # val = np.percentile(self.values[1][start:n], 15)

                self.values[0].append(time)
                self.values[1].append(value)
                self.values[2].append(avg1)
                self.values[3].append(avg2)
                self.values[4].append(avg3)

            # store values for next call
            self._calc['n'] = n
            self._calc['diff'] = diff
            self._calc['avg1'] = avg1
            self._calc['avg2'] = avg2
            self._calc['avg3'] = avg3

            if avg1>self.config['stop_current'] and self.motor_running:
                self.motor_running = False
                self.send_cmd_set_pins_to_input([4, 5, 12, 13])

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
                self.send_cmd_adc_stop_threaded(0.1)
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
                    file = ''
                    if self.current_data_file:
                        file = ' (' + self.current_data_file + ')'
                    self.axis.set_title('ADC readings %.2f packets/s interval %.0fµs%s' % (self.data['packets'] / max_time, max_time_millis * 1000.0 / num, file))
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
        self.remove_hlines(['stop_current', 'dac', 'noise'])
        self.hlines['noise'] = self.axis.hlines(15, 0, self.data['xlim'], color='yellow', label="Noise")
        self.hlines['stop_current'] = self.axis.hlines(self.config['stop_current'], 0, self.data['xlim'], color='red', label="Stop Current")
        if self.config['dac']<self.config['stop_current']*1.2:
            self.hlines['dac'] = self.axis.hlines(self.config['dac'], 0, self.data['xlim'], color='orange', label="Current Limit")

    def reset_data(self, xlim = 1000):
        if xlim>self.config['display']:
            xlim=self.config['display']
        self.current_data_file = None
        self.data = { 'time': [], 'adc_raw': [], 'packets': 0, 'dropped': False, 'xlim': xlim, 'ylim': 0 }
        self.values = [ [],[],[],[],[] ]
        self._calc = {'n': -1}
        # for line in self.axis.lines:
        #     self.axis.lines.remove(line)
        self.set_data()

    def ___thread(self, thread_id, callback, args = (), delay = 0):
        if delay>0:
            self.console.debug('thread #%u: delayed %.4f' % (thread_id, delay / 1.0))
            time.sleep(delay)
        self.console.debug('thread #%u: start' % (thread_id))
        print(callback, args)
        callback(*args)
        self.console.debug('thread #%u: end' % (thread_id))

    def start_thread(self, callback, args = (), delay = 0):
        thread = threading.Thread(target=self.___thread, args=(self.thread_id, callback, args, delay), daemon=True)
        thread.start()
        self.thread_id += 1
        return thread

    def send_pwm_cmd_threaded(self, pin, value, duration_milliseconds = 0):
        self.start_thread(self.send_pwm_cmd, (pin, value, duration_milliseconds))

    def send_pwm_cmd_threaded_wait(self, pin, value, duration_milliseconds = 0, wait = 0.1):
        self.start_thread(self.send_pwm_cmd, (pin, value, duration_milliseconds))
        time.sleep(wait)

    def send_cmd_adc_stop_threaded(self, delay):
        self.console._debug = self._debug
        self.send_cmd_set_pins_to_input([4, 5, 12, 13])
        self.start_thread(self.controller.wsc.send_cmd_adc_stop, delay=delay)

    def send_cmd_set_pins_to_input(self, list):
        parts = []
        for pin in list:
            parts.append("+PWM=%u,0" % (pin))
            parts.append("+PWM=%u,input" % (pin))
        self.send_cmd_threaded(parts, 0.05)

    def set_motor_pin_threaded(self, pin, value):
        if value==False:
            value = 0
        else:
            value=self.config['pwm']
        self.start_thread(self.__set_motor_pin_thread, (pin, value))

    def send_cmd_threaded(self, commands, delay = 0.015):
        # parts = []
        for cmd in commands:
            self.controller.wsc.send(cmd)
            time.sleep(delay)
        #     if len(parts)<2:
        #         parts.append(cmd)
        #     else:
        #         cmd_str = ';'.join(parts)
        #         parts = []
        # if len(parts):
        #     cmd_str = ';'.join(parts)
        #     self.controller.wsc.send(cmd_str)

    def __set_motor_pin_thread(self, pin, value):
        self.console.log('set motor pin %u value=%u' % (pin, value))
        if pin not in[4, 5, 12, 13]:
            raise KeyError('pin %u invalid' % pin)

        if not value:
            self.motor_running = False
            self.send_cmd_adc_stop_threaded(1.25)
        else:
            self.motor_running = True
            value_14 = 0
            if pin in [4, 5]:
                value_14 = 1023
            parts = []
            parts.append(self.get_cmd_str_pwm(14, value_14))
            parts.append(self.get_cmd_str_pwm(4, 0))
            parts.append(self.get_cmd_str_pwm(5, 0))
            parts.append(self.get_cmd_str_pwm(12, 0))
            parts.append(self.get_cmd_str_pwm(13, 0))
            self.console._debug = True
            self.send_cmd_threaded(parts, 0.05)

            self.update_current_limit(self.config['dac'])
            time.sleep(0.1)
            self.send_adc_start_cmd()
            time.sleep(0.1)

            if value!=0 and value!=False:
                parts = []
                for i in range(0, value, int(value/10)):
                    parts.append(self.get_cmd_str_pwm(pin, i))
                    # parts.append(self.get_cmd_str_delay(1))
                parts.append(self.get_cmd_str_pwm(pin, value))

                self.send_cmd_threaded(parts);
                time.sleep(0.5)
                self.console._debug = False
            else:
                self.send_pwm_cmd_threaded_wait(pin, value, duration_milliseconds)

    def get_cmd_str_pwm(self, pin, value):
        return self.controller.wsc.get_cmd_str('PWM', pin, value, self.config['frequency'], 0)

    def get_cmd_str_delay(self, delay):
        return self.controller.wsc.get_cmd_str('DLY', delay)

    def send_pwm_cmd(self, pin, value, duration_milliseconds = 0):
        if value==False:
            value=0
        elif value==True:
            value=1023
        elif value<0 or value==None:
            value=self.config['pwm']
        self.console.log('pwm pin=%u value=%u' % (pin, value))
        self.controller.wsc.send_pwm_cmd(pin, value, duration_milliseconds, self.config['frequency'])

    def send_adc_start_cmd(self):
        self.reset_data(self.config['duration'])

        # calculate packet size to get about 5 packets per second
        packets_per_second = 5.0
        packet_size = int((1000000 * 4 / self.config['interval']) / packets_per_second)
        if packet_size<128:
            packet_size=128
        elif packet_size>1024:
            packet_size=1024

        self.console._debug = False
        self.start_thread(callback=self.controller.wsc.send_cmd_adc_start, args=(self.config['interval'], self.config['duration'], packet_size))
        time.sleep(0.05)


    def remove_hlines(self, names):
        for name in names:
            if name in self.hlines:
                self.hlines[name].remove();
                del self.hlines[name]

    def update_current_limit(self, val):
        pwm = int(1024 / 3.3 * (val / 1000.0))
        self.console.log('setting DAC to %umA / %u' % (val, pwm))
        self.send_pwm_cmd(16, pwm)
        self.remove_hlines(['dac'])
        if self.config['dac']>self.config['stop_current']*1.2:
            return
        self.hlines['dac'] = self.axis.hlines(val, 0, self.data['xlim'], color='orange')

    def update_stop_current(self, val):
        self.remove_hlines(['stop_current'])
        self.hlines['stop_current'] = self.axis.hlines(val, 0, self.data['xlim'], color='red')

    def form_modified_callback(self, event, key, val, var, *args):
        if key=='dac':
            self.update_current_limit(val)
        elif key=='stop_current':
            self.update_stop_current(val)

