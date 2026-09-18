#
# Author: sascha_lammers@gmx.de
#
"""Spectrum and loudness analysis for the WARLS audio visualiser.
"""

from __future__ import annotations

from collections import deque

import numpy as np

# constants taken from AudioProcessor.cs
FREQ_MAX = 16800.0  # highest frequency used for the band edges
BAND_COUNT = 32  # `const int _bands = 32`
LOG_SCALE = 1.092  # px, 1.0 = linear spacing, > 1 = logarithmic
FFT_SIZE = 2048  # BASS_DATA_FFT2048
BAND_SCALE = 3 * 255.0  # y = sqrt(peak) * 3 * 255 - 4
BAND_OFFSET = 4
LEVEL_PERIOD = 0.02  # period of BASS_WASAPI_GetLevel(), 20 ms
LEVEL_MAX = 32768  # BASS level range, 0 (silent) .. 32768 (max)
LEVEL_SHIFT = 7  # C# uses `level >> 7` to get 0..255


def band_edges(bands: int = BAND_COUNT, log_scale: float = LOG_SCALE, freq_max: float = FREQ_MAX):
    """Upper frequency of every band in Hz.

    Replicates the recurrence from ``AudioProcessor.getSpectrumData()``::

        fIncr = freq_max / bands                 (525 Hz for 32 bands)
        freq(k) = px**k * fIncr * (k + 1) / px**bands

    For 32 bands and a log scale of 1.092 this is 31.41 Hz .. 15384.6 Hz.
    """
    f_incr = freq_max / float(bands)
    px = float(log_scale)
    px_pow_n = px**bands
    px_mul = 1.0 / px_pow_n
    max_frequency = f_incr
    mul = max_frequency * px_mul
    px_pow = 1.0
    edges = []
    for _ in range(bands):
        edges.append(px_pow * mul)
        px_pow *= px
        max_frequency += f_incr
        mul = max_frequency * px_mul
    return edges


def bin_index(frequency: float, rate: int, fft_size: int) -> int:
    """FFT bin of a frequency.

    The C# code uses ``(frequency / 22050) * (fft_size / 2)``, this uses the real
    sample rate so the bands are at the frequencies they claim to be.
    """
    return int((frequency / (rate / 2.0)) * (fft_size / 2))


def band_values(spectrum, bins, gain: float = 1.0):
    """FFT magnitudes -> band values 0..255 (the ``getSpectrumData()`` loop).

    `bins` are the exclusive upper bin of every band, `bin[k]` itself belongs to
    band `k + 1`, same as in C# (``for (; b0 < b1; b0++)``).
    """
    values = []
    last = len(spectrum) - 1
    b0 = 0
    for b1 in bins:
        if b1 > b0:
            peak = float(spectrum[b0 : min(b1, last + 1)].max())
        else:
            # C#: `if (b0 == b1) peak = fftData[b0];`
            peak = float(spectrum[min(b0, last)])
        b0 = b1
        value = int(np.sqrt(max(peak, 0.0) * gain) * BAND_SCALE - BAND_OFFSET)
        values.append(0 if value < 0 else (255 if value > 255 else value))
    return values


class SpectrumAnalyzer:
    """Turns a block of stereo frames into band values and peak levels."""

    def __init__(
        self,
        bands: int = BAND_COUNT,
        fft_size: int = FFT_SIZE,
        log_scale: float = LOG_SCALE,
        rate: int = 48000,
        gain: float = 1.0,
        smoothing: int = 2,
        freq_max: float = FREQ_MAX,
    ):
        if not 2 <= bands <= 32:
            raise ValueError('bands must be 2..32')
        if fft_size < 64 or fft_size % 2:
            raise ValueError('fft size must be an even number of at least 64 samples')

        self.bands = bands
        self.fft_size = fft_size
        self.rate = rate
        self.gain = gain
        self.freq_max = freq_max
        self.edges = band_edges(bands, log_scale, freq_max)
        # the last bin is not used (BASS returns the first half of the FFT only)
        self.bins = [min(bin_index(f, rate, fft_size), fft_size // 2) for f in self.edges]
        self.window = np.hanning(fft_size)
        # BASS scales the FFT so that a full scale sine is 1.0, the Hann window
        # (coherent gain 0.5) has to be compensated to keep that property
        self.norm = (fft_size / 2.0) * float(self.window.mean())
        self.smoothing = max(0, int(smoothing))
        self._history = deque(maxlen=max(1, self.smoothing))
        self.level_frames = max(1, int(rate * LEVEL_PERIOD))

    def _block(self, frames):
        """Last `fft_size` frames as a 2d array, zero padded if there is less data."""
        block = np.asarray(frames, dtype=np.float64)
        if block.ndim == 1:
            block = block[:, None]
        count = block.shape[0]
        if count < self.fft_size:
            block = np.pad(block, ((self.fft_size - count, 0), (0, 0)))
        else:
            block = block[-self.fft_size :]
        return block

    def magnitudes(self, frames):
        """Normalized FFT magnitudes of the first half of the spectrum (1.0 = full scale).

        All channels are combined into one mono FFT, same as BASS without the
        ``BASS_DATA_FFT_INDIVIDUAL`` flag.
        """
        mono = self._block(frames).mean(axis=1)
        return np.abs(np.fft.rfft(mono * self.window))[: self.fft_size // 2] / self.norm

    def spectrum(self, frames):
        """Band values 0..255, optionally smoothed over the last `smoothing` spectra."""
        values = band_values(self.magnitudes(frames), self.bins, self.gain)
        if self.smoothing > 1:
            self._history.append(values)
            if len(self._history) > 1:
                return np.rint(np.mean(np.asarray(self._history, dtype=np.float64), axis=0)).astype(int).tolist()
        return values

    def levels(self, frames, gain: float = 1.0):
        """Peak level of the left/right channel over the last ~20 ms, 0..255."""
        block = np.asarray(frames, dtype=np.float64)
        if block.ndim == 1:
            block = block[:, None]
        if block.size == 0:
            return 0, 0
        block = block[-self.level_frames :]
        peak = np.abs(block).max(axis=0) * gain
        left = min(255, int(min(1.0, float(peak[0])) * LEVEL_MAX) >> LEVEL_SHIFT)
        right = min(255, int(min(1.0, float(peak[-1])) * LEVEL_MAX) >> LEVEL_SHIFT)
        return left, right
