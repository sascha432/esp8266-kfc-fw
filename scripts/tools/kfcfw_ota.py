#
# Author: sascha_lammers@gmx.de
#

# requires:
# pip3 install requests-toolbelt progressbar2

import os
import time
import argparse
# import progressbar
from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor
import requests
import re
import json
import hashlib
import shutil

from os import path
import sys
libs_dir = path.realpath(path.join(path.dirname(__file__), '../libs'))
sys.path.insert(0, libs_dir)

import kfcfw.session
import kfcfw.configuration


class SimpleProgressBar:

    def __init__(self, max_value, redirect_stdout=True):
        self.bar = None
        # self.bar = progressbar.ProgressBar(...)

    def start(self):
        if self.bar!=None:
            self.bar.start()

    def update(self, value, max_value = 0):
        if self.bar!=None:
            self.bar.update(value, max_value)

    def finish(self):
        if self.bar!=None:
            self.bar.finish()


def verbose(msg, new_line=True):
    global args
    if args.quiet==False:
        if new_line:
            print(msg)
        else:
            print(msg, end="", flush=True)

def error(msg, code = 1):
    verbose(msg)
    sys.exit(code)

def get_login_error(content):
    m = re.search("id=\"login_error_message\">([^<]+)</div", content, flags=re.IGNORECASE|re.DOTALL)
    try:
        return m.group(1).strip()
    except:
        return None

def get_div(title, content):
    m = re.search(">\s*" + title + "\s*<\/div>\s*<div[^>]*>([^<]+)<", content, flags=re.IGNORECASE|re.DOTALL)
    try:
        return m.group(1).strip()
    except:
        return None

def get_h3(content):
    m = re.search("<h3[^>]*>([^<]+)</h3", content, flags=re.IGNORECASE|re.DOTALL)
    try:
        return m.group(1).strip()
    except:
        return None

def is_alive(url, target):
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

def get_alive(url, target):
    verbose("Checking if device is alive "  + target)
    resp = requests.get(url + "is-alive", timeout=30)
    if resp.status_code!=200:
        error("Device did not respond", 2)
    elif resp.content.decode()!='0':
        error("Invalid response", 2)
    verbose("Device responded")

def get_status(url, target, sid):
    verbose("Querying status "  + target)
    resp = requests.get(url + "status.html", data={"SID": sid}, timeout=30)
    if resp.status_code==200:
        content = resp.content.decode()
        login_error = get_login_error(content)
        if login_error!=None:
            error("Login failed: " + login_error + ". Check password or use --sha1 for old authentication method")
        software = get_div("Software", content)
        hardware = get_div("Hardware", content)
        if software!=None:
            verbose("Software: " + software)
        if software!=None:
            verbose("Hardware: " + hardware)
        if software==None and hardware==None:
            error("Could not get status from device, use --no-status to skip")
    elif resp.status_code==403:
        error("Access denied", 2)
    elif resp.status_code==404:
        error("Status page not found. Use -n to skip this check", 2)
    else:
        error("Invalid response code " + str(resp.status_code), 2)

def copyfile(src, dst):
    shutil.copyfile(src, dst)
    verbose('Copied: %s => %s' % (src, dst))

def flash(url, type, target, sid):

    if type=="upload":
        type = "flash"
    elif type=="uploadfs":
        type = "spiffs"

    image_type = "u_" + type

    is_alive(url, target)
    if args.no_status==False:
        get_status(url, target, sid)

    filesize = os.fstat(args.image.fileno()).st_size
    verbose("Uploading %u Bytes: %s" % (filesize, args.image.name))

    if args.elf:
        elf_hash = None
        elf_exception = None
        try:
            elf = args.image.name.replace('.bin', '.elf')
            h = hashlib.sha1()
            with open(elf, 'rb') as file:
                while True:
                    data = file.read(1024)
                    if not data:
                        break
                    h.update(data)
            elf_hash = h.hexdigest();
        except Exception as e:
            elf_exception = e

        try:
            if type=='flash':
                if elf_exception:
                    raise elf_exception
                copyfile(elf, os.path.join(args.elf, elf_hash + '.elf'))
                copyfile(args.image.name, os.path.join(args.elf, elf_hash + '.bin'))
            elif type=='spiffs' and elf_hash:
                copyfile(args.image.name, os.path.join(args.elf, elf_hash + '.spiffs.bin'))

            if elf_hash and args.ini:
                copyfile(args.ini, os.path.join(args.elf, elf_hash + '.ini'))

        except Exception as e:
                error(str(e), 5);

    bar = SimpleProgressBar(max_value=filesize, redirect_stdout=True)
    bar.start();
    encoder = MultipartEncoder(fields={ "image_type": image_type, "SID": sid, "elf_hash": elf_hash, "firmware_image": ("filename", args.image, "application/octet-stream") })

    monitor = MultipartEncoderMonitor(encoder, lambda monitor: bar.update(min(monitor.bytes_read, filesize)))

    # resp = requests.post(url + "update", files={ "firmware_image": args.image }, data={ "image_type": image_type, "SID": sid }, timeout=30, allow_redirects=False)
    resp = requests.post(url + "update", data=monitor, timeout=30, allow_redirects=False, headers={ "Content-Type": monitor.content_type })
    bar.finish();

    if resp.status_code==302:
        verbose("Update successful")
    else:
        content = resp.content.decode()
        error = get_h3(content)
        if error==None:
            verbose("Update failed with unknown response")
            verbose("Response code " + str(resp.status_code))
            verbose(content)
            sys.exit(2)
        else:
            verbose("Update failed with: " + error)
            sys.exit(3)

    if args.no_wait==False:
        max_wait = 60
        step = 0
        verbose("Waiting for device to reboot...")
        if args.quiet==False:
            bar = SimpleProgressBar(max_value=progressbar.UnknownLength, redirect_stdout=True)
            for i in range(0, 40):
                bar.update(step)
                step = step + 1
                time.sleep(0.1)
        else:
            time.sleep(4)

        while True:
            if args.quiet==False:
                for i in range(10):
                    bar.update(step)
                    step = step + 1
                    time.sleep(0.1)
            else:
                time.sleep(1)
            if is_alive(url, target):
                if args.no_status==False:
                    get_status(url, target, sid)
                break
            max_wait = max_wait - 1
            if max_wait==0:
                if args.quiet==False:
                    bar.finish();
                error("Device did not response after update", 4)
        if args.quiet==False:
            bar.finish();
        verbose("Device has been rebooted")

def export_settings(url, target, sid, output):

    if not is_alive(url, target):
        error("Device did not respond")

    resp = requests.get(url + 'export_settings', data={"SID": sid})
    if resp.status_code==200:
        if resp.headers['Content-Type']!='application/json':
            error("Content type not JSON")
        else:
            content = resp.content.decode()
            if output==None:
                print(content)
            else:
                with open(output, "wt") as file:
                    file.write(content)
                verbose("Settings exported: " + output)
    else:
        error("Invalid response status code " + str(resp.status_code))


def import_settings(url, target, sid, file, params):

    if not is_alive(url, target):
        error("Device did not respond")

    try:
        settings = json.loads(file.read())
    except Exception as e:
        error("Failed to read " + file.name + ": " + str(e))

    cfg = kfcfw.configuration.Configuration()
    data = {}
    for handle in params:
        if handle[0:2]!="0x":
            handle = "0x{:04x}".format(cfg.get_handle("Config()." + handle))
        if handle in settings['config'].keys():
            data[handle] = settings['config'][handle]
        else:
            error("Cannot find setting: " + name)

    resp = requests.post(url + "import_settings", data={"SID": sid, "config": json.dumps({"config": data})}, timeout=30, allow_redirects=False)
    if resp.status_code!=200:
        error("Invalid response code " + str(resp.status_code))

    content = resp.content.decode()
    try:
        resp = json.loads(content)
    except:
        error("Invalid json response: " + content)

    if resp["count"]==0:
        error("No data imported")
    elif resp['count']>0:
        verbose(str(resp["count"]) + " configuration setting(s) imported")
        if resp['count']!=len(data):
            error("Settings count does not match: %u" % len(data))
    else:
        error("Failed to import: " + resp["message"])


# main

parser = argparse.ArgumentParser(description="OTA for KFC Firmware", formatter_class=argparse.RawDescriptionHelpFormatter, epilog="exit codes:\n  0 - success\n  1 - general error\n  2 - device did not respond\n  3 - update failed\n  4 - device did not respond after update\n  5 - copying ELF firmware failed")
parser.add_argument("action", help="action to execute", choices=["flash", "upload", "spiffs", "uploadfs", "atmega", "status", "alive", "export", "import"])
parser.add_argument("hostname", help="web server hostname")
parser.add_argument("-u", "--user", help="username", required=True)
parser.add_argument("-p", "--pw", "--pass", help="password", required=True)
parser.add_argument("-I", "--image", help="firmware image", type=argparse.FileType("rb"))
parser.add_argument("-O", "--output", help="export settings output file")
parser.add_argument("--params", help="parameters to import", nargs='*', default=[])
parser.add_argument("-P", "--port", help="web server port", type=int, default=80)
parser.add_argument("-s", "--secure", help="use https to upload", action="store_true", default=False)
parser.add_argument("-q", "--quiet", help="do not show any output", action="store_true", default=False)
parser.add_argument("-n", "--no-status", help="do not query status", action="store_true", default=False)
parser.add_argument("-W", "--no-wait", help="wait after upload until device has been rebooted", action="store_true", default=False)
parser.add_argument("--elf", help="copy firmware.(elf|bin) or spiffs.bin to directory", type=str, default=None)
parser.add_argument("--ini", help="copy platform.ini to elf directory", type=str, default=None)
parser.add_argument("--sha1", help="use sha1 authentication", action="store_true", default=False)
args = parser.parse_args()

url = "http"
if args.secure:
    url += "s"
url = url + "://" + args.hostname + "/"

if args.sha1:
    session = kfcfw.session.Session(hashlib.sha1())
else:
    session = kfcfw.session.Session()
sid = session.generate(args.user, args.pw)
target = args.user + ":***@" + args.hostname

if args.action in("flash", "upload", "spiffs", "uploadfs", "atmega"):
    flash(url, args.action, target, sid)
elif args.action=="status":
    get_status(url, target, sid)
elif args.action=="alive":
    get_alive(url, target)
elif args.action=="export":
    export_settings(url, target, sid, args.output)
elif args.action=="import":
    if len(args.params)==0 or args.image==None:
        parser.print_usage()
    else:
        import_settings(url, target, sid, args.image, args.params)
