#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mevio_api - XPath and XSLT functions for the Mevio RSS/HTML items
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
__title__ ="mevio_api - XPath and XSLT functions for the www.mevio.com RSS/HTML"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MNV standard RSS output format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
'''

__version__="v0.1.1"
# 0.1.0 Initial development
# 0.1.1 Fixed a bug when an autoplay link cannot be created
#       Added MP4 as an acceptable downloadable video file type
#       Added checking to see if the item is already in the data base

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
        self.functList = ['mevioLinkGeneration', 'mevioTitle', 'mevioEpisode', 'mevioCheckIfDBItem', ]
        self.episodeRegex = [
            # Episode 224
            re.compile(u'''^.+?Episode\\ (?P<episodeno>[0-9]+).*$''', re.UNICODE),
            # CrankyGeeks 136:
            re.compile(u'''^.+?(?P<episodeno>[0-9]+)\\:.*$''', re.UNICODE),
            ]
        self.namespaces = {
            'atom10': u"http://www.w3.org/2005/Atom",
            'media': u"http://search.yahoo.com/mrss/",
            'itunes':"http://www.itunes.com/dtds/podcast-1.0.dtd",
            'xhtml': u"http://www.w3.org/1999/xhtml",
            'feedburner': u"http://rssnamespace.org/feedburner/ext/1.0",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'dc': "http://purl.org/dc/elements/1.1/",
            'fb': "http://www.facebook.com/2008/fbml/",
            }
        self.mediaIdFilters = [
            [etree.XPath(".//embed/@flashvars", namespaces=self.namespaces), re.compile(u'''^.+?MediaId=(?P<videocode>[0-9]+).*$''', re.UNICODE)],
            [etree.XPath(".//div[@class='player_wrapper']/a/@href", namespaces=self.namespaces), re.compile(u'''^.+?\\'(?P<videocode>[0-9]+)\\'\\)\\;.*$''', re.UNICODE)]
            ]
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def mevioLinkGeneration(self, context, *arg):
        '''Generate a link for the video.
        Call example: 'mnvXpath:mevioLinkGeneration(string(link))'
        return the url link
        '''
        webURL = arg[0]
        try:
            tmpHTML = etree.parse(webURL, etree.HTMLParser())
        except Exception, errmsg:
            sys.stderr.write(u"Error reading url(%s) error(%s)\n" % (webURL, errmsg))
            return webURL

        for index in range(len(self.mediaIdFilters)):
            mediaId = self.mediaIdFilters[index][0](tmpHTML)
            if not len(mediaId):
            	continue
            match = self.mediaIdFilters[index][1].match(mediaId[0])
            if match:
                videocode = match.groups()
                return u'file://%s/nv_python_libs/configs/HTML/mevio.html?videocode=%s' % (common.baseProcessingDir, videocode[0])
        else:
            return webURL
    # end mevioLinkGeneration()

    def mevioTitle(self, context, arg):
        '''Parse the title string extract only the title text removing the redundant show name
        Call example: 'mnvXpath:mevioTitle(./title/text())'
        return the title text
        '''
        epText = self.mevioEpisode('dummy', arg).text
        if epText:
            epText = u'Ep %s: ' % epText
        else:
            epText = u''
        seperatorStrs = [[' | ', 'before'], [': ', 'after'], [' - ', 'before']]
        for sepStr in seperatorStrs:
            if sepStr[1] == 'after':
                index = arg[0].find(sepStr[0])
            else:
                index = arg[0].rfind(sepStr[0])
            if index != -1:
                if sepStr[1] == 'after':
                    return u'%s%s' % (epText, arg[0][index+len(sepStr[0]):].strip())
                else:
                    return u'%s%s' % (epText, arg[0][:index].strip())
        else:
            if epText:
                return epText
            else:
                return arg[0].strip()
    # end mevioTitle()

    def mevioEpisode(self, context, arg):
        '''Parse the title string and extract an episode number
        Call example: 'mnvXpath:mevioEpisode(./title/text())'
        return an episode element
        '''
        episodeNumber = u''
        for index in range(len(self.episodeRegex)):
            match = self.episodeRegex[index].match(arg[0])
            if match:
                episodeNumber = match.groups()
                break
        return etree.XML(u'<episode>%s</episode>' % episodeNumber)
    # end mevioEpisode()

    def mevioCheckIfDBItem(self, context, *arg):
        '''Use a unique key value pairing to find out if the 'internetcontentarticles' table already
        has a matching item. This is done to save accessing the Internet when not required.
        Call example: 'mnvXpath:mevioCheckIfDBItem(title, description)'
        return True if a match was found
        return False if a match was not found
        '''
        return common.checkIfDBItem('dummy', {'feedtitle': 'Technology', 'title': arg[0], 'description': arg[1]})
    # end mevioCheckIfDBItem()

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
