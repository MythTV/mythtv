# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.megalobiz.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*Megalobiz',
    'description': 'Search https://megalobiz.com for synchronized lyrics',
    'author':      'ronie',
    'priority':    '140',
    'syncronized': True,
    'artist':      'Michael Jackson',
    'title':       'Beat It',
}

# it takes 8 seconds, have to move to the end -twitham, 2024/01
info['priority']    = '400'

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
