#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: revision3XSL_api - XPath and XSLT functions for the Revision3 RSS/HTML items
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
__title__ ="revision3XSL_api - XPath and XSLT functions for the www.revision3L.com RSS/HTML"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MNV standard RSS output format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
'''

__version__="v0.1.1"
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
        self.functList = ['revision3LinkGeneration', 'revision3Episode', 'revision3checkIfDBItem', ]
        self.episodeRegex = [
            re.compile(u'''^.+?\\-\\-(?P<episodeno>[0-9]+)\\-\\-.*$''', re.UNICODE),
            ]
        self.namespaces = {
            'atom': "http://www.w3.org/2005/Atom",
            'media': "http://search.yahoo.com/mrss/",
            'itunes':"http://www.itunes.com/dtds/podcast-1.0.dtd",
            'xhtml': "http://www.w3.org/1999/xhtml",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'cnettv': "http://cnettv.com/mrss/",
            'creativeCommons': "http://backend.userland.com/creativeCommonsRssModule",
            'amp': "http://www.adobe.com/amp/1.0",
            'content': "http://purl.org/rss/1.0/modules/content/",
            }
        self.mediaIdFilters = [
            [etree.XPath('//object/@id', namespaces=self.namespaces ), None],
            ]
        self.FullScreen = u'http://revision3.com/show/popupPlayer?video_id=%s&quality=high&offset=0'
        self.FullScreenParser = common.parsers['html'].copy()
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def revision3LinkGeneration(self, context, *arg):
        '''Generate a link for the video.
        Call example: 'mnvXpath:revision3LinkGeneration(string(link))'
        return the url link
        '''
        webURL = arg[0]
        try:
            tmpHTML = etree.parse(webURL, self.FullScreenParser)
        except Exception, errmsg:
            sys.stderr.write(u"Error reading url(%s) error(%s)\n" % (webURL, errmsg))
            return webURL

        for index in range(len(self.mediaIdFilters)):
            mediaId = self.mediaIdFilters[index][0](tmpHTML)
            if not len(mediaId):
            	continue
            if self.mediaIdFilters[index][1]:
                match = self.mediaIdFilters[index][1].match(mediaId[0])
                if match:
                    videocode = match.groups()
                    return self.FullScreen % (videocode[0])
            else:
                return self.FullScreen % (mediaId[0].strip().replace(u'player-', u''))
        else:
            return webURL
    # end revision3LinkGeneration()

    def revision3Episode(self, context, *arg):
        '''Parse the download link and extract an episode number
        Call example: 'mnvXpath:revision3Episode(.)'
        return the a massaged title element and an episode element in an array
        '''
        title = arg[0][0].find('title').text
        link = arg[0][0].find('enclosure').attrib['url']

        episodeNumber = u''
        for index in range(len(self.episodeRegex)):
            match = self.episodeRegex[index].match(link)
            if match:
                episodeNumber = int(match.groups()[0])
                break
        titleElement = etree.XML(u"<xml></xml>")
        etree.SubElement(titleElement, "title").text = u'Ep%03d: %s' % (episodeNumber, title)
        if episodeNumber:
            etree.SubElement(titleElement, "episode").text = u'%s' % episodeNumber
        return [titleElement]
    # end revision3Episode()

    def revision3checkIfDBItem(self, context, arg):
        '''Use a unique key value pairing to find out if the 'internetcontentarticles' table already
        has a matching item. This is done to save accessing the Internet when not required.
        Call example: 'mnvXpath:revision3checkIfDBItem(.)'
        return True if a match was found
        return False if a match was not found
        '''
        return common.checkIfDBItem('dummy', {'title': self.revision3Episode(context, arg)[0].find('title').text, })
    # end revision3checkIfDBItem()

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
