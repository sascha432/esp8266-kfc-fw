#!/usr/bin/env python3

"""Generate the LVGL icon fonts of the UI (Material Design Icons, subset).

The firmware does not ship the whole MDI font. Only the glyphs the screens draw are converted, and
only at the sizes the layouts use, because LVGL has no runtime font scaling: every size is its own
font file and every glyph in it costs flash.

The names in ICONS are MDI names (the `mdi-<name>` class of the stylesheet). Their code points are
read from the bundled @mdi/font stylesheet, so a name is never written down twice and a typo fails
here instead of on the device (same tool flow as the STM32 `src/fonts/generate_fonts.py`).

Sources, bundled next to this script (see README.md, both are fetched with --download):
    materialdesignicons.css            @mdi/font stylesheet, the name -> code point table
    materialdesignicons-webfont.ttf    @mdi/font glyph outlines

Output:
    src/plugins/lvgl/fonts/lv_font_mdi_<size>.c    one font per ICON_SIZES entry, compiled by the
                                                   lvgl plugin (needs a build_src_filter entry)
    include/lvgl_mdi_icons.h                       LVGL_MDI_<ICON> string literals (UTF-8)
    scripts/tools/fonts/mdi_preview.html           specimen for review (--preview)

Usage:
    py scripts/tools/fonts/generate_lvgl_fonts.py                # generate everything
    py scripts/tools/fonts/generate_lvgl_fonts.py --check         # verify only, changes nothing
    py scripts/tools/fonts/generate_lvgl_fonts.py --download      # fetch the MDI sources first
    py scripts/tools/fonts/generate_lvgl_fonts.py --only-size 48  # regenerate a single size
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------------------------
# what is generated
# ---------------------------------------------------------------------------------------------

# (size in pixels, bit depth). The layouts of the screens are built for these two sizes; a third
# size is another font file, so keep the list short. 4 bpp for the small size (inline glyphs),
# 2 bpp for the large one (the larger pixel grid hides the harder edges and saves flash).
ICON_SIZES = (
    (20, 4),
    (48, 2),
)

# The icon set: (name of the firmware, name in the MDI stylesheet). The firmware name is the
# IconType enumerator of lvgl_ui.h and becomes the LVGL_MDI_<name> literal in the generated header.
ICONS = (
    # weather conditions (the screens map the condition id to these)
    ("SUN", "weather-sunny"),
    ("PARTLY_CLOUDY", "weather-partly-cloudy"),
    ("CLOUDY", "weather-cloudy"),
    ("RAIN", "weather-rainy"),
    ("SNOW", "weather-snowy"),
    ("STORM", "weather-lightning"),
    ("FOG", "weather-fog"),
    ("UNKNOWN", "alert-circle-outline"),
    # icons of the screen overview (LVGLScreen::getIcon())
    ("HOUSE", "home"),
    ("CALENDAR", "calendar"),
    ("MOON", "moon-waning-crescent"),
    ("INFO", "information-outline"),
    ("CLOCK", "clock-outline"),
    ("GLOBE", "earth"),
    ("POWER", "flash"),
    ("PICTURE", "image"),
    # status icons of the dashboards (tiles of the home assistant screen)
    ("BULB", "lightbulb"),
    ("PLUG", "power-plug"),
    ("TOGGLE", "toggle-switch"),
    ("THERMOMETER", "thermometer"),
    ("HUMIDITY", "water-percent"),
    ("BUTTON", "gesture-tap-button"),
    ("DIMMER", "lightbulb-on"),
    ("RADIATOR", "radiator"),
    ("FAN", "fan"),
    # glyphs of the climate mode bar and the panels
    ("POWER_SYMBOL", "power"),
    ("FLAME", "fire"),
    ("SNOWFLAKE", "snowflake"),
    ("MOTION", "motion-sensor"),
    ("AREA", "folder"),
    ("BACK", "arrow-left"),
    ("PALETTE", "palette"),
    ("EFFECT", "creation"),
    ("RECORD", "record"),
    ("PLUS", "plus"),
    ("MINUS", "minus"),
    # state glyphs and the buttons of the light panel (appended, the values above stay stable)
    ("TOGGLE_OFF", "toggle-switch-off"),
    ("BRIGHTNESS", "brightness-6"),
    ("COLOR_TEMP", "thermometer-lines"),
    # tile icons of the home assistant dashboard (appended, `icon: flash`/`icon: co2`)
    ("CO2", "molecule-co2"),
    ("LOCK", "lock"),
    # general purpose tile icons (appended, the MDI name is the value of the `icon:` key)
    ("GAUGE", "gauge"),
    ("CAMERA", "camera"),
    ("REMOTE", "remote"),
    ("LIGHTBULB_OFF", "lightbulb-off"),
    ("LIGHTBULB_ON", "lightbulb-on"),
    ("FLASH_OFF", "flash-off"),
    ("HOME", "home"),
    ("HOME_ASSISTANT", "home-assistant"),
    # WiFi state of the quick settings header of the home assistant dashboard (appended, one glyph
    # per state: associated, connecting, error and switched off)
    ("WIFI", "wifi"),
    ("WIFI_REMOVE", "wifi-remove"),
    ("WIFI_ALERT", "wifi-alert"),
    ("WIFI_OFF", "wifi-off"),
    # state and device_class icons of the home assistant tiles (appended, the values above stay
    # stable). The off state of an icon and the glyphs a `device_class` maps to
    ("POWER_PLUG_OFF", "power-plug-off"),
    ("MOTION_SENSOR_OFF", "motion-sensor-off"),
    ("SINE_WAVE", "sine-wave"),
    ("CURRENT_AC", "current-ac"),
    ("LIGHTNING_BOLT", "lightning-bolt"),
    # glyphs of the climate panel options (appended, the values above stay stable): the mode, the
    # preset and the fan mode draw one glyph per value, and the power button of a light panel uses
    # the power symbol of the entity state
    ("AUTORENEW", "autorenew"),
    ("WATER", "water"),
    ("FAN_AUTO", "fan-auto"),
    ("WINDY", "weather-windy"),
    ("POWER_OFF", "power-off"),
)

# Theme color of every icon (LVGLUI constants, used by the preview and verified against the
# kIconColor table of lvgl_ui.cpp by --check). An active (filled) tile redraws them white.
ICON_COLORS = {
    "SUN": "kColorHighlight",
    "PARTLY_CLOUDY": "kColorHighlight",
    "CLOUDY": "kColorTextValue",
    "RAIN": "kColorAccent",
    "SNOW": "kColorAccent",
    "STORM": "kColorHighlight",
    "FOG": "kColorTextValue",
    "UNKNOWN": "kColorTextMuted",
    "HOUSE": "kColorTextValue",
    "CALENDAR": "kColorTextValue",
    "MOON": "kColorText",
    "INFO": "kColorAccent",
    "CLOCK": "kColorTextValue",
    "GLOBE": "kColorAccent",
    "POWER": "kColorActive",
    "PICTURE": "kColorTextValue",
    "BULB": "kColorHighlight",
    "PLUG": "kColorTextValue",
    "TOGGLE": "kColorTextValue",
    "THERMOMETER": "kColorActive",
    "HUMIDITY": "kColorAccent",
    "BUTTON": "kColorTextValue",
    "DIMMER": "kColorHighlight",
    "RADIATOR": "kColorActive",
    "FAN": "kColorAccent",
    "POWER_SYMBOL": "kColorTextValue",
    "FLAME": "kColorTextValue",
    "SNOWFLAKE": "kColorTextValue",
    "MOTION": "kColorHighlight",
    "AREA": "kColorAccent",
    "BACK": "kColorTextValue",
    "PALETTE": "kColorAccent",
    "EFFECT": "kColorHighlight",
    "RECORD": "kColorTextValue",
    "PLUS": "kColorTextValue",
    "MINUS": "kColorTextValue",
    "TOGGLE_OFF": "kColorTextValue",
    "BRIGHTNESS": "kColorTextValue",
    "COLOR_TEMP": "kColorTextValue",
    "CO2": "kColorAccent",
    "LOCK": "kColorTextValue",
    "GAUGE": "kColorActive",
    "CAMERA": "kColorTextValue",
    "REMOTE": "kColorTextValue",
    "LIGHTBULB_OFF": "kColorTextValue",
    "LIGHTBULB_ON": "kColorHighlight",
    "FLASH_OFF": "kColorTextValue",
    "HOME": "kColorTextValue",
    "HOME_ASSISTANT": "kColorAccent",
    "WIFI": "kColorTextValue",
    "WIFI_REMOVE": "kColorTextMuted",
    "WIFI_ALERT": "kColorError",
    "WIFI_OFF": "kColorTextMuted",
    "POWER_PLUG_OFF": "kColorTextValue",
    "MOTION_SENSOR_OFF": "kColorTextValue",
    "SINE_WAVE": "kColorAccent",
    "CURRENT_AC": "kColorAccent",
    "LIGHTNING_BOLT": "kColorHighlight",
    "AUTORENEW": "kColorTextValue",
    "WATER": "kColorAccent",
    "FAN_AUTO": "kColorTextValue",
    "WINDY": "kColorTextValue",
    "POWER_OFF": "kColorTextValue",
}

# ---------------------------------------------------------------------------------------------
# locations
# ---------------------------------------------------------------------------------------------

# pinned version of the bundled sources (see README.md)
MDI_VERSION = "7.4.47"
MDI_URL = "https://cdn.jsdelivr.net/npm/@mdi/font@{}".format(MDI_VERSION)

MDI_CSS = "materialdesignicons.css"
MDI_TTF = "materialdesignicons-webfont.ttf"

FONT_DIR = Path("src/plugins/lvgl/fonts")
HEADER_FILE = Path("include/lvgl_mdi_icons.h")
PREVIEW_FILE = Path("scripts/tools/fonts/mdi_preview.html")

# theme colors of lvgl_ui.h, needed by the preview (the values are duplicated there, the preview
# is a review artifact and never compiled)
THEME_COLORS = {
    "kColorBackground": "0x0b0f14",
    "kColorBar": "0x12181f",
    "kColorCard": "0x182029",
    "kColorCardAlt": "0x161d25",
    "kColorBorder": "0x2b3844",
    "kColorText": "0xffffff",
    "kColorTextValue": "0xc8d4dd",
    "kColorTextLabel": "0x7fa8c9",
    "kColorTextMuted": "0x4d6274",
    "kColorAccent": "0x00ffff",
    "kColorHighlight": "0xffff00",
    "kColorActive": "0xff8000",
    "kColorAlert": "0xff8000",
    "kColorError": "0xff0000",
}


def find_repo_root(script_path: Path) -> Path:
    """The PlatformIO project root (the folder that holds platformio.ini)."""
    for candidate in (script_path.resolve().parent, *script_path.resolve().parents):
        if (candidate / "platformio.ini").is_file():
            return candidate
    raise FileNotFoundError("platformio.ini not found, run the script from the project")


def find_asset(script_dir: Path, name: str, override: str | None) -> Path:
    if override:
        candidate = Path(override).expanduser().resolve()
        if candidate.is_file():
            return candidate
        raise FileNotFoundError("not found: {}".format(candidate))
    candidate = script_dir / name
    if candidate.is_file():
        return candidate
    raise FileNotFoundError("{} is missing, run the script with --download".format(candidate))


def download_assets(script_dir: Path) -> None:
    """Fetch the pinned @mdi/font sources next to this script (once)."""
    script_dir.mkdir(parents=True, exist_ok=True)
    for remote, name in (("css/{}".format(MDI_CSS), MDI_CSS), ("fonts/{}".format(MDI_TTF), MDI_TTF)):
        url = "{}/{}".format(MDI_URL, remote)
        target = script_dir / name
        if target.is_file():
            print("keep     {}".format(target.name))
            continue
        print("download {}".format(url))
        with urllib.request.urlopen(url, timeout=120) as response:  # noqa: S310 - pinned https URL
            target.write_bytes(response.read())
        print("         {} ({} bytes)".format(target.name, target.stat().st_size))


# ---------------------------------------------------------------------------------------------
# MDI stylesheet -> name/code point table
# ---------------------------------------------------------------------------------------------


def read_codepoints(css_path: Path) -> dict[str, int]:
    """`.mdi-<name>::before { content: "\\Fxxxx"; }` -> {name: code point}"""
    text = css_path.read_text(encoding="utf-8", errors="replace")
    pattern = re.compile(r"\.mdi-([a-z0-9-]+)(?:::?before)?\s*\{[^}]*?content:\s*[\"']\\([0-9A-Fa-f]{2,6})[\"']")
    codepoints: dict[str, int] = {}
    for match in pattern.finditer(text):
        codepoints[match.group(1)] = int(match.group(2), 16)
    if not codepoints:
        raise RuntimeError("no icons found in {}".format(css_path))
    return codepoints


def resolve_icons(codepoints: dict[str, int]) -> tuple[tuple[str, str, int], ...]:
    """(firmware name, mdi name, code point), fails on a name that has no glyph"""
    resolved = []
    missing = []
    for name, mdi_name in ICONS:
        codepoint = codepoints.get(mdi_name)
        if codepoint is None:
            missing.append(mdi_name)
            continue
        resolved.append((name, mdi_name, codepoint))
    if missing:
        raise RuntimeError("not in the MDI stylesheet: {}".format(", ".join(missing)))
    return tuple(resolved)


def codepoint_ranges(codepoints: tuple[int, ...]) -> list[str]:
    """sorted code points -> lv_font_conv --range arguments"""
    values = sorted(set(codepoints))
    if not values:
        return []
    ranges: list[str] = []
    start = previous = values[0]
    for value in values[1:]:
        if value == previous + 1:
            previous = value
            continue
        ranges.append("0x{:X}-0x{:X}".format(start, previous) if start != previous else "0x{:X}".format(start))
        start = previous = value
    ranges.append("0x{:X}-0x{:X}".format(start, previous) if start != previous else "0x{:X}".format(start))
    return ranges


# ---------------------------------------------------------------------------------------------
# generators
# ---------------------------------------------------------------------------------------------


def _relative(repo_root: Path, path: Path) -> str:
    """project relative path (forward slashes, keeps the generated file reproducible)"""
    try:
        return str(path.resolve().relative_to(repo_root.resolve())).replace("\\", "/")
    except ValueError:
        return str(path)


def run_font_conv(repo_root: Path, font_conv: str, ttf: Path, size: int, bpp: int, output_file: Path,
                  codepoints: tuple[int, ...]) -> None:
    # npx is a .cmd on windows, the shell wrapper cannot be created without the extension
    parts = font_conv.split()
    executable = shutil.which(parts[0])
    if executable is None:
        raise RuntimeError("{} was not found on PATH".format(parts[0]))
    cmd = [
        executable,
        *parts[1:],
        "--size", str(size),
        "--bpp", str(bpp),
        "--format", "lvgl",
        "--lv-include", "lvgl.h",
        "--font", _relative(repo_root, ttf),
        "--lv-font-name", output_file.stem,
        "-o", _relative(repo_root, output_file),
    ]
    for glyph_range in codepoint_ranges(codepoints):
        cmd.extend(("--range", glyph_range))
    subprocess.run(cmd, cwd=repo_root, check=True)


def utf8_escape(codepoint: int) -> str:
    """UTF-8 bytes of a code point as a C string escape (MDI lives above the BMP)"""
    return "".join("\\x{:02X}".format(byte) for byte in chr(codepoint).encode("utf-8"))


def write_header(repo_root: Path, resolved: tuple[tuple[str, str, int], ...]) -> None:
    font_names = "/lv_font_mdi_".join(str(size) for size, _ in ICON_SIZES)
    lines = [
        "/**",
        " * Author: sascha_lammers@gmx.de",
        " *",
        " * Material Design Icons (MDI) - generated by scripts/tools/fonts/generate_lvgl_fonts.py,",
        " * do not edit. {} icons of @mdi/font {}, the fonts are lv_font_mdi_{} in".format(
            len(resolved), MDI_VERSION, font_names),
        " * src/plugins/lvgl/fonts/ (see LVGLUI::kIconFonts).",
        " *",
        " * The literals are the UTF-8 encoded glyphs, use them with a label that has one of the icon",
        " * fonts selected (LVGLUI::createIcon()/setIcon()).",
        " */",
        "",
        "#pragma once",
        "",
        "// name in the firmware -> glyph (the enumerator of LVGLUI::IconType)",
        "",
    ]
    width = max(len(name) for name, _, _ in resolved)
    for name, mdi_name, codepoint in resolved:
        lines.append("#define LVGL_MDI_{:<{}}  \"{}\" // mdi-{} U+{:04X}".format(
            name, width, utf8_escape(codepoint), mdi_name, codepoint))
    lines.append("")
    target = repo_root / HEADER_FILE
    target.write_text("\n".join(lines), encoding="utf-8")
    print("wrote    {} ({} icons)".format(HEADER_FILE, len(resolved)))


def css_color(name: str) -> str:
    return "#" + THEME_COLORS[name][2:]


def write_preview(script_dir: Path, resolved: tuple[tuple[str, str, int], ...], preview_path: Path) -> None:
    cards = []
    for name, mdi_name, codepoint in resolved:
        color = css_color(ICON_COLORS[name])
        small = '<span class="gl" style="font-size:20px">&#x{:X};</span>'.format(codepoint)
        large = '<span class="gl" style="font-size:48px">&#x{:X};</span>'.format(codepoint)
        active = '<span class="gl" style="font-size:48px;color:#ffffff">&#x{:X};</span>'.format(codepoint)
        cards.append(
            '<div class="card">'
            '<div class="head"><b>{}</b><span>mdi-{} &middot; U+{:04X}</span></div>'
            '<div class="body">'
            '<div class="box" style="color:{}">{} {}</div>'
            '<div class="box active">{}</div>'
            '<div class="name" style="color:{}">{}</div>'
            "</div></div>".format(name, mdi_name, codepoint, color, small, large, active, color, ICON_COLORS[name])
        )
    sizes = ", ".join("{} px / {} bpp".format(size, bpp) for size, bpp in ICON_SIZES)
    html = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>LVGL MDI icon font - specimen</title>
<style>
    @font-face {{
        font-family: "Material Design Icons";
        src: url("{ttf}") format("truetype");
        font-weight: normal;
        font-style: normal;
    }}
    body {{ background: #{background}; color: #{text}; font-family: monospace; margin: 16px; }}
    h1 {{ font-size: 18px; }} h2 {{ font-size: 14px; color: #{label}; }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(320px, 1fr)); gap: 8px; }}
    .card {{ background: #{card}; border: 2px solid #{border}; border-radius: 8px; padding: 8px; }}
    .head {{ font-size: 12px; color: #{label}; margin-bottom: 6px; }}
    .head b {{ color: #{text}; font-size: 14px; }} .head span {{ margin-left: 8px; }}
    .body {{ display: flex; align-items: center; gap: 10px; }}
    .box {{ display: flex; align-items: center; justify-content: center; gap: 4px;
            width: 96px; height: 96px; background: #{card}; border: 1px solid #{border}; border-radius: 8px; }}
    .box.active {{ background: #{active}; border-color: #{active}; }}
    .name {{ font-size: 11px; color: #{label}; }}
    .gl {{ font-family: "Material Design Icons"; font-weight: normal; line-height: 1; }}
</style>
</head>
<body>
<h1>LVGL MDI icon font &middot; @mdi/font {version} &middot; {count} icons &middot; {sizes}</h1>
<h2>Every icon at both sizes in its theme color, left: color on a card, middle: white on an active (filled) card.</h2>
<div class="grid">
{cards}
</div>
</body>
</html>
""".format(ttf=MDI_TTF, version=MDI_VERSION, count=len(resolved), sizes=sizes,
           background=THEME_COLORS["kColorBackground"][2:], text=THEME_COLORS["kColorText"][2:],
           label=THEME_COLORS["kColorTextLabel"][2:], card=THEME_COLORS["kColorCard"][2:],
           border=THEME_COLORS["kColorBorder"][2:], active=THEME_COLORS["kColorActive"][2:],
           cards="\n".join(cards))
    preview_path.write_text(html, encoding="utf-8")
    print("wrote    {}".format(preview_path))


# ---------------------------------------------------------------------------------------------
# checks
# ---------------------------------------------------------------------------------------------


def check_icon_colors(repo_root: Path, resolved: tuple[tuple[str, str, int], ...]) -> list[str]:
    """the kIconColor table of lvgl_ui.cpp must match ICON_COLORS, in the order of ICONS"""
    source = (repo_root / "src/plugins/lvgl/lvgl_ui.cpp").read_text(encoding="utf-8", errors="replace")
    match = re.search(r"kIconColor\[\w*\]\s*=\s*\{(.*?)\};", source, re.DOTALL)
    if not match:
        return ["kIconColor[] not found in src/plugins/lvgl/lvgl_ui.cpp"]
    colors = re.findall(r"kColor\w+", match.group(1))
    expected = [ICON_COLORS[name] for name, _, _ in resolved]
    if colors != expected:
        return ["kIconColor[] does not match ICON_COLORS:\n    table:    {}\n    expected: {}".format(
            ", ".join(colors), ", ".join(expected))]
    return []


def check_header(repo_root: Path, resolved: tuple[tuple[str, str, int], ...]) -> list[str]:
    target = repo_root / HEADER_FILE
    if not target.is_file():
        return ["{} is missing, run the generator".format(HEADER_FILE)]
    names = re.findall(r"#define\s+LVGL_MDI_(\w+)", target.read_text(encoding="utf-8"))
    expected = [name for name, _, _ in resolved]
    if names != expected:
        return ["{} does not match the icon list ({} defines, {} icons)".format(HEADER_FILE, len(names), len(expected))]
    return []


def check_firmware_names(repo_root: Path, resolved: tuple[tuple[str, str, int], ...]) -> list[str]:
    """every LVGL_MDI_* used by the firmware has to be generated"""
    known = {"LVGL_MDI_" + name for name, _, _ in resolved}
    used: set[str] = set()
    for path in list((repo_root / "src").rglob("*.cpp")) + list((repo_root / "src").rglob("*.h")):
        used.update(re.findall(r"LVGL_MDI_\w+", path.read_text(encoding="utf-8", errors="replace")))
    unknown = sorted(used - known)
    if unknown:
        return ["used by the firmware but not generated: {}".format(", ".join(unknown))]
    return []


# ---------------------------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="verify the generated files, write nothing")
    parser.add_argument("--download", action="store_true", help="fetch the pinned MDI sources first")
    parser.add_argument("--font-path", help="path to materialdesignicons-webfont.ttf")
    parser.add_argument("--css-path", help="path to materialdesignicons.css")
    parser.add_argument("--font-conv", default="npx --yes lv_font_conv",
                        help="lv_font_conv command (default: %(default)s)")
    parser.add_argument("--only-size", type=int, help="regenerate a single size (20 or 48)")
    parser.add_argument("--preview", help="path of the preview html (default: {})".format(PREVIEW_FILE))
    args = parser.parse_args()

    script_path = Path(__file__)
    script_dir = script_path.parent
    repo_root = find_repo_root(script_path)

    if args.download:
        download_assets(script_dir)

    css_path = find_asset(script_dir, MDI_CSS, args.css_path)
    font_path = find_asset(script_dir, MDI_TTF, args.font_path)

    codepoints = read_codepoints(css_path)
    resolved = resolve_icons(codepoints)
    print("{} icons of @mdi/font {}, {} sizes".format(len(resolved), MDI_VERSION, len(ICON_SIZES)))

    if args.check:
        problems = check_header(repo_root, resolved) + check_icon_colors(repo_root, resolved) + check_firmware_names(repo_root, resolved)
        for size, _ in ICON_SIZES:
            if not (repo_root / FONT_DIR / "lv_font_mdi_{}.c".format(size)).is_file():
                problems.append("lv_font_mdi_{}.c is missing, run the generator".format(size))
        for problem in problems:
            print("ERROR    {}".format(problem))
        print("check    {}".format("failed" if problems else "ok"))
        return 1 if problems else 0

    font_dir = repo_root / FONT_DIR
    font_dir.mkdir(parents=True, exist_ok=True)
    for size, bpp in ICON_SIZES:
        if args.only_size and args.only_size != size:
            continue
        output = font_dir / "lv_font_mdi_{}.c".format(size)
        run_font_conv(repo_root, args.font_conv, font_path, size, bpp, output, tuple(cp for _, _, cp in resolved))
        print("wrote    {} ({} px, {} bpp, {} bytes)".format(
            output.relative_to(repo_root), size, bpp, output.stat().st_size))

    write_header(repo_root, resolved)
    write_preview(script_dir, resolved, Path(args.preview) if args.preview else script_dir / PREVIEW_FILE.name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
