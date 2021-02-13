#
# Author: sascha_lammers@gmx.de
#

from os import path
import os
import json
from . import font_data

font_data.verbose = False
font_data.FONT_DATA = {}
font_data.FONT_LIST = []
font_data.INCLUDE_HEADER_DIRS = []


font_data.INCLUDE_HEADER_DIRS = (
    path.abspath(path.join(path.dirname(__file__), '../../../src/plugins/weather_station/fonts/output/**')),
    path.abspath(path.join(path.dirname(__file__), '../../../scripts/tools/fonts/headers/**'))
)

class FontData:

    CACHED_FILES = {}
    FONT_DATA = font_data.FONT_DATA

    def validate(font):
        for font_id, data in font.items():
            if 'bitmap' in data and 'glyph' in data:
                continue
            raise RuntimeError('Invalid font data: %s: %s' % (font_id, data))

    def merge_font(font):
        print('merging fonts: %s' % list(font.keys()))
        FontData.validate(font)
        FontData.FONT_DATA.update(font)

    def replace_font(font):
        print('replacing fonts: %s' % list(font.keys()))
        FontData.validate(font)
        for remove in font.keys():
            del FontData.FONT_DATA[remove]
        FontData.FONT_DATA.update(font)

    def get_cache_filename():
        cache_dir = path.abspath(path.join(path.dirname(__file__), '__pycache__'))
        cache_file = path.join(cache_dir, 'font_data_cache.py')
        return cache_file

    def get_cached_file(filename):
        filename = path.abspath(filename)
        filename = filename.replace('\\', '/')
        stat = os.stat(filename)
        hash_value = 'None'
        revision = 2 # increase to invalidate cache
        modified = '%u,%s,%u,%u,%s' % (revision, filename, stat.st_mtime, stat.st_size, hash_value)
        return [filename, modified]

    def update_cached_files(filename):
        filename, modified = FontData.get_cached_file(filename)
        FontData.CACHED_FILES[filename] = modified
        return [filename, modified]

    def is_cached(filename):
        filename, modified = FontData.get_cached_file(filename)
        return filename in FontData.CACHED_FILES and FontData.CACHED_FILES[filename]==modified

    def is_cacheable(key, val):
        return 'source' in val and 'modified' in val

    def write_cache():
        filename = FontData.get_cache_filename()
        try:
            with open(filename, 'wt') as file:
                # print('writing cache: %s' % file.name)
                file.write('class FontDataCache(object):\n    FONT_DATA = ')
                file.write(json.dumps({key:val for key, val in filter(lambda data: FontData.is_cacheable(data[0], data[1]), FontData.FONT_DATA.items())}, indent=None, separators=(',', ':')))
                file.write('\n\n')
                file.write('    CACHED_FILES = ')
                file.write(json.dumps(FontData.CACHED_FILES, indent=None, separators=(',', ':')))
                file.write('\n\n')
        except Exception as e:
            print('failed to write cache: %s: %s' % (e, filename))


    def read_cache():
        if font_data.clear_font_cache:
            FontData.CACHED_FILES = {}
            return
        try:
            try:
                from fonts.__pycache__.font_data_cache import FontDataCache
            except Exception as e:
                print('Failed to read cache: %s: %s' % (e, FontData.get_cache_filename()))
                return

            from . import Fonts, FontStyle, FontStyles, FontObject

            fonts = Fonts.list()

            for font_id, cached_data in FontDataCache.FONT_DATA.items():
                if not font_id in fonts and 'createFontObject' in cached_data:
                    if font_id in FontData.FONT_DATA:
                        # merge data with cache
                        cached_data.update(FontData.FONT_DATA[font_id])
                    FontData.FONT_DATA[font_id] = cached_data
                    cached_data['createFontObject'][3] = FontStyles([FontStyle(style) for style in cached_data['createFontObject'][3].strip('[()],').split(',') if style])
                    try:
                        cached_data['createFontObject'][7] = eval(cached_data['createFontObject'][7])
                        cached_data['createFontObject'][8] = eval(cached_data['createFontObject'][8])
                    except Exception as e:
                        raise e
                    # print('creating new FontObject for cache: %s' % font_id)
                    fonts[font_id] = FontObject(*cached_data['createFontObject'])

                    FontData.CACHED_FILES[cached_data['source']] = cached_data['modified']

            Fonts.FONTS = tuple(fonts.values())

            FontDataCache.CACHED_FILES = {}
            FontDataCache.FONT_DATA = {}

        except Exception as e:
            raise e

