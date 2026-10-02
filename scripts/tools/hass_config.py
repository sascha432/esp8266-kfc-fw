#
# Author: sascha_lammers@gmx.de
#

"""
Validate a Home Assistant dashboard configuration (/hass.yaml) and upload it to the device.

The validation mirrors the parser of the firmware
(src/plugins/weather_station2/shared/home_assistant/hass_config.cpp), so a file that passes here is
accepted by the "Home Assistant" screen. The rules are printed with -V, --verbose.

The upload uses the file manager of the WebUI (multipart/form-data, the same request the browser
sends), the session id is generated from the user name and the password like kfcfw_ota.py does.
The default target is /hass.yaml.

examples:
  python hass_config.py
  python hass_config.py --file hass.yaml --host 192.168.0.199 -u admin -p secret
  python hass_config.py --check-ha

Without --file the configuration of the developer is used (not part of the repository),
src/plugins/weather_station2/docs/hass.yaml is the sample of the documentation.

dependencies:
  pip install pyyaml requests
"""
import argparse
import hashlib
import os
import re
import secrets
import sys
from os import path

try:
    import yaml
except ImportError:
    print('pyyaml is missing: pip install pyyaml')
    sys.exit(1)

try:
    import requests
except ImportError:
    print('requests is missing: pip install requests')
    sys.exit(1)

# ------------------------------------------------------------------------------------------------
# limits, they mirror hass_config.h
# ------------------------------------------------------------------------------------------------
MAX_FILE_SIZE = 32768
MAX_TILES = 128               # the whole configuration, all pages together
MAX_PAGES = 128               # the main page plus one page per area
MAX_NESTING = 6               # deepest nesting of areas
MAX_GRID_COLS = 8
MAX_GRID_ROWS = 8
ENTITY_LENGTH = 47            # 48 bytes with the NUL
NAME_LENGTH = 31              # 32 bytes with the NUL
UNIT_LENGTH = 11              # 12 bytes with the NUL
TOKEN_LENGTH = 255            # 256 byte buffer of the parser

DEFAULT_POLL = 5
MIN_POLL = 2
MAX_POLL = 3600
DEFAULT_TIMEOUT = 8
DEFAULT_GRID_COLS = 4
DEFAULT_GRID_ROWS = 2

# refresh interval of a picture tile in seconds, the camera image is requested separately and
# much larger than a state response
DEFAULT_REFRESH = 60
MIN_REFRESH = 1
MAX_REFRESH = 3600

TILE_TYPES = ('switch', 'light', 'sensor', 'button', 'dimmer', 'climate', 'spacer', 'area', 'picture')
# `picture-entity` is the type of the HA card the picture tile is modelled on
TILE_TYPE_ALIASES = {'picture-entity': 'picture'}
# a spacer only reserves cells, an area opens a page with the tiles nested below it
TILE_TYPES_WITHOUT_ENTITY = ('spacer', 'area')
# cells a type occupies (columns x rows). Every tile is one column wide, a dimmer (level fill) and
# a climate tile (+/- bars and the readout) use two rows and a picture tile (camera preview) is
# two cells wide and two rows high by default
TILE_SIZES = {'dimmer': (1, 2), 'climate': (1, 2), 'picture': (2, 2)}
# only these types may declare a size of their own. A 1x1 dimmer keeps the level fill and the
# drag, a 1x1 climate has no +/- bars (the setpoint is stepped in the panel) and a picture tile
# can be 2x2 (default), 2x1 (landscape) or 1x1 - it is the only type that may be two columns wide
TILE_TYPES_WITH_SIZE = ('dimmer', 'climate', 'picture')
MAX_PICTURE_WIDTH = 2
MAX_PICTURE_HEIGHT = 2
TILE_ICONS = ('auto', 'default', 'bulb', 'plug', 'toggle', 'thermometer', 'humidity',
              'button', 'dimmer', 'radiator', 'fan', 'motion', 'area', 'flash', 'co2', 'lock',
              'gauge', 'camera', 'remote', 'lightbulb-off', 'lightbulb-on', 'flash-off', 'home',
              'home-assistant', 'none')

HASS_KEYS = ('url', 'token', 'poll', 'timeout', 'verify')
GRID_KEYS = ('cols', 'rows')
TILE_KEYS = ('type', 'entity', 'name', 'icon', 'unit', 'position', 'size', 'decimals', 'min', 'max', 'step', 'tiles',
             'refresh', 'grid')

SIZE_PATTERN = re.compile(r'^[0-9]+\s*[xX]\s*[0-9]+$')

ENTITY_PATTERN = re.compile(r'^[a-z0-9_]+\.[a-z0-9_]+$')

EXIT_OK = 0
EXIT_INVALID = 1
EXIT_UPLOAD = 2
EXIT_HA = 3


class ConfigError(Exception):
    pass


def check(condition, message):
    if not condition:
        raise ConfigError(message)


# ------------------------------------------------------------------------------------------------
# validation
# ------------------------------------------------------------------------------------------------
class Tile:
    def __init__(self, label, entry, depth=0):
        self.label = label
        check(isinstance(entry, dict), 'tile %s is not a mapping' % label)
        for key in entry:
            check(key in TILE_KEYS, 'tile %s: unknown key "%s"' % (label, key))
        self.type = entry.get('type')
        check(isinstance(self.type, str) and self.type.lower() in TILE_TYPES + tuple(TILE_TYPE_ALIASES),
              'tile %s: type must be one of %s' % (label, ', '.join(TILE_TYPES)))
        self.type = TILE_TYPE_ALIASES.get(self.type.lower(), self.type.lower())
        check(depth < MAX_NESTING, 'tile %s: too many levels of areas, the maximum is %u' % (label, MAX_NESTING - 1))

        # tiles: the nested list of an area tile
        children = entry.get('tiles')
        check(children is None or self.type == 'area', 'tile %s: only an area has tiles' % label)
        check(children is None or (isinstance(children, list) and children),
              'tile %s: an area needs at least one tile' % label)
        if children is not None:
            for child in children:
                check(isinstance(child, dict), 'tile %s: a nested tile is not a mapping' % label)
        self.children = children or []

        self.name = entry.get('name')
        if self.name is not None:
            check(isinstance(self.name, str) and self.name, 'tile %s: name is empty' % label)
            check(len(self.name) <= NAME_LENGTH, 'tile %s: name is longer than %u characters' % (label, NAME_LENGTH))

        self.entity = entry.get('entity')
        if self.type in TILE_TYPES_WITHOUT_ENTITY:
            check(self.entity is None, 'tile %s: a %s has no entity' % (label, self.type))
            self.entity = ''
            self.name = self.name or ''
        else:
            # a picture tile names its camera with `entity` as well (there is no second key for
            # it, the two names would have to be the same anyway)
            check(isinstance(self.entity, str) and self.entity, 'tile %s: entity is missing' % label)
            check(len(self.entity) <= ENTITY_LENGTH, 'tile %s: entity is longer than %u characters' % (label, ENTITY_LENGTH))
            check(ENTITY_PATTERN.match(self.entity) is not None,
                  'tile %s: "%s" is not a valid entity id (domain.object_id, lowercase)' % (label, self.entity))
            if self.type == 'picture':
                check(self.entity.startswith('camera.'),
                      'tile %s: a picture tile needs a camera entity (camera.<name>)' % label)
            if self.name is None:
                self.name = self.entity.split('.', 1)[1].replace('_', ' ')

        # grid of the page of an area (the `grid:` block of an area tile), None for every other tile
        self.grid = None
        if self.type == 'area':
            check(self.name, 'tile %s: an area needs a name' % label)
            self.area_page = 0        # filled in by the Config
            # Grid of the page of the area. The tiles of the page are placed in it instead of the
            # grid of the document, the first cell is the back tile of the page as everywhere else
            self.grid = entry.get('grid')
            if self.grid is not None:
                check(isinstance(self.grid, dict), 'tile %s: the grid block must be a mapping' % label)
                for key in self.grid:
                    check(key in GRID_KEYS, 'tile %s: grid: unknown key "%s"' % (label, key))
                check(children is not None, 'tile %s: an area with a grid needs tiles' % label)
                for name, limit in (('cols', MAX_GRID_COLS), ('rows', MAX_GRID_ROWS)):
                    value = self.grid.get(name)
                    check(value is None or (isinstance(value, int) and not isinstance(value, bool) and 1 <= value <= limit),
                          'tile %s: grid.%s must be an integer between 1 and %u' % (label, name, limit))
                check(self.grid.get('cols') is not None or self.grid.get('rows') is not None,
                      'tile %s: the grid block needs cols and/or rows' % label)

        self.icon = entry.get('icon', 'auto')
        check(isinstance(self.icon, str) and self.icon.lower() in TILE_ICONS,
              'tile %s: icon must be one of %s' % (label, ', '.join(TILE_ICONS)))
        self.icon = self.icon.lower()
        self.unit = entry.get('unit', '')
        check(isinstance(self.unit, str), 'tile %s: unit must be a string' % label)
        check(len(self.unit) <= UNIT_LENGTH, 'tile %s: unit is longer than %u characters' % (label, UNIT_LENGTH))
        self.decimals = entry.get('decimals', 1)
        check(isinstance(self.decimals, int) and not isinstance(self.decimals, bool) and 0 <= self.decimals <= 4,
              'tile %s: decimals must be an integer between 0 and 4' % label)
        self.min = entry.get('min')
        self.max = entry.get('max')
        self.step = entry.get('step')
        for key, value in (('min', self.min), ('max', self.max), ('step', self.step)):
            if value is not None:
                check(isinstance(value, (int, float)) and not isinstance(value, bool),
                      'tile %s: %s must be a number' % (label, key))
        if self.step is not None:
            check(self.step > 0, 'tile %s: step must be greater than 0' % label)
        if self.type in ('spacer', 'area'):
            self.decimals = 0
            self.min = self.max = self.step = None

        self.position = entry.get('position')
        if self.position is not None:
            check(isinstance(self.position, list) and len(self.position) == 2,
                  'tile %s: position must be [col, row]' % label)
            for value in self.position:
                check(isinstance(value, int) and not isinstance(value, bool) and 1 <= value <= MAX_GRID_COLS,
                      'tile %s: position must contain 1 based cell numbers' % label)
            self.position = (self.position[0] - 1, self.position[1] - 1)

        # the size follows from the type unless the tile declares one of its own. Only a dimmer, a
        # climate and a picture tile may do that, the first two are 1x2 by default, a picture tile
        # is 2x2 and the only type that may be two columns wide
        self.size = entry.get('size')
        if self.size is not None:
            check(isinstance(self.size, str) and SIZE_PATTERN.match(self.size) is not None,
                  'tile %s: size must be "columns x rows", for example "1x1"' % label)
            check(self.type in TILE_TYPES_WITH_SIZE,
                  'tile %s: only a dimmer, a climate and a picture tile can have a size of their own' % label)
            width, height = [int(value) for value in re.split(r'[xX]', self.size)]
            if self.type == 'picture':
                check(width <= MAX_PICTURE_WIDTH and height <= MAX_PICTURE_HEIGHT,
                      'tile %s: a picture tile is at most %ux%u cells, for example 2x2, 2x1 or 1x1' % (label, MAX_PICTURE_WIDTH, MAX_PICTURE_HEIGHT))
            else:
                check(width == 1, 'tile %s: every tile is one column wide, size must start with 1x' % label)
                check(1 <= height <= MAX_GRID_ROWS, 'tile %s: size must be 1x1 or 1x2' % label)
            self.width, self.height = width, height
        else:
            self.width, self.height = TILE_SIZES.get(self.type, (1, 1))

        # seconds between two images of a picture tile
        self.refresh = entry.get('refresh', DEFAULT_REFRESH)
        check(isinstance(self.refresh, int) and not isinstance(self.refresh, bool),
              'tile %s: refresh must be an integer' % label)
        check(MIN_REFRESH <= self.refresh <= MAX_REFRESH,
              'tile %s: refresh must be between %u and %u seconds' % (label, MIN_REFRESH, MAX_REFRESH))
        check(entry.get('refresh') is None or self.type == 'picture',
              'tile %s: only a picture tile has a refresh interval' % label)
        self.page = 0
        self.col = None
        self.row = None

    def __str__(self):
        return '%s %ux%u %s "%s"' % (self.type, self.width, self.height, self.entity, self.name)


class Config:
    def __init__(self, data):
        check(isinstance(data, dict), 'the document must be a mapping')
        for key in data:
            check(key in ('hass', 'grid', 'tiles'), 'unknown key "%s"' % key)

        hass = data.get('hass')
        check(isinstance(hass, dict), 'the "hass" block is missing')
        for key in hass:
            check(key in HASS_KEYS, 'hass: unknown key "%s"' % key)
        self.url = hass.get('url')
        check(isinstance(self.url, str) and self.url, 'hass.url is missing')
        check(self.url.startswith('http://') or self.url.startswith('https://'),
              'hass.url must start with http:// or https://')
        self.url = self.url.rstrip('/')
        self.token = hass.get('token')
        check(isinstance(self.token, str) and self.token, 'hass.token is missing')
        self.poll = hass.get('poll', DEFAULT_POLL)
        check(isinstance(self.poll, int) and not isinstance(self.poll, bool) and MIN_POLL <= self.poll <= MAX_POLL,
              'hass.poll must be an integer between %u and %u' % (MIN_POLL, MAX_POLL))
        self.timeout = hass.get('timeout', DEFAULT_TIMEOUT)
        check(isinstance(self.timeout, int) and not isinstance(self.timeout, bool) and 1 <= self.timeout <= 120,
              'hass.timeout must be an integer between 1 and 120')
        self.verify = hass.get('verify', False)
        check(isinstance(self.verify, bool), 'hass.verify must be true or false')

        grid = data.get('grid', {})
        check(isinstance(grid, dict), 'the "grid" block must be a mapping')
        for key in grid:
            check(key in GRID_KEYS, 'grid: unknown key "%s"' % key)
        self.cols = grid.get('cols', DEFAULT_GRID_COLS)
        self.rows = grid.get('rows', DEFAULT_GRID_ROWS)
        for name, value, limit in (('cols', self.cols, MAX_GRID_COLS), ('rows', self.rows, MAX_GRID_ROWS)):
            check(isinstance(value, int) and not isinstance(value, bool) and 1 <= value <= limit,
                  'grid.%s must be an integer between 1 and %u' % (name, limit))

        entries = data.get('tiles')
        check(isinstance(entries, list) and entries, 'no tiles configured')
        # pages[0] is the main page, every area adds one page
        self.pages = [[]]
        self.tiles = []
        self._addTiles(entries, 0, 0)

        self.warnings = []
        self.place()

    # tiles of one list, an area recurses into its children (one level of nesting)
    def _addTiles(self, entries, page, depth, prefix=''):
        for index, entry in enumerate(entries, 1):
            label = ('%s.%u' % (prefix, index)) if prefix else ('%u' % index)
            check(len(self.tiles) < MAX_TILES, 'too many tiles, the maximum is %u' % MAX_TILES)
            tile = Tile(label, entry, depth)
            tile.page = page
            self.tiles.append(tile)
            self.pages[page].append(tile)
            if tile.type == 'area':
                check(len(self.pages) < MAX_PAGES, 'too many areas, the maximum is %u' % (MAX_PAGES - 1))
                tile.area_page = len(self.pages)
                self.pages.append([])
                self._addTiles(tile.children, tile.area_page, depth + 1, label)

    def get_page_name(self, page):
        for tile in self.tiles:
            if tile.type == 'area' and tile.area_page == page:
                return tile.name
        return '?'

    # grid of a page: the grid of the document, or the grid of the area that owns the page. An
    # area that sets only one of the two keeps the value of the document grid
    def get_grid(self, page):
        if page:
            for tile in self.tiles:
                if tile.type == 'area' and tile.area_page == page and tile.grid:
                    return (tile.grid.get('cols', self.cols), tile.grid.get('rows', self.rows))
        return (self.cols, self.rows)

    # first fit placement of hass_config.cpp, one grid per page. The first cell of an area page is
    # used by the back tile that closes it again
    def place(self):
        def is_free(col, row, width, height):
            if col + width > cols or row + height > rows:
                return False
            for r in range(row, row + height):
                for c in range(col, col + width):
                    if used[r][c]:
                        return False
            return True

        def mark(col, row, width, height):
            for r in range(row, row + height):
                for c in range(col, col + width):
                    used[r][c] = True

        for page, tiles in enumerate(self.pages):
            cols, rows = self.get_grid(page)
            used = [[False] * cols for _ in range(rows)]
            if page:
                used[0][0] = True
            for tile in tiles:
                suffix = '' if page == 0 else " of the area '%s'" % self.get_page_name(page)
                if tile.width > cols or tile.height > rows:
                    raise ConfigError('tile %s: a %s tile needs a %ux%u block, the grid is %ux%u'
                                      % (tile.label, tile.type, tile.width, tile.height, cols, rows))
                if tile.position is not None:
                    if page and tile.position == (0, 0):
                        raise ConfigError('tile %s: the first cell of an area page is used by the back tile' % tile.label)
                    if not is_free(tile.position[0], tile.position[1], tile.width, tile.height):
                        raise ConfigError('tile %s: position is outside the grid or overlaps another tile' % tile.label)
                    tile.col, tile.row = tile.position
                    mark(tile.col, tile.row, tile.width, tile.height)
                    continue
                placed = False
                for row in range(0, rows - tile.height + 1):
                    for col in range(0, cols - tile.width + 1):
                        if is_free(col, row, tile.width, tile.height):
                            tile.col, tile.row = col, row
                            mark(col, row, tile.width, tile.height)
                            placed = True
                            break
                    if placed:
                        break
                if not placed:
                    raise ConfigError('tile %s: the %s tile does not fit into the %ux%u grid%s'
                                      % (tile.label, tile.type, cols, rows, suffix))

        # non fatal notes
        entities = {}
        for tile in self.tiles:
            if tile.entity:
                if tile.entity in entities:
                    self.warnings.append('tile %s and %s use the same entity %s' % (entities[tile.entity], tile.label, tile.entity))
                entities[tile.entity] = tile.label
            if tile.type in ('dimmer', 'climate') and (tile.min is not None) != (tile.max is not None):
                self.warnings.append('tile %s: min and max should be set together' % tile.label)
            if tile.type in ('spacer', 'area') and (tile.unit or tile.icon != 'auto' and tile.type == 'spacer'):
                self.warnings.append('tile %s: unit is not used by a %s' % (tile.label, tile.type))
            elif tile.type != 'sensor' and tile.unit:
                self.warnings.append('tile %s: unit is only used by sensor tiles' % tile.label)
            if tile.type in ('switch', 'light') and (tile.min is not None or tile.max is not None or tile.step is not None):
                self.warnings.append('tile %s: min/max/step are only used by dimmer and climate tiles' % tile.label)
            if tile.type == 'picture' and (tile.min is not None or tile.max is not None or tile.step is not None or tile.icon != 'auto'):
                self.warnings.append('tile %s: a picture tile only uses entity, name, size and refresh' % tile.label)
            if tile.type == 'area' and tile.grid:
                self.warnings.append('tile %s: the area uses a %ux%u grid for its page instead of the %ux%u grid of the document'
                                     % (tile.label, self.get_grid(tile.area_page)[0], self.get_grid(tile.area_page)[1], self.cols, self.rows))
        if len(self.token) > TOKEN_LENGTH:
            self.warnings.append('the token is longer than %u characters and gets cut off' % TOKEN_LENGTH)
        if self.url.startswith('http://'):
            self.warnings.append('hass.url uses http, the token is sent unencrypted (fine on a local network)')

    def map(self):
        symbol = '123456789abcdefghijklmnopqrstuvwxyz'
        lines = []
        for page, tiles in enumerate(self.pages):
            cols, rows = self.get_grid(page)
            cells = [[' '] * cols for _ in range(rows)]
            if page:
                cells[0][0] = '<'
            for tile in tiles:
                mark = symbol[self.tiles.index(tile) % len(symbol)]
                for r in range(tile.row, tile.row + tile.height):
                    for c in range(tile.col, tile.col + tile.width):
                        cells[r][c] = mark
            lines.append('  page %u (%s)%s:' % (page, 'main' if page == 0 else self.get_page_name(page),
                                                '' if (cols, rows) == (self.cols, self.rows) else ' %ux%u' % (cols, rows)))
            lines.extend('  ' + ''.join('[%s]' % cell for cell in row) for row in cells)
        return '\n'.join(lines)

    def get_page_parent(self, page):
        for tile in self.tiles:
            if tile.type == 'area' and tile.area_page == page:
                return tile.page
        return 0


def load_config(file_name):
    size = os.path.getsize(file_name)
    if size > MAX_FILE_SIZE:
        raise ConfigError('%s is too large (%u bytes, the maximum is %u)' % (file_name, size, MAX_FILE_SIZE))
    with open(file_name, 'rb') as file:
        raw = file.read()
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeDecodeError as exception:
        raise ConfigError('%s is not UTF-8: %s' % (file_name, exception))
    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as exception:
        raise ConfigError('%s is not valid YAML: %s' % (file_name, exception))
    return Config(data)


# ------------------------------------------------------------------------------------------------
# home assistant check
# ------------------------------------------------------------------------------------------------
def check_home_assistant(config, timeout):
    url = config.url + '/api/config'
    headers = {'Authorization': 'Bearer ' + config.token}
    try:
        response = requests.get(url, headers=headers, timeout=timeout)
    except Exception as exception:
        raise ConfigError('cannot connect to %s: %s' % (config.url, exception))
    if response.status_code == 401:
        raise ConfigError('the access token is not accepted (HTTP 401)')
    if response.status_code != 200:
        raise ConfigError('HTTP %u from %s' % (response.status_code, url))
    try:
        info = response.json()
    except ValueError:
        raise ConfigError('invalid response from %s' % url)

    # the screen reads the values with POST /api/template, check that endpoint as well
    try:
        response = requests.post(config.url + '/api/template', headers=dict(headers, **{'Content-Type': 'application/json'}),
                                 json={'template': '1'}, timeout=timeout)
    except Exception as exception:
        raise ConfigError('cannot connect to %s: %s' % (config.url, exception))
    if response.status_code != 200 or response.text.strip() != '1':
        raise ConfigError('the template API does not work: HTTP %u, %s' % (response.status_code, response.text.strip()[:120]))
    return info


# ------------------------------------------------------------------------------------------------
# upload
# ------------------------------------------------------------------------------------------------
def upload_file(host, port, secure, sid, source, target, overwrite, timeout, verbose):
    url = ('https' if secure else 'http') + '://' + host
    if port and port != 80 and port != 443:
        url += ':%u' % port
    url += '/'

    directory = path.dirname(target)
    if not directory:
        directory = '/'
    files = {'upload_file': (path.basename(source), open(source, 'rb'), 'application/octet-stream')}
    data = {
        'upload_filename': path.basename(target),
        'upload_current_dir': directory,
        'ajax_upload': '1',
        'overwrite_target': '1' if overwrite else '0',
        'SID': sid,
    }
    verbose('POST %sfile_manager/upload (%s -> %s, overwrite=%u)' % (url, source, target, overwrite))
    try:
        try:
            response = requests.post(url + 'file_manager/upload', params={'SID': sid}, data=data, files=files,
                                     timeout=timeout, allow_redirects=False)
        except requests.exceptions.RequestException as exception:
            # the firmware answers with a malformed status line for uploads ("HTTP/1.1 1 ...")
            if 'HTTP/1.1 1 ' in str(exception) or 'BadStatusLine' in str(exception):
                return True, 'Upload successful (firmware status line workaround)'
            raise
    finally:
        files['upload_file'][1].close()

    content = response.content.decode('utf-8', errors='replace').strip()
    verbose('HTTP %u: %s' % (response.status_code, content[:200]))
    if response.status_code in (401, 403):
        return False, ('access denied (HTTP %u), the WebUI user name or password is wrong - '
                       'pass -u/-p or --sid' % response.status_code)
    if content.startswith('ERROR:'):
        return False, content[6:]
    if response.status_code in (1, 200) or 200 <= response.status_code < 300:
        return True, content if content else 'Upload successful'
    return False, 'HTTP %u: %s' % (response.status_code, content[:200])


def generate_session_id(username, password, sha1=False, rounds=1024):
    """session id of the WebUI, mirrors scripts/libs/kfcfw/session.py and session.cpp"""
    algorithm = hashlib.sha1 if sha1 else hashlib.sha256
    salt = secrets.token_bytes(8)
    digest = algorithm(salt + password.encode('utf8') + username.encode('utf8')).digest()
    for _ in range(rounds):
        digest = algorithm(digest + salt).digest()
    return salt.hex() + digest.hex()


def make_session(args):
    if args.sid:
        return args.sid
    if not args.user or args.pw is None:
        raise ConfigError('--user and --pw are required for the upload, or pass --sid')
    return generate_session_id(args.user, args.pw, args.sha1)


# ------------------------------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description='Validate and upload the Home Assistant dashboard configuration',
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog='exit codes:\n  0 success\n  1 invalid configuration\n  2 upload failed\n  3 home assistant check failed')
    parser.add_argument('--file', '-f', default=path.join(path.dirname(path.realpath(__file__)), '..', '..',
                                                          'include', 'retracted', 'custom_config', 'hass.yaml'),
                        help='configuration file to validate and upload')
    parser.add_argument('--host', help='device hostname or IP, without it the file is only validated')
    parser.add_argument('-P', '--port', type=int, default=80, help='web server port of the device (default 80)')
    parser.add_argument('-u', '--user', help='web server user name')
    parser.add_argument('-p', '--pw', '--pass', dest='pw', help='web server password')
    parser.add_argument('--sid', help='session id, skips the login (see the WebUI cookie)')
    parser.add_argument('--sha1', action='store_true', help='use sha1 authentication')
    parser.add_argument('--secure', '-s', action='store_true', help='use https to upload')
    parser.add_argument('--target', '-t', default='/hass.yaml', help='target file on the device (default /hass.yaml)')
    parser.add_argument('--no-overwrite', action='store_true', help='do not overwrite an existing file')
    parser.add_argument('--validate-only', action='store_true', help='only validate, do not upload')
    parser.add_argument('--check-ha', action='store_true', help='verify the url and the token with the Home Assistant API')
    parser.add_argument('--timeout', type=int, default=30, help='timeout of the requests in seconds (default 30)')
    parser.add_argument('--quiet', '-q', action='store_true', help='only print errors')
    parser.add_argument('--verbose', '-V', action='store_true', help='print the details of every request')
    args = parser.parse_args()

    def verbose(message):
        if not args.quiet:
            print(message)

    def debug(message):
        if args.verbose:
            print(message)

    file_name = path.realpath(args.file)
    if not path.isfile(file_name):
        print('File not found: %s' % file_name)
        return EXIT_INVALID

    try:
        config = load_config(file_name)
    except ConfigError as exception:
        print('Invalid configuration: %s' % exception)
        return EXIT_INVALID

    verbose('%s is valid' % file_name)
    verbose('Home Assistant: %s, %u tile(s) on %u page(s), poll %us, timeout %us%s' %
            (config.url, len(config.tiles), len(config.pages), config.poll, config.timeout,
             ', TLS without verification' if config.url.startswith('https') else ''))
    verbose('Grid %ux%u, %u cell(s) used' % (config.cols, config.rows, sum(tile.width * tile.height for tile in config.tiles)))
    for tile in config.tiles:
        parent = config.get_page_parent(tile.page)
        # the number in front is the index of the tile in the firmware, 0 based in the order the
        # tiles are placed (the debug keys of the screenshot feature use it, "set=hasspage:<page>"
        # and "set=hassfull:<index>")
        verbose('  %3u  %6s  page %u%s  (%u,%u)  %s' % (config.tiles.index(tile), tile.label, tile.page,
                                                         '' if tile.page == 0 else ' (back to %u)' % parent,
                                                         tile.col + 1, tile.row + 1, tile))
    for line in config.map().splitlines():
        verbose(line)
    for warning in config.warnings:
        verbose('warning: %s' % warning)

    if args.check_ha:
        try:
            info = check_home_assistant(config, args.timeout)
        except ConfigError as exception:
            print('Home Assistant check failed: %s' % exception)
            return EXIT_HA
        verbose('Home Assistant is reachable: version %s, location "%s"' %
                (info.get('version', '?'), info.get('location_name', '?')))

    if args.validate_only or args.host is None:
        verbose('Not uploaded (no --host given)' if not args.validate_only else 'Validation only')
        return EXIT_OK

    if not args.sid and (not args.user or args.pw is None):
        print('--user and --pw are required for the upload, or pass --sid')
        return EXIT_UPLOAD

    try:
        sid = make_session(args)
        success, message = upload_file(args.host, args.port, args.secure, sid, file_name, args.target,
                                       not args.no_overwrite, args.timeout, debug)
    except Exception as exception:
        print('Upload failed: %s' % exception)
        return EXIT_UPLOAD

    if not success:
        print('Upload failed: %s' % message)
        return EXIT_UPLOAD

    verbose('%s -> %s:%s (%s)' % (file_name, args.host, args.target, message))
    verbose('The screen reads the file within a few seconds, no reboot is required')
    return EXIT_OK


if __name__ == '__main__':
    sys.exit(main())
