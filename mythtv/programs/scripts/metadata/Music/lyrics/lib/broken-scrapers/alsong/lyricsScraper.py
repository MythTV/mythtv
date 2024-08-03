#-*- coding: UTF-8 -*-
'''
Scraper for http://lyrics.alsong.co.kr/
driip
'''

import sys
import socket
import urllib.request
import difflib
import xml.dom.minidom as xml
from utilities import *

__title__ = 'Alsong'
__priority__ = '150'
__lrc__ = True

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
    def __init__(self):
        self.base_url = 'http://lyrics.alsong.co.kr/'

    def get_lyrics(self, song):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title))
        lyrics = Lyrics()
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            headers = {'Content-Type':'text/xml; charset=utf-8'}
            request = urllib.request.Request(ALSONG_URL, bytes(ALSONG_TMPL % (song.title,song.artist), 'utf-8'), headers)
            response = urllib.request.urlopen(request)
            Page = response.read().decode('utf-8')
        except:
            return        
        tree = xml.parseString(Page)

        try:
            name = tree.getElementsByTagName('strArtistName')[0].childNodes[0].data
            track = tree.getElementsByTagName('strTitle')[0].childNodes[0].data
        except:
            return
        if (difflib.SequenceMatcher(None, song.artist.lower(), name.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), track.lower()).ratio() > 0.8):
            lyr = tree.getElementsByTagName('strLyric')[0].childNodes[0].data.replace('<br>','\n')
            lyrics.lyrics = lyr
            return lyrics
