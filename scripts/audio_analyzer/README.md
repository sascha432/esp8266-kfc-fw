# Audio Analyzer

Streams the audio spectrum and the loudness of the Windows output device to a single
kfc_fw LED matrix over UDP (WARLS protocol).

Python replacement for the Windows application
[IoT-Audio-Visualization-Center](https://github.com/NimmLor/IoT-Audio-Visualization-Center)
(`Analyzer.exe`, C#/BASS/WASAPI) without BASS, .NET, a GUI or device HTTP control:
capture the system audio with WASAPI loopback (`soundcard`), analyze it with `numpy`
and send one UDP packet every 20 ms.

## Setup

```bat
py -3 -m venv scripts\audio_analyzer\.venv
scripts\audio_analyzer\.venv\Scripts\python.exe -m pip install --upgrade pip
scripts\audio_analyzer\.venv\Scripts\python.exe -m pip install -r scripts\audio_analyzer\requirements.txt
```

`run.bat` calls the venv interpreter, no activation needed:

```bat
scripts\audio_analyzer\run.bat --ip 192.168.0.196
```

## Device configuration (once)

The kfc_fw device is the receiver, nothing has to be changed there:

* firmware built with `IOT_LED_MATRIX_ENABLE_VISUALIZER=1`
  (`conf/envs/led_matrix.ini`, `led_strip.ini`, `hexagon_panel.ini` or `wled_board.ini`)
* clock plugin: `animation` = `VISUALIZER`, `visualizer.input` = `UDP`,
  `visualizer.port` = 21324 (default of `IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT`),
  `multicast` off
* the animation can be switched from the WebUI, MQTT or AT mode

`src/plugins/clock/animation_visualizer.cpp` parses the packets and rejects anything
that does not match the layout below; unknown or misaligned packets are dropped
silently.

## Usage

```bat
run.bat --list                                  list the recordable output devices
run.bat --self-test                             verify the DSP and the packet layout
run.bat --dry-run                               analyze only, print the band values
run.bat --ip 192.168.0.21 --dump-bands          print the 32 band values and both levels
run.bat --ip 192.168.0.21 --gain 1.5            boost the bands if the display is too dark
run.bat --device "Realtek" --port 21324         pick another output device
```

| option | default | description |
| --- | --- | --- |
| `--ip` | `192.168.0.21` | destination, the broadcast address also works (all devices) |
| `--port` | `21324` | UDP port of the device |
| `--device` | default speaker | loopback device, index (see `--list`) or part of the name |
| `--rate` | `48000` | capture sample rate |
| `--interval` | `0.02` | seconds between packets (50 Hz, the C# app uses a 20 ms timer) |
| `--bands` | `32` | bands, 2..32; the device copies 32, doubles 16/8 and interpolates the rest |
| `--fft` | `2048` | FFT size |
| `--log-scale` | `1.092` | band spacing, `1.0` = linear |
| `--smooth` | `2` | average the last N spectra, 0/1 disables smoothing |
| `--gain` | `1.0` | band gain, applied to the magnitude before `sqrt()` |
| `--level-gain` | `1.0` | loudness (VU meter) gain |
| `--timeout` | `5` | device idle timeout in seconds, `0` disables the packets |
| `--blocksize` | auto | WASAPI block size in frames (`rate * interval * 2`) |
| `--stats` | `1.0` | status line interval in seconds |
| `--dump-bands` | off | print the band values instead of the status line |
| `--dry-run` | off | analyze the audio but send nothing |
| `--list` | | list the loopback devices and exit |
| `--self-test` | | run the built in tests and exit |

Ctrl+C sends one packet with zero levels and zero bands, so the display goes dark
immediately instead of waiting for the timeout.

## Protocol

One WARLS packet per update, identical to `UdpDevice.Send()` of the C# application.
For 32 bands it is 42 bytes long and `(size - 2)` is dividable by 4:

| offset | size | content |
| --- | --- | --- |
| 0 | 1 | protocol id, `1` = WARLS |
| 1 | 1 | idle timeout in seconds, `0` = packet is ignored, `255` = max |
| 2 | 2 | magic `0x4DDE` little endian (`222`, `77`) |
| 4 | 1 | left channel level, 0..255 |
| 5 | 1 | right channel level, 0..255 |
| 6 | 1 | magic `0x42` (`66`) |
| 7 | 1 | number of bands, 2..32 (`kVisualizerPacketSize`) |
| 8 | n | spectrum, one byte per band |
| 8+n | pad | zero bytes, `(size - 2) % 4 == 0` |

The old format of the C# app (port 4210), the video streaming mode and the device
HTTP endpoints (`/pattern`, `/power`, `/brightness`, `/all`) are not implemented.

## DSP

`spectrum.py` is a port of `AudioProcessor.cs`:

* 2048 point FFT of the combined stereo channels, Hann window, first 1024 bins
* band `k` is the peak bin of the range `[bins[k-1], bins[k])`; the bin of a band edge
  belongs to the next band (`b0 < b1` in the C# loop)
* band edges from `fIncr = 16800 / bands` and `freq(k) = px**k * fIncr * (k+1) / px**bands`
  -> 31.41 Hz .. 15384.6 Hz for 32 bands and a log scale of 1.092
* value `= clamp(int(sqrt(peak * gain) * 765 - 4))`, `peak` is normalized so that a full
  scale sine is 1.0
* levels are the peak of the last 20 ms, `min(255, int(peak * 32768) >> 7)` like
  `BASS_WASAPI_GetLevel()` (left = low word, right = high word) shifted by 7

### Differences to the C# application

| | C# | here |
| --- | --- | --- |
| band -> bin | `(freq / 22050) * 1024` | real sample rate, on 48 kHz the bands are ~9% lower and match their label |
| `range = 0.7` | dead code, the inner `getSpectrumData` loops a hard coded 32 bands and always returns 71 values | dropped, together with the 71 value padding and `sourceFactor` |
| stereo | BASS scaling (Hann window not compensated) | average of both channels, Hann coherent gain compensated |
| level | `(byte)(32768 >> 7)` wraps to 0 at full scale | clamped to 255 |
| smoothing | `sum(value / count)` with the partial history divided by the full count | average over the values available |
| port 4210 | legacy format without magic bytes | not supported |

The band count generalization (`fIncr = freq_max / bands`, `px**bands`) reproduces the
C# values for `--bands 32`.

## Troubleshooting

* `error: the "soundcard" module is missing` - the venv is not used, call `run.bat`
  or the venv interpreter directly.
* `The binary mode of fromstring is removed` or `'numpy.ndarray' object has no attribute
  'tostring'` - a soundcard version older than 0.4.4 is installed, those releases call
  numpy functions that were removed in numpy 2.x. `pip install -r requirements.txt`
  installs 0.4.6.
* no sound / all bands zero - `--list` and check `--device`, the recording follows the
  selected *output* device. Some applications need to be restarted to appear in the
  loopback stream.
* bands too dark or always saturated - adjust `--gain` (0.5 .. 4.0) and compare with
  `--dump-bands`.
* packets are lost - the device drops a packet when the FFT data does not fit into a
  UDP datagram; 32 bands = 42 bytes is far below the limit, so increase
  `--blocksize` if WASAPI underruns (`data discontinuity in recording` warnings).
