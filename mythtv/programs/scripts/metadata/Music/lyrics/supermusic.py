# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.supermusic.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        'Supermusic',
    'description': 'Search https://supermusic.cz for lyrics',
    'author':      'Jose Riha',
    'priority':    '250',
    'syncronized': False,
    'artist':      'Karel Gott',
    'title':       'Trezor',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
