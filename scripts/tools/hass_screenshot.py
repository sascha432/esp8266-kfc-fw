"""
Grab a screenshot of the LVGL screen from the device (debug builds only, DEBUG_LVGL_SCREENSHOT).

The firmware serves the active screen as a 24 bit BMP at /lvgl-screen.bmp; the endpoint requires
an authenticated WebUI session. This tool builds the session id the same way as the WebUI
(scripts/libs/kfcfw/session.py), downloads the BMP and converts it to PNG for inspection.

  python hass_screenshot.py --host 192.168.0.199 -u user -p password -o shot.png
  python hass_screenshot.py --host 192.168.0.199 -u user -p password --screen HASS

Options:
  --screen <index|name>   show that screen before the capture (for example HASS)
  --set <key:value;...>   push values into the application before the capture
  --backlight <0-100>     set the backlight before the capture
"""

import argparse
import hashlib
import io
import os
import secrets
import sys

import requests
from PIL import Image


def generate_session_id(username, password, sha1=False, rounds=1024):
    """session id of the WebUI, mirrors scripts/libs/kfcfw/session.py and session.cpp"""
    algorithm = hashlib.sha1 if sha1 else hashlib.sha256
    salt = secrets.token_bytes(8)
    digest = algorithm(salt + password.encode('utf8') + username.encode('utf8')).digest()
    for _ in range(rounds):
        digest = algorithm(digest + salt).digest()
    return salt.hex() + digest.hex()


def main():
    parser = argparse.ArgumentParser(description='Screenshot of the LVGL screen (BMP to PNG)')
    parser.add_argument('--host', required=True, help='device hostname or IP')
    parser.add_argument('-P', '--port', type=int, default=80, help='web server port (default 80)')
    parser.add_argument('-u', '--user', help='web server user name')
    parser.add_argument('-p', '--pw', '--pass', dest='pw', help='web server password')
    parser.add_argument('--sid', help='session id, skips the login (see the WebUI cookie)')
    parser.add_argument('--sha1', action='store_true', help='use sha1 authentication')
    parser.add_argument('-s', '--secure', action='store_true', help='use https')
    parser.add_argument('--screen', help='screen index or name shown before the capture (HASS, MAIN, ...)')
    parser.add_argument('--set', dest='values', help='values pushed before the capture, "key:value;key:value"')
    parser.add_argument('--backlight', type=int, help='backlight 0-100 before the capture')
    parser.add_argument('-o', '--output', default='screen.png', help='output file (default screen.png)')
    parser.add_argument('--timeout', type=int, default=30, help='timeout in seconds (default 30)')
    args = parser.parse_args()

    if args.sid:
        sid = args.sid
    elif args.user and args.pw is not None:
        sid = generate_session_id(args.user, args.pw, args.sha1)
    else:
        print('--user and --pw are required, or pass --sid')
        return 1

    url = ('https' if args.secure else 'http') + '://' + args.host
    if args.port and args.port not in (80, 443):
        url += ':%u' % args.port
    params = {'SID': sid}
    if args.screen:
        params['screen'] = args.screen
    if args.values:
        params['set'] = args.values
    if args.backlight is not None:
        params['backlight'] = str(args.backlight)

    print('GET %s/lvgl-screen.bmp (%s)' % (url, ', '.join('%s=%s' % item for item in params.items() if item[0] != 'SID')))
    try:
        response = requests.get(url + '/lvgl-screen.bmp', params=params, timeout=args.timeout)
    except requests.exceptions.RequestException as exception:
        print('request failed: %s' % exception)
        return 2
    if response.status_code != 200:
        print('HTTP %u: %s' % (response.status_code, response.content[:200].decode('utf-8', errors='replace')))
        return 2

    image = Image.open(io.BytesIO(response.content))
    # the BMP is 24 bit bottom-up; LV_COLOR_16_SWAP does not change the order of the bytes
    image = image.convert('RGB')
    image.save(args.output)
    print('wrote %s (%ux%u, %u bytes)' % (args.output, image.width, image.height, len(response.content)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
