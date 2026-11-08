---
applyTo: "scripts/tools/kfc_coredump.py,scripts/extra_script.py"
description: "Build archives, ESP32 core dumps and ESP8266 exceptions of a crashed device"
---

# Crash dumps, stack traces and build archives

Read this before touching a crashed device or decoding a stack trace. The general build, flash and verify
rules are in [`.github/copilot-instructions.md`](../copilot-instructions.md).

## Build archive

- `-t upload` / `-t uploadota` only add `erase_core_dump` (ESP32 only) before flashing, in `scripts/extra_script.py` -
  they do **not** archive. `-t buildarchive` builds, uploads and archives in one step and runs the archive step
  only after a successful upload, so a failed build or upload never leaves an archive behind.
- The ESP32 core dump is erased over **serial** (`esptool erase_region <coredump offset> <size>` taken from
  `$PARTITIONS_TABLE_CSV`) - no WebUI, no credentials, and it works when the device has crashed.
- A build is archived as `elf/<env>_<build>.tar.gz`: `firmware.elf`, `firmware.bin`, `filesystem.bin` (the
  `spiffs` partition read back from the device) plus `source/` (project + `lib/KFCLibrary`) and `info.txt`
  (build, env, git revisions, checksums, restore hints). `elf/archive.log` lists every archive; `elf/` is gitignored.
- **Archives are kept indefinitely**: every `-t buildarchive` leaves its own `elf/<env>_<build>.tar.gz` behind and
  nothing is ever deleted, so a crash can be decoded as long as that build was archived.
- The tar is written **straight from the working tree** (no staging copy, no temp directory) with
  `compresslevel=6`, and the esptool reads use `$UPLOAD_SPEED` with a fallback to esptool's 115200.

## ESP32 core dumps

- `scripts/tools/kfc_coredump.py info|download|trace|erase` is the only way to touch a device crash. It
  authenticates with the WebUI session id (user = device name = `KFC` + the last 3 bytes of the MAC upper
  hex, e.g. `arp -a | Select-String <ip>` -> `c4-4f-33-0a-42-71` -> `KFC0A4271`; default password 12345678);
  the address and environment are in `conf/envs/*.ini`.
- `trace` is the workflow: download `/coredump.bin`, match it against `elf/<env>_<build>.tar.gz` (the crash
  summary carries the first 16 hex of the ELF sha256), decode with `esp-coredump` plus the PlatformIO xtensa
  gdb and resolve the panic addresses with `addr2line`. Artifacts: `logs/coredump_<env>_<build>.{bin,elf}`
  and `logs/crash_<env>_<build>.{json,txt}`. `--erase` deletes the dump on the device afterwards.
- The download is a **20 byte flash header plus an ELF core dump** - do not hand `/coredump.bin` to
  `esp-coredump` as it is and `-t raw` is wrong for these dumps (`-t elf` on the stripped file is correct).
- Read the `reason:` line: `abort() was called at PC <addr>` names the trigger (`task_wdt_isr` = task
  watchdog, something starved IDLE0; `esp_core_dump_do_write_elf_pass` = the panic path itself faulted and the
  dump is incomplete, so the culprit task is missing - `IDLE0` is then only the panic context).
- Only `-t buildarchive` stores an archive, so a trace needs the crashed build to have been archived (a plain
  `-t upload` leaves none) - or `--elf` with the exact `firmware.elf` of that build, never a rebuilt one (the
  addresses would not match).
- The skill `.github/skills/trace-and-fix-a-crash/SKILL.md` (`/trace-and-fix-a-crash <ip>`) runs capture,
  decode, root cause and fix from the chat.
- PowerShell pipes into a file (`>`, `Tee-Object`) write **UTF-16** - use `Out-File -Encoding ascii` for
  captured tool output or greps find nothing in it.

## ESP8266 exceptions

- The exception is printed on serial (`Exception (NN)`, `epc1`, `>>>stack>>>`, `last failed alloc call`);
  resolve the `40xxxxxx` words with
  `~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-addr2line.exe -pfiaC -e .pio/build/<env>/firmware.elf`.
  The stack dump contains stale frames.
