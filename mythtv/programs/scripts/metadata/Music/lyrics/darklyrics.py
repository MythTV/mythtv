# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.darklyrics.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        'DarkLyrics',
    'description': 'Search http://www.darklyrics.com/ - the largest metal lyrics archive on the Web',
    'author':      'Paul Harrison and smory',
    'priority':    '260',
    'syncronized': False,
    'artist':      'Neurosis',
    'title':       'Lost',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
