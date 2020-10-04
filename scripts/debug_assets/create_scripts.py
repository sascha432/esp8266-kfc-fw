import os
import glob
import json

items = []
dir = glob.glob('./Resources/js/*.js')
dir += glob.glob('./Resources/js/forms/*.js')
for n in dir:
    n = str(n)
    items.append(n.replace('\\', '/'));

if False:
    # dump list of files
    print(json.dumps(items, indent=4))

else:

    # dump script tags
    files = [
        "./Resources/js/webui-alerts.js",
        "./Resources/js/forms.js",
        "./Resources/js/forms/alarm.js",
        "./Resources/js/forms/alive-check.js",
        "./Resources/js/forms/clock.js",
        "./Resources/js/forms/device.js",
        "./Resources/js/forms/network.js",
        "./Resources/js/forms/ntp.js",
        "./Resources/js/forms/remote.js",
        "./Resources/js/forms/syslog.js",
        "./Resources/js/forms/update-fw.js",
        "./Resources/js/forms/webui.js",
        "./Resources/js/forms/wifi.js",
        "./Resources/js/sync-time.js",
        "./Resources/js/file-manager.js",
        "./Resources/js/ws-console.js",
        "./Resources/js/http2serial.js",
        "./Resources/js/ping-console.js",
        "./Resources/js/webui.js",
        "./Resources/js/charts/chartjs-plugin-dragdata.min.js"
    ]

    for n in files:
        print('<script src="$DEBUG_ASSETS_URL2$/%s?rnd=%%RANDOM%%"></script>' % n[12:])
