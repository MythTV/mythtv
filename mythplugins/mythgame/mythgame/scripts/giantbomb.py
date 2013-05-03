#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: giantbomb.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Game data lookups
#   based on information found on the http://www.giantbomb.com/ website. It
#   follows the MythTV Univeral standards set for grabbers
#   http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
#   This script uses the python module giantbomb_api.py which should be included
#   with this script.
#   The giantbomb_api.py module uses the full access XML api published by
#   api.giantbomb.com see: http://api.giantbomb.com/documentation/
#   Users of this script are encouraged to populate www.giantbomb.com with Game
#   informationand images. The richer the source the more
#   valuable the script.
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Verify the command line options (display help or version and exit)
#   2) Verify that www.giantbomb.com has the Game being requested exit if it does not exit
#   3) Find the requested information and send to stdout if any found
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="GiantBomb Query";
__author__="R.D. Vaughan"
__version__="0.11"
# 0.10  Initial development
# 0.11  Added the -l option to conform to grabber standards. Currently www.giantbomb.com does not support
#       multiple languages.


__usage_examples__='''
Request giantbomb.py verison number:
> ./giantbomb.py -v
<grabber>
  <name>GiantBomb Query</name>
  <author>R.D. Vaughan</author>
  <thumbnail>giantbomb.png</thumbnail>
  <command>giantbomb.py</command>
  <type>games</type>
  <description>Search and Metadata downloads for Games from the giantbomb.com API</description>
  <version>v0.11</version>
</grabber>

Request a list of matching game titles:
> ./giantbomb.py -M "Terminator"
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>en</language>
    <title>Terminator Salvation</title>
    <inetref>24514</inetref>
    <description>Based on the “Terminator Salvation” film, the game offers players the chance to assume the role of John Connor, a soldier in the resistance, battling for survival against the far superior forces of Skynet.</description>
    <releasedate>2009-05-19</releasedate>
    <homepage>http://www.giantbomb.com/terminator-salvation/61-24514/</homepage>
    <images>
      <image type="coverart" url="http://media.giantbomb.com/uploads/1/13154/1110143-gb_super.png" thumb="http://media.giantbomb.com/uploads/1/13154/1110143-gb_thumb.png"/>
    </images>
    <lastupdated>Thu, 04 Mar 2010 06:23:22 GMT</lastupdated>
  </item>
...
  <item>
    <language>en</language>
    <title>Terminator 2</title>
    <inetref>28832</inetref>
    <description>Terminator 2</description>
    <homepage>http://www.giantbomb.com/terminator-2/61-28832/</homepage>
    <lastupdated>Sat, 03 Oct 2009 00:21:31 GMT</lastupdated>
  </item>
</metadata>

Request game details using a GiantBomb#:
> ./giantbomb.py -D 24514
<?xml version='1.0' encoding='UTF-8'?>
<metadata>
  <item>
    <language>en</language>
    <title>Terminator Salvation</title>
    <description>Based on the “Terminator Salvation” film, the game offers players the chance to assume the role of John Connor, a soldier in the resistance, battling for survival against the far superior forces of Skynet.</description>
    <categories>
      <category type="genre" name="Shooter"/>
      <category type="genre" name="Vehicular Combat"/>
      <category type="genre" name="Action"/>
    </categories>
    <systems>
      <system>PC</system>
      <system>PlayStation 3</system>
      <system>Xbox 360</system>
    </systems>
    <studios>
      <studio name="GRIN"/>
      <studio name="Evolved Games"/>
      <studio name="Warner Bros. Interactive Entertainment Inc."/>
    </studios>
    <releasedate>2009-05-19</releasedate>
    <lastupdated>Thu, 04 Mar 2010 06:23:22 GMT</lastupdated>
    <inetref>24514</inetref>
    <homepage>http://www.giantbomb.com/terminator-salvation/61-24514/</homepage>
    <trailer/>
    <people>
      <person name="Rose McGowan" job="Actor"/>
    </people>
    <images>
      <image type="coverart" url="http://media.giantbomb.com/uploads/1/13154/1110143-gb_super.png" thumb="http://media.giantbomb.com/uploads/1/13154/1110143-gb_thumb.png"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115329-terminatorsalvation_2009_08_23_12_36_06_46_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115329-terminatorsalvation_2009_08_23_12_36_06_46_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115327-terminatorsalvation_2009_08_23_12_35_50_70_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115327-terminatorsalvation_2009_08_23_12_35_50_70_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115326-terminatorsalvation_2009_08_23_12_35_40_07_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115326-terminatorsalvation_2009_08_23_12_35_40_07_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115325-terminatorsalvation_2009_08_23_12_35_24_83_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115325-terminatorsalvation_2009_08_23_12_35_24_83_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115324-terminatorsalvation_2009_08_23_12_34_32_33_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115324-terminatorsalvation_2009_08_23_12_34_32_33_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115323-terminatorsalvation_2009_08_23_12_33_27_96_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115323-terminatorsalvation_2009_08_23_12_33_27_96_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115322-terminatorsalvation_2009_08_23_12_33_19_64_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115322-terminatorsalvation_2009_08_23_12_33_19_64_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115321-terminatorsalvation_2009_08_23_12_33_15_70_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115321-terminatorsalvation_2009_08_23_12_33_15_70_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115319-terminatorsalvation_2009_08_23_12_32_28_68_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115319-terminatorsalvation_2009_08_23_12_32_28_68_thumb.jpg"/>
      <image type="screenshot" url="http://media.giantbomb.com/uploads/0/8484/1115320-terminatorsalvation_2009_08_23_12_32_34_27_super.jpg" thumb="http://media.giantbomb.com/uploads/0/8484/1115320-terminatorsalvation_2009_08_23_12_32_34_27_thumb.jpg"/>
    </images>
  </item>
</metadata>
'''

import sys, os
from optparse import OptionParser
import re
from string import capitalize


class OutStreamEncoder(object):
    """Wraps a stream with an encoder
    """
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, unicode):
            self.out.write(obj.encode(self.encoding))
        else:
            self.out.write(obj)

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
# Sub class sys.stdout and sys.stderr as a utf8 stream. Deals with print and stdout unicode issues
sys.stdout = OutStreamEncoder(sys.stdout)
sys.stderr = OutStreamEncoder(sys.stderr)


try:
    from StringIO import StringIO
    from lxml import etree
except Exception, e:
    sys.stderr.write(u'\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
    sys.exit(1)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
version = ''
for digit in etree.LIBXML_VERSION:
    version+=str(digit)+'.'
version = version[:-1]
if version < '2.7.2':
    sys.stderr.write(u'''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


# Verify that the giantbomb_api modules are installed and accessable
#import giantbomb.giantbomb_api as giantbomb_api    # Only used when debugging. Normally commented out.

try:
    import giantbomb.giantbomb_api as giantbomb_api
except Exception, e:
    sys.stderr.write('''
The subdirectory "giantbomb" containing the modules giantbomb_api.py (v0.1.0 or greater) and
giantbomb_exceptions.py must have been installed with the MythTV gaming plugin.
Error:(%s)
''' %  e)
    sys.exit(1)

if giantbomb_api.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed giantbomb_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % giantbomb_api.__version__)
    sys.exit(1)


def main():
    """Gets game details using a GiantBomb# OR using a game name
    """
    # api.giantbomb.com api key provided for Mythtv
    apikey = "b5883a902a8ed88b15ce21d07787c94fd6ad9f33"

    parser = OptionParser(usage=u"%prog usage: giantbomb -hdluvMD [parameters]\n <game name or gameid number>\n\nFor details on using giantbomb from the command execute './giantbomb.py -u'. For details on the meaning of the XML element tags see the wiki page at:\nhttp://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format")

    parser.add_option(  "-d", "--debug", action="store_true", default=False, dest="debug",
                        help=u"Show debugging info")
    parser.add_option(  "-u", "--usage", action="store_true", default=False, dest="usage",
                        help=u"Display examples for executing the giantbomb script")
    parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                        help=u"Display version and author")
    parser.add_option(  "-l", "--language", metavar="LANGUAGE", default=u'en', dest="language",
                        help=u"Select data that matches the specified language. At this time giantbomb.com only supports 'en' English.")
    parser.add_option(  "-a", "--area", metavar="AREA", default=u"gb", dest="area",
			help=u"Select data that matches the specified country. This option is currently not used.")
    parser.add_option(  "-M", "--gamelist", action="store_true", default=False, dest="gamelist",
                        help=u"Get matching Movie list")
    parser.add_option(  "-D", "--gamedata", action="store_true", default=False, dest="gamedata",
                        help=u"Get Movie metadata including graphic URLs")

    opts, args = parser.parse_args()

    # Make all command line arguments unicode utf8
    for index in range(len(args)):
        args[index] = unicode(args[index], 'utf8')

    if opts.debug:
        sys.stdout.write("\nopts: %s\n" % opts)
        sys.stdout.write("\nargs: %s\n\n" % args)

    # Process version command line requests
    if opts.version:
        version = etree.XML(u'<grabber></grabber>')
        etree.SubElement(version, "name").text = __title__
        etree.SubElement(version, "author").text = __author__
        etree.SubElement(version, "thumbnail").text = 'giantbomb.png'
        etree.SubElement(version, "command").text = 'giantbomb.py'
        etree.SubElement(version, "type").text = 'games'
        etree.SubElement(version, "description").text = 'Search and Metadata downloads for Games from the giantbomb.com API'
        etree.SubElement(version, "version").text = __version__
        sys.stdout.write(etree.tostring(version, encoding='UTF-8', pretty_print=True))
        sys.exit(0)

    # Process usage command line requests
    if opts.usage:
        sys.stdout.write(__usage_examples__)
        sys.exit(0)

    if not len(args) == 1:
        sys.stderr.write("! Error: There must be one value for any option. Your options are (%s)\n" % (args))
        sys.exit(1)

    if args[0] == u'':
        sys.stderr.write("! Error: There must be a non-empty argument, yours is empty.\n")
        sys.exit(1)

    Queries = giantbomb_api.gamedbQueries(apikey,
                debug = opts.debug,
                )

    # Process requested option
    if opts.gamelist:                  # Game Search -M
       Queries.gameSearch(args[0])
    elif opts.gamedata:                # Game metadata -D
       Queries.gameData(args[0])

    sys.exit(0)
# end main()

if __name__ == '__main__':
    main()
