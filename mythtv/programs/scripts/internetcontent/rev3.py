#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: rev3.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Revision3 video lookups for the MythTV Netvision plugin
#   based on information found on the http://revision3.com/ website. It
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
__title__ ="Revision3";
__mashup_title__ = "rev3"
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
> ./rev3.py -h
Usage: ./rev3.py -hduvlST [parameters] <search text>
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

> ./rev3.py -v
<grabber>
  <name>Revision3</name>
  <author>R.D.Vaughan</author>
  <thumbnail>rev3.png</thumbnail>
  <type>video</type>
  <description>Revision3 is the leading television network for the internet generation.</description>
  <version>v0.13</version>
  <search>true</search>
  <tree>true</tree>
</grabber>

> ./rev3.py -S "iPad" -p 1
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:amp="http://www.adobe.com/amp/1.0"
  xmlns:atom="http://www.w3.org/2005/Atom"
  xmlns:cnettv="http://cnettv.com/mrss/"
  xmlns:content="http://purl.org/rss/1.0/modules/content/"
  xmlns:creativecommons="http://backend.userland.com/creativeCommonsRssModule"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:media="http://search.yahoo.com/mrss/">
  <channel>
    <title>Revision3</title>
    <link>http://revision3.com/</link>
    <description>Revision3 is the leading television network for the internet generation.</description>
    <numresults>16</numresults>
    <returned>15</returned>
    <startindex>15</startindex>
    <item>
      <title>AppJudgment &amp;gt; Episode 101 &amp;gt; Voice Band for the ...</title>
      <author>Revision3</author>
      <pubDate>Mon, 05 Apr 2010 23:10:47 GMT</pubDate>
      <description>AppJudgment: Voice Band for the iPhone/iPod Touch - &amp;lt;br /&amp;gt; &amp;lt;br /&amp;gt; AppJudgment:&amp;lt;br /&amp;gt; Download&amp;lt;br /&amp;gt; &amp;lt;br /&amp;gt; &amp;lt;br /&amp;gt; &amp;lt;br /&amp;gt; Voice Band&amp;lt;br /&amp;gt; &amp;lt;br /&amp;gt; Version 1.1 ...</description>
      <link>http://revision3.com/appjudgment/ip_mau_voiceband/voice-band-for-the-iphone-ipod-touch?fs</link>
      <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
        <mrss:thumbnail url=""/>
        <mrss:content duration="" height="" lang=""
          url="http://revision3.com/appjudgment/ip_mau_voiceband/voice-band-for-the-iphone-ipod-touch?fs" width=""/>
      </mrss:group>
      <rating>0.0</rating>
    </item>
...
    <item>
      <title>The Totally Rad Show &amp;gt; Episode 150: Hockey Kahn ...</title>
      <author>Revision3</author>
      <pubDate>Mon, 05 Apr 2010 23:10:47 GMT</pubDate>
      <description>The Totally Rad Show: Hockey Kahn - Edge of Darkness, MAG, iPad Gaming, the End of Miramax, and More Twitter Q&amp;amp;A - We walk the Edge of Darkness with Mel Gibson. MAG ...</description>
      <link>http://revision3.com/trs/hockeykhan?fs</link>
      <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
        <mrss:thumbnail url=""/>
        <mrss:content duration="" height="" lang=""
          url="http://revision3.com/trs/hockeykhan?fs" width=""/>
      </mrss:group>
      <rating>0.0</rating>
    </item>
  </channel>
</rss>

> ./rev3.py -T
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0" xmlns:amp="http://www.adobe.com/amp/1.0"
  xmlns:atom="http://www.w3.org/2005/Atom"
  xmlns:cnettv="http://cnettv.com/mrss/"
  xmlns:content="http://purl.org/rss/1.0/modules/content/"
  xmlns:creativecommons="http://backend.userland.com/creativeCommonsRssModule"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:media="http://search.yahoo.com/mrss/">
  <channel>
    <title>Revision3</title>
    <link>http://revision3.com/</link>
    <description>Revision3 is the leading television network for the internet generation.</description>
    <numresults>1</numresults>
    <returned>1</returned>
    <startindex>1</startindex>
    <directory name="Revision3" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/revision3.png">
      <directory name="Best Of... (HD MP4)" thumbnail="http://bitcast-a.bitgravity.com/revision3/images/shows/bestof/bestof.jpg">
        <item>
          <title>Hick - How to Train Your Dragon - Best Of...</title>
          <author>feedback@revision3.com (Revision3)</author>
          <pubDate>Fri, 02 Apr 2010 00:00:00 GMT</pubDate>
          <description>Dreamworks Animation has played second fiddle to the Pixar juggernaut for years...</description>
          <link>http://revision3.com/bestof/trs-0158</link>
          <mrss:group xmlns:mrss="http://search.yahoo.com/mrss/">
            <mrss:thumbnail url="http://bitcast-a.bitgravity.com/revision3/images/shows/bestof/0501/bestof--0501--trs-0158--mini.thumb.jpg"/>
            <mrss:content duration="338" height="" lang="en"
              url="http://www.podtrac.com/pts/redirect.mp4/bitcast-a.bitgravity.com/revision3/web/bestof/0501/bestof--0501--trs-0158--hd.h264.mp4" width=""/>
          </mrss:group>
          <rating>0.0</rating>
        </item>
...
        <item>
          <title>What Should You Know: Revision3 Beta</title>
          <author/>
          <pubDate>Wed, 10 Sep 2008 20:25:30 GMT</pubDate>
          <description>So when someone comes up to you and asks if you know about Revision3 Beta. This is what you should say... http://wsyk.opensermo.com</description>
          <link>http://revision3beta.com/watch/wsyk/4dcf948a/</link>
          <media:group>
            <media:thumbnail url="http://cdn-ll-80.viddler.com/e2/thumbnail_2_4dcf948a.jpg"/>
            <media:content duration="" height="" lang=""
              url="http://revision3beta.com/watch/wsyk/4dcf948a/" width=""/>
          </media:group>
          <rating>0.0</rating>
        </item>
      </directory>
    </directory>
  </channel>
</rss>
'''
__search_max_page_items__ = 15  # Hardcoded value as this is the maximum allowed by Rev3
__tree_max_page_items__ = 15

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
#import nv_python_libs.rev3.rev3_api as target
try:
    '''Import the python Rev3 support classes
    '''
    import nv_python_libs.rev3.rev3_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/rev3" containing the modules rev3_api.py and
rev3_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of rev3.py.
Error(%s)
''' % e)
    sys.exit(1)
if target.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed rev3_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % target.__version__)
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
    main.grabberInfo['command'] = u'rev3.py'
    main.grabberInfo['mashup_title'] = __mashup_title__
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'rev3.png'
    main.grabberInfo['type'] = ['video', ]
    main.grabberInfo['desc'] = u"Revision3 is the leading television network for the internet generation."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
