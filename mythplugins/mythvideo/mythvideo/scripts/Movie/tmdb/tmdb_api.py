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

__version__="v0.1.4"
# 0.1.0 Initial development
# 0.1.1 Alpha Release
# 0.1.2 Added removal of any line-feeds from data
# 0.1.3 Added display of URL to TMDB XML when debug was specified
#       Added check and skipping any empty data from TMDB
# 0.1.4 More data validation added (e.g. valid image file extentions)
#       More data massaging added.

import os, struct, sys
import urllib, urllib2
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
        except IOError:
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
        if not fileExtension[1:] in self.image_extentions:
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

class MovieDb:
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

        custom_ui (tvdb_ui.BaseUI subclass):
            A callable subclass of tvdb_ui.BaseUI (overrides interactive option)

        language (2 character language abbreviation):
            The language of the returned data. Is also the language search
            uses. Default is "en" (English). For full list, run..

            >>> MovieDb().config['valid_languages'] #doctest: +ELLIPSIS
            ['da', 'fi', 'nl', ...]

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

        # The supported languages have not been published or enabled at this time.
        # List of language from ???????
        # Hard-coded here as it is realtively static, and saves another HTTP request, as
        # recommended on ?????
        #self.config['valid_languages'] = [
        #    "da", "fi", "nl", "de", "it", "es", "fr","pl", "hu","el","tr",
        #    "ru","he","ja","pt","zh","cs","sl", "hr","ko","en","sv","no"
        #]

        # ONLY ENGISH is supported at this time
        self.config['language'] = "en"
        #if language is None:
        #    self.config['language'] = "en"
        #elif language not in self.config['valid_languages']:
        #    raise ValueError("Invalid language %s, options are: %s" % (
        #        language, self.config['valid_languages']
        #    ))
        #else:
        #    self.config['language'] = language

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
        self.config[u'mythtv_translation'] = {u'actor': u'cast', u'backdrop': u'fanart', u'categories': u'genres', u'director':  u'director', u'id': u'inetref', u'name': u'title', u'overview': u'plot', u'rating': u'userrating', u'poster': u'coverart', u'production_countries': u'countries', u'released': u'releasedate', u'runtime': u'runtime', u'url': u'url', u'imdb_id': u'imdb', }
        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions
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
            ui = self.config['custom_ui'](config = self.config, log = self.log)
        else:
            if not self.config['interactive']:
                self.log.debug('Auto-selecting first search result using BaseUI')
                ui = BaseUI(config = self.config, log = self.log)
            else:
                self.log.debug('Interactivily selecting movie using ConsoleUI')
                ui = ConsoleUI(config = self.config, log = self.log)
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
                                cur_poster[key.strip()].append(poster.get('url').strip())
                            else:
                                cur_poster[key.strip()] = [poster.get('url').strip()]
                    elif image.tag == u"backdrop":
                        for backdrop in image.getchildren():
                            key = backdrop.get('size')
                            if key in cur_backdrop.keys():
                                cur_backdrop[key.strip()].append(backdrop.get('url').strip())
                            else:
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
                        person[p.tag] = self.textUtf8(p.text.strip())
                    elif len(p.getchildren()):
                        person[p.tag] = {}
                        for image in p.getchildren():
                            person[p.tag][image.get('size')] = image.get('url').strip()
                people.append(person)

        # Check if no ui has been requested and therefore just return the raw search results.
        if (self.config['interactive'] == False and self.config['select_first'] == False and self.config['custom_ui'] == None) or not len(people):
            return people

        # Select the first result (most likely match) or invoke user interaction to select the correct movie
        if self.config['custom_ui'] is not None:
            self.log.debug("Using custom UI %s" % (repr(self.config['custom_ui'])))
            ui = self.config['custom_ui'](config = self.config, log = self.log)
        else:
            if not self.config['interactive']:
                self.log.debug('Auto-selecting first search result using BaseUI')
                ui = BaseUI(config = self.config, log = self.log)
            else:
                self.log.debug('Interactivily selecting movie using ConsoleUI')
                ui = ConsoleUI(config = self.config, log = self.log)
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
                    if alias:
                        person[elem.tag] = alias
                elif elem.tag == 'filmography':
                    movies = []
                    for a in elem.getchildren():
                        details = {}
                        for get in ['url', 'name', 'character', 'job', 'id']:
                            details[get] = a.get(get).strip()
                        if details:
                            movies.append(details)
                    if movies:
                        person[elem.tag] = movies
                elif len(elem.getchildren()):
                    images = {}
                    for image in elem.getchildren():
                        (dirName, fileName) = os.path.split(image.get('url'))
                        (fileBaseName, fileExtension) = os.path.splitext(fileName)
                        if not fileExtension[1:] in self.config[u'image_extentions']:
                            continue
                        if image.get('size') in images.keys():
                            images[image.get('size')]+= u',%s' % image.get('url').strip()
                        else:
                            images[image.get('size')] = u'%s' % image.get('url').strip()
                    if images:
                        person[elem.tag] = images
            else:
                if elem.text:
                    person[elem.tag] = self.textUtf8(elem.text.strip()).replace(u'\n',u' ')
        return person
    # end personInfo()

# end MovieDb

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
