#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: giantbomb_api.py  Simple-to-use Python interface to the GiantBomb's API (api.giantbomb.com)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and
#           access text metadata and image URLs from GiantBomb. These routines are based on the
#           GiantBomb api. Specifications for this api are published at:
#           http://api.giantbomb.com/documentation/
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="giantbomb_api - Simple-to-use Python interface to The GiantBomb's API (api.giantbomb.com)";
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
metadata and image URLs from GiantBomb. These routines are based on the GiantBomb api. Specifications
for this api are published at http://api.giantbomb.com/documentation/
'''

__version__="v0.1.0"
# 0.1.0 Initial development

import os, struct, sys, datetime, time, re
import urllib
from copy import deepcopy

from giantbomb_exceptions import (GiantBombBaseError, GiantBombHttpError, GiantBombXmlError,  GiantBombGameNotFound,)


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


class gamedbQueries():
    '''Methods that query api.giantbomb.com for metadata and outputs the results to stdout any errors are output
    to stderr.
    '''
    def __init__(self,
                apikey,
                debug = False,
                ):
        """apikey (str/unicode):
            Specify the api.giantbomb.com API key. Applications need their own key.
            See http://api.giantbomb.com to get your own API key

        debug (True/False):
             shows verbose debugging information
        """
        self.config = {}

        self.config['apikey'] = apikey
        self.config['debug'] = debug

        self.config['searchURL'] = u'http://api.giantbomb.com/search/?api_key=%s&offset=0&query=%%s&resources=game&format=xml' % self.config['apikey']
        self.config['dataURL'] = u'http://api.giantbomb.com/game/%%s/?api_key=%s&format=xml' % self.config['apikey']

        self.error_messages = {'GiantBombHttpError': u"! Error: A connection error to api.giantbomb.com was raised (%s)\n", 'GiantBombXmlError': u"! Error: Invalid XML was received from api.giantbomb.com (%s)\n", 'GiantBombBaseError': u"! Error: An error was raised (%s)\n", }

        self.baseProcessingDir = os.path.dirname( os.path.realpath( __file__ ))

        self.pubDateFormat = u'%a, %d %b %Y %H:%M:%S GMT'
        self.xmlParser = etree.XMLParser(remove_blank_text=True)

        self.supportedJobList = ["actor", "author", "producer", "executive producer", "director", "cinematographer", "composer", "editor", "casting", "voice actor", "music", "writer", "technical director", "design director", ]
        self.tagTranslations = {
            'actor': 'Actor',
            'author': 'Author',
            'producer': 'Producer',
            'executive producer': 'Executive Producer',
            'director': 'Director',
            'cinematographer': 'Cinematographer',
            'composer': 'Composer',
            'editor': 'Editor',
            'casting': 'Casting',
            'voice actor': 'Actor',
            'music': 'Composer',
            'writer': 'Author',
            'technical director': 'Director',
            'design director': 'Director',
            }
    # end __init__()


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


    def htmlToString(self, context, html):
        ''' Remove HTML tags and LFs from a string
        return the string without HTML tags or LFs
        '''
        if not len(html):
            return u""
        return self.massageText(html).strip().replace(u'\n', u' ').replace(u'', u"&apos;").replace(u'', u"&apos;")
    # end htmlToString()

    def getHtmlData(self, context, *args):
        ''' Take a HTML string and convert it to an HTML element. Then apply a filter and return
        the results.
        return filter array
        return an empty array if the filter failed to find any values.
        '''
        xpathFilter = None
        if len(args) > 1:
            xpathFilter = args[0]
            htmldata = args[1]
        else:
            htmldata = args[0]
        if not htmldata:
            return []
        htmlElement = etree.HTML(htmldata)
        if not xpathFilter:
            return htmlElement
        filteredData = htmlElement.xpath(xpathFilter)
        if len(filteredData):
            if xpathFilter.find('@') != -1:
                return filteredData[0]
            else:
                return filteredData[0].text
        return u''
    # end getHtmlData()

    def pubDate(self, context, *inputArgs):
        '''Convert a date/time string in a specified format into a pubDate. The default is the
        MNV item format
        return the formatted pubDate string
        return on error return the original date string
        '''
        args = []
        for arg in inputArgs:
            args.append(arg)
        if args[0] == u'':
            return datetime.datetime.now().strftime(self.pubDateFormat)
        index = args[0].find('+')
        if index == -1:
            index = args[0].find('-')
        if index != -1 and index > 5:
            args[0] = args[0][:index].strip()
        args[0] = args[0].replace(',', u'').replace('.', u'')
        try:
            if len(args) > 1:
                args[1] = args[1].replace(',', u'').replace('.', u'')
                if args[1].find('GMT') != -1:
                    args[1] = args[1][:args[1].find('GMT')].strip()
                    args[0] = args[0][:args[0].rfind(' ')].strip()
                try:
                    pubdate = time.strptime(args[0], args[1])
                except ValueError:
                    if args[1] == '%a %d %b %Y %H:%M:%S':
                        pubdate = time.strptime(args[0], '%a %d %B %Y %H:%M:%S')
                    elif args[1] == '%a %d %B %Y %H:%M:%S':
                        pubdate = time.strptime(args[0], '%a %d %b %Y %H:%M:%S')
                if len(args) > 2:
                    return time.strftime(args[2], pubdate)
                else:
                    return time.strftime(self.pubDateFormat, pubdate)
            else:
                return datetime.datetime.now().strftime(self.pubDateFormat)
        except Exception, err:
            sys.stderr.write(u'! Error: pubDate variables(%s) error(%s)\n' % (args, err))
        return args[0]
    # end pubDate()

    def futureReleaseDate(self, context, gameElement):
        '''Convert the "expected" release date into the default MNV item format.
        return the formatted pubDate string
        return If there is not enough information to make a date then return an empty string
        '''
        try:
            if gameElement.find('expected_release_year').text != None:
                year = gameElement.find('expected_release_year').text
            else:
                year = None
            if gameElement.find('expected_release_quarter').text != None:
                quarter = gameElement.find('expected_release_quarter').text
            else:
                quarter = None
            if gameElement.find('expected_release_month').text != None:
                month = gameElement.find('expected_release_month').text
            else:
                month = None
        except:
            return u''
        if not year:
            return u''
        if month and not quarter:
            pubdate = time.strptime((u'%s-%s-01' % (year, month)), '%Y-%m-%d')
        elif not month and quarter:
            month = str((int(quarter)*3))
            pubdate = time.strptime((u'%s-%s-01' % (year, month)), '%Y-%m-%d')
        else:
            pubdate = time.strptime((u'%s-12-01' % (year, )), '%Y-%m-%d')

        return time.strftime('%Y-%m-%d', pubdate)
    # end futureReleaseDate()

    def findImages(self, context, *args):
        '''Parse the "image" and "description" elements for images and put in a persistant array
        return True when there are images available
        return False if there are no images
        '''
        def makeImageElement(typeImage, url, thumb):
            ''' Create a single Image element
            return the image element
            '''
            imageElement = etree.XML(u"<image></image>")
            imageElement.attrib['type'] = typeImage
            imageElement.attrib['url'] = url
            imageElement.attrib['thumb'] = thumb
            return imageElement
        # end makeImageElement()

        superImageFilter = etree.XPath('.//super_url/text()')
        self.imageElements = []
        for imageElement in args[0]:
            imageList = superImageFilter(imageElement)
            if len(imageList):
                for image in imageList:
                    self.imageElements.append(makeImageElement('coverart', image, image.replace(u'super', u'thumb')))
        htmlElement = self.getHtmlData('dummy', etree.tostring(args[1][0], method="text", encoding=unicode).strip())
        if len(htmlElement):
            for image in htmlElement[0].xpath('.//a/img/@src'):
                if image.find('screen') == -1:
                    continue
                if image.find('thumb') == -1:
                    continue
                self.imageElements.append(makeImageElement('screenshot', image.replace(u'thumb', u'super'), image))

        if len(args) > 2:
            for imageElement in args[2]:
                imageList = superImageFilter(imageElement)
                if len(imageList):
                    for image in imageList:
                        self.imageElements.append(makeImageElement('screenshot', image, image.replace(u'super', u'thumb')))

        if not len(self.imageElements):
            return False
        return True
    # end findImages()

    def getImages(self, context, arg):
        '''Return an array of image elements that was created be a previous "findImages" function call
        return the array of image elements
        '''
        return self.imageElements
    # end getImages()

    def supportedJobs(self, context, *inputArgs):
        '''Validate that the job category is supported by the
        Universal Metadata Format item format
        return True is supported
        return False if not supported
        '''
        if type([]) == type(inputArgs[0]):
            tmpCopy = inputArgs[0]
        else:
            tmpCopy = [inputArgs[0]]
        for job in tmpCopy:
            if job.lower() in self.supportedJobList:
                return True
        return False
    # end supportedJobs()

    def translateName(self, context, *inputArgs):
        '''Translate a tag name into the Universal Metadata Format item equivalent
        return the translated tag equivalent
        return the input name as the name does not need translating and is already been validated
        '''
        name = inputArgs[0]
        name = name.lower()
        if name in self.tagTranslations.keys():
            return self.tagTranslations[name]
        return name
    # end translateName()

    def buildFuncDict(self):
        """ Build a dictionary of the XPath extention function for the XSLT stylesheets
        Returns nothing
        """
        self.FuncDict = {
            'htmlToString': self.htmlToString,
            'getHtmlData': self.getHtmlData,
            'pubDate': self.pubDate,
            'futureReleaseDate': self.futureReleaseDate,
            'findImages': self.findImages,
            'getImages': self.getImages,
            'supportedJobs': self.supportedJobs,
            'translateName': self.translateName,
            }
        return
    # end buildFuncDict()

    def gameSearch(self, gameTitle):
        """Display a Game query in XML format:
        http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
        Returns nothing
        """
        url = self.config['searchURL'] % urllib.quote_plus(gameTitle.encode("utf-8"))
        if self.config['debug']:
            print "URL(%s)" % url
            print

        try:
            queryResult = etree.parse(url, parser=self.xmlParser)
        except Exception, errmsg:
            sys.stderr.write(u"! Error: Invalid XML was received from www.giantbomb.com (%s)\n" % errmsg)
            sys.exit(1)

        queryXslt = etree.XSLT(etree.parse(u'%s/XSLT/giantbombQuery.xsl' % self.baseProcessingDir))
        gamebombXpath = etree.FunctionNamespace('http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format')
        gamebombXpath.prefix = 'gamebombXpath'
        self.buildFuncDict()
        for key in self.FuncDict.keys():
            gamebombXpath[key] = self.FuncDict[key]

        items = queryXslt(queryResult)

        if items.getroot() != None:
            if len(items.xpath('//item')):
                sys.stdout.write(etree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
        sys.exit(0)
    # end gameSearch()

    def gameData(self, gameId):
        """Display a Game details in XML format:
        http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
        Returns nothing
        """
        url = self.config['dataURL'] % gameId
        if self.config['debug']:
            print "URL(%s)" % url
            print

        try:
            videoResult = etree.parse(url, parser=self.xmlParser)
        except Exception, errmsg:
            sys.stderr.write(u"! Error: Invalid XML was received from www.giantbomb.com (%s)\n" % errmsg)
            sys.exit(1)

        gameXslt = etree.XSLT(etree.parse(u'%s/XSLT/giantbombGame.xsl' % self.baseProcessingDir))
        gamebombXpath = etree.FunctionNamespace('http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format')
        gamebombXpath.prefix = 'gamebombXpath'
        self.buildFuncDict()
        for key in self.FuncDict.keys():
            gamebombXpath[key] = self.FuncDict[key]
        items = gameXslt(videoResult)

        if items.getroot() != None:
            if len(items.xpath('//item')):
                sys.stdout.write(etree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
        sys.exit(0)
    # end gameData()

# end Class gamedbQueries()

def main():
    """Simple example of using giantbomb_api - it just
    searches for any Game with the word "Grand" in its title and returns a list of matches
    in Universal XML format. Also gets game details using a GameBomb#.
    """
    # api.giantbomb.com api key provided for MythTV
    apikey = "b5883a902a8ed88b15ce21d07787c94fd6ad9f33"
    gamebomb = gamedbQueries(api_key)
    # Output a dictionary of matching movie titles
    gamebomb.gameSearch(u'Grand')
    print
    # Output a dictionary of matching movie details for GiantBomb number '19995'
    gamebomb.gameData(u'19995')
# end main()

if __name__ == '__main__':
    main()
