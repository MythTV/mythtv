#-*- coding: UTF-8 -*-
"""
Scraper for http://lyrics.alsong.co.kr/
driip
"""

import sys
import socket
import urllib2
import difflib
import xml.dom.minidom as xml
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and 'driip'"
__title__       = "Alsong"
__description__ = "Search http://lyrics.alsong.co.kr"
__version__     = "0.1"
__priority__    = "140"
__syncronized__ = True

debug = False

socket.setdefaulttimeout(10)

ALSONG_URL = 'http://lyrics.alsong.net/alsongwebservice/service1.asmx'

ALSONG_TMPL = '''\
<?xml version='1.0' encoding='UTF-8'?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV='http://www.w3.org/2003/05/soap-envelope' xmlns:SOAP-ENC='http://www.w3.org/2003/05/soap-encoding' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xmlns:xsd='http://www.w3.org/2001/XMLSchema' xmlns:ns2='ALSongWebServer/Service1Soap' xmlns:ns1='ALSongWebServer' xmlns:ns3='ALSongWebServer/Service1Soap12'>
<SOAP-ENV:Body>
    <ns1:GetResembleLyric2>
    <ns1:stQuery>
        <ns1:strTitle>%s</ns1:strTitle>
        <ns1:strArtistName>%s</ns1:strArtistName>
        <ns1:nCurPage>0</ns1:nCurPage>
    </ns1:stQuery>
    </ns1:GetResembleLyric2>
</SOAP-ENV:Body>
</SOAP-ENV:Envelope>
'''

class LyricsFetcher:
    def __init__( self ):
        self.base_url = 'http://lyrics.alsong.co.kr/'

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try:
            headers = {'Content-Type':'text/xml; charset=utf-8'}
            request = urllib2.Request(ALSONG_URL, ALSONG_TMPL % (lyrics.title, lyrics.artist), headers)
            response = urllib2.urlopen(request)
            Page = response.read()
        except:
            return False
        tree = xml.parseString(Page)
        try:
            name = tree.getElementsByTagName('strArtistName')[0].childNodes[0].data
            track = tree.getElementsByTagName('strTitle')[0].childNodes[0].data
        except:
            return False
        if (difflib.SequenceMatcher(None, lyrics.artist.lower(), name.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, lyrics.title.lower(), track.lower()).ratio() > 0.8):
            lyr = tree.getElementsByTagName('strLyric')[0].childNodes[0].data.replace('<br>','\n')
            lyrics.lyrics = lyr.encode('utf-8')
            return True

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

    if (len(args) > 0):
        utilities.log('ERROR: invalid arguments found')
        sys.exit(1)

    fetcher = LyricsFetcher()
    if fetcher.get_lyrics(lyrics):
        buildLyrics(lyrics)
        sys.exit(0)
    else:
        utilities.log(True, "No lyrics found for this track")
        sys.exit(1)

if __name__ == '__main__':
    main()
