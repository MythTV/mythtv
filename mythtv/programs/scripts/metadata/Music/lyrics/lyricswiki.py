#-*- coding: UTF-8 -*-

import sys, re, socket

try:
    from urllib2 import quote, urlopen, HTTPError
except ImportError:
    from urllib.request import urlopen, HTTPError
    from urllib.parse import quote

try:
    import HTMLParser as html_parser
except ImportError:
    from html import parser as html_parser

from optparse import OptionParser

if sys.version_info < (2, 7):
    import simplejson
else:
    import json as simplejson

from common import *

__author__      = "Paul Harrison and ronie'"
__title__       = "LyricsWiki"
__description__ = "Search http://lyrics.wikia.com for lyrics"
__priority__    = "150"
__version__     = "0.2"
__syncronized__ = False


LIC_TXT = 'we are not licensed to display the full lyrics for this song at the moment'

debug = False

socket.setdefaulttimeout(10)

class LyricsFetcher:
    def __init__( self ):
        self.url = 'http://lyrics.wikia.com/api.php?func=getSong&artist=%s&song=%s&fmt=realjson'

    def get_lyrics(self, lyrics):
        utilities.log(debug,  "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try:
            req = urlopen(self.url % (quote(lyrics.artist), quote(lyrics.title)))
            response = req.read().decode('utf-8')
        except:
            return False
        req.close()
        data = simplejson.loads(response)
        try:
            self.page = data['url']
        except:
            return False
        if not self.page.endswith('action=edit'):
            utilities.log(debug, "%s: search url: %s" % (__title__, self.page))
            try:
                req = urlopen(self.page)
                response = req.read().decode('utf-8')
            except HTTPError as error: # strange... sometimes lyrics are returned with a 404 error
                if error.code == 404:
                    response = error.read().decode('utf-8')
                else:
                    return False
            req.close()
            matchcode = re.search("lyricbox'>.*?(&#.*?)<div", response)
            try:
                lyricscode = (matchcode.group(1))
                htmlparser = html_parser.HTMLParser()
                lyricstext = htmlparser.unescape(lyricscode).replace('<br />', '\n')
                lyr = re.sub('<[^<]+?>', '', lyricstext)
                if LIC_TXT in lyr:
                    return False
                lyrics.lyrics = lyr
                return True
            except:
                return False
        else:
            return False

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
    etree.SubElement(version, "command").text = 'lyricswiki.py'
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
        utilities.log(True,  "No lyrics found for this track")
        sys.exit(1)


if __name__ == '__main__':
    main()
