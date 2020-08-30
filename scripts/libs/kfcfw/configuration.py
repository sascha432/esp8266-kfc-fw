#
# Author: sascha_lammers@gmx.de
#

class Configuration:

    def crc16_compute(self, crc, a):
        crc = crc ^ a
        for i in range(0, 8):
            if (crc & 1)!=0:
                crc = (crc >> 1) ^ 0xa001
            else:
                crc = (crc >> 1)
        return crc

    def crc16_update(self, crc, data):
        for i in range(0, len(data)):
            crc = self.crc16_compute(crc, data[i])
        return crc

    def get_handle(self, name):
        data = name.encode('utf8')
        return self.crc16_update(0xffff, data)

