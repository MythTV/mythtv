# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.music163.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'music163.py'

info['name']        = '*Music163'
info['description'] = 'Search http://music.163.com for synchronized lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '120'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'Madonna'
info['title']       = 'Vogue'
info['album']       = ''

# its reporting author only so I need move it last -twitham, 2024/01
info['priority']    = '500'

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
