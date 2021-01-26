# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb3.py
# Python Script
# Author: Raymond Wagner
# Purpose: This python script is intended to translate lookups between the
#          TheMovieDB.org V3 API and MythTV's internal metadata format.
#               http://www.mythtv.org/wiki/MythVideo_Grabber_Script_Format
#               http://help.themoviedb.org/kb/api/about-3
# Code was originally in metadata/Movie/tmdb3.py.
# Moved here so it can be called for movies or TV
#-----------------------
__title__ = "TheMovieDB.org V3"
__author__ = "Raymond Wagner, Roland Ernst"
__version__ = "0.3.9"
# 0.1.0 Initial version
# 0.2.0 Add language support, move cache to home directory
# 0.3.0 Enable version detection to allow use in MythTV
# 0.3.1 Add --test parameter for proper compatibility with mythmetadatalookup
# 0.3.2 Add --area parameter to allow country selection for release date and
#       parental ratings
# 0.3.3 Use translated title if available
# 0.3.4 Add support for finding by IMDB under -D (simulate previous version)
# 0.3.5 Add debugging mode
# 0.3.6 Add handling for TMDB site and library returning null results in
#       search. This should only need to be a temporary fix, and should be
#       resolved upstream.
# 0.3.7 Add handling for TMDB site returning insufficient results from a
#       query
# 0.3.7.a : Added compatibiliy to python3, tested with python 3.6 and 2.7
# 0.3.8 Sort posters by system language or 'en', if not found for given language
# 0.3.9 Support TV lookup

# ~ from optparse import OptionParser
import sys
import signal

def print_etree(etostr):
    """lxml.etree.tostring is a bytes object in python3, and a str in python2.
    """
    if sys.version_info[0] == 2:
        sys.stdout.write(etostr)
    else:
        sys.stdout.write(etostr.decode())

def timeouthandler(signal, frame):
    raise RuntimeError("Timed out")

def buildSingle(inetref, opts):
    from MythTV.tmdb3.tmdb_exceptions import TMDBRequestInvalid
    from MythTV.tmdb3 import Movie, get_locale
    from MythTV import VideoMetadata
    from lxml import etree

    import locale as py_locale
    import re

    if re.match('^0[0-9]{6}$', inetref):
        movie = Movie.fromIMDB(inetref)
    else:
        movie = Movie(inetref)

    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity'],
               ['budget',       'budget'],      ['revenue',     'revenue']]
    m = VideoMetadata()
    for i,j in mapping:
        try:
            if getattr(movie, j):
                setattr(m, i, getattr(movie, j))
        except TMDBRequestInvalid:
            return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                            xml_declaration=True)

    if movie.title:
        m.title = movie.title

    releases = list(movie.releases.items())

# get the release date for the wanted country
# TODO if that is not part of the reply use the primary release date (Primary=true)
# if that is not part of the reply use whatever release date is first in list
# if there is not a single release date in the reply, then leave it empty
    if len(releases) > 0:
        if opts.country:
            # resort releases with selected country at top to ensure it
            # is selected by the metadata libraries
            r = list(zip(*releases))
            if opts.country in r[0]:
                index = r[0].index(opts.country)
                releases.insert(0, releases.pop(index))

        m.releasedate = releases[0][1].releasedate

    m.inetref = str(movie.id)
    if movie.collection:
        m.collectionref = str(movie.collection.id)
    if m.releasedate:
        m.year = m.releasedate.year
    for country, release in releases:
        if release.certification:
            m.certifications[country] = release.certification
    for genre in movie.genres:
        m.categories.append(genre.name)
    for studio in movie.studios:
        m.studios.append(studio.name)
    for country in movie.countries:
        m.countries.append(country.name)
    for cast in movie.cast:
        d = {'name':cast.name, 'character':cast.character, 'department':'Actors',
             'job':'Actor', 'url':'http://www.themoviedb.org/people/{0}'.format(cast.id)}
        if cast.profile: d['thumb'] = cast.profile.geturl()
        m.people.append(d)
    for crew in movie.crew:
        d = {'name':crew.name, 'job':crew.job, 'department':crew.department,
             'url':'http://www.themoviedb.org/people/{0}'.format(crew.id)}
        if crew.profile: d['thumb'] = crew.profile.geturl()
        m.people.append(d)
    for backdrop in movie.backdrops:
        m.images.append({'type':'fanart', 'url':backdrop.geturl(),
                        'thumb':backdrop.geturl(backdrop.sizes()[0]),
                        'height':str(backdrop.height),
                        'width':str(backdrop.width)})

    # tmdb already sorts the posters by language
    # if no poster of given language was found,
    # try to sort by system language and then by language "en"
    system_language = py_locale.getdefaultlocale()[0].split("_")[0]
    locale_language = get_locale().language
    if opts.debug:
        print("system_language : ", system_language)
        print("locale_language : ", locale_language)

    loc_posters = movie.posters
    if loc_posters[0].language != locale_language \
                    and locale_language != system_language:
        if opts.debug:
            print("1: No poster found for language '%s', trying to sort posters by '%s' :"
                    %(locale_language, system_language))
        loc_posters = sorted(movie.posters,
                    key = lambda x: x.language==system_language, reverse = True)

    if loc_posters[0].language != system_language \
                    and loc_posters[0].language != locale_language:
        if opts.debug:
            print("2: No poster found for language '%s', trying to sort posters by '%s' :"
                    %(system_language, "en"))
        loc_posters = sorted(movie.posters,
                    key = lambda x: x.language=="en", reverse = True)

    for poster in loc_posters:
        if opts.debug:
            print("Poster : ", poster.language, " | ", poster.userrating,
                    "\t | ", poster.geturl())
        m.images.append({'type':'coverart', 'url':poster.geturl(),
                        'thumb':poster.geturl(poster.sizes()[0]),
                        'height':str(poster.height),
                        'width':str(poster.width)})

    tree.append(m.toXML())
    return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)

def buildMovieList(query, opts):
    # TEMPORARY FIX:
    # replace all dashes from queries to work around search behavior
    # as negative to all text that comes afterwards
    query = query.replace('-',' ')

    from MythTV.tmdb3 import searchMovie
    from MythTV import VideoMetadata
    from lxml import etree
    results = iter(searchMovie(query))
    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['runtime',      'runtime'],     ['title',       'originaltitle'],
               ['releasedate',  'releasedate'], ['tagline',     'tagline'],
               ['description',  'overview'],    ['homepage',    'homepage'],
               ['userrating',   'userrating'],  ['popularity',  'popularity']]

    count = 0
    while True:
        try:
            res = next(results)
        except StopIteration:
            # end of results
            break
        except IndexError:
            # unexpected end of results
            # we still want to return whatever we have so far
            break

        if res is None:
            # faulty data, skip it and continue
            continue

        m = VideoMetadata()
        for i,j in mapping:
            if getattr(res, j):
                setattr(m, i, getattr(res, j))
        m.inetref = str(res.id)

        if res.title:
            m.title = res.title

        #TODO:
        # should releasedate and year be pulled from the country-specific data
        # or should it be left to the default information to cut down on
        # traffic from searches
        if res.releasedate:
            m.year = res.releasedate.year
        if res.backdrop:
            b = res.backdrop
            m.images.append({'type':'fanart', 'url':b.geturl(),
                             'thumb':b.geturl(b.sizes()[0])})
        if res.poster:
            p = res.poster
            m.images.append({'type':'coverart', 'url':p.geturl(),
                             'thumb':p.geturl(p.sizes()[0])})
        tree.append(m.toXML())
        count += 1
        if count >= 60:
            # page limiter, dont want to overload the server
            break

    return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)

def buildTVList(query, opts):
    from MythTV.tmdb3 import searchSeries
    from MythTV import VideoMetadata
    from lxml import etree
    from datetime import date

    resultsx = searchSeries(query)
    results = iter(resultsx) # searchSeries(query))
    mapping = [['language', 'original_language'],
        ['title', 'name'], ['inetref','id'],
        ['collectionref','id'], ['description','overview'],
        ['releasedate','first_air_date']]
    tree = etree.XML(u'<metadata></metadata>')

    count = 0
    while True:
        try:
            res = next(results)
        except StopIteration:
            # end of results
            break
        except IndexError:
            # unexpected end of results
            # we still want to return whatever we have so far
            break

        if res is None:
            # faulty data, skip it and continue
            continue

        m = VideoMetadata()
        for i,j in mapping:
            if getattr(res, j):
                setattr(m, i, getattr(res, j))

        # These need to be strings not ints
        m.inetref = str(res.id)
        m.collectionref = str(res.id)

        if res.backdrop:
            b = res.backdrop
            m.images.append({'type':'fanart', 'url':b.geturl(),
                             'thumb':b.geturl(b.sizes()[0])})
        if res.poster:
            p = res.poster
            m.images.append({'type':'coverart', 'url':p.geturl(),
                             'thumb':p.geturl(p.sizes()[0])})
        tree.append(m.toXML())
        count += 1
        if count >= 60:
            # page limiter, dont want to overload the server
            break

    return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)

#   for buildEpisode, args are one of the following
#   - title subtitle         (-N command line)
#   - inetref subtitle       (-N command line)
#   - inetref season episode (-D command line)
def buildEpisode(args, opts):

    query = args[0]

    from MythTV.tmdb3 import Series, Season, Episode
    from MythTV import VideoMetadata
    from lxml import etree
    from MythTV.tmdb3 import searchSeries

    if query.isnumeric():
        inetref = query
    else:
        results = searchSeries(query)
        series = results[0]
        inetref = str(series.id)

    series = Series(inetref)
    season_number = series.number_of_seasons
    episode_number = None
    subtitle = None
    if len(args) == 2:
        subtitle = args[1]
    elif len(args) == 3:
        season_number = int(args[1])
        episode_number = int(args[2])

    episode = None
    # process seasons backwards because it is more likely
    # that you have a recent one than an old one
    while season_number > 0:
        season = Season(inetref,str(season_number))
        if episode_number:
            episode = season.episodes[episode_number]
            break
        for ep_num, ep in season.episodes.items():
            if ep.name == subtitle:
                episode = ep
                episode_number = int(ep_num)
                break
        if episode:
            break
        season_number = season_number - 1

    if not episode_number and not episode:
        sys.stdout.write('ERROR: Episode not found: ' + str(args))
        return 9

    # reload episode with full details
    episode = Episode(inetref,season_number,episode_number)

    tree = etree.XML(u'<metadata></metadata>')
    mapping = [['subtitle','name'],
               ['description', 'overview'], ['season', 'season_number'],
               ['episode', 'episode_number'], ['releasedate', 'air_date']]
    m = VideoMetadata()
    m.title = series.name
    for i,j in mapping:
        if getattr(episode, j):
            setattr(m, i, getattr(episode, j))

    # These need to be strings not ints
    m.inetref = inetref
    m.collectionref = inetref

    for cast in episode.cast:
        d = {'name':cast.name, 'character':cast.character, 'department':'Actors',
             'job':'Actor', 'url':'http://www.themoviedb.org/people/{0}'.format(cast.id)}
        if cast.profile: d['thumb'] = cast.profile.geturl()
        m.people.append(d)
    for crew in episode.crew:
        d = {'name':crew.name, 'job':crew.job, 'department':crew.department,
             'url':'http://www.themoviedb.org/people/{0}'.format(crew.id)}
        if crew.profile: d['thumb'] = crew.profile.geturl()
        m.people.append(d)
    for guest in episode.guest_stars:
        d = {'name':guest.name, 'job':"Guest Star",
             'url':'http://www.themoviedb.org/people/{0}'.format(guest.id)}
        if guest.profile: d['thumb'] = guest.profile.geturl()
        m.people.append(d)
    if episode.still:
        b = episode.still
        m.images.append({'type':'screenshot', 'url':b.geturl(),
                         'thumb':b.geturl(b.sizes()[0])})
    if season.poster:
        p = season.poster
        m.images.append({'type':'coverart', 'url':p.geturl(),
                         'thumb':p.geturl(p.sizes()[0])})
    m.language = series.original_language
    if series.backdrop:
        b = series.backdrop
        m.images.append({'type':'fanart', 'url':b.geturl(),
                             'thumb':b.geturl(b.sizes()[0])})
    for genre in series.genres:
        m.categories.append(genre.name)

    tree.append(m.toXML())

    return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)

def buildCollection(inetref, opts):
    from MythTV.tmdb3.tmdb_exceptions import TMDBRequestInvalid
    from MythTV.tmdb3 import Collection
    from MythTV import VideoMetadata
    from lxml import etree
    collection = Collection(inetref)
    tree = etree.XML(u'<metadata></metadata>')
    m = VideoMetadata()
    m.collectionref = str(collection.id)
    try:
        m.title = collection.name
    except TMDBRequestInvalid:
        print_etree(etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                        xml_declaration=True))
        sys.exit()
    if collection.backdrop:
        b = collection.backdrop
        m.images.append({'type':'fanart', 'url':b.geturl(),
                  'thumb':b.geturl(b.sizes()[0])})
    if collection.poster:
        p = collection.poster
        m.images.append({'type':'coverart', 'url':p.geturl(),
                  'thumb':p.geturl(p.sizes()[0])})
    tree.append(m.toXML())
    return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)

#   For television only - get series info
#   Same results as for television search but only 1 result
def buildTVSeries(inetref, opts):

    from MythTV.tmdb3 import Series
    from MythTV import VideoMetadata
    from lxml import etree

    series = Series(inetref)

    mapping = [['language', 'original_language'],
        ['title', 'name'], ['inetref','id'],
        ['collectionref','id'], ['description','overview'],
        ['releasedate','first_air_date']]
    tree = etree.XML(u'<metadata></metadata>')
    m = VideoMetadata()
    for i,j in mapping:
        if getattr(series, j):
            setattr(m, i, getattr(series, j))

    # These need to be strings not ints
    m.inetref = str(series.id)
    m.collectionref = str(series.id)

    for genre in series.genres:
        m.categories.append(genre.name)

    if series.backdrop:
        b = series.backdrop
        m.images.append({'type':'fanart', 'url':b.geturl(),
                         'thumb':b.geturl(b.sizes()[0])})
    if series.poster:
        p = series.poster
        m.images.append({'type':'coverart', 'url':p.geturl(),
                         'thumb':p.geturl(p.sizes()[0])})
    tree.append(m.toXML())

    return etree.tostring(tree, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)

def buildVersion(showType, command):
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__ + ' ' + showType
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "thumbnail").text = 'tmdb.png'
    etree.SubElement(version, "command").text = command
    etree.SubElement(version, "type").text = showType
    etree.SubElement(version, "description").text = \
                                'Search and metadata downloads for themoviedb.org'
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "accepts").text = 'tmdb.py'
    etree.SubElement(version, "accepts").text = 'tmdb.pl'
    return etree.tostring(version, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True)
