# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.supermusic.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'supermusic.py'

info['name']        = 'Supermusic'
info['description'] = 'Search https://supermusic.cz for lyrics'
info['author']      = 'Jose Riha'
info['priority']    = '250'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'Karel Gott'
info['title']       = 'Trezor'
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
