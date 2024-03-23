# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap
from lib.culrcscrapers.azlyrics.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        'AZLyrics',
    'description': 'Search https://azlyrics.com for lyrics',
    'author':      'ronie',
    'priority':    '230',
    'syncronized': False,
    'artist':      'La Dispute',
    'title':       'Such Small Hands',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)
