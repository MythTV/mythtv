# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.lrclib.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'lrclib.py'

info['name']        = '*LrcLib'
info['description'] = 'Search https://lrclib.net for synchronized lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '110'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'CHVRCHES'
info['title']       = 'Clearest Blue'
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
