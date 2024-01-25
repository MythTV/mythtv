# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-
"""
Scraper for file lyrics
"""

from lib.utils import *

__title__       = "FileLyrics"
__description__ = "Search the same directory as the track for lyrics"
__author__      = "Paul Harrison"
__version__     = "2.0"
__priority__    = "90"
__lrc__         = True

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s - %s" % (__title__, song.artist, song.album, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__

        filename = song.filepath
        filename = os.path.splitext(filename)[0]

        # look for a file ending in .lrc with the same filename as the track minus the extension
        lyricFile = filename + '.lrc'
        log("%s: searching for lyrics file: %s " % (__title__, lyricFile), debug=self.DEBUG)
        if os.path.exists(lyricFile) and os.path.isfile(lyricFile):
            #load the text file
            with open (lyricFile, "r") as f:
                lines = f.readlines()

            for line in lines:
                lyrics.lyrics += line

            return lyrics

        return False
