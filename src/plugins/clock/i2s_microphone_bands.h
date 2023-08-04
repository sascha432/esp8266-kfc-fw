// generated by scripts/tools/i2s_microphone_band_gen.py

#pragma once

// band 1 = 0-151 Hz
// band 2 = 152-312 Hz
// band 3 = 312-481 Hz
// band 4 = 481-659 Hz
// band 5 = 660-847 Hz
// band 6 = 848-1045 Hz
// band 7 = 1046-1254 Hz
// band 8 = 1255-1473 Hz
// band 9 = 1474-1704 Hz
// band 10 = 1705-1947 Hz
// band 11 = 1947-2201 Hz
// band 12 = 2202-2469 Hz
// band 13 = 2469-2749 Hz
// band 14 = 2750-3044 Hz
// band 15 = 3045-3353 Hz
// band 16 = 3353-3676 Hz
// band 17 = 3677-4016 Hz
// band 18 = 4016-4371 Hz
// band 19 = 4372-4743 Hz
// band 20 = 4744-5132 Hz
// band 21 = 5133-5540 Hz
// band 22 = 5541-5966 Hz
// band 23 = 5967-6412 Hz
// band 24 = 6413-6878 Hz
// band 25 = 6879-7366 Hz
// band 26 = 7366-7875 Hz
// band 27 = 7876-8407 Hz
// band 28 = 8407-8962 Hz
// band 29 = 8963-9542 Hz
// band 30 = 9543-10148 Hz
// band 31 = 10148-10780 Hz
// band 32 = 10780-11439 Hz

static constexpr int kMaxBands = 32;
static constexpr uint16_t bands[] = {BW(0, 0), BW(0, 1), BW(1, 2), BW(2, 3), BW(3, 4), BW(4, 5), BW(5, 7), BW(7, 8), BW(8, 9), BW(9, 10), BW(10, 12), BW(12, 13), BW(13, 15), BW(15, 16), BW(16, 18), BW(18, 20), BW(20, 22), BW(22, 24), BW(24, 26), BW(26, 28), BW(28, 30), BW(30, 33), BW(33, 35), BW(35, 38), BW(38, 41), BW(41, 43), BW(43, 46), BW(46, 50), BW(50, 53), BW(53, 56), BW(56, 60), BW(60, 63)};
static constexpr float factors1[] = {0.0, 0.15625, 0.2578125, 0.31640625, 0.3203125, 0.26953125, 0.1640625, 1.0, 0.77734375, 0.48828125, 0.13671875, 0.71484375, 0.22265625, 0.65625, 0.01171875, 0.2890625, 0.484375, 0.58984375, 0.60546875, 0.53125, 0.359375, 0.08203125, 0.703125, 0.21484375, 0.61328125, 0.89453125, 0.0546875, 0.0859375, 0.984375, 0.75, 0.37109375, 0.84375};
static constexpr float factors2[] = {0.84375, 0.7421875, 0.68359375, 0.6796875, 0.73046875, 0.8359375, 0.0, 0.22265625, 0.51171875, 0.86328125, 0.28515625, 0.77734375, 0.34375, 0.98828125, 0.7109375, 0.515625, 0.41015625, 0.39453125, 0.46875, 0.640625, 0.91796875, 0.296875, 0.78515625, 0.38671875, 0.10546875, 0.9453125, 0.9140625, 0.015625, 0.25, 0.62890625, 0.15625, 0.8359375};