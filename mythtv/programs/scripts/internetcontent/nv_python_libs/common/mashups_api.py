#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mashups_api.py - Common class libraries for all MythNetvision Mashup processing
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script contains a number of common functions used for processing MythNetvision
#           Mashups.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mashups_common - Common class libraries for all MythNetvision Mashup processing"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions for the processing of
MythNetvision Mashups scripts that run as a Web application and global functions used by many
MNV grabbers.
'''

__version__="v0.1.4"
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

import os, struct, sys, re, time, subprocess
import urllib
import logging
import telnetlib
from threading import Thread

from mashups_exceptions import (MashupsUrlError, MashupsHttpError, MashupsRssError, MashupsVideoNotFound, MashupsXmlError, )

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


class Videos(object):
    """Main interface to the MNV Grabbers that run as a Web server application
    This is done to support common functions when interfacing the MNV CGI Web server interface.
    Supporting both search and tree view methods.
    """
    def __init__(self,
                apikey,
                mythtv = True,
                interactive = False,
                select_first = False,
                debug = False,
                custom_ui = None,
                language = None,
                search_all_languages = False,
                ):
        """apikey (str/unicode):
            Specify the target site API key. Applications need their own key in some cases

        mythtv (True/False):
            When True, the returned meta data is being returned has the key and values
            massaged to match MythTV
            When False, the returned meta data  is being returned matches what target site returned

        interactive (True/False): (This option is not supported by all target site apis)
            When True, uses built-in console UI is used to select the correct show.
            When False, the first search result is used.

        select_first (True/False): (This option is not supported currently implemented in any grabbers)
            Automatically selects the first series search result (rather
            than showing the user a list of more than one series).
            Is overridden by interactive = False, or specifying a custom_ui

        debug (True/False):
             shows verbose debugging information

        custom_ui (xx_ui.BaseUI subclass): (This option is not supported currently implemented in any grabbers)
            A callable subclass of interactive class (overrides interactive option)

        language (2 character language abbreviation): (This option is not supported by all target site apis)
            The language of the returned data. Is also the language search
            uses. Default is "en" (English). For full list, run..

        search_all_languages (True/False): (This option is not supported by all target site apis)
            By default, a Netvision grabber will only search in the language specified using
            the language option. When this is True, it will search for the
            show in any language

        """
        self.config = {}

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug    # Set the common function debug level

        self.log_name = "WebCGI_Mashups"
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['search_all_languages'] = search_all_languages

        # Defaulting to ENGISH most mashups do not support specifying a language
        self.config['language'] = "en"

        self.error_messages = {'MashupsUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'MashupsHttpError': u"! Error: An HTTP communicating error with MNV CGI Web application was raised (%s)\n", 'MashupsRssError': u"! Error: Invalid RSS meta data\nwas received from MNV CGI Web application error (%s). Skipping item.\n", 'MashupsVideoNotFound': u"! Error: Video search with MNV CGI Web application did not return any results (%s)\n", }

        # The following url_ configs are based for the Web CGI interface. Example:
        # http://192.168.0.100:80/cgi-bin
        self.config['base_url'] = u"http://%s:%s/%s/"
        if self.config['debug_enabled']:
            self.config['search'] = self.config['base_url']+u"%s?searchterms=%s&page=%s&debug=true&hostname=%s&port=%s"
            self.config['treeview'] = self.config['base_url']+u"%s?debug=true&hostname=%s&port=%s"
        else:
            self.config['search'] = self.config['base_url']+u"%s?searchterms=%s&page=%s&hostname=%s&port=%s"
            self.config['treeview'] = self.config['base_url']+u"%s?hostname=%s&port=%s"
    # end __init__()


    def searchMashup(self, mashupName, searchTerm, pagenumber, pagelen):
        '''Key word video search using the EMML mashup server for a specific search emml file
        When there are items returned display the results to stdout
        When no items are returned display nothing but post an appropriate message to stderr
        '''
        url = self.config['search'] % (slef.common.ip_hostname, slef.common.port, urllib.quote_plus(mashupName.lower().encode("utf-8")), urllib.quote_plus(searchTerm.encode("utf-8")), pagenumber, slef.common.ip_hostname, slef.common.port, )

        if self.config['debug_enabled']:
            print "Search URL:"
            print url
            print

        try:
            tree = etree.parse(url, etree.XMLParser(remove_blank_text=True))
        except Exception, errormsg:
            raise MashupsUrlError(self.error_messages['MashupsUrlError'] % (url, errormsg))

        if tree is None:
            raise MashupsVideoNotFound(u"Nothing returned for Video search value (%s)" % searchTerm)

        if tree.xpath('not(boolean(//item))'):
            raise MashupsVideoNotFound(u"No Video matches found for search value (%s)" % searchTerm)

        # Display the results
        sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True))

        return
        # end searchMashup()


    def searchForVideos(self, searchTerm, pagenumber):
        '''Execute a search Mashup. This code allows search mashups to be just stub scripts
        return nothing just display results or error messages
        '''
        # Read the "~/.mythtv/mnvMashupsConfig.xml file"
        try:
            self.common.getFEConfig()
        except Exception, e:
            sys.stderr.write('''
%s\n''' % e)
            sys.exit(1)

        # Get and display all enabled search Mashups results
        try:
            self.searchMashup(self.mashup_title, searchTerm, pagenumber, self.page_limit)
        except Exception, e:
            sys.stderr.write('''
%s\n''' % e)
            sys.exit(1)

        sys.exit(0)
    # end searchForVideos()


    def treeviewMashup(self, mashupName):
        '''Call the CGI Web server using the treeview mashup name and display the results if any
        return nothing
        When no items are returned display nothing but post an appropriate message to stderr
        '''
        url = self.config['treeview'] % (slef.common.ip_hostname, slef.common.port, urllib.quote_plus(mashupName.lower().encode("utf-8")), slef.common.ip_hostname, slef.common.port, )

        if self.config['debug_enabled']:
            print "Treeview URL:"
            print url
            print

        try:
            tree = etree.parse(url, etree.XMLParser(remove_blank_text=True))
        except Exception, errormsg:
            raise MashupsUrlError(self.error_messages['MashupsUrlError'] % (url, errormsg))

        if tree is None:
            raise MashupsVideoNotFound(u"Nothing returned from treeview mashup (%s)" % mashupName)

        if tree.xpath('not(boolean(//directory))'):
            raise MashupsVideoNotFound(u"No Videos from treeview mashup (%s)" % mashupName)

        # Display the treeview results
        sys.stdout.write(etree.tostring(tree, encoding='UTF-8', pretty_print=True))
        return
        # end treeviewMashup()


    def displayTreeView(self):
        '''Execute all enabled treeview mashups aggrigated into a single multi-channel MNV RSS treeview
        return nothing
        '''
        # Read the "~/.mythtv/mnvMashupsConfig.xml file"
        try:
            slef.common.getFEConfig()
        except Exception, e:
            sys.stderr.write('''
%s\n''' % e)
            sys.exit(1)

        # Get and display a treeview Mashup
        try:
            self.treeviewMashup(self.mashup_title, shutdown=self.shutdownOnExit)
        except Exception, e:
            sys.stderr.write('''
%s\n''' % e)
            sys.exit(1)

        sys.exit(0)
    # end displayTreeView()
# end Mashups() class


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
        self.namespaces = {
            'xsi': u"http://www.w3.org/2001/XMLSchema-instance",
            'media': u"http://search.yahoo.com/mrss/",
            'xhtml': u"http://www.w3.org/1999/xhtml",
            'atm': u"http://www.w3.org/2005/Atom",
            'mythtv': "http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format",
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
        '''Replace all "&" characters with "&amp;"
        '''
        text = self.textUtf8(text)
        return text.replace(u'&amp;',u'~~~~~').replace(u'&',u'&amp;').replace(u'~~~~~', u'&amp;')
    # end ampReplace()

    def callCommandLine(self, command, stderr=False):
        '''Perform the requested command line and return an array of stdout strings and stderr strings if
        stderr=True
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


    def checkForCGIServer(self, tn_host, tn_port):
        '''Using Telnet check if the CGI Web server is running
        The code was copied from: http://www.velocityreviews.com/forums/t327444-telnet-instead-ping.html
        return True if the server is responding
        return Fale if the server is NOT responding
        '''
        try:
            tn = telnetlib.Telnet(tn_host,tn_port)
            tn.close()
            return True
        except:
            False
    # end checkForCGIServer()


    def getFEConfig(self):
        '''Load and extract data from the FE's Mashups configuration file. Initialize all the appropriate
        variables from the config file or use set defaults if the file does not exist.
        This configuration file may not exist.
        return nothing
        '''
        # Iniitalize variables to their defaults
        self.local = True
        self.ip_hostname = u'localhost'
        self.port = u'80'

        url = u"file://%s/%s" % (os.path.expanduser(u"~"), u".mythtv/MythNetvision/userGrabberPrefs/mnvMashupsConfig.xml")
        if self.debug:
            print url
            print

        try:
            tree = etree.parse(url)
        except Exception, errormsg:
            self.local = True
            return

        if tree is None:
            raise MashupsVideoNotFound(u"No Mashups configuration file found (%s)" % url)

        if tree.find('local') is not None:
            if tree.find('local').text != 'true':
                self.local = False
            else:
                return

        if tree.find('ip_hostname') is not None:
            self.ip_hostname = tree.find('ip_hostname').text

        if tree.find('port') is not None:
            self.ip_hostname = tree.find('port').text

        return
    # end getFEConfig()

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


    def getUrlData(self, inputUrls, pageFilter=None):
        ''' Fetch url data and extract the desired results using a dynamic filter.
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
            urlDictionary[key]['href'] = element.find('href').text
            urlFilter = element.findall('filter')
            for index in range(len(urlFilter)):
                urlFilter[index] = urlFilter[index].text
            urlDictionary[key]['filter'] = urlFilter
            urlDictionary[key]['pageFilter'] = pageFilter
            urlDictionary[key]['parser'] = self.parsers[element.find('parserType').text].copy()
            urlDictionary[key]['namespaces'] = self.namespaces
            urlDictionary[key]['result'] = []
            urlDictionary[key]['morePages'] = u'false'
            urlDictionary[key]['tmp'] = None
            urlDictionary[key]['tree'] = None

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
            etree.SubElement(results, "pageInfo").text = getURL.urlDictionary[key]['morePages']
            result = etree.SubElement(results, "result")
            for index in range(len(getURL.urlDictionary[key]['result'])):
                for element in getURL.urlDictionary[key]['result'][index]:
                    result.append(element)

        if self.debug:
            print "root:"
            sys.stdout.write(etree.tostring(root, encoding='UTF-8', pretty_print=True))
            print

        return root
    # end getShows()
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
            print "getURL href(%s)" % (self.urlDictionary[self.urlKey]['href'], )
            print

        # Input the data from a url
        try:
            self.urlDictionary[self.urlKey]['tree'] = etree.parse(self.urlDictionary[self.urlKey]['href'], self.urlDictionary[self.urlKey]['parser'])
        except Exception, errormsg:
            sys.stderr.write(u"! Error: The URL (%s) cause the exception error (%s)\n" % (self.urlDictionary[self.urlKey]['href'], errormsg))
            return

        if self.debug:
            print "Raw unfiltered URL imput:"
            sys.stdout.write(etree.tostring(self.urlDictionary[self.urlKey]['tree'], encoding='UTF-8', pretty_print=True))
            print

        if len(self.urlDictionary[self.urlKey]['filter']):
            for index in range(len(self.urlDictionary[self.urlKey]['filter'])):
                # Filter out the desired data
                try:
                   self.urlDictionary[self.urlKey]['tmp'] = self.urlDictionary[self.urlKey]['tree'].xpath(self.urlDictionary[self.urlKey]['filter'][index], namespaces=self.urlDictionary[self.urlKey]['namespaces'])
                except AssertionError, e:
                    sys.stderr.write("No filter results for Name(%s)\n" % self.urlKey)
                    sys.stderr.write("No filter results for url(%s)\n" % self.urlDictionary[self.urlKey]['href'])
                    sys.stderr.write("! Error:(%s)\n" % e)
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
