#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: spitzer_api - XPath and XSLT functions for the www.spitzer.caltech.edu grabber
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
__title__ ="spitzer_api - XPath and XSLT functions for the www.spitzer.caltech.edu grabber"
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
        self.functList = ['spitzerLinkGeneration', 'spitzerThumbnailLink', 'spitzerCheckIfDBItem', ]
        self.TextTail = etree.XPath("string()")
        self.persistence = {}
        self.htmlParser = common.parsers['html'].copy()
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def spitzerLinkGeneration(self, context, *args):
        '''Generate a link for the www.spitzer.caltech.edu site.
        Call example: 'mnvXpath:spitzerLinkGeneration(normalize-space(link), $paraMeter)'
        return the url link
        '''
        webURL = args[0]
        pageTitle = args[1]
        try:
            tmpHandle = urllib.urlopen(webURL)
            tmpHTML = unicode(tmpHandle.read(), 'utf-8')
            tmpHandle.close()
        except Exception, errmsg:
            sys.stderr.write(u"Error reading url(%s) error(%s)\n" % (webURL, errmsg))
            return webURL

        # Get videocode
        findText = u"file=mp4:"
        lenText = len(findText)
        posText = tmpHTML.find(findText)
        if posText == -1:
            return webURL
        tmpHTML = tmpHTML[posText+lenText:]
        tmpLink = tmpHTML[:tmpHTML.find('.')]

        # Fill out as much of the URL as possible
        customHTML = common.linkWebPage('dummy', 'spitzer')
        customHTML = customHTML.replace('TITLE', urllib.quote(pageTitle))
        customHTML = customHTML.replace('VIDEOCODE', tmpLink)

        # Get Thumbnail image
        findText = u"image="
        lenText = len(findText)
        posText = tmpHTML.find(findText)
        if posText == -1:
            self.persistence['spitzerThumbnailLink'] = False
            return customHTML.replace('IMAGE', u'')
        tmpHTML = tmpHTML[posText+lenText:]
        tmpImage = tmpHTML[:tmpHTML.find('"')]
        self.persistence['spitzerThumbnailLink'] = u'http://www.spitzer.caltech.edu%s' % tmpImage

        return customHTML.replace('IMAGE', tmpImage)
    # end spitzerLinkGeneration()

    def spitzerThumbnailLink(self, context, *args):
        '''Verify that the thumbnail actually exists. If it does not then use the site image.
        Call example: 'mnvXpath:spitzerThumbnailLink('dummy')'
        return the thumbnail url
        '''
        if not self.persistence['spitzerThumbnailLink']:
            return u''
        else:
            return self.persistence['spitzerThumbnailLink']
    # end spitzerThumbnailLink()

    def spitzerCheckIfDBItem(self, context, *arg):
        '''Use a unique key value pairing to find out if the 'internetcontentarticles' table already
        has a matching item. This is done to save accessing the Internet when not required.
        Call example: 'mnvXpath:spitzerCheckIfDBItem(title, author, description)'
        return True if a match was found
        return False if a match was not found
        '''
        return common.checkIfDBItem('dummy', {'feedtitle': 'Space', 'title': arg[0], 'author': arg[1], 'description': arg[2]})
    # end spitzerCheckIfDBItem()

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
