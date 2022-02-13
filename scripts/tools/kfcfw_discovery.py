from zeroconf import ServiceBrowser, Zeroconf

TYPE = "_kfcmdns._udp.local."
FORMAT = "%-16.16s | %-22.22s | %s | %-16.16s | %-24.24s | %-19.19s | %s"

class MyListener:

    def remove_service(self, zeroconf, type, name):
        print("Service %s removed" % (name,))

    def add_service(self, zeroconf, type, name):
        info = zeroconf.get_service_info(type, name)
        # print("Service %s added, service info: %s" % (name, info))
        addresses = []
        for addr in info.addresses:
            addresses.append("%d.%d.%d.%d" % (addr[0], addr[1], addr[2], addr[3]))
        name = name.replace('.' + TYPE, '')
        try:
            version = '%s Build %s ' % (info.properties[b'v'].decode('ascii') , info.properties[b'b'].decode('ascii'))
        except:
            version = '???'
            version = str(info.properties)
            pass
        try:
            title = info.properties[b't'].decode('ascii')
        except:
            title = '???'
            pass
        try:
            device = info.properties[b'd'].decode('ascii')
        except:
            device = '???'
            pass
        print(FORMAT % (name, info.server, ("%- 5u" % (info.port)), ', '.join(addresses), title, version, device))


zeroconf = Zeroconf()
listener = MyListener()

browser = ServiceBrowser(zeroconf, TYPE, listener)

try:
    print("Press enter to exit...")
    title  = FORMAT % ('name', 'server', 'port ', 'addresses', 'device title', 'version', 'device')
    print(title)
    print('-' * len(title))
    input("")
finally:
    zeroconf.close()
