import sys
import os
import unicodedata

def log(debug, txt):
    if debug:
        print txt

class Lyrics:
    def __init__(self):
        self.artist = ""
        self.album = ""
        self.title = ""
        self.filename = ""
        self.lyrics = ""
        self.source = ""
        self.list = None
        self.syncronized = False

def deAccent(str):
    return unicodedata.normalize('NFKD', unicode(str, 'utf-8'))
