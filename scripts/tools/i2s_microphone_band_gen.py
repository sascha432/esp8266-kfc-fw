
import math
import random

samples = 256.0 # number of FFT samples
bands = 32 # number of bands
px = 1.068
fIncr = 13250 / bands
maxFrequency = fIncr
mul = maxFrequency / math.pow(px, bands)
pxMul = 1.0 / math.pow(px, bands)

fl = []

frequencyB = 0
pxPow = 1.0
for x in range(0, bands + 1):

    frequency = pxPow * mul
    pxPow *= px
    maxFrequency += fIncr
    mul = maxFrequency * pxMul

    fl.append(int(frequencyB))

    if x<32:
        print('// band %d = %.0f-%.0f Hz' % (x + 1, frequencyB, frequency - 1))

    frequencyB = frequency

band_index = []
factors1 = []
factors2 = []

for b in range(0, bands):

    # start, end and range
    fstart = fl[b] / samples * 2.0
    fend = fl[b + 1] / samples * 2.0
    frange = fend - fstart

    # start, end and range in the samples array
    bstart = int(fstart)
    bend = int(fend)
    brange = bend - bstart + 1

    if brange==1:
        # for a single sample we can use frange
        factor1 = 0.0
        factor2 = frange
    else:
        # factor1 + factor2 = frange but split into the first and last sample
        factor1 = 1.0 - (fstart - int(fstart))
        factor2 = (fend - int(fend))

    band_index.append('BW(%d, %d)' % (bstart, bend))
    # band_index.append('%d | (%d << 8)' % bstart | (bend << 8))

    factors1.append(factor1)
    factors2.append(factor2)

    # print('%f-%f (%f) %d-%d (%d)' % (fstart, fend, frange, bstart, bend, brange))

s = str(band_index).replace('[', '{').replace(']', '}').replace("'", '')
print('static constexpr uint16_t bands[] = %s;' % s)
s = str(factors1).replace('[', '{').replace(']', '}')
print('static constexpr float factors1[] = %s;' % s)
s = str(factors2).replace('[', '{').replace(']', '}')
print('static constexpr float factors2[] = %s;' % s)
