#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: tmdb_api.py    Simple-to-use Python interface to TMDB's API v3
# Python Library
# Author: Raymond Wagner
# Purpose: This Python library is intended to provide a series of classes
#          and methods for search and retrieval of text metadata and image
#          URLs from TMDB.
#          Preliminary API specifications can be found at
#          http://help.themoviedb.org/kb/api/about-3
# License: Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-----------------------

__title__ = "tmdb_api - Simple-to-use Python interface to TMDB's API v3 "+\
            "(www.themoviedb.org)"
__author__ = "Raymond Wagner"
__purpose__ = """
This Python library is intended to provide a series of classes and methods
for search and retrieval of text metadata and image URLs from TMDB.
Preliminary API specifications can be found at
http://help.themoviedb.org/kb/api/about-3"""

__version__="v0.4.1"
# 0.1.0 Initial development
# 0.2.0 Add caching mechanism for API queries
# 0.2.1 Temporary work around for broken search paging
# 0.3.0 Rework backend machinery for managing OO interface to results
# 0.3.1 Add collection support
# 0.3.2 Remove MythTV key from results.py
# 0.3.3 Add functional language support
# 0.3.4 Re-enable search paging
# 0.3.5 Add methods for grabbing current, popular, and top rated movies
# 0.3.6 Rework paging mechanism
# 0.3.7 Generalize caching mechanism, and allow controllability
# 0.4.0 Add full locale support (language and country) and optional fall through
# 0.4.1 Add custom classmethod for dealing with IMDB movie IDs

from request import set_key, Request
from util import Datapoint, Datalist, Datadict, Element
from pager import PagedRequest
from locales import get_locale, set_locale
from tmdb_exceptions import *

import json
import urllib
import urllib2
import datetime

DEBUG = False

class Configuration( Element ):
    images = Datapoint('images')
    def _populate(self):
        return Request('configuration')
Configuration = Configuration()

def searchMovie(query, locale=None, adult=False):
    return MovieSearchResult(
                    Request('search/movie', query=query, include_adult=adult),
                    locale=locale)

class MovieSearchResult( PagedRequest ):
    """Stores a list of search matches."""
    _name = None
    def __init__(self, request, locale=None):
        if locale is None:
            locale = get_locale()
        super(MovieSearchResult, self).__init__(
                                request.new(language=locale.language),
                                lambda x: Movie(raw=x, locale=locale))
    
    def __repr__(self):
        name = self._name if self._name else self._request._kwargs['query']
        return u"<Search Results: {0}>".format(name)

def searchPerson(query):
    return PeopleSearchResult(Request('search/person', query=query))

class PeopleSearchResult( PagedRequest ):
    """Stores a list of search matches."""
    def __init__(self, request):
        super(PeopleSearchResult, self).__init__(request,
                                lambda x: Person(raw=x))

    def __repr__(self):
        return u"<Search Results: {0}>".format(self._request._kwargs['query'])

class Image( Element ):
    filename        = Datapoint('file_path', initarg=1,
                                handler=lambda x: x.lstrip('/'))
    aspectratio     = Datapoint('aspect_ratio')
    height          = Datapoint('height')
    width           = Datapoint('width')
    language        = Datapoint('iso_639_1')

    def sizes(self):
        return ['original']

    def geturl(self, size='original'):
        if size not in self.sizes():
            raise TMDBImageSizeError
        url = Configuration.images['base_url'].rstrip('/')
        return url+'/{0}/{1}'.format(size, self.filename)

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.filename}'>".format(self)

class Backdrop( Image ):
    def sizes(self):
        return Configuration.images['backdrop_sizes']
class Poster( Image ):
    def sizes(self):
        return Configuration.images['poster_sizes']
class Profile( Image ):
    def sizes(self):
        return Configuration.images['profile_sizes']

class AlternateTitle( Element ):
    country     = Datapoint('iso_3166_1')
    title       = Datapoint('title')

class Person( Element ):
    id          = Datapoint('id', initarg=1)
    name        = Datapoint('name')
    biography   = Datapoint('biography')
    dayofbirth  = Datapoint('birthday', default=None, handler=lambda x: \
                                         datetime.datetime.strptime(x, '%Y-%m-%d'))
    dayofdeath  = Datapoint('deathday', default=None, handler=lambda x: \
                                         datetime.datetime.strptime(x, '%Y-%m-%d'))
    homepage    = Datapoint('homepage')
    birthplace  = Datapoint('place_of_birth')
    profile     = Datapoint('profile_path', handler=Profile, raw=False)

    def __repr__(self):
        return u"<{0} '{1.name}' at {2}>".\
                        format(self.__class__.__name__, self, hex(id(self)))

    def _populate(self):
        return Request('person/{0}'.format(self.id))
    def _populate_credits(self):
        return Request('person/{0}/credits'.format(self.id), language=self._locale.language)
    def _populate_images(self):
        return Request('person/{0}/images'.format(self.id))

    roles       = Datalist('cast', handler=lambda x: ReverseCast(raw=x), poller=_populate_credits)
    crew        = Datalist('crew', handler=lambda x: ReverseCrew(raw=x), poller=_populate_credits)
    profiles    = Datalist('profiles', handler=Profile, poller=_populate_images)

class Cast( Person ):
    character   = Datapoint('character')
    order       = Datapoint('order')

    def __repr__(self):
        return u"<{0} '{1.name}' as '{1.character}'>".\
                        format(self.__class__.__name__, self)

class Crew( Person ):
    job         = Datapoint('job')
    department  = Datapoint('department')

    def __repr__(self):
        return u"<{0.__class__.__name__} '{1.name}','{1.job}'>".format(self)

class Keyword( Element ):
    id   = Datapoint('id')
    name = Datapoint('name')

    def __repr__(self):
        return u"<{0.__class__.__name__} {0.name}>".format(self)

class Release( Element ):
    certification   = Datapoint('certification')
    country         = Datapoint('iso_3166_1')
    releasedate     = Datapoint('release_date', handler=lambda x: \
                                         datetime.datetime.strptime(x, '%Y-%m-%d'))
    def __repr__(self):
        return u"<Release {0.country}, {0.releasedate}>".format(self)

class Trailer( Element ):
    name    = Datapoint('name')
    size    = Datapoint('size')
    source  = Datapoint('source')

class YoutubeTrailer( Trailer ):
    def geturl(self):
        return "http://www.youtube.com/watch?v={0}".format(self.source)

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self)

class AppleTrailer( Element ):
    name    = Datapoint('name')
    sources = Datadict('sources', handler=Trailer, attr='size')

    def sizes(self):
        return self.sources.keys()

    def geturl(self, size=None):
        if size is None:
            # sort assuming ###p format for now, take largest resolution
            size = str(sorted([int(size[:-1]) for size in self.sources])[-1])+'p'
        return self.sources[size].source

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self)

class Translation( Element ):
    name          = Datapoint('name')
    language      = Datapoint('iso_639_1')
    englishname   = Datapoint('english_name')

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}' ({0.language})>".format(self)

class Genre( Element ):
    id      = Datapoint('id')
    name    = Datapoint('name')

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self)

class Studio( Element ):
    id      = Datapoint('id')
    name    = Datapoint('name')

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self)

class Country( Element ):
    code    = Datapoint('iso_3166_1')
    name    = Datapoint('name')

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self)

class Language( Element ):
    code    = Datapoint('iso_639_1')
    name    = Datapoint('name')

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self)

class Movie( Element ):
    @classmethod
    def latest(cls):
        req = Request('latest/movie')
        req.lifetime = 600
        return cls(raw=req.readJSON())

    @classmethod
    def nowplaying(cls, locale=None):
        res = MovieSearchResult(Request('movie/now-playing'), locale=locale)
        res._name = 'Now Playing'
        return res

    @classmethod
    def mostpopular(cls, locale=None):
        res = MovieSearchResult(Request('movie/popular'), locale=locale)
        res._name = 'Popular'
        return res

    @classmethod
    def toprated(cls, locale=None):
        res = MovieSearchResult(Request('movie/top-rated'), locale=locale)
        res._name = 'Top Rated'
        return res

    @classmethod
    def fromIMDB(cls, imdbid, locale=None):
        try:
            # assume string
            if not imdbid.startswith('tt'):
                imdbid = "tt{0:0>7}".format(imdbid)
        except AttributeError:
            # assume integer
            imdbid = "tt{0:0>7}".format(imdbid)
        if locale is None:
            locale = get_locale()
        movie = cls(imdbid, locale=locale)
        movie._populate()
        return movie

    id              = Datapoint('id', initarg=1)
    title           = Datapoint('title')
    originaltitle   = Datapoint('original_title')
    tagline         = Datapoint('tagline')
    overview        = Datapoint('overview')
    runtime         = Datapoint('runtime')
    budget          = Datapoint('budget')
    revenue         = Datapoint('revenue')
    releasedate     = Datapoint('release_date', handler=lambda x: \
                                datetime.datetime.strptime(x, '%Y-%m-%d'))
    homepage        = Datapoint('homepage')
    imdb            = Datapoint('imdb_id')

    backdrop        = Datapoint('backdrop_path', handler=Backdrop, raw=False)
    poster          = Datapoint('poster_path', handler=Poster, raw=False)

    popularity      = Datapoint('popularity')
    userrating      = Datapoint('vote_average')
    votes           = Datapoint('vote_count')

    adult       = Datapoint('adult')
    collection  = Datapoint('belongs_to_collection', handler=lambda x: \
                                                        Collection(raw=x))
    genres      = Datalist('genres', handler=Genre)
    studios     = Datalist('production_companies', handler=Studio)
    countries   = Datalist('production_countries', handler=Country)
    languages   = Datalist('spoken_languages', handler=Language)

    def _populate(self):
        return Request('movie/{0}'.format(self.id), language=self._locale.language)
    def _populate_titles(self):
        return Request('movie/{0}/alternative_titles'.format(self.id), country=self._locale.country)
    def _populate_cast(self):
        return Request('movie/{0}/casts'.format(self.id))
    def _populate_images(self):
        return Request('movie/{0}/images'.format(self.id), language=self._locale.language)
    def _populate_keywords(self):
        return Request('movie/{0}/keywords'.format(self.id))
    def _populate_releases(self):
        return Request('movie/{0}/releases'.format(self.id))
    def _populate_trailers(self):
        return Request('movie/{0}/trailers'.format(self.id), language=self._locale.language)
    def _populate_translations(self):
        return Request('movie/{0}/translations'.format(self.id))

    alternate_titles = Datalist('titles', handler=AlternateTitle, poller=_populate_titles)
    cast             = Datalist('cast', handler=Cast, poller=_populate_cast, sort='order')
    crew             = Datalist('crew', handler=Crew, poller=_populate_cast)
    backdrops        = Datalist('backdrops', handler=Backdrop, poller=_populate_images)
    posters          = Datalist('posters', handler=Poster, poller=_populate_images)
    keywords         = Datalist('keywords', handler=Keyword, poller=_populate_keywords)
    releases         = Datadict('countries', handler=Release, poller=_populate_releases, attr='country')
    youtube_trailers = Datalist('youtube', handler=YoutubeTrailer, poller=_populate_trailers)
    apple_trailers   = Datalist('quicktime', handler=AppleTrailer, poller=_populate_trailers)
    translations     = Datalist('translations', handler=Translation, poller=_populate_translations)

    def __repr__(self):
        if self.title is not None:
            s = u"'{0}'".format(self.title)
        elif self.originaltitle is not None:
            s = u"'{0}'".format(self.originaltitle)
        else:
            s = u"'No Title'"
        if self.releasedate:
            s = u"{0} ({1})".format(s, self.releasedate.year)
        return u"<{0} {1}>".format(self.__class__.__name__, s).encode('utf-8')

class ReverseCast( Movie ):
    character   = Datapoint('character')

    def __repr__(self):
        if self.title is not None:
            s = u"'{0}'".format(self.title)
        elif self.originaltitle is not None:
            s = u"'{0}'".format(self.originaltitle)
        else:
            s = u"'No Title'"
        if self.releasedate:
            s = u"{0} ({1})".format(s, self.releasedate.year)
        return u"<{0.__class__.__name__} '{0.character}' on {1}>".format(self, s).encode('utf-8')

class ReverseCrew( Movie ):
    department  = Datapoint('department')
    job         = Datapoint('job')

    def __repr__(self):
        if self.title is not None:
            s = u"'{0}'".format(self.title)
        elif self.originaltitle is not None:
            s = u"'{0}'".format(self.originaltitle)
        else:
            s = u"'No Title'"
        if self.releasedate:
            s = u"{0} ({1})".format(s, self.releasedate.year)
        return u"<{0.__class__.__name__} '{0.job}' for {1}>".format(self, s).encode('utf-8')

class Collection( Element ):
    id       = Datapoint('id', initarg=1)
    name     = Datapoint('name')
    backdrop = Datapoint('backdrop_path', handler=Backdrop, raw=False)
    poster   = Datapoint('poster_path', handler=Poster, raw=False)
    members  = Datalist('parts', handler=Movie, sort='releasedate')

    def _populate(self):
        return Request('collection/{0}'.format(self.id), language=self._locale.language)

    def __repr__(self):
        return u"<{0.__class__.__name__} '{0.name}'>".format(self).encode('utf-8')
        return u"<{0} {1}>".format(self.__class__.__name__, s).encode('utf-8')

if __name__ == '__main__':
    set_key('c27cb71cff5bd76e1a7a009380562c62') #MythTV API Key
    DEBUG = True

    banner = 'tmdb_api interactive shell.'
    import code
    try:
        import readline, rlcompleter
    except:
        pass
    else:
        readline.parse_and_bind("tab: complete")
        banner += ' TAB completion available.'
    namespace = globals().copy()
    namespace.update(locals())
    code.InteractiveConsole(namespace).interact(banner)
