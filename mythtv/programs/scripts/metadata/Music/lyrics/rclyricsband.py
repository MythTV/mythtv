# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.rclyricsband.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*RCLyricsBand',
    'description': 'Search http://www.rclyricsband.com for lyrics',
    'author':      'ronie',
    'priority':    '130',
    'syncronized': True,
    'artist':      'Taylor Swift',
    'title':       'The Archer',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
