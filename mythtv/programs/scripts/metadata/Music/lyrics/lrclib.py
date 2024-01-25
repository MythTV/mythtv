# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.lrclib.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*LrcLib',
    'description': 'Search https://lrclib.net for synchronized lyrics',
    'author':      'ronie',
    'priority':    '110',
    'syncronized': True,
    'artist':      'CHVRCHES',
    'title':       'Clearest Blue',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
