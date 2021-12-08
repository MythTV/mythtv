# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for http://www.lyrdb.com/

ronie
"""

import sys
import re
from urllib.parse import quote_plus
from urllib.request import urlopen

import socket
import difflib
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and 'ronie'"
__title__       = "Lyrics.Com"
__description__ = "Search http://www.lyrics.com for lyrics"
__priority__    = "240"
__version__     = "0.1"
__syncronized__ = False

debug = False

socket.setdefaulttimeout(10)

class LyricsFetcher:
    def __init__( self ):
        self.url = 'http://www.lyrics.com/serp.php?st=%s&qtype=2'

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try:
            from bs4 import BeautifulSoup
        except:
            utilities.log(True, "Failed to import BeautifulSoup. This grabber requires python-bs4")
            return False

        try:
            request = urlopen(self.url % quote_plus(lyrics.artist))
            response = request.read()
        except:
            return False

        request.close()
        soup = BeautifulSoup(response, 'html.parser')
        url = ''
        for link in soup.find_all('a'):
            if link.string and link.get('href').startswith('artist/'):
                url = 'http://www.lyrics.com/' + link.get('href')
                break
        if url:
            utilities.log(debug, "%s: Artist url is %s"  % (__title__, url))
            try:
                req = urlopen(url)
                resp = req.read().decode('utf-8')
            except:
                return False
            req.close()
            soup = BeautifulSoup(resp, 'html.parser')
            url = ''
            for link in soup.find_all('a'):
                if link.string and link.get('href').startswith('/lyric/') and (difflib.SequenceMatcher(None, link.string.lower(), lyrics.title.lower()).ratio() > 0.8):
                    url = 'http://www.lyrics.com' + link.get('href')
                    break

            if url:
                utilities.log(debug, "%s: Song url is %s"  % (__title__, url))

                try:
                    req2 = urlopen(url)
                    resp2 = req2.read().decode('utf-8')
                except:
                    return False
                req2.close()

                matchcode = re.search(u'<pre.*?>(.*?)</pre>', resp2, flags=re.DOTALL)
                if matchcode:
                    lyricscode = (matchcode.group(1))
                    lyr = re.sub(u'<[^<]+?>', '', lyricscode)
                    lyrics.lyrics = lyr.replace('\\n','\n')
                    return True

            return False

def performSelfTest():
    try:
        from bs4 import BeautifulSoup
    except:
        utilities.log(True, "Failed to import BeautifulSoup. This grabber requires python-bs4")
        sys.exit(1)

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

    utilities.log(True, utilities.convert_etree(etree.tostring(xml, encoding='UTF-8',
                                                pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'lyricscom.py'
    etree.SubElement(version, "type").text = 'lyrics'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "priority").text = __priority__
    etree.SubElement(version, "syncronized").text = 'True' if __syncronized__ else 'False'

    utilities.log(True, utilities.convert_etree(etree.tostring(version, encoding='UTF-8',
                                                pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def main():
    global debug

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
