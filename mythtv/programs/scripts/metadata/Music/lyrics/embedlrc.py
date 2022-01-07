#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
"""
Scraper for embedded lyrics
"""

import sys, os, re, chardet
import xml.dom.minidom as xml
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and 'ronin'"
__title__       = "EmbeddedLyrics"
__description__ = "Search tracks tag for embedded lyrics"
__version__     = "0.2"
__priority__    = "100"
__syncronized__ = True

debug = False

def getLyrics3(filename):
    #Get lyrics embed with Lyrics3/Lyrics3V2 format
    #See: http://id3.org/Lyrics3
    #http://id3.org/Lyrics3v2

    utilities.log(debug, "%s: trying %s" % (__title__, "lyrics embed with Lyrics3/Lyrics3V2 format"))

    f = File(filename)
    f.seek(-128-9, os.SEEK_END)
    buf = f.read(9)
    if (buf != "LYRICS200" and buf != "LYRICSEND"):
        f.seek(-9, os.SEEK_END)
        buf = f.read(9)
    if (buf == "LYRICSEND"):
        """ Find Lyrics3v1 """
        f.seek(-5100-9-11, os.SEEK_CUR)
        buf = f.read(5100+11)
        f.close();
        start = buf.find("LYRICSBEGIN")
    elif (buf == "LYRICS200"):
        """ Find Lyrics3v2 """
        f.seek(-9-6, os.SEEK_CUR)
        size = int(f.read(6))
        f.seek(-size-6, os.SEEK_CUR)
        buf = f.read(11)
        if(buf == "LYRICSBEGIN"):
            buf = f.read(size-11)
            tags=[]
            while buf!= '':
                tag = buf[:3]
                length = int(buf[3:8])
                content = buf[8:8+length]
                if (tag == 'LYR'):
                    return content
                buf = buf[8+length:]
    f.close();
    return None

def endOfString(string, utf16=False):
    if (utf16):
        pos = 0
        while True:
            pos += string[pos:].find('\x00\x00') + 1
            if (pos % 2 == 1):
                return pos - 1
    else:
        return string.find('\x00')

def ms2timestamp(ms):
    mins = "0%s" % int(ms/1000/60)
    sec = "0%s" % int((ms/1000)%60)
    msec = "0%s" % int((ms%1000)/10)
    timestamp = "[%s:%s.%s]" % (mins[-2:],sec[-2:],msec[-2:])
    return timestamp

# Uses the high level interface in taglib to find the lyrics
# should work with all the tag formats supported by taglib that can have lyrics
def getLyricsGeneric(filename):
    try:
        import taglib
    except:
        utilities.log(True, "Failed to import taglib. This grabber requires "
                            "pytaglib TagLib bindings for Python. "
                            "https://github.com/supermihi/pytaglib")
        return None

    try:
        utilities.log(debug, "%s: trying to open %s" % (__title__, filename))
        f = taglib.File(filename)

        # see if we can find a lyrics tag
        for tag in f.tags:
            if tag.startswith('LYRICS'):
                return f.tags[tag][0]

        return None
    except:
        return None

# Get USLT/SYLT/TXXX lyrics embed with ID3v2 format
# See: http://id3.org/id3v2.3.0
def getID3Lyrics(filename):
    utilities.log(debug, "%s: trying %s" % (__title__, "lyrics embed with ID3v2 format"))

    # just use the generic taglib method for now
    return getLyricsGeneric(filename)

def getFlacLyrics(filename):
    utilities.log(debug, "%s: trying %s" % (__title__, "lyrics embed with Flac format"))

    # just use the generic taglib method for now
    return getLyricsGeneric(filename)

def getMP4Lyrics(filename):
    utilities.log(debug, "%s: trying %s" % (__title__, "lyrics embed with MP4 format"))

    # just use the generic taglib method for now
    return getLyricsGeneric(filename)

class LyricsFetcher:


    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        filename = lyrics.filename

        ext = os.path.splitext(filename)[1].lower()
        lry = None

        try:
            if ext == '.mp3':
                lry = getLyrics3(filename)
        except:
            pass

        if lry:
            enc = chardet.detect(lry)
            lyrics.lyrics = lry.decode(enc['encoding'])
        else:
            if ext == '.mp3':
                lry = getID3Lyrics(filename)
            elif  ext == '.flac':
                lry = getFlacLyrics(filename)
            elif  ext == '.m4a':
                lry = getMP4Lyrics(filename)
            if not lry:
                return False
            lyrics.lyrics = lry

        return True


def performSelfTest():
    try:
        import taglib
    except:
        utilities.log(True, "Failed to import taglib. This grabber requires "
                            "pytaglib ? TagLib bindings for Python. "
                            "https://github.com/supermihi/pytaglib")
        sys.exit(1)

    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Robb Benson'
    lyrics.album = 'Demo Tracks'
    lyrics.title = 'Lone Rock'
    lyrics.filename = os.path.dirname(os.path.abspath(__file__)) + '/examples/taglyrics.mp3'
    fetcher = LyricsFetcher()
    found = fetcher.get_lyrics(lyrics)

    if found:
        utilities.log(True, "Everything appears in order.")
        buildLyrics(lyrics)
        sys.exit(0)

    utilities.log(True, "The lyrics for the test search failed!")
    sys.exit(1)

def buildLyrics(lyrics):
    from lxml import etree
    xml = etree.XML(u'<lyrics></lyrics>')
    etree.SubElement(xml, "artist").text = lyrics.artist
    etree.SubElement(xml, "album").text = lyrics.album
    etree.SubElement(xml, "title").text = lyrics.title
    etree.SubElement(xml, "syncronized").text = 'True' if __syncronized__ else 'False'
    etree.SubElement(xml, "grabber").text = lyrics.source

    lines = lyrics.lyrics.splitlines()
    for line in lines:
        etree.SubElement(xml, "lyric").text = line

    utilities.log(True,  utilities.convert_etree(etree.tostring(xml, encoding='UTF-8',
                                                 pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'alsong.py'
    etree.SubElement(version, "type").text = 'lyrics'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "priority").text = __priority__
    etree.SubElement(version, "syncronized").text = 'True' if __syncronized__ else 'False'

    utilities.log(True,  utilities.convert_etree(etree.tostring(version, encoding='UTF-8',
                                                 pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def main():
    global debug

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Perform self-test for dependencies.")
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

    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__

    if opts.debug:
        debug = True

    if opts.version:
        buildVersion()

    if opts.test:
        performSelfTest()

    if opts.artist:
        lyrics.artist = opts.artist
    if opts.album:
        lyrics.album = opts.album
    if opts.title:
        lyrics.title = opts.title
    if opts.filename:
        lyrics.filename = opts.filename

    fetcher = LyricsFetcher()
    if fetcher.get_lyrics(lyrics):
        buildLyrics(lyrics)
        sys.exit(0)
    else:
        utilities.log(True, "No lyrics found for this track")
        sys.exit(1)

if __name__ == '__main__':
    main()
