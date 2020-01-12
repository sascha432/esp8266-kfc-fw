#
# Author: sascha_lammers@gmx.de
#

import sys
import os
import time
import argparse
import kfcfw_session
import requests
import re
import json

def verbose(msg, new_line=True):
    global args
    if args.quiet==False:
        if new_line:
            print(msg)
        else:
            print(msg, end="")

def error(msg, code = 1):
    verbose(msg)
    sys.exit(code)

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

def display_status(resp):
    if resp.status_code==200:
        content = resp.content.decode()
        software = get_div("Software", content)
        hardware = get_div("Hardware", content)
        if software!=None:
            verbose("Software: " + software)
        if software!=None:
            verbose("Hardware: " + hardware)
    elif resp.status_code==403:
        error("Access denied")
    elif resp.status_code==404:
        error("Status page not found")
    else:
        error("Invalid response code " + str(resp.status_code))

parser = argparse.ArgumentParser(description="OTA for KFC Firmware", formatter_class=argparse.RawDescriptionHelpFormatter, epilog="exit codes:\n  0 - success\n  1 - general error\n  2 - update failed\n  3 - device did not respond after update")
parser.add_argument("hostname", help="web server hostname")
parser.add_argument("image", help="firmware image", type=argparse.FileType("rb"))
parser.add_argument("-u", "--user", help="Username", required=True)
parser.add_argument("-p", "--pw", help="password", required=True)
parser.add_argument("-P", "--port", help="web server port", type=int, default=80)
parser.add_argument("-t", "--type", help="type of image ", choices=["firmware", "spiffs", "atmega"], default="firmware")
parser.add_argument("-s", "--secure", help="use https to upload", action="store_true", default=False)
parser.add_argument("-q", "--quiet", help="do not show any output", action="store_true", default=False)
parser.add_argument("-n", "--no-status", help="do not query status", action="store_true", default=False)
parser.add_argument("-W", "--no-wait", help="wait after upload until device has been rebooted", action="store_true", default=False)
args = parser.parse_args()

url = "http"
if args.secure:
    url += "s"
url = url + "://" + args.hostname + "/"

# //firmware_image
image_type = "u_flash"
if args.type!="firmware":
    image_type = "u_" + image_type

sid = kfcfw_session.Session().generate(args.user, args.pw)

if args.no_status:
    verbose("Checking if device is alive "  + args.user + ":***@" + args.hostname)
    resp = requests.get(url + "is_alive", timeout=30)
    if resp.status_code!=200 or resp.content.decode()!='0':
        error("Device did not respond")
else:
    verbose("Querying status "  + args.user + ":***@" + args.hostname)
    resp = requests.get(url + "status.html", data={"SID": sid}, timeout=30)
    display_status(resp)

verbose("Uploading " + str(os.fstat(args.image.fileno()).st_size) + " Bytes: " + args.image.name)

resp = requests.post(url + "update", files={"firmware_image": args.image}, data={"image_type": image_type, "SID": sid}, timeout=30, allow_redirects=False)
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
        sys.exit(2)

if args.no_wait==False:
    verbose("Waiting for device to reboot", False)
    if args.quiet==False:
        for i in range(0, 2):
            time.sleep(2)
            print(".", end="", flush=True)
    else:
        time.sleep(4)

    max_wait = 60
    while True:
        if args.quiet==False:
            print(".", end="", flush=True)
        try:
            if args.no_status:
                resp = requests.get(url + "is_alive", timeout=1)
                if resp.status_code==200 and resp.content.decode()=='0':
                    verbose("")
                    break
            else:
                resp = requests.get(url + "status.html", data={"SID": sid}, timeout=1)
                if resp.status_code==200:
                    verbose("")
                    display_status(resp)
                    break
        except:
            time.sleep(1)
            max_wait = max_wait - 1
            if max_wait==0:
                error("Device did not response after update", 3)
        else:
            break
