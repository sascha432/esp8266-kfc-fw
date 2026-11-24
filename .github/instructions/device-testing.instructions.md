---
applyTo: "scripts/tools/**"
description: "Flashing and testing a device: serial port and logs, PC tools, resets, WebUI login"
---

# Flashing and testing a device

The build, flash and device test rules (which envs are serial only, what a test must not touch) are in
[`.github/copilot-instructions.md`](../copilot-instructions.md), section "Build, flash and verify".

- **PC tools** (`scripts/tools`) run in the workspace venv
  `.venv` (packages in `requirements.txt`, `python -m venv .venv` + `.venv\Scripts\python -m pip install -r
  requirements.txt`); the build scripts run in PlatformIO's own Python. Never add a hardcoded `sys.path` to a
  `site-packages` directory - a missing package goes into `requirements.txt`.
- Serial log (pass the env's `monitor_port`): `.venv\Scripts\python scripts/tools/serial_log.py <port> <seconds>
  logs/x.txt` opens the port with DTR/RTS low (no reset of the board) and writes UTF-8 with a timestamp per line.
  ESP8266 output contains NUL bytes (`tr -d '\000'` before grepping). PowerShell redirects (`>`) write UTF-16 -
  greps find nothing in such a file.
- Free the serial port first: a leftover `platformio.exe ... monitor` blocks the upload
  (`Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'platformio.exe.*monitor' }`).
  Several resets in a row can leave the device in **safe mode** (plugins off) - one more reset clears it.
- If the user forgot to disconnect their terminal or if the device becomes unresponsive, reset it via
  serial - that requires terminating the user's process. Don't use web uploads/OTA if serial programming
  is selected. Don't terminate uploads over serial that block, wait for them.
- The WebUI **user name is the device name**; the default password is 12345678.
- `scripts/tools/webui_test.py` checks pages, endpoints, forms and WebSockets in bulk - see
  [`.github/instructions/esp8266-test-env.instructions.md`](esp8266-test-env.instructions.md).
