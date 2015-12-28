# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for http://www.lyrdb.com/

taxigps
"""

import os
import sys
import urllib
import socket
import re
import difflib
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and taxigps'"
__title__       = "Lyrdb"
__description__ = "Search http://www.lyrdb.com for lyrics"
__priority__    = "150"
__version__     = "0.12"
__syncronized__ = True

debug = False

socket.setdefaulttimeout(30)

class LyricsFetcher:
    def __init__( self ):
        self.base_url = "http://www.lyrdb.com/karaoke/"

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try:
            url = 'http://www.lyrdb.com/karaoke/?q=%s+%s&action=search' %(lyrics.artist.replace(' ','+').lower(), lyrics.title.replace(' ','+').lower())
            f = urllib.urlopen(url)
            Page = f.read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return False

        links_query = re.compile('<tr><td class="tresults"><a href="/karaoke/([0-9]+).htm">(.*?)</td><td class="tresults">(.*?)</td>')
        urls = re.findall(links_query, Page)
        links = []
        for x in urls:
            if (difflib.SequenceMatcher(None, song.artist.lower(), x[2].lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), x[1].lower()).ratio() > 0.8):
                links.append( ( x[2] + ' - ' + x[1], x[0], x[2], x[1] ) )
        if len(links) == 0:
            return False
        elif len(links) > 1:
            lyrics.list = links
        lyr = self.get_lyrics_from_list(links[0])
        if not lyr:
            return False
        lyrics.lyrics = lyr
        return True

    def get_lyrics_from_list(self, link):
        title, Id, artist, song = link
        utilities.log(debug, '%s %s %s' %(Id, artist, song))
        try:
            url = 'http://www.lyrdb.com/karaoke/downloadlrc.php?q=%s' %(Id)
            f = urllib.urlopen(url)
            Page = f.read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return None
        return Page

def performSelfTest():
    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Dire Straits'
    lyrics.album = 'Brothers In Arms'
    lyrics.title = 'Money For Nothing'

    fetcher = LyricsFetcher()
    found = fetcher.get_lyrics(lyrics)

    if found:
        utilities.log(True, "Everything appears in order.")
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

    utilities.log(True, etree.tostring(xml, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'lyrdb.py'
    etree.SubElement(version, "type").text = 'lyrics'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "priority").text = __priority__
    etree.SubElement(version, "syncronized").text = 'True' if __syncronized__ else 'False'

    utilities.log(True, etree.tostring(version, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit(0)

def main():
    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Test grabber with a know good search")
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
