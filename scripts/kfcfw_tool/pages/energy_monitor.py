#
# Author: sascha_lammers@gmx.de
#

from .base import PageBase
import tkinter
import tkinter as tk
from tkinter import ttk
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure
# import matplotlib.animation as animation
# import matplotlib.pyplot as plt
# from matplotlib import style
# import numpy as np
# from threading import Lock, Thread

class PageEnergyMonitor(tk.Frame, PageBase):

    def __init__(self, parent, controller):

        tk.Frame.__init__(self, parent)
        PageBase.__init__(self, controller)

        top = tk.Frame(self)
        top.pack(side=tkinter.TOP)

        label = tk.Label(self, text="HLW8012 Live Stats", font=self.LARGE_FONT)
        label.pack(pady=10, padx=10, in_=top)

        ttk.Button(self, text="Home", width=7, command=lambda: self.show_frame('PageStart')).pack(in_=top, side=tkinter.LEFT, padx=10)

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

    def set_animation(self, state):
        self.controller.plot.set_animation(state)
