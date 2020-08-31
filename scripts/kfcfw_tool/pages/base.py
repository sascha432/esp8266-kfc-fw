#
# Author: sascha_lammers@gmx.de
#

class PageBase:

    classes = [ 'PageStart', 'PageEnergyMonitor', 'PageTouchpad', 'PageADC' ]

    def __init__(self, controller):
        self.controller = controller
        self._debug = self.controller.console._debug
        self.console = self.controller.console
        self.LARGE_FONT = ("Verdana", 16)

    def show_frame(self, name):
        self.controller.show_frame(name)

    def set_animation(self, state):
        print('set_animation=%s state=%s ignored' % (self.__class__.__name__, str(state)))

    def set_active(self, state):
        print('set_active=%s animation=%s' % (self.__class__.__name__, str(state)))
        self.set_animation(state)
