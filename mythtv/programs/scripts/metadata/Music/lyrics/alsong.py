# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for http://lyrics.alsong.co.kr/

edge
"""

import sys, os
import socket
import hashlib
import urllib2
import xml.dom.minidom as xml
from optparse import OptionParser
from common import *
from common import audiofile

__author__      = "Paul Harrison and 'edge'"
__title__       = "Alsong"
__description__ = "Search http://lyrics.alsong.co.kr for lyrics"
__version__     = "0.1"
__priority__    = "115"
__syncronized__ = True

debug = False

socket.setdefaulttimeout(10)

ALSONG_URL = "http://lyrics.alsong.net/alsongwebservice/service1.asmx"

ALSONG_TMPL = '''\
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:SOAP-ENC="http://www.w3.org/2003/05/soap-encoding" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:ns2="ALSongWebServer/Service1Soap" xmlns:ns1="ALSongWebServer" xmlns:ns3="ALSongWebServer/Service1Soap12">
<SOAP-ENV:Body>
<ns1:GetLyric5>
    <ns1:stQuery>
        <ns1:strChecksum>%s</ns1:strChecksum>
        <ns1:strVersion>2.2</ns1:strVersion>
        <ns1:strMACAddress />
        <ns1:strIPAddress />
    </ns1:stQuery>
</ns1:GetLyric5>
</SOAP-ENV:Body>
</SOAP-ENV:Envelope>
'''

class alsongClient(object):
    '''
    privide alsong specific function, such as key from mp3
    '''
    @staticmethod
    def GetKeyFromFile(filename):
        musf = audiofile.AudioFile()
        musf.Open(filename)
        ext = os.path.splitext(filename)[1].lower()

        if ext == '.ogg':
            buf = musf.ReadAudioStream(160*1024,11) # 160KB excluding header
        elif ext == '.wma':
            buf = musf.ReadAudioStream(160*1024,24) # 160KB excluding header
        else:
            buf = musf.ReadAudioStream(160*1024) # 160KB from audio data
        musf.Close()

        # calculate hashkey
        m = hashlib.md5(); m.update(buf);

        return m.hexdigest()


class LyricsFetcher:
    def __init__( self ):
        self.base_url = "http://lyrics.alsong.co.kr/"

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        key = None
        try:
            ext = os.path.splitext(lyrics.filename.decode("utf-8"))[1].lower()
            sup_ext = ['.mp3', '.ogg', '.wma', '.flac', '.ape', '.wav']
            if ext in sup_ext:
                key = alsongClient.GetKeyFromFile(lyrics.filename)
            if not key:
                return False
            headers = { 'Content-Type' : 'text/xml; charset=utf-8' }
            request = urllib2.Request(ALSONG_URL, ALSONG_TMPL % key, headers)
            response = urllib2.urlopen(request)
            Page = response.read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                    __title__, self.__class__.__name__,
                    sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                    sys.exc_info()[ 2 ].tb_lineno,
                    sys.exc_info()[ 1 ]
                ))
            return False

        tree = xml.parseString( Page )
        if tree.getElementsByTagName("strInfoID")[0].childNodes[0].data == '-1':
            return False
        lyr = tree.getElementsByTagName("strLyric")[0].childNodes[0].data.replace('<br>','\n')
        lyrics.lyrics = lyr.encode('utf-8')
        return True

def performSelfTest():
    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Dire Straits'
    lyrics.album = 'Brothers In Arms'
    lyrics.title = 'Money For Nothing'
    lyrics.filename = ''
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

    fetcher = LyricsFetcher()
    if fetcher.get_lyrics(lyrics):
        buildLyrics(lyrics)
        sys.exit(0)
    else:
        utilities.log(True, "No lyrics found for this track")
        sys.exit(1)


if __name__ == '__main__':
    main()
