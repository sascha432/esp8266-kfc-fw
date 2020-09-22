#
# Author: sascha_lammers@gmx.de
#

#
# Controller base class with console that uses print
#
# controller = Controller(True)                         debug
# controller = Controller(False)                        no debug
# controller = Controller(True, MyConsole())            debug with different console object
#
# functions:
#   controller.log('message')
#   controller.debug('debug message')
#
# callbacks:
#   controller.connection_status(connection)
#       connection.conn()
#       connection.is_connected()
#       connection.is_authenticated()
#           see connection class for more info...
#

import tkinter as tk
import traceback

class PrintConsole:
    def __init__(self, debug = True):
        self._debug = debug

    def log(self, msg, end = '\n'):
        print(msg, end=end, flush=True)

    def error(self, msg, end = '\n'):
        self.log('ERROR: %s' % msg, end)

    def debug(self, msg, end = '\n'):
        if self._debug:
            self.log('DEBUG: %s' % msg, end)

    def exception(self):
        self.log(traceback.format_exc())

    def show_message(message, title = 'Error'):
        tk.messagebox.showerror(title=title, message=message)

class Controller:
    def __init__(self, debug = True, console = None):
        self.debug = debug
        if console:
            self.console = console
            self.console._debug = debug
        else:
            self.console = PrintConsole(debug)

    def connection_status(self, connection):
        self.console.log('Connection status: %s' % connection.conn())
