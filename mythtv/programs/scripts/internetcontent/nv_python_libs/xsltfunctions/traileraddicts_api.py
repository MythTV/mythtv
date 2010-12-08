#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: traileraddicts_api - XPath and XSLT functions for the TrailerAddicts.com grabber
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
__title__ ="traileraddicts_api - XPath and XSLT functions for the TrailerAddicts.com grabber"
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
        self.functList = ['traileraddictsLinkGenerationMovie', 'traileraddictsLinkGenerationClip', 'traileraddictsCheckIfDBItem']
        self.TextTail = etree.XPath("string()")
        self.persistence = {}
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def traileraddictsLinkGenerationMovie(self, context, *args):
        '''Generate a link for the TrailerAddicts.com site.
        Call example: 'mnvXpath:traileraddictsLinkGenerationMovie(position(), link)'
        return the url link
        '''
        webURL = args[1].strip()

        # If this is for the download element then just return what was found for the "link" element
        if self.persistence.has_key('traileraddictsLinkGenerationMovie'):
            if args[0] == self.persistence['traileraddictsLinkGenerationMovie']['position']:
                return self.persistence['traileraddictsLinkGenerationMovie']['link']
        else:
            self.persistence['traileraddictsLinkGenerationMovie'] = {}
            self.persistence['traileraddictsLinkGenerationMovie']['embedRSS'] = etree.parse(u'http://www.traileraddict.com/embedrss', common.parsers['xml'].copy())
            self.persistence['traileraddictsLinkGenerationMovie']['matchlink'] = etree.XPath('//link[string()=$link]/..', namespaces=common.namespaces)
            self.persistence['traileraddictsLinkGenerationMovie']['description'] = etree.XPath('normalize-space(description)', namespaces=common.namespaces)
            self.persistence['traileraddictsLinkGenerationMovie']['embedded'] = etree.XPath('//embed/@src', namespaces=common.namespaces)

        self.persistence['traileraddictsLinkGenerationMovie']['position'] = args[0]

        matchLink = self.persistence['traileraddictsLinkGenerationMovie']['matchlink'](self.persistence['traileraddictsLinkGenerationMovie']['embedRSS'], link=webURL)[0]
        self.persistence['traileraddictsLinkGenerationMovie']['link'] = self.persistence['traileraddictsLinkGenerationMovie']['embedded'](common.getHtmlData(u'dummy',(self.persistence['traileraddictsLinkGenerationMovie']['description'](matchLink))))[0]

        return self.persistence['traileraddictsLinkGenerationMovie']['link']
    # end traileraddictsLinkGenerationMovie()


    def traileraddictsLinkGenerationClip(self, context, *args):
        '''Generate a link for the TrailerAddicts.com site.
        Call example: 'mnvXpath:traileraddictsLinkGenerationClip(position(), link)'
        return the url link
        '''
        webURL = args[1].strip()
        # If this is for the download element then just return what was found for the "link" element
        if self.persistence.has_key('traileraddictsLinkGenerationClip'):
            if args[0] == self.persistence['traileraddictsLinkGenerationClip']['position']:
                return self.persistence['traileraddictsLinkGenerationClip']['link']
        else:
            self.persistence['traileraddictsLinkGenerationClip'] = {}
            self.persistence['traileraddictsLinkGenerationClip']['embedded'] = etree.XPath('//embed[@allowfullscreen="true"]/@src', namespaces=common.namespaces)

        self.persistence['traileraddictsLinkGenerationClip']['position'] = args[0]

        tmpHTML = etree.parse(webURL, etree.HTMLParser())
        self.persistence['traileraddictsLinkGenerationClip']['link'] = self.persistence['traileraddictsLinkGenerationClip']['embedded'](tmpHTML)[0]
        return self.persistence['traileraddictsLinkGenerationClip']['link']
    # end traileraddictsLinkGenerationClip()

    def traileraddictsCheckIfDBItem(self, context, *arg):
        '''Use a unique key value pairing to find out if the 'internetcontentarticles' table already
        has a matching item. This is done to save accessing the Internet when not required.
        Call example: 'mnvXpath:traileraddictsCheckIfDBItem(.)'
        return True if a match was found
        return False if a match was not found
        '''
        return common.checkIfDBItem('dummy', {'feedtitle': 'Movie Trailers', 'title': arg[0], 'author': arg[1], 'description': arg[2]})
    # end traileraddictsCheckIfDBItem()

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
