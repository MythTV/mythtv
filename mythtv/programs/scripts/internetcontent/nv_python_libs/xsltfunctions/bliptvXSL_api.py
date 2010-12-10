#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bliptvXSL_api - XPath and XSLT functions for the Blip.tv RSS/HTML itmes
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
__title__ ="bliptvXSL_api - XPath and XSLT functions for the Blip.tv RSS/HTML"
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
        self.functList = ['bliptvFlvLinkGeneration', 'bliptvDownloadLinkGeneration', 'bliptvEpisode', 'bliptvIsCustomHTML', ]
        self.episodeRegex = [
            re.compile(u'''TERRA\\ (?P<episodeno>[0-9]+).*$''', re.UNICODE),
            ]
        self.namespaces = {
            'xsi': u"http://www.w3.org/2001/XMLSchema-instance",
            'media': u"http://search.yahoo.com/mrss/",
            'xhtml': u"http://www.w3.org/1999/xhtml",
            'atm': u"http://www.w3.org/2005/Atom",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'itunes':"http://www.itunes.com/dtds/podcast-1.0.dtd",
            'creativeCommons': "http://backend.userland.com/creativeCommonsRssModule",
            'geo': "http://www.w3.org/2003/01/geo/wgs84_pos#",
            'blip': "http://blip.tv/dtd/blip/1.0",
            'wfw': "http://wellformedweb.org/CommentAPI/",
            'amp': "http://www.adobe.com/amp/1.0",
            'dcterms': "http://purl.org/dc/terms",
            'gm': "http://www.google.com/schemas/gm/1.1",
            'mediaad': "http://blip.tv/dtd/mediaad/1.0",
            }
        self.flvFilter = etree.XPath(".//media:content[@type='video/x-flv']", namespaces=self.namespaces)
        self.m4vFilter = etree.XPath(".//media:content[@type='video/mp4' or @type='video/quicktime' or @type='video/x-m4v']", namespaces=self.namespaces)
        self.durationFilter = etree.XPath(".//blip:runtime/text()", namespaces=self.namespaces)
        self.linkFilter = etree.XPath("./link/text()", namespaces=self.namespaces)
        self.languageFilter = etree.XPath("../language/text()", namespaces=self.namespaces)
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def bliptvFlvLinkGeneration(self, context, arg):
        '''Generate a link for the Blip.tv site.
        Call example: 'mnvXpath:bliptvFlvLinkGeneration(.)'
        return the url link
        '''
        flvFile = self.flvFilter(arg[0])
        if len(flvFile):
            flvFileLink = flvFile[0].attrib['url']
            return u'%s%s' % (common.linkWebPage('dummy', 'bliptv'), flvFileLink.replace(u'.flv', u'').replace(u'http://blip.tv/file/get/', u''))
        else:
            return self.linkFilter(arg[0])[0]
    # end bliptvXSLLinkGeneration()

    def bliptvDownloadLinkGeneration(self, context, arg):
        '''Generate a download link for the Blip.tv site.
        Call example: 'mnvXpath:bliptvDownloadLinkGeneration(.)'
        return an array of one download link element
        '''
        downloadLink = etree.XML(u'<link></link>')
        flvFile = self.flvFilter(arg[0])
        m4vFile = self.m4vFilter(arg[0])
        if len(m4vFile):
            downloadLink.attrib['url'] = m4vFile[0].attrib['url']
            if m4vFile[0].attrib.get('width'):
                downloadLink.attrib['width'] = m4vFile[0].attrib['width']
            if m4vFile[0].attrib.get('height'):
                downloadLink.attrib['height'] = m4vFile[0].attrib['height']
            if m4vFile[0].attrib.get('fileSize'):
                downloadLink.attrib['length'] = m4vFile[0].attrib['fileSize']
            if len(self.durationFilter(arg[0])):
                downloadLink.attrib['duration'] = self.durationFilter(arg[0])[0]
            downloadLink.attrib['lang'] = self.languageFilter(arg[0])[0]
            return [downloadLink]
        elif len(flvFile):
            downloadLink.attrib['url'] = flvFile[0].attrib['url']
            if flvFile[0].attrib.get('width'):
                downloadLink.attrib['width'] = flvFile[0].attrib['width']
            if flvFile[0].attrib.get('height'):
                downloadLink.attrib['height'] = flvFile[0].attrib['height']
            if flvFile[0].attrib.get('fileSize'):
                downloadLink.attrib['length'] = flvFile[0].attrib['fileSize']
            if len(self.durationFilter(arg[0])):
                downloadLink.attrib['duration'] = self.durationFilter(arg[0])[0]
            downloadLink.attrib['lang'] = self.languageFilter(arg[0])[0]
            return [downloadLink]
        else:
            downloadLink.attrib['url'] = self.linkFilter(arg[0])[0]
            if len(self.durationFilter(arg[0])):
                downloadLink.attrib['duration'] = self.durationFilter(arg[0])[0]
            downloadLink.attrib['lang'] = self.languageFilter(arg[0])[0]
            return [downloadLink]
    # end bliptvDownloadLinkGeneration()

    def bliptvEpisode(self, context, arg):
        '''Parse the title string and extract an episode number
        Call example: 'mnvXpath:bliptvEpisode(./title/text())'
        return the url link
        '''
        episodeNumber = u''
        for index in range(len(self.episodeRegex)):
            match = self.episodeRegex[index].match(arg[0])
            if match:
                episodeNumber = match.groups()
                break
        return etree.XML(u'<episode>%s</episode>' % episodeNumber)
    # end bliptvEpisode()

    def bliptvIsCustomHTML(self, context, arg):
        '''Parse the item element and deternmine if there is a flv file
        Call example: 'mnvXpath:bliptvIsCustomHTML(.)'
        return True is there is a '.flv' file
        return False if there is no .flv' file
        '''
        if len(self.flvFilter(arg[0])):
            return True
        return False
    # end bliptvIsCustomHTML()

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
