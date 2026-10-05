"""
Mock of the sensor panel of the Home Assistant screen (ws2_screen_hass.cpp) - a quick preview of
the layout without compiling and flashing the firmware.

The panel is redrawn with PIL from the constants of _buildSensorPanel(), _drawSensorChart(),
_drawSensorTimeline() and _layoutSensorLegend(), with random data. It is an approximation: the text
is rendered from Montserrat-Medium.ttf (the TTF the LVGL fonts are generated from, placed on the
baseline LVGL uses), the icons are placeholders and the antialiasing differs. Use it for
proportions, gaps and overlaps; the device screenshot (scripts/tools/hass_screenshot.py) is the
reference. The constants below have to follow the firmware when it changes.

  python src/plugins/weather_station2/docs/scripts/mock_sensor_panel.py
  python src/plugins/weather_station2/docs/scripts/mock_sensor_panel.py --type timeline --hours 48 --portrait
"""

import argparse
import glob
import math
import os
import random
import time

from PIL import Image, ImageDraw, ImageFont

# ------------------------------------------------------------------------------------------
# constants of the firmware
# ------------------------------------------------------------------------------------------

# lvgl_ui.h
COLOR_BACKGROUND = 0x0b0f14
COLOR_CARD = 0x182029
COLOR_CARD_ALT = 0x161d25
COLOR_BORDER = 0x2b3844
COLOR_TEXT = 0xffffff
COLOR_TEXT_VALUE = 0xc8d4dd
COLOR_TEXT_MUTED = 0x4d6274
COLOR_ACCENT = 0x00ffff
COLOR_ACTIVE = 0xff8000
CARD_RADIUS = 12
ICON_SIZE_SMALL = 20
ICON_SIZE_LARGE = 48

# lvgl_ui.cpp: kFont* -> lv_font_montserrat_<size>, line_height/base_line of lv_font_montserrat_<size>.c
FONT_SMALL = 12
FONT_MEDIUM = 16
FONT_TITLE = 24
FONT_VALUE = 28
FONT_METRICS = {12: (15, 3), 14: (16, 3), 16: (18, 3), 20: (22, 4), 24: (27, 5), 28: (30, 5), 32: (35, 6)}
# kFontLadder: huge, value, title, large, medium, normal, small
FONT_LADDER = (32, 28, 24, 20, 16, 14, 12)

# ws2_screen_hass.cpp
TILE_GAP = 8
TILE_MARGIN = 8
SENSOR_BACK_SIZE = 48
SENSOR_HEADER_GAP = 6
SENSOR_HEADER_Y = TILE_MARGIN
SENSOR_TITLE_Y = SENSOR_HEADER_Y + SENSOR_BACK_SIZE + TILE_GAP
SENSOR_TITLE_HEIGHT = 30
SENSOR_CHIP_WIDTH = 52
SENSOR_CHIP_HEIGHT = 26
SENSOR_CHIP_GAP = 6
SENSOR_CARD_X = TILE_MARGIN
SENSOR_CARD_GAP = 10
SENSOR_CARD_Y = SENSOR_TITLE_Y + SENSOR_TITLE_HEIGHT + SENSOR_CARD_GAP
SENSOR_LEVEL_WIDTH = 40
SENSOR_LEVEL_HEIGHT = 14
SENSOR_TIME_TOP = 2
SENSOR_TIME_LINE_HEIGHT = 15
SENSOR_TIME_GAP = 6
SENSOR_TIME_HEIGHT = SENSOR_TIME_TOP + SENSOR_TIME_LINE_HEIGHT + SENSOR_TIME_GAP
SENSOR_GRAPH_X = SENSOR_LEVEL_WIDTH + 8
SENSOR_GRAPH_GAP = 6
SENSOR_TIMELINE_GAP = 3 * SENSOR_GRAPH_GAP
SENSOR_GRAPH_Y = SENSOR_LEVEL_HEIGHT + 4
SENSOR_GRAPH_INSET = 2
SENSOR_GRID_HOURS = 4
SENSOR_BUCKET_SECONDS = 300
SENSOR_TIMELINE_HEIGHT = 40
TIMELINE_COLOR_ON = COLOR_ACTIVE
TIMELINE_COLOR_OFF = COLOR_BORDER
TIMELINE_COLOR_UNKNOWN = COLOR_BACKGROUND
SENSOR_LEGEND_DOT = 14
SENSOR_LEGEND_DOT_GAP = 6
SENSOR_LEGEND_ITEM_GAP = 24
# gap between the stacked items of a legend that does not fit into one row
SENSOR_LEGEND_ROW_GAP = 6
# ws2_screens.h: StatsRange (HOURS_48, HOURS_24, HOURS_12)
STATS_RANGES = (48, 24, 12)

# TimelineState
NONE, UNKNOWN, OFF, ON = 0, 1, 2, 3


def rgb(value):
    return ((value >> 16) & 0xff, (value >> 8) & 0xff, value & 0xff)


def find_ttf():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, '..', '..', '..', '..', '..'))
    for path in glob.glob(os.path.join(root, '.pio', 'libdeps', '*', 'lvgl', 'scripts', 'built_in_font', 'Montserrat-Medium.ttf')):
        return path
    raise SystemExit('Montserrat-Medium.ttf not found - build an LVGL env once (it is part of the lvgl library in .pio/libdeps)')


class Panel:
    def __init__(self, width, height, ttf):
        self.width = width
        self.height = height
        self.ttf = ttf
        self.fonts = {}
        self.image = Image.new('RGB', (width, height), rgb(COLOR_BACKGROUND))
        self.draw = ImageDraw.Draw(self.image)

    def font(self, size):
        if size not in self.fonts:
            self.fonts[size] = ImageFont.truetype(self.ttf, size)
        return self.fonts[size]

    def text_width(self, text, size):
        return int(math.ceil(self.font(size).getlength(text)))

    # A label at x/y (the top of its box) like LVGLUI::addLabel(): the glyphs sit on the baseline
    # line_height - base_line below the top. `width`/`align` give the box of an aligned label
    def label(self, x, y, text, size, color, width=None, align='left'):
        line_height, base_line = FONT_METRICS[size]
        if width is not None and align != 'left':
            text_width = self.text_width(text, size)
            x = x + (width - text_width) // 2 if align == 'center' else x + width - text_width
        self.draw.text((x, y + line_height - base_line), text, font=self.font(size), fill=rgb(color), anchor='ls')

    def rect(self, x, y, w, h, fill=None, outline=None, radius=0, border=1):
        box = (x, y, x + w - 1, y + h - 1)
        if radius:
            self.draw.rounded_rectangle(box, radius=radius, fill=rgb(fill) if fill is not None else None,
                                        outline=rgb(outline) if outline is not None else None, width=border)
        else:
            self.draw.rectangle(box, fill=rgb(fill) if fill is not None else None,
                                outline=rgb(outline) if outline is not None else None, width=border)


def format_time(value, hours24):
    tm = time.localtime(value)
    if hours24:
        return time.strftime('%H:%M', tm)
    text = time.strftime('%I:%M %p', tm)
    return text[1:] if text.startswith('0') else text


def nice_step(raw):
    if not raw > 0:
        return 1.0
    exponent = math.floor(math.log10(raw))
    base = 10.0 ** exponent
    return max(1.0, round(raw / base)) * base


def random_values(start, hours):
    # a temperature that drifts with the day, one value per 5 minute bucket (a few are missing)
    values = []
    value = 21.0
    for bucket in range(hours * 12 + 1):
        value += random.uniform(-0.08, 0.08) + 0.02 * math.sin((start + bucket * SENSOR_BUCKET_SECONDS) / 86400.0 * 2 * math.pi)
        if random.random() < 0.01:
            continue
        values.append((start + bucket * SENSOR_BUCKET_SECONDS, value))
    return values


def random_changes(start, end):
    # a motion sensor: busy during the day, a few short unknown blips (the sensor reconnecting)
    changes = [(start, 0.0)]
    now = start
    state = 0.0
    while now < end:
        hour = time.localtime(now).tm_hour
        busy = 7 <= hour <= 23
        if state == 1.0:
            now += random.randint(30, 900)
            state = 0.0
        else:
            now += random.randint(120, 1800) if busy else random.randint(3600, 4 * 3600)
            state = 1.0
            if random.random() < 0.05:
                changes.append((now, float('nan')))
                now += 1
        if now < end:
            changes.append((now, state))
    return changes


def fit_text(panel, text, largest, max_width):
    # LVGLUI::fitTextDown(): the next smaller font of the ladder until the text fits, an ellipsis
    # when no font does
    size = largest
    for size in [s for s in FONT_LADDER if s <= largest]:
        if panel.text_width(text, size) <= max_width:
            return text, size
    while text and panel.text_width(text + '...', size) > max_width:
        text = text[:-1]
    return text + '...', size


# The header of the panel. `mode`:
#   stacked  the firmware: the name (kFontMedium) above the value (kFontValue), both left aligned
#            next to the icon and using the whole width - 18 + 30 px fill the 48 px of the back tile
#   side     the previous layout: the value has a fixed box (150 px, a third of a portrait display),
#            the name gets the rest and shrinks; the value overflowed its box in portrait
#   fit      a rejected proposal: the value box is the measured width of the value (at most half of
#            the header), the value shrinks into it, the name gets the rest
def draw_header(panel, mode, name_x, header_center, name, value):
    width = panel.width
    right = width - TILE_MARGIN
    if mode == 'stacked':
        top = header_center - (FONT_METRICS[FONT_MEDIUM][0] + FONT_METRICS[FONT_VALUE][0]) // 2
        text, size = fit_text(panel, name, FONT_MEDIUM, right - name_x)
        panel.label(name_x, top, text, size, COLOR_TEXT_VALUE)
        text, size = fit_text(panel, value, FONT_VALUE, right - name_x)
        panel.label(name_x, top + FONT_METRICS[FONT_MEDIUM][0], text, size, COLOR_TEXT)
        return
    if mode == 'fit':
        value_width = min(panel.text_width(value, FONT_VALUE), (right - name_x) // 2)
        value, value_size = fit_text(panel, value, FONT_VALUE, value_width)
        value_width = panel.text_width(value, value_size)
    else:
        value_width = min(150, width // 3)
        value_size = FONT_VALUE
    name_width = right - value_width - SENSOR_HEADER_GAP - name_x
    text, size = fit_text(panel, name, FONT_TITLE, name_width)
    panel.label(name_x, header_center - FONT_METRICS[FONT_TITLE][0] // 2, text, size, COLOR_TEXT)
    panel.label(right - value_width, header_center - FONT_METRICS[value_size][0] // 2, value, value_size, COLOR_TEXT, value_width,
                'right')


def draw_panel(args, ttf):
    width, height = (320, 480) if args.portrait else (480, 320)
    panel = Panel(width, height, ttf)
    timeline = args.type == 'timeline'

    # back tile and the icon of the entity (placeholders for the glyphs)
    panel.rect(TILE_MARGIN, SENSOR_HEADER_Y, SENSOR_BACK_SIZE, SENSOR_BACK_SIZE, fill=COLOR_CARD, radius=CARD_RADIUS)
    cx = TILE_MARGIN + SENSOR_BACK_SIZE // 2
    cy = SENSOR_HEADER_Y + SENSOR_BACK_SIZE // 2
    panel.draw.line([(cx + 4, cy - 8), (cx - 4, cy), (cx + 4, cy + 8)], fill=rgb(COLOR_TEXT_VALUE), width=3)
    icon_x = TILE_MARGIN + SENSOR_BACK_SIZE + SENSOR_HEADER_GAP
    panel.draw.ellipse((icon_x + 8, SENSOR_HEADER_Y + 8, icon_x + ICON_SIZE_LARGE - 9, SENSOR_HEADER_Y + ICON_SIZE_LARGE - 9),
                       outline=rgb(COLOR_ACTIVE if timeline else COLOR_TEXT_VALUE), width=3)

    # header: name and live value
    name_x = icon_x + ICON_SIZE_LARGE + SENSOR_HEADER_GAP
    header_center = SENSOR_HEADER_Y + SENSOR_BACK_SIZE // 2
    name = 'Motion Living Room' if timeline else 'Living Room'
    value = 'Detected' if timeline else '22.4 °C'
    draw_header(panel, args.header, name_x, header_center, name, value)

    # "History" and the range chips
    panel.label(TILE_MARGIN, SENSOR_TITLE_Y + 2, 'History', FONT_TITLE, COLOR_TEXT)
    chip_x = width - TILE_MARGIN - (len(STATS_RANGES) * SENSOR_CHIP_WIDTH + (len(STATS_RANGES) - 1) * SENSOR_CHIP_GAP)
    chip_y = SENSOR_TITLE_Y + (SENSOR_TITLE_HEIGHT - SENSOR_CHIP_HEIGHT) // 2
    for hours in STATS_RANGES:
        active = hours == args.hours
        panel.rect(chip_x, chip_y, SENSOR_CHIP_WIDTH, SENSOR_CHIP_HEIGHT, fill=COLOR_ACCENT if active else COLOR_CARD_ALT,
                   outline=COLOR_ACCENT if active else COLOR_BORDER, radius=SENSOR_CHIP_HEIGHT // 2)
        panel.label(chip_x, chip_y + (SENSOR_CHIP_HEIGHT - FONT_METRICS[FONT_SMALL][0]) // 2, '%u h' % hours, FONT_SMALL,
                    COLOR_BACKGROUND if active else COLOR_TEXT_MUTED, SENSOR_CHIP_WIDTH, 'center')
        chip_x += SENSOR_CHIP_WIDTH + SENSOR_CHIP_GAP

    # the card and the geometry of the graph inside it (_sensorGraphX()/_sensorGraphWidth())
    card_w = width - 2 * TILE_MARGIN
    card_h = height - TILE_MARGIN - SENSOR_CARD_Y
    panel.rect(SENSOR_CARD_X, SENSOR_CARD_Y, card_w, card_h, fill=COLOR_CARD, outline=COLOR_BORDER, radius=CARD_RADIUS)
    ox, oy = SENSOR_CARD_X + 1, SENSOR_CARD_Y + 1   # children are placed inside the 1 px border
    graph_x = SENSOR_TIMELINE_GAP if timeline else SENSOR_GRAPH_X
    graph_w = card_w - graph_x - (SENSOR_TIMELINE_GAP if timeline else SENSOR_GRAPH_GAP)
    graph_h = card_h - SENSOR_GRAPH_Y - SENSOR_TIME_HEIGHT

    timeline_h = min(SENSOR_TIMELINE_HEIGHT, graph_h - 2 * SENSOR_GRAPH_INSET)
    timeline_y = SENSOR_GRAPH_Y + (graph_h - timeline_h) // 2
    grid_y = SENSOR_GRAPH_Y + SENSOR_GRAPH_INSET
    grid_h = graph_h - 2 * SENSOR_GRAPH_INSET
    if timeline:
        grid_h = grid_y + grid_h - timeline_y
        grid_y = timeline_y

    # the window of the request
    now = int(time.time())
    end = now
    start = (now - args.hours * 3600) // SENSOR_BUCKET_SECONDS * SENSOR_BUCKET_SECONDS
    span = end - start

    # X axis: grid lines every 4 hours at whole local hours, the labels under their lines
    label_step = 8 if args.hours > 24 else SENSOR_GRID_HOURS
    time_y = SENSOR_GRAPH_Y + graph_h + SENSOR_TIME_TOP
    tick = list(time.localtime(start))
    tick[4] = tick[5] = 0
    tick[3] = ((tick[3] + SENSOR_GRID_HOURS - 1) // SENSOR_GRID_HOURS) * SENSOR_GRID_HOURS
    tick[8] = -1
    value_time = int(time.mktime(tuple(tick)))
    while value_time <= end:
        text = format_time(value_time, not args.ampm)
        text_w = panel.text_width(text, FONT_SMALL)
        x = graph_x + graph_w * (value_time - start) // span
        if x - text_w // 2 >= 2 and x + text_w // 2 <= card_w - 2:
            panel.rect(ox + x, oy + grid_y, 1, grid_h, fill=COLOR_BORDER)
            if time.localtime(value_time).tm_hour % label_step == 0:
                panel.label(ox + x - text_w // 2, oy + time_y, text, FONT_SMALL, COLOR_TEXT_MUTED, text_w + 2, 'center')
        tick = list(time.localtime(value_time))
        tick[3] += SENSOR_GRID_HOURS
        tick[8] = -1
        value_time = int(time.mktime(tuple(tick)))

    if timeline:
        draw_timeline(panel, ox, oy, graph_x, graph_w, timeline_y, timeline_h, start, end)
    else:
        draw_graph(panel, ox, oy, graph_x, graph_w, graph_h, start, args.hours)
    return panel.image


def draw_graph(panel, ox, oy, graph_x, graph_w, graph_h, start, hours):
    values = random_values(start, hours)
    low_value = min(v for _, v in values)
    high_value = max(v for _, v in values)
    if high_value - low_value <= 0:
        flat = abs(high_value) * 0.1 if abs(high_value) > 0.01 else 1.0
        low_value -= flat / 2
        high_value += flat / 2
    step = nice_step((high_value - low_value) / 2)
    low = math.floor(low_value / step) * step
    high = low + 2 * step
    if high < high_value:
        high += step * math.ceil((high_value - high) / step)

    # unit and the three levels at the left
    panel.label(ox + 2, oy + 2, '°C', FONT_SMALL, COLOR_TEXT_MUTED, SENSOR_LEVEL_WIDTH - 4, 'right')
    level_top = SENSOR_GRAPH_Y + SENSOR_GRAPH_INSET
    level_h = graph_h - 2 * SENSOR_GRAPH_INSET
    decimals = 0 if step >= 1 else (1 if step >= 0.1 else 2)
    for y, level in ((level_top, high), (level_top + level_h // 2 - SENSOR_LEVEL_HEIGHT // 2, (high + low) / 2),
                     (level_top + level_h - SENSOR_LEVEL_HEIGHT, low)):
        panel.label(ox + 2, oy + y, '%.*f' % (decimals, level), FONT_SMALL, COLOR_TEXT_MUTED, SENSOR_LEVEL_WIDTH - 4, 'right')

    # the three value lines of the chart (lv_chart_set_div_line_count(3, 0)) and the curve
    content_y = SENSOR_GRAPH_Y + SENSOR_GRAPH_INSET
    content_h = graph_h - 2 * SENSOR_GRAPH_INSET
    for i in range(3):
        y = content_y + content_h * i // 2
        panel.rect(ox + graph_x, oy + y, graph_w, 1, fill=COLOR_BORDER)
    buckets = hours * 12 + 1
    points = []
    previous = None
    for when, value in values:
        bucket = (when - start) // SENSOR_BUCKET_SECONDS
        x = ox + graph_x + graph_w * bucket // (buckets - 1)
        y = oy + content_y + content_h - int(round((value - low) * content_h / (high - low)))
        if previous is not None and bucket != previous + 1:
            # a bucket without data is a gap in the curve
            if len(points) > 1:
                panel.draw.line(points, fill=rgb(COLOR_ACCENT), width=2)
            points = []
        points.append((x, y))
        previous = bucket
    if len(points) > 1:
        panel.draw.line(points, fill=rgb(COLOR_ACCENT), width=2)


def draw_timeline(panel, ox, oy, graph_x, graph_w, timeline_y, timeline_h, start, end):
    changes = random_changes(start, end)
    span = end - start
    columns = [NONE] * graph_w
    for i, (when, value) in enumerate(changes):
        begin = max(when, start)
        stop = min(changes[i + 1][0] if i + 1 < len(changes) else end, end)
        if stop < begin:
            continue
        first = graph_w * (begin - start) // span
        last = (graph_w * (stop - start) + span - 1) // span
        last = min(max(last, first + 1), graph_w)
        state = UNKNOWN if math.isnan(value) else (ON if value == 1.0 else OFF)
        for x in range(first, last):
            columns[x] = max(columns[x], state)
    colors = {ON: TIMELINE_COLOR_ON, OFF: TIMELINE_COLOR_OFF, UNKNOWN: TIMELINE_COLOR_UNKNOWN}
    x = 0
    while x < graph_w:
        state = columns[x]
        following = x + 1
        while following < graph_w and columns[following] == state:
            following += 1
        if state != NONE:
            panel.rect(ox + graph_x + x, oy + timeline_y, following - x, timeline_h, fill=colors[state])
        x = following

    # the legend, centered over the strip
    names = ('Detected', 'Clear', 'Unknown')
    dot_colors = (TIMELINE_COLOR_ON, TIMELINE_COLOR_OFF, TIMELINE_COLOR_UNKNOWN)
    widths = [panel.text_width(name, FONT_MEDIUM) for name in names]
    total = 2 * SENSOR_LEGEND_ITEM_GAP + sum(SENSOR_LEGEND_DOT + SENSOR_LEGEND_DOT_GAP + w for w in widths)
    line_height = FONT_METRICS[FONT_MEDIUM][0]
    row_height = max(SENSOR_LEGEND_DOT, line_height)

    def item(x, y, name, color):
        dot_y = y + (row_height - SENSOR_LEGEND_DOT) // 2
        panel.draw.ellipse((ox + x, oy + dot_y, ox + x + SENSOR_LEGEND_DOT - 1, oy + dot_y + SENSOR_LEGEND_DOT - 1), fill=rgb(color),
                           outline=rgb(COLOR_BORDER), width=1)
        panel.label(ox + x + SENSOR_LEGEND_DOT + SENSOR_LEGEND_DOT_GAP, oy + y + (row_height - line_height) // 2, name, FONT_MEDIUM,
                    COLOR_TEXT_MUTED)

    # The legend is centered vertically in the space between the top edge of the card and the
    # strip (the card has no other content there)
    if total <= graph_w:
        # one row, centered over the strip
        row_y = (timeline_y - row_height) // 2
        x = graph_x + (graph_w - total) // 2
        for name, color, text_w in zip(names, dot_colors, widths):
            item(x, row_y, name, color)
            x += SENSOR_LEGEND_DOT + SENSOR_LEGEND_DOT_GAP + text_w + SENSOR_LEGEND_ITEM_GAP
    else:
        # The row does not fit (portrait): the items are stacked, the block is centered over the
        # strip and the bullets form one column
        block_w = SENSOR_LEGEND_DOT + SENSOR_LEGEND_DOT_GAP + max(widths)
        pitch = row_height + SENSOR_LEGEND_ROW_GAP
        block_h = len(names) * row_height + (len(names) - 1) * SENSOR_LEGEND_ROW_GAP
        x = graph_x + (graph_w - block_w) // 2
        y = (timeline_y - block_h) // 2
        for name, color in zip(names, dot_colors):
            item(x, y, name, color)
            y += pitch


def main():
    parser = argparse.ArgumentParser(description='Mock of the sensor panel of the Home Assistant screen')
    parser.add_argument('--type', choices=('graph', 'timeline', 'both'), default='both')
    parser.add_argument('--hours', type=int, choices=STATS_RANGES, default=24)
    parser.add_argument('--portrait', action='store_true', help='320x480 instead of 480x320')
    parser.add_argument('--ampm', action='store_true', help='12 hour time labels')
    parser.add_argument('--header', choices=('stacked', 'side', 'fit'), default='stacked',
                        help='layout of the header: stacked (the firmware), side (the previous side by side layout), fit (a rejected proposal)')
    parser.add_argument('--seed', type=int, default=1, help='seed of the random data')
    parser.add_argument('--scale', type=int, default=2, help='the image is enlarged by this factor (nearest neighbor)')
    parser.add_argument('-o', '--output', default='logs', help='directory of the images')
    args = parser.parse_args()

    ttf = find_ttf()
    os.makedirs(args.output, exist_ok=True)
    for kind in (('graph', 'timeline') if args.type == 'both' else (args.type,)):
        random.seed(args.seed)
        args.type = kind
        image = draw_panel(args, ttf)
        if args.scale > 1:
            image = image.resize((image.width * args.scale, image.height * args.scale), Image.Resampling.NEAREST)
        path = os.path.join(args.output, 'mock_sensor_%s_%uh%s.png' % (kind, args.hours, '_portrait' if args.portrait else ''))
        image.save(path)
        print(path)


if __name__ == '__main__':
    main()
