import socket
import time
import os
import random
import itertools
import copy
import struct
import tkinter
import tkinter as tk
from tkinter import ttk
import threading
from threading import Lock, Thread
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure
import matplotlib.animation as animation
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
from matplotlib.colors import ListedColormap, BoundaryNorm
import numpy;


class MainPage(tk.Frame):
    def __init__(self, *args, **kwargs):
        tk.Frame.__init__(self, *args, **kwargs)

class MainApp(tk.Tk):
    def __init__(self, *args, **kwargs):

        tk.Tk.__init__(self, *args, **kwargs)
        tk.Tk.wm_title(self, "Audio Analyzer")

        container = tk.Frame(self)
        container.pack(side="top", fill="both", expand=True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        self._fig = Figure(figsize=(16, 8), dpi=100)
        self._axis = self._fig.add_subplot(111)

        canvas = FigureCanvasTkAgg(self._fig, self)
        canvas.draw()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True, pady=0, padx=0)
        canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=1)

        self._lock = Lock()

        self._max_cols = 48
        self._max_rows = 255
        self._max_timeout = 80
        self._screen_size = self._max_cols * self._max_rows * 3;
        self._data = {
            'udp_buf': bytearray(1),
            'pixels': bytearray(self._max_cols),
            'peaks': bytearray(self._max_cols),
            'counter': bytearray(self._max_cols),
            'timeout': self._max_timeout,
            'screen': bytearray(self._screen_size)
        }

        self._buffer2 = (bytearray(self._screen_size), bytearray(self._screen_size))

        self._axis.set_ylim([-5, self._max_rows + 5])
        self._axis.set_xlim([-5, self._max_cols + 5])

        for i in range(0, self._max_cols):
            self._data['pixels'][i] = i * 2

        self._bar = self._axis.bar(range(0, self._max_cols), self._data['pixels'], bottom=0, facecolor='Blue', edgecolor='Blue', linewidth=5)
        self._scat = self._axis.scatter(range(0, self._max_cols), self._data['pixels'], s=110, marker='s')


        self._axis.set_yticks(bytearray(self._max_cols))
        self._axis.set_xticks(bytearray(self._max_cols))
        self._axis.set_axis_off()

        self._peak_hold = 40.0
        self._peak_hold_still = self._peak_hold - (self._peak_hold * 0.03)

        self._ani = animation.FuncAnimation(self._fig, self.draw_values, interval=1000 / 30, cache_frame_data=False)

        _thread = threading.Thread(target=self.udp_reader, args=(), daemon=True)
        _thread.start()

        self._page = MainPage(self)
        self._page.tkraise()

    def udp_init(self):
        ip = '192.168.0.62'
        port = 21324
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setblocking(True)
        sock.bind((ip, port))
        self._sock = sock

    def pos(self):
        n = self._pos
        self._pos += 1
        return n

    def write_rgb(self, r, g, b):
        s = self._data['screen']
        s[self.pos()] = r
        s[self.pos()] = g
        s[self.pos()] = b

    def udp_reader(self):

        self.udp_init()
        self._last_update = time.monotonic_ns()

        while True:
            try:
                msg = self._sock.recvfrom(256)
                data = msg[0]
                data_len = len(data)
                if data_len>8:
                    for col in range(0, min(data_len, self._max_cols)):
                        v = self._data['pixels'][col]
                        d = data[col]
                        v = min(1, int(v - v * 0.58))
                        v = min(255, v + d)
                        c = self._data['counter'][col]
                        p = self._data['peaks'][col]
                        if d>=p:
                            # a new data value is higher than the stored peak, restart
                            p = min(255, d + 2)
                            c = int(self._peak_hold)
                        if c > 0:
                            # decrease counter
                            c -= 1
                        else:
                            # the higher the points are the faster they drop down
                            p -= max(1, int(p / (255 / 8.0)))
                        self._data['peaks'][col] = p
                        self._data['counter'][col] = c
                        self._data['pixels'][col] = v
                        self._data['timeout'] = self._max_timeout

            except Exception:
                pass

            timeout = self._data['timeout']
            if timeout==0:
                continue
            if timeout>0:
                timeout -= 1
                self._data['timeout'] = timeout
                if timeout==0:
                    self._data['pixels'] = bytearray(self._max_cols)
                    self._data['peaks'] = bytearray(self._max_cols)
                    self._data['counter'] = bytearray(self._max_cols)
                    continue


            now = time.monotonic_ns()
            dur_ms = (now - self._last_update) // 1000000
            if  dur_ms > 30:
                self._last_update = now

                n = bytearray(self._max_cols)
                m = bytearray(self._max_cols)
                k = []

                for i in range(0, self._max_cols):
                    k.append(min(1.0, self._data['counter'][i] / self._peak_hold_still))

                self._pos = 0
                for row in range(self._max_rows, 0, -1):
                    pd = False
                    for col in range(0, self._max_cols):
                        v = self._data['pixels'][col]
                        p = self._data['peaks'][col]
                        c = self._data['counter'][col]
                        # hold the first 10% at the same spot, then slowly let it fall down
                        if row == p:
                            if c > self._peak_hold_still:
                                # self.write_rgb(255, 0 , 0)
                                m[col] = max(row, m[col])
                            else:
                                # self.write_rgb(0, 0 , 128)
                                m[col] = max(row, m[col])
                            pd = True
                        elif row < v or (row <= p and not pd):
                            if not pd:
                                pd = True
                                # self.write_rgb(0, 0 , 128)
                                m[col] = max(row, m[col])
                            else:
                                # self.write_rgb(0, 200, 0)
                                n[col] = n[col] + 1
                        else:
                            # self.write_rgb(0, 0, 0)
                            pass

                self._buffer2 = (copy.copy(n), copy.copy(m), copy.copy(k))

                # # create a copy of the screen buffer while the drawing thread has been locked
                # if self._lock.acquire(False):
                #     try:
                #         self._buffer2 = (copy.copy(out1), copy.copy(out2))
                #     except:
                #         pass
                #     finally:
                #         self._lock.release()



    def draw_values(self, i):


        out1 = copy.copy(self._buffer2[0])
        out2 = copy.copy(self._buffer2[1])
        out3 = copy.copy(self._buffer2[2])


        a = []
        c = []
        for i in range(0, self._max_cols):

            self._bar[i].set_height(out1[i])
            a.append([i, out2[i]])
            c.append([int(out3[i] * -200 + 200), 255, 255])

        self._scat.set_facecolors(c)
        self._scat.set_offsets(a)


        # rects = self._axis.bar(range(0, self._max_cols), self._data['pixels'], color='blue')
        # self._axis.bar_label(rects, padding=1)

        # remove buffer array while the udp_reader is locked
        # if self._lock.acquire(False):
        #     try:
        #         numpy.copyto(self._buffer2, self._buffer)
        #     except:
        #         return
        #     finally:
        #         self._lock.release()

            # draw the buffer on the canvas
            # print('draw', len(self._buffer2))






app = MainApp()
app.mainloop()
