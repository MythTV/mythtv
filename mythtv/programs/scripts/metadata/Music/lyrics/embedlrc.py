# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

import os
from common import culrcwrap
from               lib.embedlrc import *
# make sure this-------^^^^^^^^^ matches this file's basename

info = {
    'name':        '*EmbeddedLyrics',
    'description': 'Search track tags for embedded lyrics',
    'author':      'Paul Harrison and ronie',
    'priority':    '50',        # first, before filelyrics
    'syncronized': True,
    'artist':      'Robb Benson',
    'title':       'Lone Rock',
    'album':       'Demo Tracks',
    'filename':    os.path.dirname(os.path.abspath(__file__)) + '/examples/taglyrics.mp3',
}

#  lib/embedlrc.py has no LyricsFetcher, so we create it here:
class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s"
            % (info['name'], song.artist, song.title), debug=self.DEBUG)
        log("%s: searching file %s"
            % (info['name'], song.filepath), debug=self.DEBUG)
        log("%s: searching for SYNCHRONIZED lyrics"
            % info['name'], debug=self.DEBUG)
        lrc = getEmbedLyrics(song, True, culrcwrap.lyricssettings)
        if lrc:
            return lrc
        log("%s: searching for NON-synchronized lyrics"
            % info['name'], debug=self.DEBUG)
        lrc = getEmbedLyrics(song, False, culrcwrap.lyricssettings)
        if lrc:
            return lrc
        return None

if __name__ == '__main__':
    culrcwrap.main(__file__, info, LyricsFetcher)

# most of the code moved to lib/embedlrc.py
