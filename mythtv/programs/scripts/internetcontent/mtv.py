#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mtv.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform MTV video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.mtv.com/ website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module mtv_api.py which should be included
#   with this script.
#   The mtv.py module uses the full access API published by
#   http://www.mtv.com/ see: http://developer.mtvnservices.com/docs
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Import the specific target site API library.
#   2) Set the title for the scrips and the API optional key for the target video site
#   3) Call the common processing routine
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="MTV";
__author__="R.D. Vaughan"
__version__="0.23"
# 0.1.0 Initial development
# 0.1.1 Added Tree View processing
# 0.1.2 Documentation review
# 0.2.0 Public release
# 0.2.1 Improve error message display when there is an abort condition
# 0.22  Change to support xml version information display
# 0.23  Added the "command" tag to the xml version information display

__usage_examples__ ='''
(Option Help)
> ./mtv.py -h
Usage: ./mtv.py -hduvlST [parameters] <search text>
Version: v0.2.0 Author: R.D.Vaughan

For details on the MythTV Netvision plugin see the wiki page at:
http://www.mythtv.org/wiki/MythNetvision

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
                        is page 1. Page number is ignored with the Tree View
                        option (-T).
  -S, --search          Search for videos
  -T, --treeview        Display a Tree View of a sites videos


(Search MTV for videos matching search words)
> ./mtv.py -S "Sleeping" -p 2
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
        <title>MTV</title>
        <link>http://www.mtv.com</link>
        <description>Visit MTV (Music Television) for TV shows, music videos, celebrity photos, news.</description>
        <numresults>41</numresults>
        <returned>20</returned>
        <startindex>40</startindex>
        <item>
            <title>While You Were Sleeping</title>
            <author>Elvis Perkins</author>
            <pubDate>Tue, 20 Feb 2007 00:00:00 GMT</pubDate>
            <description>Elvis Perkins - While You Were Sleeping - XL Recordings</description>
            <link>http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:170754</link>
            <media:group>
                <media:thumbnail url='http://www.mtv.com/shared/promoimages/bands/p/perkins_elvis/while_you_were_sleeping/281x211.jpg'/>
                <media:content url='http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:170754' duration='229' width='' height='' lang=''/>
            </media:group>
            <rating></rating>
        </item>
...
        <item>
            <title>She is Love</title>
            <author>Parachute</author>
            <pubDate>Mon, 16 Mar 2009 00:00:00 GMT</pubDate>
            <description>Parachute - She is Love - Mercury</description>
            <link>http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:353524</link>
            <media:group>
                <media:thumbnail url='http://www.mtv.com/shared/promoimages/bands/p/parachute/she_is_love/281x211.jpg'/>
                <media:content url='http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:353524' duration='226.76' width='' height='' lang=''/>
            </media:group>
            <rating></rating>
        </item>
    </channel>
</rss>


(Option Tree view)
> ./mtv.py -T
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
        <title>MTV</title>
        <link>http://www.mtv.com</link>
        <description>Visit MTV (Music Television) for TV shows, music videos, celebrity photos, news.</description>
        <numresults>5050</numresults>
        <returned>20</returned>
        <startindex>20</startindex>
            <directory name="New over the last 3 months" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/directories/topics/recent.png">
            <directory name="Rock" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/directories/music_genres/rock.png">
                <item>
                    <title>Useless</title>
                    <author>Tiny Animals</author>
                    <pubDate>Thu, 17 Dec 2009 00:00:00 GMT</pubDate>
                    <description></description>
                    <link>http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:444694</link>
                    <media:group>
                        <media:thumbnail url='http://www.mtv.com/shared/promoimages/bands/t/tiny_animals/useless/281x211.jpg'/>
                        <media:content url='http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:444694' duration='212.88' width='' height='' lang='en'/>
                    </media:group>
                    <rating></rating>
                </item>
...
                <item>
                    <title>Our Velocity</title>
                    <author>Maximo Park</author>
                    <pubDate>Mon, 02 Apr 2007 00:00:00 GMT</pubDate>
                    <description></description>
                    <link>http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:149873</link>
                    <media:group>
                        <media:thumbnail url='http://www.mtv.com/shared/promoimages/bands/m/maximo_park/our_velocity/281x211.jpg'/>
                        <media:content url='http://media.mtvnservices.com/mgid:uma:video:api.mtvnservices.com:149873' duration='218' width='' height='' lang='en'/>
                    </media:group>
                    <rating></rating>
                </item>
            </directory>
            </directory>
    </channel>
</rss>
'''
__search_max_page_items__ = 10
__tree_max_page_items__ = 20

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


# Used for debugging
#import nv_python_libs.mtv.mtv_api as target

# Verify that the tmdb_api modules are installed and accessible
try:
    import nv_python_libs.mtv.mtv_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/mtv" containing the modules mtv_api.py (v0.2.0 or greater),
They should have been included with the distribution of mtv.py.
Error(%s)
''' % e)
    sys.exit(1)

if target.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed mtv_api.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % target.__version__)
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
    main.grabberInfo['command'] = u'mtv.py'
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'mtv.png'
    main.grabberInfo['type'] = ['video']
    main.grabberInfo['desc'] = u"Visit MTV (Music Television) for TV shows, music videos, celebrity photos, and news."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
