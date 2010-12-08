#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: hubbleCast_api - XPath and XSLT functions for the HubbleCast RSS/HTML items
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
__title__ ="hubbleCast_api - XPath and XSLT functions for the HubbleCast RSS/HTML"
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
        self.functList = ['hubbleCastLinkGeneration', ]
        self.namespaces = {
            'atom': "http://www.w3.org/2005/Atom",
            'atom10': "http://www.w3.org/2005/Atom",
            'media': "http://search.yahoo.com/mrss/",
            'itunes':"http://www.itunes.com/dtds/podcast-1.0.dtd",
            'xhtml': "http://www.w3.org/1999/xhtml",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'feedburner': "http://rssnamespace.org/feedburner/ext/1.0",
            'amp': "http://www.adobe.com/amp/1.0",
            }
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def hubbleCastLinkGeneration(self, context, *arg):
        '''Generate a link for the video.
        Call example: 'mnvXpath:hubbleCastLinkGeneration(string(guid))'
        return the url link
        '''
        guidURL = arg[0]
        index = guidURL.find('.m4v')
        if index != -1:
            index2 = guidURL.rfind('/')
            return u'%s%s' % (common.linkWebPage('dummy', 'hubble'), guidURL[index2+1:index])
        elif guidURL[len(guidURL)-1] == '/':
            index2 = guidURL[:-1].rfind('/')
            return u'%s%s' % (common.linkWebPage('dummy', 'hubble'), guidURL[index2+1:-1])
        else:
            return guidURL
    # end hubbleCastLinkGeneration()

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
