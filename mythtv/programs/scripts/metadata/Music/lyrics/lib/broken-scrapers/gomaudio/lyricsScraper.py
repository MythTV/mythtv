#-*- coding: UTF-8 -*-
'''
Scraper for http://newlyrics.gomtv.com/

edge
'''

import sys
import hashlib
import requests
import urllib.parse
import re
import unicodedata
from lib.utils import *
from lib.audiofile import AudioFile

__title__ = 'GomAudio'
__priority__ = '110'
__lrc__ = True


GOM_URL = 'http://newlyrics.gomtv.com/cgi-bin/lyrics.cgi?cmd=find_get_lyrics&file_key=%s&title=%s&artist=%s&from=gomaudio_local'

def remove_accents(data):
    nfkd_data = unicodedata.normalize('NFKD', data)
    return u"".join([c for c in nfkd_data if not unicodedata.combining(c)])


class gomClient(object):
    '''
    privide Gom specific function, such as key from mp3
    '''
    @staticmethod
    def GetKeyFromFile(file):
        musf = AudioFile()
        musf.Open(file)
        buf = musf.ReadAudioStream(100*1024)	# 100KB from audio data
        musf.Close()
        # buffer will be empty for streaming audio
        if not buf:
            return
        # calculate hashkey
        m = hashlib.md5()
        m.update(buf)
        return m.hexdigest()

    @staticmethod
    def mSecConv(msec):
        s,ms = divmod(msec/10,100)
        m,s = divmod(s,60)
        return m,s,ms

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.base_url = 'http://newlyrics.gomtv.com/'

    def get_lyrics(self, song, key=None, ext=None):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            if not ext:
               ext = os.path.splitext(song.filepath)[1].lower()
            sup_ext = ['.mp3', '.ogg', '.wma', '.flac', '.ape', '.wav']
            if ext in sup_ext and key == None:
                key = gomClient.GetKeyFromFile(song.filepath)
            if not key:
                return None
            url = GOM_URL %(key, urllib.parse.quote(remove_accents(song.title).encode('euc-kr')), urllib.parse.quote(remove_accents(song.artist).encode('euc-kr')))
            response = requests.get(url, timeout=10)
            response.encoding = 'euc-kr'
            Page = response.text
        except:
            log('%s: %s::%s (%d) [%s]' % (
                    __title__, self.__class__.__name__,
                    sys.exc_info()[2].tb_frame.f_code.co_name,
                    sys.exc_info()[2].tb_lineno,
                    sys.exc_info()[1]
               ), debug=self.DEBUG)
            return None
        if Page[:Page.find('>')+1] != '<lyrics_reply result="0">':
            return None
        syncs = re.compile('<sync start="(\d+)">([^<]*)</sync>').findall(Page)
        lyrline = []
        lyrline.append('[ti:%s]' %song.title)
        lyrline.append('[ar:%s]' %song.artist)
        for sync in syncs:
            # timeformat conversion
            t = '%02d:%02d.%02d' % gomClient.mSecConv(int(sync[0]))
            # unescape string
            try:
                s = sync[1].replace('&apos;',"'").replace('&quot;','"')
                lyrline.append('[%s]%s' %(t,s))
            except:
                pass
        lyrics.lyrics = '\n'.join(lyrline)
        return lyrics
