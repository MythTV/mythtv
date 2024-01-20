import sys
import os
import unicodedata

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
    return unicodedata.normalize('NFKD', str).replace('"', '')

def convert_etree(etostr):
    """lxml.etree.tostring is a bytes object in python3, and a str in python2.
    """
    return(etostr.decode())

def getCacheDir():
    confdir = os.environ.get('MYTHCONFDIR', '')

    if (not confdir) or (confdir == '/'):
        confdir = os.environ.get('HOME', '')

    if (not confdir) or (confdir == '/'):
        print ("Unable to find MythTV directory for metadata cache.")
        return '/tmp'

    confdir = os.path.join(confdir, '.mythtv')
    cachedir = os.path.join(confdir, 'cache')

    if not os.path.exists(cachedir):
        os.makedirs(cachedir)

    return cachedir
