# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.lyricsify.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*Lyricsify',
    'description': 'Search https://lyricsify.com for synchronized lyrics',
    'author':      'ronie',
    'priority':    '130',
    'syncronized': True,
    'artist':      'Madonna',
    'title':       'Crazy For You',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
