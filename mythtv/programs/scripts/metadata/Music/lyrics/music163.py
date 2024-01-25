# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.music163.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*Music163',
    'description': 'Search http://music.163.com for synchronized lyrics',
    'author':      'ronie',
    'priority':    '120',
    'syncronized': True,
    'artist':      'Madonna',
    'title':       'Vogue',
}

#  -a Rainmakers -n Doomsville, for example, reports author only which
#  is incomplete and stops the search so I need to move it last.
#  Simply comment this if you prefer the above 120 from CU LRC
#  -twitham, 2024/01
info['priority']    = '500'

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
