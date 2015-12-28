#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: common_api.py - Common class libraries for all MythNetvision Mashup processing
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script contains a number of common functions used for processing MythNetvision
#           Grabbers.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="common_api - Common class libraries for all MythNetvision Mashup processing"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions for the processing of
MythNetvision Grabber scripts that run as a Web application and global functions used by many
MNV grabbers.
'''

__version__="v0.2.3"
# 0.0.1 Initial development
# 0.1.0 Alpha release
# 0.1.1 Added the ability to have a mashup name independant of the mashup title
#       Added passing on the url the emml hostname and port so a mashup can call other emml mashups
# 0.1.2 Modifications to support launching single treeview requests for better integration with MNV
#       subscription logic.
#       With the change to allow MNV launching individual tree views the auto shutdown feature had to be
#       disabled. Unless a safe work around can be found the feature may need to be removed entierly.
# 0.1.3 Modifications to support calling grabbers that run on a Web server
#       Added a class of global functions that could be used by all grabbers
# 0.1.4 Changed the rating item element to default to be empty rather than "0.0"
#       Changed the default logger to stderr
# 0.1.5 Added functions and data structures for common "Mashups" grabbers
#       Changed the api name from "mashups_api" to "common_api"
#       Added XSLT stylsheets as an alternate process option in the threaded URL download functions
# 0.1.6 Removed all logic associated with Web CGI calls as the MNV plugin is now on the backend
#       Made the pubDate fucntion more adaptable to various input date strings
# 0.1.7 Added a common function to get the current selected language (default is 'en' English)
# 0.1.8 Fixed a bug with two string functions
#       Added a customhtml reference for bliptv
# 0.1.9 Add a function that allows grabbers to check if an item is already in the data base. This is used
#       to make grabbers more efficient when processing sources that are largely made up of the same
#       data. This is particularly important when a grabber is forces to do additional Interent accesses
#       to aquire all the needed MNV item data.
#       Added a function that checks if there are any treeview items in the data base for a specific
#       grabber. Some Mashup grabber's search option only returns results when then there are treeview
#       items in the database.
# 0.2.0 Made the creation of custom HTML page links more flexible so code did not need to be changed
#       when new custom HTML pages were added.
# 0.2.1 Add the ability for a parameters to be passed to a XSLT style sheet
# 0.2.2 Added a common XPath extention to test if a string starts or ends with a substring
# 0.2.3 Fixed Error messages that were not unicode strings

import os, struct, sys, re, datetime, time, subprocess, string
import urllib
import logging
import telnetlib
from threading import Thread

from common_exceptions import (WebCgiUrlError, WebCgiHttpError, WebCgiRssError, WebCgiVideoNotFound, WebCgiXmlError, )

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
    sys.stderr.write(u'\n! Error - Importing the "lxml" python library failed on error(%s)\n' % e)
    sys.exit(1)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
# >>> from lxml import etree
# >>> print "lxml.etree:", etree.LXML_VERSION
# lxml.etree: (2, 1, 5, 0)
# >>> print "libxml used:", etree.LIBXML_VERSION
# libxml used: (2, 7, 5)
# >>> print "libxml compiled:", etree.LIBXML_COMPILED_VERSION
# libxml compiled: (2, 6, 32)
# >>> print "libxslt used:", etree.LIBXSLT_VERSION
# libxslt used: (1, 1, 24)
# >>> print "libxslt compiled:", etree.LIBXSLT_COMPILED_VERSION
# libxslt compiled: (1, 1, 24)

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


###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################
class Common(object):
    """A collection of common functions used by many grabbers
    """
    def __init__(self,
            logger=False,
            debug=False,
        ):
        self.logger = logger
        self.debug = debug
        self.baseProcessingDir = os.path.dirname( os.path.realpath( __file__ )).replace(u'/nv_python_libs/common', u'')
        self.namespaces = {
            'xsi': u"http://www.w3.org/2001/XMLSchema-instance",
            'media': u"http://search.yahoo.com/mrss/",
            'xhtml': u"http://www.w3.org/1999/xhtml",
            'atm': u"http://www.w3.org/2005/Atom",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
            'itunes':"http://www.itunes.com/dtds/podcast-1.0.dtd",
            }
        self.parsers = {
            'xml': etree.XMLParser(remove_blank_text=True),
            'html': etree.HTMLParser(remove_blank_text=True),
            'xhtml': etree.HTMLParser(remove_blank_text=True),
            }
        self.pubDateFormat = u'%a, %d %b %Y %H:%M:%S GMT'
        self.mnvRSS = u"""
<rss version="2.0"
    xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
    xmlns:content="http://purl.org/rss/1.0/modules/content/"
    xmlns:cnettv="http://cnettv.com/mrss/"
    xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
    xmlns:media="http://search.yahoo.com/mrss/"
    xmlns:atom="http://www.w3.org/2005/Atom"
    xmlns:amp="http://www.adobe.com/amp/1.0"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">
"""
        self.mnvItem = u'''
<item>
    <title></title>
    <author></author>
    <pubDate></pubDate>
    <description></description>
    <link></link>
    <media:group xmlns:media="http://search.yahoo.com/mrss/">
        <media:thumbnail url=''/>
        <media:content url='' length='' duration='' width='' height='' lang=''/>
    </media:group>
    <rating></rating>
</item>
'''
        # Season and Episode detection regex patterns
        self.s_e_Patterns = [
            # "Series 7 - Episode 4" or "Series 7 - Episode 4" or "Series 7: On Holiday: Episode 10"
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+).*.+?Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 5 - 1
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+)\\ \\-\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 1 - Warriors of Kudlak - Part 2
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+).*.+?Part\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 3: Programme 3
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+)\\:\\ Programme\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 3:
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # Episode 1
            re.compile(u'''^.+?Episode\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # Title: "s18 | e87"
            re.compile(u'''^.+?[Ss](?P<seasno>[0-9]+).*.+?[Ee](?P<epno>[0-9]+).*$''', re.UNICODE),
            # Description: "season 1, episode 5"
            re.compile(u'''^.+?season\ (?P<seasno>[0-9]+).*.+?episode\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Thumbnail: "http://media.thewb.com/thewb/images/thumbs/firefly/01/firefly_01_07.jpg"
            re.compile(u'''(?P<seriesname>[^_]+)\\_(?P<seasno>[0-9]+)\\_(?P<epno>[0-9]+).*$''', re.UNICODE),
            # Guid: "http://traffic.libsyn.com/divefilm/episode54hd.m4v"
            re.compile(u'''^.+?episode(?P<epno>[0-9]+).*$''', re.UNICODE),
            # Season 3, Episode 8
            re.compile(u'''^.+?Season\\ (?P<seasno>[0-9]+).*.+?Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # "Episode 1" anywhere in text
            re.compile(u'''^.+?Episode\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # "Episode 1" at the start of the text
            re.compile(u'''Episode\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # "--0027--" when the episode is in the URL link
            re.compile(u'''^.+?--(?P<seasno>[0-9]+)--.*$''', re.UNICODE),
            ]
        self.nv_python_libs_path = u'nv_python_libs'
        self.apiSuffix = u'_api'
        self.language = u'en'
        self.mythdb = None
        self.linksWebPage = None
        self.etree = etree
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


    def initLogger(self, path=sys.stderr, log_name=u'MNV_Grabber'):
        """Setups a logger using the logging module, returns a logger object
        """
        logger = logging.getLogger(log_name)
        formatter = logging.Formatter('%(asctime)s-%(levelname)s: %(message)s', '%Y-%m-%dT%H:%M:%S')

        if path == sys.stderr:
            hdlr = logging.StreamHandler(sys.stderr)
        else:
            hdlr = logging.FileHandler(u'%s/%s.log' % (path, log_name))

        hdlr.setFormatter(formatter)
        logger.addHandler(hdlr)

        if self.debug:
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.INFO)
        self.logger = logger
        return logger
    #end initLogger


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
        '''Replace all &, ', ", <, and > characters with the predefined XML
           entities
        '''
        text = self.textUtf8(text)
        text = text.replace(u'&amp;',u'~~~~~').replace(u'&',u'&amp;').replace(u'~~~~~', u'&amp;')
        text = text.replace(u"'", u"&apos;").replace(u'"', u'&quot;')
        text = text.replace(u'<', u'&lt;').replace(u'>', u'&gt;')
        return text
    # end ampReplace()

    def callCommandLine(self, command, stderr=False):
        '''Perform the requested command line and return an array of stdout strings and
        stderr strings if stderr=True
        return array of stdout string array or stdout and stderr string arrays
        '''
        stderrarray = []
        stdoutarray = []
        try:
            p = subprocess.Popen(command, shell=True, bufsize=4096, stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
        except Exception, e:
            if self.logger:
                self.logger.error(u'callCommandLine Popen Exception, error(%s)' % e)
            if stderr:
                return [[], []]
            else:
                return []

        if stderr:
            while True:
                data = p.stderr.readline()
                if not data:
                    break
                try:
                    data = unicode(data, 'utf8')
                except (UnicodeDecodeError):
                    continue    # Skip any line is cannot be cast as utf8 characters
                except (UnicodeEncodeError, TypeError):
                    pass
                stderrarray.append(data)

        while True:
            data = p.stdout.readline()
            if not data:
                break
            try:
                data = unicode(data, 'utf8')
            except (UnicodeDecodeError):
                continue    # Skip any line that has non-utf8 characters in it
            except (UnicodeEncodeError, TypeError):
                pass
            stdoutarray.append(data)

        if stderr:
            return [stdoutarray, stderrarray]
        else:
            return stdoutarray
    # end callCommandLine()


    def detectUserLocationByIP(self):
        '''Get longitude and latitiude to find videos relative to your location. Up to three different
        servers will be tried before giving up.
        return a dictionary e.g.
        {'Latitude': '43.6667', 'Country': 'Canada', 'Longitude': '-79.4167', 'City': 'Toronto'}
        return an empty dictionary if there were any errors
        Code found at: http://blog.suinova.com/2009/04/from-ip-to-geolocation-country-city.html
        '''
        def getExternalIP():
            '''Find the external IP address of this computer.
            '''
            url = urllib.URLopener()
            try:
                resp = url.open('http://www.whatismyip.com/automation/n09230945.asp')
                return resp.read()
            except:
                return None
            # end getExternalIP()

        ip = getExternalIP()

        if ip == None:
            return {}

        try:
            gs = urllib.urlopen('http://blogama.org/ip_query.php?ip=%s&output=xml' % ip)
            txt = gs.read()
        except:
            try:
                gs = urllib.urlopen('http://www.seomoz.org/ip2location/look.php?ip=%s' % ip)
                txt = gs.read()
            except:
                try:
                    gs = urllib.urlopen('http://api.hostip.info/?ip=%s' % ip)
                    txt = gs.read()
                except:
                    logging.error('GeoIP servers not available')
                    return {}
        try:
            if txt.find('<Response>') > 0:
                countrys = re.findall(r'<CountryName>([\w ]+)<',txt)[0]
                citys = re.findall(r'<City>([\w ]+)<',txt)[0]
                lats,lons = re.findall(r'<Latitude>([\d\-\.]+)</Latitude>\s*<Longitude>([\d\-\.]+)<',txt)[0]
            elif txt.find('GLatLng') > 0:
                citys,countrys = re.findall('<br />\s*([^<]+)<br />\s*([^<]+)<',txt)[0]
                lats,lons = re.findall('LatLng\(([-\d\.]+),([-\d\.]+)',txt)[0]
            elif txt.find('<gml:coordinates>') > 0:
                citys = re.findall('<Hostip>\s*<gml:name>(\w+)</gml:name>',txt)[0]
                countrys = re.findall('<countryName>([\w ,\.]+)</countryName>',txt)[0]
                lats,lons = re.findall('gml:coordinates>([-\d\.]+),([-\d\.]+)<',txt)[0]
            else:
                logging.error('error parsing IP result %s'%txt)
                return {}
            return {'Country':countrys,'City':citys,'Latitude':lats,'Longitude':lons}
        except:
            logging.error('Error parsing IP result %s'%txt)
            return {}
    # end detectUserLocationByIP()


    def displayCustomHTML(self):
        """Common name for a custom HTML display. Used to interface with MythTV plugin NetVision
        """
        embedFlashVarFilter = etree.XPath('//embed', namespaces=self.namespaces)
        variables = self.HTMLvideocode.split(u'?')

        url = u'%s/nv_python_libs/configs/HTML/%s' % (baseProcessingDir, variables[0])
        try:
            customHTML = etree.parse(url)
        except Exception, e:
            raise Exception(u"! Error: The Custom HTML file (%s) cause the exception error (%s)\n" % (url, errormsg))

        # There may be one or more argumants to replace in the HTML code
        # Example:
        # "bbciplayer.html?AttribName1/FirstReplace=bc623bc?SecondReplace/AttribName2=wonderland/..."
        for arg in variables[1:]:
            (attrib, key_value) = arg.split(u'/')
            (key, value) = key_value.split(u'=')
            embedFlashVarFilter(customHTML)[0].attrib[attrib] = embedFlashVarFilter(customHTML)[0].attrib[attrib].replace(key, value)

        sys.stdout.write(etree.tostring(customHTML, encoding='UTF-8', pretty_print=True))

        sys.exit(0)
    # end displayCustomHTML()


    def mnvChannelElement(self, channelDetails):
        ''' Create a MNV Channel element populated with channel details
        return the channel element
        '''
        mnvChannel = etree.fromstring(u"""
<channel>
    <title>%(channel_title)s</title>
    <link>%(channel_link)s</link>
    <description>%(channel_description)s</description>
    <numresults>%(channel_numresults)d</numresults>
    <returned>%(channel_returned)d</returned>
    <startindex>%(channel_startindex)d</startindex>
</channel>
""" % channelDetails
        )
        return mnvChannel
    # end mnvChannelElement()

    # Verify the a URL actually exists
    def checkURL(self, url):
        '''Verify that a URL actually exists. Be careful as redirects can lead to false positives. Use
        the info details to be sure.
        return True when it exists and info
        return False when it does not exist and info
        '''
        urlOpened = urllib.urlopen(url)
        code = urlOpened.getcode()
        actualURL = urlOpened.geturl()
        info = urlOpened.info()
        urlOpened.close()
        if code != 200:
            return [False, info]
        if url != actualURL:
            return [False, info]
        return [True, info]
    # end checkURL()


    def getUrlData(self, inputUrls, pageFilter=None):
        ''' Fetch url data and extract the desired results using a dynamic filter or XSLT stylesheet.
        The URLs are requested in parallel using threading
        return the extracted data organised into directories
        '''
        urlDictionary = {}

        if self.debug:
            print "inputUrls:"
            sys.stdout.write(etree.tostring(inputUrls, encoding='UTF-8', pretty_print=True))
            print

        for element in inputUrls.xpath('.//url'):
            key = element.find('name').text
            urlDictionary[key] = {}
            urlDictionary[key]['type'] = 'raw'
            urlDictionary[key]['href'] = element.find('href').text
            urlFilter = element.findall('filter')
            if len(urlFilter):
                urlDictionary[key]['type'] = 'xpath'
            for index in range(len(urlFilter)):
                urlFilter[index] = urlFilter[index].text
            urlDictionary[key]['filter'] = urlFilter
            urlXSLT = element.findall('xslt')
            if len(urlXSLT):
                urlDictionary[key]['type'] = 'xslt'
            for index in range(len(urlXSLT)):
                urlXSLT[index] = etree.XSLT(etree.parse(u'%s/nv_python_libs/configs/XSLT/%s.xsl' % (self.baseProcessingDir, urlXSLT[index].text)))
            urlDictionary[key]['xslt'] = urlXSLT
            urlDictionary[key]['pageFilter'] = pageFilter
            urlDictionary[key]['parser'] = self.parsers[element.find('parserType').text].copy()
            urlDictionary[key]['namespaces'] = self.namespaces
            urlDictionary[key]['result'] = []
            urlDictionary[key]['morePages'] = u'false'
            urlDictionary[key]['tmp'] = None
            urlDictionary[key]['tree'] = None
            if element.find('parameter') != None:
                urlDictionary[key]['parameter'] = element.find('parameter').text

        if self.debug:
            print "urlDictionary:"
            print urlDictionary
            print

        thread_list = []
        getURL.urlDictionary = urlDictionary

        # Single threaded (commented out) - Only used to prove that multi-threading does
        # not cause data corruption
#        for key in urlDictionary.keys():
#            current = getURL(key, self.debug)
#            thread_list.append(current)
#            current.start()
#            current.join()

        # Multi-threaded
        for key in urlDictionary.keys():
            current = getURL(key, self.debug)
            thread_list.append(current)
            current.start()
        for thread in thread_list:
            thread.join()

        # Take the results and make the return element tree
        root = etree.XML(u"<xml></xml>")
        for key in sorted(getURL.urlDictionary.keys()):
            if not len(getURL.urlDictionary[key]['result']):
                continue
            results = etree.SubElement(root, "results")
            etree.SubElement(results, "name").text = key
            etree.SubElement(results, "url").text = urlDictionary[key]['href']
            etree.SubElement(results, "type").text = urlDictionary[key]['type']
            etree.SubElement(results, "pageInfo").text = getURL.urlDictionary[key]['morePages']
            result = etree.SubElement(results, "result")
            if len(getURL.urlDictionary[key]['filter']):
                for index in range(len(getURL.urlDictionary[key]['result'])):
                    for element in getURL.urlDictionary[key]['result'][index]:
                        result.append(element)
            elif len(getURL.urlDictionary[key]['xslt']):
                for index in range(len(getURL.urlDictionary[key]['result'])):
                    for element in getURL.urlDictionary[key]['result'][index].getroot():
                        result.append(element)
            else:
                for element in getURL.urlDictionary[key]['result'][0].xpath('/*'):
                    result.append(element)

        if self.debug:
            print "root:"
            sys.stdout.write(etree.tostring(root, encoding='UTF-8', pretty_print=True))
            print

        return root
    # end getShows()

    ##############################################################################
    # Start - Utility functions specifically used to modify MNV item data
    ##############################################################################
    def buildFunctionDict(self):
        ''' Create a dictionary of functions that manipulate items data. These functions are imported
        from other MNV grabbers. These functions are meant to be used by the MNV WebCgi type of grabber
        which aggregates data from a number of different sources (e.g. RSS feeds and HTML Web pages)
        including sources from other grabbers.
        Using a dictionary facilitates mixing XSLT functions with pure python functions to use the best
        capabilities of both technologies when translating source information into MNV compliant item
        data.
        return nothing
        '''
        # Add the common XPath extention functions
        self.functionDict = {
            'pubDate': self.pubDate,
            'getSeasonEpisode': self.getSeasonEpisode,
            'convertDuration': self.convertDuration,
            'getHtmlData': self.getHtmlData,
            'linkWebPage': self.linkWebPage,
            'baseDir': self.baseDir,
            'stringLower': self.stringLower,
            'stringUpper': self.stringUpper,
            'stringReplace': self.stringReplace,
            'stringEscape': self.stringEscape,
            'removePunc': self.removePunc,
            'htmlToString': self.htmlToString,
            'checkIfDBItem': self.checkIfDBItem,
            'getItemElement': self.getItemElement,
            'getDBRecords': self.getDBRecords,
            'createItemElement': self.createItemElement,
            'testSubString': self.testSubString,
            }
        # Get the specific source functions
        self.addDynamicFunctions('xsltfunctions')
        return
    # end buildFunctionDict()

    def addDynamicFunctions(self, dirPath):
        ''' Dynamically add functions to the function dictionary from a specified directory
        return nothing
        '''
        fullPath = u'%s/nv_python_libs/%s' % (self.baseProcessingDir, dirPath)
        sys.path.append(fullPath)
        # Make a list of all functions that need to be included
        fileList = []
        for fPath in os.listdir(fullPath):
            filepath, filename = os.path.split( fPath )
            filename, ext = os.path.splitext( filename )
            if filename == u'__init__':
                continue
            if ext != '.py':
                continue
            fileList.append(filename)

        # Do not stop when there is an abort on a library just send an error message to stderr
        for fileName in fileList:
            filename = {'filename': fileName, }
            try:
                exec('''
import %(filename)s
%(filename)s.common = self
for xpathClass in %(filename)s.__xpathClassList__:
    exec(u'xpathClass = %(filename)s.%%s()' %% xpathClass)
    for func in xpathClass.functList:
        exec("self.functionDict['%%s'] = %%s" %% (func, u'xpathClass.%%s' %% func))
for xsltExtension in %(filename)s.__xsltExtentionList__:
    exec("self.functionDict['%%s'] = %%s" %% (xsltExtension, u'%(filename)s.%%s' %% xsltExtension))''' % filename )
            except Exception, errmsg:
                sys.stderr.write(u'! Error: Dynamic import of (%s) XPath and XSLT extention functions\nmessage(%s)\n' % (fileName, errmsg))

        return
    # end addDynamicFunctions()

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

    def getSeasonEpisode(self, context, text):
        ''' Check is there is any season or episode number information in an item's text
        return a string of season and/or episode numbers e.g. "2_21"
        return a string with "None_None" values
        '''
        s_e = [None, None]
        for regexPattern in self.s_e_Patterns:
            match = regexPattern.match(text)
            if not match:
                continue
            season_episode = match.groups()
            if len(season_episode) > 1:
                s_e[0] = season_episode[0]
                s_e[1] = season_episode[1]
            else:
                s_e[1] = season_episode[0]
            return u'%s_%s' % (s_e[0], s_e[1])
        return u'%s_%s' % (s_e[0], s_e[1])
    # end getSeasonEpisode()

    def convertDuration(self, context, duration):
        ''' Take a duration and convert it to seconds
        return a string of seconds
        '''
        min_sec = duration.split(':')
        seconds = 0
        for count in range(len(min_sec)):
            if count != len(min_sec)-1:
                seconds+=int(min_sec[count])*(60*(len(min_sec)-count-1))
            else:
                seconds+=int(min_sec[count])
        return u'%s' % seconds
    # end convertDuration()

    def getHtmlData(self, context, *args):
        ''' Take a HTML string and convert it to an HTML element. Then apply a filter and return
        that value.
        return filter value as a string
        return an empty sting if the filter failed to find any values.
        '''
        xpathFilter = None
        if len(args) > 1:
            xpathFilter = args[0]
            htmldata = args[1]
        else:
            htmldata = args[0]
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

    def linkWebPage(self, context, sourceLink):
        ''' Check if there is a special local HTML page for the link. If not then return a generic
        download only local HTML url.
        return a file://.... link to a local HTML web page
        '''
        # Currently there are no link specific Web pages
        if not self.linksWebPage:
            self.linksWebPage = etree.parse(u'%s/nv_python_libs/configs/XML/customeHtmlPageList.xml' % (self.baseProcessingDir, ))
        if self.linksWebPage.find(sourceLink) != None:
            return u'file://%s/nv_python_libs/configs/HTML/%s' % (self.baseProcessingDir, self.linksWebPage.find(sourceLink).text)
        return u'file://%s/nv_python_libs/configs/HTML/%s' % (self.baseProcessingDir, 'nodownloads.html')
    # end linkWebPage()

    def baseDir(self, context, dummy):
        ''' Return the base directory string
        return the base directory
        '''
        return self.baseProcessingDir
    # end baseDir()

    def stringLower(self, context, data):
        '''
        return a lower case string
        '''
        if not len(data):
            return u''
        return data[0].lower()
    # end stringLower()

    def stringUpper(self, context, data):
        '''
        return a upper case string
        '''
        if not len(data):
            return u''
        return data[0].upper()
    # end stringUpper()

    def stringReplace(self, context, *inputArgs):
        ''' Replace substring values in a string
        return the resulting string from a replace operation
        '''
        args = []
        for arg in inputArgs:
            args.append(arg)
        if not len(args) or len(args) == 1:
            return data
        if len(args) == 2:
            args[0] = args[0].replace(args[1], "")
        else:
            args[0] = args[0].replace(args[1], args[2])
        return args[0].strip()
    # end stringReplace()

    def stringEscape(self, context, *args):
        ''' Replace substring values in a string
        return the resulting string from a replace operation
        '''
        if not len(args):
            return u""
        if len(args) == 1:
            return urllib.quote_plus(args[0].encode("utf-8"))
        else :
            return urllib.quote_plus(args[0].encode("utf-8"), args[1])
    # end stringEscape()

    def removePunc(self, context, data):
        ''' Remove all punctuation for a string
        return the resulting string
        '''
        if not len(data):
            return u""
        return re.sub('[%s]' % re.escape(string.punctuation), '', data)
    # end removePunc()

    def htmlToString(self, context, html):
        ''' Remove HTML tags and LFs from a string
        return the string without HTML tags or LFs
        '''
        if not len(html):
            return u""
        return self.massageText(html).strip().replace(u'\n', u' ').replace(u'', u"&apos;").replace(u'', u"&apos;")
    # end htmlToString()

    def getLanguage(self, context, args):
        ''' Return the current selected language code
        return language code
        '''
        return self.language
    # end getLanguage()

    def checkIfDBItem(self, context, arg):
        ''' Find an 'internetcontentarticles' table record based on fields and values
        return True if a record was found and an item element created
        return False if no record was found
        '''
        results = self.getDBRecords('dummy', arg)
        if len(results):
            self.itemElement = self.createItemElement('dummy', results[0])
            return True
        return False
    # end checkIfDBItem()

    def getItemElement(self, context, arg):
        ''' Return an item element that was created by a previous call to the checkIfDBItem function
        '''
        return self.itemElement
    # end getItemElement()

    def testSubString(self, context, *arg):
        ''' Return True or False if a substring is at the beginning or end of a string
        '''
        if arg[0] == 'starts':
            return arg[1].startswith(arg[2])
        elif arg[0] == 'ends':
            return arg[1].endswith(arg[2])
        else:
            index = arg[1].find(arg[2])
            if index == -1:
                return False
            else:
                return True
    # end testSubString()

    def getDBRecords(self, context, *arg):
        ''' Return a list of 'internetcontentarticles' table records based on field and value matches
        '''
        if not self.mythdb:
            self.initializeMythDB()
            self.itemThumbnail = etree.XPath('.//media:thumbnail', namespaces=self.namespaces)
            self.itemContent = etree.XPath('.//media:content', namespaces=self.namespaces)
        # Encode the search text to UTF-8
        for key in arg[0].keys():
            try:
                arg[0][key] = arg[0][key].encode('UTF-8')
            except:
                return []
        return list(self.mythdb.searchInternetContent(**arg[0]))
    # end getDBItem()

    def createItemElement(self, context, *arg):
        ''' Create an item element from an 'internetcontentarticles' table record dictionary
        return the item element
        '''
        result = arg[0]
        itemElement = etree.XML(self.mnvItem)
        # Insert data into a new item element
        itemElement.find('link').text = result['url']
        if result['title']:
            itemElement.find('title').text = result['title']
        if result['subtitle']:
            etree.SubElement(itemElement, "subtitle").text = result['subtitle']
        if result['description']:
            itemElement.find('description').text = result['description']
        if result['author']:
            itemElement.find('author').text = result['author']
        if result['date']:
            itemElement.find('pubDate').text = result['date'].strftime(self.pubDateFormat)
        if result['rating'] != '32576' and result['rating'][0] != '-':
            itemElement.find('rating').text = result['rating']
        if result['thumbnail']:
            self.itemThumbnail(itemElement)[0].attrib['url'] = result['thumbnail']
        if result['mediaURL']:
            self.itemContent(itemElement)[0].attrib['url'] = result['mediaURL']
        if result['filesize'] > 0:
            self.itemContent(itemElement)[0].attrib['length'] = unicode(result['filesize'])
        if result['time'] > 0:
            self.itemContent(itemElement)[0].attrib['duration'] = unicode(result['time'])
        if result['width'] > 0:
            self.itemContent(itemElement)[0].attrib['width'] = unicode(result['width'])
        if result['height'] > 0:
            self.itemContent(itemElement)[0].attrib['height'] = unicode(result['height'])
        if result['language']:
            self.itemContent(itemElement)[0].attrib['lang'] = result['language']
        if result['season'] > 0:
            etree.SubElement(itemElement, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = unicode(result['season'])
        if result['episode'] > 0:
            etree.SubElement(itemElement, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = unicode(result['episode'])
        if result['customhtml'] == 1:
            etree.SubElement(itemElement, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}customhtml").text = 'true'
        if result['countries']:
            countries = result['countries'].split(u' ')
            for country in countries:
                etree.SubElement(itemElement, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = country
        return itemElement
    # end createItemElement()

    def initializeMythDB(self):
        ''' Import the MythTV database bindings
        return nothing
        '''
        try:
            from MythTV import MythDB, MythLog, MythError
            try:
                '''Create an instance of each: MythDB
                '''
                MythLog._setlevel('none') # Some non option -M cannot have any logging on stdout
                self.mythdb = MythDB()
            except MythError, e:
                sys.stderr.write(u'\n! Error - %s\n' % e.args[0])
                filename = os.path.expanduser("~")+'/.mythtv/config.xml'
                if not os.path.isfile(filename):
                    sys.stderr.write(u'\n! Error - A correctly configured (%s) file must exist\n' % filename)
                else:
                    sys.stderr.write(u'\n! Error - Check that (%s) is correctly configured\n' % filename)
                sys.exit(1)
            except Exception, e:
                sys.stderr.write(u"\n! Error - Creating an instance caused an error for one of: MythDB. error(%s)\n" % e)
                sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"\n! Error - MythTV python bindings could not be imported. error(%s)\n" % e)
            sys.exit(1)
    # end initializeMythDB()


    ##############################################################################
    # End  - Utility functions specifically used to modify MNV item data
    ##############################################################################
# end Common() class

class getURL(Thread):
    ''' Threaded download of a URL and filter out the desired data for XML and (X)HTML
    return the filter results
    '''
    def __init__ (self, urlKey, debug):
        Thread.__init__(self)
        self.urlKey = urlKey
        self.debug = debug

    def run(self):
        if self.debug:
            print u"getURL href(%s)" % (self.urlDictionary[self.urlKey]['href'], )
            print

        # Input the data from a url
        try:
            self.urlDictionary[self.urlKey]['tree'] = etree.parse(self.urlDictionary[self.urlKey]['href'], self.urlDictionary[self.urlKey]['parser'])
        except Exception, errormsg:
            sys.stderr.write(u"! Error: The URL (%s) cause the exception error (%s)\n" % (self.urlDictionary[self.urlKey]['href'], errormsg))
            return

        if self.debug:
            print "Raw unfiltered URL input:"
            sys.stdout.write(etree.tostring(self.urlDictionary[self.urlKey]['tree'], encoding='UTF-8', pretty_print=True))
            print

        if len(self.urlDictionary[self.urlKey]['filter']):
            for index in range(len(self.urlDictionary[self.urlKey]['filter'])):
                # Filter out the desired data
                try:
                   self.urlDictionary[self.urlKey]['tmp'] = self.urlDictionary[self.urlKey]['tree'].xpath(self.urlDictionary[self.urlKey]['filter'][index], namespaces=self.urlDictionary[self.urlKey]['namespaces'])
                except AssertionError, e:
                    sys.stderr.write(u"No filter results for Name(%s)\n" % self.urlKey)
                    sys.stderr.write(u"No filter results for url(%s)\n" % self.urlDictionary[self.urlKey]['href'])
                    sys.stderr.write(u"! Error:(%s)\n" % e)
                    if len(self.urlDictionary[self.urlKey]['filter']) == index-1:
                        return
                    else:
                        continue
                self.urlDictionary[self.urlKey]['result'].append(self.urlDictionary[self.urlKey]['tmp'])
        elif len(self.urlDictionary[self.urlKey]['xslt']):
            for index in range(len(self.urlDictionary[self.urlKey]['xslt'])):
                # Process the results through a XSLT stylesheet out the desired data
                try:
                    if self.urlDictionary[self.urlKey].has_key('parameter'):
                        self.urlDictionary[self.urlKey]['tmp'] = self.urlDictionary[self.urlKey]['xslt'][index](self.urlDictionary[self.urlKey]['tree'], paraMeter= etree.XSLT.strparam(
self.urlDictionary[self.urlKey]['parameter']) )
                    else:
                        self.urlDictionary[self.urlKey]['tmp'] = self.urlDictionary[self.urlKey]['xslt'][index](self.urlDictionary[self.urlKey]['tree'])
                except Exception, e:
                    sys.stderr.write(u"! XSLT Error:(%s) Key(%s)\n" % (e, self.urlKey))
                    if len(self.urlDictionary[self.urlKey]['filter']) == index-1:
                        return
                    else:
                        continue
                # Was any data found?
                if self.urlDictionary[self.urlKey]['tmp'].getroot() == None:
                    sys.stderr.write(u"No Xslt results for Name(%s)\n" % self.urlKey)
                    sys.stderr.write(u"No Xslt results for url(%s)\n" % self.urlDictionary[self.urlKey]['href'])
                    if len(self.urlDictionary[self.urlKey]['filter']) == index-1:
                        return
                    else:
                        continue
                self.urlDictionary[self.urlKey]['result'].append(self.urlDictionary[self.urlKey]['tmp'])
        else:
            # Just pass back the raw data
            self.urlDictionary[self.urlKey]['result'] = [self.urlDictionary[self.urlKey]['tree']]

        # Check whether there are more pages available
        if self.urlDictionary[self.urlKey]['pageFilter']:
            if len(self.urlDictionary[self.urlKey]['tree'].xpath(self.urlDictionary[self.urlKey]['pageFilter'], namespaces=self.urlDictionary[self.urlKey]['namespaces'])):
                self.urlDictionary[self.urlKey]['morePages'] = 'true'
        return
   # end run()
# end class getURL()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################
