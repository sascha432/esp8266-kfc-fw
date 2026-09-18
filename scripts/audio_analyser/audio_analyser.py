#!/usr/bin/env python3
#
# Author: sascha_lammers@gmx.de
#
"""Stream the audio spectrum of the Windows output device to a kfc_fw LED matrix.

Python replacement for the Windows application
`IoT-Audio-Visualization-Center <https://github.com/NimmLor/IoT-Audio-Visualization-Center>`_
(Analyzer.exe, C#/BASS/WASAPI) that sends WARLS packets to a single device.

    scripts\\audio_analyser\\.venv\\Scripts\\python.exe scripts\\audio_analyser\\audio_analyser.py --ip 192.168.0.196

The device has to be configured once: LED matrix firmware built with
``IOT_LED_MATRIX_ENABLE_VISUALIZER=1``, clock animation ``VISUALIZER``,
visualizer input ``UDP`` and the same port (default 21324).
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
import warnings

import numpy as np

try:
    import soundcard as sc
except ImportError:  # pragma: no cover - handled with a hint in main()
    sc = None

import warls
from spectrum import BAND_COUNT, FFT_SIZE, LOG_SCALE, SpectrumAnalyzer

DEFAULT_IP = '192.168.0.196'  # same default as the other scripts in scripts/tools
DEFAULT_RATE = 48000  # Windows shared mode mix format
DEFAULT_INTERVAL = 0.02  # 20 ms = 50 Hz, the C# app uses a 20 ms DispatcherTimer
DEFAULT_CHANNELS = 2  # soundcard records garbage on WASAPI if only one channel is used


# ----------------------------------------------------------------------------- devices


def list_devices():
    """Print all WASAPI loopback devices that can be recorded from."""
    loopbacks = loopback_devices()
    print('loopback (output) devices that can be recorded:')
    if not loopbacks:
        print('  none found')
    for index, mic in enumerate(loopbacks):
        try:
            channels = mic.channels
        except Exception:  # pragma: no cover - device disappeared
            channels = '?'
        print(f'  [{index}] {mic.name} ({channels} channels)')
    try:
        print(f'default speaker: {sc.default_speaker().name}')
    except Exception as exc:  # pragma: no cover - depends on the system
        print(f'default speaker: error - {exc}')
    return 0


def loopback_devices():
    return [mic for mic in sc.all_microphones(include_loopback=True) if mic.isloopback]


def find_loopback(selector=None):
    """Loopback device of the default speaker, by index or by part of the name."""
    loopbacks = loopback_devices()
    if not loopbacks:
        raise RuntimeError('no loopback device found, is a playback device installed?')

    if selector is None:
        try:
            name = sc.default_speaker().name
        except Exception:
            name = None
        for mic in loopbacks:
            if name is not None and mic.name == name:
                return mic
        return loopbacks[0]

    if selector.isdigit():
        index = int(selector)
        if 0 <= index < len(loopbacks):
            return loopbacks[index]
        raise RuntimeError(f'device index {index} does not exist, use --list')

    wanted = selector.lower()
    for mic in loopbacks:
        if wanted in mic.name.lower():
            return mic
    raise RuntimeError(f'no loopback device matches "{selector}", use --list')


# ----------------------------------------------------------------------------- helpers


def setup_warnings():
    """Print warnings as one line, the default output breaks the status line."""

    def showwarning(message, category, filename, lineno, file=None, line=None):
        print(f'{category.__name__}: {message}', file=file or sys.stderr)

    warnings.showwarning = showwarning


def ensure_stereo(block):
    """(frames, channels) float32 array with exactly two channels."""
    block = np.asarray(block, dtype=np.float32)
    if block.ndim == 1:
        block = block[:, None]
    if block.shape[1] == 1:
        block = np.repeat(block, 2, axis=1)
    elif block.shape[1] > 2:
        block = block[:, :2]
    return block


def status_line(destination, packet_size_bytes, seconds, packets, values, left, right, late):
    fps = packets / seconds if seconds > 0 else 0.0
    return (
        f'{destination[0]}:{destination[1]}  {fps:5.1f} fps  {packet_size_bytes:2d} B/pkt  '
        f'{fps * packet_size_bytes / 1024.0:5.1f} kB/s  '
        f'L {left:3d} R {right:3d}  bands min {min(values):3d} max {max(values):3d}  late {late}'
    )


def band_line(left, right, values):
    return f'L {left:3d} R {right:3d}  ' + ' '.join(f'{value:3d}' for value in values)


# ----------------------------------------------------------------------------- capture


def capture(args):
    """Capture the default (or selected) output device and send WARLS packets."""
    device = find_loopback(args.device)
    analyzer = SpectrumAnalyzer(
        bands=args.bands,
        fft_size=args.fft,
        log_scale=args.log_scale,
        rate=args.rate,
        gain=args.gain,
        smoothing=args.smooth,
    )
    destination = (args.ip, args.port)
    frames_per_tick = max(1, int(args.rate * args.interval))
    # WASAPI underruns if the block size is smaller than the amount of data read per tick
    blocksize = args.blocksize if args.blocksize > 0 else max(1024, frames_per_tick * 2)
    keep_frames = max(analyzer.fft_size * 2, frames_per_tick * 4)

    print(f'device:      {device.name}')
    print(f'destination: {destination[0]}:{destination[1]}{"  (dry run)" if args.dry_run else ""}')
    print(
        f'capture:     {args.rate} Hz, {analyzer.fft_size} point FFT, {args.bands} bands, '
        f'log scale {args.log_scale}, smoothing {args.smooth}, block size {blocksize}'
    )
    print('press Ctrl+C to stop')

    sock = None
    if not args.dry_run:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    buffer = np.zeros((0, DEFAULT_CHANNELS), dtype=np.float32)
    values = [0] * args.bands
    left = right = 0
    packets = 0
    late = 0
    started = False
    next_tick = time.perf_counter() + args.interval
    stats_time = time.perf_counter()
    try:
        with device.recorder(samplerate=args.rate, channels=DEFAULT_CHANNELS, blocksize=blocksize) as recorder:
            while True:
                block = recorder.record(numframes=None)
                if block.size:
                    buffer = np.concatenate((buffer, ensure_stereo(block)))
                    if buffer.shape[0] > keep_frames:
                        buffer = buffer[-keep_frames:]

                now = time.perf_counter()
                if now < next_tick:
                    continue
                if not started:
                    # wait for a full FFT window before sending the first packet
                    if buffer.shape[0] < analyzer.fft_size:
                        next_tick = now + args.interval
                        continue
                    started = True

                values = analyzer.spectrum(buffer)
                left, right = analyzer.levels(buffer, args.level_gain)
                packet = warls.build_packet(values, left, right, args.timeout)
                if sock is not None:
                    sock.sendto(packet, destination)
                packets += 1

                elapsed = now - stats_time
                if args.dump_bands:
                    print(f'\r{band_line(left, right, values)}', end='', flush=True)
                elif args.stats and elapsed >= args.stats:
                    print(
                        f'\r{status_line(destination, len(packet), elapsed, packets, values, left, right, late)}',
                        end='',
                        flush=True,
                    )
                    packets = 0
                    stats_time = now

                next_tick += args.interval
                if next_tick <= now:
                    # we are behind, drop the missed ticks instead of flooding the device
                    late += 1
                    next_tick = now + args.interval
    except KeyboardInterrupt:
        pass
    finally:
        if sock is not None:
            try:
                sock.sendto(warls.build_silent_packet(args.bands, args.timeout), destination)
            except OSError:
                pass
            sock.close()
        print()
    return 0


# ----------------------------------------------------------------------------- self test


def self_test():
    """Verify the band mapping, the level scaling and the packet layout."""
    failures = []

    def check(name, condition, info=''):
        print(f'  {"ok  " if condition else "FAIL"}  {name}' + (f'  -> {info}' if info else ''))
        if not condition:
            failures.append(name)

    print('band edges (32 bands, log scale 1.092, 16800 Hz max)')
    analyzer = SpectrumAnalyzer(bands=BAND_COUNT, fft_size=FFT_SIZE, log_scale=LOG_SCALE, rate=48000, smoothing=0)
    edges = analyzer.edges
    check('band 0  = 31.41 Hz', abs(edges[0] - 31.41) < 0.2, f'{edges[0]:.2f} Hz')
    check('band 1  = 68.60 Hz', abs(edges[1] - 68.60) < 0.3, f'{edges[1]:.2f} Hz')
    check('band 31 = 15384.6 Hz', abs(edges[31] - 15384.6) < 5.0, f'{edges[31]:.2f} Hz')
    print('        edges: ' + ' '.join(f'{edge:.0f}' for edge in edges))
    check('32 bands', len(edges) == 32)

    print('fft bins (48000 Hz, 2048 point FFT)')
    csharp_bins = [int((edge / 22050.0) * (FFT_SIZE // 2)) for edge in edges]
    check('bin(31.41 Hz) = 1', analyzer.bins[0] == 1, f'{analyzer.bins[0]}')
    check('bin(68.60 Hz) = 2', analyzer.bins[1] == 2, f'{analyzer.bins[1]}')
    check('bin(15384.6 Hz) = 656', analyzer.bins[31] == 656, f'{analyzer.bins[31]}')
    print(f'        bins: {analyzer.bins}')
    print(f'        C# (22050 Hz reference): {csharp_bins}')

    print('warls packet (32 bands)')
    packet = warls.build_packet([0] * 32, 10, 20, warls.DEFAULT_TIMEOUT)
    check('42 bytes', len(packet) == 42, f'{len(packet)}')
    check('4 byte aligned', (len(packet) - 2) % 4 == 0)
    check('header 1,5,222,77,10,20,66,32', packet[:8] == bytes((1, 5, 222, 77, 10, 20, 66, 32)), f'{list(packet[:8])}')
    check('2 padding bytes', packet[40:] == b'\x00\x00')
    check('silent packet is all zero', warls.build_silent_packet(32)[8:] == bytes(34))

    print('sine waves (bin centered, 48000 Hz)')
    rate = 48000
    time_axis = np.arange(analyzer.fft_size) / rate

    def sine(frequency, amplitude):
        samples = (amplitude * np.sin(2 * np.pi * frequency * time_axis)).astype(np.float32)
        return np.repeat(samples[:, None], 2, axis=1)

    for tone_bin in (4, 43, 213):
        frequency = tone_bin * rate / analyzer.fft_size
        # the bin of the upper edge belongs to the next band (`b0 < b1` in the C# loop)
        expected = next(i for i, edge_bin in enumerate(analyzer.bins) if edge_bin > tone_bin)
        quiet = analyzer.spectrum(sine(frequency, 0.01))
        index = int(np.argmax(quiet))
        check(f'{frequency:7.2f} Hz (bin {tone_bin:3d}) peaks in band {index:2d}', index == expected, f'expected {expected}')
        check(f'{frequency:7.2f} Hz @ 0.01 = {quiet[index]:3d}', 60 <= quiet[index] <= 85, '60..85')
        loud = analyzer.spectrum(sine(frequency, 0.5))
        check(f'{frequency:7.2f} Hz @ 0.5  = {loud[index]:3d}', loud[index] == 255, '255')
        left, right = analyzer.levels(sine(frequency, 0.5))
        check(f'{frequency:7.2f} Hz level    = {left}/{right}', left == 128 and right == 128, '128/128')

    print('silence')
    analyzer = SpectrumAnalyzer(bands=BAND_COUNT, fft_size=FFT_SIZE, log_scale=LOG_SCALE, rate=rate, smoothing=0)
    silent = np.zeros((analyzer.fft_size, 2), dtype=np.float32)
    check('bands are zero', max(analyzer.spectrum(silent)) == 0)
    check('levels are zero', analyzer.levels(silent) == (0, 0))

    print('smoothing')
    analyzer = SpectrumAnalyzer(bands=BAND_COUNT, fft_size=FFT_SIZE, log_scale=LOG_SCALE, rate=rate, smoothing=2)
    check('first spectrum is unsmoothed', analyzer.spectrum(silent) == [0] * BAND_COUNT)
    check('average is smooth', len(analyzer.spectrum(silent)) == BAND_COUNT)

    print()
    if failures:
        print(f'{len(failures)} check(s) failed: {", ".join(failures)}')
        return 1
    print('all checks passed')
    return 0


# ----------------------------------------------------------------------------- cli


def bands_type(value):
    bands = int(value)
    if not 2 <= bands <= warls.MAX_BANDS:
        raise argparse.ArgumentTypeError(f'must be 2..{warls.MAX_BANDS}')
    return bands


def timeout_type(value):
    timeout = int(value)
    if not 0 <= timeout <= 255:
        raise argparse.ArgumentTypeError('must be 0..255 seconds')
    return timeout


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        prog='audio_analyser',
        description='Send the spectrum and loudness of the Windows output device to a single kfc_fw LED matrix.',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument('--ip', default=DEFAULT_IP, help='destination IPv4 address (use the broadcast address to reach several devices)')
    parser.add_argument('--port', type=int, default=warls.DEFAULT_PORT, help='UDP port of the device')
    parser.add_argument('--device', default=None, help='loopback device, index or part of the name')
    parser.add_argument('--list', action='store_true', help='list the loopback devices and exit')
    parser.add_argument('--rate', type=int, default=DEFAULT_RATE, help='capture sample rate in Hz')
    parser.add_argument('--interval', type=float, default=DEFAULT_INTERVAL, help='seconds between two packets')
    parser.add_argument('--bands', type=bands_type, default=BAND_COUNT, help=f'number of bands, 2..{warls.MAX_BANDS}')
    parser.add_argument('--fft', type=int, default=FFT_SIZE, help='FFT size in samples')
    parser.add_argument('--log-scale', type=float, default=LOG_SCALE, help='band spacing, 1.0 = linear')
    parser.add_argument('--smooth', type=int, default=2, help='average the last N spectra, 0/1 disables smoothing')
    parser.add_argument('--gain', type=float, default=1.0, help='band gain, applied to the magnitude before sqrt()')
    parser.add_argument('--level-gain', type=float, default=1.0, help='loudness (VU meter) gain')
    parser.add_argument('--timeout', type=timeout_type, default=warls.DEFAULT_TIMEOUT, help='device idle timeout in seconds')
    parser.add_argument('--blocksize', type=int, default=0, help='WASAPI block size in frames, 0 = auto')
    parser.add_argument('--stats', type=float, default=1.0, help='print a status line every N seconds, 0 = off')
    parser.add_argument('--dump-bands', action='store_true', help='print the band values instead of the status line')
    parser.add_argument('--dry-run', action='store_true', help='analyze the audio but do not send packets')
    parser.add_argument('--self-test', action='store_true', help='run the built in DSP and protocol test, then exit')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if args.self_test:
        return self_test()
    setup_warnings()
    if sc is None:
        print('error: the "soundcard" module is missing', file=sys.stderr)
        print(f'install it with: {sys.executable} -m pip install -r requirements.txt', file=sys.stderr)
        return 2
    if args.list:
        return list_devices()
    try:
        return capture(args)
    except (RuntimeError, ValueError, IndexError) as exc:
        print(f'error: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
