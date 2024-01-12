# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
import os
from common import main
from               lib.embedlrc import *
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'embedlrc.py'

info['name']        = '*EmbeddedLyrics'
info['description'] = 'Search track tags for embedded lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '50'      # first, before filelyrics
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'Robb Benson'
info['title']       = 'Lone Rock'
info['album']       = 'Demo Tracks'
info['filename']    = os.path.dirname(os.path.abspath(__file__)) + '/examples/taglyrics.mp3'

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
        lrc = getEmbedLyrics(song, True, main.lyricssettings)
        if lrc:
            return lrc
        log("%s: searching for NON-synchronized lyrics"
            % info['name'], debug=self.DEBUG)
        lrc = getEmbedLyrics(song, False, main.lyricssettings)
        if lrc:
            return lrc
        return None

if __name__ == '__main__':
    main.main(info, LyricsFetcher)

# most of the code moved to lib/embedlrc.py and common/main.py
