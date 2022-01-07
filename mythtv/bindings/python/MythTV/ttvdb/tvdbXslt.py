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
__title__ ="tvdbXslt - XPath and XSLT functions for the ttvdb.com grabber"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions
for the conversion of data to the MythTV Universal Metadata format.
See this link for the specifications:
http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
'''

__version__="v0.1.2"
# 0.1.0 Initial development
# 0.1.1 Converted categories, genre, ... etc text characters to be XML compliant
# 0.1.2 Performance improvements by removing complex data searches from the XLST stylesheet


# Specify the class names that have XPath extention functions
__xpathClassList__ = ['xpathFunctions', ]

# Specify the XSLT extention class names. Each class is a stand lone extention function
#__xsltExtentionList__ = ['xsltExtExample', ]
__xsltExtentionList__ = []

import os, sys, re, time, datetime, shutil, urllib, string
from copy import deepcopy

baseXsltDir = '%s/XSLT/' % os.path.dirname(os.path.realpath(__file__))

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
        if isinstance(obj, str):
            obj.encode(self.encoding)
        try:
            self.out.buffer.write(obj)
        except IOError:
            pass

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)

import io
stdio_type = io.TextIOWrapper

if isinstance(sys.stdout, stdio_type):
    sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
    sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')

try:
    try:
        from StringIO import StringIO
    except ImportError:
        from io import StringIO
    from lxml import etree
except Exception as e:
    sys.stderr.write('\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
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
    sys.stderr.write('''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


class xpathFunctions(object):
    """Functions specific extending XPath
    """
    def __init__(self):
        self.filters = {
            'fanart': ['//_banners/%(type)s/raw/item'],
            'poster': ['//_banners/season/raw/item[subKey/text()="%(season)s"]',
                       '//_banners/%(type)s/raw/item'],
            'banner': ['//_banners/seasonwide/raw/item[subKey/text()="%(season)s"]',
                       '//_banners/series/raw/item[subKey/text()="graphical"]'],
            }
        self.dataFilters = {
            'subtitle': ['//Data/Episode[SeasonNumber/text()="%(season)s" and EpisodeNumber/text()="%(episode)s"]/EpisodeName/text()',
                         '//data/n%(season)s/n%(episode)s/episodeName/text()'],
            'description': ['//Data/Episode[SeasonNumber/text()="%(season)s" and EpisodeNumber/text()="%(episode)s"]/Overview/text()',
                            '//data/n%(season)s/n%(episode)s/overview/text()'],
            'IMDB': ['//Data/Episode[SeasonNumber/text()="%(season)s" and EpisodeNumber/text()="%(episode)s"]/IMDB_ID/text()',
                     '//data/n%(season)s/n%(episode)s/imdbId/text()'],
            'allEpisodes': ['//Data/Episode[SeasonNumber/text()="%(season)s" and EpisodeNumber/text()="%(episode)s"]',
                            '//data/n%(season)s/n%(episode)s'],
            }
        self.persistentResult = ''
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
            'replace': self.replace,
            'htmlToString': self.htmlToString,
            'stringToList': self.stringToList,
            'imageElements': self.imageElements,
            'getValue': self.getValue,
            'getResult': self.getResult,
            }
        return
    # end buildFuncDict()

    def lastUpdated(self, context, epoch):
        '''Convert the epoch lastupdate time to the
        Example call: tvdbXpath:ilastUpdated(string(lastupdated))
        return a date string in the standard format
        '''
        return time.strftime('%a, %d %b %Y %H:%M:%S GMT', time.localtime(int(epoch)))
    # end lastUpdated()

    def htmlToString(self, context, html):
        ''' Remove HTML tags and LFs from a string
        return the string without HTML tags or LFs
        '''
        if not len(html):
            return ""
        return self.massageText(html).strip().replace('', "&apos;").replace('', "&apos;")
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
            if value == '':
                continue
            value = value.replace('\n', ' ').strip()
            if value != '':
                elementList.append(etree.XML('<listItem>%s</listItem>' % self.massageText(value)))
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
        # print("image filter on %s" % args[1])
        for filter in self.filters[args[1]]:
           filters.append(etree.XPath(filter % parmDict))

        # Get the preferred images
        for xpathFilter in filters:
            # print("xpf %r %r" % (args[0][0], xpathFilter))
            for image in xpathFilter(args[0][0]):
                # print("im %r" % image)
                # print(etree.tostring(image, method="xml", xml_declaration=False, pretty_print=True, ))
                if image.find('fileName') is None:
                    continue
                # print("im2 %r" % image)
                tmpElement = etree.XML('<image></image>')
                if args[1] == 'poster':
                    tmpElement.attrib['type'] = 'coverart'
                else:
                    tmpElement.attrib['type'] = args[1]
                tmpElement.attrib['url'] = 'http://www.thetvdb.com/banners/%s' % image.find('fileName').text
                tmpElement.attrib['thumb'] = 'http://www.thetvdb.com/banners/%s' % image.find('thumbnail').text
                tmpElement.attrib['rating'] = image.find('ratingsInfo').find('average').text
                tmpImageSize = image.find('resolution').text
                if tmpImageSize:
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
        if text is None:
            return text
        try:
            return str(text, 'utf8')
        except UnicodeDecodeError:
            return ''
        except (UnicodeEncodeError, TypeError):
            return text
    # end textUtf8()

    def replace(self, context, text, search_text, replace_text):
        '''Replace search with replace
        '''
        text = self.textUtf8(text)
        return text.replace(self.textUtf8(search_text), self.textUtf8(replace_text))
    # end ampReplace()

    def ampReplace(self, text):
        '''Replace all "&" characters with "&amp;"
        '''
        text = self.textUtf8(text)
        return text.replace('&amp;','~~~~~').replace('&','&amp;').replace('~~~~~', '&amp;')
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
                        return chr(int(text[3:-1], 16))
                    else:
                        return chr(int(text[2:-1]))
                except ValueError:
                    pass
            elif text[:1] == "&":
                import htmlentitydefs
                entity = htmlentitydefs.entitydefs.get(text[1:-1])
                if entity:
                    if entity[:2] == "&#":
                        try:
                            return chr(int(entity[2:-1]))
                        except ValueError:
                            pass
                    else:
                        return str(entity, "iso-8859-1")
            return text # leave as is
        return self.ampReplace(re.sub(r"(?s)<[^>]*>|&#?\w+;", fixup, self.textUtf8(text))).replace('\n',' ')
    # end massageText()

    def getValue(self, context, *args):
        ''' Use xpath filters to perform complex element tree data searching. This is significantly more
        efficent than using the same search statements in a xslt template. Also save the results so that the
        data can be used both in an if statement then actually use the data without performing an identical
        element tree data search.
        return the results of the data search
        '''
        if len(args) < 4:
            allValues = False
        else:
            allValues = True
        parmDict = {
            'season': args[0][0].attrib['season'],
            'episode': args[0][0].attrib['episode'],
            }

        # print("filter on list %r" % args[2])
        filters = self.dataFilters[args[2]]
        if isinstance(filters, list):
            for filter in filters:
                xpathFilter = etree.XPath(filter % parmDict)
                results = xpathFilter(args[1][0])
                if len(results):
                    break
        else:
            xpathFilter = etree.XPath(filters % parmDict)
            results = xpathFilter(args[1][0])

        # Sometimes all the results are required
        if allValues == True:
            self.persistentResult = results
            return self.persistentResult

        if len(results):
            self.persistentResult = results[0] # Return only the first result
        else:
            self.persistentResult = ''
        return self.persistentResult
    # end getValue()

    def getResult(self, context):
        ''' Return the last saved result
        '''
        return self.persistentResult
    # end getResult()

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
