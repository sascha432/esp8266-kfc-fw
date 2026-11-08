---
applyTo: "Resources/**,KFCWebBuilder.yaml,include/templates.h,src/templates.cpp"
description: "WebUI sources, the Python web builder, help texts and server-rendered forms"
---

# WebUI and forms

Details for `Resources/**` and the form templates. The rules for the filesystem image and for forms in
general are in [`.github/copilot-instructions.md`](../copilot-instructions.md).

- `Resources/html|js|css/**` are built into `data/webui/<hash>` (+ `.listings.txt`) by the Python web builder
  (`lib/KFCLibrary/KFCWebBuilder/kfc_webbuilder`, docs in `lib/KFCLibrary/KFCWebBuilder/README.md`) driven by
  `KFCWebBuilder.yaml`; `-t buildfs`/`rebuildfs` run it with PlatformIO's Python. Its preprocessor in the HTML
  pages is `<!--#if EXPR-->` / `<!--#else-->` / `<!--#endif-->`, `/*--#if EXPR--*/` inside `<script>`/`<style>`
  blocks; the expression cannot contain a `-`. Node.js, Java and uglify-js are auto-installed into
  `~/.platformio/kfc_webbuilder` if missing; gzip compression is zopfli (pip package, results cached by content
  hash in that directory).
- Built pages and assets are content-hashed (`data/webui/<hash>`), JS/CSS are gzip-compressed on disk and
  `data/webui/.listings.txt` maps a hash back to its original name - look a file up there, not by grep.
- Page-level help texts live in `Resources/html/<page>.html` as
  `div.form-help-block > div[data-target="#field_id"]`; `Resources/js/forms.js` applies them.
- Server-rendered forms come from a plugin's `createConfigureForm()` (`FormUI::Form::BaseForm`) and are
  streamed via `include/templates.h` + `src/templates.cpp` (`TemplateDataProvider`/`SSIProxyStream`).
