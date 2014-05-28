#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bbciplayer.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform BBC iplayer video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.bbc.co.uk/iplayer/ website. It
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
__title__ ="BBC iPlayer";
__mashup_title__ = "bbcipplayer"
__author__="R.D. Vaughan"
__version__="0.15"
# 0.1.0 Initial development
# 0.1.1 Added treeview support
# 0.1.2 Convert to detect and use either local or remote processing
# 0.13  Change to support xml version information display
# 0.14  Added the "command" tag to the xml version information display
# 0.15  Converted to new common_api.py library

__usage_examples__ ='''
(Option Help)
> ./bbciplayer.py -h
Usage: ./bbciplayer.py -hduvlST [parameters] <search text>
Version: v0.1.2 Author: R.D.Vaughan

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

> ./bbciplayer.py -v
<grabber>
  <name>BBC iPlayer</name>
  <author>R.D. Vaughan</author>
  <thumbnail>bbciplayer.png</thumbnail>
  <type>video</type>
  <description>BBC iPlayer is our service that lets you catch up with radio and television programmes from the past week.</description>
  <version>v0.13</version>
  <search>true</search>
  <tree>true</tree>
</grabber>

> ./bbciplayer.py -S "Doctor Who"
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:amp="http://www.adobe.com/amp/1.0"
  xmlns:atom="http://www.w3.org/2005/Atom"
  xmlns:cnettv="http://cnettv.com/mrss/"
  xmlns:content="http://purl.org/rss/1.0/modules/content/"
  xmlns:creativecommons="http://backend.userland.com/creativeCommonsRssModule"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:media="http://search.yahoo.com/mrss/">
  <channel>
    <title>BBC iPlayer</title>
    <link>http://www.bbc.co.uk</link>
    <description>BBC iPlayer is our service that lets you catch up with radio and television programmes from the past week.</description>
    <numresults>7</numresults>
    <returned>7</returned>
    <startindex>7</startindex>
    <item>
      <title>Doctor Who - Series 4 - 11. Turn Left</title>
      <author>British Broadcasting Corporation</author>
      <pubDate>Wed, 24 Mar 2010 18:35:41 GMT</pubDate>
      <description>Can Donna and Rose stop the approaching Darkness? (R)</description>
      <link>http://www.bbc.co.uk/iplayer/episode/b00c7ytx/Doctor_Who_Series_4_Turn_Left/</link>
      <media:group>
        <media:thumbnail url="http://node1.bbcimg.co.uk/iplayer/images/episode/b00c7ytx_120_68.jpg"/>
        <media:content duration="" height="" lang=""
          url="http://www.bbc.co.uk/iplayer/episode/b00c7ytx/Doctor_Who_Series_4_Turn_Left/" width=""/>
      </media:group>
      <rating>0.0</rating>
    </item>
...
    <item>
      <title>Doctor Who Confidential - Series 4 - 13. The End of an Era</title>
      <author>British Broadcasting Corporation</author>
      <pubDate>Wed, 24 Mar 2010 18:35:41 GMT</pubDate>
      <description>The series finale as the Doctor's arch enemy brings the universe to the edge of extinction (R)</description>
      <link>http://www.bbc.co.uk/iplayer/episode/b00cgphl/Doctor_Who_Confidential_Series_4_The_End_of_an_Era/</link>
      <media:group>
        <media:thumbnail url="http://node1.bbcimg.co.uk/iplayer/images/episode/b00cgphl_120_68.jpg"/>
        <media:content duration="" height="" lang=""
          url="http://www.bbc.co.uk/iplayer/episode/b00cgphl/Doctor_Who_Confidential_Series_4_The_End_of_an_Era/" width=""/>
      </media:group>
      <rating>0.0</rating>
    </item>
  </channel>
</rss>

> ./bbciplayer.py -T
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:amp="http://www.adobe.com/amp/1.0"
  xmlns:atom="http://www.w3.org/2005/Atom"
  xmlns:cnettv="http://cnettv.com/mrss/"
  xmlns:content="http://purl.org/rss/1.0/modules/content/"
  xmlns:creativecommons="http://backend.userland.com/creativeCommonsRssModule"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:media="http://search.yahoo.com/mrss/">
  <channel>
    <title>BBC iPlayer</title>
    <link>http://www.bbc.co.uk</link>
    <description>BBC iPlayer is our service that lets you catch up with radio and television programmes from the past week.</description>
    <numresults>1</numresults>
    <returned>1</returned>
    <startindex>1</startindex>
    <directory name="BBC iPlayer" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/bbciplayer.png">
      <directory name="BBC iPlayer - TV Highlights" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/bbciplayer.png">
        <item>
          <title>Great Ormond Street: Pushing the Boundaries</title>
          <author>BBC</author>
          <pubDate>Wed, 07 Apr 2010 17:00:49 GMT</pubDate>
          <description>A look at the work of the largest children&amp;apos;s cardiac unit in the UK.</description>
          <link>http://localhost:8080/emml/cgi-bin/bbciplayer_embedded.py?videocode=b00s02ct</link>
          <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
            <mrss:thumbnail url="http://node2.bbcimg.co.uk/iplayer/images/episode/b00s02ct_150_84.jpg"/>
            <mrss:content duration="" height="" lang=""
              url="http://localhost:8080/emml/cgi-bin/bbciplayer_embedded.py?videocode=b00s02ct" width=""/>
          </mrss:group>
          <rating>0.0</rating>
        </item>
...
        <item>
          <title>The Real Hustle: Series 7: On Holiday - Cutdowns: Episode 8</title>
          <author>BBC</author>
          <pubDate>Fri, 02 Apr 2010 07:29:46 GMT</pubDate>
          <description>Paul relieves some unsuspecting tourists of their spending money in Oxford.</description>
          <link>http://localhost:8080/emml/cgi-bin/bbciplayer_embedded.py?videocode=b00rw6xb</link>
          <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
            <mrss:thumbnail url="http://node2.bbcimg.co.uk/iplayer/images/episode/b00rw6xb_150_84.jpg"/>
            <mrss:content duration="" height="" lang=""
              url="http://localhost:8080/emml/cgi-bin/bbciplayer_embedded.py?videocode=b00rw6xb" width=""/>
          </mrss:group>
          <rating>0.0</rating>
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
#import nv_python_libs.bbciplayer.bbciplayer_api as target
try:
    '''Import the python bbciplayer support classes
    '''
    import nv_python_libs.bbciplayer.bbciplayer_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/bbciplayer" containing the modules bbciplayer_api and
bbciplayer_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of bbciplayer.py.
Error(%s)
''' % e)
    sys.exit(1)
if target.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed bbciplayer_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % target.__version__)
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
    main.grabberInfo['mashup_title'] = __mashup_title__
    main.grabberInfo['command'] = u'bbciplayer.py'
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'bbciplayer.png'
    main.grabberInfo['type'] = ['video', ]
    main.grabberInfo['desc'] = u"BBC iPlayer is our service that lets you catch up with radio and television programmes from the past week."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
