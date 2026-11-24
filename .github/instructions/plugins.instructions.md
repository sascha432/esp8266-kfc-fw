---
applyTo: "src/plugins/**,src/plugins.cpp,include/plugins.h,include/PluginComponent.h,src/PluginComponent.cpp,src/at_mode*.cpp,docs/AtModeHelp.md"
description: "Plugin structure and registration, AT commands versus the status page"
---

# Plugins

Details for adding or changing a plugin (`src/plugins/**`). Which plugins a build compiles and the main loop as
the single writer are in [`.github/copilot-instructions.md`](../copilot-instructions.md).

- **Plugins** derive from `PluginComponent` (`include/PluginComponent.h`), declare their metadata with
  `PROGMEM_DEFINE_PLUGIN_OPTIONS(...)` and register themselves with `REGISTER_PLUGIN(this, "Name")` in
  the constructor - the registry (`include/plugins.h`, `src/plugins.cpp`) sorts by `PriorityType` and
  calls `preSetup()` / `setup()` / `createMenu()`. There is no per-plugin `loop()`: a plugin adds a
  `LoopFunctions` callback or an `Event::Timer`.
- Smallest plugin to copy: `src/plugins/ssdp/` (one `.h` + one `.cpp`).
- **AT mode is the developer console, the status page is user UI.** AT commands (serial/WebSocket console) may print
  verbose diagnostics in plain text (counters, timings, internal state); a plugin's `getStatus()` is shown in the WebUI
  and keeps to a short summary in HTML. Both only list what the build compiles in - no counters of a transport or
  feature that is not part of the build. Document AT commands in `docs/AtModeHelp.md`.
