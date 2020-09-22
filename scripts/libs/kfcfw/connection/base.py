#
# Author: sascha_lammers@gmx.de
#

# BaseConnection

class BaseConnection:
    def __init__(self, controller):
        self.controller = controller
        self.connected = False
        self.authenticated = False
        self.set_error()
        self.connection = { 'type': None }

    def log(self, msg, end = '\n'):
        self.controller.console.log(msg, end)

    def debug(self, msg, end = '\n'):
        self.controller.console.debug(msg, end)

    def error(self, msg, end = '\n'):
        self.controller.console.error(msg, end)

    def exception(self):
        self.controller.console.exception()

    def has_error(self):
        return self.error_str!=None

    def set_error(self, error_msg = None):
        self.error_str = error_msg
        if error_msg:
            self.controller.connection_status(self)

    def get_error(self):
        return self.error_str

    def set_connected(self, conn_state, auth_state = False):
        self.connected = conn_state
        self.authenticated = auth_state
        self.controller.connection_status(self)

    def details(self):
        return self.conn()

    def conn(self):
        tmp = self.connection
        tmp['state'] = self.connected and self.authenticated and 'authenticated' or self.connected and 'connected' or 'disconnected'
        if self.has_error():
            tmp['error'] = self.get_error()
        return str(tmp)

    def on_connect(self):
        self.set_connected(True)
        self.debug('connected to %s' % self.conn())

    def on_auth(self):
        self.debug('authenticated')
        self.controller.connection_status(self)

    def on_disconnect(self):
        self.set_connected(False)
        self.debug('disconnected')

    def on_error(self, error):
        self.set_error(error)
        self.debug('error %s' % self.get_error())

    # raw message
    def on_message(self, data):
        try:
            self.debug('raw message auth=%u type=%s len=%u' % (self.authenticated, type(data), len(data)))

            if isinstance(data, bytes):
                self.on_binary(data)
            else:
                for msg in data.split('\n'):
                    msg = msg.rstrip()
                    if msg:
                        self.on_text(msg)
        except Exception as e:
            self.debug(e)
            raise e

    # binary message
    def on_binary(self, data):
        s = ''
        n = 0
        for b in data:
            s += '%02x ' % b
            n += 1
            if n>24:
                s += ' [...]'
                break
        self.debug('binary message %d: %s' % (len(data), s))

    # non empty single message right trimmed
    def on_text(self, msg):
        smsg = msg
        if len(smsg)>64:
            smsg = msg[0:63] + '[...]'
        self.debug('text message %d: %s' % (len(msg), smsg))

    def is_connected(self):
        return self.connected

    # alias for is_auth
    def is_authenticated(self):
        return self.is_auth()

    def is_auth(self):
        return self.connected and self.authenticated

    def connect(self, connection):
        self.set_connected(False)
        self.set_error()
        self.connection = connection
        self.debug('connecting to %s' % self.conn())

    # alias for disconnect
    def close(self):
        self.disconnect()

    def disconnect(self):
        self.debug('disconnecting from %s' % self.conn())
        self.set_connected(False)
