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
import sys
import copy
import json
import struct
from pathlib import Path

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure
import matplotlib.animation as animation
import matplotlib.pyplot as plt
import matplotlib.ticker as plticker
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
        self.store_data_in_file_lock = Lock()
        self.store_invoked = None
        self.invoke_stop = None
        self.threshold = 4
        self.motor_running = False
        self.motor_pin = 0
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

        cfg = tk.Frame(self)
        cfg.pack(in_=top, side=tkinter.LEFT, padx=15)
        self.form = tk_form_var.TkFormVar(self.config)
        self.form.trace('m', callback=self.form_modified_callback)

        grid = tk_ez_grid.TkEZGrid(self, cfg, padx=[2, 4], pady=[0, 1], direction='h', sticky=tkinter.W)

        grid.first(ttk.Label(self, text="Pin:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['pin']))

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

        grid.first(ttk.Label(self, text="DAC mA 40KHz:"))
        grid.next(self.form.set_textvariable(ttk.Entry(self, width=10), 'dac', True))

        grid.first(ttk.Label(self, text="Stop current (mA):"))
        grid.next(self.form.set_textvariable(ttk.Entry(self, width=10), 'stop_current', True))

        grid.first(ttk.Label(self, text="Stop time (ms):"))
        grid.next(self.form.set_textvariable(ttk.Entry(self, width=10), 'stop_time', True))

        grid.first(ttk.Label(self, text="PWM value:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['pwm']))

        grid.first(ttk.Label(self, text="frequency:"))
        grid.next(ttk.Entry(self, width=10, textvariable=self.form['frequency']))

        grid.first(ttk.Label(self, text="Name:"))
        values = list(self.controller.config['adc']['items'].keys())
        name = grid.next(self.form.set_textvariable(ttk.Combobox(self, width=15, values=values), 'name', True))
        name.bind('<<ComboboxSelected>>', lambda event: self.load_config_by_number(event.widget.current()))
        # grid.next(self.form.set_textvariable(ttk.Entry(self, width=15), 'name', True))

        grid.first(ttk.Button(self, text="Reset", command=self.reset_named_config), padx=4)
        grid.next(ttk.Button(self, text="Save", command=self.write_config))

        action2 = tk.Frame(self)
        action2.pack(in_=top, side=tkinter.LEFT, padx=15)
        grid = tk_ez_grid.TkEZGrid(self, action2, padx=1, pady=1, direction='h')
        grid.first(ttk.Button(self, text="pin 14 HIGH", command=lambda: self.send_pwm_cmd_threaded(14, True)))
        grid.next(ttk.Button(self, text="pin 14 LOW", command=lambda: self.send_pwm_cmd_threaded(14, False)))
        grid.next(ttk.Button(self, text="run test", command=lambda: self.load_and_execute('test')))
        grid.next(ttk.Button(self, text="increase dac", command=lambda: self.increase_dac_threaded(0.5, 10, 1013, 25)))
        # grid.next(ttk.Button(self, text="increase pwm", command=lambda: self.increase_pwm_threaded(1.0, self.config['pwm'], 1023, 20)))


        grid.first(ttk.Button(self, text="pin 4 PWM", command=lambda: self.set_motor_pin_threaded(4, True)))
        grid.next(ttk.Button(self, text="pin 4 LOW", command=lambda: self.set_motor_pin_threaded(4, False)))
        grid.next(ttk.Button(self, text="pin 5 PWM", command=lambda: self.set_motor_pin_threaded(5, True)))
        grid.next(ttk.Button(self, text="pin 5 LOW", command=lambda: self.set_motor_pin_threaded(5, False)))

        grid.first(ttk.Button(self, text="pin 12 PWM", command=lambda: self.set_motor_pin_threaded(12, True)))
        grid.next(ttk.Button(self, text="pin 12 LOW", command=lambda: self.set_motor_pin_threaded(12, False)))
        grid.next(ttk.Button(self, text="pin 13 PWM", command=lambda: self.set_motor_pin_threaded(13, True)))
        grid.next(ttk.Button(self, text="pin 13 LOW", command=lambda: self.set_motor_pin_threaded(13, False)))

        grid.first(ttk.Button(self, text="turn_open", command=lambda: self.load_and_execute('turn_open')))
        grid.next(ttk.Button(self, text="turn_close", command=lambda: self.load_and_execute('turn_close')))
        grid.next(ttk.Button(self, text="move_open", command=lambda: self.load_and_execute('move_open')))
        grid.next(ttk.Button(self, text="move_close", command=lambda: self.load_and_execute('move_close')))

        grid.first(ttk.Button(self, text="ch0 open", command=lambda: self.send_cmd_threaded(['START', '+BCME=open,0'])))
        grid.next(ttk.Button(self, text="ch0 close", command=lambda: self.send_cmd_threaded(['START', '+BCME=close,0'])))
        grid.next(ttk.Button(self, text="ch1 open", command=lambda: self.send_cmd_threaded(['START', '+BCME=open,1'])))
        grid.next(ttk.Button(self, text="ch1 close", command=lambda: self.send_cmd_threaded(['START', '+BCME=close,1'])))

        # .grid(in_=cfg, row=1, column=2)
        # ttk.Button(self, text="PWM pin 4 500", command=lambda: self.send_pwm_cmd(12, 500, 0)).grid(in_=cfg, row=1, column=1)
        # ttk.Button(self, text="PWM pin 4 300", command=lambda: self.send_pwm_cmd(12, 300, 0)).grid(in_=action, row=1, column=10)
        # ttk.Button(self, text="PWM pin 4 0", command=lambda: self.send_pwm_cmd(12, 0, 0)).grid(in_=action, row=1, column=3)
        # .grid(in_=action, row=1, column=4)
        # ttk.Button(self, text="pin 14 HIGH", command=lambda: self.send_pwm_cmd(14, 1023, 0)).grid(in_=action, row=1, column=5)
        # # //VREF/(10 * RS)
        # ttk.Button(self, text="Seq #1", command=self.seq1).grid(in_=action, row=1, column=9)

        self.fig = Figure(figsize=(12, 8), dpi=100)
        self.fig.subplots_adjust(top=0.96, bottom=0.06, left=0.06, right=0.96)
        self.axis = self.fig.add_subplot(111)
        self.axis.grid(True)
        self.axis.grid(True, linewidth=0.1, color='gray', which='minor')
        self.axis.xaxis.set_major_locator(plticker.MultipleLocator(base=1000))
        self.axis.xaxis.set_minor_locator(plticker.MultipleLocator(base=100))
        self.axis.yaxis.set_major_locator(plticker.MultipleLocator(base=100))
        self.axis.yaxis.set_minor_locator(plticker.MultipleLocator(base=10))
        # self.axis.set_yticks(True)
        self.axis.set_title('ADC readings')
        self.axis.set_xlabel('time (ms)')
        self.hvlines = {}
        if self.config['multiplier']==1:
            self.axis.set_ylabel('ADC value')
        else:
            self.axis.set_ylabel('ADC value converter to %s' % self.config['unit'])


        self.averaging = [1.0, 50.0, 10.0]
        lines_width = [ 0.1, 0.2, 0.5, 0.5, 4.0 ]
        #self.averaging[2] = current limit

        # lines_width = [ 0.01, 0.01, 2.0, 0.01 ]

        self.lines = []
        self.reset_data()
        tmp, = self.axis.plot(self.values[0], self.values[1], lw=lines_width[0], color='blue', alpha = 0.5, label='ADC')
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[2], lw=lines_width[1], color='green', label='Average/%ums' % self.averaging[0])
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[3], lw=lines_width[2], color='orange', label='Average/%ums' % self.averaging[1])
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[4], lw=lines_width[3], color='purple', alpha = 0.7, label='Average/%ums' % self.averaging[2])
        self.lines.append(tmp)
        tmp, = self.axis.plot(self.values[0], self.values[5], lw=lines_width[4], color='red', label='BlindsCtrl current')
        self.lines.append(tmp)
        self.axis.legend()


        self.anim_running = False
        self.update_plot = False
        canvas = FigureCanvasTkAgg(self.fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=5, padx=5)
        self.anim = animation.FuncAnimation(self.fig, func=self.plot_values, interval=100)

        toolbar = NavigationToolbar2Tk(canvas, self)
        toolbar.update()
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

    def get_config_defaults(self):
        return {
            'pin': 0,
            'multiplier': 1,
            'offset': 0,
            'unit': 'raw',
            'interval': 2500,
            'duration': 15000,
            'display': 15000,
            'frequency': 40000,
            'dac': 850,
            'stop_current': 650,
            'stop_time': 1000,
            'pwm': 128,
        }

    def update_config_defaults(self, name, config):
        tmp = self.get_config_defaults()
        for key, val in tmp.items():
            if key not in config:
                config[key] = val
        config['name'] = name

    def get_named_config_copy(self, name='defaults'):
        if name in self.controller.config['adc']['items']:
            return copy.deepcopy(self.controller.config['adc']['items'][name])
        else:
            self.console.log('config %s not found, creating from defaults' % name)
            if name!='defaults':
                return self.get_named_config_copy()
        return self.get_config_defaults()

    def read_config(self):
        # merge config with main config
        try:
            config = copy.deepcopy(self.controller.config["adc"])
        except:
            self.controller.config["adc"] = {
                'last': 'defaults',
                'items': {
                    'defaults': self.get_config_defaults()
                }
            }
            config = copy.deepcopy(self.controller.config["adc"])
            config['items']['defaults']['name'] = 'defaults'
            self.console.exception()

        self.console.debug('ADC config: %s' % (self.controller.config["adc"]['items']))
        name = config['last']
        if name not in config['items']:
            name = 'defaults'
        self.config = config['items'][name]
        self.update_config_defaults(name, self.config)
        self.console.debug('ADC config selected: %s' % (self.config))

    def write_config(self):
        # remove name before storing
        tmp = copy.deepcopy(self.config)
        name = tmp['name']
        del tmp['name']
        self.console.debug('write ADC config %s: %s' % (name, tmp))
        self.controller.config['adc']['items'][name] = tmp
        self.controller.write_config()

    def reset_named_config(self):
        name = self.config['name']
        self.console.debug('reset config %s' % (name))
        self.load_named_config(name, True)

    def load_config_by_number(self, num):
        self.console.debug('load config #%u' % num)
        n = 0
        for name in self.controller.config['adc']['items'].keys():
            if n == num:
                self.load_named_config(name)
                break
            n += 1

    def load_named_config(self, name, reset=False):
        if reset==False:
            self.console.debug('load config %s' % name)
            self.controller.config["adc"]['last'] = name
            self.controller.write_config()
        tmp = self.get_named_config_copy(name)
        self.update_config_defaults(name, tmp)
        # copy to form vars
        for key in self.config.keys():
            val = None
            try:
                val = tmp[key]
                self.config[key] = val
                self.form[key].set(val)
            except:
                self.console.exception()

    def load_previous_data(self):
        try:
            self.reset_data()

            dir = os.path.dirname(self.data_file)
            patt = 'adc_data.*.json'
            files = sorted(Path(dir).iterdir(), key=lambda key: key.is_file() and key.match(patt) and key.stat().st_mtime or 0, reverse=True)
            if files and files[0].is_file():
                with open(files[0].resolve()) as file:
                    self.data = json.loads(file.read())
                self.data['display'] = max(self.data['time'])
                self.calc_data(True)
                self.set_data()
                self.current_data_file = files[0].name
                self.update_plot = True
            else:
                self.console.error('cannot find %s' % os.path.join(dir, patt))

        except Exception as e:
            self.reset_data()
            self.console.exception()
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

        time_val = self.data['time'][-1]

        try:
            diff = self._calc['diff']
            avg1 = self._calc['avg1']
            avg2 = self._calc['avg2']
            avg3 = self._calc['avg3']

            for n in range(self._calc['n'], num):
                time_val = self.data['time'][n]
                raw_value = self.data['adc_raw'][n]
                raw_value2 = self.data['adc2_raw'][n]
                value = self.adc_cvt_raw_value(raw_value)
                value2 = self.adc_cvt_raw_value(raw_value2)
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

                diff0 = time_val - diff
                if diff0>0:
                    # averaging diff1 = diff0 * N = last N milliseconds

                    diff = time_val
                    diff1 = diff0 * self.averaging[0]
                    diff2 = diff1 + 1.0
                    avg1 = ((avg1 * diff1) + value) / diff2

                    diff1 = diff0 * self.averaging[1]
                    diff2 = diff1 + 1.0
                    avg2 = ((avg2 * diff1) + value) / diff2

                    diff1 = diff0 * self.averaging[2]
                    diff2 = diff1 + 1.0
                    avg3 = ((avg3 * diff1) + value) / diff2

                # xval = 0
                # if n>0:
                #     # cover the last 200ms
                #     start = self.find_start(time_val - 10, n)
                #     xval = np.mean(self.values[1][start:n])
                #     # xval = np.mean(self.data['adc_raw'][start:n])
                # value2=xval;

                if self.stopped==None and avg1>self.threshold:
                    self.stopped = False
                    self.add_vlines['start'] = time_val
                    if self.invoke_stop!=None:
                        tmp = self.invoke_stop / 1000.0
                        self.add_vlines['timeout'] = self.invoke_stop
                        self.invoke_stop = None
                        self.send_stop_delayed(tmp, self.add_vlines['start'])
                        self.console.debug("auto off in %.3fs triggered by avg1=%u time_val=%u @%.6f" % (tmp, avg1, time_val, time.monotonic()))
                elif self.stopped==False and avg1<self.threshold:
                    self.stopped = True
                    self.add_vlines['stop'] = time_val

                # CURRENT LIMIT
                if avg3>self.config['stop_current'] and self.motor_running:
                    self.console.log('Current limit @ %u pin=%u' % (time_val, self.motor_pin))
                    if self.motor_pin!=0:
                        self.send_pwm_cmd_threaded(self.motor_pin, 0)
                    self.send_cmd_set_pins_to_input([4, 5, 12, 13])
                    self.add_vlines['limit'] = time_val
                    self.motor_running = False

                self.values[0].append(time_val)
                self.values[1].append(value)
                self.values[2].append(avg1)
                self.values[3].append(avg2)
                self.values[4].append(avg3)
                self.values[5].append(value2)

            # store values for next call
            self._calc['n'] = n
            self._calc['diff'] = diff
            self._calc['avg1'] = avg1
            self._calc['avg2'] = avg2
            self._calc['avg3'] = avg3

            if last_packet:
                if False:
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
            self.console.exception()
            raise e

    def data_handler(self, data, flags, packet_size, msg, odata):
        if not self.lock.acquire(True):
            self.console.error('data_handler: failed to lock plot')
            return
        time_val = 0
        try:
            self.data['packets'] += 1

            i = -1
            try:
                for i in range(0, len(data) - 1, packet_size):
                    if packet_size==6:
                        (dword, word) = struct.unpack_from("=LH", data[i:i + 6])
                    elif packet_size==4:
                        word = 0
                        (dword) = struct.unpack_from("=L", data[i:i + 4])

                    time_val = dword & 0x3fffff
                    value = dword >> 22
                    value2 = word / 64.0
                    self.data['time'].append(time_val)
                    self.data['adc_raw'].append(value)
                    self.data['adc2_raw'].append(value2)
            except Exception as e:
                self.console.error(msg)
                self.console.error('packet error data_len=%u packet_size=%u idx=%d' % (len(data), packet_size, i))
                print(odata)
                self.console.exception()
                os._exit(1)
                raise e


            # # 4 byte packets
            # for packed in data:
            #     time_val = packed & 0x3fffff
            #     value = packed >> 22
            #     self.data['time'].append(time_val)
            #     self.data['adc_raw'].append(value)

            self.calc_data(flags & 0x0001)

            # last packet flag
            if flags & 0x0002:
                self.data['dropped'] = True
            if flags & 0x0001:
                self.console._debug = self._debug
                self.console.debug('last packet flag set')
                if self.data['dropped']:
                    self.console.error('packets lost flag set')
                self.store_data_in_file_delayed(2.0)
                self.send_cmd_adc_stop_threaded(0.5)
        except:
            self.console.exception()
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
                x_right = max(self.data['xlim'], max_time_millis)
                x_right = max(x_right, self.data['display'])
                self.data['xlim'] = x_right
                x_left = max(self.data['xlim'] - self.data['display'], 0)
                self.axis.set_xlim(left=x_left, right=x_right)

                max_time = max_time_millis / 1000.0
                self.data['ylim'] = max(self.data['hlines']['threshold'] * 2, int(max(self.values[1]) * 1.05 / 10) * 10 + 1)
                self.axis.set_ylim(bottom=0, top=self.data['ylim'])
                if max_time>=1.0:
                    file = ''
                    if self.current_data_file:
                        file = ' (' + self.current_data_file + ')'
                    self.axis.set_title('ADC readings %.2f packets/s interval %.0fµs%s' % (self.data['packets'] / max_time, max_time_millis * 1000.0 / num, file))

            self.add_vlines_from_list(self.add_vlines)
            self.add_vlines = {}

        except:
            self.console.exception()

        finally:
            self.update_plot = False
            self.lock.release()

    def set_animation(self, state):
        if self.anim_running!=state:
            self.anim_running = state
            self.update_plot = state
            if state:
                self.anim.event_source.start()

    def get_vlines(self):
        return [('start', 'green'), ('stop', 'blue'), ('limit', 'red'), ('timeout', 'purple')]

    def get_hlines(self):
        return {'threshold': 'purple', 'current': 'red', 'dac': 'orange'}

    def get_hvlines_keys(self):
        l = list(self.get_hlines().keys())
        for item in self.get_vlines():
            l.append(item[0])
        return l

    def add_vlines_from_list(self, lines):
        for item in self.get_vlines():
            name = item[0]
            if name in lines:
                time_val = lines[name]
                color = item[1]
                self.data['vlines'][name] = time_val
                self.remove_hvlines(name)
                self.hvlines[name] = self.axis.vlines(time_val, 0, self.adc_cvt_raw_value(1024), color=color, label=name)
                self.update_plot = True
                # self.hvlines[name] = self.axis.vlines(time_val, 0, self.config['stop_current'], color=color, label=name)

    def value_with_unit(self, y):
        if self.config['multiplier']==1:
            return str(y)
        return '%u%s' % (y, self.config['unit'])

    def add_hline(self, name, y, color = None):
        colors = self.get_hlines()
        if color==None:
            color = colors[name]
        self.remove_hvlines(name)
        self.hvlines[name] = self.axis.hlines(y, 0, self.config['duration'] * 2, color=color, label='%s (%s)' % (name, self.value_with_unit(y)), linestyle='dotted')
        self.axis.legend()

    def set_data(self):
        self.threshold = self.adc_cvt_raw_value(1024 / 200.0) + 0.5
        has_data = len(self.data['time'])>1
        x_left = 0
        x_right = 1000
        if has_data:
            # show all data when loaded from file
            max_time = self.data['time'][-1]
            x_lim = max_time
            x_right = max(200, x_lim)
            self.data['xlim'] = x_right

        self.remove_hvlines(self.get_hvlines_keys())
        if has_data:
            n = 1
            for line in self.lines:
                line.set_data(self.values[0], self.values[n])
                n += 1

            self.axis.set_xlim(left=x_left, right=x_right)
            self.add_vlines_from_list(self.data['vlines'])
        else:
            for line in self.lines:
                line.set_data([0, 0], [1000, 100])
            self.axis.set_ylim(bottom=0, top=100)
            # self.add_hline('threshold', self.threshold)
            # self.add_hline('current', self.config['stop_current'])
            # self.add_hline('dac', self.config['dac'])
        self.axis.set_xlim(left=0, right=1000)
        for name, val in self.data['hlines'].items():
            self.add_hline(name, val)

    def reset_data(self, x_lim = 1000):
        self.store_invoked = None
        self.current_data_file = None
        self.data = { 'time': [], 'adc_raw': [], 'adc2_raw': [], 'packets': 0, 'dropped': False, 'xlim': x_lim, 'ylim': self.threshold * 2, 'display': self.config['display'], 'vlines': {}, 'hlines': {'current': self.config['stop_current'], 'threshold': self.threshold, 'dac': self.config['dac']} }
        self.values = [ [],[],[],[],[],[] ]
        self.add_vlines = {}
        self.stopped = None
        self._calc = {'n': -1}
        self.set_data()
        xloc = int(self.config['display'] / 10000) * 1000
        if xloc<1000:
            xloc=1000
        self.axis.xaxis.set_major_locator(plticker.MultipleLocator(base=xloc))
        self.axis.xaxis.set_minor_locator(plticker.MultipleLocator(base=int(xloc / 10)))

    def ___thread(self, thread_id, callback, args = (), delay = 0):
        if delay>0:
            self.console.debug('thread @%.6f #%u: delayed %.4f' % (time.monotonic(), thread_id, delay / 1.0))
            time.sleep(delay)
        self.console.debug('thread @%.6f #%u: start (delay=%.4f)' % (time.monotonic(), thread_id, delay / 1.0))
        # print(callback, args)
        callback(*args)
        self.console.debug('thread #%u: end' % (thread_id))

    def start_thread(self, callback, args = (), delay = 0):
        thread = threading.Thread(target=self.___thread, args=(self.thread_id, callback, args, delay), daemon=True)
        thread.start()
        self.thread_id += 1
        return thread

    def send_pwm_cmd_threaded(self, pin, value, duration_milliseconds = 0):
        self.start_thread(self.send_pwm_cmd, (pin, value, duration_milliseconds))

    def write_data(self, val, avg):
        tmp = 'data_pwm_%04u_freq_%05u.csv' % (self.config['pwm'], self.config['frequency'])
        print("%u,%.3f,%u,%u,%u (%s)" % (val, avg, self.config['dac'], self.config['pwm'], self.config['frequency'], tmp))

        with open(tmp, 'at') as file:
            file.write('%u,%.3f,%u,%u,%u\n' % (val, avg, int(self.config['dac']), int(self.config['pwm']), int(self.config['frequency'])))


    def increase_dac(self, delay, from_val, to_val, step):
        val_before = 0
        for val in range(from_val, to_val, step):
            time.sleep(delay)
            try:
                avg = self._calc['avg2']
            except:
                avg = 0
            self.console.log("setting DAC to %u (avg current before %.2f)" % (val, avg))
            self.write_data(val_before, avg)
            val_before = val
            self.config['dac']= val
            if self.motor_running!=True:
                return
            self.send_pwm_cmd(16, val, 0, 40000);
        self.write_data(val_before, avg)
        self.send_cmd_adc_stop()


    def increase_pwm(self, delay, from_val, to_val, step):
        val_before = 0
        for val in range(from_val, to_val + step + 1, step):
            time.sleep(delay)
            if val>1023:
                val=1023;
            try:
                avg = self._calc['avg2']
            except:
                avg = 0
            # self.console.log("setting DAC to %u (avg current before %.2f)" % (val, avg))
            self.write_data(val_before, avg)
            val_before = val
            self.config['pwm']= val
            if self.motor_running!=True:
                return
            self.send_pwm_cmd(5, val, 0);
        self.write_data(val_before, avg)
        self.send_cmd_adc_stop()


    def increase_dac_threaded(self, delay, from_val, to_val, step):
        self.start_thread(self.increase_dac, (delay, from_val, to_val, step))

    def increase_pwm_threaded(self, delay, from_val, to_val, step):
        self.start_thread(self.increase_pwm, (delay, from_val, to_val, step))

    def send_pwm_cmd_threaded_wait(self, pin, value, duration_milliseconds = 0, wait = 0.1):
        self.start_thread(self.send_pwm_cmd, (pin, value, duration_milliseconds))
        time.sleep(wait)

    def store_data_in_file(self):
        if not self.store_data_in_file_lock.acquire(False):
            return
        try:
            if self.store_invoked=='thread_started':
                # 0 = time
                # 1 = value
                # 2 = avg1
                # 3 = avg2...
                max_value = max(self.values[2])
                self.console.debug('write if %u>%u = %s' % (max_value, self.threshold, str(max_value>self.threshold)))
                if max_value>self.threshold:
                    self.store_invoked = 'writing'
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
                    self.store_invoked = 'written'
                else:
                    self.store_invoked = 'no data'
        except Exception as e:
            self.store_invoked = 'write_error'
            self.console.error('write adc data: %s' % e)
        finally:
            self.store_data_in_file_lock.release();

    def store_data_in_file_delayed(self, delay = 2.0):
        if self.store_invoked==False:
            if self.store_data_in_file_lock.acquire(False):
                try:
                    self.store_invoked = 'thread_started'
                    self.start_thread(self.store_data_in_file, (), delay)
                except:
                    self.store_invoked = 'lock_error'
                finally:
                    self.store_data_in_file_lock.release()

    def adc_reading_ended(self):
        self.store_data_in_file_delayed()

    def send_cmd_adc_stop(self):
        self.send_cmd_set_pins_to_input([4, 5, 12, 13])
        self.motor_running = False
        self.controller.wsc.send_cmd_adc_stop()
        self.store_data_in_file_delayed(2.0)

    def send_cmd_adc_stop_threaded(self, delay):
        self.start_thread(self.send_cmd_adc_stop, (), delay)

    def send_stop_delayed_exec(self, time_val, dummy):
        self.send_cmd_set_pins_to_input([4, 5, 12, 13])
        self.motor_running = False
        self.controller.wsc.send_cmd_adc_stop()
        try:
            self.add_vlines['timeout'] = self.data['time'][-1]
        except:
            self.add_vlines['timeout'] = time_val
        self.add_vlines_from_list(self.add_vlines)
        self.add_vlines = {}
        self.console._debug = self._debug

    def send_stop_delayed(self, delay, time_val):
        self.start_thread(self.send_stop_delayed_exec, (time_val, 1), delay)

    def send_cmd_set_pins_to_input(self, list):
        parts = []
        for pin in list:
            parts.append("+PWM=%u,0" % (pin))
            parts.append("+PWM=%u,input" % (pin))
        self.send_cmd_threaded(parts, 0.05)

    def load_and_execute(self, name):
        self.console.debug('execute config %s' % (name))
        self.load_named_config(name, True)
        self.invoke_stop = self.config['stop_time']
        self.set_motor_pin_threaded(self.config['pin'], True)

    def set_motor_pin_threaded(self, pin, value):
        if value==False:
            value = 0
        else:
            value=self.config['pwm']
        self.start_thread(self.__set_motor_pin_thread, (pin, value))

    def send_cmd_threaded(self, commands, delay = 0.015):
        # parts = []
        for cmd in commands:
            if cmd=="START":
                self.send_adc_start_cmd();
                time.sleep(0.250)
            else:
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
            self.motor_pin = 0
            self.send_cmd_adc_stop_threaded(1.25)
        else:
            self.motor_running = True
            self.motor_pin = pin
            value_14 = 0
            if pin in [4, 5]:
                value_14 = 1023
            parts = []
            parts.append(self.get_cmd_str_pwm(14, value_14))
            self.send_cmd_threaded(parts, 0.05)
            parts = []
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
                duration = int(self.config['stop_time'])
                # 40-100% in 4 steps
                start_val = int(value * 0.4)
                step = int((value - start_val) / 3 + 0.5)
                for i in range(start_val, value, step):
                    parts.append(self.get_cmd_str_pwm(pin, i, duration))
                    duration = 0
                    # parts.append(self.get_cmd_str_delay(1))
                parts.append(self.get_cmd_str_pwm(pin, value, duration))

                self.send_cmd_threaded(parts);
                time.sleep(0.5)
                self.console._debug = False
            else:
                self.send_pwm_cmd_threaded_wait(pin, value)

    def get_cmd_str_pwm(self, pin, value, duration = 0):
        return self.controller.wsc.get_cmd_str('PWM', pin, value, self.config['frequency'], duration)

    def get_cmd_str_delay(self, delay):
        return self.controller.wsc.get_cmd_str('DLY', delay)

    def send_pwm_cmd(self, pin, value, duration_milliseconds = 0, freq = None):
        if value==False:
            value=0
        elif value==True:
            value=1023
        elif value<0 or value==None:
            value=self.config['pwm']
        if freq==None:
            freq = self.config['frequency']
        self.console.debug('pwm pin=%u value=%u frequency=%u' % (pin, value, int(freq)))
        self.controller.wsc.send_pwm_cmd(pin, value, duration_milliseconds, freq)

    def send_adc_start_cmd(self):
        self.reset_data(self.config['display'])
        self.axis.set_xlim(left=0, right=self.data['xlim'])
        # allows to store the data
        if self.store_invoked!=False:
            if not self.store_data_in_file_lock.acquire(False):
                raise Exception('cannot lock store_data_in_file_lock')
            self.store_invoked = False
            self.store_data_in_file_lock.release()

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

    def remove_hvlines(self, names):
        if isinstance(names, str):
            names = [names]
        for name in names:
            if name in self.hvlines:
                self.hvlines[name].remove();
                del self.hvlines[name]

    def update_current_limit(self, val):
        pwm = int(1024 / 3.3 * (val / 1000.0))
        self.console.log('setting DAC to %umA / %u' % (val, pwm))
        self.send_pwm_cmd(16, pwm, 0, 40000)
        self.remove_hvlines('dac')
        if self.config['dac']>self.config['stop_current']*1.2:
            return
        self.hvlines['dac'] = self.axis.hlines(val, 0, self.data['xlim'], color='orange')

    def update_stop_current(self, val):
        self.remove_hvlines('stop_current')
        self.hvlines['stop_current'] = self.axis.hlines(val, 0, self.data['xlim'], color='red')

    def form_modified_callback(self, event, key, val, var, *args):
        self.console.debug('callback %s=%s' % (str(key), str(val)))
        if key=='dac':
            self.update_current_limit(val)
        elif key=='stop_current':
            self.update_stop_current(val)
        elif key=='name':
            self.load_named_config(val)

