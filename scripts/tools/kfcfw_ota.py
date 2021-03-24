#
# Author: sascha_lammers@gmx.de
#

# requires:
# pip3 install requests-toolbelt progressbar2 websocket

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
import subprocess
from os import path

from os import path
import sys
libs_dir = path.realpath(path.join(path.dirname(__file__), '../libs'))
sys.path.insert(0, libs_dir)

import kfcfw

class SimpleProgressBar:

    UnknownLength = 0

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




class OTA(kfcfw.OTAHelpers):

    def __init__(self, args):
        kfcfw.OTAHelpers.__init__(self)
        self.args = args

    def verbose(self, msg, new_line=True):
        if self.args.quiet==False:
            if new_line:
                print(msg)
            else:
                print(msg, end="", flush=True)

    def error(self, msg, code = 1):
        self.verbose(msg)
        sys.exit(code)


    def copyfile(self, src, dst):
        shutil.copyfile(src, dst)
        self.verbose('Copied: %s => %s' % (src, dst))

    def flash(self, url, type, target, sid):

        if type=="upload":
            type = "flash"
        elif type=="uploadfs":
            type = "spiffs"

        image_type = "u_" + type

        self.is_alive(url, target)
        if self.args.no_status==False:
            self.get_status(url, target, sid)

        filesize = os.fstat(self.args.image.fileno()).st_size
        self.verbose("Uploading %u Bytes: %s" % (filesize, self.args.image.name))

        elf_hash = None
        if self.args.elf:
            elf_exception = None
            try:
                elf = self.args.image.name.replace('.bin', '.elf')
                h = hashlib.md5()
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
                    self.copyfile(elf, path.join(self.args.elf, elf_hash + '.elf'))
                    self.copyfile(self.args.image.name, path.join(self.args.elf, elf_hash + '.bin'))
                elif type=='spiffs' and elf_hash:
                    self.copyfile(self.args.image.name, path.join(self.args.elf, elf_hash + '.spiffs.bin'))

                if elf_hash and self.args.ini:

                    archive = path.join(self.args.elf, elf_hash + '.tgz')
                    git_dir = path.abspath(path.join(self.args.ini, '.git'))
                    ini_file = path.abspath(path.join(self.args.ini, 'platformio.ini'))
                    ini_dir = path.abspath(path.join(self.args.ini, 'conf', '*'))
                    git_info = path.abspath(path.join(self.args.ini, 'git.txt'))

                    subprocess.run(['git', '--git-dir', git_dir, 'log', '--pretty=oneline', '--since=4.weeks', '>', git_info], shell=True)
                    subprocess.run(['tar', 'cfz', archive, ini_file, git_info, ini_dir], shell=True)

            except Exception as e:
                self.error(str(e), 5);

        bar = SimpleProgressBar(max_value=filesize, redirect_stdout=True)
        bar.start();
        encoder = MultipartEncoder(fields={ "image_type": image_type, "SID": sid, "elf_hash": elf_hash, "firmware_image": ("filename", self.args.image, "application/octet-stream") })

        monitor = MultipartEncoderMonitor(encoder, lambda monitor: bar.update(min(monitor.bytes_read, filesize)))

        # resp = requests.post(url + "update", files={ "firmware_image": self.args.image }, data={ "image_type": image_type, "SID": sid }, timeout=30, allow_redirects=False)
        resp = requests.post(url + 'update', data=monitor, timeout=30, allow_redirects=False, headers={ 'User-Agent': 'KFCFW OTA', "Content-Type": monitor.content_type })
        bar.finish();

        if resp.status_code==302:
            self.verbose("Update successful")
        else:
            content = resp.content.decode()
            if not 'Device is rebooting after' in content:
                error = self.get_tag(content, 'h4')
                if error==None:
                    error = self.get_h3(content)
                if error==None:
                    self.verbose("Update failed with unknown response")
                    self.verbose("Response code " + str(resp.status_code))
                    self.verbose(content)
                    sys.exit(2)
                else:
                    self.verbose("Update failed with: " + error)
                    sys.exit(3)

        if self.args.no_wait==False:
            max_wait = 60
            step = 0
            self.verbose("Waiting for device to reboot...")
            if self.args.quiet==False:
                bar = SimpleProgressBar(max_value=SimpleProgressBar.UnknownLength, redirect_stdout=True)
                for i in range(0, 40):
                    bar.update(step)
                    step = step + 1
                    time.sleep(0.1)
            else:
                time.sleep(4)

            while True:
                if self.args.quiet==False:
                    for i in range(10):
                        bar.update(step)
                        step = step + 1
                        time.sleep(0.1)
                else:
                    time.sleep(1)
                if self.is_alive(url, target):
                    if self.args.no_status==False:
                        self.get_status(url, target, sid)
                    break
                max_wait = max_wait - 1
                if max_wait==0:
                    if self.args.quiet==False:
                        bar.finish();
                    error("Device did not response after update", 4)
            if self.args.quiet==False:
                bar.finish();
            self.verbose("Device has been rebooted")

    def export_settings(self, url, target, sid, output):

        if not self.is_alive(url, target):
            self.error("Device did not respond")

        resp = requests.get(url + 'export_settings', data={"SID": sid})
        if resp.status_code==200:
            if resp.headers['Content-Type']!='application/json':
                self.error("Content type not JSON")
            else:
                content = resp.content.decode()
                if output==None:
                    print(content)
                else:
                    with open(output, "wt") as file:
                        file.write(content)
                    self.verbose("Settings exported: " + output)
        else:
            self.error("Invalid response status code " + str(resp.status_code))


# main

parser = argparse.ArgumentParser(description="OTA for KFC Firmware", formatter_class=argparse.RawDescriptionHelpFormatter, epilog="exit codes:\n  0 - success\n  1 - general error\n  2 - device did not respond\n  3 - update failed\n  4 - device did not respond after update\n  5 - copying ELF firmware failed")
parser.add_argument("action", help="action to execute", choices=["flash", "upload", "spiffs", "uploadfs", "atmega", "status", "alive", "export", "import", "factoryreset"])
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
parser.add_argument("--ini", help="copy platform.ini and other files to elf directory", type=str, default=None)
parser.add_argument("--sha1", help="use sha1 authentication", action="store_true", default=False)
args = parser.parse_args()


ota = OTA(args)

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
    ota.flash(url, args.action, target, sid)
elif args.action=="factoryreset":
    payload_sent = False
    timeout = time.monotonic() + 30
    socket = kfcfw.OTASerialConsole(args.hostname, sid)
    while time.monotonic()<timeout and not socket.is_closed:
        if socket.is_authenticated and payload_sent==False:
            socket.ws.send('+factory\r\n')
            socket.ws.send('+store\r\n')
            socket.ws.send('+rst\r\n')
            payload_sent = True
        time.sleep(1)

elif args.action=="status":
    ota.get_status(url, target, sid)
elif args.action=="alive":
    ota.get_alive(url, target)
elif args.action=="export":
    ota.export_settings(url, target, sid, args.output)
elif args.action=="import":
    if len(args.params)==0 or args.image==None:
        parser.print_usage()
    else:
        ota.import_settings(url, target, sid, args.image, args.params)
