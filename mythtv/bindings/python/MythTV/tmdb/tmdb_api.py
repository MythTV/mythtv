#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb_api.py      Simple-to-use Python interface to The TMDB's API (www.themoviedb.org)
# Python Script
# Author:   dbr/Ben modified by R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           metadata and image URLs from TMDB. These routines are based on the v2.1 TMDB api. Specifications
#           for this apu are published at http://api.themoviedb.org/2.1/
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tmdb_api - Simple-to-use Python interface to The TMDB's API (www.themoviedb.org)";
__author__="dbr/Ben modified by R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
metadata and image URLs from TMDB. These routines are based on the v2.1 TMDB api. Specifications
for this api are published at http://api.themoviedb.org/2.1/
'''

__version__="v0.2.5"
# 0.1.0 Initial development
# 0.1.1 Alpha Release
# 0.1.2 Added removal of any line-feeds from data
# 0.1.3 Added display of URL to TMDB XML when debug was specified
#       Added check and skipping any empty data from TMDB
# 0.1.4 More data validation added (e.g. valid image file extentions)
#       More data massaging added.
# 0.1.5 Added a superclass to perform TMDB Trailer searches for the Mythnetvison grabber tmdb_nv.py
# 0.1.6 Improved displayed error messages on an exception abort
# 0.1.7 Fixed issues with interactive movie selection
# 0.1.8 Fixed the error message reporting when the machines Internet connection or DNS is not working
# 0.1.9 Fixed an abort with the People data logic due to empty data (e.g. biography) being passed by TMDB
#       Included in this change are a number of other potential empty data checks.
# 0.2.0 Support multi-languages. TMDB supports multi-languages with a fall back to English when the
#       data is not in the specified language
# 0.2.1 Add support for XML display. http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
# 0.2.2 Fixed a bug with XSLT title and subtitle.
# 0.2.3 Added encoding XML declaration "<?xml version='1.0' encoding='UTF-8'?>" to output
# 0.2.4 Added width and height attributes to images
# 0.2.5 Added a replace text XPatch extention so that "person" full size images can be created from
#       the thumbnail.

import os, struct, sys, time
import urllib, urllib2
from copy import deepcopy
import logging

from tmdb_ui import BaseUI, ConsoleUI
from tmdb_exceptions import (TmdBaseError, TmdHttpError, TmdXmlError, TmdbUiAbort, TmdbMovieOrPersonNotFound,)


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
    from lxml import etree as eTree
except Exception, e:
    sys.stderr.write(u'\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
    sys.exit(1)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
version = ''
for digit in eTree.LIBXML_VERSION:
    version+=str(digit)+'.'
version = version[:-1]
if version < '2.7.2':
    sys.stderr.write(u'''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


try:
    import xml.etree.cElementTree as ElementTree
except ImportError:
    import xml.etree.ElementTree as ElementTree


class XmlHandler:
    """Deals with retrieval of XML files from API
    """
    def __init__(self, url):
        self.url = url

    def _grabUrl(self, url):
        try:
            urlhandle = urllib.urlopen(url)
        except IOError, errormsg:
            raise TmdHttpError(errormsg)
        return urlhandle.read()

    def getEt(self):
        xml = self._grabUrl(self.url)
        try:
            et = ElementTree.fromstring(xml)
        except SyntaxError, errormsg:
            raise TmdXmlError(errormsg)
        return et


class SearchResults(list):
    """Stores a list of Movie's that matched the search
    """
    def __repr__(self):
        return u"<Search results: %s>" % (list.__repr__(self))


class Movie(dict):
    """A dict containing the information about the film
    """
    def __repr__(self):
        return u"<Movie: %s>" % self.get(u"title")


class MovieAttribute(dict):
    """Base class for more complex attributes (like Poster,
    which has multiple resolutions)
    """
    pass


class Poster(MovieAttribute):
    """Stores poster image URLs, each size is under the appropriate dict key.
    Common sizes are: cover, mid, original, thumb
    """
    def __repr__(self):
        return u"<%s with sizes %s>" % (
            self.__class__.__name__,
            u", ".join(
                [u"'%s'" % x for x in sorted(self.keys())]
            )
        )

    def set(self, poster_et):
        """Takes an elementtree Element ('poster') and appends the poster,
        with a the size as the dict key.

        For example:
        <image type="poster"
            size="cover"
            url="http://example.org/poster_original.jpg"
            id="36431"
        />

        ..becomes:
        poster['cover'] = ['http://example.org/poster_original.jpg, '36431']
        """
        size = poster_et.get(u"size").strip()
        url = poster_et.get(u"url").strip()
        (dirName, fileName) = os.path.split(url)
        (fileBaseName, fileExtension)=os.path.splitext(fileName)
        if not fileExtension[1:].lower() in self.image_extentions:
            return
        imageid = poster_et.get(u"id").strip()
        if not self.has_key(size):
            self[size] = [[url, imageid]]
        else:
            self[size].append([url, imageid])

    def largest(self):
        """Attempts to return largest image.
        """
        for cur_size in [u"original", u"mid", u"cover", u"thumb"]:
            if cur_size in self:
                return self[cur_size]

    def medium(self):
        """Attempts to return medium size image.
        """
        for cur_size in [u"cover", u"thumb", u"mid", u"original", ]:
            if cur_size in self:
                return self[cur_size]

class Backdrop(Poster):
    """Stores backdrop image URLs, each size under the appropriate dict key.
    Common sizes are: mid, original, thumb
    """
    pass

class People(MovieAttribute):
    """Stores people in a dictionary of roles in the movie.
    """
    def __repr__(self):
        return u"<%s with jobs %s>" % (
            self.__class__.__name__,
            u", ".join(
                [u"'%s'" % x for x in sorted(self.keys())]
            )
        )

    def set(self, cast_et):
        """Takes an element tree Element ('cast') and stores a dictionary of roles,
        for each person.

        For example:
        <cast>
            <person
                url="http://www.themoviedb.org/person/138"
                name="Quentin Tarantino" job="Director"
                character="Special Guest Director"
                id="138"/>
            ...
         </cast>

        ..becomes:
        self['people']['director'] = 'Robert Rodriguez'
        """
        self[u'people']={}
        people = self[u'people']
        for node in cast_et.getchildren():
            if node.get(u'name') != None:
                try:
                    key = unicode(node.get(u"job").lower(), 'utf8')
                except (UnicodeEncodeError, TypeError):
                    key = node.get(u"job").lower().strip()
                try:
                    data = unicode(node.get(u'name'), 'utf8')
                except (UnicodeEncodeError, TypeError):
                    data = node.get(u'name')
                if people.has_key(key):
                    people[key]=u"%s,%s" % (people[key], data.strip())
                else:
                    people[key]=data.strip()

class MovieDb(object):
    """Main interface to www.themoviedb.org

    Supports several search TMDB search methods and a number of TMDB data retrieval methods.
    The apikey is a maditory parameter when creating an instance of this class
    """
    def __init__(self,
                apikey,
                mythtv = False,
                interactive = False,
                select_first = False,
                debug = False,
                custom_ui = None,
                language = None,
                search_all_languages = False, ###CHANGE - Needs to be added
                ):
        """apikey (str/unicode):
            Specify the themoviedb.org API key. Applications need their own key.
            See http://api.themoviedb.org/2.1/ to get your own API key

        mythtv (True/False):
            When True, the movie metadata is being returned has the key and values massaged to match MythTV
            When False, the movie metadata is being returned matches what TMDB returned

        interactive (True/False):
            When True, uses built-in console UI is used to select the correct show.
            When False, the first search result is used.

        select_first (True/False):
            Automatically selects the first series search result (rather
            than showing the user a list of more than one series).
            Is overridden by interactive = False, or specifying a custom_ui

        debug (True/False):
             shows verbose debugging information

        custom_ui (tmdb_ui.BaseUI subclass):
            A callable subclass of tmdb_ui.BaseUI (overrides interactive option)

        language (2 character language abbreviation):
            The language of the returned data. Is also the language search
            uses. Default is "en" (English).

        search_all_languages (True/False):
            By default, TMDB will only search in the language specified using
            the language option. When this is True, it will search for the
            show in any language

        """
        self.config = {}

        if apikey is not None:
            self.config['apikey'] = apikey
        else:
            sys.stderr.write("\n! Error: An TMDB API key must be specified. See http://api.themoviedb.org/2.1/ to get your own API key\n\n")
            sys.exit(1)

        # Set the movie details function to either massaged for MythTV or left as it is returned by TMDB
        if mythtv:
            self.movieDetails = self._mythtvDetails
        else:
            self.movieDetails = self._tmdbDetails

        self.config['debug_enabled'] = debug # show debugging messages

        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        # The supported languages are any two chracters from
        # http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
        # The default is English including returned data if the movie exists but not in the
        # specified language.
        if language is None:
            self.config['language'] = "en"
        else:
            self.config['language'] = language

        # The following url_ configs are based of the
        # http://api.themoviedb.org/2.1/
        self.config['base_url'] = "http://api.themoviedb.org/2.1"

        self.lang_url = u'/%s/xml/' # Used incase the user wants to overside the configured/default language

        self.config[u'urls'] = {}

        # v2.1 api calls
        self.config[u'urls'][u'movie.search'] = u'%(base_url)s/Movie.search/%(language)s/xml/%(apikey)s/%%s' % (self.config)
        self.config[u'urls'][u'tmdbid.search'] = u'%(base_url)s/Movie.getInfo/%(language)s/xml/%(apikey)s/%%s' % (self.config)
        self.config[u'urls'][u'imdb.search'] = u'%(base_url)s/Movie.imdbLookup/%(language)s/xml/%(apikey)s/tt%%s'  % (self.config)
        self.config[u'urls'][u'image.search'] = u'%(base_url)s/Movie.getImages/%(language)s/xml/%(apikey)s/%%s' % (self.config)
        self.config[u'urls'][u'person.search'] = u'%(base_url)s/Person.search/%(language)s/xml/%(apikey)s/%%s' % (self.config)
        self.config[u'urls'][u'person.info'] = u'%(base_url)s/Person.getInfo/%(language)s/xml/%(apikey)s/%%s' % (self.config)
        self.config[u'urls'][u'hash.info'] = u'%(base_url)s/Hash.getInfo/%(language)s/xml/%(apikey)s/%%s' % (self.config)

        # Translation of TMDB elements into MythTV keys/db grabber names
        self.config[u'mythtv_translation'] = {u'actor': u'cast', u'backdrop': u'fanart', u'categories': u'genres', u'director':  u'director', u'id': u'inetref', u'name': u'title', u'overview': u'plot', u'rating': u'userrating', u'poster': u'coverart', u'production_countries': u'countries', u'released': u'releasedate', u'runtime': u'runtime', u'url': u'url', u'imdb_id': u'imdb', u'certification': u'movierating', }
        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions
        self.thumbnails = False
        self.xml = False
        self.pubDateFormat = u'%a, %d %b %Y %H:%M:%S GMT'
        self.xmlParser = eTree.HTMLParser(remove_blank_text=True)
        self.baseProcessingDir = os.path.dirname( os.path.realpath( __file__ ))
        self.supportedJobList = ["actor", "author", "producer", "executive producer", "director", "cinematographer", "composer", "editor", "casting", ]
        self.tagTranslations = {
            'poster': 'coverart',
            'backdrop': 'fanart',
            }
        self.separatorTitleSubtitle = [
            ':',
            ' - ',
            ]
    # end __init__()


    def _initLogger(self):
        """Setups a logger using the logging module, returns a log object
        """
        logger = logging.getLogger("tmdb")
        formatter = logging.Formatter('%(asctime)s) %(levelname)s %(message)s')

        hdlr = logging.StreamHandler(sys.stdout)

        hdlr.setFormatter(formatter)
        logger.addHandler(hdlr)

        if self.config['debug_enabled']:
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.WARNING)
        return logger
    #end initLogger


    def textUtf8(self, text):
        if text == None:
            return text
        try:
            return unicode(text, 'utf8')
        except (UnicodeEncodeError, TypeError):
            return text
    # end textUtf8()


    def getCategories(self, categories_et):
        """Takes an element tree Element ('categories') and create a string of comma seperated film
        categories
        return comma seperated sting of film category names

        For example:
        <categories>
            <category type="genre" url="http://themoviedb.org/encyclopedia/category/80" name="Crime"/>
            <category type="genre" url="http://themoviedb.org/encyclopedia/category/18" name="Drama"/>
            <category type="genre" url="http://themoviedb.org/encyclopedia/category/53" name="Thriller"/>
        </categories>

        ..becomes:
        'Crime,Drama,Thriller'
        """
        cat = u''
        for category in categories_et.getchildren():
            if category.get(u'name') != None:
                cat+=u"%s," % self.textUtf8(category.get(u'name').strip())
        if len(cat):
            cat=cat[:-1]
        return cat


    def getStudios(self, studios_et):
        """Takes an element tree Element ('studios') and create a string of comma seperated film
        studios names
        return comma seperated sting of film studios names

        For example:
        <studios>
            <studio url="http://www.themoviedb.org/encyclopedia/company/20" name="Miramax Films"/>
            <studio url="http://www.themoviedb.org/encyclopedia/company/334" name="Dimension"/>
        </studios>

        ..becomes:
        'Miramax Films,Dimension'
        """
        cat = u''
        for studio in studios_et.getchildren():
            if studio.get(u'name') != None:
                cat+=u"%s," % self.textUtf8(studio.get(u'name').strip())
        if len(cat):
            cat=cat[:-1]
        return cat

    def getProductionCountries(self, countries_et):
        """Takes an element tree Element ('countries') and create a string of comma seperated film
        countries
        return comma seperated sting of countries associated with the film

        For example:
        <countries>
            <country url="http://www.themoviedb.org/encyclopedia/country/223" name="United States of America" code="US"/>
        </countries>
        ..becomes:
        'United States of America'
        """
        countries = u''
        for country in countries_et.getchildren():
            if country.get(u'name') != None:
                if len(countries):
                    countries+=u", %s" % self.textUtf8(country.get(u'name').strip())
                else:
                    countries=self.textUtf8(country.get(u'name').strip())
        return countries


    def _tmdbDetails(self, movie_element):
        '''Create a dictionary of movie details including text metadata, image URLs (poster/fanart)
        return the dictionary of movie information
        '''
        cur_movie = Movie()
        cur_poster = Poster()
        cur_backdrop = Backdrop()
        cur_poster.image_extentions = self.config[u'image_extentions']
        cur_backdrop.image_extentions = self.config[u'image_extentions']
        cur_people = People()

        for item in movie_element.getchildren():
            if item.tag.lower() == u"images":
                for image in item.getchildren():
                    if image.get(u"type").lower() == u"poster":
                        cur_poster.set(image)
                    elif image.get(u"type").lower() == u"backdrop":
                        cur_backdrop.set(image)
            elif item.tag.lower() == u"categories":
                cur_movie[u'categories'] = self.getCategories(item)
            elif item.tag.lower() == u"studios":
                cur_movie[u'studios'] = self.getStudios(item)
            elif item.tag.lower() == u"countries":
                cur_movie[u'production_countries'] = self.getProductionCountries(item)
            elif item.tag.lower() == u"cast":
                cur_people.set(item)
            else:
                if item.text != None:
                    tag = self.textUtf8(item.tag.strip())
                    cur_movie[tag] = self.textUtf8(item.text.strip())

        if cur_poster.largest() != None:
            tmp = u''
            for imagedata in cur_poster.largest():
                if imagedata[0]:
                    tmp+=u"%s," % imagedata[0]
            if len(tmp):
                tmp = tmp[:-1]
                cur_movie[u'poster'] = tmp
        if self.thumbnails and cur_poster.medium() != None:
            tmp = u''
            for imagedata in cur_poster.medium():
                if imagedata[0]:
                    tmp+=u"%s," % imagedata[0]
            if len(tmp):
                tmp = tmp[:-1]
                cur_movie[self.thumbnails] = tmp
        if cur_backdrop.largest() != None:
            tmp = u''
            for imagedata in cur_backdrop.largest():
                if imagedata[0]:
                    tmp+=u"%s," % imagedata[0]
            if len(tmp):
                tmp = tmp[:-1]
                cur_movie[u'backdrop'] = tmp
        if cur_people.has_key(u'people'):
            if cur_people[u'people'] != None:
                for key in cur_people[u'people']:
                    if cur_people[u'people'][key]:
                        cur_movie[key] = cur_people[u'people'][key]

        if self._tmdbDetails == self.movieDetails:
            data = {}
            for key in cur_movie.keys():
                if cur_movie[key]:
                    data[key] = cur_movie[key]
            return data
        else:
            return cur_movie
    # end _tmdbDetails()


    def _mythtvDetails(self, movie_element):
        '''Massage the movie details into key value pairs as compatible with MythTV
        return a dictionary of massaged movie details
        '''
        if movie_element == None:
            return {}
        cur_movie = self._tmdbDetails(movie_element)
        translated={}
        for key in cur_movie:
            if cur_movie[key] == None or cur_movie[key] == u'None':
                continue
            if isinstance(cur_movie[key], str) or isinstance(cur_movie[key], unicode):
                if cur_movie[key].strip() == u'':
                    continue
                else:
                   cur_movie[key] = cur_movie[key].strip()
            if key in [u'rating']:
                if cur_movie[key] == 0.0 or cur_movie[key] == u'0.0':
                    continue
            if key in [u'popularity', u'budget', u'runtime', u'revenue', ]:
                if cur_movie[key] == 0 or cur_movie[key] == u'0':
                    continue
            if key == u'imdb_id':
                cur_movie[key] = cur_movie[key][2:]
            if key == u'released':
                translated[u'year'] = cur_movie[key][:4]
            if self.config[u'mythtv_translation'].has_key(key):
                translated[self.config[u'mythtv_translation'][key]] = cur_movie[key]
            else:
                translated[key] = cur_movie[key]
            for key in translated.keys():
                if translated[key]:
                    translated[key] = translated[key].replace(u'\n',u' ') # Remove any line-feeds from data
        return translated
    # end _mythtvDetails()


    def searchTitle(self, title, lang=False):
        """Searches for a film by its title.
        Returns SearchResults (a list) containing all matches (Movie instances)
        """
        if lang: # Override language
            URL = self.config[u'urls'][u'movie.search'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'movie.search']
        org_title = title
        title = urllib.quote(title.encode("utf-8"))
        url = URL % (title)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        # Display a XML query if it was requested
        if self.xml:
            self.queryXML(url)

        etree = XmlHandler(url).getEt()
        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the title (%s)' % org_title)

        search_results = SearchResults()
        for cur_result in etree.find(u"movies").findall(u"movie"):
            if cur_result == None:
                continue
            cur_movie = self._tmdbDetails(cur_result)
            search_results.append(cur_movie)
        if not len(search_results):
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the title (%s)' % org_title)

        # Check if no ui has been requested and therefore just return the raw search results.
        if (self.config['interactive'] == False and self.config['select_first'] == False and self.config['custom_ui'] == None) or not len(search_results):
            return search_results

        # Select the first result (most likely match) or invoke user interaction to select the correct movie
        if self.config['custom_ui'] is not None:
            self.log.debug("Using custom UI %s" % (repr(self.config['custom_ui'])))
            ui = self.config['custom_ui'](config = self.config, log = self.log, searchTerm = org_title)
        else:
            if not self.config['interactive']:
                self.log.debug('Auto-selecting first search result using BaseUI')
                ui = BaseUI(config = self.config, log = self.log, searchTerm = org_title)
            else:
                self.log.debug('Interactivily selecting movie using ConsoleUI')
                ui = ConsoleUI(config = self.config, log = self.log, searchTerm = org_title)
        return ui.selectMovieOrPerson(search_results)
    # end searchTitle()

    def searchTMDB(self, by_id, lang=False):
        """Searches for a film by its TMDB id number.
        Returns a movie data dictionary
        """
        if lang: # Override language
            URL = self.config[u'urls'][u'tmdbid.search'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'tmdbid.search']
        id_url = urllib.quote(by_id.encode("utf-8"))
        url = URL % (id_url)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        # Display a XML video details if it was requested
        if self.xml:
            self.videoXML(url)

        etree = XmlHandler(url).getEt()

        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the TMDB number (%s)' % by_id)
        if etree.find(u"movies").find(u"movie"):
            return self.movieDetails(etree.find(u"movies").find(u"movie"))
        else:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the TMDB number (%s)' % by_id)

    def searchIMDB(self, by_id, lang=False):
        """Searches for a film by its IMDB number.
        Returns a movie data dictionary
        """
        if lang: # Override language
            URL = self.config[u'urls'][u'imdb.search'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'imdb.search']
        id_url = urllib.quote(by_id.encode("utf-8"))
        url = URL % (id_url)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        # Display a XML video details if it was requested
        if self.xml:
            self.videoXML(url)

        etree = XmlHandler(url).getEt()

        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the IMDB number (%s)' % by_id)
        if etree.find(u"movies").find(u"movie") == None:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the IMDB number (%s)' % by_id)

        if self._tmdbDetails(etree.find(u"movies").find(u"movie")).has_key(u'id'):
            return self.searchTMDB(self._tmdbDetails(etree.find(u"movies").find(u"movie"))[u'id'],)
        else:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the IMDB number (%s)' % by_id)

    def searchHash(self, by_hash, lang=False):
        """Searches for a film by its TMDB id number.
        Returns a movie data dictionary
        """
        if lang: # Override language
            URL = self.config[u'urls'][u'hash.info'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'hash.info']
        id_url = urllib.quote(by_hash.encode("utf-8"))
        url = URL % (id_url)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        # Display a XML video details if it was requested
        if self.xml:
            self.videoXML(url)

        etree = XmlHandler(url).getEt()

        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the hash value (%s)' % by_hash)
        if etree.find(u"movies").find(u"movie"):
            return self.movieDetails(etree.find(u"movies").find(u"movie"))
        else:
            raise TmdbMovieOrPersonNotFound(u'No Movies matching the hash value (%s)' % by_hash)


    def searchImage(self, by_id, lang=False, filterout=False):
        """Searches for a film's images URLs by TMDB number.
        Returns a image URL dictionary
        """
        if lang: # Override language
            URL = self.config[u'urls'][u'image.search'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'image.search']
        id_url = urllib.quote(by_id.encode("utf-8"))
        url = URL % (id_url)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        etree = XmlHandler(url).getEt()
        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No Movie matching the TMDB number (%s)' % by_id)
        if not etree.find(u"movies").find(u"movie"):
            raise TmdbMovieOrPersonNotFound(u'No Movie matching the TMDB number (%s)' % by_id)

        cur_poster = {}
        cur_backdrop = {}

        for item in etree.find(u"movies").find(u"movie").getchildren():
            if item.tag.lower() == u"images":
                for image in item.getchildren():
                    if image.tag == u"poster":
                        for poster in image.getchildren():
                            key = poster.get('size')
                            if key in cur_poster.keys():
                                if poster.get('url'):
                                    cur_poster[key.strip()].append(poster.get('url').strip())
                            else:
                                if poster.get('url'):
                                    cur_poster[key.strip()] = [poster.get('url').strip()]
                    elif image.tag == u"backdrop":
                        for backdrop in image.getchildren():
                            key = backdrop.get('size')
                            if key in cur_backdrop.keys():
                                if backdrop.get('url'):
                                    cur_backdrop[key.strip()].append(backdrop.get('url').strip())
                            else:
                                if backdrop.get('url'):
                                    cur_backdrop[key.strip()] = [backdrop.get('url').strip()]
        images = {}
        if cur_poster.keys():
            for cur_size in [u"original", u"mid", u"cover", u"thumb"]:
                keyvalue = u'poster_%s' % cur_size
                tmp = u''
                if cur_size in cur_poster:
                    for data in cur_poster[cur_size]:
                        tmp+=u'%s,' % data
                if len(tmp):
                    tmp=tmp[:-1]
                    images[keyvalue] = tmp

        if cur_backdrop.keys():
            for cur_size in [u"original", u"mid", u"cover", u"thumb"]:
                keyvalue = u'fanart_%s' % cur_size
                tmp = u''
                if cur_size in cur_backdrop:
                    for data in cur_backdrop[cur_size]:
                        tmp+= u'%s,' % data
                if len(tmp):
                    tmp=tmp[:-1]
                    images[keyvalue] = tmp
        if filterout:
            if images.has_key(filterout):
                return images[filterout]
            else:
                return u''
        else:
            return images
    # end searchImage()

    def searchPeople(self, name, lang=False):
        """Searches for a People by name.
        Returns a list if matching persons and a dictionary of their attributes
        """
        tmp_name = name.strip().replace(u' ',u'+')
        try:
            id_url = urllib.quote(tmp_name.encode("utf-8"))
        except (UnicodeEncodeError, TypeError):
            id_url = urllib.quote(tmp_name)
        if lang: # Override language
            URL = self.config[u'urls'][u'person.search'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'person.search']
        url = URL % (id_url)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        etree = XmlHandler(url).getEt()
        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No People matches found for the name (%s)' % name)
        if not etree.find(u"people").find(u"person"):
            raise TmdbMovieOrPersonNotFound(u'No People matches found for the name (%s)' % name)

        people = []
        for item in etree.find(u"people").getchildren():
            if item.tag == u"person":
                person = {}
                for p in item.getchildren():
                    if p.tag != u'images':
                        if p.text != None:
                            person[p.tag] = self.textUtf8(p.text.strip())
                    elif len(p.getchildren()):
                        person[p.tag] = {}
                        for image in p.getchildren():
                            if image.get('url') != None and image.get('size') != None:
                                person[p.tag][image.get('size')] = image.get('url').strip()
                people.append(person)

        # Check if no ui has been requested and therefore just return the raw search results.
        if (self.config['interactive'] == False and self.config['select_first'] == False and self.config['custom_ui'] == None) or not len(people):
            return people

        # Select the first result (most likely match) or invoke user interaction to select the correct movie
        if self.config['custom_ui'] is not None:
            self.log.debug("Using custom UI %s" % (repr(self.config['custom_ui'])))
            ui = self.config['custom_ui'](config = self.config, log = self.log, searchTerm = name.strip())
        else:
            if not self.config['interactive']:
                self.log.debug('Auto-selecting first search result using BaseUI')
                ui = BaseUI(config = self.config, log = self.log, searchTerm = name.strip())
            else:
                self.log.debug('Interactivily selecting movie using ConsoleUI')
                ui = ConsoleUI(config = self.config, log = self.log, searchTerm = name.strip())
        return ui.selectMovieOrPerson(people)
    # end searchPeople()


    def personInfo(self, by_id, lang=False):
        """Retrieve a Person's informtaion by their TMDB id.
        Returns dictionary of the persons information attributes
        """
        if lang: # Override language
            URL = self.config[u'urls'][u'person.info'].replace(self.lang_url % self.config['language'], self.lang_url % lang)
        else:
            URL = self.config[u'urls'][u'person.info']
        id_url = urllib.quote(by_id)
        url = URL % (id_url)
        if self.config['debug_enabled']:        # URL so that raw TMDB XML data can be viewed in a browser
            sys.stderr.write(u'\nDEBUG: XML URL:%s\n\n' % url)

        etree = XmlHandler(url).getEt()
        if etree is None:
            raise TmdbMovieOrPersonNotFound(u'No Person match found for the Person ID (%s)' % by_id)
        if not etree.find(u"people").find(u"person"):
            raise TmdbMovieOrPersonNotFound(u'No Person match found for the Person ID (%s)' % by_id)
        person = {}
        elements = ['also_known_as', 'filmography', 'images' ]

        for elem in etree.find(u"people").find(u"person").getchildren():
            if elem.tag in elements:
                if elem.tag == 'also_known_as':
                    alias = []
                    for a in elem.getchildren():
                        if a.text:
                            alias.append(self.textUtf8(a.text.strip()).replace(u'\n',u' '))
                    if len(alias):
                        person[elem.tag] = alias
                elif elem.tag == 'filmography':
                    movies = []
                    for a in elem.getchildren():
                        details = {}
                        for get in ['url', 'name', 'character', 'job', 'id']:
                            if a.get(get):
                                details[get] = a.get(get).strip()
                        if len(details):
                            movies.append(details)
                    if len(movies):
                        person[elem.tag] = movies
                elif len(elem.getchildren()):
                    images = {}
                    for image in elem.getchildren():
                        if image.get('url') == None:
                            continue
                        (dirName, fileName) = os.path.split(image.get('url'))
                        (fileBaseName, fileExtension) = os.path.splitext(fileName)
                        if not fileExtension[1:] in self.config[u'image_extentions']:
                            continue
                        if image.get('size') in images.keys():
                            if image.get('url'):
                                images[image.get('size')]+= u',%s' % image.get('url').strip()
                        else:
                            if image.get('url') and image.get('size'):
                                images[image.get('size')] = u'%s' % image.get('url').strip()
                    if len(images):
                        person[elem.tag] = images
            else:
                if elem.text:
                    person[elem.tag] = self.textUtf8(elem.text.strip()).replace(u'\n',u' ')
        return person
    # end personInfo()

    def lastUpdated(self, context, *inputArgs):
        '''Convert a date/time string in a specified format into a lastupdated. The default is the
        Universal Metadata Format item format
        return the formatted lastupdated string
        return on error return the original date string
        '''
        args = []
        for arg in inputArgs:
            args.append(arg)
        if args[0] == u'':
            return datetime.datetime.now().strftime(self.pubDateFormat)
        index = args[0].find('+')
        if index != -1:
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
            sys.stderr.write(u'! Error: lastupdated variables(%s) error(%s)\n' % (args, err))
        return args[0]
    # end lastUpdated()

    def supportedJobs(self, context, *inputArgs):
        '''Validate that the job category is supported by the
        Universal Metadata Format item format
        return True is supported
        return False if not supported
        '''
        for job in inputArgs[0]:
            if job.lower() in self.supportedJobList:
                return True
        return False
    # end supportedJobs()

    def verifyName(self, context, *inputArgs):
        '''Verify a tag name is supported in the Universal Metadata Format. Handles one or more names.
        When there is a tuple list of names only one in the list needs to be a supported tag.
        return True when the tag is supported
        return False when the tag is not supported
        '''
        for tag in inputArgs[0]:
            if tag.lower() in self.tagTranslations.keys():
                return True
        return False
    # end verifyName()

    def translateName(self, context, *inputArgs):
        '''Translate a tag name into the Universal Metadata Format item equivalent
        return the translated tag equivalent
        return False if the tag has no translation value
        '''
        name = inputArgs[0]
        name = name.lower()
        if name in self.tagTranslations.keys():
            return self.tagTranslations[name]
        return False
    # end translateName()

    def titleElement(self, context, *inputArgs):
        '''Check the title to see if it should be split into a title and subtitle
        return a list containing an element with attributes for title and subtitle
        '''
        name = inputArgs[0]
        titleElement = eTree.XML(u'<title></title>')
        titleElement.attrib['title'] = name
        for separator in self.separatorTitleSubtitle:
            index = name.find(separator)
            if index == -1:
                continue
            titleElement.attrib['title'] = name[:index].strip()
            titleElement.attrib['subtitle'] = name[index+len(separator):].strip()
        return [titleElement]
    # end titleElement()

    def makeImageElements(self, context, *inputArgs):
        '''Take list of image elements and create Universal Metadata Format item image elements
        return a list of processed image elements
        '''
        imageList = []
        currentImageID = None
        imageElement = None
        for tmdbImage in inputArgs[0]:
            if not tmdbImage.attrib['size'] in ['original', 'cover', 'thumb']:
                continue
            if currentImageID != None and currentImageID != tmdbImage.attrib['id']:
                imageList.append(deepcopy(imageElement))
                imageElement = None
            if imageElement == None:
                imageElement = eTree.XML(u'<image></image>')
                imageElement.attrib['type'] = self.translateName('dummy', ((tmdbImage.attrib['type'])))
                currentImageID = tmdbImage.attrib['id']
            if tmdbImage.attrib['size'] == 'original':
                imageElement.attrib['url'] = tmdbImage.attrib['url']
                imageElement.attrib['width'] = tmdbImage.attrib['width']
                imageElement.attrib['height'] = tmdbImage.attrib['height']
                continue
            if tmdbImage.attrib['size'] == 'cover' and imageElement.attrib['type'] == 'coverart':
                imageElement.attrib['thumb'] = tmdbImage.attrib['url']
                continue
            if tmdbImage.attrib['size'] == 'thumb' and imageElement.attrib['type'] == 'fanart':
                imageElement.attrib['thumb'] = tmdbImage.attrib['url']
                continue

        # Add any last image elements
        if imageElement != None:
            imageList.append(deepcopy(imageElement))
        return imageList
    # end makeImageElements()

    def replaceText(self, context, *inputArgs):
        '''Replace text in a string
        return the text after a replace was performed
        '''
        text = inputArgs[0][0]
        return text.replace(inputArgs[1], inputArgs[2])
    # end replaceText()

    def buildFuncDict(self):
        """ Build a dictionary of the XPath extention function for the XSLT stylesheets
        Returns nothing
        """
        self.FuncDict = {
            'lastUpdated': self.lastUpdated,
            'supportedJobs': self.supportedJobs,
            'verifyName': self.verifyName,
            'translateName': self.translateName,
            'makeImageElements': self.makeImageElements,
            'titleElement': self.titleElement,
            'replaceText': self.replaceText,
            }
        return
    # end buildFuncDict()

    def queryXML(self, url):
        """Display a Movie query in XML format:
        http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
        Returns nothing
        """
        try:
            queryResult = eTree.parse(url, parser=self.xmlParser)
        except Exception, errmsg:
            sys.stderr.write(u"! Error: Invalid XML was received from themoviedb.org (%s)\n" % errmsg)
            sys.exit(1)

        queryXslt = eTree.XSLT(eTree.parse(u'%s/XSLT/tmdbQuery.xsl' % self.baseProcessingDir))
        tmdbXpath = eTree.FunctionNamespace('http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format')
        tmdbXpath.prefix = 'tmdbXpath'
        self.buildFuncDict()
        for key in self.FuncDict.keys():
            tmdbXpath[key] = self.FuncDict[key]

        items = queryXslt(queryResult)

        if items.getroot() != None:
            if len(items.xpath('//item')):
                sys.stdout.write(eTree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
        sys.exit(0)
    # end queryXML()

    def videoXML(self, url):
        """Display a Movie details in XML format:
        http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format
        Returns nothing
        """
        try:
            videoResult = eTree.parse(url, parser=self.xmlParser)
        except Exception, errmsg:
            sys.stderr.write(u"! Error: Invalid XML was received from themoviedb.org (%s)\n" % errmsg)
            sys.exit(1)

        videoXslt = eTree.XSLT(eTree.parse(u'%s/XSLT/tmdbVideo.xsl' % self.baseProcessingDir))
        tmdbXpath = eTree.FunctionNamespace('http://www.mythtv.org/wiki/MythTV_Universal_Metadata_Format')
        tmdbXpath.prefix = 'tmdbXpath'
        self.buildFuncDict()
        for key in self.FuncDict.keys():
            tmdbXpath[key] = self.FuncDict[key]
        items = videoXslt(videoResult)

        if items.getroot() != None:
            if len(items.xpath('//item')):
                sys.stdout.write(eTree.tostring(items, encoding='UTF-8', method="xml", xml_declaration=True, pretty_print=True, ))
        sys.exit(0)
    # end videoXML()

# end MovieDb class

class Videos(MovieDb):
    """A super class of the MovieDB functionality for the MythTV Netvision plugin functionality.
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.
    """
    def __init__(self, apikey, mythtv, interactive, select_first, debug, custom_ui, language, search_all_languages, ):
        """Pass the configuration options
        """
        super(Videos, self).__init__(apikey, mythtv, interactive, select_first, debug, custom_ui, language, search_all_languages, )
    # end __init__()

    error_messages = {'TmdHttpError': u"! Error: A connection error to themoviedb.org was raised (%s)\n", 'TmdXmlError': u"! Error: Invalid XML was received from themoviedb.org (%s)\n", 'TmdBaseError': u"! Error: A user interface error was raised (%s)\n", 'TmdbUiAbort': u"! Error: A user interface input error was raised (%s)\n", }
    key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'item_author': 'item_author', 'releasedate': 'item_pubdate', 'overview': 'item_description', 'url': 'item_link', 'trailer': 'item_url', 'runtime': 'item_duration', 'userrating': 'item_rating', 'width': 'item_width', 'height': 'item_height', 'language': 'item_lang'}]

    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        def displayMovieData(data):
            '''Parse movie trailer metadata
            return None if no valid data
            return a dictionary of Movie trailer metadata
            '''
            if data == None:
                return None
            if not 'trailer' in data.keys():
                return None
            if data['trailer'] == u'':
                return None

            trailer_data = {}
            for key in self.key_translation[1].keys():
                if key in data.keys():
                    if key == self.thumbnails:
                        thumbnail = data[key].split(u',')
                        trailer_data[self.key_translation[1][key]] = thumbnail[0]
                        continue
                    if key == 'url':    # themoviedb.org always uses Youtube for trailers
                        trailer_data[self.key_translation[1][key]] = data['trailer']
                        continue
                    if key == 'releasedate':
                        c = time.strptime(data[key],"%Y-%m-%d")
                        trailer_data[self.key_translation[1][key]] = time.strftime("%a, %d %b %Y 00:%M:%S GMT",c) # <pubDate>Tue, 14 Jul 2009 17:05:00 GMT</pubDate> <pubdate>Wed, 24 Jun 2009 03:53:00 GMT</pubdate>
                        continue
                    trailer_data[self.key_translation[1][key]] = data[key]
                else:
                    trailer_data[self.key_translation[1][key]] = u''
            trailer_data[self.key_translation[1][u'overview']] = self.overview

            return trailer_data
        # end displayMovieData()

        def movieData(tmdb_id):
            '''Get Movie data by IMDB or TMDB number and return the details
            '''
            try:
                return displayMovieData(self.searchTMDB(tmdb_id))
            except TmdbMovieOrPersonNotFound, msg:
                sys.stderr.write(u"%s\n" % msg)
                return None
            except TmdHttpError, msg:
                sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
                sys.exit(1)
            except TmdXmlError, msg:
                sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
                sys.exit(1)
            except TmdBaseError, msg:
                sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
                sys.exit(1)
            except TmdbUiAbort, msg:
                sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
                sys.exit(1)
            except Exception, e:
                sys.stderr.write(u"! Error: Unknown error during a Movie (%s) information lookup\nError(%s)\n" % (tmdb_id, e))
                sys.exit(1)
        # end movieData()

        try:
            data = self.searchTitle(title)
        except TmdbMovieOrPersonNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            return []
        except TmdHttpError, msg:
            sys.stderr.write(self.error_messages['TmdHttpError'] % msg)
            sys.exit(1)
        except TmdXmlError, msg:
            sys.stderr.write(self.error_messages['TmdXmlError'] % msg)
            sys.exit(1)
        except TmdBaseError, msg:
            sys.stderr.write(self.error_messages['TmdBaseError'] % msg)
            sys.exit(1)
        except TmdbUiAbort, msg:
            sys.stderr.write(self.error_messages['TmdbUiAbort'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Movie Trailer search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        if data == None:
            return None

        # Set the size of the thumbnail graphics that will be returned
        self.thumbnails = 'mid'
        self.key_translation[1][self.thumbnails] = 'item_thumbnail'

        # Channel details and search results
        channel = {'channel_title': u'themoviedb.org', 'channel_link': u'http://themoviedb.org', 'channel_description': u'themoviedb.org is an open “wiki-style” movie database', 'channel_numresults': 0, 'channel_returned': self.page_limit, u'channel_startindex': 0}

        trailers = []
        trailer_total = 0
        starting_index = (int(pagenumber)-1) * self.page_limit
        for match in data:
            if match.has_key('overview'):
                self.overview = match['overview']
            else:
                self.overview = u''
            trailer = movieData(match[u'id'])
            if trailer:
                if starting_index != 0:
                    if not trailer_total > starting_index:
                        continue
                trailers.append(trailer)
                trailer_total+=1
                if self.page_limit == trailer_total:
                    break
        channel['channel_numresults'] = trailer_total
        startindex = trailer_total + starting_index
        if startindex < int(pagenumber) * self.page_limit:
            channel['channel_startindex'] = startindex + 1
        else:
            channel['channel_startindex'] = (int(pagenumber) * self.page_limit) + startindex - 1
        return [[channel, trailers]]
    # end searchForVideos()
# end Videos() class

def main():
    """Simple example of using tmdb_api - it just
    searches for any movies with the word "Avatar" in its tile and returns a list of matches with their summary
    information in a dictionary. And gets movie details using an IMDB# and a TMDB#
    """
    # themoviedb.org api key given by Travis Bell for Mythtv
    api_key = "c27cb71cff5bd76e1a7a009380562c62"
    tmdb = MovieDb(api_key)
    # Output a dictionary of matching movie titles
    print tmdb.searchTitle(u'Avatar')
    print
    # Output a dictionary of matching movie details for IMDB number '0401792'
    print tmdb.searchIMDB(u'0401792')
    # Output a dictionary of matching movie details for TMDB number '19995'
    print tmdb.searchTMDB(u'19995')
# end main()

if __name__ == '__main__':
    main()
