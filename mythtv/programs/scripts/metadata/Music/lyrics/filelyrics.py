# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
import os
from common import main
from            common.filelyrics import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'filelyrics.py'

info['name']        = '*FileLyrics'
info['description'] = 'Search the same directory as the track for lyrics'
info['author']      = "Paul Harrison"
info['priority']    = '90'	# before all remote web scrapers 100+
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'Robb Benson'
info['title']       = 'Lone Rock'
info['album']       = 'Demo Tracks'
info['filename']    = os.path.dirname(os.path.abspath(__file__)) + '/examples/filelyrics.mp3'

if __name__ == '__main__':
    main.main(info, LyricsFetcher)

# most of the code moved to common/filelyrics.py and common/main.py
