#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: linuxAction_api - XPath and XSLT functions for the www.jupiterbroadcasting.com RSS/HTML items
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
__title__ ="linuxAction_api - XPath and XSLT functions for the www.jupiterbroadcasting.com RSS/HTML"
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
        self.functList = ['linuxActionLinkGeneration', 'linuxActionTitleSeEp', 'linuxActioncheckIfDBItem', ]
        self.s_e_Regex = [
            # s12e05
            re.compile(u'''^.+?[Ss](?P<seasno>[0-9]+)\\e(?P<epno>[0-9]+).*$''', re.UNICODE),
            # Season 11 Episode 3
            re.compile(u'''^.+?Season\\ (?P<seasno>[0-9]+)\\ Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            ]
        self.namespaces = {
            'atom': "http://www.w3.org/2005/Atom",
            'atom10': "http://www.w3.org/2005/Atom",
            'media': "http://search.yahoo.com/mrss/",
            'itunes':"http://www.itunes.com/dtds/podcast-1.0.dtd",
            'xhtml': "http://www.w3.org/1999/xhtml",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'feedburner': "http://rssnamespace.org/feedburner/ext/1.0",
            'fb': "http://www.facebook.com/2008/fbml",
            }
        self.mediaIdFilters = [
            [etree.XPath('//object/@id', namespaces=self.namespaces ), None],
            ]
        self.FullScreen = u'http://linuxAction.com/show/popupPlayer?video_id=%s&quality=high&offset=0'
        self.FullScreenParser = common.parsers['html'].copy()
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def linuxActionLinkGeneration(self, context, *arg):
        '''Generate a link for the video.
        Call example: 'mnvXpath:linuxActionLinkGeneration(string(link))'
        return the url link
        '''
        webURL = arg[0]
        try:
            tmpHandle = urllib.urlopen(webURL)
            tmpHTML = unicode(tmpHandle.read(), 'utf-8')
            tmpHandle.close()
        except Exception, errmsg:
            sys.stderr.write(u"Error reading url(%s) error(%s)\n" % (webURL, errmsg))
            return webURL

        findText = u"<embed src="
        lenText = len(findText)
        posText = tmpHTML.find(findText)
        if posText == -1:
            return webURL
        tmpHTML = tmpHTML[posText+lenText+1:]

        tmpLink = tmpHTML[:tmpHTML.find('"')]
        if tmpLink.find('www.youtube.com') != -1:
            return u'%s&autoplay=1' % tmpLink
        else:
            return u'%s?autostart=1' % tmpLink
    # end linuxActionLinkGeneration()

    def linuxActionTitleSeEp(self, context, *arg):
        '''Parse the download link and extract an episode number
        Call example: 'mnvXpath:linuxActionTitleSeEp(title)'
        return the a massaged title element and an episode element in an array
        '''
        title = arg[0]
        index = title.find('|')
        if index > 0:
            title = title[:index].strip()
        index = title.find('The Linux Action Show')
        if index > 0:
            title = title[:index].strip()
        index = title.find('! Season')
        if index > 0:
            title = title[:index-1].strip()
        title = common.htmlToString('dummy', title)

        elementArray = []
        seasonNumber = u''
        episodeNumber = u''
        for index in range(len(self.s_e_Regex)):
            match = self.s_e_Regex[index].match(arg[0])
            if match:
                (seasonNumber, episodeNumber) = match.groups()
                seasonNumber = u'%s' % int(seasonNumber)
                episodeNumber = u'%s' % int(episodeNumber)
                elementArray.append(etree.XML(u"<title>%s</title>" % (u'S%02dE%02d: %s' % (int(seasonNumber), int(episodeNumber), title))))
                break
        else:
            elementArray.append(etree.XML(u"<title>%s</title>" % title ))
        if seasonNumber:
            tmpElement = etree.Element('{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season')
            tmpElement.text = seasonNumber
            elementArray.append(tmpElement)
        if episodeNumber:
            tmpElement = etree.Element('{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode')
            tmpElement.text = episodeNumber
            elementArray.append(tmpElement)
        return elementArray
    # end linuxActionTitleSeEp()

    def linuxActioncheckIfDBItem(self, context, *arg):
        '''Use a unique key value pairing to find out if the 'internetcontentarticles' table already
        has a matching item. This is done to save accessing the Internet when not required.
        Call example: 'mnvXpath:linuxActioncheckIfDBItem(title, author)'
        return True if a match was found
        return False if a match was not found
        '''
        titleElement = self.linuxActionTitleSeEp('dummy', arg[0])[0]
        return common.checkIfDBItem('dummy', {'feedtitle': 'Technology', 'title': titleElement.text, 'author': arg[1]})
    # end linuxActioncheckIfDBItem()

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
