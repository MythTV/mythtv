#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tvdbXslt - XPath and XSLT functions for the tvdb.py XML output
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions
#           for the conversion of data to the MythTV Universal Metadata format.
#           See this link for the specifications:
#           http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tvdbXslt - XPath and XSLT functions for the Tribute.ca grabber"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MythTV Universal Metadata format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
'''

__version__="v0.1.1"
# 0.1.0 Initial development
# 0.1.1 Converted categories, genre, ... etc text characters to be XML compliant


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
#if version < '2.7.2':
#    sys.stderr.write(u'''
#! Error - The installed version of the "lxml" python library "libxml" version is too old.
#          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
#''' % version)
#    sys.exit(1)


class xpathFunctions(object):
    """Functions specific extending XPath
    """
    def __init__(self):
        self.filters = {
            'fanart': [u'//Banner[BannerType/text()="%(type)s" and Language/text()="%(language)s"]', u'//Banner[BannerType/text()="%(type)s" and Language/text()="en"]', u'//Banner[BannerType/text()="%(type)s"]'],
            'poster': [u'//Banner[BannerType/text()="season" and Language/text()="%(language)s" and Season/text()="%(season)s" and BannerType2/text()="season"]', u'//Banner[BannerType/text()="%(type)s" and Language/text()="%(language)s"]', u'//Banner[BannerType/text()="season" and Language/text()="en" and Season/text()="%(season)s" and BannerType2/text()="season"]', u'//Banner[BannerType/text()="season" and Season/text()="%(season)s" and BannerType2/text()="season"]', u'//Banner[BannerType/text()="%(type)s" and Language/text()="en"]', u'//Banner[BannerType/text()="%(type)s"]'],
            'banner': ['//Banner[BannerType/text()="season" and Language/text()="%(language)s" and Season/text()="%(season)s" and BannerType2/text()="seasonwide"]', u'//Banner[BannerType/text()="series" and Language/text()="%(language)s" and BannerType2/text()="graphical"]', '//Banner[BannerType/text()="season" and Language/text()="en" and Season/text()="%(season)s" and BannerType2/text()="seasonwide"]', '//Banner[BannerType/text()="season" and Season/text()="%(season)s" and BannerType2/text()="seasonwide"]', u'//Banner[BannerType/text()="series" and Language/text()="en" and BannerType2/text()="graphical"]', '//Banner[BannerType/text()="series" and BannerType2/text()="graphical"]'],
            }
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def buildFuncDict(self):
        """ Build a dictionary of the XPath extention function for the XSLT stylesheets
        Returns nothing
        """
        self.FuncDict = {
            'lastUpdated': self.lastUpdated,
            'htmlToString': self.htmlToString,
            'stringToList': self.stringToList,
            'imageElements': self.imageElements,
            }
        return
    # end buildFuncDict()

    def lastUpdated(self, context, epoch):
        '''Convert the epoch lastupdate time to the
        Example call: tvdbXpath:ilastUpdated(string(lastupdated))
        return a date string in the standard format
        '''
        return time.strftime(u'%a, %d %b %Y %H:%M:%S GMT', time.localtime(long(epoch)))
    # end lastUpdated()

    def htmlToString(self, context, html):
        ''' Remove HTML tags and LFs from a string
        return the string without HTML tags or LFs
        '''
        if not len(html):
            return u""
        return self.massageText(html).strip().replace(u'', u"&apos;").replace(u'', u"&apos;")
    # end htmlToString()

    def stringToList(self, context, arg):
        ''' Split a string into substrings and return each as an element.
        Example: tvdbXpath:stringToCategories(string(./Genre), '|')
        return a list of elements with each substrings text value
        '''
        if not arg:
            return []
        elementList = []
        tmpString = arg
        tmpList1 = tmpString.split('|')
        tmpList2 = tmpString.split(',')
        if len(tmpList1) > len(tmpList2):
            tmpList = tmpList1
        else:
            tmpList = tmpList2
        for value in tmpList:
            if value == u'':
                continue
            value = value.replace(u'\n', u' ').strip()
            if value != u'':
                elementList.append(etree.XML(u'<listItem>%s</listItem>' % self.massageText(value)))
        return elementList
    # end stringToList()

    def imageElements(self, context, *args):
        ''' Take the images and make image elements using the language of preference as the priority.
        Example: tvdbXpath:imageElements(/Banners, 'fanart', /requestDetails)
        return a list of image elements
        '''
        if len(args) != 3:
            return []
        if not len(args[0]):
            return []
        elementList = []

        parmDict = {
            'type': args[1],
            'language': args[2][0].attrib['lang'],
            'season': args[2][0].attrib['season'],
            'episode': args[2][0].attrib['episode'],
            }
        filters = []
        for index in range(len(self.filters[args[1]])):
           filters.append(etree.XPath(self.filters[args[1]][index] % parmDict))

        # Get the preferred images
        for xpathFilter in filters:
            for image in xpathFilter(args[0][0]):
                if image.find('BannerPath') == None:
                    continue
                tmpElement = etree.XML(u'<image></image>')
                if args[1] == 'poster':
                    tmpElement.attrib['type'] = 'coverart'
                else:
                    tmpElement.attrib['type'] = args[1]
                tmpElement.attrib['url'] = u'http://www.thetvdb.com/banners/%s' % image.find('BannerPath').text
                tmpElement.attrib['thumb'] = u'http://www.thetvdb.com/banners/_cache/%s' % image.find('BannerPath').text
                tmpImageSize = image.find('BannerType2').text
                index = tmpImageSize.find('x')
                if index != -1:
                    tmpElement.attrib['width'] = tmpImageSize[:index]
                    tmpElement.attrib['height'] = tmpImageSize[index+1:]
                elementList.append(tmpElement)
            if len(elementList):
                break
        return elementList
    # end imageElements()

    def textUtf8(self, text):
        if text == None:
            return text
        try:
            return unicode(text, 'utf8')
        except UnicodeDecodeError:
            return u''
        except (UnicodeEncodeError, TypeError):
            return text
    # end textUtf8()

    def ampReplace(self, text):
        '''Replace all "&" characters with "&amp;"
        '''
        text = self.textUtf8(text)
        return text.replace(u'&amp;',u'~~~~~').replace(u'&',u'&amp;').replace(u'~~~~~', u'&amp;')
    # end ampReplace()

    def massageText(self, text):
        '''Removes HTML markup from a text string.
        @param text The HTML source.
        @return The plain text.  If the HTML source contains non-ASCII
        entities or character references, this is a Unicode string.
        '''
        def fixup(m):
            text = m.group(0)
            if text[:1] == "<":
                return "" # ignore tags
            if text[:2] == "&#":
                try:
                    if text[:3] == "&#x":
                        return unichr(int(text[3:-1], 16))
                    else:
                        return unichr(int(text[2:-1]))
                except ValueError:
                    pass
            elif text[:1] == "&":
                import htmlentitydefs
                entity = htmlentitydefs.entitydefs.get(text[1:-1])
                if entity:
                    if entity[:2] == "&#":
                        try:
                            return unichr(int(entity[2:-1]))
                        except ValueError:
                            pass
                    else:
                        return unicode(entity, "iso-8859-1")
            return text # leave as is
        return self.ampReplace(re.sub(u"(?s)<[^>]*>|&#?\w+;", fixup, self.textUtf8(text))).replace(u'\n',u' ')
    # end massageText()

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
