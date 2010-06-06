#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: youtubeXSL_api - XPath and XSLT functions for the mashup grabbers
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
__title__ ="youtubeXSL_api - XPath and XSLT functions for the mashup grabbers"
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
        self.functList = ['youtubeTrailerFilter', 'youtubePaging', ]
        self.tailerNum_Patterns = [
            # "trailer 7"
            re.compile(u'''^.+?trailer\\ (?P<trailerNum>[0-9]+).*$''', re.UNICODE),
            # "trailer #7"
            re.compile(u'''^.+?trailer\\ \\#(?P<trailerNum>[0-9]+).*$''', re.UNICODE),
            ]
    # end __init__()

######################################################################################################
#
# Start of XPath extension functions
#
######################################################################################################

    def youtubeTrailerFilter(self, context, *args):
        '''Generate a list of entry elements that are relevant to the requested search term. Basically
        remove duplicate and non-relevant search results and order them to provide the best results
        for the user.
        Also set the paging variables.
        Call example: 'mnvXpath:youtubeTrailerFilter(//atm:entry)'
        return the list of relevant "entry" elements
        '''
        searchTerm = common.removePunc('dummy', common.searchterm.lower())
        titleFilter = etree.XPath('.//atm:title', namespaces=common.namespaces)

        # Remove any leading word "The" from the search term
        if searchTerm.startswith(u'the '):
            searchTerm = searchTerm[4:].strip()

        titleDict = {}
        for entry in args[0]:
            titleDict[titleFilter(entry)[0].text] = entry

        # Tag so that there is an order plus duplicates can be easily spotted
        filteredDict = {}
        for key in titleDict.keys():
            title = common.removePunc('dummy', key.lower())
            if title.startswith(u'the '):
                title = title[4:].strip()
            if searchTerm.find('new ') == -1:
                title = title.replace(u'new ', u'')
            if searchTerm.find('official ') == -1:
                title = title.replace(u'official ', u'')
            if title.find(searchTerm) != -1:
                addOns = u''
                HD = False
                if searchTerm.find('game ') == -1:
                    if title.find('game') != -1:
                        addOns+=u'ZZ-Game'
                if title.find('hd') != -1 or title.find('1080p') != -1 or title.find('720p') != -1:
                    HD = True
                if title.startswith(searchTerm):
                    addOns+=u'1-'
                for regexPattern in self.tailerNum_Patterns:
                    match = regexPattern.match(title)
                    if not match:
                        continue
                    trailerNum = match.groups()
                    if int(trailerNum[0]) < 20:
                        addOns+=u'Trailer #%s' % trailerNum[0]
                        title = title.replace((u'trailer %s' % trailerNum[0]), u'')
                    else:
                        addOns+=u'Trailer #1'
                    break
                else:
                    if title.find('trailer') != -1:
                        addOns+=u'Trailer #1'
                if HD and not addOns.startswith(u'ZZ-Game'):
                    if addOns:
                        addOns=u'HD-'+addOns
                    else:
                        addOns=u'YHD'
                for text in [u'hd', u'trailer', u'game', u'1080p', u'720p']:
                    title = title.replace(text, u'').replace(u'  ', u' ').strip()
                filteredDict[(u'%s %s' % (addOns, title)).strip()] = titleDict[key]

        # Get rid of obvious duplicates
        filtered2Dict = {}
        sortedList = sorted(filteredDict.keys())
        for index in range(len(sortedList)):
            if index == 0:
                filtered2Dict[sortedList[index]] = deepcopy(filteredDict[sortedList[index]])
                continue
            if sortedList[index] != sortedList[index-1]:
                filtered2Dict[sortedList[index]] = deepcopy(filteredDict[sortedList[index]])

        # Copy the remaining elements to a list
        finalElements = []
        sortedList = sorted(filtered2Dict.keys())
        for index in range(len(sortedList)):
            titleFilter(filtered2Dict[sortedList[index]])[0].text = u'%02d. %s' % (index+1, titleFilter(filtered2Dict[sortedList[index]])[0].text)
            finalElements.append(filtered2Dict[sortedList[index]])

        # Set the paging values
        common.numresults = str(len(finalElements))
        common.returned = common.numresults
        common.startindex = common.numresults

        return finalElements
    # end youtubeTrailerFilter()

    def youtubePaging(self, context, args):
        '''Generate a page value specific to the mashup search for YouTube searches
        Call example: 'mnvXpath:youtubePaging('dummy')'
        The page value is some times a page # and sometimes an item position number
        return the page value that will be used in the search as a string
        '''
        return str((int(common.pagenumber) -1) * common.page_limit + 1)
    # end youtubeTrailerFilter()

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
