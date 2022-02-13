
import re
import requests
import json
import websocket
import _thread as thread
from . import Configuration


class OTASerialConsole(object):
    def __init__(self, hostname, sid):
        self.hostname = hostname
        self.sid = sid
        self.ws = websocket.WebSocketApp('ws://%s/serial-console' % hostname, on_message = self.before_auth, on_error = self.on_error, on_close = self.on_close, on_open = self.on_open)
        self.is_authenticated = False;
        self.is_connected = False
        self.is_closed = False
        def run(*args):
            self.ws.run_forever()
        thread.start_new_thread(run, ())

    def on_binary(self, data):
        pass

    def on_message(self, msg):
        if msg.startswith('+ATMODE_CMDS_HTTP2SERIAL='):
            return
        print('>%s' % msg, end='')
        if isinstance(msg, str):
            pass

    def before_auth(self, msg):
        # print('*>%s' % msg)
        if isinstance(msg, str):
            if not self.is_authenticated:
                if msg == '+AUTH_OK':
                    self.is_authenticated = True
                    self.ws.on_message = self.on_message
                elif msg == '+AUTH_ERROR':
                    self.is_authenticated = False
                    self.on_error('Authentication failed')
                elif msg == '+REQ_AUTH':
                    self.ws.send('+SID %s' % self.sid)

    def on_error(self, error):
        self.disconnect()
        print('Disconnected %s...' % error)
        self.is_closed = True

    def on_close(self):
        self.is_closed = True

    def on_open(self):
        print("Connection to %s established" % self.hostname)
        self.is_connected = True

    def disconnect(self):
        try:
            if self.is_connected or self.ws:
                self.ws.disconnect()
        except Exception as e:
            self.debug(e)
        self.ws = None
        self.is_closed = True


class OTAHelpers(object):

    def error(*arg):
        pass
    def verbose(*arg):
        pass

    def get_login_error(self, content):
        m = re.search("id=\"login_error_message\">([^<]+)</div", content, flags=re.IGNORECASE|re.DOTALL)
        if m:
            return m.group(1).strip()

    def get_div(self, title, content):
        m = re.search(">\s*" + title + "\s*<\/div>\s*<div[^>]*>([^<]+)<", content, flags=re.IGNORECASE|re.DOTALL)
        if m:
            return m.group(1).strip()

    def get_h3(self, content):
        return self.get_tag(content, 'h3');

    def get_tag(self, content, tag):
        m = re.search("<" + tag + "[^>]*>([^<]+)</" + tag, content, flags=re.IGNORECASE|re.DOTALL)
        if m:
            return m.group(1).strip()

    def is_alive(self, url, target):
        # verbose("Checking if device is alive "  + target)
        try:
            resp = requests.get(url + "is-alive", timeout=1)
            if resp.status_code==404:
                resp = requests.get(url + "is_alive", timeout=1)
            if resp.status_code!=200:
                return False
            elif resp.content.decode()!='0':
                return False
            return True
        except:
            return False

    def get_alive(self, url, target):
        self.verbose("Checking if device is alive "  + target)
        resp = requests.get(url + "is-alive", timeout=30)
        if resp.status_code!=200:
            self.error("Device did not respond", 2)
        elif resp.content.decode()!='0':
            self.error("Invalid response", 2)
        self.verbose("Device responded")

    def get_status(self, url, target, sid):
        self.verbose("Querying status "  + target)
        resp = requests.get(url + "status.html", data={"SID": sid}, timeout=30)
        if resp.status_code==200:
            content = resp.content.decode()
            login_error = self.get_login_error(content)
            if login_error!=None:
                self.error("Login failed: " + login_error + ". Check password or use --sha1 for old authentication method")
            software = self.get_div("Software", content)
            hardware = self.get_div("Hardware", content)
            if software!=None:
                self.verbose("Software: " + software)
            if software!=None:
                self.verbose("Hardware: " + hardware)
            if software==None and hardware==None:
                self.error("Could not get status from device, use --no-status to skip")
        elif resp.status_code==403:
            self.error("Access denied", 2)
        elif resp.status_code==404:
            self.error("Status page not found. Use -n to skip this check", 2)
        else:
            self.error("Invalid response code " + str(resp.status_code), 2)

    def import_settings(self, url, target, sid, file, params):
        if not self.is_alive(url, target):
            self.error("Device did not respond")
        try:
            settings = json.loads(file.read())
        except Exception as e:
            self.error("Failed to read " + file.name + ": " + str(e))
        cfg = Configuration()
        data = {}
        for handle in params:
            if handle[0:2]!="0x":
                handle = "0x{:04x}".format(cfg.get_handle("Config()." + handle))
            if handle in settings['config'].keys():
                data[handle] = settings['config'][handle]
            else:
                self.error("Cannot find setting: " + file.name)

        resp = requests.post(url + "import_settings", data={"SID": sid, "config": json.dumps({"config": data})}, timeout=30, allow_redirects=False)
        if resp.status_code!=200:
            self.error("Invalid response code " + str(resp.status_code))

        content = resp.content.decode()
        try:
            resp = json.loads(content)
        except:
            self.error("Invalid json response: " + content)

        if resp["count"]==0:
            self.error("No data imported")
        elif resp['count']>0:
            self.verbose(str(resp["count"]) + " configuration setting(s) imported")
            if resp['count']!=len(data):
                self.error("Settings count does not match: %u" % len(data))
        else:
            self.error("Failed to import: " + resp["message"])





