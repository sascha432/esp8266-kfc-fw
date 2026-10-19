# serial_log.py <port> <seconds> <outfile> - logs a serial port without resetting the board (DTR/RTS low)
import sys, time, serial
port, seconds, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
ser = serial.Serial(); ser.port = port; ser.baudrate = 115200; ser.timeout = 0.2
ser.dtr = False; ser.rts = False          # before open(): no auto reset
ser.open()
end, line = time.time() + seconds, b''
with open(out, 'w', encoding='utf8', errors='replace') as f:
    while time.time() < end:
        line += ser.read(4096)
        while b'\n' in line:
            text, line = line.split(b'\n', 1)
            f.write('%.3f %s\n' % (time.time(), text.replace(b'\0', b'').decode('utf8', 'replace').rstrip('\r'))); f.flush()
ser.close()
