#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tedtalksXSL_api - XPath and XSLT functions for the TedTalks RSS/HTML itmes
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
__title__ ="tedtalksXSL_api - XPath and XSLT functions for the TedTalks RSS/HTML"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MNV standard RSS output format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format
'''

__version__="v0.1.1"
# 0.1.0 Initial development
# 0.1.1 Fixed URL of Flash player due to web site change


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
        self.functList = ['tedtalksMakeItem', 'tedtalksGetItem', 'tedtalksMakeLink', 'tedtalksTitleRSS', ]
        self.namespaces = {
            'media': u"http://search.yahoo.com/mrss/",
            'xhtml': u"http://www.w3.org/1999/xhtml",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            }
        # XPath filters
        self.descriptionFilter = etree.XPath('//p[@id="tagline"]', namespaces=self.namespaces)
        self.durationFilter = etree.XPath('//dl[@class="talkMedallion  clearfix"]//em[@class="date"]/text()', namespaces=self.namespaces)
        self.persistence = {}
        self.flvPlayerLink = u'http://static.hd-trailers.net/mediaplayer/player.swf?autostart=true&backcolor=000000&frontcolor=999999&lightcolor=000000&screencolor=000000&controlbar=over&file=%s'

    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def tedtalksMakeItem(self, context, *arg):
        '''Generate item elements from a Video HTML page on the TedTalks site.
        Call example: 'mnvXpath:tedtalksMakeItem(concat('http://www.ted.com', normalize-space(./@href), $paraMeter))/link'
        return an number of item elements
        '''
        webURL = arg[0]
        parmDict = self.parameterArgs( arg[1])

        # Read the detailed Web page
        try:
            tmpHandle = urllib.urlopen(webURL)
            htmlString = unicode(tmpHandle.read(), 'utf-8')
            tmpHandle.close()
        except errmsg:
            sys.stderr.write(u'! Error: TedTalk web page read issue for URL(%s)\nerror(%s)\n' % (webURL, errmsg))
            return etree.XML(u"<xml></xml>" )

        htmlElementTree = etree.HTML(htmlString)

        # Create the base element that will contain the item elements. It must have a "media" namespace
        mediaNamespace = "http://search.yahoo.com/mrss/"
        media = "{%s}" % mediaNamespace
        NSMAP = {'media' : mediaNamespace}
        elementTmp = etree.Element(media + "media", nsmap=NSMAP)

        # Get and format the publishing Date
        tmpPubDate = self.stripSubstring(htmlString, '\tpd:"', '"')
        if tmpPubDate:
            tmpPubDate = common.pubDate('dummy', u'1 '+tmpPubDate, "%d %b %Y")
        else:
            tmpPubDate = common.pubDate('dummy', u'')

        # Get the flv link
        if self.stripSubstring(htmlString, '\ths:"', '"'):
            tmpFlvLink = self.flvPlayerLink % u'http://video.ted.com/%s' % self.stripSubstring(htmlString, '\ths:"', '"').replace('high', parmDict['flv'])
        else:
          tmpFlvLink = webURL

        # Get the download link
        tmpFileName = self.stripSubstring(htmlString, '\ths:"talks/dynamic/', '-')
        tmpDownloadLink = u'http://video.ted.com/talks/podcast/%s' % tmpFileName
        if parmDict['download'] == 'HD':
            tmpDownloadLink+='_480.mp4'
        else:
            tmpDownloadLink+='.mp4'

        # Get thumbnail link
        tmpThumbNail = self.stripSubstring(htmlString, 'amp;su=', '&amp')

        # Get item description
        tmpDesc = self.descriptionFilter(htmlElementTree)
        if len(tmpDesc):
            tmpDesc = tmpDesc[0].text
        else:
            tmpDesc = u''

        # Get duration
        tmpDuration = self.durationFilter(htmlElementTree)
        if len(tmpDuration):
            index = tmpDuration[0].find(' ')
            if index != -1:
                tmpDuration = common.convertDuration('dummy', tmpDuration[0][:index])
            else:
                tmpDuration = u''
        else:
            tmpDuration = u''

        # Add Item elements and attributes
        etree.SubElement(elementTmp, "pubDate").text = tmpPubDate
        etree.SubElement(elementTmp, "description").text = tmpDesc
        etree.SubElement(elementTmp, "link").text = tmpFlvLink
        tmpgroup = etree.SubElement(elementTmp, media + "group")
        tmpTNail = etree.SubElement(tmpgroup, media + "thumbnail")
        tmpTNail.attrib['url'] = tmpThumbNail
        tmpContent = etree.SubElement(tmpgroup, media + "content")
        tmpContent.attrib['url'] = tmpDownloadLink
        tmpContent.attrib['duration'] = tmpDuration
        tmpContent.attrib['lang'] = u'en'

        self.persistence[webURL] = deepcopy(elementTmp)
        return elementTmp
    # end tedtalksMakeItem()

    def tedtalksGetItem(self, context, *arg):
        '''Return item elements that were previously created in "tedtalksMakeItem" call
        Call example: 'mnvXpath:tedtalksGetItem(concat('http://www.ted.com', normalize-space(./@href))/*'
        return an number of item elements
        '''
        elementTmp = self.persistence[arg[0]]
        del self.persistence[arg[0]]
        return elementTmp
    # end tedtalksGetItem()

    def tedtalksMakeLink(self, context, *arg):
        '''Return item elements that were previously created in "tedtalksMakeItem" call
        Call example: 'mnvXpath:tedtalksMakeLink(enclosure/@url, $paraMeter)'
        return a link for playing the flv file
        '''
        tmpDownloadLink = arg[0]
        parmDict = self.parameterArgs(arg[1])
        index = tmpDownloadLink.rfind('/')
        videoFileName = u'http://video.ted.com/talks/dynamic%s' % tmpDownloadLink[index:].replace('_480', u'').replace('.mp4', u'')
        videoFileName+=u'-%s.flv' % parmDict['flv']
        return self.flvPlayerLink % videoFileName
    # end tedtalksMakeLink()

    def tedtalksTitleRSS(self, context, *arg):
        '''Return item elements that were previously created in "tedtalksMakeItem" call
        Call example: 'mnvXpath:tedtalksTitleRSS(string(title))'
        return a massaged title string
        '''
        title = arg[0]
        index = title.rfind('-')
        if index == -1:
            return title
        return title[:index].strip()
    # end tedtalksTitleRSS()

    def stripSubstring(self, string, startText, terminatorChar):
        '''Return a substring terminated by specific character(s)
        return a substring
        '''
        index = string.find(startText)
        if index == -1:
            return u''
        string = string[index+len(startText):]
        index = string.find(terminatorChar)
        if index == -1:
            return u''
        return string[:index].strip()
    # end stripSubstring()

    def parameterArgs(self, parameters, terminatorChar=u';'):
        '''Set the parameters for TedTalks
        return a dictionary of parameters
        '''
        paramDict = {}
        args = parameters.split(terminatorChar)
        for arg in args:
            tmp = arg.split('=')
            paramDict[tmp[0]] = tmp[1]
        return paramDict
    # end parameterArgs()

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
