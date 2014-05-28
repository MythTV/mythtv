#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bliptv.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform blip.tv lookups for the MythTV Netvision plugin
#   based on information found on the http://blip.tv/ website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module bliptv_api.py which should be included
#   with this script.
#   The bliptv.py module uses the full access API 2.0 api published by
#   http://blip.tv/ see: http://blip.tv/about/api/
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Import the specific target site API library.
#   2) Set the title for the scripts and the API key (optional) for the target video site
#   3) Call the common processing routine
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="blip.tv";
__author__="R.D. Vaughan"
__version__="0.23"
# 0.1.0 Initial development
# 0.1.1 Add tree view
# 0.1.2 Documentation review
# 0.2.0 Public release
# 0.2.1 Improve the information displayed by trapped aborts
# 0.22  Change to support xml version information display
# 0.23  Added the "command" tag to the xml version information display

__usage_examples__ ='''
> ./bliptv.py -h
Usage: ./bliptv.py -hduvlST [parameters] <search text>

For details on the MythTV Netvision plugin see the wiki page at:
http://www.mythtv.org/wiki/MythNetvision
Version: v0.22 Author: R.D.Vaughan

Options:
  -h, --help            show this help message and exit
  -d, --debug           Show debugging info (URLs, raw XML ... etc, info
                        varies per grabber)
  -u, --usage           Display examples for executing the script
  -v, --version         Display grabber name and supported options
  -l LANGUAGE, --language=LANGUAGE
                        Select data that matches the specified language fall
                        back to English if nothing found (e.g. 'es' Español,
                        'de' Deutsch ... etc). Not all sites or grabbers
                        support this option.
  -p PAGE NUMBER, --pagenumber=PAGE NUMBER
                        Display specific page of the search results. Default
                        is page 1. Page number is ignored with the Tree
                        View option (-T).
  -S, --search          Search for videos
  -T, --treeview        Display a Tree View of a sites videos


(Search for all videos identified with the word "flute")
>./bliptv.py -p 1 -S "flute"
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/">
    <channel>
        <title>blip.tv</title>
        <link>http://blip.tv</link>
        <description>We're the next generation television network</description>
        <numresults>21</numresults>
        <returned>20</returned>
        <startindex>20</startindex>
        <item>
            <title>空中散歩</title>
            <author>flute</author>
            <pubDate>Sun, 25 May 2008 02:39:49 +0000</pubDate>
            <description>test</description>
            <link>http://blip.tv/file/933615</link>
            <media:group>
                <media:thumbnail url='http://a.images.blip.tv/Flute-416-140-781.jpg'/>
                <media:content url='http://blip.tv/file/get/Flute-416.avi' duration='30' width='' height='' lang='en'/>
            </media:group>
            <rating>0.0</rating>
        </item>
...
        <item>
            <title>Bright Sunny Place (music video excerpt)</title>
            <author>tokyosax</author>
            <pubDate>Mon, 24 Sep 2007 11:05:46 +0000</pubDate>
            <description>A brief snippet of my flute work from a larger video composition viewable at http://www.youtube.com/profile?user=japanjazzman. Original tune, original forehead.</description>
            <link>http://blip.tv/file/394159</link>
            <media:group>
                <media:thumbnail url='http://a.images.blip.tv/Arnoldbaruch-BrightSunnyPlace503-931.jpg'/>
                <media:content url='http://blip.tv/file/get/Arnoldbaruch-BrightSunnyPlace310.mov' duration='45' width='' height='' lang='en'/>
            </media:group>
            <rating>0.0</rating>
        </item>
    </channel>
</rss>

(Create a Blip.tv Tree view)
>./bliptv.py -T
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/">
    <channel>
        <title>blip.tv</title>
        <link>http://blip.tv</link>
        <description>We're the next generation television network</description>
        <numresults>0</numresults>
        <returned>1</returned>
        <startindex>0</startindex>
            <directory name="Popular/Recent/Features/Random ..." thumbnail="/usr/local/share/mythtv//mythnetvision/icons/bliptv.png">
            <directory name="Most Comments" thumbnail="/usr/local/share/mythtv//mythnetvision/icons/directories/topics/most_comments.png">
                <item>
                    <title>VIDEO: Eritrea Movie Timali (Part 24)</title>
                    <author>asmarino5</author>
                    <pubDate>Mon, 04 Jan 2010 05:30:21 +0000</pubDate>
                    <description>VIDEO: Eritrea Movie Timali (Part 24)</description>
                    <link>http://blip.tv/file/3043223</link>
                    <media:group>
                        <media:thumbnail url='http://a.images.blip.tv/Asmarino5-VIDEOEritreaMovieTimaliPart24121-227.jpg'/>
                        <media:content url='http://blip.tv/file/get/Asmarino5-VIDEOEritreaMovieTimaliPart24121.wmv' duration='2216' width='320' height='240' lang='en'/>
                    </media:group>
                    <rating>0.0</rating>
                </item>
...
                <item>
                    <title>informest</title>
                    <author>fluido</author>
                    <pubDate>Sun, 03 Jan 2010 22:23:56 +0000</pubDate>
                    <description>informest</description>
                    <link>http://blip.tv/file/3042202</link>
                    <media:group>
                        <media:thumbnail url='http://a.images.blip.tv/Fluido-informest458-244.jpg'/>
                        <media:content url='http://blip.tv/file/get/Fluido-informest458.mp4' duration='310' width='720' height='576' lang='en'/>
                    </media:group>
                    <rating>0.0</rating>
                </item>
            </directory>
            </directory>
    </channel>
</rss>
'''

__max_page_items__ = 20

import sys, os

class OutStreamEncoder(object):
    """Wraps a stream with an encoder"""
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, unicode):
            try:
                self.out.write(obj.encode(self.encoding))
            except IOError:
                pass
        else:
            try:
                self.out.write(obj)
            except IOError:
                pass

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')


# Used for debugging - usually commented out
#import nv_python_libs.bliptv.bliptv_api as target

# Verify that the tmdb_api modules are installed and accessible
try:
    import nv_python_libs.bliptv.bliptv_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/bliptv" containing the modules bliptv_api.py (v0.2.0 or greater),
They should have been included with the distribution of bliptv.py.
Error(%s)
''' % e)
    sys.exit(1)

if target.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed bliptv_api.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % target.__version__)
    sys.exit(1)


# Verify that the common process modules are installed and accessible
try:
    import nv_python_libs.mainProcess as process
except Exception, e:
    sys.stderr.write('''
The python script "nv_python_libs/mainProcess.py" must be present.
Error(%s)
''' % e)
    sys.exit(1)

if process.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed mainProcess.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % process.__version__)
    sys.exit(1)

if __name__ == '__main__':
    # No api key is required
    apikey = ""
    main = process.mainProcess(target, apikey, )
    main.grabberInfo = {}
    main.grabberInfo['title'] = __title__
    main.grabberInfo['command'] = u'bliptv.py'
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'bliptv.png'
    main.grabberInfo['type'] = ['video']
    main.grabberInfo['desc'] = u"We're the next generation television network."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __max_page_items__
    main.grabberInfo['TmaxPage'] = __max_page_items__
    main.main()
