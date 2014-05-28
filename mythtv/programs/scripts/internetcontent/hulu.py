#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: hulu.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Hulu video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.hulu.com/ website. It
#   follows the MythTV Netvision grabber standards.
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Read the ".../emml/feConfig.xml"
#   2) Check if the CGI Web server should be used or if the script is run locally
#   3) Initialize the correct target functions for processing (local or remote)
#   4) Process the search or treeview request and display to stdout
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="Hulu";
__mashup_title__ = "hulu"
__author__="R.D. Vaughan"
__version__="0.13"
# 0.1.0 Initial development
# 0.11  Change to support xml version information display
# 0.12  Added the "command" tag to the xml version information display
# 0.13  Converted to new common_api.py library

__usage_examples__ ='''
(Option Help)
> ./hulu.py -h
Usage: ./hulu.py -hduvlST [parameters] <search text>
Version: v0.1.1 Author: R.D.Vaughan

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

> ./hulu.py -v
<grabber>
  <name>Hulu</name>
  <author>R.D.Vaughan</author>
  <thumbnail>hulu.png</thumbnail>
  <type>video</type>
  <description>Hulu.com is a free online video service that offers hit TV shows including Family Guy, 30 Rock, and the Daily Show with Jon Stewart, etc.</description>
  <version>v0.11</version>
  <search>true</search>
  <tree>true</tree>
</grabber>

> ./hulu.py -S "Burn Notice"
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:amp="http://www.adobe.com/amp/1.0"
  xmlns:atom="http://www.w3.org/2005/Atom"
  xmlns:cnettv="http://cnettv.com/mrss/"
  xmlns:content="http://purl.org/rss/1.0/modules/content/"
  xmlns:creativecommons="http://backend.userland.com/creativeCommonsRssModule"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:media="http://search.yahoo.com/mrss/">
  <channel>
    <title>Hulu</title>
    <link>http://hulu.com</link>
    <description>Hulu.com is a free online video service that offers hit TV shows including Family Guy, 30 Rock, and the Daily Show with Jon Stewart, etc.</description>
    <numresults>25</numresults>
    <returned>21</returned>
    <startindex>21</startindex>
    <item>
      <title>Burn Notice - A Dark Road</title>
      <author>USA</author>
      <pubDate>Mon, 25 Jan 2010 17:45:06 GMT</pubDate>
      <description>Commentary with Matt Nix</description>
      <link>http://www.hulu.com/watch/122855/burn-notice-a-dark-road#http%3A%2F%2Fwww.hulu.com%2Ffeed%2Fsearch%3Fquery%3DBurn%2BNotice%2Bdate%253Anewest%26sort_by%3Drelevance%26st%3D0</link>
      <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
        <mrss:thumbnail url="http://thumbnails.hulu.com/231/50029231/140025_145x80_generated.jpg"/>
        <mrss:content duration="05:19" height="" lang=""
          url="http://www.hulu.com/watch/122855/burn-notice-a-dark-road#http%3A%2F%2Fwww.hulu.com%2Ffeed%2Fsearch%3Fquery%3DBurn%2BNotice%2Bdate%253Anewest%26sort_by%3Drelevance%26st%3D0" width=""/>
      </mrss:group>
      <rating>4.1</rating>
    </item>
...
    <item>
      <title>Burn Notice - s3 | e15 - Good Intentions</title>
      <author>USA</author>
      <pubDate>Sat, 27 Feb 2010 03:05:06 GMT</pubDate>
      <description>Fiona seems to get more than she bargained for when she becomes involved with a paranoid kidnapper, only to discover that he's more than what he appears.</description>
      <link>http://www.hulu.com/watch/131000/burn-notice-good-intentions#http%3A%2F%2Fwww.hulu.com%2Ffeed%2Fsearch%3Fquery%3DBurn%2BNotice%2Bdate%253Anewest%26sort_by%3Drelevance%26st%3D0</link>
      <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
        <mrss:thumbnail url="http://thumbnails.hulu.com/183/50037183/148491_145x80_generated.jpg"/>
        <mrss:content duration="43:32" height="" lang=""
          url="http://www.hulu.com/watch/131000/burn-notice-good-intentions#http%3A%2F%2Fwww.hulu.com%2Ffeed%2Fsearch%3Fquery%3DBurn%2BNotice%2Bdate%253Anewest%26sort_by%3Drelevance%26st%3D0" width=""/>
      </mrss:group>
      <rating>4.5</rating>
    </item>
  </channel>
</rss>

> ./hulu.py -T
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:amp="http://www.adobe.com/amp/1.0"
  xmlns:atom="http://www.w3.org/2005/Atom"
  xmlns:cnettv="http://cnettv.com/mrss/"
  xmlns:content="http://purl.org/rss/1.0/modules/content/"
  xmlns:creativecommons="http://backend.userland.com/creativeCommonsRssModule"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:media="http://search.yahoo.com/mrss/">
  <channel>
    <title>Hulu</title>
    <link>http://hulu.com</link>
    <description>Hulu.com is a free online video service that offers hit TV shows including Family Guy, 30 Rock, and the Daily Show with Jon Stewart, etc.</description>
    <numresults>1</numresults>
    <returned>1</returned>
    <startindex>1</startindex>
    <directory name="Hulu" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/hulu.png">
      <directory name="Hulu - Recently added videos" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/hulu.png">
        <item>
          <title>NBC TODAY Show - Mood Meds Pose ‘Weighty’ Issue</title>
          <author>MSNBC</author>
          <pubDate>Wed, 07 Apr 2010 14:54:05 GMT</pubDate>
          <description>While battling depression, Lauren Slater faced a tough choice: She could take a prescription that would improve her mood, but it would make her gain weight. She discusses her experience with psychiatrist Dr. Catherine Birndorf and Dr. Nancy Snyderman.</description>
          <link>http://rss.hulu.com/~r/HuluRecentlyAddedVideos/~3/ExUXTu_hOTo/nbc-today-show-mood-meds-pose-%E2%80%98weighty%E2%80%99-issue</link>
          <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
            <mrss:thumbnail url="http://thumbnails.hulu.com/931/50046931/159024_145x80_generated.jpg"/>
            <mrss:content duration="05:05" height="" lang=""
              url="http://rss.hulu.com/~r/HuluRecentlyAddedVideos/~3/ExUXTu_hOTo/nbc-today-show-mood-meds-pose-%E2%80%98weighty%E2%80%99-issue" width=""/>
          </mrss:group>
          <rating>0.0</rating>
        </item>
...
        <item>
          <title>X-Play - Top 4 Open World Games of 2008</title>
          <author>G4</author>
          <pubDate>Thu, 10 Apr 2008 04:06:27 GMT</pubDate>
          <description>X-Play brings you their list of the most anticipated sandbox games for this year.</description>
          <link>http://www.hulu.com/watch/16729/x-play-top-4-open-world-games-of-2008#http%3A%2F%2Fwww.hulu.com%2Ffeed%2Fexpiring%2Fvideos</link>
          <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
            <mrss:thumbnail url="http://thumbnails.hulu.com/7/827/18906_145x80_manicured__6DZDj36Oa0GlHMbezc3SfA.jpg"/>
            <mrss:content duration="02:14" height="" lang=""
              url="http://www.hulu.com/watch/16729/x-play-top-4-open-world-games-of-2008#http%3A%2F%2Fwww.hulu.com%2Ffeed%2Fexpiring%2Fvideos" width=""/>
          </mrss:group>
          <rating>4.1</rating>
        </item>
      </directory>
    </directory>
  </channel>
</rss>
'''
__search_max_page_items__ = 20
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
#import nv_python_libs.common.common_api
try:
    '''Import the common python class
    '''
    import nv_python_libs.common.common_api as common_api
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/common" containing the modules common_api.py and
common_exceptions.py (v0.1.3 or greater),
They should have been included with the distribution of MythNetvision
Error(%s)
''' % e)
    sys.exit(1)
if common_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed common_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % target.__version__)
    sys.exit(1)

# Used for debugging
#import nv_python_libs.hulu.hulu_api as target
try:
    '''Import the python hulu support classes
    '''
    import nv_python_libs.hulu.hulu_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/hulu" containing the modules hulu_api and
hulu_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of hulu.py.
Error(%s)
''' % e)
    sys.exit(1)
if target.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed hulu_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % target.__version__)
    sys.exit(1)

# Verify that the main process modules are installed and accessible
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
    # Set the base processing directory that the grabber is installed
    target.baseProcessingDir = os.path.dirname( os.path.realpath( __file__ ))
    # Make sure the target functions have an instance of the common routines
    target.common = common_api.Common()
    main = process.mainProcess(target, apikey, )
    main.grabberInfo = {}
    main.grabberInfo['title'] = __title__
    main.grabberInfo['command'] = u'hulu.py'
    main.grabberInfo['mashup_title'] = __mashup_title__
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'hulu.png'
    main.grabberInfo['type'] = ['video', ]
    main.grabberInfo['desc'] = u"Hulu.com is a free online video service that offers hit TV shows including Family Guy, 30 Rock, and the Daily Show with Jon Stewart."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
