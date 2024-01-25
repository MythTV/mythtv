# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.lyricscom.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        'Lyrics.Com',
    'description': 'Search https://lyrics.com for lyrics',
    'author':      'Paul Harrison and ronie',
    'priority':    '240',
    'syncronized': False,
    'artist':      'Blur',
    'title':       "You're so Great",
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
