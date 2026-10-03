---
name: trace-and-fix-a-crash
description: Download the core dump of a crashed KFC device, decode it against the matching build archive and fix the root cause
disable-model-invocation: true
argument-hint: "<device address> [environment] (e.g. 192.168.0.196 wled_esp32_controller)"
---
Follow the project rules in [`.github/copilot-instructions.md`](../copilot-instructions.md) - above all the
"Crash dumps and stack traces" and "Build, flash and verify" sections. They own the workflow, the device
access and the verification; do not restate or second-guess them and do not decode a dump by hand.

Target: the device given after this command (address or WebUI link, optionally the environment). Ask for
what is missing instead of guessing.

1. `python scripts/tools/kfc_coredump.py trace --host <ip> -u <device> -p <password> --erase`
2. Report what the dump proves - firmware (environment + build) and its archive, task, exception, fault
   address, the `reason:` line, the symbol of the abort caller, every frame - and what it does not. An
   incomplete or corrupted dump and the information missing because of it must be called out explicitly.
3. Only then look for the root cause in the sources of that build, and separate what the dump proves from
   what is a hypothesis.
4. Fix it, build both platforms, flash and verify on the device, then update `CHANGELOG.md` and the docs.

