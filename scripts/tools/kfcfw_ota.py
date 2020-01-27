#
# Author: sascha_lammers@gmx.de
#

import sys
import os
import time
import argparse
import kfcfw_session
import kfcfw_configuration
import requests
import re
import json
import hashlib

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
    resp = requests.get(url + "is_alive", timeout=30)
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
            error("Log failed: " + login_error)
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
        error("Status page not found", 2)
    else:
        error("Invalid response code " + str(resp.status_code), 2)

def flash(url, type, target, sid):

    if type=="upload":
        type = "flash"
    elif type=="uploadfs":
        type = "spiffs"

    image_type = "u_" + type

    is_alive(url, target)
    if args.no_status==False:
        get_status(url, target, sid)

    image = args.image
    # if args.image_path!=None:
    #     if type=="flash":
    #         image_name = "firmware.bin"
    #     elif type=="spiffs":
    #         image_name = "spiffs.bin"
    #     image = open(args.image_path + "/" + image_name, "rb")

    verbose("Uploading " + str(os.fstat(image.fileno()).st_size) + " Bytes: " + image.name)

    resp = requests.post(url + "update", files={"firmware_image": image}, data={"image_type": image_type, "SID": sid}, timeout=30, allow_redirects=False)
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
        verbose("Waiting for device to reboot", False)
        if args.quiet==False:
            for i in range(0, 2):
                time.sleep(3)
                print(".", end="", flush=True)
        else:
            time.sleep(4)

        max_wait = 60
        while True:
            if args.quiet==False:
                print(".", end="", flush=True)
            if is_alive(url, target):
                if args.no_status==False:
                    get_status(url, target, sid)
                break
            time.sleep(1)
            max_wait = max_wait - 1
            if max_wait==0:
                error("Device did not response after update", 4)
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

    cfg = kfcfw_configuration.Configuration()
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
            error("Settings count does not match: " + str(len(data)))
    else:
        error("Failed to import: " + resp["message"])


# main

parser = argparse.ArgumentParser(description="OTA for KFC Firmware", formatter_class=argparse.RawDescriptionHelpFormatter, epilog="exit codes:\n  0 - success\n  1 - general error\n  2 - device did not respond\n  3 - update failed\n  4 - device did not respond after update")
parser.add_argument("action", help="action to execute", choices=["flash", "upload", "spiffs", "uploadfs", "atmega", "status", "alive", "export", "import"])
parser.add_argument("hostname", help="web server hostname")
parser.add_argument("-u", "--user", help="username", required=True)
parser.add_argument("-p", "--pw", help="password", required=True)
parser.add_argument("-I", "--image", help="firmware image", type=argparse.FileType("rb"))
# parser.add_argument("--image-path", help="path to firmware/spiffs image")
parser.add_argument("-O", "--output", help="export settings output file")
parser.add_argument("--params", help="parameters to import", nargs='*', default=[])
parser.add_argument("-P", "--port", help="web server port", type=int, default=80)
parser.add_argument("-s", "--secure", help="use https to upload", action="store_true", default=False)
parser.add_argument("-q", "--quiet", help="do not show any output", action="store_true", default=False)
parser.add_argument("-n", "--no-status", help="do not query status", action="store_true", default=False)
parser.add_argument("-W", "--no-wait", help="wait after upload until device has been rebooted", action="store_true", default=False)
parser.add_argument("--sha1", help="use sha1 authentication", action="store_true", default=False)
args = parser.parse_args()

url = "http"
if args.secure:
    url += "s"
url = url + "://" + args.hostname + "/"

if args.sha1:
    session = kfcfw_session.Session(hashlib.sha1())
else:
    session = kfcfw_session.Session()
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
