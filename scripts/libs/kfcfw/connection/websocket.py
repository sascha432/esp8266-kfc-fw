#
# Author: sascha_lammers@gmx.de
#

import websocket
import _thread as thread
import struct
from .base import BaseConnection
import time

class WebSocket(BaseConnection):

    def __init__(self, controller):
        BaseConnection.__init__(self, controller)
        self.client_id = None
        self.ws = None

    def get_cmd_str(self, command, *args):
        parts = []
        for arg in args:
            if not isinstance(arg, str):
                arg = str(arg)
            if ' ' in arg or '"' in arg or ',' in arg:
                arg = '"%s"' % arg.replace('\\',  '\\\\').replace('"',  '\\"')
            parts.append(arg)
        if not command.upper().startswith('AT+') and not command.startswith('+'):
            command = '+' + command

        if parts:
            return '%s=%s' % (command, ','.join(parts))
        return command

    def send_cmd(self, command, *args):
        if self.is_connected():
            self.send(self.get_cmd_str(command, *args))
            return True
        return False

    def send_pwm_cmd(self, pin, value, duration_milliseconds, freq = 40000):
        if value==True:
            value = 1023
        elif value==False:
            value = 0
        if self.is_authenticated():
            self.send_cmd('PWM', pin, value, freq, duration_milliseconds)
        else:
            self.error('not connected')

    def send(self, msg, end = '\r\n'):
        if self.is_connected():
            self.debug('sending: %s' % msg)
            self.ws.send(msg + end)
            # self.ws.send(msg)
            return True
        self.debug('sending failed: not connected: %s' % msg)
        return False

    def on_binary(self, data):
        BaseConnection.on_binary(self, data)

        try:
            (packet_id, ) = struct.unpack_from('H', data)
            if packet_id==2: # WsClient::BinaryPacketType::TOUCHPAD_DATA
                (packet_id, x, y, px, py, time, event_type) = struct.unpack_from('HHHHHLc', data)
                data = {
                    "x": x,
                    "y": y,
                    "px": px,
                    "py": py,
                    "time": time,
                    "raw_type": event_type
                }
        except Exception as e:
            self.console.error('packet parse error: %u: %s' % (packet_id, e))

    def on_text(self, msg):
        BaseConnection.on_text(self, msg)

        if not self.is_authenticated():
            if msg == '+AUTH_OK':
                self.set_connected(True, True)
            elif msg == '+AUTH_ERROR':
                self.set_connected(True, False)
                self.on_error('Authentication failed')
            elif msg == '+REQ_AUTH':
                self.send('+SID ' + self.sid, end = '')
        else:
            if msg.startswith('+CLIENT_ID='):
                self.client_id = msg[msg.find('=') + 1:]
                self.log('ClientID: %s' % self.client_id)
                self.connection['client_id'] = self.client_id;
                self.debug('Connection: %s' % self.conn());

    def on_error(self, error):
        BaseConnection.on_error(self, error)
        # tk.messagebox.showerror(title='Error', message='Connection failed: ' + str(error))
        self.disconnect()
        time.sleep(1)
        self.log('Reconnecting after %s...' % error)
        self.connect(self.hostname, self.sid)

    def on_close(self):
        BaseConnection.on_close(self)
        if self.has_error():
            tk.messagebox.showerror(title='Error', message='Connection closed: %s' % self.get_error())

    def on_open(self):
        BaseConnection.on_connect(self)

    def url(self):
        return 'ws://' + self.hostname + '/serial-console'

    def connect(self, hostname, sid):
        if self.is_connected():
            self.disconnect()

        self.client_id = None
        self.hostname = hostname
        self.sid = sid
        BaseConnection.connect(self, { 'url': self.url(), 'client_id': self.client_id, 'type': 'websocket' })

        # websocket.enableTrace(True)

        self.ws = websocket.WebSocketApp(self.url(),
            on_message = self.on_message,
            on_error = self.on_error,
            on_close = self.on_close,
            on_open = self.on_open)

        def run(*args):
            self.ws.run_forever()

        thread.start_new_thread(run, ())

    def disconnect(self):
        try:
            if self.is_connected() or self.ws:
                self.ws.disconnect()
                self.ws = None
        except Exception as e:
            self.debug(e)

        BaseConnection.disconnect(self)
