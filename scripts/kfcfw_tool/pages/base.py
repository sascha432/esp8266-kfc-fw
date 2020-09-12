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
        self.MEDIUM_FONT = ("Verdana", 13)
        self.SMALL_FONT = ("Verdana", 9)

    def get_config(self):
        return self.controller.config

    def set_config(self, config):
        self.controller.config = config

    def write_config(self):
        self.controller.write_config()

    def get_connection_name(self):
        return self.controller.get_connection_name()

    def show_error(self, message, title = 'Error'):
        self.console.show_message(message, title)

    def show_frame(self, name):
        self.controller.show_frame(name)

    def set_animation(self, state):
        # self.console.debug('page=%s active=%s animation=None' % (self.__class__.__name__, str(state)))
        pass

    def set_active(self, state):
        # self.console.debug('set_active=%s animation=%s' % (self.__class__.__name__, str(state)))
        self.set_animation(state)
