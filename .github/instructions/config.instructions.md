---
applyTo: "include/kfc_fw_config.h,include/kfc_fw_config/**,src/kfc_fw_config*"
description: "Config classes and NVS parameters, layout changes, the ESP8266 NVS handle"
---

# Configuration

Details for the config classes and their NVS storage. The general rules are in
[`.github/copilot-instructions.md`](../copilot-instructions.md).

- Config classes live in `include/kfc_fw_config.h` / `include/kfc_fw_config/**`; values are stored per
  parameter in NVS (`nvs2`) under a 16-bit CRC handle registered in `src/kfc_fw_config_classes.cpp`.
- A plugin implements a packed `ConfigStructType` and a handler deriving
  `ConfigGetterSetter<ConfigType, _H(...)>` (`include/kfc_fw_config/base.h`); bit fields use
  `CREATE_BITFIELD_TYPE_MIN_MAX(...)`.
- **Changing the layout resets the parameter**: `getConfig()` compares the stored size with
  `sizeof(ConfigStructType)` and falls back to the constructor defaults on a mismatch - a new or changed
  member resets that parameter **once** after flashing. That is the accepted design: put the config into
  the existing `ConfigStructType`/blob it belongs to and let it resize. Do **not** invent a separate
  parameter (its own handle + `Plugins::X` alias + `DECLARE/DEFINE_CONFIG_HANDLE_PROGMEM_STR`) or a
  nested config class just to avoid that one-time reset. Keep each bit field inside its storage unit or
  `-Wpacked-bitfield-compat` moves it silently.
- **No layout or version field for a binary config that breaks by size** (`KFCConfigurationClasses::Plugins::*`):
  the parameter is reset to the defaults (or treated as invalid) anyway, so a layout/version flag only widens
  the layout it is meant to guard - leave it out.
- **`getWriteableConfig()` creates an all-zero blob** for a parameter that does not exist yet, so a form
  can show 0 while `getConfig()` shows the constructor defaults. Add a range/default fallback in the
  getter (and note that a legal value of 0 cannot be told apart from "unset").
- **NVS (ESP8266)**: never deinitialize the partition while a handle is open - the handle becomes invalid (and
  older NVS library versions leaked it). `Configuration::_nvs_open()`/`_nvs_deinit()` in KFCLibrary take care of
  it; do not reopen a handle through `_nvs_close()`, it arms the deinit timer.
