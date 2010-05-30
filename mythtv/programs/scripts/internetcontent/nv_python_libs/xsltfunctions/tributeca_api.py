#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tributeca_api - XPath and XSLT functions for the Tribute.ca grabber
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
__title__ ="tributeca_api - XPath and XSLT functions for the Tribute.ca grabber"
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
        self.functList = ['tributecaLinkGeneration', 'tributecaThumbnailLink', 'tributecaTopTenTitle', 'tributecaIsCustomHTML', ]
        self.TextTail = etree.XPath("string()")
        self.persistence = {}
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def tributecaLinkGeneration(self, context, *args):
        '''Generate a link for the Tribute.ca site. Sigificant massaging of the title is required.
        Call example: 'mnvXpath:tributecaLinkGeneration(string(.), string(preceding-sibling::a[1]))'
        return the url link
        '''
        downloadURL = u'http://www.tribute.ca/streamingflash/%s.flv'
        position = int(args[0])-1
        webURL = u'http://www.tribute.ca%s' % args[1][position].attrib['href'].strip()
        # If this is for the download then just return what was found for the "link" element
        if self.persistence.has_key('tributecaLinkGeneration'):
            if self.persistence['tributecaLinkGeneration'] != None:
                returnValue = self.persistence['tributecaLinkGeneration']
                self.persistence['tributecaLinkGeneration'] = None
                if returnValue != webURL:
                    return downloadURL % returnValue
                else:
                    return webURL

        currentTitle = self.TextTail(args[1][position]).strip()
        if position == 0:
            previousTitle = u''
        else:
           previousTitle = self.TextTail(args[1][position-1]).strip()

        # Rule: "IMAX: Hubble 3D": http://www.tribute.ca/streamingflash/hubble3d.flv
        titleArray = [currentTitle, previousTitle]
        if titleArray[0].startswith(u'IMAX:'):
            titleArray[0] = titleArray[0].replace(u'IMAX:', u'').strip()
        else:
            # Rule: "How to Train Your Dragon: An IMAX 3D Experience" did not even have a trailer
            #       on the Web page but stip off anything after the ":"
            for counter in range(len(titleArray)):
                index = titleArray[counter].find(": ")
                if index != -1:
                    titleArray[counter] = titleArray[counter][:index].strip()
                index = titleArray[counter].find(" (")
                if index != -1:
                    titleArray[counter] = titleArray[counter][:index].strip()
                if titleArray[0].startswith(titleArray[1]) and titleArray[1]:
                    index = titleArray[counter].find("3D")
                    if index != -1:
                        titleArray[counter] = titleArray[counter][:index].strip()

        # If the previous title starts with the same title as the current then this is trailer #2
        trailer2 = u''
        if titleArray[0].startswith(titleArray[1]) and titleArray[1]:
            trailer2 = u'tr2'
        if currentTitle.find(': An IMAX') != -1:
            trailer2 = u'tr2'

        titleArray[0] = titleArray[0].replace(u'&', u'and')
        self.persistence['tributecaThumbnailLink'] = urllib.quote_plus(titleArray[0].lower().replace(u' ', u'_').replace(u"'", u'').replace(u'-', u'_').replace(u'?', u'').replace(u'.', u'').encode("utf-8"))
        titleArray[0] = urllib.quote_plus(re.sub('[%s]' % re.escape(string.punctuation), '', titleArray[0].lower().replace(u' ', u'').encode("utf-8")))

        # Verify that the FLV file url really exits. If it does not then use the Web page link.
        videocode = u'%s%s' % (titleArray[0], trailer2)
        flvURL = downloadURL % videocode

        resultCheckUrl = common.checkURL(flvURL)
        if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != u'video/x-flv':
            if trailer2 != u'':
                videocode = titleArray[0]
                flvURL = downloadURL % titleArray[0]
                resultCheckUrl = common.checkURL(flvURL) # Drop the 'tr2' this time
                if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != u'video/x-flv':
                    flvURL = webURL
            else:
                videocode = titleArray[0]+u'tr2'
                flvURL = downloadURL % videocode
                resultCheckUrl = common.checkURL(flvURL) # Add the 'tr2' this time
                if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != u'video/x-flv':
                    if currentTitle.find(': An IMAX') == -1 and currentTitle.find(': ') != -1:
                        titleArray[0] = currentTitle.replace(u'&', u'and')
                        titleArray[0] = urllib.quote_plus(re.sub('[%s]' % re.escape(string.punctuation), '', titleArray[0].lower().replace(u' ', u'').encode("utf-8")))
                        videocode = titleArray[0]
                        flvURL = downloadURL % videocode
                        resultCheckUrl = common.checkURL(flvURL) # Add the 'tr2' this time
                        if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != u'video/x-flv':
                            flvURL = webURL
                    else:
                        flvURL = webURL

        if flvURL != webURL:
            self.persistence['tributecaLinkGeneration'] = videocode
            return common.linkWebPage(u'dummycontext', 'tributeca')+videocode
        else:
            self.persistence['tributecaLinkGeneration'] = flvURL
            return flvURL
    # end linkGeneration()

    def tributecaThumbnailLink(self, context, *args):
        '''Verify that the thumbnail actually exists. If it does not then use the site image.
        Call example: 'mnvXpath:tributecaThumbnailLink(string(.//img/@src))'
        return the thumbnail url
        '''
        siteImage = u'http://www.tribute.ca/images/tribute_title.gif'
        if not len(args[0]) or not self.persistence['tributecaThumbnailLink']:
            return siteImage

        if args[0].startswith(u'http:'):
            url = args[0].strip()
        else:
            url = u'http://www.tribute.ca/tribute_objects/images/movies/%s%s' % (self.persistence['tributecaThumbnailLink'], u'/poster.jpg')
        resultCheckUrl = common.checkURL(url)
        if not resultCheckUrl[0] or resultCheckUrl[1]['Content-Type'] != u'image/jpeg':
            return siteImage

        return url
    # end tributecaThumbnailLink()

    def tributecaTopTenTitle(self, context, *args):
        '''Take a top ten title and add a leading '0' if less than 10 as it forces correct sort order
        Call example: 'mnvXpath:tributecaTopTenTitle(string(..))'
        return a replacement title
        '''
        if not len(args[0]):
            return args[0]

        index = args[0].find('.')
        if index == 1:
            return u'0'+args[0]
        else:
            return args[0]
    # end tributecaTopTenTitle()

    def tributecaIsCustomHTML(self, context, *args):
        '''Check if the link is for a custom HTML
        Example call: mnvXpath:isCustomHTML(('dummy'))
        return True if the link does not starts with "http://"
        return False if the link starts with "http://"
        '''
        if self.persistence['tributecaLinkGeneration'] == None:
            return False

        if self.persistence['tributecaLinkGeneration'].startswith(u'http://'):
            return False
        else:
            return True
    # end isCustomHTML()

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

class xsltExtExample(etree.XSLTExtension):
    '''Example of an XSLT extension. This code must be changed to do anything useful!!!
    return nothing
    '''
    def execute(self, context, self_node, input_node, output_parent):
        copyItem = deepcopy(input_node)
        min_sec = copyItem.xpath('duration')[0].text.split(':')
        seconds = 0
        for count in range(len(min_sec)):
            seconds+=int(min_sec[count])*(60*(len(min_sec)-count-1))
        output_parent.text = u'%s' % seconds

######################################################################################################
#
# End of XSLT extension functions
#
######################################################################################################
