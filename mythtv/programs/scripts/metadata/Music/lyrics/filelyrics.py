#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
"""
Scraper for file lyrics
"""

import sys, os, re, chardet
import xml.dom.minidom as xml
from optparse import OptionParser
from common import *

__author__      = "Paul Harrison"
__title__       = "FileLyrics"
__description__ = "Search the same directory as the track for lyrics"
__version__     = "0.1"
__priority__    = "105"
__syncronized__ = True

debug = False


class LyricsFetcher:


    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        filename = lyrics.filename.decode("utf-8")
        filename = os.path.splitext(filename)[0]

        # look for a file ending in .lrc with the same filename as the track minus the extension
        lyricFile = filename + '.lrc'
        utilities.log(debug, "%s: searching for lyrics file: %s " % (__title__, lyricFile))
        if os.path.exists(lyricFile) and os.path.isfile(lyricFile):
            #load the text file
            with open (lyricFile, "r") as f:
                lines = f.readlines()

            for line in lines:
                lyrics.lyrics += line

            return True

        return False;

def performSelfTest():
    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Robb Benson'
    lyrics.album = 'Demo Tracks'
    lyrics.title = 'Lone Rock'
    lyrics.filename = os.path.dirname(os.path.abspath(__file__)) + '/examples/filelyrics.mp3'
    fetcher = LyricsFetcher()
    found = fetcher.get_lyrics(lyrics)

    if found:
        try:
            buildLyrics(lyrics)
        except:
            utilities.log(True, "Failed to build lyrics xml file. "
                                "Maybe you don't have lxml installed?")
            sys.exit(1)

        utilities.log(True, "Everything appears in order.")
        sys.exit(0)

    utilities.log(True, "Failed to find the lyrics for the test search!")
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

    utilities.log(True, etree.tostring(xml, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))

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

    utilities.log(True, etree.tostring(version, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
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
