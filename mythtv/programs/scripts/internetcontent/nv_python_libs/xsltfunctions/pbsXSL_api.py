#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: pbsXSL_api - XPath and XSLT functions for the PBS RSS/HTML itmes
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions
#           for the conversion of data to the MNV standard RSS output format.
#           See this link for the specifications:
#           http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="pbsXSL_api - XPath and XSLT functions for the PBS RSS/HTML"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MNV standard RSS output format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
'''

__version__="v0.1.0"
# 0.1.0 Initial development


# Specify the class names that have XPath extention functions
__xpathClassList__ = ['xpathFunctions', ]

# Specify the XSLT extention class names. Each class is a stand lone extention function
#__xsltExtentionList__ = ['xsltExtExample', ]
__xsltExtentionList__ = []

import os, sys, re, time, datetime, shutil, urllib, string
from copy import deepcopy


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


class xpathFunctions(object):
    """Functions specific extending XPath
    """
    def __init__(self):
        self.functList = ['pbsTitleSeriesEpisodeLink', 'pbsDuration', 'pbsDownloadlink']
        self.seriesEpisodeRegex = [
            # Season 8: Episode 1
            re.compile(u'''^.+?Season\\ (?P<seasno>[0-9]+)\\:\\ Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Season 8, Episode 1
            re.compile(u'''^.+?Season\\ (?P<seasno>[0-9]+)\\,\\ Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Episode 1:
            re.compile(u'''^.+?Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            ]
        self.namespaces = {
            'media': u"http://search.yahoo.com/mrss/",
            'xhtml': u"http://www.w3.org/1999/xhtml",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'mediaad': "http://PBS/dtd/mediaad/1.0",
            }
        self.persistence = {}
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def pbsTitleSeriesEpisodeLink(self, context, *arg):
        '''Generate a link for the PBS site.
        Call example: 'mnvXpath:pbsTitleSeriesEpisodeLink(normalize-space(./title),normalize-space(./link))/*'
        return the title, link and season and episode number elements if any were in the title
        '''
        tmpTitle = arg[0]
        tmpLink = arg[1]

        # Set the custom HTML PBS link
        tmpVideoCode = tmpLink.replace(u'http://video.pbs.org/video/', u'')[:-1]
        self.persistence[tmpLink] = common.linkWebPage('dummy', 'pbs')

        # Parse out any Season
        seasonNumber = None
        episodeNumber = None
        for index in range(len(self.seriesEpisodeRegex)):
            match = self.seriesEpisodeRegex[index].match(tmpTitle)
            if match:
                if index < 2:
                    (seasonNumber, episodeNumber) = match.groups()
                    break
                else:
                    episodeNumber = match.groups()[0]
                    break

        # Massage video title
        index = tmpTitle.rfind('|')
        if index != -1:
            tmpTitle = tmpTitle[index+1:].strip()
        if seasonNumber and episodeNumber:
            tmpTitle = u'S%02dE%02d: %s' % (int(seasonNumber), int(episodeNumber), tmpTitle)
        elif episodeNumber:
            index = tmpTitle.find(':')
            if index != -1:
                tmpTitle = tmpTitle[index+1:].strip()
            tmpTitle = u'Ep%02d: %s' % (int(episodeNumber), tmpTitle)
        self.persistence[tmpLink] = common.linkWebPage('dummy', 'pbs').replace(u'TITLE', urllib.quote(tmpTitle)).replace(u'VIDEOCODE', tmpVideoCode)

        # Make the elements to include in the item
        elementTmp = etree.XML('<xml></xml>')
        etree.SubElement(elementTmp, "title").text = tmpTitle
        if seasonNumber:
            etree.SubElement(elementTmp, "season").text = u"%s" % int(seasonNumber)
        if episodeNumber:
            etree.SubElement(elementTmp, "episode").text = u"%s" % int(episodeNumber)
        etree.SubElement(elementTmp, "link").text = self.persistence[tmpLink]
        return elementTmp
    # end pbsTitleSeriesEpisodeLink()

    def pbsDuration(self, context, *arg):
        '''Return the video duration in seconds
        Call example: 'mnvXpath:pbsDuration(normalize-string(./dd/p[@class="info"]/span[@class="time"]))'
        return the video duration
        '''
        if not arg[0]:
            return u''
        return arg[0][:-3]
    # end pbsDuration()

    def pbsDownloadlink(self, context, *arg):
        '''Return a previously created download link
        Call example: 'mnvXpath:pbsDownloadlink(normalize-space(./link))'
        return the url link
        '''
        downloadLink = self.persistence[arg[0]]
        del self.persistence[arg[0]]
        return downloadLink
    # end pbsDownloadlink()

######################################################################################################
#
# End of XPath extension functions
#
######################################################################################################

######################################################################################################
#
# Start of XSLT extension functions
#
######################################################################################################

######################################################################################################
#
# End of XSLT extension functions
#
######################################################################################################
