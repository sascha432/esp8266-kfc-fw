#
# Author: sascha_lammers@gmx.de
#

import importlib

class PageBase:

    classes = [ 'PageStart', 'PageEnergyMonitor', 'PageTouchpad', 'PageADC' ]

    def __init__(self, controller):
        self.controller = controller
        self.LARGE_FONT = ("Verdana", 16)

    def show_frame(self, name):
        self.controller.show_frame(name)

    def set_active(self, state):
        print('set_active=%s animation=%u' % (self.__class__.__name__, state))
        self.controller.plot.set_animation(state)
