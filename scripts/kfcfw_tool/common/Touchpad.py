#
# Author: sascha_lammers@gmx.de
#

import matplotlib.animation as animation
from matplotlib.figure import Figure
import numpy as np

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
