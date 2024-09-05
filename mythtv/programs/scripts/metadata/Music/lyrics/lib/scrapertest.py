#-*- coding: UTF-8 -*-
import time
from lib.utils import *
from lib.culrcscrapers.azlyrics import lyricsScraper as lyricsScraper_azlyrics
from lib.culrcscrapers.darklyrics import lyricsScraper as lyricsScraper_darklyrics
from lib.culrcscrapers.genius import lyricsScraper as lyricsScraper_genius
from lib.culrcscrapers.lrclib import lyricsScraper as lyricsScraper_lrclib
from lib.culrcscrapers.lyricscom import lyricsScraper as lyricsScraper_lyricscom
from lib.culrcscrapers.lyricsmode import lyricsScraper as lyricsScraper_lyricsmode
from lib.culrcscrapers.megalobiz import lyricsScraper as lyricsScraper_megalobiz
from lib.culrcscrapers.music163 import lyricsScraper as lyricsScraper_music163
from lib.culrcscrapers.musixmatch import lyricsScraper as lyricsScraper_musixmatch
from lib.culrcscrapers.musixmatchlrc import lyricsScraper as lyricsScraper_musixmatchlrc
from lib.culrcscrapers.rclyricsband import lyricsScraper as lyricsScraper_rclyricsband
from lib.culrcscrapers.supermusic import lyricsScraper as lyricsScraper_supermusic

FAILED = []

def test_scrapers():
    lyricssettings = {}
    lyricssettings['debug'] = ADDON.getSettingBool('log_enabled')
    lyricssettings['save_filename_format'] = ADDON.getSettingInt('save_filename_format')
    lyricssettings['save_lyrics_path'] = ADDON.getSettingString('save_lyrics_path')
    lyricssettings['save_subfolder'] = ADDON.getSettingBool('save_subfolder')
    lyricssettings['save_subfolder_path'] = ADDON.getSettingString('save_subfolder_path')

    dialog = xbmcgui.DialogProgress()
    TIMINGS = []

    # test alsong
    dialog.create(ADDONNAME, LANGUAGE(32163) % 'azlyrics')
    log('==================== azlyrics ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'La Dispute'
    song.title = 'Such Small Hands'
    st = time.time()
    lyrics = lyricsScraper_azlyrics.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['azlyrics',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('azlyrics')
        log('FAILED: azlyrics', debug=True)
    if dialog.iscanceled():
        return

    # test darklyrics
    dialog.update(8, LANGUAGE(32163) % 'darklyrics')
    log('==================== darklyrics ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Neurosis'
    song.title = 'Lost'
    st = time.time()
    lyrics = lyricsScraper_darklyrics.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['darklyrics',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('darklyrics')
        log('FAILED: darklyrics', debug=True)
    if dialog.iscanceled():
        return

    # test genius
    dialog.update(16, LANGUAGE(32163) % 'genius')
    log('==================== genius ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Maren Morris'
    song.title = 'My Church'
    st = time.time()
    lyrics = lyricsScraper_genius.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['genius',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('genius')
        log('FAILED: genius', debug=True)
    if dialog.iscanceled():
        return

    # test lrclib
    dialog.update(24, LANGUAGE(32163) % 'lrclib')
    log('==================== lrclib ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'CHVRCHES'
    song.title = 'Clearest Blue'
    st = time.time()
    lyrics = lyricsScraper_lrclib.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['lrclib',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('lrclib')
        log('FAILED: lrclib', debug=True)
    if dialog.iscanceled():
        return

    # test lyricscom
    dialog.update(32, LANGUAGE(32163) % 'lyricscom')
    log('==================== lyricscom ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Blur'
    song.title = 'You\'re So Great'
    st = time.time()
    lyrics = lyricsScraper_lyricscom.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['lyricscom',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('lyricscom')
        log('FAILED: lyricscom', debug=True)
    if dialog.iscanceled():
        return

    # test lyricsmode
    dialog.update(40, LANGUAGE(32163) % 'lyricsmode')
    log('==================== lyricsmode ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Maren Morris'
    song.title = 'My Church'
    st = time.time()
    lyrics = lyricsScraper_lyricsmode.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['lyricsmode',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('lyricsmode')
        log('FAILED: lyricsmode', debug=True)
    if dialog.iscanceled():
        return

    # test megalobiz
    dialog.update(49, LANGUAGE(32163) % 'megalobiz')
    log('==================== megalobiz ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Michael Jackson'
    song.title = 'Beat It'
    st = time.time()
    lyrics = lyricsScraper_megalobiz.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['megalobiz',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('megalobiz')
        log('FAILED: megalobiz', debug=True)
    if dialog.iscanceled():
        return

    # test music163
    dialog.update(58, LANGUAGE(32163) % 'music163')
    log('==================== music163 ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Madonna'
    song.title = 'Vogue'
    st = time.time()
    lyrics = lyricsScraper_music163.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['music163',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('music163')
        log('FAILED: music163', debug=True)
    if dialog.iscanceled():
        return

    # test musixmatch
    dialog.update(66, LANGUAGE(32163) % 'musixmatch')
    log('==================== musixmatch ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Kate Bush'
    song.title = 'Wuthering Heights'
    st = time.time()
    lyrics = lyricsScraper_musixmatch.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['musixmatch',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('musixmatch')
        log('FAILED: musixmatch', debug=True)
    if dialog.iscanceled():
        return

    # test musixmatchlrc
    dialog.update(73, LANGUAGE(32163) % 'musixmatchlrc')
    log('==================== musixmatchlrc ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Kate Bush'
    song.title = 'Wuthering Heights'
    st = time.time()
    lyrics = lyricsScraper_musixmatchlrc.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['musixmatchlrc',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('musixmatchlrc')
        log('FAILED: musixmatchlrc', debug=True)
    if dialog.iscanceled():
        return

    # test rclyricsband
    dialog.update(80, LANGUAGE(32163) % 'rclyricsband')
    log('==================== rclyricsband ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Taylor Swift'
    song.title = 'The Archer'
    st = time.time()
    lyrics = lyricsScraper_rclyricsband.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['rclyricsband',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('rclyricsband')
        log('FAILED: rclyricsband', debug=True)
    if dialog.iscanceled():
        return

    # test supermusic
    dialog.update(88, LANGUAGE(32163) % 'supermusic')
    log('==================== supermusic ====================', debug=True)
    song = Song(opt=lyricssettings)
    song.artist = 'Karel Gott'
    song.title = 'Trezor'
    st = time.time()
    lyrics = lyricsScraper_supermusic.LyricsFetcher(settings=lyricssettings, debug=True).get_lyrics(song)
    ft = time.time()
    tt = ft - st
    TIMINGS.append(['supermusic',tt])
    if lyrics:
        log(lyrics.lyrics, debug=True)
    else:
        FAILED.append('supermusic')
        log('FAILED: supermusic', debug=True)
    if dialog.iscanceled():
        return

    dialog.close()
    log('=======================================', debug=True)
    log('FAILED: %s' % str(FAILED), debug=True)
    log('=======================================', debug=True)
    for item in TIMINGS:
        log('%s - %i' % (item[0], item[1]), debug=True)
    log('=======================================', debug=True)
    if FAILED:
        dialog = xbmcgui.Dialog().ok(ADDONNAME, LANGUAGE(32165) % ' / '.join(FAILED))
    else:
        dialog = xbmcgui.Dialog().ok(ADDONNAME, LANGUAGE(32164))
