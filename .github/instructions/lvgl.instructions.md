---
applyTo: "src/plugins/lvgl/**,conf/envs/wt32_sc01.ini"
description: "LVGL touch UI: widget threading, LVGL 8.4 traps, screenshots over HTTP"
---

# LVGL touch UI

Details for `src/plugins/lvgl/**`. The general rules (the main loop as the single writer) are in
[`.github/copilot-instructions.md`](../copilot-instructions.md).

- **LVGL is not re-entrant**: never build or touch a widget tree from an LVGL event callback or a second
  task - record the intent and apply it in `update()`.
- **LVGL 8.4**: `lv_obj_get_x/y/width()` are layout-dependent (0 before the first layout pass - use
  `lv_obj_get_style_*`); `lv_label_set_text()` reallocates and restarts scroll animations when the
  pointer changes; every object is clickable by default; never call `lv_chart_remove_series()`.
- Verification: LVGL screens (`wt32_sc01_test1`, `DEBUG_LVGL_SCREENSHOT`) are captured over HTTP:
  `/lvgl-screen` and `/lvgl-screen.bmp`, documented in `src/plugins/lvgl/lvgl_debug.h`.
