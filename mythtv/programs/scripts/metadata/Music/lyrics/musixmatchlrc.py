# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

from common import culrcwrap

#  In this grabber we need to point its PROFILE to mythtv's cache
#  directory, like this:
import lib.culrcscrapers.musixmatchlrc.lyricsScraper
lib.culrcscrapers.musixmatchlrc.lyricsScraper.PROFILE = culrcwrap.getCacheDir()
# make sure this--^^^^^^^^^^^^^ matches this file's basename

info = {
    'name':        '*Musixmatchlrc',
    'description': 'Search https://musixmatch.com for synchronized lyrics',
    'author':      'ronie',
    'priority':    '100',
    'syncronized': True,
    'artist':      'Kate Bush',
    'title':       'Wuthering Heights',
}

if __name__ == '__main__':
    culrcwrap.main(__file__, info, lib.culrcscrapers.musixmatchlrc.lyricsScraper.LyricsFetcher)
