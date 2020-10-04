#
# Author: sascha_lammers@gmx.de
#

import http.server
import socketserver
import sys
import os
import threading
import time

webroot = os.path.normpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../'))

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_my_headers()
        http.server.SimpleHTTPRequestHandler.end_headers(self)

    def send_my_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")

    def do_GET(self):
        http.server.SimpleHTTPRequestHandler.do_GET(self)

def start_http(dir, HandlerClass=MyHTTPRequestHandler, ServerClass=http.server.ThreadingHTTPServer, protocol="HTTP/1.0", port=8888, bind=""):
    server_address = (bind, port)
    HandlerClass.protocol_version = protocol
    with ServerClass(server_address, HandlerClass) as httpd:
        sa = httpd.socket.getsockname()
        serve_message = "Serving HTTP on {host} port {port} (http://{host}:{port}/) for %s ..." % dir
        print(serve_message.format(host=sa[0], port=sa[1]))
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nKeyboard interrupt received, exiting.")
            sys.exit(0)

def run(port, dir):
    handler_class = http.server.partial(MyHTTPRequestHandler, directory=dir)
    start_http(dir, HandlerClass=handler_class, port=port, bind='')

thread1 = threading.Thread(target=run, args=(8000, os.path.join(webroot, 'lib/KFCWebBuilder/Resources')), daemon=True)
thread1.start()
thread2 = threading.Thread(target=run, args=(8001, os.path.join(webroot, 'Resources')), daemon=True)
thread2.start()

time.sleep(1)
print("Press Ctrl+C to terminate...");
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\nKeyboard interrupt received, exiting.")
    sys.exit(0)
