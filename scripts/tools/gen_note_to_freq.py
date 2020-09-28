def getFrequency(i, octave, A4 = 440):
    keyNumber = i + o * 12
    return A4 * 2** ((keyNumber - 49) / 12)


f = 32.0
n = 0
for o in range(1, 7):
    for i in range(0, 12):
        freq = getFrequency(i, o)
        value = int(freq * f + (0.5 / f))
        if n>=4 and n<60:
            print("%s, // [%d] %.3f %u" % (value, n-4, freq, value >> 5))
        n += 1
