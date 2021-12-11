# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for http://newlyrics.gomtv.com/

edge
"""

import sys
import os
import socket
import hashlib
import urllib
import re
import unicodedata
from optparse import OptionParser
from common import utilities
from common import audiofile

__author__      = "Paul Harrison and edge'"
__title__       = "GomAudio"
__description__ = "Search http://newlyrics.gomtv.com for lyrics"
__priority__    = "135"
__version__     = "0.1"
__syncronized__ = True

debug = False

socket.setdefaulttimeout(10)

GOM_URL = "http://newlyrics.gomtv.com/cgi-bin/lyrics.cgi?cmd=find_get_lyrics&file_key=%s&title=%s&artist=%s&from=gomaudio_local"

def remove_accents(data):
    nfkd_data = unicodedata.normalize('NFKD', data)
    return u"".join([c for c in nfkd_data if not unicodedata.combining(c)])


class gomClient(object):
    '''
    privide Gom specific function, such as key from mp3
    '''
    @staticmethod
    def GetKeyFromFile(file):
        musf = audiofile.AudioFile()
        musf.Open(file)
        buf = musf.ReadAudioStream(100*1024) # 100KB from audio data
        musf.Close()
        # calculate hashkey
        m = hashlib.md5(); m.update(buf);
        return m.hexdigest()

    @staticmethod
    def mSecConv(msec):
        s,ms = divmod(msec/10,100)
        m,s = divmod(s,60)
        return m,s,ms

class LyricsFetcher:
    def __init__( self ):
        self.base_url = "http://newlyrics.gomtv.com/"

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__,  lyrics.artist, lyrics.album, lyrics.title))
        key = None
        try:
            ext = os.path.splitext(lyrics.filename.decode("utf-8"))[1].lower()
            sup_ext = ['.mp3', '.ogg', '.wma', '.flac', '.ape', '.wav']
            if ext in sup_ext:
                key = gomClient.GetKeyFromFile(lyrics.filename)
            if not key:
                return False
            url = GOM_URL %(key, urllib.quote(remove_accents(lyrics.title.decode('utf-8')).encode('euc-kr')), (remove_accents(lyrics.artist.decode('utf-8')).encode('euc-kr')))
            response = urllib.urlopen(url)
            Page = response.read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                    __title__, self.__class__.__name__,
                    sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                    sys.exc_info()[ 2 ].tb_lineno,
                    sys.exc_info()[ 1 ]
                ))
            return False

        if Page[:Page.find('>')+1] != '<lyrics_reply result="0">':
            return False
        syncs = re.compile(r'<sync start="(\d+)">([^<]*)</sync>').findall(Page)
        lyrline = []
        lyrline.append( "[ti:%s]" %lyrics.title )
        lyrline.append( "[ar:%s]" %lyrics.artist )
        for sync in syncs:
            # timeformat conversion
            t = "%02d:%02d.%02d" % gomClient.mSecConv( int(sync[0]) )
            # unescape string
            try:
                s = unicode(sync[1], "euc-kr").encode("utf-8").replace("&apos;","'").replace("&quot;",'"')
                lyrline.append( "[%s]%s" %(t,s) )
            except:
                pass
        lyrics.lyrics = '\n'.join( lyrline )
        return True

def performSelfTest():
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
    etree.SubElement(version, "command").text = 'gomaudio.py'
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
