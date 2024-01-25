# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

import os
from common import culrcwrap
from            common.filelyrics import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*FileLyrics',
    'description': 'Search the same directory as the track for lyrics',
    'author':      'Paul Harrison',
    'priority':    '90',        # before all remote web scrapers 100+
    'version':     '2.0',
    'syncronized': True,
    'artist':      'Robb Benson',
    'title':       'Lone Rock',
    'album':       'Demo Tracks',
    'filename':    os.path.dirname(os.path.abspath(__file__)) + '/examples/filelyrics.mp3',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)

# most of the code moved to common/filelyrics.py and common/main.py
