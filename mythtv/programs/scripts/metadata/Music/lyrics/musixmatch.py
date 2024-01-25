# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.musixmatch.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        'Musixmatch',
    'description': 'Search https://musixmatch.com for lyrics',
    'author':      'ronie',
    'priority':    '210',
    'syncronized': False,
    'artist':      'Kate Bush',
    'title':       'Wuthering Heights',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
