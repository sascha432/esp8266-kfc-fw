from zeroconf import ServiceBrowser, Zeroconf

KFC_TYPE = "_kfcmdns._udp.local."
# devices with other firmware that only announce a web server, matched by the instance name
HTTP_TYPE = "_http._tcp.local."
HTTP_PREFIXES = ('pixeled-', 'hassctrl-')
FORMAT = "%-16.16s | %-22.22s | %s | %-16.16s | %-32.32s | %-19.19s | %-12.12s"

def get_txt(info, key):
    value = info.properties.get(key)
    if value is None:
        return '???'
    return value.decode('ascii', 'replace')

class MyListener:

    def remove_service(self, zeroconf, type, name):
        print("Service %s removed" % (name,))

    def update_service(self, zeroconf, type, name):
        pass

    def add_service(self, zeroconf, type, name):
        name = name[:-len('.' + type)] if name.endswith('.' + type) else name
        prefix = None
        if type == HTTP_TYPE:
            prefix = next((p for p in HTTP_PREFIXES if name.lower().startswith(p)), None)
            if prefix is None:
                return
        info = zeroconf.get_service_info(type, name + '.' + type)
        if info is None:
            return
        addresses = info.parsed_addresses()
        if prefix:
            title = '-'
            version = '-'
            device = prefix.rstrip('-')
        else:
            version = '%s Build %s' % (get_txt(info, b'v'), get_txt(info, b'b'))
            title = get_txt(info, b't')
            device = get_txt(info, b'd')
        print(FORMAT % (name, info.server, ("%- 5u" % (info.port)), ', '.join(addresses), title, version, device))


zeroconf = Zeroconf()
listener = MyListener()

browser = ServiceBrowser(zeroconf, [KFC_TYPE, HTTP_TYPE], listener)

try:
    print("----------------------")
    print("Press enter to exit...")
    print("----------------------")
    title  = FORMAT % ('Name', 'Server', 'Port ', 'Addresses', 'Device Title', 'Version', 'Device')
    print(title)
    print('-' * len(title))
    input("")
finally:
    zeroconf.close()
