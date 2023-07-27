
import math
import random

samples = 256.0 # number of FFT samples
bands = 32 # number of bands
px = 1.075
fIncr = 16800 / bands
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
    fstart = fl[b] / samples * 4.0
    fend = fl[b + 1] / samples * 4.0
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

    band_index.append(bstart | (bend << 8))

    factors1.append(factor1)
    factors2.append(factor2)

    # print('%f-%f (%f) %d-%d (%d)' % (fstart, fend, frange, bstart, bend, brange))

s = str(band_index).replace('[', '{').replace(']', '}')
print('static constexpr uint16_t bands[] = %s; // start=low byte, end=high byte' % s);
s = str(factors1).replace('[', '{').replace(']', '}')
print('static constexpr float factors1[] = %s;' % s);
s = str(factors2).replace('[', '{').replace(']', '}')
print('static constexpr float factors2[] = %s;' % s);


exit(0)

# how to use the band_index and factors
samples = []
for i in range(0, 256):
    samples.append(random.randrange(100))

for b in range(0, bands):
    start = band_index[b] & 0xff
    end = band_index[b] >> 8
    num = end - start + 1
    mean = 0
    sumsamples = 0
    f1 = factors1[b]
    f2 = factors2[b]
    frange = num - (f1 + f2)
    for s in range(start, end + 1):
        val = samples[s]
        sumsamples += val
        if s == start and f1!=0:
            print('s', b, s, f1)
            val = val * f1
        if s == end:
            val = val * f2
            print('e', b, s, f2)
        mean += val
    mean /= frange

    print("%d=%f (%d/%f)" % (b, mean, sumsamples, frange))

