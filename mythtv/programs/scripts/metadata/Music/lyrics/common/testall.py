#-*- coding: utf-8 -*-

# simulate kodi/xbmc via very simplified Kodistubs.
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + '/../Kodistubs')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + '/..')

from lib.scrapertest import *

def main():
    test_scrapers();

if __name__ == '__main__':
    main()
