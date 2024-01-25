# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-
"""
Scraper for broken scrapers - do nothing
"""

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']

    def get_lyrics(self, song):
        return False
