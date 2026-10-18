"""Test suite for the WebUI of a running KFC firmware device (pages, assets, API endpoints, forms, WebSockets).

The suite is read-only by design: it never saves a form, never triggers MQTT, never touches the LEDs, motors
or sound, never changes brightness/volume, WiFi, passwords or accounts. Everything that writes is either never
requested (NEVER_GET / NEVER_POST) or sent in a way that the firmware rejects before anything is applied:

    forms   Every settings form is posted without any of its real fields, plus padding fields up to the
            tested size (urlencoded, one big field, multipart). FormUI treats a missing field as an error
            ("This field is missing"), so validate() fails, the form is discarded with config.discard() and
            nothing is saved or reconfigured (web_server.cpp, _handleFileRead). The response is checked for
            the validation errors and the form values are compared before and after. A response that says
            "Applying Changes" means something was saved: the test fails and no further form is posted.

The sizes of the large forms are limited to 1/8 of the free heap of the device (status page) and
--max-form-size (8 KB). A build with the old ESPAsyncWebServer crashed with 16 KB bodies (heap corruption,
2026-10-18), and a crash reboots the device, which switches the LED power off and on. The forms group
stops at the first POST that fails. The device is checked for a reboot (uptime) and for the free heap after
every form and every group.

Tests that fail on a build with the old ESPAsyncWebServer without crashing it (robustness): body beyond
Content-Length (no answer), multipart boundary 100 characters (200 instead of 400), unmasked WebSocket
frame (accepted).

Groups (default: all except newlib and slow):
    auth        access without a session: login page, 403 for the API, public assets
    pages       crawls the menu and requests every page (status, content type, unresolved template variables)
    assets      the CSS/JS/images the pages reference
    api         JSON/text endpoints (/is-alive, /webui-handler, /savecrash.json, /export-settings, file manager)
    forms       the large invalid form POSTs described above
    websocket   /webui-ws and /serial-console: handshake, authentication, +get_values, ping, close
    robustness  HEAD, unknown method, long URL, many headers, %00 in the query, body beyond Content-Length,
                a too long multipart boundary, an unmasked WebSocket frame, parallel requests
    newlib      needs a firmware with the fixed ESPAsyncWebServer (older builds crash, so it is opt-in):
                a WebSocket frame header split across TCP segments, a multipart boundary > 255 characters
    slow        speed test download

Usage (workspace venv, see requirements.txt):
    .venv\\Scripts\\python scripts/tools/webui_test.py --env wled_esp32_controller_neopixelbus --env bme280_serial
    .venv\\Scripts\\python scripts/tools/webui_test.py --host 192.168.0.195 -u KFC43946D -p 12345678 --repeat 12
    .venv\\Scripts\\python scripts/tools/webui_test.py --env bme280_serial --groups forms --max-form-size 8192
    .venv\\Scripts\\python scripts/tools/webui_test.py --env bme280_serial --newlib --json logs/webui_test.json

The exit code is 1 if a test failed, 2 if the device was not reachable.
"""

import argparse
import base64
import concurrent.futures
import hashlib
import html.parser
import json
import os
import random
import re
import socket
import struct
import sys
import time
import urllib.parse

import requests

# test devices of the environments (the WebUI user name is the device name, KFC + the last 3 MAC bytes)
ENV_PRESETS = {
    'wled_esp32_controller_neopixelbus': {'host': '192.168.0.196', 'user': 'KFC0A4271'},
    'bme280_serial': {'host': '192.168.0.195', 'user': 'KFC43946D'},
}
DEFAULT_PASSWORD = '12345678'

ALL_GROUPS = ['auth', 'pages', 'assets', 'api', 'forms', 'websocket', 'robustness', 'newlib', 'slow']
DEFAULT_GROUPS = ['auth', 'pages', 'assets', 'api', 'forms', 'websocket', 'robustness']

# never requested: these change the device, its network, publish MQTT or trigger LED actions. The API
# group requests some of them explicitly in a read-only way (without arguments, GET instead of POST)
NEVER_GET = [re.compile(p) for p in (
    r'-arduino-ota',                     # starts/stops ArduinoOTA
    r'^/mqtt-publish-ad\.html',          # publishes MQTT auto discovery
    r'^/import-settings',                # writes the configuration
    r'^/logout',                         # ends the session, tested at the end of the auth group
    r'^/scan-wifi',                      # WiFi scan
    r'^/update',                         # firmware upload
    r'^/file_manager/(?!list|view|download)',  # mkdir, remove, rename, upload
    r'^/remote', r'sleep', r'^/dimmer-fw',     # deep sleep, dimmer firmware commands
    r'^/led-matrix-test\.json',          # saves and applies the matrix form (api: GET -> 405 only)
    r'^/ir-remote\.json',                # api: without arguments only (action=learn enables learning)
    r'^/savecrash\.json', r'^/coredump\.bin',  # api: without arguments only (cmd=erase-coredump)
    r'^/is-alive',                       # api: p only (reset-device restarts the device)
    r'^/speedtest\.',                    # slow group
    r'^/zeroconf', r'^/mdns_discovery',  # network queries
    r'^/webui-ws', r'^/serial-console$', # WebSockets, websocket group
)]

# forms that are never posted, not even the invalid POSTs of the forms group
NEVER_POST = re.compile(r'^/(wifi|network|password|mqtt|device|update-fw|reboot|factory|login|file-manager|'
                        r'serial-console|remote|remotectrl|export-settings|import-settings)\b')

APPLYING_MARKER = 'Applying Changes'
MISSING_MARKER = 'This field is missing'
VALIDATOR_MARKER = '$.formValidator.addErrors('
LOGIN_MARKER = 'name="keep"'  # the "keep me logged in" checkbox only exists on the login page
TEMPLATE_VAR = re.compile(r'%[A-Z][A-Z0-9_]{2,}%')
FREE_RAM = re.compile(r'Free RAM ([\d.]+)(B|KB|MB)')
UPTIME = re.compile(r'(?:(\d+) days? )?(\d\d)h:(\d\d)m:(\d\d)s')
WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'


class Abort(Exception):
    """the device is gone or a safety check failed, the remaining tests of the device are skipped"""


# --------------------------------------------------------------------------------------------------------
# results

class Results:

    def __init__(self, verbose):
        self.items = []
        self.verbose = verbose
        self.device = ''

    def add(self, group, name, status, detail='', duration=0.0):
        item = {'device': self.device, 'group': group, 'name': name, 'status': status, 'detail': detail,
                'duration': round(duration, 3)}
        self.items.append(item)
        if status != 'PASS' or self.verbose:
            print('  %-4s %-10s %-48s %s' % (status, group, name[:48], detail))
        return status == 'PASS'

    def count(self, status):
        return sum(1 for item in self.items if item['status'] == status)


# --------------------------------------------------------------------------------------------------------
# HTML parsing

class PageParser(html.parser.HTMLParser):
    """links, assets and the fields of the POST forms of a page"""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.links = []
        self.assets = []
        self.forms = []
        self._form = None
        self._select = None
        self._textarea = None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == 'a' and a.get('href'):
            self.links.append(a['href'])
        elif tag == 'link' and a.get('href'):
            self.assets.append(a['href'])
        elif tag in ('script', 'img') and a.get('src'):
            self.assets.append(a['src'])
        elif tag == 'form':
            self._form = {'method': (a.get('method') or 'get').lower(), 'action': a.get('action') or '',
                          'fields': [], 'names': set()}
            self.forms.append(self._form)
        elif self._form is None:
            return
        elif tag == 'input' and a.get('name'):
            kind = (a.get('type') or 'text').lower()
            self._form['names'].add(a['name'])
            if kind in ('checkbox', 'radio'):
                if 'checked' in a:
                    self._form['fields'].append((a['name'], a.get('value', 'on')))
            elif kind not in ('submit', 'button', 'reset', 'file', 'image'):
                self._form['fields'].append((a['name'], a.get('value', '')))
        elif tag == 'select' and a.get('name'):
            self._form['names'].add(a['name'])
            self._select = {'name': a['name'], 'value': None, 'first': None}
        elif tag == 'option' and self._select is not None:
            value = a.get('value', '')
            if self._select['first'] is None:
                self._select['first'] = value
            if 'selected' in a:
                self._select['value'] = value
        elif tag == 'textarea' and a.get('name'):
            self._form['names'].add(a['name'])
            self._textarea = [a['name'], '']

    def handle_data(self, data):
        if self._textarea is not None:
            self._textarea[1] += data

    def handle_endtag(self, tag):
        if tag == 'select' and self._select is not None:
            value = self._select['value'] if self._select['value'] is not None else self._select['first']
            self._form['fields'].append((self._select['name'], value or ''))
            self._select = None
        elif tag == 'textarea' and self._textarea is not None:
            self._form['fields'].append(tuple(self._textarea))
            self._textarea = None
        elif tag == 'form':
            self._form = None


def parse_page(text):
    parser = PageParser()
    parser.feed(text)
    return parser


def settings_forms(parser):
    """the FormUI forms of a page: POST to the page itself with named fields"""
    return [f for f in parser.forms if f['method'] == 'post' and f['names'] and 'keep' not in f['names']]


def size_from(value, unit):
    return int(float(value) * {'B': 1, 'KB': 1024, 'MB': 1024 * 1024}[unit])


# --------------------------------------------------------------------------------------------------------
# minimal WebSocket client: raw frames are needed for the split header and the unmasked frame tests

class WebSocket:

    def __init__(self, host, path, timeout):
        self.sock = socket.create_connection((host, 80), timeout=timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.buffer = b''
        key = base64.b64encode(os.urandom(16)).decode()
        request = ('GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                   'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n' % (path, host, key))
        self.sock.sendall(request.encode())
        while b'\r\n\r\n' not in self.buffer:
            data = self.sock.recv(4096)
            if not data:
                raise ConnectionError('connection closed during the handshake')
            self.buffer += data
        head, self.buffer = self.buffer.split(b'\r\n\r\n', 1)
        lines = head.decode('latin1').split('\r\n')
        self.status = int(lines[0].split(' ')[1])
        headers = {k.strip().lower(): v.strip() for k, v in (l.split(':', 1) for l in lines[1:] if ':' in l)}
        expected = base64.b64encode(hashlib.sha1((key + WS_GUID).encode()).digest()).decode()
        self.accept_ok = headers.get('sec-websocket-accept') == expected

    @staticmethod
    def frame(opcode, payload, mask=True, fin=True):
        if isinstance(payload, str):
            payload = payload.encode()
        head = bytes([(0x80 if fin else 0) | opcode])
        length = len(payload)
        mbit = 0x80 if mask else 0
        if length < 126:
            head += bytes([mbit | length])
        elif length < 65536:
            head += bytes([mbit | 126]) + struct.pack('>H', length)
        else:
            head += bytes([mbit | 127]) + struct.pack('>Q', length)
        if mask:
            key = os.urandom(4)
            payload = bytes(b ^ key[i % 4] for i, b in enumerate(payload))
            head += key
        return head + payload

    def send(self, opcode, payload, mask=True, piece=0, delay=0.0):
        data = self.frame(opcode, payload, mask)
        if not piece:
            self.sock.sendall(data)
            return
        # one TCP segment per piece (TCP_NODELAY), the device sees the frame header in several parts
        for pos in range(0, len(data), piece):
            self.sock.sendall(data[pos:pos + piece])
            time.sleep(delay)

    def recv(self, timeout):
        """next frame (opcode, payload) or None on timeout"""
        end = time.time() + timeout
        while True:
            frame = self._parse()
            if frame:
                return frame
            remaining = end - time.time()
            if remaining <= 0:
                return None
            self.sock.settimeout(remaining)
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                return None
            if not data:
                return (-1, b'')  # closed
            self.buffer += data

    def _parse(self):
        b = self.buffer
        if len(b) < 2:
            return None
        opcode = b[0] & 0x0F
        length = b[1] & 0x7F
        pos = 2
        if length == 126:
            if len(b) < 4:
                return None
            length = struct.unpack('>H', b[2:4])[0]
            pos = 4
        elif length == 127:
            if len(b) < 10:
                return None
            length = struct.unpack('>Q', b[2:10])[0]
            pos = 10
        if len(b) < pos + length:
            return None
        self.buffer = b[pos + length:]
        return opcode, b[pos:pos + length]

    def recv_text(self, predicate, timeout):
        """first text message the predicate accepts, None on timeout or close"""
        end = time.time() + timeout
        while time.time() < end:
            frame = self.recv(end - time.time())
            if frame is None or frame[0] in (-1, 8):
                return None
            if frame[0] == 1:
                text = frame[1].decode('utf8', 'replace')
                if predicate(text):
                    return text
        return None

    def close(self, code=1000):
        try:
            self.send(8, struct.pack('>H', code))
            self.recv(2)
        except OSError:
            pass
        self.sock.close()


# --------------------------------------------------------------------------------------------------------
# device

class Device:

    def __init__(self, args, host, user, password, results):
        self.args = args
        self.host = host
        self.user = user
        self.password = password
        self.results = results
        self.base = 'http://%s' % host
        self.session = requests.Session()
        self.session.headers['User-Agent'] = 'kfc_fw webui_test'
        self.sid = None
        self.pages = {}       # path -> parsed page
        self.assets = set()
        self.health = None    # (uptime seconds, free heap)
        self.uptime_base = None
        self.heap_base = None
        self.forms_stopped = False

    # ---------------------------------------------------------------------------------------- helpers

    def url(self, path):
        return urllib.parse.urljoin(self.base, path)

    def get(self, path, session=None, **kwargs):
        kwargs.setdefault('timeout', self.args.timeout)
        return (session or self.session).get(self.url(path), **kwargs)

    def post(self, path, **kwargs):
        kwargs.setdefault('timeout', self.args.timeout)
        return self.session.post(self.url(path), **kwargs)

    def check(self, group, name, func):
        """runs one test, an exception is a failure; Abort stops the device"""
        start = time.time()
        try:
            status, detail = func()
        except Abort:
            raise
        except (requests.RequestException, OSError) as e:
            status, detail = 'FAIL', '%s: %s' % (type(e).__name__, e)
            if not self.alive():
                self.results.add(group, name, status, detail, time.time() - start)
                raise Abort('device not reachable after %s' % name)
        except AssertionError as e:
            status, detail = 'FAIL', str(e)
        return self.results.add(group, name, status, detail, time.time() - start)

    def alive(self, wait=30):
        end = time.time() + wait
        while time.time() < end:
            try:
                r = requests.get(self.url('/is-alive?p=1'), timeout=3)
                if r.status_code == 200:
                    return True
            except requests.RequestException:
                pass
            time.sleep(1)
        return False

    def read_health(self):
        r = self.get('/status.html')
        assert r.status_code == 200, 'status page: %u' % r.status_code
        m = UPTIME.search(r.text)
        uptime = None
        if m:
            uptime = int(m.group(1) or 0) * 86400 + int(m.group(2)) * 3600 + int(m.group(3)) * 60 + int(m.group(4))
        m = FREE_RAM.search(r.text)
        heap = size_from(m.group(1), m.group(2)) if m else None
        return uptime, heap

    def health_check(self, label):
        """fails and aborts if the device rebooted, warns if the free heap dropped"""
        def run():
            uptime, heap = self.read_health()
            prev = self.health
            self.health = (uptime, heap)
            if prev and uptime is not None and prev[0] is not None and uptime < prev[0]:
                self.results.add('health', 'reboot detected ' + label, 'FAIL', 'uptime %us -> %us' % (prev[0], uptime))
                raise Abort('the device rebooted during %s' % label)
            detail = 'uptime %ss, free heap %s' % (uptime, heap)
            if heap and self.heap_base and heap < self.heap_base - self.args.heap_tolerance:
                return 'WARN', detail + ' (%d bytes below the start)' % (self.heap_base - heap)
            return 'PASS', detail
        self.check('health', 'after ' + label, run)

    def login(self):
        r = self.post('/index.html', data={'username': self.user, 'password': self.password},
                      allow_redirects=False)
        self.sid = self.session.cookies.get('SID')
        if not self.sid:
            raise Abort('login failed (HTTP %u, no SID cookie) - check user and password' % r.status_code)
        return r

    # ---------------------------------------------------------------------------------------- groups

    def run(self, groups):
        print('%s (%s)' % (self.host, self.user))
        self.results.device = self.host
        if not self.alive(wait=5):
            self.results.add('health', 'device reachable', 'FAIL', 'no answer from /is-alive')
            return False
        try:
            self.login()
            self.health = self.read_health()
            self.uptime_base, self.heap_base = self.health
            self.results.add('health', 'start', 'PASS', 'uptime %ss, free heap %s' % self.health)
            for group in ['auth', 'pages', 'assets', 'api', 'forms', 'websocket', 'robustness', 'newlib', 'slow']:
                if group not in groups:
                    continue
                if group in ('assets', 'forms', 'robustness', 'newlib') and not self.pages:
                    self.crawl(record=False)
                getattr(self, 'group_' + group)()
                self.health_check(group)
            self.check('auth', 'logout removes the session', self.test_logout)
        except Abort as e:
            self.results.add('health', 'aborted', 'FAIL', str(e))
            return False
        return True

    # ---------------------------------------------------------------------------------------- auth

    def group_auth(self):
        anon = requests.Session()
        anon.headers['User-Agent'] = 'kfc_fw webui_test (anonymous)'

        def login_page():
            r = self.get('/status.html', session=anon)
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            assert LOGIN_MARKER in r.text, 'no login form without a session'
            assert 'Free RAM' not in r.text, 'the status page is served without a session'
            return 'PASS', ''
        self.check('auth', 'protected page shows the login form', login_page)

        for path in ('/savecrash.json', '/export-settings', '/webui-handler', '/sync-time', '/file_manager/list?dir=/'):
            def forbidden(path=path):
                r = self.get(path, session=anon)
                assert r.status_code in (401, 403), 'HTTP %u' % r.status_code
                return 'PASS', 'HTTP %u' % r.status_code
            self.check('auth', 'no session: ' + path, forbidden)

        def public_asset():
            r = self.get('/login.html', session=anon)
            assets = [a for a in parse_page(r.text).assets if a.endswith(('.css', '.js'))]
            if not assets:
                return 'SKIP', 'the login page references no CSS/JS'
            r = self.get(assets[0], session=anon)
            assert r.status_code == 200, '%s: HTTP %u' % (assets[0], r.status_code)
            return 'PASS', assets[0]
        self.check('auth', 'public assets without a session', public_asset)

        def is_alive():
            value = str(random.randint(1, 1000000))
            r = self.get('/is-alive?p=' + value, session=anon)
            assert r.status_code == 200 and r.text.strip() == value, 'HTTP %u %r' % (r.status_code, r.text[:40])
            return 'PASS', ''
        self.check('auth', '/is-alive without a session', is_alive)

    def test_logout(self):
        r = self.get('/logout', allow_redirects=False)
        assert r.status_code in (301, 302, 303, 307), 'HTTP %u' % r.status_code
        self.session.cookies.clear()
        r = self.get('/status.html')
        assert LOGIN_MARKER in r.text, 'still logged in after /logout'
        return 'PASS', ''

    # ---------------------------------------------------------------------------------------- pages

    def crawl(self, record=True):
        queue = ['/', '/status.html', '/index.html']
        seen = set()
        skipped = set()
        while queue and len(seen) < self.args.max_pages:
            path = queue.pop(0)
            if path in seen:
                continue
            seen.add(path)
            if any(p.search(path) for p in NEVER_GET):
                skipped.add(path)
                continue

            def page(path=path):
                start = time.time()
                r = self.get(path)
                assert r.status_code == 200, 'HTTP %u' % r.status_code
                ctype = r.headers.get('Content-Type', '')
                assert 'text/html' in ctype, 'Content-Type %s' % ctype
                assert LOGIN_MARKER not in r.text, 'login form instead of the page (session lost)'
                parsed = parse_page(r.text)
                self.pages[path] = {'parser': parsed, 'text': r.text}
                for href in parsed.links:
                    target = self.normalize(path, href)
                    if target and target not in seen:
                        queue.append(target)
                for src in parsed.assets:
                    target = self.normalize(path, src, assets=True)
                    if target:
                        self.assets.add(target)
                leftovers = sorted(set(TEMPLATE_VAR.findall(r.text)))
                detail = '%u bytes, %.0f ms' % (len(r.content), (time.time() - start) * 1000)
                if leftovers:
                    return 'WARN', detail + ', unresolved template variables: ' + ' '.join(leftovers[:5])
                return 'PASS', detail
            if record:
                self.check('pages', path, page)
            else:
                try:
                    page()
                except (AssertionError, requests.RequestException):
                    pass
        if record:
            self.results.add('pages', 'skipped (NEVER_GET)', 'PASS', ' '.join(sorted(skipped)))

    def normalize(self, base, href, assets=False):
        """same-device path of a link, None for external links, queries and anchors"""
        if not href or href.startswith(('#', 'javascript:', 'mailto:', 'data:')):
            return None
        url = urllib.parse.urlparse(urllib.parse.urljoin(self.url(base), href))
        if url.netloc and url.netloc != self.host:
            return None
        if url.query and not assets:
            return None  # links with arguments are commands (dimmer-fw?..., remote actions)
        path = url.path or '/'
        if not assets and not (path.endswith(('.html', '/'))):
            return None
        return path

    def group_pages(self):
        self.crawl(record=True)
        self.check('pages', 'menu pages found', lambda: ('PASS' if len(self.pages) >= 5 else 'FAIL',
                                                         '%u pages' % len(self.pages)))

    # ---------------------------------------------------------------------------------------- assets

    def group_assets(self):
        for path in sorted(self.assets):
            if any(p.search(path) for p in NEVER_GET):
                continue

            def asset(path=path):
                r = self.get(path)
                assert r.status_code == 200, 'HTTP %u' % r.status_code
                detail = '%u bytes %s' % (len(r.content), r.headers.get('Content-Type', ''))
                if path.endswith(('.js', '.css')) and r.headers.get('Content-Encoding') != 'gzip':
                    return 'WARN', detail + ', not gzip encoded'
                return 'PASS', detail
            self.check('assets', path, asset)

    # ---------------------------------------------------------------------------------------- api

    def group_api(self):
        def is_alive():
            value = str(random.randint(1, 1000000))
            r = self.get('/is-alive?p=' + value)
            assert r.text.strip() == value, repr(r.text[:40])
            return 'PASS', ''
        self.check('api', '/is-alive', is_alive)

        def json_endpoint(path, allow=()):
            def run():
                r = self.get(path)
                if r.status_code in allow:
                    return 'SKIP', 'HTTP %u' % r.status_code
                assert r.status_code == 200, 'HTTP %u' % r.status_code
                data = json.loads(r.text)  # raises on invalid JSON
                return 'PASS', '%u bytes, %s' % (len(r.content), type(data).__name__)
            return run
        self.check('api', '/webui-handler', json_endpoint('/webui-handler', allow=(503,)))
        self.check('api', '/savecrash.json', json_endpoint('/savecrash.json'))
        # contains the passwords: parsed only, never printed or stored
        self.check('api', '/export-settings', json_endpoint('/export-settings'))

        def sync_time():
            r = self.get('/sync-time')
            assert r.status_code == 200 and r.text.strip(), 'HTTP %u' % r.status_code
            return 'PASS', ''
        self.check('api', '/sync-time', sync_time)

        def alerts():
            r = self.get('/alerts')
            assert r.status_code == 503, 'HTTP %u (alerts are not compiled in)' % r.status_code
            return 'PASS', ''
        self.check('api', '/alerts', alerts)

        def coredump():
            r = self.get('/coredump.bin', stream=True)
            if r.status_code == 404:
                r.close()
                return 'PASS', 'no core dump (or ESP8266)'
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            size = sum(len(chunk) for chunk in r.iter_content(4096))
            return 'PASS', '%u bytes' % size
        self.check('api', '/coredump.bin (read only)', coredump)

        def led_matrix_test():
            r = self.get('/led-matrix-test.json')
            if r.status_code == 404:
                return 'SKIP', 'no LED matrix'
            assert r.status_code == 405, 'GET must be rejected with 405, got %u' % r.status_code
            return 'PASS', ''
        self.check('api', '/led-matrix-test.json GET -> 405', led_matrix_test)

        def ir_remote():
            r = self.get('/ir-remote.json')
            if r.status_code == 404:
                return 'SKIP', 'no IR receiver'
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            data = json.loads(r.text)
            assert data.get('learn') == 0, 'learn mode is active: %s' % r.text
            return 'PASS', ''
        self.check('api', '/ir-remote.json (status only)', ir_remote)

        listing = []

        def file_list():
            r = self.get('/file_manager/list?dir=/')
            if r.status_code == 404:
                return 'SKIP', 'no file manager'
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            # {"t":..., "f":[{"f":"<path>","n":"<name>","b":<bytes>,"d":0|1,...}]}
            data = json.loads(r.text)
            listing.extend(e for e in data.get('f', []) if isinstance(e, dict))
            return 'PASS', '%u entries' % len(listing)
        self.check('api', '/file_manager/list', file_list)

        small = [e for e in listing if e.get('d') == 0 and e.get('f') and int(e.get('b', 0)) < 16384]
        if small:
            def download(entry=small[0]):
                name = entry['f']
                r = self.get('/file_manager/download?filename=' + urllib.parse.quote(name))
                assert r.status_code == 200, 'HTTP %u' % r.status_code
                return 'PASS', '%s %u bytes' % (name, len(r.content))
            self.check('api', '/file_manager/download', download)

        def description_xml():
            r = self.get('/description.xml')
            if r.status_code == 404:
                return 'SKIP', 'no SSDP'
            assert r.status_code == 200 and '<root' in r.text, 'HTTP %u' % r.status_code
            return 'PASS', ''
        self.check('api', '/description.xml', description_xml)

        def not_found():
            r = self.get('/does-not-exist-%u.html' % random.randint(1, 100000))
            assert r.status_code == 404, 'HTTP %u' % r.status_code
            return 'PASS', ''
        self.check('api', 'unknown page -> 404', not_found)

        def hidden_file():
            r = self.get('/.message.html')
            assert r.status_code == 404, 'hidden file served: HTTP %u' % r.status_code
            return 'PASS', ''
        self.check('api', 'hidden file -> 404', hidden_file)

    # ---------------------------------------------------------------------------------------- forms

    def form_sizes(self):
        """sizes of the large forms, limited by the free heap"""
        heap = self.health[1] or 0
        # the request keeps the body several times (parser buffer, decoded parameter, form rendering) and
        # the largest free block is smaller than the free heap: 16 KB crashed the ESP32 with 95 KB free
        limit = min(self.args.max_form_size, heap // 8) if heap else self.args.max_form_size
        sizes = [s for s in (1024, 4096, 16384, 32768, 65536) if s <= limit]
        return sizes or [512], limit

    def group_forms(self):
        sizes, limit = self.form_sizes()
        self.results.add('forms', 'form sizes', 'PASS', '%s (limit %u bytes, free heap %s)' % (sizes, limit, self.health[1]))
        targets = []
        for path, page in sorted(self.pages.items()):
            if NEVER_POST.search(path):
                continue
            forms = settings_forms(page['parser'])
            if forms:
                targets.append((path, forms[0]))
        if not targets:
            self.results.add('forms', 'settings forms found', 'FAIL', 'no settings form on the crawled pages')
            return
        for path, form in targets:
            if self.forms_stopped:
                self.results.add('forms', 'stopped', 'FAIL', 'a POST failed, the remaining forms are skipped')
                break
            self.test_form(path, form, sizes)
            self.health_check('form ' + path)  # a reboot is caught right after the form that caused it

    def snapshot(self, path):
        r = self.get(path)
        assert r.status_code == 200, 'HTTP %u' % r.status_code
        forms = settings_forms(parse_page(r.text))
        assert forms, 'the form is gone'
        return sorted(forms[0]['fields'])

    def invalid_post(self, path, fields, multipart=False):
        """POST without any real field: validate() fails, nothing is saved"""
        start = time.time()
        try:
            if multipart:
                r = self.post(path, files={name: (None, value) for name, value in fields})
            else:
                r = self.post(path, data=fields)
        except requests.RequestException:
            # the device dropped the connection: no further form is posted, the health check follows
            self.forms_stopped = True
            raise
        elapsed = (time.time() - start) * 1000
        if APPLYING_MARKER in r.text:
            self.forms_stopped = True
            raise AssertionError('the form was SAVED (Applying Changes) - no further form is posted')
        assert r.status_code == 200, 'HTTP %u' % r.status_code
        assert VALIDATOR_MARKER in r.text and MISSING_MARKER in r.text, 'no validation errors in the response'
        return elapsed

    def test_form(self, path, form, sizes):
        action = form['action'] if form['action'] and not form['action'].startswith('%') else path
        target = self.normalize(path, action) or path
        try:
            before = self.snapshot(path)
        except (AssertionError, requests.RequestException) as e:
            self.results.add('forms', path, 'FAIL', 'snapshot: %s' % e)
            return
        pad = lambda i: '__webui_test_pad_%u' % i
        # every parameter costs ~160 byte heap on the device (name, value, list node), more than its body size
        max_fields = max(1, self.form_sizes()[1] // 160)

        variants = [('empty', [(pad(0), '1')], False)]
        for size in sizes:
            count = max(1, min(size // 64, max_fields))
            variants.append(('%u fields/%uB' % (count, size), [(pad(i), 'x' * 48) for i in range(count)], False))
            variants.append(('1 field/%uB' % size, [(pad(0), 'y' * size)], False))
        mp_size = sizes[min(1, len(sizes) - 1)]
        variants.append(('multipart %uB' % mp_size, [(pad(i), 'z' * 48) for i in range(max(1, min(mp_size // 64, max_fields)))], True))

        for label, fields, multipart in variants:
            if self.forms_stopped:
                return
            self.check('forms', '%s %s' % (target, label),
                       lambda f=fields, m=multipart: ('PASS', '%.0f ms' % self.invalid_post(target, f, m)))

        def unchanged():
            after = self.snapshot(path)
            changed = [n for n, v in set(before) ^ set(after)]
            assert not changed, 'values changed: %s' % ', '.join(sorted(set(changed))[:8])
            return 'PASS', '%u fields' % len(before)
        self.check('forms', '%s values unchanged' % path, unchanged)

    # ---------------------------------------------------------------------------------------- websocket

    def ws(self, path):
        ws = WebSocket(self.host, path, self.args.timeout)
        if ws.status == 404:
            ws.sock.close()
            return None
        assert ws.status == 101, 'handshake: HTTP %u' % ws.status
        assert ws.accept_ok, 'wrong Sec-WebSocket-Accept'
        return ws

    def ws_auth(self, ws):
        ws.send(1, '+SID ' + self.sid)
        text = ws.recv_text(lambda t: t.startswith('+AUTH'), 5)
        assert text == '+AUTH_OK', 'authentication: %r' % text

    def group_websocket(self):
        def webui():
            ws = self.ws('/webui-ws')
            if ws is None:
                return 'SKIP', 'no /webui-ws'
            try:
                self.ws_auth(ws)
                ws.send(1, '+get_values')  # read only, +set/+set_state are never sent
                text = ws.recv_text(lambda t: True, 5)
                assert text is not None, 'no answer to +get_values'
                ws.send(1, '+iPING 12.5')
                pong = ws.recv_text(lambda t: t.startswith('+iPONG'), 5)
                assert pong, 'no +iPONG'
                ws.send(9, b'webui_test')  # protocol ping
                frame = ws.recv(5)
                while frame and frame[0] == 1:
                    frame = ws.recv(5)
                assert frame and frame[0] == 10 and frame[1] == b'webui_test', 'no pong: %r' % (frame,)
                ws.send(8, struct.pack('>H', 1000))
                frame = ws.recv(5)
                while frame and frame[0] == 1:
                    frame = ws.recv(5)
                assert frame and frame[0] in (8, -1), 'no close frame: %r' % (frame,)
            finally:
                ws.sock.close()
            return 'PASS', ''
        self.check('websocket', '/webui-ws auth, get_values, ping, close', webui)

        def console():
            ws = self.ws('/serial-console')
            if ws is None:
                return 'SKIP', 'no /serial-console'
            try:
                self.ws_auth(ws)  # nothing is sent to the console, it would reach the AT command parser
                ws.recv(1)
            finally:
                ws.close()
            return 'PASS', ''
        self.check('websocket', '/serial-console auth', console)

    # ---------------------------------------------------------------------------------------- robustness

    def raw(self, data, timeout=None):
        """sends raw bytes, returns the status code of the response (None without a response)"""
        sock = socket.create_connection((self.host, 80), timeout=timeout or self.args.timeout)
        try:
            sock.sendall(data)
            response = b''
            while b'\r\n' not in response:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
        finally:
            sock.close()
        line = response.split(b'\r\n', 1)[0].decode('latin1')
        return int(line.split(' ')[1]) if line.startswith('HTTP/') else None

    def cookie(self):
        return 'Cookie: SID=%s\r\n' % self.sid

    def postable_form(self):
        for path, page in sorted(self.pages.items()):
            if not NEVER_POST.search(path) and settings_forms(page['parser']):
                return path
        return None

    def multipart_boundary(self, length):
        path = self.postable_form()
        if not path:
            return 'SKIP', 'no settings form'
        boundary = 'b' * length
        body = ('--%s\r\nContent-Disposition: form-data; name="__webui_test_pad_0"\r\n\r\n1\r\n--%s--\r\n'
                % (boundary, boundary))
        r = self.post(path, data=body.encode(), headers={'Content-Type': 'multipart/form-data; boundary=' + boundary})
        assert APPLYING_MARKER not in r.text, 'the form was saved'
        assert r.status_code == 400, 'HTTP %u (expected 400 for a %u character boundary)' % (r.status_code, length)
        return 'PASS', ''

    def group_robustness(self):
        def head():
            r = self.session.head(self.url('/status.html'), timeout=self.args.timeout)
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            return 'PASS', ''
        self.check('robustness', 'HEAD /status.html', head)

        def unknown_method():
            status = self.raw(('PROPFIND /status.html HTTP/1.1\r\nHost: %s\r\n%sConnection: close\r\n\r\n'
                               % (self.host, self.cookie())).encode())
            assert status == 501, 'HTTP %s (expected 501)' % status
            return 'PASS', ''
        self.check('robustness', 'unknown method -> 501', unknown_method)

        def long_url():
            r = self.get('/' + 'a' * 1500 + '.html')
            assert r.status_code in (404, 414, 431), 'HTTP %u' % r.status_code
            return 'PASS', 'HTTP %u' % r.status_code
        self.check('robustness', 'long URL', long_url)

        def many_headers():
            headers = {'X-Webui-Test-%u' % i: 'v' * 32 for i in range(40)}
            r = self.get('/status.html', headers=headers)
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            return 'PASS', ''
        self.check('robustness', '40 extra headers', many_headers)

        def header_without_colon():
            status = self.raw(('GET /is-alive?p=7 HTTP/1.1\r\nHost: %s\r\nBrokenHeaderLine\r\nX-Short:x\r\n'
                               'Connection: close\r\n\r\n' % self.host).encode())
            assert status == 200, 'HTTP %s' % status
            return 'PASS', ''
        self.check('robustness', 'header line without colon', header_without_colon)

        def nul_in_query():
            r = self.get('/is-alive?p=5%00x&q=%zz&r=%')
            assert r.status_code == 200 and r.text.strip() == '5', 'HTTP %u %r' % (r.status_code, r.text[:20])
            return 'PASS', ''
        self.check('robustness', '%00 and invalid escapes in the query', nul_in_query)

        def body_beyond_length():
            path = self.postable_form()
            if not path:
                return 'SKIP', 'no settings form'
            body = b'__webui_test_pad_0=1' + b'&__webui_test_pad_1=' + b'g' * 400
            head = ('POST %s HTTP/1.1\r\nHost: %s\r\n%sContent-Type: application/x-www-form-urlencoded\r\n'
                    'Content-Length: 20\r\nConnection: close\r\n\r\n' % (path, self.host, self.cookie()))
            status = self.raw(head.encode() + body)
            assert status == 200, 'HTTP %s' % status
            return 'PASS', ''
        self.check('robustness', 'body beyond Content-Length', body_beyond_length)

        self.check('robustness', 'multipart boundary 100 chars -> 400', lambda: self.multipart_boundary(100))

        def unmasked_frame():
            ws = self.ws('/webui-ws')
            if ws is None:
                return 'SKIP', 'no /webui-ws'
            try:
                ws.send(1, '+iPING 1', mask=False)
                frame = ws.recv(5)
                while frame and frame[0] == 1 and not frame[1].startswith(b'+iPONG'):
                    frame = ws.recv(5)
                assert frame and frame[0] == 8, 'unmasked client frame accepted: %r' % (frame,)
                code = struct.unpack('>H', frame[1][:2])[0] if len(frame[1]) >= 2 else 0
                assert code == 1002, 'close code %u (expected 1002)' % code
            finally:
                ws.sock.close()
            return 'PASS', ''
        self.check('robustness', 'unmasked WebSocket frame -> close 1002', unmasked_frame)

        def parallel():
            with concurrent.futures.ThreadPoolExecutor(max_workers=self.args.parallel) as pool:
                codes = list(pool.map(lambda _: self.get('/status.html').status_code, range(self.args.parallel * 2)))
            assert all(c == 200 for c in codes), 'HTTP %s' % codes
            return 'PASS', '%u requests, %u parallel' % (len(codes), self.args.parallel)
        self.check('robustness', 'parallel requests', parallel)

    # ---------------------------------------------------------------------------------------- newlib

    def group_newlib(self):
        def split_header():
            ws = self.ws('/webui-ws')
            if ws is None:
                return 'SKIP', 'no /webui-ws'
            try:
                ws.send(1, '+iPING 3', piece=1, delay=0.05)  # one byte per TCP segment
                pong = ws.recv_text(lambda t: t.startswith('+iPONG'), 5)
                assert pong, 'no +iPONG for a frame split into 1 byte segments'
            finally:
                ws.close()
            return 'PASS', ''
        self.check('newlib', 'WebSocket header split across segments', split_header)
        self.check('newlib', 'multipart boundary 300 chars -> 400', lambda: self.multipart_boundary(300))

    # ---------------------------------------------------------------------------------------- slow

    def group_slow(self):
        def speedtest():
            start = time.time()
            r = self.get('/speedtest.zip', stream=True)
            if r.status_code == 404:
                r.close()
                return 'SKIP', 'no speed test'
            assert r.status_code == 200, 'HTTP %u' % r.status_code
            size = sum(len(chunk) for chunk in r.iter_content(8192))
            elapsed = time.time() - start
            return 'PASS', '%u bytes, %.1f KB/s' % (size, size / 1024 / max(elapsed, 0.001))
        self.check('slow', '/speedtest.zip', speedtest)


# --------------------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='Read-only WebUI test suite of a KFC firmware device',
                                     formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument('--env', action='append', default=[], choices=sorted(ENV_PRESETS),
                        help='test device of an environment (can be repeated)')
    parser.add_argument('--host', help='device address (instead of --env)')
    parser.add_argument('-u', '--user', help='WebUI user = device name, e.g. KFC43946D')
    parser.add_argument('-p', '--password', default=DEFAULT_PASSWORD)
    parser.add_argument('--groups', default=','.join(DEFAULT_GROUPS),
                        help='comma separated, from: %s' % ', '.join(ALL_GROUPS))
    parser.add_argument('--newlib', action='store_true', help='add the newlib group (fixed ESPAsyncWebServer only)')
    parser.add_argument('--slow', action='store_true', help='add the slow group')
    parser.add_argument('--repeat', type=int, default=1, help='runs of the whole suite (intermittent failures)')
    parser.add_argument('--max-form-size', type=int, default=8192,
                        help='largest form body in bytes (also limited to 1/8 of the free heap)')
    parser.add_argument('--heap-tolerance', type=int, default=4096, help='free heap drop that is reported')
    parser.add_argument('--max-pages', type=int, default=120)
    parser.add_argument('--parallel', type=int, default=3, help='parallel requests of the robustness group')
    parser.add_argument('--timeout', type=float, default=20)
    parser.add_argument('--json', help='write the results to this file')
    parser.add_argument('-v', '--verbose', action='store_true', help='print the passed tests as well')
    args = parser.parse_args()

    groups = [g.strip() for g in args.groups.split(',') if g.strip()]
    unknown = [g for g in groups if g not in ALL_GROUPS]
    if unknown:
        parser.error('unknown group: %s' % ', '.join(unknown))
    if args.newlib:
        groups.append('newlib')
    if args.slow:
        groups.append('slow')

    devices = [(ENV_PRESETS[e]['host'], ENV_PRESETS[e]['user'], e) for e in args.env]
    if args.host:
        if not args.user:
            parser.error('--host needs --user (the device name)')
        devices.append((args.host, args.user, args.host))
    if not devices:
        parser.error('--env or --host is required')

    results = Results(args.verbose)
    reachable = True
    for run in range(args.repeat):
        for host, user, label in devices:
            print('== run %u/%u: %s' % (run + 1, args.repeat, label))
            device = Device(args, host, user, args.password, results)
            reachable = device.run(groups) and reachable

    passed, failed, warned, skipped = (results.count(s) for s in ('PASS', 'FAIL', 'WARN', 'SKIP'))
    print('== %u passed, %u failed, %u warnings, %u skipped' % (passed, failed, warned, skipped))
    if args.json:
        os.makedirs(os.path.dirname(os.path.abspath(args.json)), exist_ok=True)
        with open(args.json, 'w', encoding='utf8') as f:
            json.dump(results.items, f, indent=1)
    if not reachable and not failed:
        return 2
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
