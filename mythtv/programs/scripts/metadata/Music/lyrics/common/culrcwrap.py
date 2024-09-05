# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-
"""
Wrapper for using CU LRC scrapers with MythMusic of MythTV

Paul Harrison, ronie, Timothy Witham
"""

# UPDATE THIS from https://gitlab.com/ronie/script.cu.lrclyrics
# second line of its addon.xml:
__version__ = '6.6.8'

# simulate kodi/xbmc via very simplified Kodistubs.
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + '/../Kodistubs')
from lib.utils import *
from optparse import OptionParser

# album is never searched, but it is given in the xml to mythtv
class Song(Song):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs) # from ../lib/utils.py
        self.album = ''

debug = False

lyricssettings = {}
lyricssettings['debug'] = ADDON.getSettingBool('log_enabled')
lyricssettings['save_filename_format'] = ADDON.getSettingInt('save_filename_format')
lyricssettings['save_lyrics_path'] = ADDON.getSettingString('save_lyrics_path')
lyricssettings['save_subfolder'] = ADDON.getSettingBool('save_subfolder')
lyricssettings['save_subfolder_path'] = ADDON.getSettingString('save_subfolder_path')

def getCacheDir():
    confdir = os.environ.get('MYTHCONFDIR', '')

    if (not confdir) or (confdir == '/'):
        confdir = os.environ.get('HOME', '')

    if (not confdir) or (confdir == '/'):
        log("Unable to find MythTV directory for metadata cache.", debug=True)
        return '/tmp'

    confdir = os.path.join(confdir, '.mythtv')
    cachedir = os.path.join(confdir, 'cache')

    if not os.path.exists(cachedir):
        os.makedirs(cachedir)

    return cachedir

def performSelfTest():
    try:
        from bs4 import BeautifulSoup
    except:
        log("Failed to import BeautifulSoup. This grabber requires python-bs4", debug=True)
        sys.exit(1)

    found = False
    song = Song(opt=lyricssettings)
    song.artist = about.get('artist')
    song.title = about.get('title')
    song.album = about.get('album')
    song.filepath = about.get('filename')

    fetcher = LyricsFetcher(settings=lyricssettings, debug=True)
    lyrics = fetcher.get_lyrics(song)

    if lyrics:
        if debug:
            print(lyrics.lyrics)
        try:
            buildLyrics(song, lyrics)
        except:
            log("Failed to build lyrics xml file. "
                "Maybe you don't have lxml installed?", debug=True)
            sys.exit(1)

        log("Everything appears in order.", debug=True)
        sys.exit(0)

    log("Failed to find the lyrics for the test search!", debug=True)
    sys.exit(1)

def buildLyrics(song, lyrics):
    from lxml import etree
    xml = etree.XML(u'<lyrics></lyrics>')
    etree.SubElement(xml, "artist").text = song.artist
    etree.SubElement(xml, "album").text = song.album
    etree.SubElement(xml, "title").text = song.title
    etree.SubElement(xml, "syncronized").text = 'True' if about['syncronized'] else 'False'
    etree.SubElement(xml, "grabber").text = about['name']

    lines = lyrics.lyrics.splitlines()
    for line in lines:
        etree.SubElement(xml, "lyric").text = line

    print(etree.tostring(xml, encoding='UTF-8',
                         pretty_print=True, xml_declaration=True).decode())

def buildVersion():
    from lxml import etree
    xml = etree.XML(u'<grabber></grabber>')
    etree.SubElement(xml, "name").text = about['name']
    etree.SubElement(xml, "author").text = about['author']
    etree.SubElement(xml, "command").text = about['command']
    etree.SubElement(xml, "type").text = 'lyrics'
    etree.SubElement(xml, "description").text = about['description']
    etree.SubElement(xml, "version").text = about['version']
    etree.SubElement(xml, "priority").text = about['priority']
    etree.SubElement(xml, "syncronized").text = 'True' if about['syncronized'] else 'False'

    print(etree.tostring(xml, encoding='UTF-8',
                         pretty_print=True, xml_declaration=True).decode())
    sys.exit(0)

def main(filename, info, fetcher):
    global debug
    global about
    about = info
    about['command'] = os.path.basename(filename)
    if not about.get('version'):
        about['version'] = __version__
    if not about.get('album'):
        about['album'] = ''
    global LyricsFetcher
    LyricsFetcher = fetcher

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Test grabber with a known good search")
    parser.add_option('-s', "--search", action="store_true", default=False,
                      dest="search", help="Search for lyrics.")
    parser.add_option('-a', "--artist", metavar="ARTIST", default=None,
                      dest="artist", help="Artist of track.")
    parser.add_option('-b', "--album", metavar="ALBUM", default=None,
                      dest="album", help="Album of track.")
    parser.add_option('-n', "--title", metavar="TITLE", default=None,
                      dest="title", help="Title of track.")
    parser.add_option('-f', "--filename", metavar="FILENAME", default=None,
                      dest="filename", help="Filename of track.")
    parser.add_option('-d', '--debug', action="store_true", default=False,
                      dest="debug", help=("Show debug messages"))

    opts, args = parser.parse_args()

    song = Song(opt=lyricssettings)

    if opts.debug:
        debug = True

    if opts.version:
        buildVersion()

    if opts.test:
        performSelfTest()

    if opts.artist:
        song.artist = opts.artist
    if opts.album:
        song.album = opts.album
    if opts.title:
        song.title = opts.title
    if opts.filename:
        song.filepath = opts.filename

    fetcher = LyricsFetcher(settings=lyricssettings, debug=debug)
    lyrics = fetcher.get_lyrics(song)
    if lyrics:
        buildLyrics(song, lyrics)
        sys.exit(0)
    else:
        log("No lyrics found for this track", debug=True)
        sys.exit(1)
