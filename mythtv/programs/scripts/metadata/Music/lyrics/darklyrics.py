# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.darklyrics.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'darklyrics.py'

info['name']        = 'DarkLyrics'
info['description'] = "Search http://www.darklyrics.com/ - the largest metal lyrics archive on the Web"
info['author']      = "Paul Harrison and smory"
info['priority']    = '260'     # 180 in v33-
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'Neurosis'
info['title']       = 'Lost'
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
