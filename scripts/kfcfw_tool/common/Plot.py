#
# Author: sascha_lammers@gmx.de
#

import matplotlib.animation as animation
from matplotlib.figure import Figure
from threading import Lock, Thread

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
        try:
            running = self.ani.running
        except:
            return
        if not running:
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

