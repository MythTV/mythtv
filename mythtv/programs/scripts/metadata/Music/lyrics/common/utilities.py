import sys
import os
import unicodedata

IS_PY2 = sys.version_info[0] == 2

def log(debug, txt):
    if debug:
        print(txt)

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

def convert_etree(etostr):
    """lxml.etree.tostring is a bytes object in python3, and a str in python2.
    """
    if IS_PY2:
        return(etostr)
    else:
        return(etostr.decode())

