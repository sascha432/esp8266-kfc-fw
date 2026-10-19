# leak_isolate.py - free heap before/after 100 GETs of a small form page, 100 invalid POSTs to it and 100 GETs of
# a plain page (ESP8266 test device). Read-only: the POSTs lack every real field, the firmware discards them.
import re, sys, time, requests
HOST, USER, PW = '192.168.0.195', 'KFC43946D', '12345678'
s = requests.Session()
s.post('http://%s/index.html' % HOST, data={'username': USER, 'password': PW}, timeout=20)

def heap():
    for _ in range(3):
        r = s.get('http://%s/status.html' % HOST, timeout=20)
        m = re.search(r'Free RAM\s*([0-9.]+)\s*KB', re.sub(r'<[^>]*>', ' ', r.text))
        if m:
            return int(float(m.group(1)) * 1024)
        time.sleep(1)
    return None

def loop(label, fn, n=100):
    time.sleep(3)
    before = heap()
    errors = 0
    for i in range(n):
        try:
            fn()
        except requests.RequestException:
            errors += 1
    time.sleep(3)
    after = heap()
    print('%-40s before=%s after=%s delta=%+d per request=%+.1f errors=%u' % (label, before, after, after - before, (after - before) / n, errors))
    sys.stdout.flush()

form = 'http://%s/led-matrix/ani-solid.html' % HOST
loop('GET form (ani-solid)', lambda: s.get(form, timeout=20))
loop('POST invalid (1 pad field)', lambda: s.post(form, data={'__webui_test_pad_0': '1'}, timeout=20))
loop('POST invalid (14 fields, 955 B)', lambda: s.post(form, data={'__webui_test_pad_%u' % i: 'x' * 48 for i in range(14)}, timeout=20))
loop('GET plain page (about.html)', lambda: s.get('http://%s/about.html' % HOST, timeout=20))
loop('GET form (ani-solid) again', lambda: s.get(form, timeout=20))
