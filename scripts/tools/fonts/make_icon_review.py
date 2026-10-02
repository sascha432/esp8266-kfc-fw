"""
Icon approval sheet for the Home Assistant screen.

Renders every candidate glyph of a group with the bundled Material Design Icons font, so the
glyphs of the control bar of the light panel and the on/off versions of the state glyphs can be
picked before they are added to the subset font of the firmware.

  python scripts/tools/fonts/make_icon_review.py

Writes scripts/tools/fonts/icon_review.html (open it in a browser, no internet needed: the
stylesheet and the TTF are in the same directory).

The picked names still have to be added to ICONS of generate_lvgl_fonts.py and the fonts have to
be regenerated for the firmware to have them.
"""

import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
OUTPUT = HERE / 'icon_review.html'
CSS = HERE / 'materialdesignicons.css'

# groups of candidates: (title, explanation, [ (mdi name, note) ])
GROUPS = (
    (
        "On / off state glyphs (a state icon has to show the state)",
        "The tile draws the first one while the entity is on, the second one while it is off.",
        (
            ("toggle-switch", "current, used for every switch state"),
            ("toggle-switch-off", "proposed: off"),
            ("toggle-switch-outline", "alternative on"),
            ("toggle-switch-off-outline", "alternative off"),
        ),
    ),
    (
        "Power button of the light panel",
        "Turns the light on and off. The button is drawn as a white round background with the glyph.",
        (
            ("power", "current POWER_SYMBOL"),
            ("power-standby", "alternative"),
            ("power-plug", "alternative"),
            ("flash", "current IconType::POWER (the flash of the overview)"),
            ("lightbulb-on", "alternative"),
        ),
    ),
    (
        "Brightness (level) button of the light panel",
        "Switches the control to the level slider. Every light has it.",
        (
            ("brightness-6", "proposed"),
            ("brightness-7", "proposed"),
            ("brightness-4", "proposed"),
            ("white-balance-sunny", "alternative"),
            ("lightbulb-on", "alternative"),
            ("weather-sunny", "alternative"),
        ),
    ),
    (
        "RGB colour button of the light panel",
        "Shows the colour wheel. Only lights that report hs_color have it.",
        (
            ("palette", "current PALETTE"),
            ("palette-outline", "alternative"),
            ("color-helper", "alternative"),
            ("eyedropper", "alternative"),
            ("format-color-fill", "alternative"),
            ("gradient-horizontal", "alternative"),
        ),
    ),
    (
        "Colour temperature button of the light panel",
        "Second colour control, for lights that report color_temp.",
        (
            ("thermometer", "already used by the climate tiles and sensors"),
            ("thermometer-lines", "proposed"),
            ("temperature-celsius", "proposed"),
            ("temperature-kelvin", "proposed"),
            ("white-balance-incandescent", "alternative"),
        ),
    ),
    (
        "Effect button of the light panel",
        "Opens the effect list of the entity. Only lights that report effect_list have it.",
        (
            ("creation", "current EFFECT"),
            ("auto-fix", "alternative"),
            ("magic-staff", "alternative"),
            ("star-four-points", "alternative"),
            ("shimmer", "alternative"),
            ("lightbulb-multiple", "alternative"),
        ),
    ),
)


def read_codepoints(css_file: pathlib.Path) -> dict[str, int]:
    # .mdi-toggle-switch::before { content: "\F0521"; }
    pattern = re.compile(r'\.mdi-([a-z0-9-]+)::?before\s*\{\s*content:\s*"\\([0-9A-Fa-f]+)"', re.DOTALL)
    return {name: int(value, 16) for name, value in pattern.findall(css_file.read_text(encoding='utf-8'))}


def main() -> int:
    if not CSS.exists():
        print('{} is missing, run generate_lvgl_fonts.py --download first'.format(CSS.name))
        return 1
    codepoints = read_codepoints(CSS)
    missing = []
    sections = []
    for title, explanation, items in GROUPS:
        rows = []
        for name, note in items:
            codepoint = codepoints.get(name)
            if codepoint is None:
                missing.append(name)
                continue
            rows.append(
                '<tr><td class="glyph"><i>&#x{codepoint:04X};</i></td>'
                '<td class="name">mdi-{name}</td>'
                '<td class="code">U+{codepoint:04X}</td>'
                '<td class="note">{note}</td></tr>'.format(name=name, codepoint=codepoint, note=note)
            )
        sections.append(
            '<h2>{title}</h2><p class="lead">{explanation}</p>'
            '<table><thead><tr><th>glyph</th><th>name</th><th>code point</th><th>note</th></tr></thead>'
            '<tbody>{rows}</tbody></table>'.format(title=title, explanation=explanation, rows=''.join(rows))
        )

    OUTPUT.write_text(
        """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Material Design Icons - pick the ones for the light panel</title>
<style>
 @font-face {{
    font-family: "Material Design Icons";
    src: url("materialdesignicons-webfont.ttf") format("truetype");
    font-weight: normal;
    font-style: normal;
 }}
 :root {{ color-scheme: dark; }}
 body {{ margin: 0; padding: 28px 36px 64px; background: #0b0f14; color: #d7e3ec;
        font-family: 'DejaVu Sans','Segoe UI',Arial,sans-serif; }}
 h1 {{ font-size: 24px; margin: 0 0 6px; }}
 h2 {{ font-size: 18px; margin: 34px 0 2px; color: #8fc0e6; border-bottom: 1px solid #26313b; padding-bottom: 6px; }}
 p.lead {{ margin: 6px 0 10px; color: #90a4b4; max-width: 900px; line-height: 1.5; font-size: 13px; }}
 table {{ border-collapse: collapse; min-width: 640px; }}
 th {{ text-align: left; font-size: 11px; letter-spacing: .08em; text-transform: uppercase;
       color: #6d8598; padding: 4px 18px 4px 0; border-bottom: 1px solid #26313b; }}
 td {{ padding: 6px 18px 6px 0; border-bottom: 1px solid #16202a; vertical-align: middle; font-size: 13px; }}
 td.glyph {{ width: 64px; }}
 td.glyph i {{ font-family: "Material Design Icons"; font-weight: normal; font-size: 44px; color: #ffffff; line-height: 1; }}
 td.name, td.code {{ font-family: 'DejaVu Sans Mono',Consolas,monospace; font-size: 12.5px; color: #c8b98f; }}
 td.note {{ color: #90a4b4; }}
</style>
</head>
<body>
<h1>Material Design Icons - candidates for the light panel</h1>
<p class="lead">Every row is a glyph of the bundled <code>@mdi/font</code> {version} that is not in
the subset font of the firmware yet (the &quot;current&quot; ones are). Tell me the name of the row to
use for each group and I add it to <code>ICONS</code> in <code>generate_lvgl_fonts.py</code> and
regenerate the fonts.</p>
{sections}
</body>
</html>
""".format(version='7.4.47', sections=''.join(sections)),
        encoding='utf-8',
    )
    print('wrote {} ({} groups)'.format(OUTPUT.name, len(GROUPS)))
    if missing:
        print('not in the stylesheet: {}'.format(', '.join(missing)))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
