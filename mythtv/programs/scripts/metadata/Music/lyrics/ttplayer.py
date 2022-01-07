# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for http://lrcct2.ttplayer.com/

taxigps
"""

import os
import sys
import socket

from urllib.request import urlopen

import re
import chardet
import random
import difflib
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and 'taxigps'"
__title__       = "TTPlayer"
__description__ = "Search http://lrcct2.ttplayer.com for lyrics"
__priority__    = "110"
__version__     = "0.1"
__syncronized__ = True

debug = False

socket.setdefaulttimeout(10)

LYRIC_TITLE_STRIP=[r"\(live[^\)]*\)", r"\(acoustic[^\)]*\)",
                   r"\([^\)]*mix\)", r"\([^\)]*version\)",
                   r"\([^\)]*edit\)", r"\(feat[^\)]*\)"]
LYRIC_TITLE_REPLACE=[("/", "-"),(" & ", " and ")]
LYRIC_ARTIST_REPLACE=[("/", "-"),(" & ", " and ")]

class ttpClient(object):
    '''
    privide ttplayer specific function, such as encoding artist and title,
    generate a Id code for server authorizition.
    (see http://ttplyrics.googlecode.com/svn/trunk/crack)
    '''
    @staticmethod
    def CodeFunc(Id, data):
        '''
        Generate a Id Code
        These code may be ugly coz it is translated
        from C code which is translated from asm code
        grabed by ollydbg from ttp_lrcs.dll.
        (see http://ttplyrics.googlecode.com/svn/trunk/crack)
        '''
        length = len(data)

        tmp2=0
        tmp3=0

        tmp1 = (Id & 0x0000FF00) >> 8                                                   #??8???x0000015F

            #tmp1 0x0000005F
        if ( (Id & 0x00FF0000) == 0 ):
            tmp3 = 0x000000FF & ~tmp1                                                   #CL 0x000000E7
        else:
            tmp3 = 0x000000FF & ((Id & 0x00FF0000) >> 16)                               #??16??x00000001

        tmp3 = tmp3 | ((0x000000FF & Id) << 8)                                          #tmp3 0x00001801
        tmp3 = tmp3 << 8                                                                #tmp3 0x00180100
        tmp3 = tmp3 | (0x000000FF & tmp1)                                               #tmp3 0x0018015F
        tmp3 = tmp3 << 8                                                                #tmp3 0x18015F00
        if ( (Id & 0xFF000000) == 0 ) :
            tmp3 = tmp3 | (0x000000FF & (~Id))                                          #tmp3 0x18015FE7
        else :
            tmp3 = tmp3 | (0x000000FF & (Id >> 24))                                     #??24???0x00000000

        #tmp3   18015FE7

        i=length-1
        while(i >= 0):
            char = ord(data[i])
            if char >= 0x80:
                char = char - 0x100
            tmp1 = (char + tmp2) & 0x00000000FFFFFFFF
            tmp2 = (tmp2 << (i%2 + 4)) & 0x00000000FFFFFFFF
            tmp2 = (tmp1 + tmp2) & 0x00000000FFFFFFFF
            #tmp2 = (ord(data[i])) + tmp2 + ((tmp2 << (i%2 + 4)) & 0x00000000FFFFFFFF)
            i -= 1

        #tmp2 88203cc2
        i=0
        tmp1=0
        while(i<=length-1):
            char = ord(data[i])
            if char >= 128:
                char = char - 256
            tmp7 = (char + tmp1) & 0x00000000FFFFFFFF
            tmp1 = (tmp1 << (i%2 + 3)) & 0x00000000FFFFFFFF
            tmp1 = (tmp1 + tmp7) & 0x00000000FFFFFFFF
            #tmp1 = (ord(data[i])) + tmp1 + ((tmp1 << (i%2 + 3)) & 0x00000000FFFFFFFF)
            i += 1

        #EBX 5CC0B3BA

        #EDX = EBX | Id
        #EBX = EBX | tmp3
        tmp1 = (((((tmp2 ^ tmp3) & 0x00000000FFFFFFFF) + (tmp1 | Id)) & 0x00000000FFFFFFFF) * (tmp1 | tmp3)) & 0x00000000FFFFFFFF
        tmp1 = (tmp1 * (tmp2 ^ Id)) & 0x00000000FFFFFFFF

        if tmp1 > 0x80000000:
            tmp1 = tmp1 - 0x100000000
        return tmp1

    @staticmethod
    def EncodeArtTit(mystr):
        rtn = ''
        uni = str(mystr)
        mystr = uni.encode('UTF-16')[2:]
        for i in range(len(mystr)):
            rtn += '%02x' % (mystr[i])
        return rtn


class LyricsFetcher:
    def __init__( self ):
        self.LIST_URL = 'http://ttlrccnc.qianqian.com/dll/lyricsvr.dll?sh?Artist=%s&Title=%s&Flags=0'
        self.LYRIC_URL = 'http://ttlrccnc.qianqian.com/dll/lyricsvr.dll?dl?Id=%d&Code=%d&uid=01&mac=%012x'

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        # replace ampersands and the like
        for exp in LYRIC_ARTIST_REPLACE:
                p = re.compile(exp[0])
                artist = p.sub(exp[1], lyrics.artist)
        for exp in LYRIC_TITLE_REPLACE:
                p = re.compile(exp[0])
                title = p.sub(exp[1], lyrics.title)

        # strip things like "(live at Somewhere)", "(accoustic)", etc
        for exp in LYRIC_TITLE_STRIP:
            p = re.compile(exp)
            title = p.sub('', title)

        # compress spaces
        title = title.strip().replace('`','').replace('/','')
        artist = artist.strip().replace('`','').replace('/','')

        try:
            url = self.LIST_URL %(ttpClient.EncodeArtTit(artist.replace(' ','').lower()), ttpClient.EncodeArtTit(title.replace(' ','').lower()))
            f = urlopen(url)
            Page = f.read().decode('utf-8')
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return False

        links_query = re.compile('<lrc id=\"(.*?)\" artist=\"(.*?)\" title=\"(.*?)\"></lrc>')
        urls = re.findall(links_query, Page)
        links = []
        for x in urls:
            if (difflib.SequenceMatcher(None, artist.lower(), x[1].lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, title.lower(), x[2].lower()).ratio() > 0.8):
                links.append( ( x[1] + ' - ' + x[2], x[0], x[1], x[2] ) )
        if len(links) == 0:
            return False
        elif len(links) > 1:
            lyrics.list = links
        for link in links:
            lyr = self.get_lyrics_from_list(link)
            if lyr and lyr.startswith(b'['):
                enc = chardet.detect(lyr)
                lyr = lyr.decode(enc['encoding'], 'ignore')
                lyrics.lyrics = lyr
                return True
        return False

    def get_lyrics_from_list(self, link):
        title,Id,artist,song = link
        utilities.log(debug, '%s %s %s' %(Id, artist, song))
        try:
            url = self.LYRIC_URL %(int(Id),ttpClient.CodeFunc(int(Id), artist + song), random.randint(0,0xFFFFFFFFFFFF))
            f = urlopen(url)
            Page = f.read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return None
        if Page.startswith(b'['):
            return Page
        return ''

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
    etree.SubElement(version, "command").text = 'ttplayer.py'
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
