from mutagen.flac import FLAC
from mutagen.mp3 import MP3
from mutagen.mp4 import MP4
from mutagen.oggvorbis import OggVorbis
from mutagen.apev2 import APEv2
from lib.utils import *

LANGUAGE = ADDON.getLocalizedString


class BinaryFile(xbmcvfs.File):
    def read(self, numBytes):
        if numBytes == 0:
            return b""
        else:
            return bytes(super().readBytes(numBytes))


def getEmbedLyrics(song, getlrc, lyricssettings):
    lyrics = Lyrics(settings=lyricssettings)
    lyrics.song = song
    lyrics.source = LANGUAGE(32002)
    lyrics.lrc = getlrc
    lry = lyrics.song.embed
    if lry:
        match = isLRC(lry)
        if (getlrc and match) or ((not getlrc) and (not match)):
            if lyrics.song.source:
                lyrics.source = lyrics.song.source
            lyrics.lyrics = lry
            return lyrics
    filename = song.filepath
    ext = os.path.splitext(filename)[1].lower()
    sup_ext = ['.mp3', '.flac', '.ogg', '.ape', '.m4a']
    lry = None
    if ext in sup_ext:
        bfile = BinaryFile(filename)
        if ext == '.mp3':
            lry = getID3Lyrics(bfile, getlrc)
            if not lry:
                try:
                    lry = getLyrics3(bfile, getlrc)
                except:
                    pass
        elif ext == '.flac':
            lry = getFlacLyrics(bfile, getlrc)
        elif ext == '.m4a':
            lry = getMP4Lyrics(bfile, getlrc)
        elif ext == '.ogg':
            lry = getOGGLyrics(bfile, getlrc)
        elif ext == '.ape':
            lry = getAPELyrics(bfile, getlrc)
        bfile.close()
    if not lry:
        return None
    lyrics.lyrics = lry
    return lyrics

'''
Get lyrics embed with Lyrics3/Lyrics3V2 format
See: http://id3.org/Lyrics3
     http://id3.org/Lyrics3v2
'''
def getLyrics3(bfile, getlrc):
    bfile.seek(-128-9, os.SEEK_END)
    buf = bfile.read(9)
    if (buf != b'LYRICS200' and buf != b'LYRICSEND'):
        bfile.seek(-9, os.SEEK_END)
        buf = bfile.read(9)
    if (buf == b'LYRICSEND'):
        ''' Find Lyrics3v1 '''
        bfile.seek(-5100-9-11, os.SEEK_CUR)
        buf = bfile.read(5100+11)
        start = buf.find(b'LYRICSBEGIN')
        data = buf[start+11:]
        enc = chardet.detect(data)
        content = data.decode(enc['encoding'])
        if (getlrc and isLRC(content)) or (not getlrc and not isLRC(content)):
            return content
    elif (buf == b'LYRICS200'):
        ''' Find Lyrics3v2 '''
        bfile.seek(-9-6, os.SEEK_CUR)
        size = int(bfile.read(6))
        bfile.seek(-size-6, os.SEEK_CUR)
        buf = bfile.read(11)
        if(buf == b'LYRICSBEGIN'):
            buf = bfile.read(size-11)
            tags=[]
            while buf!= '':
                tag = buf[:3]
                length = int(buf[3:8])
                data = buf[8:8+length]
                enc = chardet.detect(data)
                content = data.decode(enc['encoding'])
                if (tag == b'LYR'):
                    if (getlrc and isLRC(content)) or (not getlrc and not isLRC(content)):
                        return content
                buf = buf[8+length:]

def ms2timestamp(ms):
    mins = '0%s' % int(ms/1000/60)
    sec = '0%s' % int((ms/1000)%60)
    msec = '0%s' % int((ms%1000)/10)
    timestamp = '[%s:%s.%s]' % (mins[-2:],sec[-2:],msec[-2:])
    return timestamp

'''
Get USLT/SYLT/TXXX lyrics embed with ID3v2 format
See: http://id3.org/id3v2.3.0
'''
def getID3Lyrics(bfile, getlrc):
    try:
        data = MP3(bfile)
        lyr = ''
        for tag,value in data.items():
            if getlrc and tag.startswith('SYLT'):
                for line in data[tag].text:
                    txt = line[0].strip()
                    stamp = ms2timestamp(line[1])
                    lyr += '%s%s\r\n' % (stamp, txt)
            elif not getlrc and tag.startswith('USLT'):
                if data[tag].text:
                    lyr = data[tag].text
            elif tag.startswith('TXXX'):
                if getlrc and tag.upper().endswith('SYNCEDLYRICS'): # TXXX tags contain arbitrary info. only accept 'TXXX:SYNCEDLYRICS'
                    lyr = data[tag].text[0]
                elif not getlrc and tag.upper().endswith('LYRICS'): # TXXX tags contain arbitrary info. only accept 'TXXX:LYRICS'
                    lyr = data[tag].text[0]
            if lyr:
                return lyr
    except:
        return

def getFlacLyrics(bfile, getlrc):
    try:
        tags = FLAC(bfile)
        if 'lyrics' in tags:
            lyr = tags['lyrics'][0]
            match = isLRC(lyr)
            if (getlrc and match) or ((not getlrc) and (not match)):
                return lyr
    except:
        return

def getMP4Lyrics(bfile, getlrc):
    try:
        tags = MP4(bfile)
        if '©lyr' in tags:
            lyr = tags['©lyr'][0]
            match = isLRC(lyr)
            if (getlrc and match) or ((not getlrc) and (not match)):
                return lyr
    except:
        return

def getOGGLyrics(bfile, getlrc):
    try:
        tags = OggVorbis(bfile)
        if 'lyrics' in tags:
            lyr = tags['lyrics'][0]
            match = isLRC(lyr)
            if (getlrc and match) or ((not getlrc) and (not match)):
                return lyr
    except:
        return

def getAPELyrics(bfile, getlrc):
    try:
        tags = APEv2(bfile)
        if 'lyrics' in tags:
            lyr = tags['lyrics'][0]
            match = isLRC(lyr)
            if (getlrc and match) or ((not getlrc) and (not match)):
                return lyr
    except:
        return

def isLRC(lyr):
    match = re.compile('\[(\d+):(\d\d)(\.\d+|)\]').search(lyr)
    if match:
        return True
    else:
        return False
